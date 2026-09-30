// Aseprite
// Copyright (C) 2020  Igara Studio S.A.
// Copyright (C) 2001-2016  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_SNAP_TO_GRID_H_INCLUDED
#define APP_SNAP_TO_GRID_H_INCLUDED
#pragma once

#include "gfx/fwd.h"

namespace doc {
class Grid;
}

namespace app {

enum class PreferSnapTo {
  ClosestGridVertex,
  BoxOrigin,
  BoxEnd,
  FloorGrid,
  CeilGrid,
};

gfx::Point snap_to_grid(const gfx::Rect& grid, const gfx::Point& point, const PreferSnapTo prefer);

// Returns the position of the tile of a hexagonal grid (the origin of
// its bounds) closest to the given point. It's the equivalent of the
// closest vertex of an orthogonal grid.
gfx::Point snap_to_hex_tile_position(const doc::Grid& grid, const gfx::Point& point);

// Returns the displacement closest to the given one that keeps the
// stagger of a hexagonal grid (a multiple of two rows or columns), so
// a tilemap moved by this displacement keeps its tiles in the same
// lattice.
gfx::Point snap_to_hex_stagger_period(const doc::Grid& grid, const gfx::Point& delta);

} // namespace app

#endif
