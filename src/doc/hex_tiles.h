// Aseprite Document Library
// Copyright (c) 2026-present  Igara Studio S.A.
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifndef DOC_HEX_TILES_H_INCLUDED
#define DOC_HEX_TILES_H_INCLUDED
#pragma once

#include "doc/algorithm/flip_type.h"
#include "doc/image_ref.h"
#include "gfx/point.h"
#include "gfx/rect.h"

namespace doc {

class Grid;
class Image;
class Mask;

// Functions to move, copy and flip a set of hexagonal tiles (e.g. a
// selection of tiles moved in tiles mode). The tiles are a tilemap
// image whose first tile is the tile "origin" of the "grid" (aligned
// to the stagger of the grid, see Grid::alignTilesToStagger()).
//
// Tiles are moved by a canvas displacement between two tiles of the
// grid: moving a tile by an odd number of rows (pointy-top) or
// columns (flat-top) changes its stagger, so the tiles are placed in
// the destination by their positions in the canvas (not by their
// index in the tilemap).

// Returns the displacement between the tiles under the two canvas
// points.
gfx::Point hex_tiles_delta(const Grid& grid, const gfx::Point& from, const gfx::Point& to);

// Returns the range of tiles of the grid covered by the non-empty
// tiles moved by "delta".
gfx::Rect moved_hex_tiles_bounds(const Image* tiles,
                                 const gfx::Point& origin,
                                 const Grid& grid,
                                 const gfx::Point& delta);

// Copies the non-empty tiles moved by "delta" in the "dst" tilemap,
// whose first tile is the tile "dstOrigin" of the grid.
void copy_moved_hex_tiles(Image* dst,
                          const gfx::Point& dstOrigin,
                          const Image* tiles,
                          const gfx::Point& origin,
                          const Grid& grid,
                          const gfx::Point& delta);

// Finds the origin of the tiles so all their non-empty tiles have
// their center inside the "mask" (the selection of the tiles).
// Returns false if there is no such origin.
bool find_hex_tiles_origin(const Image* tiles,
                           const Mask* mask,
                           const Grid& grid,
                           gfx::Point& origin);

// Flips the positions of the tiles and the mask around an axis close
// to the center of the mask where each tile lands on another tile of
// the grid. Returns false (and nothing is changed) if the mask of the
// tiles isn't symmetric on that axis.
bool flip_hex_tiles(ImageRef& tiles,
                    gfx::Point& origin,
                    Mask& mask,
                    const Grid& grid,
                    algorithm::FlipType flipType);

// Flips the positions of the tiles (and the mask if it isn't nullptr)
// around an axis close to the center of "bounds" (e.g. the canvas
// bounds to flip a whole tilemap).
bool flip_hex_tiles_in_bounds(ImageRef& tiles,
                              gfx::Point& origin,
                              Mask* mask,
                              const Grid& grid,
                              algorithm::FlipType flipType,
                              const gfx::Rect& bounds);

} // namespace doc

#endif
