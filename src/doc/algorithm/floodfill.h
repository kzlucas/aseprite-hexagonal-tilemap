// Aseprite Document Library
// Copyright (c) 2001-2017 David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifndef DOC_ALGORITHM_FLOODFILL_H_INCLUDED
#define DOC_ALGORITHM_FLOODFILL_H_INCLUDED
#pragma once

#include "doc/algorithm/hline.h"
#include "doc/color.h"
#include "gfx/fwd.h"

namespace doc {

class Grid;
class Image;
class Mask;

namespace algorithm {

void floodfill(const Image* image,
               const Mask* mask,
               const int x,
               const int y,
               const gfx::Rect& bounds,
               const doc::color_t srcColor,
               const int tolerance,
               const bool contiguous,
               const bool isEightConnected,
               void* data,
               AlgoHLine proc);

// Fills the contiguous tiles of a tilemap in a hexagonal grid (with
// the 6 neighbors of each tile, see hex_neighbors()) starting at the
// tile x, y with the same value. "grid" places the tiles of the
// tilemap in the canvas: if "mask" isn't nullptr, only tiles with
// their center inside the mask are filled. "proc" is called for each
// filled tile (a line of one tile).
void hex_floodfill(const Image* tilemap,
                   const Grid& grid,
                   const Mask* mask,
                   const int x,
                   const int y,
                   void* data,
                   AlgoHLine proc);

} // namespace algorithm
} // namespace doc

#endif
