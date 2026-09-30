// Aseprite
// Copyright (C) 2019  Igara Studio S.A.
// Copyright (C) 2016  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_CMD_CROP_CEL_H_INCLUDED
#define APP_CMD_CROP_CEL_H_INCLUDED
#pragma once

#include "app/cmd.h"
#include "app/cmd/with_cel.h"
#include "gfx/point.h"
#include "gfx/rect.h"

namespace app { namespace cmd {

class CropCel : public Cmd,
                public WithCel {
public:
  CropCel(doc::Cel* cel, const gfx::Rect& newBounds);

  // Changes the tiles of a tilemap cel of a hexagonal tileset:
  // "newTiles" is the new range of tiles in the current tilemap
  // coordinates, and it's aligned to the stagger of the grid (so the
  // tiles don't move).
  struct Tiles {};
  CropCel(doc::Cel* cel, const gfx::Rect& newTiles, Tiles);

protected:
  void onExecute() override;
  void onUndo() override;
  size_t onMemSize() const override { return sizeof(*this); }

private:
  void cropImage(const gfx::Point& origin, const gfx::Rect& bounds);
  void cropTiles(const gfx::Point& origin, const gfx::Rect& tiles);
  void cropImage(const gfx::Point& origin, bool crop, const gfx::Rect& localBounds);

  gfx::Point m_oldOrigin;
  gfx::Point m_newOrigin;
  gfx::Rect m_oldBounds;
  gfx::Rect m_newBounds;
  // Ranges of tiles (hexagonal tilesets)
  bool m_tiles = false;
  gfx::Rect m_oldTiles;
  gfx::Rect m_newTiles;
};

}} // namespace app::cmd

#endif
