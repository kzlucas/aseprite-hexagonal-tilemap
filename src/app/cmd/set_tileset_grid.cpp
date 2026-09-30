// Aseprite
// Copyright (C) 2026-present  Igara Studio S.A.
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "app/cmd/set_tileset_grid.h"

#include "app/doc.h"
#include "doc/cel.h"
#include "doc/image.h"
#include "doc/layer_tilemap.h"
#include "doc/primitives.h"
#include "doc/sprite.h"
#include "doc/tileset.h"

namespace app { namespace cmd {

SetTilesetGrid::SetTilesetGrid(Tileset* tileset, const Grid& grid)
  : WithTileset(tileset)
  , m_oldGrid(tileset->grid())
  , m_newGrid(grid)
{
  ASSERT(grid.tileSize() == tileset->grid().tileSize());
}

void SetTilesetGrid::onExecute()
{
  Tileset* ts = tileset();

  // Save the tiles with pixels outside the new mask
  m_oldTiles.clear();
  if (m_newGrid.isHexagonal() && m_newGrid.hasMask()) {
    const Image* mask = m_newGrid.mask().get();
    for (tile_index ti = 0; ti < ts->size(); ++ti) {
      const ImageRef tile = ts->get(ti);
      if (!tile || tile->size() != mask->size())
        continue;
      const color_t bg = tile->maskColor();
      bool modified = false;
      for (int y = 0; y < tile->height() && !modified; ++y)
        for (int x = 0; x < tile->width() && !modified; ++x)
          if (!mask->getPixel(x, y) && tile->getPixel(x, y) != bg)
            modified = true;
      if (modified)
        m_oldTiles.push_back({ ti, ImageRef(Image::createCopy(tile.get())) });
    }
  }

  setGrid(m_newGrid);
}

void SetTilesetGrid::onUndo()
{
  Tileset* ts = tileset();
  setGrid(m_oldGrid);

  // Restore the pixels in the same tile images
  for (const auto& old : m_oldTiles) {
    if (const ImageRef tile = ts->get(old.first)) {
      copy_image(tile.get(), old.second.get());
      ts->notifyTileContentChange(old.first);
    }
  }
  m_oldTiles.clear();
}

void SetTilesetGrid::onFireNotifications()
{
  Tileset* ts = tileset();
  if (ts->sprite())
    static_cast<Doc*>(ts->sprite()->document())->notifyTilesetChanged(ts);
}

size_t SetTilesetGrid::onMemSize() const
{
  size_t size = sizeof(*this);
  for (const auto& old : m_oldTiles)
    size += old.second->rowBytes() * old.second->height();
  return size;
}

void SetTilesetGrid::setGrid(const Grid& grid)
{
  Tileset* ts = tileset();
  ts->setGrid(grid);
  ts->incrementVersion();

  // Tilemap cels using this tileset change their bounds
  if (Sprite* sprite = ts->sprite()) {
    for (Layer* layer : sprite->allTilemaps()) {
      if (static_cast<LayerTilemap*>(layer)->tileset() != ts)
        continue;
      for (Cel* cel : layer->cels())
        cel->data()->adjustBounds(layer);
    }
    sprite->incrementVersion();
  }
}

}} // namespace app::cmd
