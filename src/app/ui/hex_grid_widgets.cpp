// Aseprite
// Copyright (C) 2026-present  Igara Studio S.A.
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "app/ui/hex_grid_widgets.h"

#include "doc/image.h"
#include "gfx/color.h"
#include "ui/graphics.h"
#include "ui/message.h"
#include "ui/paint_event.h"
#include "ui/scale.h"
#include "ui/size_hint_event.h"

#include <algorithm>

namespace app {

using namespace ui;

namespace {

// Maximum size of the mask editor and preview (without the GUI scale)
const int kMaxEditorSize = 160;
const gfx::Size kPreviewSize(240, 140);

const gfx::Color kMaskOn = gfx::rgba(40, 40, 40);
const gfx::Color kMaskOff = gfx::rgba(235, 235, 235);
const gfx::Color kWrong = gfx::rgba(230, 30, 30);
const gfx::Color kGap = gfx::rgba(230, 40, 40);
const gfx::Color kOverlap = gfx::rgba(220, 0, 220);
const gfx::Color kTileColors[4] = {
  gfx::rgba(120, 170, 220),
  gfx::rgba(240, 190, 110),
  gfx::rgba(150, 210, 140),
  gfx::rgba(210, 150, 200),
};

gfx::Color darker(const gfx::Color c)
{
  return gfx::rgba(gfx::getr(c) * 3 / 5, gfx::getg(c) * 3 / 5, gfx::getb(c) * 3 / 5);
}

// How many tiles cover the pixel, and the last one found
int tiles_covering(const doc::Grid& grid, const gfx::Point& pt, gfx::Point& owner)
{
  if (!grid.isHexagonal()) {
    owner = grid.canvasToTile(pt);
    return 1;
  }

  const doc::Image* mask = (grid.hasMask() ? grid.mask().get() : nullptr);
  const gfx::Point t0 = grid.originToTile(pt);
  int count = 0;
  for (int v = -2; v <= 1; ++v) {
    for (int u = -2; u <= 1; ++u) {
      const gfx::Point tile(t0.x + u, t0.y + v);
      const gfx::Rect bounds = grid.tileBoundsInCanvas(tile);
      if (bounds.contains(pt) && (!mask || mask->getPixel(pt.x - bounds.x, pt.y - bounds.y))) {
        owner = tile;
        ++count;
      }
    }
  }
  return count;
}

} // anonymous namespace

//////////////////////////////////////////////////////////////////////
// HexMaskEditor

HexMaskEditor::HexMaskEditor()
{
  initTheme();
}

void HexMaskEditor::setMask(const doc::ImageRef& mask)
{
  m_mask.reset(mask ? doc::Image::createCopy(mask.get()) : nullptr);
  m_wrongPixels.clear();
  invalidate();
}

void HexMaskEditor::setWrongPixels(const std::vector<gfx::Point>& pixels)
{
  m_wrongPixels = pixels;
  invalidate();
}

int HexMaskEditor::cellSize() const
{
  if (!m_mask)
    return 1;
  const int maxSide = std::max(m_mask->width(), m_mask->height());
  return std::clamp(kMaxEditorSize / std::max(1, maxSide), 2, 12) * guiscale();
}

gfx::Rect HexMaskEditor::cellsBounds() const
{
  if (!m_mask)
    return gfx::Rect();
  const int cell = cellSize();
  const gfx::Rect rc = clientBounds();
  return gfx::Rect(rc.x, rc.y, m_mask->width() * cell, m_mask->height() * cell);
}

bool HexMaskEditor::paintAt(const gfx::Point& screenPos)
{
  if (!m_mask || m_paintValue < 0)
    return false;

  const int cell = cellSize();
  const gfx::Point pos = screenPos - bounds().origin() - cellsBounds().origin();
  const gfx::Point pt(pos.x / cell, pos.y / cell);
  if (pos.x < 0 || pos.y < 0 || !m_mask->bounds().contains(pt))
    return false;

  if (int(m_mask->getPixel(pt.x, pt.y)) == m_paintValue)
    return false;

  m_mask->putPixel(pt.x, pt.y, m_paintValue);
  invalidate();
  return true;
}

bool HexMaskEditor::onProcessMessage(Message* msg)
{
  switch (msg->type()) {
    case kMouseDownMessage: {
      auto mouseMsg = static_cast<MouseMessage*>(msg);
      m_paintValue = (mouseMsg->right() ? 0 : 1);
      captureMouse();
      if (paintAt(mouseMsg->position()))
        Change();
      return true;
    }

    case kMouseMoveMessage:
      if (hasCapture()) {
        if (paintAt(static_cast<MouseMessage*>(msg)->position()))
          Change();
        return true;
      }
      break;

    case kMouseUpMessage:
      if (hasCapture()) {
        releaseMouse();
        m_paintValue = -1;
        return true;
      }
      break;
  }
  return Widget::onProcessMessage(msg);
}

void HexMaskEditor::onSizeHint(SizeHintEvent& ev)
{
  ev.setSizeHint(cellsBounds().size());
}

void HexMaskEditor::onPaint(PaintEvent& ev)
{
  Graphics* g = ev.graphics();
  g->fillRect(bgColor(), clientBounds());
  if (!m_mask)
    return;

  const int cell = cellSize();
  const gfx::Rect cells = cellsBounds();
  const int gap = (cell >= 6 ? 1 : 0);
  for (int y = 0; y < m_mask->height(); ++y) {
    for (int x = 0; x < m_mask->width(); ++x) {
      const gfx::Rect rc(cells.x + x * cell, cells.y + y * cell, cell - gap, cell - gap);
      g->fillRect(m_mask->getPixel(x, y) ? kMaskOn : kMaskOff, rc);
    }
  }

  for (const gfx::Point& pt : m_wrongPixels) {
    if (m_mask->bounds().contains(pt))
      g->drawRect(kWrong, gfx::Rect(cells.x + pt.x * cell, cells.y + pt.y * cell, cell, cell));
  }
}

//////////////////////////////////////////////////////////////////////
// HexGridPreview

HexGridPreview::HexGridPreview()
{
  initTheme();
}

void HexGridPreview::setGrid(const doc::Grid& grid)
{
  m_grid = grid;
  m_grid.origin(gfx::Point(0, 0));
  invalidate();
}

void HexGridPreview::onSizeHint(SizeHintEvent& ev)
{
  ev.setSizeHint(kPreviewSize * guiscale());
}

void HexGridPreview::onPaint(PaintEvent& ev)
{
  Graphics* g = ev.graphics();
  const gfx::Rect rc = clientBounds();
  g->fillRect(bgColor(), rc);
  if (m_grid.isEmpty() || m_grid.tileOffset().x < 1 || m_grid.tileOffset().y < 1)
    return;

  // Canvas area with 4x3 tiles
  const gfx::Rect area = m_grid.tileToCanvas(gfx::Rect(0, 0, 4, 3));
  if (area.isEmpty())
    return;

  // Size of each canvas pixel on the screen (or less than 1 if the
  // tiles are big)
  const double scale = std::min(double(rc.w) / area.w, double(rc.h) / area.h);
  const double px = (scale >= 1.0 ? std::floor(scale) : scale);
  const int dstW = std::max(1, int(area.w * px));
  const int dstH = std::max(1, int(area.h * px));
  const gfx::Point dst0(rc.x + (rc.w - dstW) / 2, rc.y + (rc.h - dstH) / 2);

  auto colorAt = [this](const gfx::Point& pt) -> gfx::Color {
    gfx::Point owner;
    const int n = tiles_covering(m_grid, pt, owner);
    if (n == 0)
      return kGap;
    if (n > 1)
      return kOverlap;

    const gfx::Color c = kTileColors[(((owner.x + 2 * owner.y) % 4) + 4) % 4];
    // Darker edges between tiles
    gfx::Point other;
    if ((tiles_covering(m_grid, pt + gfx::Point(1, 0), other) == 1 && other != owner) ||
        (tiles_covering(m_grid, pt + gfx::Point(0, 1), other) == 1 && other != owner))
      return darker(c);
    return c;
  };

  // Draw each row of the screen joining pixels of the same color
  const int rowStep = std::max(1, int(px));
  for (int dy = 0; dy < dstH; dy += rowStep) {
    const int cy = area.y + int(dy / px);
    int runX = 0;
    gfx::Color runColor = colorAt(gfx::Point(area.x, cy));
    for (int dx = 1; dx <= dstW; ++dx) {
      const gfx::Color color = (dx < dstW ? colorAt(gfx::Point(area.x + int(dx / px), cy)) :
                                            gfx::Color(0));
      if (dx == dstW || color != runColor) {
        g->fillRect(runColor, gfx::Rect(dst0.x + runX, dst0.y + dy, dx - runX, rowStep));
        runX = dx;
        runColor = color;
      }
    }
  }
}

} // namespace app
