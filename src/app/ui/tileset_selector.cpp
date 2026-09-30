// Aseprite
// Copyright (C) 2019-2023  Igara Studio S.A.
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "app/ui/tileset_selector.h"

#include "app/doc.h"
#include "app/i18n/strings.h"
#include "app/pref/preferences.h"
#include "base/scoped_value.h"
#include "doc/image.h"
#include "doc/mask.h"
#include "doc/primitives.h"
#include "doc/sprite.h"
#include "doc/tilesets.h"
#include "fmt/format.h"
#include "ui/listitem.h"
#include "ui/window.h"

namespace app {

using namespace ui;

TilesetSelector::TilesetSelector(const doc::Sprite* sprite, const TilesetSelector::Info& info)
  : m_sprite(sprite)
  , m_info(info)
{
  initTheme();

  // Hexagonal grid controls
  hexMaskEditorPlace()->addChild(&m_maskEditor);
  hexPreviewPlace()->addChild(&m_preview);

  fillControls(m_info.name, m_info.grid.tileSize(), m_info.baseIndex, m_info.matchFlags);
  fillHexControls(m_info.grid);

  if (!m_info.allowNewTileset) {
    tilesets()->deleteAllItems();
  }

  if (m_info.allowExistentTileset) {
    doc::tileset_index tsi = 0;
    for (doc::Tileset* tileset : *sprite->tilesets()) {
      if (!tileset)
        continue;

      auto item = new ListItem(fmt::format("Tileset #{0} ({1}x{2}{4}): \"{3}\"",
                                           tsi,
                                           tileset->grid().tileSize().w,
                                           tileset->grid().tileSize().h,
                                           tileset->name(),
                                           tileset->grid().isHexagonal() ? " hex" : ""));
      tilesets()->addItem(item);

      if (m_info.tsi == tsi)
        tilesets()->setSelectedItem(item);

      ++tsi;
    }
  }

  if (m_info.enabled) {
    tilesets()->Change.connect([this]() { updateControlsState(); });
  }

  // Changes of the grid settings update the hexagonal grid controls
  shape()->Change.connect([this]() {
    resetHexStepAndMask();
    updateHexControls();
  });
  gridWidth()->Change.connect([this]() {
    resetHexStepAndMask();
    updateHexPreview();
  });
  gridHeight()->Change.connect([this]() {
    resetHexStepAndMask();
    updateHexPreview();
  });
  hexStep()->Change.connect([this]() { updateHexPreview(); });
  hexParity()->Change.connect([this]() { updateHexPreview(); });
  hexMaskMode()->ItemChange.connect([this](ButtonSet::Item*) {
    if (m_updatingHex)
      return;
    // Start the custom mask from the current automatic one
    if (isCustomMask())
      m_maskEditor.setMask(
        doc::Grid::MakeHexMask(selectedTileSize(), selectedType(), hexStep()->textInt()));
    updateHexControls();
  });
  m_maskEditor.Change.connect([this]() { updateHexPreview(); });
  hexMaskReset()->Click.connect([this]() {
    m_maskEditor.setMask(doc::Grid::MakeHex(selectedTileSize(),
                                            selectedType(),
                                            doc::Grid::Parity::Odd,
                                            hexStep()->textInt())
                           .mask());
    updateHexPreview();
  });
  hexMaskFromSelection()->Click.connect([this]() { onMaskFromSelection(); });

  // Advanced controls
  const Preferences& pref = Preferences::instance();
  advanced()->setSelected(pref.tileset.advanced());
  advanced()->Click.connect([this]() {
    updateControlsVisibility();
    if (auto win = window())
      win->expandWindow(win->sizeHint());
  });

  updateControlsState();
  updateControlsVisibility();
  updateHexControls();
}

void TilesetSelector::fillControls(const std::string& nameValue,
                                   const gfx::Size& gridSize,
                                   const int baseIndexValue,
                                   const doc::tile_flags matchFlags)
{
  name()->setText(nameValue);
  gridWidth()->setTextf("%d", gridSize.w);
  gridHeight()->setTextf("%d", gridSize.h);
  baseIndex()->setTextf("%d", baseIndexValue);
  xflip()->setSelected((matchFlags & doc::tile_f_xflip) ? true : false);
  yflip()->setSelected((matchFlags & doc::tile_f_yflip) ? true : false);
  dflip()->setSelected((matchFlags & doc::tile_f_dflip) ? true : false);
}

void TilesetSelector::updateControlsState()
{
  const doc::Tilesets* spriteTilesets = m_sprite->tilesets();

  if (m_info.enabled) {
    const int index = getSelectedItemIndex();
    const bool isNewTileset = (index == 0);
    if (isNewTileset) {
      name()->setText("");
      baseIndex()->setTextf("%d", 1);
    }
    else {
      const doc::Tileset* ts = spriteTilesets->get(index - 1);
      fillControls(ts->name(), ts->grid().tileSize(), ts->baseIndex(), ts->matchFlags());
      fillHexControls(ts->grid());
    }

    name()->setEnabled(true);
    gridWidth()->setEnabled(isNewTileset);
    gridHeight()->setEnabled(isNewTileset);
    baseIndex()->setEnabled(true);
    updateHexControls();
  }
  else {
    tilesets()->setEnabled(false);
    gridWidth()->setEnabled(false);
    gridHeight()->setEnabled(false);
    baseIndex()->setEnabled(false);
    xflip()->setEnabled(false);
    yflip()->setEnabled(false);
    dflip()->setEnabled(false);
  }
}

void TilesetSelector::updateControlsVisibility()
{
  const bool v = advanced()->isSelected();
  baseIndexLabel()->setVisible(v);
  baseIndex()->setVisible(v);
  baseIndexFiller()->setVisible(v);
  flipsLabel()->setVisible(v);
  flips()->setVisible(v);
  flipsFiller()->setVisible(v);
}

TilesetSelector::Info TilesetSelector::getInfo()
{
  int itemIndex = getSelectedItemIndex();
  Info info;
  if (itemIndex == 0) {
    info.newTileset = true;
    info.grid = makeGrid(false);
  }
  else {
    info.newTileset = false;
    info.tsi = itemIndex - 1;

    // Tileset properties: the grid can be modified
    if (!m_info.allowNewTileset)
      info.grid = makeGrid(false);
  }
  info.name = name()->text();

  // If we are creating a new tilemap/tileset, and the advanced
  // options are hidden, we use the default values (only in that
  // case). In other case we use the edited options (even if the
  // advanced options are hidden).
  if (m_info.allowNewTileset && m_info.newTileset && !advanced()->isSelected()) {
    info.baseIndex = 1;
    info.matchFlags = 0;
  }
  else {
    info.baseIndex = baseIndex()->textInt();
    info.matchFlags = (xflip()->isSelected() ? doc::tile_f_xflip : 0) |
                      (yflip()->isSelected() ? doc::tile_f_yflip : 0) |
                      (dflip()->isSelected() ? doc::tile_f_dflip : 0);
  }

  // Hexagonal tiles can be matched only with their valid flips
  if (info.grid.isHexagonal())
    info.matchFlags &= info.grid.validTileFlags();

  return info;
}

void TilesetSelector::saveAdvancedPreferences()
{
  Preferences& pref = Preferences::instance();
  pref.tileset.advanced(advanced()->isSelected());
}

void TilesetSelector::fillHexControls(const doc::Grid& grid)
{
  base::ScopedValue lock(m_updatingHex, true);

  switch (grid.type()) {
    case doc::Grid::Type::Orthogonal:   shape()->setSelectedItemIndex(0); break;
    case doc::Grid::Type::HexPointyTop: shape()->setSelectedItemIndex(1); break;
    case doc::Grid::Type::HexFlatTop:   shape()->setSelectedItemIndex(2); break;
  }
  hexParity()->setSelectedItemIndex(grid.parity() == doc::Grid::Parity::Even ? 1 : 0);

  if (grid.isHexagonal()) {
    hexStep()->setTextf("%d", grid.hexStep());

    // Custom mask if it's different from the automatic one
    const doc::ImageRef autoMask =
      doc::Grid::MakeHexMask(grid.tileSize(), grid.type(), grid.hexStep());
    const bool custom = (grid.hasMask() && !doc::is_same_image(autoMask.get(), grid.mask().get()));
    hexMaskMode()->setSelectedItem(custom ? 1 : 0, false);
    m_maskEditor.setMask(grid.hasMask() ? grid.mask() : autoMask);
  }
  else {
    hexStep()->setTextf("%d",
                        doc::Grid::DefaultHexStep(grid.tileSize(), doc::Grid::Type::HexPointyTop));
    hexMaskMode()->setSelectedItem(0, false);
    m_maskEditor.setMask(doc::ImageRef());
  }
}

void TilesetSelector::resetHexStepAndMask()
{
  if (m_updatingHex)
    return;

  // Default step and automatic mask for the new size/shape
  const doc::Grid::Type type = selectedType();
  if (doc::Grid::isHexagonal(type)) {
    const gfx::Size sz = selectedTileSize();
    hexStep()->setTextf("%d", doc::Grid::DefaultHexStep(sz, type));
    m_maskEditor.setMask(doc::Grid::MakeHexMask(sz, type, doc::Grid::DefaultHexStep(sz, type)));
  }
}

void TilesetSelector::updateHexControls()
{
  const bool hex = doc::Grid::isHexagonal(selectedType());
  const bool custom = (hex && isCustomMask());
  const bool editable = canEditGrid();

  hexStepLabel()->setVisible(hex);
  hexStep()->setVisible(hex);
  hexParityLabel()->setVisible(hex);
  hexParity()->setVisible(hex);
  hexMaskLabel()->setVisible(hex);
  hexMaskMode()->setVisible(hex);
  hexMaskRow()->setVisible(custom);
  hexPreviewPlace()->setVisible(hex);
  hexStatus()->setVisible(hex);

  shape()->setEnabled(editable);
  hexStep()->setEnabled(editable);
  hexParity()->setEnabled(editable);
  hexMaskMode()->setEnabled(editable);
  m_maskEditor.setEnabled(editable);
  hexMaskFromSelection()->setEnabled(editable);
  hexMaskReset()->setEnabled(editable);

  updateParityItems();
  updateHexPreview();

  if (auto win = window()) {
    win->layout();
    win->expandWindow(win->sizeHint());
  }
}

void TilesetSelector::updateParityItems()
{
  const bool flat = (selectedType() == doc::Grid::Type::HexFlatTop);
  hexParity()->setItemText(
    0,
    flat ? Strings::tileset_selector_parity_odd_flat() : Strings::tileset_selector_parity_odd());
  hexParity()->setItemText(
    1,
    flat ? Strings::tileset_selector_parity_even_flat() : Strings::tileset_selector_parity_even());
  if (hexParity()->getSelectedItemIndex() >= 0)
    hexParity()->getEntryWidget()->setText(
      hexParity()->getItemText(hexParity()->getSelectedItemIndex()));
}

void TilesetSelector::updateHexPreview()
{
  const doc::Grid::Type type = selectedType();
  if (!doc::Grid::isHexagonal(type)) {
    m_validGrid = true;
  }
  else {
    const gfx::Size sz = selectedTileSize();

    const doc::Grid grid = makeGrid(true);
    doc::Grid::TessellationError err;
    m_validGrid = grid.validateTessellation(&err);
    m_preview.setGrid(grid);

    std::vector<gfx::Point> wrongPixels;
    std::string status;
    if (m_validGrid) {
      const int w = (type == doc::Grid::Type::HexFlatTop ? sz.h : sz.w);
      status = ((w & 1) ? Strings::tileset_selector_hex_valid_bricks() :
                          Strings::tileset_selector_hex_valid());
    }
    else {
      switch (err.kind) {
        case doc::Grid::TessellationError::Kind::Gap:
          status = Strings::tileset_selector_hex_gap(err.pos.x, err.pos.y);
          break;
        case doc::Grid::TessellationError::Kind::Overlap:
          status = Strings::tileset_selector_hex_overlap(err.pos.x, err.pos.y, err.coverage);
          break;
        default: status = Strings::tileset_selector_hex_bad_mask(); break;
      }

      // Show the wrong pixel in the mask editor (in each tile that
      // could include it)
      if (err.kind == doc::Grid::TessellationError::Kind::Gap ||
          err.kind == doc::Grid::TessellationError::Kind::Overlap) {
        const gfx::Point t0 = grid.originToTile(err.pos);
        for (int v = -2; v <= 1; ++v) {
          for (int u = -2; u <= 1; ++u) {
            const gfx::Rect bounds = grid.tileBoundsInCanvas(t0 + gfx::Point(u, v));
            if (bounds.contains(err.pos))
              wrongPixels.push_back(err.pos - bounds.origin());
          }
        }
      }
    }
    hexStatus()->setText(status);
    m_maskEditor.setWrongPixels(wrongPixels);

    // Only valid flips can be matched with hexagonal tiles
    const doc::tile_flags validFlags = grid.validTileFlags();
    for (auto item : { std::make_pair(xflip(), doc::tile_f_xflip),
                       std::make_pair(yflip(), doc::tile_f_yflip),
                       std::make_pair(dflip(), doc::tile_f_dflip) }) {
      const bool valid = ((validFlags & item.second) != 0);
      if (!valid)
        item.first->setSelected(false);
      item.first->setEnabled(valid && m_info.enabled);
    }
  }

  if (!doc::Grid::isHexagonal(type)) {
    xflip()->setEnabled(m_info.enabled);
    yflip()->setEnabled(m_info.enabled);
    dflip()->setEnabled(m_info.enabled);
  }

  // The window can't be accepted with an invalid mask
  if (auto win = window()) {
    if (auto ok = win->findChild("ok"))
      ok->setEnabled(m_validGrid);
  }
}

void TilesetSelector::onMaskFromSelection()
{
  const auto* doc = static_cast<const Doc*>(m_sprite->document());
  const doc::Mask* mask = (doc && doc->isMaskVisible() ? doc->mask() : nullptr);
  if (!mask || mask->isEmpty()) {
    hexStatus()->setText(Strings::tileset_selector_hex_no_selection());
    return;
  }

  const gfx::Size sz = selectedTileSize();
  const gfx::Rect bounds = mask->bounds();
  doc::ImageRef image(doc::Image::create(doc::IMAGE_BITMAP, sz.w, sz.h));
  image->clear(0);
  for (int y = 0; y < sz.h; ++y)
    for (int x = 0; x < sz.w; ++x)
      if (mask->containsPoint(bounds.x + x, bounds.y + y))
        image->putPixel(x, y, 1);

  m_maskEditor.setMask(image);
  hexMaskMode()->setSelectedItem(1, false);
  updateHexControls();
}

doc::Grid::Type TilesetSelector::selectedType()
{
  switch (shape()->getSelectedItemIndex()) {
    case 1:  return doc::Grid::Type::HexPointyTop;
    case 2:  return doc::Grid::Type::HexFlatTop;
    default: return doc::Grid::Type::Orthogonal;
  }
}

gfx::Size TilesetSelector::selectedTileSize()
{
  const int index = getSelectedItemIndex();
  if (index > 0 && m_sprite->tilesets()) {
    if (const doc::Tileset* ts = m_sprite->tilesets()->get(index - 1))
      return ts->grid().tileSize();
  }
  return gfx::Size(std::max(1, gridWidth()->textInt()), std::max(1, gridHeight()->textInt()));
}

bool TilesetSelector::canEditGrid()
{
  if (!m_info.enabled)
    return false;
  // A new tileset, or the properties of an existent tileset
  return (getSelectedItemIndex() == 0 || !m_info.allowNewTileset);
}

bool TilesetSelector::isCustomMask()
{
  return (hexMaskMode()->selectedItem() == 1);
}

doc::Grid TilesetSelector::makeGrid(const bool forPreview)
{
  const gfx::Size sz = selectedTileSize();
  const doc::Grid::Type type = selectedType();

  if (!doc::Grid::isHexagonal(type)) {
    // Keep the grid of an existent orthogonal tileset as it is
    const int index = getSelectedItemIndex();
    if (index > 0 && m_sprite->tilesets()) {
      if (const doc::Tileset* ts = m_sprite->tilesets()->get(index - 1)) {
        if (!ts->grid().isHexagonal())
          return ts->grid();
      }
    }
    return doc::Grid::MakeRect(sz);
  }

  const doc::Grid::Parity parity =
    (hexParity()->getSelectedItemIndex() == 1 ? doc::Grid::Parity::Even : doc::Grid::Parity::Odd);
  doc::Grid grid = doc::Grid::MakeHex(sz, type, parity, hexStep()->textInt());
  const doc::ImageRef mask = m_maskEditor.mask();
  if (isCustomMask() && mask && mask->size() == sz) {
    // The preview shows an invalid mask as it is, the final grid uses
    // the automatic mask (the window can't be accepted anyway)
    if (forPreview)
      grid.setMask(doc::ImageRef(doc::Image::createCopy(mask.get())));
    else
      grid = doc::Grid::MakeHex(sz,
                                type,
                                parity,
                                hexStep()->textInt(),
                                doc::ImageRef(doc::Image::createCopy(mask.get())));
  }
  return grid;
}

int TilesetSelector::getSelectedItemIndex()
{
  int index = tilesets()->getSelectedItemIndex();
  // We have to compensate the selected index when allowNewTileset is false,
  // because the "New Tileset" item was removed.
  if (!m_info.allowNewTileset) {
    index++;
  }
  return index;
}

} // namespace app
