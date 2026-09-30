// Aseprite
// Copyright (C) 2026-present  Igara Studio S.A.
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_UI_HEX_GRID_WIDGETS_H_INCLUDED
#define APP_UI_HEX_GRID_WIDGETS_H_INCLUDED
#pragma once

#include "doc/grid.h"
#include "doc/image_ref.h"
#include "gfx/point.h"
#include "obs/signal.h"
#include "ui/widget.h"

#include <vector>

namespace app {

// Small pixel editor for the mask of a hexagonal tile: left click
// adds pixels to the hexagon, right click removes them.
class HexMaskEditor : public ui::Widget {
public:
  HexMaskEditor();

  // The mask is copied (it's a bitmap with the tile size)
  void setMask(const doc::ImageRef& mask);
  doc::ImageRef mask() const { return m_mask; }

  // Pixels of the mask (in tile coordinates) where the tiling fails
  void setWrongPixels(const std::vector<gfx::Point>& pixels);

  obs::signal<void()> Change;

protected:
  bool onProcessMessage(ui::Message* msg) override;
  void onSizeHint(ui::SizeHintEvent& ev) override;
  void onPaint(ui::PaintEvent& ev) override;

private:
  int cellSize() const;
  gfx::Rect cellsBounds() const;
  bool paintAt(const gfx::Point& screenPos);

  doc::ImageRef m_mask;
  std::vector<gfx::Point> m_wrongPixels;
  int m_paintValue = -1; // Value painted while the mouse is captured
};

// Shows how the tiles of a grid tessellate: several tiles with
// different colors, darker edges between tiles, and gaps (red) or
// overlaps (magenta) when the mask doesn't tessellate.
class HexGridPreview : public ui::Widget {
public:
  HexGridPreview();

  void setGrid(const doc::Grid& grid);

protected:
  void onSizeHint(ui::SizeHintEvent& ev) override;
  void onPaint(ui::PaintEvent& ev) override;

private:
  doc::Grid m_grid;
};

} // namespace app

#endif
