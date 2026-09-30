// Aseprite Document Library
// Copyright (c) 2026-present  Igara Studio S.A.
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "doc/hex_tiles.h"

#include "doc/grid.h"
#include "doc/image.h"
#include "doc/mask.h"
#include "doc/tile.h"

#include <algorithm>
#include <vector>

namespace doc {

namespace {

// Calls f(tile, position) for each non-empty tile, where "position"
// is the position of the tile in the canvas moved by "delta".
template<typename F>
void for_each_moved_tile(const Image* tiles,
                         const gfx::Point& origin,
                         const Grid& grid,
                         const gfx::Point& delta,
                         F&& f)
{
  for (int v = 0; v < tiles->height(); ++v) {
    for (int u = 0; u < tiles->width(); ++u) {
      const tile_t t = tiles->getPixel(u, v);
      if (t != notile)
        f(t, grid.tileToCanvas(origin + gfx::Point(u, v)) + delta);
    }
  }
}

// Returns n/d (d > 0) rounded to the nearest integer, where ties go
// to the even integer. This makes the result the same for a value and
// its mirror around the result (so flipping twice gives the original
// tiles).
int div_round_half_even(const int n, const int d)
{
  int q = n / d;
  int r = n % d;
  if (r < 0) {
    r += d;
    --q;
  }
  if (2 * r > d || (2 * r == d && (q & 1)))
    ++q;
  return q;
}

} // anonymous namespace

gfx::Point hex_tiles_delta(const Grid& grid, const gfx::Point& from, const gfx::Point& to)
{
  return grid.tileToCanvas(grid.canvasToTile(to)) - grid.tileToCanvas(grid.canvasToTile(from));
}

gfx::Rect moved_hex_tiles_bounds(const Image* tiles,
                                 const gfx::Point& origin,
                                 const Grid& grid,
                                 const gfx::Point& delta)
{
  gfx::Rect bounds;
  for_each_moved_tile(tiles, origin, grid, delta, [&](tile_t, const gfx::Point& pos) {
    // The position is exactly the position of a tile, so the tile
    // can be found without the mask
    bounds |= gfx::Rect(grid.originToTile(pos), gfx::Size(1, 1));
  });
  return bounds;
}

void copy_moved_hex_tiles(Image* dst,
                          const gfx::Point& dstOrigin,
                          const Image* tiles,
                          const gfx::Point& origin,
                          const Grid& grid,
                          const gfx::Point& delta)
{
  const gfx::Rect dstBounds = dst->bounds();
  for_each_moved_tile(tiles, origin, grid, delta, [&](tile_t t, const gfx::Point& pos) {
    const gfx::Point pt = grid.originToTile(pos) - dstOrigin;
    if (dstBounds.contains(pt))
      dst->putPixel(pt.x, pt.y, t);
  });
}

bool find_hex_tiles_origin(const Image* tiles,
                           const Mask* mask,
                           const Grid& grid,
                           gfx::Point& origin)
{
  const gfx::Size period = grid.staggerPeriod();
  const gfx::Point base = grid.alignTilesToStagger(grid.canvasToTile(mask->bounds())).origin();
  origin = base;

  // Try the tiles around the tile at the top-left corner of the mask
  // (keeping the alignment to the stagger)
  for (int dv = 0; dv <= 2; ++dv) {
    for (int du = 0; du <= 2; ++du) {
      for (int sv : { 1, -1 }) {
        for (int su : { 1, -1 }) {
          const gfx::Point candidate(base.x + su * du * period.w, base.y + sv * dv * period.h);
          bool found = true;
          bool any = false;
          for (int v = 0; v < tiles->height() && found; ++v) {
            for (int u = 0; u < tiles->width() && found; ++u) {
              if (tiles->getPixel(u, v) == notile)
                continue;
              const gfx::Point center = grid.tileToCanvas(candidate + gfx::Point(u, v)) +
                                        grid.tileCenter();
              any = true;
              if (!mask->containsPoint(center.x, center.y))
                found = false;
            }
          }
          if (found && any) {
            origin = candidate;
            return true;
          }
        }
      }
    }
  }
  return false;
}

bool flip_hex_tiles(ImageRef& tiles,
                    gfx::Point& origin,
                    Mask& mask,
                    const Grid& grid,
                    const algorithm::FlipType flipType)
{
  if (!mask.bitmap())
    return false;
  return flip_hex_tiles_in_bounds(tiles, origin, &mask, grid, flipType, mask.bounds());
}

bool flip_hex_tiles_in_bounds(ImageRef& tiles,
                              gfx::Point& origin,
                              Mask* mask,
                              const Grid& grid,
                              const algorithm::FlipType flipType,
                              const gfx::Rect& bounds)
{
  if (!grid.isHexagonal() ||
      (flipType != algorithm::FlipHorizontal && flipType != algorithm::FlipVertical))
    return false;

  const tile_flags validFlags = grid.validTileFlags();
  if ((flipType == algorithm::FlipHorizontal && !(validFlags & tile_f_xflip)) ||
      (flipType == algorithm::FlipVertical && !(validFlags & tile_f_yflip)))
    return false;

  // Flip along the stagger axis (rows of pointy-top grids, columns of
  // flat-top grids) or across it
  const bool flat = (grid.type() == Grid::Type::HexFlatTop);
  const bool alongStagger = ((flipType == algorithm::FlipVertical) != flat);

  // Size and step in the flipped axis
  const int size = (flipType == algorithm::FlipHorizontal ? grid.tileSize().w : grid.tileSize().h);
  const int step = (flipType == algorithm::FlipHorizontal ? grid.tileOffset().x :
                                                            grid.tileOffset().y);
  const int base = (flipType == algorithm::FlipHorizontal ? grid.origin().x : grid.origin().y);
  // Two times the center of the bounds
  const int center2 = (flipType == algorithm::FlipHorizontal ? 2 * bounds.x + bounds.w :
                                                               2 * bounds.y + bounds.h);

  // Each pixel "p" is moved to "m - p", so a tile at position "a"
  // goes to "m - a - size + 1" (its last pixel is its new first
  // pixel), which must be the position of another tile:
  // - across the stagger, "m - size + 1 - 2*base" must be a multiple
  //   of the step (a staggered tile goes to a staggered position if
  //   the stagger offset is exactly half the step, which is checked
  //   later for each tile),
  // - along the stagger, it must be an even multiple of the step (so
  //   staggered lines go to staggered lines).
  // The axis is the closest one to the center of the bounds.
  int m;
  if (alongStagger) {
    const int j = div_round_half_even(center2 - 2 * base - size, 2 * step);
    m = 2 * base + 2 * j * step + size - 1;
  }
  else {
    const int k = div_round_half_even(center2 - 2 * base - size, step);
    m = 2 * base + k * step + size - 1;
  }

  auto flipPoint = [flipType, m](const gfx::Point& pt) {
    return (flipType == algorithm::FlipHorizontal ? gfx::Point(m - pt.x, pt.y) :
                                                    gfx::Point(pt.x, m - pt.y));
  };

  // Flip the positions of the tiles
  const Image* src = tiles.get();
  gfx::Rect newBounds;
  std::vector<std::pair<gfx::Point, tile_t>> newTiles;
  for (int v = 0; v < src->height(); ++v) {
    for (int u = 0; u < src->width(); ++u) {
      const tile_t t = src->getPixel(u, v);
      if (t == notile)
        continue;
      gfx::Point pos = grid.tileToCanvas(origin + gfx::Point(u, v));
      // Flip the position of the tile (its last pixel goes to the
      // first one)
      pos = flipPoint(pos);
      if (flipType == algorithm::FlipHorizontal)
        pos.x -= size - 1;
      else
        pos.y -= size - 1;
      const gfx::Point tile = grid.originToTile(pos);
      if (grid.tileToCanvas(tile) != pos) // Not a position of a tile
        return false;
      newTiles.push_back(std::make_pair(tile, t));
      newBounds |= gfx::Rect(tile, gfx::Size(1, 1));
    }
  }

  // Flip the mask
  Mask newMask;
  if (mask) {
    const gfx::Rect bounds = mask->bounds();
    const gfx::Point a = flipPoint(bounds.origin());
    const gfx::Point b = flipPoint(bounds.point2() - gfx::Point(1, 1));
    const gfx::Rect newMaskBounds(std::min(a.x, b.x), std::min(a.y, b.y), bounds.w, bounds.h);
    newMask.replace(newMaskBounds);
    Image* bitmap = newMask.bitmap();
    bitmap->clear(0);
    for (int y = bounds.y; y < bounds.y2(); ++y) {
      for (int x = bounds.x; x < bounds.x2(); ++x) {
        if (mask->containsPoint(x, y)) {
          const gfx::Point pt = flipPoint(gfx::Point(x, y)) - newMaskBounds.origin();
          bitmap->putPixel(pt.x, pt.y, 1);
        }
      }
    }
  }

  if (!newBounds.isEmpty()) {
    newBounds = grid.alignTilesToStagger(newBounds);
    ImageRef newImage(Image::create(IMAGE_TILEMAP, newBounds.w, newBounds.h));
    newImage->setMaskColor(notile);
    newImage->clear(notile);
    for (const auto& it : newTiles) {
      const gfx::Point pt = it.first - newBounds.origin();
      newImage->putPixel(pt.x, pt.y, it.second);
    }
    tiles = newImage;
    origin = newBounds.origin();
  }
  if (mask)
    mask->copyFrom(&newMask);
  return true;
}

} // namespace doc
