// Aseprite Document Library
// Copyright (c) 2019-present  Igara Studio S.A.
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "doc/grid.h"

#include "doc/image.h"
#include "doc/image_ref.h"
#include "doc/primitives.h"
#include "gfx/point.h"
#include "gfx/rect.h"
#include "gfx/region.h"
#include "gfx/size.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <vector>

namespace doc {

namespace {

int floor_div(const int a, const int b)
{
  ASSERT(b > 0);
  const int q = a / b;
  return ((a % b) != 0 && a < 0 ? q - 1 : q);
}

int ceil_div(const int a, const int b)
{
  return -floor_div(-a, b);
}

gfx::Region mask_region(const Image* mask)
{
  gfx::Region rgn;
  for (int y = 0; y < mask->height(); ++y) {
    int x0 = -1;
    for (int x = 0; x <= mask->width(); ++x) {
      const bool on = (x < mask->width() && mask->getPixel(x, y));
      if (on && x0 < 0)
        x0 = x;
      else if (!on && x0 >= 0) {
        rgn |= gfx::Region(gfx::Rect(x0, y, x - x0, 1));
        x0 = -1;
      }
    }
  }
  return rgn;
}

// Geometry of a hexagonal grid in "stagger space": "v" is the stagger
// axis (rows of a pointy-top grid, columns of a flat-top grid) and
// "u" is the other axis. A flat-top grid is a transposed pointy-top
// grid, so points/sizes/rects are transposed with uv() on the way in
// and out (uv() is its own inverse).
class HexGeom {
public:
  explicit HexGeom(const Grid& grid)
    : m_grid(grid)
    , m_flat(grid.type() == Grid::Type::HexFlatTop)
    , m_origin(uv(grid.origin()))
    , m_size(uv(grid.tileSize()))
    , m_step(uv(grid.tileOffset()))
    , m_shift(m_flat ? grid.oddColOffset().y : grid.oddRowOffset().x)
    , m_mask(grid.mask().get())
  {
    ASSERT(grid.isHexagonal());
    ASSERT(m_step.x > 0 && m_step.y > 0);

    if (m_mask) {
      m_emptyMask = true;
      for (int y = 0; y < m_mask->height() && m_emptyMask; ++y)
        for (int x = 0; x < m_mask->width() && m_emptyMask; ++x)
          if (m_mask->getPixel(x, y))
            m_emptyMask = false;
    }
  }

  gfx::Point uv(const gfx::Point& pt) const { return (m_flat ? gfx::Point(pt.y, pt.x) : pt); }
  gfx::Size uv(const gfx::Size& sz) const { return (m_flat ? gfx::Size(sz.h, sz.w) : sz); }
  gfx::Rect uv(const gfx::Rect& rc) const
  {
    return (m_flat ? gfx::Rect(rc.y, rc.x, rc.h, rc.w) : rc);
  }

  // Offset of the tiles in the given line (a row in stagger space)
  int shift(const int v) const { return (m_grid.isStaggered(v) ? m_shift : 0); }

  // Position of the tile (c, r)
  gfx::Point tileOrigin(const gfx::Point& tile) const
  {
    return gfx::Point(m_origin.x + tile.x * m_step.x + shift(tile.y),
                      m_origin.y + tile.y * m_step.y);
  }

  // Mask pixel in tile-local coordinates
  bool maskAt(const int u, const int v) const
  {
    if (!m_mask)
      return true;
    const int x = (m_flat ? v : u);
    const int y = (m_flat ? u : v);
    if (x < 0 || y < 0 || x >= m_mask->width() || y >= m_mask->height())
      return false;
    return (m_mask->getPixel(x, y) != 0);
  }

  // Returns the tile that owns the given pixel
  gfx::Point owner(const gfx::Point& pt) const
  {
    const int rMin = ceil_div(pt.y - m_origin.y - m_size.h + 1, m_step.y);
    const int rMax = floor_div(pt.y - m_origin.y, m_step.y);
    for (int r = rMin; r <= rMax; ++r) {
      const int sh = shift(r);
      const int cMin = ceil_div(pt.x - m_origin.x - sh - m_size.w + 1, m_step.x);
      const int cMax = floor_div(pt.x - m_origin.x - sh, m_step.x);
      for (int c = cMin; c <= cMax; ++c) {
        const gfx::Point tile(c, r);
        const gfx::Point pos = tileOrigin(tile);
        if (maskAt(pt.x - pos.x, pt.y - pos.y))
          return tile;
      }
    }
    // The mask has a gap here, return the tile with the nearest
    // position before this pixel.
    return gfx::Point(floor_div(pt.x - m_origin.x - shift(rMax), m_step.x), rMax);
  }

  // True if the mask of the given tile has pixels inside "rc"
  bool touches(const gfx::Point& tile, const gfx::Rect& rc) const
  {
    const gfx::Rect bounds(tileOrigin(tile), m_size);
    const gfx::Rect clip = (bounds & rc);
    if (clip.isEmpty())
      return false;
    if (clip == bounds)
      return !m_emptyMask;
    for (int v = clip.y; v < clip.y2(); ++v)
      for (int u = clip.x; u < clip.x2(); ++u)
        if (maskAt(u - bounds.x, v - bounds.y))
          return true;
    return false;
  }

  // Calls f(tile) for each tile whose mask touches the given rectangle
  template<typename F>
  void forEachTileTouching(const gfx::Rect& rc, F&& f) const
  {
    if (rc.isEmpty())
      return;
    const int rMin = ceil_div(rc.y - m_origin.y - m_size.h + 1, m_step.y);
    const int rMax = floor_div(rc.y2() - 1 - m_origin.y, m_step.y);
    for (int r = rMin; r <= rMax; ++r) {
      const int sh = shift(r);
      const int cMin = ceil_div(rc.x - m_origin.x - sh - m_size.w + 1, m_step.x);
      const int cMax = floor_div(rc.x2() - 1 - m_origin.x - sh, m_step.x);
      for (int c = cMin; c <= cMax; ++c) {
        const gfx::Point tile(c, r);
        if (touches(tile, rc))
          f(tile);
      }
    }
  }

  // Union of the bounds of the given range of tiles
  gfx::Rect tilesBounds(const gfx::Rect& tiles) const
  {
    int shMin, shMax;
    if (tiles.h > 1) {
      shMin = std::min(0, m_shift);
      shMax = std::max(0, m_shift);
    }
    else {
      shMin = shMax = shift(tiles.y);
    }
    const int u0 = m_origin.x + tiles.x * m_step.x + shMin;
    const int u1 = m_origin.x + (tiles.x2() - 1) * m_step.x + m_size.w + shMax;
    const int v0 = m_origin.y + tiles.y * m_step.y;
    const int v1 = m_origin.y + (tiles.y2() - 1) * m_step.y + m_size.h;
    return gfx::Rect(u0, v0, u1 - u0, v1 - v0);
  }

  // Size of a tilemap measured from the position of its first tile
  gfx::Size tilemapSize(const gfx::Size& tiles) const
  {
    const int shMax = (tiles.h > 1 ? std::max(0, m_shift) : (tiles.h == 1 ? shift(0) : 0));
    return gfx::Size(tiles.w > 0 ? (tiles.w - 1) * m_step.x + m_size.w + shMax : 0,
                     tiles.h > 0 ? (tiles.h - 1) * m_step.y + m_size.h : 0);
  }

private:
  const Grid& m_grid;
  bool m_flat;
  gfx::Point m_origin;
  gfx::Size m_size;
  gfx::Point m_step;
  int m_shift;
  const Image* m_mask;
  bool m_emptyMask = false;
};

} // anonymous namespace

// static
Grid Grid::MakeRect(const gfx::Size& sz)
{
  return Grid(sz);
}

// static
Grid Grid::MakeRect(const gfx::Rect& rc)
{
  return Grid(rc);
}

// static
Grid Grid::MakeHex(const gfx::Size& sz,
                   const Type type,
                   const Parity parity,
                   int step,
                   const ImageRef& mask,
                   TessellationError* error)
{
  if (error)
    *error = TessellationError();

  ASSERT(sz.w > 0 && sz.h > 0);
  if (!isHexagonal(type) || sz.w < 1 || sz.h < 1)
    return Grid(sz);

  step = (step > 0 ? std::clamp(step, MinHexStep(sz, type), MaxHexStep(sz, type)) :
                     DefaultHexStep(sz, type));

  Grid grid(sz);
  grid.m_type = type;
  grid.m_parity = parity;
  if (type == Type::HexPointyTop) {
    grid.m_tileOffset = gfx::Point(sz.w, step);
    grid.m_oddRowOffset = gfx::Point(sz.w / 2, 0);
  }
  else {
    grid.m_tileOffset = gfx::Point(step, sz.h);
    grid.m_oddColOffset = gfx::Point(0, sz.h / 2);
  }

  if (mask) {
    grid.m_mask = mask;
    if (!grid.validateTessellation(error))
      grid.m_mask = MakeHexMask(sz, type, step);
  }
  else {
    grid.m_mask = MakeHexMask(sz, type, step);
  }

  // The tile center must be a pixel of the tile (a custom mask could
  // exclude the center), so we use the nearest pixel of the mask.
  const Image* m = grid.m_mask.get();
  if (!m->getPixel(grid.m_tileCenter.x, grid.m_tileCenter.y)) {
    int bestDist = std::numeric_limits<int>::max();
    for (int y = 0; y < m->height(); ++y) {
      for (int x = 0; x < m->width(); ++x) {
        const int dx = x - grid.m_tileCenter.x;
        const int dy = y - grid.m_tileCenter.y;
        if (m->getPixel(x, y) && dx * dx + dy * dy < bestDist) {
          bestDist = dx * dx + dy * dy;
          grid.m_tileCenter = gfx::Point(x, y);
        }
      }
    }
  }
  return grid;
}

// static
int Grid::DefaultHexStep(const gfx::Size& sz, const Type type)
{
  const int h = (type == Type::HexFlatTop ? sz.w : sz.h);
  return std::max(1, h - h / 4);
}

// static
int Grid::MinHexStep(const gfx::Size& sz, const Type type)
{
  // The step must be greater than half the height: with step == h/2
  // no row of the mask has the full width (so tiles in the same row
  // don't touch) and the tips touch the tiles two rows away (a
  // diamond tiling instead of a hexagonal one).
  const int h = (type == Type::HexFlatTop ? sz.w : sz.h);
  return std::min(h / 2 + 1, std::max(1, h));
}

// static
int Grid::MaxHexStep(const gfx::Size& sz, const Type type)
{
  const int h = (type == Type::HexFlatTop ? sz.w : sz.h);
  return std::max(1, h);
}

// static
ImageRef Grid::MakeHexMask(const gfx::Size& sz, const Type type, int step)
{
  const bool flat = (type == Type::HexFlatTop);

  // Size in stagger space (the mask of a flat-top hexagon is the
  // transposed mask of a pointy-top one)
  const int w = std::max(1, flat ? sz.h : sz.w);
  const int h = std::max(1, flat ? sz.w : sz.h);
  step = std::clamp(step, MinHexStep(sz, type), MaxHexStep(sz, type));

  // Rows shared with the tiles above (at the top) and below (at the
  // bottom)
  const int overlap = h - step;

  std::vector<uint8_t> pixels(w * h, 0);
  auto at = [&pixels, w](const int u, const int v) -> uint8_t& { return pixels[v * w + u]; };

  if (w & 1) {
    // With an odd width, rows are staggered by w/2 but the rows
    // below are w/2+1 pixels apart from the next rows, so there is no
    // shape with slanted edges that fits both. The only valid tiling
    // is to give each row of the overlap to one tile (bricks).
    const int top = overlap / 2;
    for (int v = top; v < top + step; ++v)
      for (int u = 0; u < w; ++u)
        at(u, v) = 1;
  }
  else {
    // Top edge: centered spans that grow up to the full width to
    // approximate the slanted edges of a regular hexagon. The second
    // half of the rows mirrors the first half so the shape is
    // symmetric vertically (when it's possible).
    std::vector<int> widths(overlap);
    for (int k = 0; k < overlap; ++k) {
      const int mirror = overlap - 1 - k;
      if (mirror < k)
        widths[k] = w - widths[mirror];
      else
        widths[k] = 2 * int(std::floor(double(w) * (2 * k + 1) / (4.0 * overlap) + 0.5));
      widths[k] = std::clamp(widths[k], 0, w);
    }
    for (int k = 0; k < overlap; ++k)
      for (int u = (w - widths[k]) / 2; u < (w + widths[k]) / 2; ++u)
        at(u, k) = 1;

    // Middle rows: no other tile covers them
    for (int v = overlap; v < step; ++v)
      for (int u = 0; u < w; ++u)
        at(u, v) = 1;

    // Bottom edge: the complement of the top edge of the two tiles
    // below (which are shifted by half a tile), so they fit exactly.
    for (int k = 0; k < overlap; ++k)
      for (int u = 0; u < w; ++u)
        at(u, step + k) = (at((u + w / 2) % w, k) ? 0 : 1);
  }

  ImageRef mask(Image::create(IMAGE_BITMAP, flat ? h : w, flat ? w : h));
  for (int v = 0; v < h; ++v)
    for (int u = 0; u < w; ++u)
      mask->putPixel(flat ? v : u, flat ? u : v, at(u, v));
  return mask;
}

int Grid::hexStep() const
{
  switch (m_type) {
    case Type::HexPointyTop: return m_tileOffset.y;
    case Type::HexFlatTop:   return m_tileOffset.x;
    default:                 return 0;
  }
}

bool Grid::isStaggered(const int i) const
{
  const bool odd = ((i & 1) != 0);
  return (m_parity == Parity::Odd ? odd : !odd);
}

// Converts a tile position into a canvas position
gfx::Point Grid::tileToCanvas(const gfx::Point& tile) const
{
  gfx::Point result;
  result.x = tile.x * m_tileOffset.x + m_origin.x;
  result.y = tile.y * m_tileOffset.y + m_origin.y;
  if (isStaggered(tile.y)) // Staggered (odd) row
    result += m_oddRowOffset;
  if (isStaggered(tile.x)) // Staggered (odd) column
    result += m_oddColOffset;
  return result;
}

gfx::Rect Grid::tileToCanvas(const gfx::Rect& tileBounds) const
{
  if (isHexagonal()) {
    if (tileBounds.w < 1 || tileBounds.h < 1)
      return gfx::Rect(tileToCanvas(tileBounds.origin()), gfx::Size(0, 0));

    const HexGeom hex(*this);
    return hex.uv(hex.tilesBounds(hex.uv(tileBounds)));
  }

  gfx::Point pt1 = tileToCanvas(tileBounds.origin());
  gfx::Point pt2 = tileToCanvas(tileBounds.point2());
  return gfx::Rect(pt1, pt2);
}

gfx::Region Grid::tileToCanvas(const gfx::Region& tileRgn)
{
  gfx::Region canvasRgn;
  for (const gfx::Rect& rc : tileRgn) {
    canvasRgn |= gfx::Region(tileToCanvas(rc));
  }
  return canvasRgn;
}

gfx::Point Grid::canvasToTile(const gfx::Point& canvasPoint) const
{
  ASSERT(m_tileSize.w > 0);
  ASSERT(m_tileSize.h > 0);
  if (m_tileSize.w < 1 || m_tileSize.h < 1)
    return canvasPoint;

  // In hexagonal grids the owner is the tile whose mask contains the
  // pixel.
  if (isHexagonal()) {
    const HexGeom hex(*this);
    return hex.uv(hex.owner(hex.uv(canvasPoint)));
  }

  gfx::Point tile;
  std::div_t divx = std::div((canvasPoint.x - m_origin.x), m_tileSize.w);
  std::div_t divy = std::div((canvasPoint.y - m_origin.y), m_tileSize.h);
  tile.x = divx.quot;
  tile.y = divy.quot;
  if (canvasPoint.x < m_origin.x && divx.rem)
    --tile.x;
  if (canvasPoint.y < m_origin.y && divy.rem)
    --tile.y;

  if (m_oddRowOffset.x != 0 || m_oddRowOffset.y != 0 || m_oddColOffset.x != 0 ||
      m_oddColOffset.y != 0) {
    gfx::Point bestTile = tile;
    int bestDist = std::numeric_limits<int>::max();
    for (int v = -1; v <= 2; ++v) {
      for (int u = -1; u <= 2; ++u) {
        gfx::Point neighbor(tile.x + u, tile.y + v);
        gfx::Point neighborCanvas = tileToCanvas(neighbor);

        if (hasMask()) {
          if (doc::get_pixel(m_mask.get(),
                             canvasPoint.x - neighborCanvas.x,
                             canvasPoint.y - neighborCanvas.y))
            return neighbor;
        }

        gfx::Point delta = neighborCanvas + m_tileCenter - canvasPoint;
        int dist = delta.x * delta.x + delta.y * delta.y;
        if (bestDist > dist) {
          bestDist = dist;
          bestTile = neighbor;
        }
      }
    }
    tile = bestTile;
  }

  return tile;
}

gfx::Rect Grid::canvasToTile(const gfx::Rect& canvasBounds) const
{
  // Bounds of the tiles whose masks touch the rectangle
  if (isHexagonal()) {
    const HexGeom hex(*this);
    int c0 = INT_MAX, r0 = INT_MAX, c1 = INT_MIN, r1 = INT_MIN;
    hex.forEachTileTouching(hex.uv(canvasBounds), [&](const gfx::Point& tile) {
      c0 = std::min(c0, tile.x);
      r0 = std::min(r0, tile.y);
      c1 = std::max(c1, tile.x);
      r1 = std::max(r1, tile.y);
    });
    if (c0 > c1)
      return gfx::Rect(originToTile(canvasBounds.origin()), gfx::Size(0, 0));
    return hex.uv(gfx::Rect(c0, r0, c1 - c0 + 1, r1 - r0 + 1));
  }

  gfx::Point pt1 = canvasToTile(canvasBounds.origin());
  gfx::Point pt2 = canvasToTile(gfx::Point(canvasBounds.x2() - 1, canvasBounds.y2() - 1));
  return gfx::Rect(pt1, gfx::Size(pt2.x - pt1.x + 1, pt2.y - pt1.y + 1));
}

gfx::Region Grid::canvasToTile(const gfx::Region& canvasRgn)
{
  gfx::Region tilesRgn;

  if (isHexagonal()) {
    // Join consecutive tiles of each row in one rectangle
    gfx::Rect run;
    for (const gfx::Point& pt : tilesInCanvasRegion(canvasRgn)) {
      if (!run.isEmpty() && pt.y == run.y && pt.x == run.x2()) {
        ++run.w;
      }
      else {
        if (!run.isEmpty())
          tilesRgn |= gfx::Region(run);
        run = gfx::Rect(pt, gfx::Size(1, 1));
      }
    }
    if (!run.isEmpty())
      tilesRgn |= gfx::Region(run);
    return tilesRgn;
  }

  for (const gfx::Rect& rc : canvasRgn) {
    tilesRgn |= gfx::Region(canvasToTile(rc));
  }
  return tilesRgn;
}

gfx::Point Grid::originToTile(const gfx::Point& canvasPoint) const
{
  if (!isHexagonal())
    return canvasToTile(canvasPoint);

  ASSERT(m_tileOffset.x > 0 && m_tileOffset.y > 0);
  return gfx::Point(floor_div(canvasPoint.x - m_origin.x, m_tileOffset.x),
                    floor_div(canvasPoint.y - m_origin.y, m_tileOffset.y));
}

gfx::Size Grid::staggerPeriod() const
{
  return gfx::Size(m_oddColOffset != gfx::Point(0, 0) ? 2 : 1,
                   m_oddRowOffset != gfx::Point(0, 0) ? 2 : 1);
}

gfx::Rect Grid::alignTilesToStagger(const gfx::Rect& tiles) const
{
  const gfx::Size period = staggerPeriod();
  const int x = floor_div(tiles.x, period.w) * period.w;
  const int y = floor_div(tiles.y, period.h) * period.h;
  return gfx::Rect(x, y, tiles.x2() - x, tiles.y2() - y);
}

gfx::Point Grid::tilemapPosition(const gfx::Point& firstTile) const
{
  if (!isHexagonal())
    return tileToCanvas(firstTile);

  ASSERT(alignTilesToStagger(gfx::Rect(firstTile, gfx::Size(1, 1))).origin() == firstTile);
  return gfx::Point(m_origin.x + firstTile.x * m_tileOffset.x,
                    m_origin.y + firstTile.y * m_tileOffset.y);
}

gfx::Size Grid::tilemapSizeToCanvas(const gfx::Size& tilemapSize) const
{
  if (isHexagonal()) {
    const HexGeom hex(*this);
    return hex.uv(hex.tilemapSize(hex.uv(tilemapSize)));
  }

  return gfx::Size(tilemapSize.w * m_tileSize.w, tilemapSize.h * m_tileSize.h);
}

gfx::Rect Grid::tileBoundsInCanvas(const gfx::Point& tile) const
{
  return gfx::Rect(tileToCanvas(tile), m_tileSize);
}

gfx::Rect Grid::alignBounds(const gfx::Rect& bounds) const
{
  if (isHexagonal())
    return tileToCanvas(canvasToTile(bounds));

  gfx::Point pt1 = canvasToTile(bounds.origin());
  gfx::Point pt2 = canvasToTile(gfx::Point(bounds.x2() - 1, bounds.y2() - 1));
  return tileBoundsInCanvas(pt1) | tileBoundsInCanvas(pt2);
}

gfx::Region Grid::tileRegionInCanvas(const gfx::Point& tile) const
{
  if (!hasMask())
    return gfx::Region(tileBoundsInCanvas(tile));

  gfx::Region rgn = mask_region(m_mask.get());
  rgn.offset(tileToCanvas(tile));
  return rgn;
}

gfx::Region Grid::tilesRegionInCanvas(const std::vector<gfx::Point>& tiles) const
{
  gfx::Region result;
  if (!hasMask()) {
    for (const gfx::Point& tile : tiles)
      result |= gfx::Region(tileBoundsInCanvas(tile));
    return result;
  }

  const gfx::Region maskRgn = mask_region(m_mask.get());
  for (const gfx::Point& tile : tiles) {
    gfx::Region rgn(maskRgn);
    rgn.offset(tileToCanvas(tile));
    result |= rgn;
  }
  return result;
}

std::vector<gfx::Point> Grid::tilesInCanvasRegion(const gfx::Region& rgn) const
{
  std::vector<gfx::Point> result;
  if (rgn.isEmpty())
    return result;

  const gfx::Rect bounds = canvasToTile(rgn.bounds());
  if (bounds.w < 1 || bounds.h < 1)
    return result;

  ImageRef tmp(Image::create(IMAGE_BITMAP, bounds.w, bounds.h));
  const gfx::Rect tmpBounds = tmp->bounds();
  tmp->clear(0);

  if (isHexagonal()) {
    const HexGeom hex(*this);
    for (const gfx::Rect& rc : rgn) {
      hex.forEachTileTouching(hex.uv(rc), [&](const gfx::Point& tileUV) {
        const gfx::Point pt = hex.uv(tileUV) - bounds.origin();
        if (tmpBounds.contains(pt))
          tmp->putPixel(pt.x, pt.y, 1);
      });
    }
  }
  else {
    for (const gfx::Rect& rc : rgn) {
      gfx::Rect tileBounds = canvasToTile(rc);
      tileBounds.x -= bounds.x;
      tileBounds.y -= bounds.y;
      tileBounds &= tmpBounds;
      if (!tileBounds.isEmpty())
        tmp->fillRect(tileBounds.x, tileBounds.y, tileBounds.x2() - 1, tileBounds.y2() - 1, 1);
    }
  }

  const LockImageBits<BitmapTraits> bits(tmp.get());
  for (auto it = bits.begin(), end = bits.end(); it != end; ++it) {
    if (*it)
      result.push_back(gfx::Point(it.x() + bounds.x, it.y() + bounds.y));
  }
  return result;
}

bool Grid::validateTessellation(TessellationError* error) const
{
  TessellationError err;
  auto fail = [&err, error](const TessellationError::Kind kind) {
    err.kind = kind;
    if (error)
      *error = err;
    return false;
  };

  if (m_tileSize.w < 1 || m_tileSize.h < 1 || m_tileOffset.x < 1 || m_tileOffset.y < 1)
    return fail(TessellationError::Kind::BadGrid);

  if (hasMask() && (m_mask->pixelFormat() != IMAGE_BITMAP || m_mask->size() != m_tileSize))
    return fail(TessellationError::Kind::BadMask);

  // The tiling repeats every two rows/columns when they are staggered
  const bool staggeredCols = (m_oddColOffset != gfx::Point(0, 0));
  const bool staggeredRows = (m_oddRowOffset != gfx::Point(0, 0));
  const gfx::Size period(m_tileOffset.x * (staggeredCols ? 2 : 1),
                         m_tileOffset.y * (staggeredRows ? 2 : 1));

  // Simulate 2x2 periods, counting the tiles that cover each pixel
  const gfx::Rect area(m_origin, gfx::Size(2 * period.w, 2 * period.h));
  std::vector<uint8_t> coverage(area.w * area.h, 0);

  const int maxShiftX = std::abs(m_oddRowOffset.x) + std::abs(m_oddColOffset.x);
  const int maxShiftY = std::abs(m_oddRowOffset.y) + std::abs(m_oddColOffset.y);
  const int c0 = floor_div(area.x - m_origin.x - m_tileSize.w - maxShiftX, m_tileOffset.x);
  const int r0 = floor_div(area.y - m_origin.y - m_tileSize.h - maxShiftY, m_tileOffset.y);
  const int c1 = floor_div(area.x2() - m_origin.x + maxShiftX, m_tileOffset.x) + 1;
  const int r1 = floor_div(area.y2() - m_origin.y + maxShiftY, m_tileOffset.y) + 1;

  for (int r = r0; r <= r1; ++r) {
    for (int c = c0; c <= c1; ++c) {
      const gfx::Rect bounds = tileBoundsInCanvas(gfx::Point(c, r));
      const gfx::Rect clip = (bounds & area);
      for (int y = clip.y; y < clip.y2(); ++y) {
        for (int x = clip.x; x < clip.x2(); ++x) {
          if (!hasMask() || m_mask->getPixel(x - bounds.x, y - bounds.y)) {
            uint8_t& n = coverage[(y - area.y) * area.w + (x - area.x)];
            if (n < 255)
              ++n;
          }
        }
      }
    }
  }

  for (int y = 0; y < area.h; ++y) {
    for (int x = 0; x < area.w; ++x) {
      const int n = coverage[y * area.w + x];
      if (n != 1) {
        if (err.count == 0) {
          err.pos = gfx::Point(area.x + x, area.y + y);
          err.coverage = n;
        }
        ++err.count;
      }
    }
  }

  if (err.count > 0)
    return fail(err.coverage == 0 ? TessellationError::Kind::Gap :
                                    TessellationError::Kind::Overlap);

  if (error)
    *error = err;
  return true;
}

tile_flags Grid::validTileFlags() const
{
  if (!isHexagonal())
    return tile_f_xflip | tile_f_yflip | tile_f_dflip;

  tile_flags flags = tile_f_xflip | tile_f_yflip;
  if (hasMask()) {
    const Image* mask = m_mask.get();
    const int w = mask->width();
    const int h = mask->height();
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        const color_t c = mask->getPixel(x, y);
        if (c != mask->getPixel(w - 1 - x, y))
          flags &= ~tile_f_xflip;
        if (c != mask->getPixel(x, h - 1 - y))
          flags &= ~tile_f_yflip;
      }
    }
  }
  return flags;
}

bool Grid::operator==(const Grid& other) const
{
  return m_tileSize == other.m_tileSize && m_type == other.m_type && m_parity == other.m_parity &&
         m_origin == other.m_origin && m_tileCenter == other.m_tileCenter &&
         m_tileOffset == other.m_tileOffset && m_oddRowOffset == other.m_oddRowOffset &&
         m_oddColOffset == other.m_oddColOffset && hasMask() == other.hasMask() &&
         (!m_mask || is_same_image(m_mask.get(), other.m_mask.get()));
}

std::array<gfx::Point, 6> hex_neighbors(const gfx::Point& tile, const Grid& grid)
{
  ASSERT(grid.isHexagonal());

  // In stagger space (see HexGeom) the neighbors are the two tiles in
  // the same line, and two tiles in the previous and next lines. A
  // staggered line is shifted forward by half a tile, so its
  // neighbors are at c and c+1, otherwise they are at c-1 and c.
  const bool flat = (grid.type() == Grid::Type::HexFlatTop);
  const gfx::Point t = (flat ? gfx::Point(tile.y, tile.x) : tile);
  const int d = (grid.isStaggered(t.y) ? 0 : -1);

  std::array<gfx::Point, 6> result = {
    gfx::Point(t.x + 1, t.y),     gfx::Point(t.x - 1, t.y),
    gfx::Point(t.x + d, t.y - 1), gfx::Point(t.x + d + 1, t.y - 1),
    gfx::Point(t.x + d, t.y + 1), gfx::Point(t.x + d + 1, t.y + 1),
  };
  if (flat) {
    for (gfx::Point& pt : result)
      std::swap(pt.x, pt.y);
  }
  return result;
}

} // namespace doc
