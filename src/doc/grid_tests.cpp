// Aseprite Document Library
// Copyright (c) 2019-2024 Igara Studio S.A.
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include <gtest/gtest.h>

#include "doc/grid.h"
#include "doc/image.h"
#include "doc/mask.h"
#include "doc/util.h"
#include "gfx/rect_io.h"
#include "gfx/region.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

// TODO add gfx/point_io.h and gfx/size_io.h
namespace gfx {

inline std::ostream& operator<<(std::ostream& os, const Point& pt)
{
  return os << "(" << pt.x << ", " << pt.y << ")";
}

inline std::ostream& operator<<(std::ostream& os, const Size& sz)
{
  return os << "(" << sz.w << ", " << sz.h << ")";
}

} // namespace gfx

using namespace doc;
using namespace gfx;

TEST(Grid, Rect)
{
  auto grid = Grid::MakeRect(Size(16, 16));

  EXPECT_EQ(Size(16, 16), grid.tileSize());
  EXPECT_EQ(Point(16, 16), grid.tileOffset());

  EXPECT_EQ(Point(16, 16), grid.tileToCanvas(Point(1, 1)));
  EXPECT_EQ(Point(32, 16), grid.tileToCanvas(Point(2, 1)));
  EXPECT_EQ(Point(0, 0), grid.canvasToTile(Point(5, 5)));
  EXPECT_EQ(Point(1, 1), grid.canvasToTile(Point(16, 16)));
  EXPECT_EQ(Rect(1, 0, 1, 2), grid.canvasToTile(Rect(16, 5, 16, 16)));

  EXPECT_EQ(Rect(0, 0, 32, 32), grid.alignBounds(Rect(4, 5, 16, 17)));
  EXPECT_EQ(Rect(16, 16, 16, 16), grid.alignBounds(Rect(16, 16, 16, 16)));
  EXPECT_EQ(Rect(16, 16, 32, 16), grid.alignBounds(Rect(16, 16, 17, 16)));
  EXPECT_EQ(Rect(16, 16, 48, 16), grid.alignBounds(Rect(17, 16, 32, 16)));

  gfx::Region rgn;
  rgn |= gfx::Region(gfx::Rect(5, 17, 8, 8));
  rgn |= gfx::Region(gfx::Rect(17, 5, 16, 8));
  auto pts = grid.tilesInCanvasRegion(rgn);
  ASSERT_EQ(3, pts.size());
  EXPECT_EQ(gfx::Point(1, 0), pts[0]);
  EXPECT_EQ(gfx::Point(2, 0), pts[1]);
  EXPECT_EQ(gfx::Point(0, 1), pts[2]);
}

TEST(Grid, RectWithOffset)
{
  auto grid = Grid::MakeRect(Size(16, 16));
  grid.origin(gfx::Point(17, 16));

  EXPECT_EQ(Point(0, 0), grid.canvasToTile(Point(17, 16)));
  EXPECT_EQ(Point(0, 0), grid.canvasToTile(Point(17 + 15, 16)));
  EXPECT_EQ(Point(1, 0), grid.canvasToTile(Point(17 + 16, 16)));

  EXPECT_EQ(Point(-1, 0), grid.canvasToTile(Point(16, 16)));
  EXPECT_EQ(Point(-1, 0), grid.canvasToTile(Point(2, 16)));
  EXPECT_EQ(Point(-1, 0), grid.canvasToTile(Point(1, 16)));

  grid.origin(gfx::Point(-1, -1));
  EXPECT_EQ(Rect(1, 1, 1, 1), grid.canvasToTile(Rect(30, 30, 1, 1)));
}

TEST(Grid, MakeAlignedMask)
{
  auto grid = Grid::MakeRect(Size(4, 4));
  grid.origin(gfx::Point(1, 1));
  auto mask = Mask();
  mask.replace(gfx::Rect(3, 3, 4, 4));
  auto gridAlignedMask = make_aligned_mask(&grid, &mask);
  EXPECT_EQ(gfx::Rect(1, 1, 8, 8), gridAlignedMask.bounds());

  mask.replace(gfx::Rect(1, 1, 4, 4));
  auto gridAlignedMask2 = make_aligned_mask(&grid, &mask);
  EXPECT_EQ(gfx::Rect(1, 1, 4, 4), gridAlignedMask2.bounds());

  mask.add(gfx::Rect(8, 4, 1, 1));
  mask.add(gfx::Rect(7, 7, 1, 1));
  auto gridAlignedMask3 = make_aligned_mask(&grid, &mask);
  const bool _ = false;
  const bool X = true;
  bool expected[8 * 8] = {
    X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,

    _, _, _, _, X, X, X, X, _, _, _, _, X, X, X, X, _, _, _, _, X, X, X, X, _, _, _, _, X, X, X, X,
  };
  int c = 0;
  for (int j = 0; j < 8; ++j)
    for (int i = 0; i < 8; ++i)
      EXPECT_EQ(expected[c++], gridAlignedMask3.bitmap()->getPixel(i, j));

  mask.replace(gfx::Rect(4, 4, 1, 1));
  mask.add(gfx::Rect(5, 5, 1, 1));
  mask.add(gfx::Rect(8, 4, 1, 1));
  mask.add(gfx::Rect(9, 1, 1, 1));
  auto gridAlignedMask4 = make_aligned_mask(&grid, &mask);
  bool expected2[12 * 8] = {
    X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,
    X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,

    _, _, _, _, X, X, X, X, _, _, _, _, _, _, _, _, X, X, X, X, _, _, _, _,
    _, _, _, _, X, X, X, X, _, _, _, _, _, _, _, _, X, X, X, X, _, _, _, _,
  };
  c = 0;
  for (int j = 0; j < 8; ++j)
    for (int i = 0; i < 12; ++i)
      EXPECT_EQ(expected2[c++], gridAlignedMask4.bitmap()->getPixel(i, j));

  grid.origin(gfx::Point(2, 1));
  auto gridAlignedMask5 = make_aligned_mask(&grid, &mask);
  bool expected3[8 * 8] = {
    X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,

    X, X, X, X, _, _, _, _, X, X, X, X, _, _, _, _, X, X, X, X, _, _, _, _, X, X, X, X, _, _, _, _,
  };
  c = 0;
  for (int j = 0; j < 8; ++j)
    for (int i = 0; i < 8; ++i)
      EXPECT_EQ(expected3[c++], gridAlignedMask5.bitmap()->getPixel(i, j));
}

namespace {

// Copy of the upstream (orthogonal) grid functions before hexagonal
// grids were added, to prove that orthogonal grids give the same
// results.
struct UpstreamGrid {
  Size tileSize;
  Point origin;

  Point tileToCanvas(const Point& tile) const
  {
    return Point(tile.x * tileSize.w + origin.x, tile.y * tileSize.h + origin.y);
  }
  Rect tileToCanvas(const Rect& tileBounds) const
  {
    return Rect(tileToCanvas(tileBounds.origin()), tileToCanvas(tileBounds.point2()));
  }
  Point canvasToTile(const Point& canvasPoint) const
  {
    Point tile;
    std::div_t divx = std::div((canvasPoint.x - origin.x), tileSize.w);
    std::div_t divy = std::div((canvasPoint.y - origin.y), tileSize.h);
    tile.x = divx.quot;
    tile.y = divy.quot;
    if (canvasPoint.x < origin.x && divx.rem)
      --tile.x;
    if (canvasPoint.y < origin.y && divy.rem)
      --tile.y;
    return tile;
  }
  Rect canvasToTile(const Rect& canvasBounds) const
  {
    Point pt1 = canvasToTile(canvasBounds.origin());
    Point pt2 = canvasToTile(Point(canvasBounds.x2() - 1, canvasBounds.y2() - 1));
    return Rect(pt1, Size(pt2.x - pt1.x + 1, pt2.y - pt1.y + 1));
  }
  Size tilemapSizeToCanvas(const Size& tilemapSize) const
  {
    return Size(tilemapSize.w * tileSize.w, tilemapSize.h * tileSize.h);
  }
  Rect alignBounds(const Rect& bounds) const
  {
    Point pt1 = canvasToTile(bounds.origin());
    Point pt2 = canvasToTile(Point(bounds.x2() - 1, bounds.y2() - 1));
    return Rect(tileToCanvas(pt1), tileSize) | Rect(tileToCanvas(pt2), tileSize);
  }
};

// Deterministic pseudo-random numbers for the tests
class Random {
public:
  int next(const int max)
  {
    m_state = m_state * 1103515245u + 12345u;
    return int((m_state >> 16) % unsigned(max));
  }

private:
  unsigned m_state = 12345u;
};

bool row_major_less(const Point& a, const Point& b)
{
  return (a.y < b.y || (a.y == b.y && a.x < b.x));
}

const char* type_name(const Grid::Type type)
{
  switch (type) {
    case Grid::Type::Orthogonal:   return "orthogonal";
    case Grid::Type::HexPointyTop: return "pointy";
    case Grid::Type::HexFlatTop:   return "flat";
  }
  return "";
}

std::string describe(const Grid& grid)
{
  std::stringstream s;
  s << type_name(grid.type()) << " " << grid.tileSize() << " step=" << grid.hexStep()
    << " parity=" << (grid.parity() == Grid::Parity::Odd ? "odd" : "even")
    << " origin=" << grid.origin();
  return s.str();
}

// All the hexagonal grids used in the tests: both orientations and
// parities, many sizes (including odd ones), the default/minimum/maximum
// step, and several origins (including negative ones).
std::vector<Grid> all_hex_grids()
{
  const Size sizes[] = {
    Size(2, 2),   Size(4, 3),   Size(6, 5),   Size(7, 7),   Size(8, 8),   Size(10, 9),
    Size(14, 14), Size(15, 16), Size(16, 15), Size(16, 16), Size(16, 17), Size(18, 12),
    Size(20, 11), Size(24, 20), Size(32, 28), Size(9, 14),  Size(12, 10),
  };
  const Point origins[] = { Point(0, 0), Point(5, -3), Point(-17, 9) };

  std::vector<Grid> grids;
  for (const Size& sz : sizes) {
    for (auto type : { Grid::Type::HexPointyTop, Grid::Type::HexFlatTop }) {
      for (auto parity : { Grid::Parity::Odd, Grid::Parity::Even }) {
        for (int step : { 0, Grid::MinHexStep(sz, type), Grid::MaxHexStep(sz, type) }) {
          for (const Point& origin : origins) {
            Grid grid = Grid::MakeHex(sz, type, parity, step);
            grid.origin(origin);
            grids.push_back(grid);
          }
        }
      }
    }
  }
  return grids;
}

// Brute force: tiles whose mask contains the given pixel
std::vector<Point> owners_of(const Grid& grid, const Point& pt)
{
  std::vector<Point> result;
  const Point t0 = grid.originToTile(pt);
  for (int v = -3; v <= 3; ++v) {
    for (int u = -3; u <= 3; ++u) {
      const Point tile(t0.x + u, t0.y + v);
      const Rect bounds = grid.tileBoundsInCanvas(tile);
      if (bounds.contains(pt) && grid.mask()->getPixel(pt.x - bounds.x, pt.y - bounds.y))
        result.push_back(tile);
    }
  }
  return result;
}

// Area around the grid origin (with negative coordinates) used to test
// each pixel
Rect test_area(const Grid& grid)
{
  const Size sz = grid.tileSize();
  return Rect(grid.origin().x - 2 * sz.w, grid.origin().y - 2 * sz.h, 5 * sz.w, 5 * sz.h);
}

// Tiles owning at least one pixel of the given rectangle, sorted in
// row-major order (brute force using canvasToTile() per pixel)
std::vector<Point> owners_in_rect(const Grid& grid, const Rect& rc)
{
  std::vector<Point> result;
  for (int y = rc.y; y < rc.y2(); ++y)
    for (int x = rc.x; x < rc.x2(); ++x)
      result.push_back(grid.canvasToTile(Point(x, y)));
  std::sort(result.begin(), result.end(), row_major_less);
  result.erase(std::unique(result.begin(), result.end()), result.end());
  return result;
}

Rect bounds_of_points(const std::vector<Point>& pts)
{
  Rect rc;
  for (const Point& pt : pts)
    rc |= Rect(pt, Size(1, 1));
  return rc;
}

ImageRef make_mask(const Size& sz,
                   const std::vector<int>& rowStarts,
                   const std::vector<int>& rowEnds)
{
  ImageRef mask(Image::create(IMAGE_BITMAP, sz.w, sz.h));
  mask->clear(0);
  for (int y = 0; y < sz.h; ++y)
    for (int x = rowStarts[y]; x < rowEnds[y]; ++x)
      mask->putPixel(x, y, 1);
  return mask;
}

} // anonymous namespace

TEST(Grid, RectMatchesUpstream)
{
  Random rnd;
  const Size sizes[] = { Size(16, 16), Size(8, 5), Size(1, 1), Size(3, 7), Size(32, 24) };
  const Point origins[] = { Point(0, 0), Point(17, 16), Point(-1, -1), Point(-13, 5) };
  for (const Size& sz : sizes) {
    for (const Point& origin : origins) {
      Grid grid = Grid::MakeRect(sz);
      grid.origin(origin);
      const UpstreamGrid ref{ sz, origin };
      EXPECT_EQ(Grid::Type::Orthogonal, grid.type());
      EXPECT_FALSE(grid.isHexagonal());
      EXPECT_EQ(0, grid.hexStep());
      EXPECT_EQ(tile_f_xflip | tile_f_yflip | tile_f_dflip, grid.validTileFlags());
      EXPECT_TRUE(grid.validateTessellation());

      for (int y = -40; y < 40; ++y) {
        for (int x = -40; x < 40; ++x) {
          const Point pt(x, y);
          ASSERT_EQ(ref.canvasToTile(pt), grid.canvasToTile(pt));
          ASSERT_EQ(ref.canvasToTile(pt), grid.originToTile(pt));
          ASSERT_EQ(ref.tileToCanvas(pt), grid.tileToCanvas(pt));
        }
      }

      for (int i = 0; i < 200; ++i) {
        const Rect rc(rnd.next(80) - 40, rnd.next(80) - 40, rnd.next(40) + 1, rnd.next(40) + 1);
        const Rect tiles(rnd.next(10) - 5, rnd.next(10) - 5, rnd.next(6), rnd.next(6));
        ASSERT_EQ(ref.canvasToTile(rc), grid.canvasToTile(rc));
        ASSERT_EQ(ref.alignBounds(rc), grid.alignBounds(rc));
        ASSERT_EQ(ref.tileToCanvas(tiles), grid.tileToCanvas(tiles));
        ASSERT_EQ(ref.tilemapSizeToCanvas(tiles.size()), grid.tilemapSizeToCanvas(tiles.size()));
        const Region tileRgn = grid.tileRegionInCanvas(tiles.origin());
        ASSERT_TRUE(tileRgn.isRect());
        ASSERT_EQ(ref.tileToCanvas(Rect(tiles.origin(), Size(1, 1))), tileRgn.bounds());

        // tilesInCanvasRegion() must return the tiles of the union of
        // tile rectangles in row-major order
        Region rgn(rc);
        rgn |= Region(Rect(rc.x + 7, rc.y2() + 3, 9, 4));
        std::vector<Point> expected;
        for (const Rect& r : rgn) {
          const Rect t = ref.canvasToTile(r);
          for (int v = t.y; v < t.y2(); ++v)
            for (int u = t.x; u < t.x2(); ++u)
              expected.push_back(Point(u, v));
        }
        std::sort(expected.begin(), expected.end(), row_major_less);
        expected.erase(std::unique(expected.begin(), expected.end()), expected.end());
        ASSERT_EQ(expected, grid.tilesInCanvasRegion(rgn));
      }
    }
  }
}

TEST(Grid, HexKnownValues)
{
  // 16x16 pointy-top, default step = 16 - 16/4 = 12
  Grid grid = Grid::MakeHex(Size(16, 16), Grid::Type::HexPointyTop);
  EXPECT_TRUE(grid.isHexagonal());
  EXPECT_EQ(Grid::Parity::Odd, grid.parity());
  EXPECT_EQ(12, grid.hexStep());
  EXPECT_EQ(Point(16, 12), grid.tileOffset());
  EXPECT_EQ(Point(8, 0), grid.oddRowOffset());
  EXPECT_EQ(Point(0, 0), grid.oddColOffset());
  EXPECT_EQ(Point(8, 8), grid.tileCenter());
  ASSERT_TRUE(grid.hasMask());

  // Width of each mask row (the pixels are centered)
  const int widths[16] = { 2, 6, 10, 14, 16, 16, 16, 16, 16, 16, 16, 16, 14, 10, 6, 2 };
  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      const bool inside = (x >= (16 - widths[y]) / 2 && x < (16 + widths[y]) / 2);
      EXPECT_EQ(inside ? 1 : 0, grid.mask()->getPixel(x, y)) << "x=" << x << " y=" << y;
    }
  }

  EXPECT_EQ(Point(0, 0), grid.tileToCanvas(Point(0, 0)));
  EXPECT_EQ(Point(8, 12), grid.tileToCanvas(Point(0, 1)));
  EXPECT_EQ(Point(16, 24), grid.tileToCanvas(Point(1, 2)));
  EXPECT_EQ(Point(-8, -12), grid.tileToCanvas(Point(-1, -1)));

  // The corner of the tile (0,0) belongs to the tile (-1,-1)
  EXPECT_EQ(Point(-1, -1), grid.canvasToTile(Point(0, 0)));
  EXPECT_EQ(Point(0, 0), grid.canvasToTile(Point(7, 0)));
  EXPECT_EQ(Point(0, 0), grid.canvasToTile(Point(8, 8)));
  EXPECT_EQ(Point(0, 0), grid.originToTile(Point(0, 0)));
  EXPECT_EQ(Point(0, 1), grid.originToTile(Point(8, 12)));
  EXPECT_EQ(Point(-1, 1), grid.canvasToTile(Point(0, 15)));
  EXPECT_EQ(Point(0, 0), grid.canvasToTile(Point(8, 15)));
  EXPECT_EQ(Point(0, 1), grid.canvasToTile(Point(9, 15)));

  // A tilemap of 3x2 tiles: 3*16 + 8 (the staggered row) x 12 + 16
  EXPECT_EQ(Size(56, 28), grid.tilemapSizeToCanvas(Size(3, 2)));
  EXPECT_EQ(Size(48, 16), grid.tilemapSizeToCanvas(Size(3, 1)));
  EXPECT_EQ(Rect(0, 0, 56, 28), grid.tileToCanvas(Rect(0, 0, 3, 2)));
  EXPECT_EQ(Rect(8, 12, 48, 16), grid.tileToCanvas(Rect(0, 1, 3, 1)));

  auto nb = hex_neighbors(Point(0, 0), grid);
  std::vector<Point> nbs(nb.begin(), nb.end());
  std::sort(nbs.begin(), nbs.end(), row_major_less);
  EXPECT_EQ((std::vector<Point>{ Point(-1, -1),
                                 Point(0, -1),
                                 Point(-1, 0),
                                 Point(1, 0),
                                 Point(-1, 1),
                                 Point(0, 1) }),
            nbs);

  EXPECT_EQ(tile_f_xflip | tile_f_yflip, grid.validTileFlags());

  // Even parity: row 0 is staggered
  grid.parity(Grid::Parity::Even);
  EXPECT_EQ(Point(8, 0), grid.tileToCanvas(Point(0, 0)));
  EXPECT_EQ(Point(0, 12), grid.tileToCanvas(Point(0, 1)));
  EXPECT_EQ(Size(56, 16), grid.tilemapSizeToCanvas(Size(3, 1)));

  // Flat-top: the transposed grid
  Grid flat = Grid::MakeHex(Size(16, 16), Grid::Type::HexFlatTop);
  EXPECT_EQ(12, flat.hexStep());
  EXPECT_EQ(Point(12, 16), flat.tileOffset());
  EXPECT_EQ(Point(0, 0), flat.oddRowOffset());
  EXPECT_EQ(Point(0, 8), flat.oddColOffset());
  for (int y = 0; y < 16; ++y)
    for (int x = 0; x < 16; ++x)
      EXPECT_EQ(grid.mask()->getPixel(y, x), flat.mask()->getPixel(x, y));
  EXPECT_EQ(Point(12, 8), flat.tileToCanvas(Point(1, 0)));
  EXPECT_EQ(Point(-1, -1), flat.canvasToTile(Point(0, 0)));
  EXPECT_EQ(Size(28, 56), flat.tilemapSizeToCanvas(Size(2, 3)));
}

TEST(Grid, HexStepRange)
{
  EXPECT_EQ(12, Grid::DefaultHexStep(Size(16, 16), Grid::Type::HexPointyTop));
  EXPECT_EQ(13, Grid::DefaultHexStep(Size(16, 17), Grid::Type::HexPointyTop));
  EXPECT_EQ(9, Grid::MinHexStep(Size(16, 17), Grid::Type::HexPointyTop));
  EXPECT_EQ(17, Grid::MaxHexStep(Size(16, 17), Grid::Type::HexPointyTop));
  EXPECT_EQ(8, Grid::DefaultHexStep(Size(10, 32), Grid::Type::HexFlatTop));
  EXPECT_EQ(6, Grid::MinHexStep(Size(10, 32), Grid::Type::HexFlatTop));
  EXPECT_EQ(1, Grid::MinHexStep(Size(1, 1), Grid::Type::HexPointyTop));
  EXPECT_EQ(2, Grid::MinHexStep(Size(2, 2), Grid::Type::HexPointyTop));

  // Out of range steps are clamped
  EXPECT_EQ(16,
            Grid::MakeHex(Size(16, 16), Grid::Type::HexPointyTop, Grid::Parity::Odd, 99).hexStep());
  EXPECT_EQ(9,
            Grid::MakeHex(Size(16, 16), Grid::Type::HexPointyTop, Grid::Parity::Odd, 2).hexStep());

  // MakeHex() with an orthogonal type is a regular grid
  EXPECT_EQ(Grid::MakeRect(Size(16, 16)), Grid::MakeHex(Size(16, 16), Grid::Type::Orthogonal));
}

TEST(Grid, HexAutoMaskTessellates)
{
  for (const Grid& grid : all_hex_grids()) {
    SCOPED_TRACE(describe(grid));
    Grid::TessellationError err;
    ASSERT_TRUE(grid.validateTessellation(&err))
      << "kind=" << int(err.kind) << " pos=" << err.pos << " coverage=" << err.coverage;
    EXPECT_EQ(Grid::TessellationError::Kind::None, err.kind);
    EXPECT_EQ(0, err.count);
    ASSERT_TRUE(grid.hasMask());
    EXPECT_EQ(grid.tileSize(), grid.mask()->size());
    EXPECT_EQ(IMAGE_BITMAP, grid.mask()->pixelFormat());
    EXPECT_EQ(0, grid.validTileFlags() & tile_f_dflip);
  }
}

TEST(Grid, HexPixelOwnership)
{
  for (const Grid& grid : all_hex_grids()) {
    SCOPED_TRACE(describe(grid));
    const Rect area = test_area(grid);
    for (int y = area.y; y < area.y2(); ++y) {
      for (int x = area.x; x < area.x2(); ++x) {
        const Point pt(x, y);
        const auto owners = owners_of(grid, pt);
        ASSERT_EQ(1, owners.size()) << "pixel " << pt;
        ASSERT_EQ(owners[0], grid.canvasToTile(pt)) << "pixel " << pt;
      }
    }
  }
}

TEST(Grid, HexRoundTrip)
{
  for (const Grid& grid : all_hex_grids()) {
    SCOPED_TRACE(describe(grid));
    for (int v = -4; v <= 4; ++v) {
      for (int u = -4; u <= 4; ++u) {
        const Point tile(u, v);
        const Point pos = grid.tileToCanvas(tile);
        ASSERT_EQ(tile, grid.canvasToTile(pos + grid.tileCenter()));
        ASSERT_EQ(tile, grid.originToTile(pos));
        ASSERT_EQ(Rect(pos, grid.tileSize()), grid.tileBoundsInCanvas(tile));
        ASSERT_EQ(grid.tileBoundsInCanvas(tile), grid.tileToCanvas(Rect(tile, Size(1, 1))));
        ASSERT_EQ(Rect(tile, Size(1, 1)),
                  grid.canvasToTile(Rect(pos + grid.tileCenter(), Size(1, 1))));
      }
    }
  }
}

TEST(Grid, HexRectsAndRegions)
{
  Random rnd;
  for (const Grid& grid : all_hex_grids()) {
    SCOPED_TRACE(describe(grid));
    const Rect area = test_area(grid);
    for (int i = 0; i < 12; ++i) {
      const Rect rc(area.x + rnd.next(area.w),
                    area.y + rnd.next(area.h),
                    rnd.next(3 * grid.tileSize().w) + 1,
                    rnd.next(3 * grid.tileSize().h) + 1);
      SCOPED_TRACE("rect x=" + std::to_string(rc.x) + " y=" + std::to_string(rc.y) +
                   " w=" + std::to_string(rc.w) + " h=" + std::to_string(rc.h));

      // The tiles touched by the rectangle are exactly the ones
      // owning one of its pixels.
      const std::vector<Point> expected = owners_in_rect(grid, rc);
      ASSERT_EQ(expected, grid.tilesInCanvasRegion(Region(rc)));
      ASSERT_EQ(bounds_of_points(expected), grid.canvasToTile(rc));

      Region expectedRgn;
      for (const Point& pt : expected)
        expectedRgn |= Region(Rect(pt, Size(1, 1)));
      Grid mutableGrid = grid; // canvasToTile(Region) isn't const
      Region tilesRgn = mutableGrid.canvasToTile(Region(rc));
      Region diff;
      diff.createSubtraction(expectedRgn, tilesRgn);
      ASSERT_TRUE(diff.isEmpty());
      diff.createSubtraction(tilesRgn, expectedRgn);
      ASSERT_TRUE(diff.isEmpty());

      // The canvas region of the touched tiles covers the rectangle
      const Region covered = grid.tilesRegionInCanvas(expected);
      diff.createSubtraction(Region(rc), covered);
      ASSERT_TRUE(diff.isEmpty());

      // tileToCanvas(Rect) is the union of the bounds of all the tiles
      // in the range, and alignBounds() is the range of touched tiles.
      const Rect range = grid.canvasToTile(rc);
      Rect unionBounds;
      for (int v = range.y; v < range.y2(); ++v)
        for (int u = range.x; u < range.x2(); ++u)
          unionBounds |= grid.tileBoundsInCanvas(Point(u, v));
      ASSERT_EQ(unionBounds, grid.tileToCanvas(range));
      ASSERT_EQ(unionBounds, grid.alignBounds(rc));
      ASSERT_TRUE(unionBounds.contains(rc));
    }

    // Region of each tile = pixels owned by the tile
    for (int v = -1; v <= 1; ++v) {
      for (int u = -1; u <= 1; ++u) {
        const Point tile(u, v);
        const Region rgn = grid.tileRegionInCanvas(tile);
        const Rect bounds = grid.tileBoundsInCanvas(tile);
        ASSERT_TRUE(bounds.contains(rgn.bounds()));
        for (int y = bounds.y; y < bounds.y2(); ++y)
          for (int x = bounds.x; x < bounds.x2(); ++x)
            ASSERT_EQ(grid.canvasToTile(Point(x, y)) == tile, rgn.contains(Point(x, y)))
              << "pixel " << Point(x, y) << " tile " << tile;
      }
    }
  }
}

TEST(Grid, HexTilemapSize)
{
  for (const Grid& grid : all_hex_grids()) {
    SCOPED_TRACE(describe(grid));
    Grid cel = grid;
    cel.origin(Point(0, 0));
    for (int h = 1; h <= 4; ++h) {
      for (int w = 1; w <= 4; ++w) {
        // The bounds of a tilemap cel (origin + tilemapSizeToCanvas)
        // contain all its tiles and end with the last ones.
        const Size size = cel.tilemapSizeToCanvas(Size(w, h));
        const Rect tiles = cel.tileToCanvas(Rect(0, 0, w, h));
        ASSERT_EQ(size.w, tiles.x2());
        ASSERT_EQ(size.h, tiles.y2());
        ASSERT_GE(tiles.x, 0);
        ASSERT_GE(tiles.y, 0);
      }
    }
    ASSERT_EQ(Size(0, 0), cel.tilemapSizeToCanvas(Size(0, 0)));
  }
}

TEST(Grid, HexTilemapAlignment)
{
  Random rnd;
  for (const Grid& grid : all_hex_grids()) {
    SCOPED_TRACE(describe(grid));
    const Size period = grid.staggerPeriod();
    EXPECT_EQ(grid.type() == Grid::Type::HexPointyTop ? Size(1, 2) : Size(2, 1), period);

    for (int i = 0; i < 20; ++i) {
      const Rect tiles(rnd.next(20) - 10, rnd.next(20) - 10, rnd.next(5) + 1, rnd.next(5) + 1);
      const Rect aligned = grid.alignTilesToStagger(tiles);
      ASSERT_TRUE(aligned.contains(tiles));
      ASSERT_EQ(0, ((aligned.x % period.w) + period.w) % period.w);
      ASSERT_EQ(0, ((aligned.y % period.h) + period.h) % period.h);
      ASSERT_LT(tiles.x - aligned.x, period.w);
      ASSERT_LT(tiles.y - aligned.y, period.h);

      // A tilemap cel starting at the aligned tile, with a grid at the
      // cel position, places each tile exactly like the original grid.
      Grid celGrid = grid;
      celGrid.origin(grid.tilemapPosition(aligned.origin()));
      for (int v = 0; v < aligned.h; ++v)
        for (int u = 0; u < aligned.w; ++u)
          ASSERT_EQ(grid.tileToCanvas(aligned.origin() + Point(u, v)),
                    celGrid.tileToCanvas(Point(u, v)));
      // And the cel bounds contain all the tiles
      const Rect celBounds(celGrid.origin(), celGrid.tilemapSizeToCanvas(aligned.size()));
      ASSERT_TRUE(celBounds.contains(grid.tileToCanvas(aligned)));
    }
  }

  // Orthogonal grids don't change
  Grid rect = Grid::MakeRect(Size(16, 8));
  rect.origin(Point(3, -5));
  EXPECT_EQ(Size(1, 1), rect.staggerPeriod());
  EXPECT_EQ(Rect(-3, 7, 2, 5), rect.alignTilesToStagger(Rect(-3, 7, 2, 5)));
  EXPECT_EQ(rect.tileToCanvas(Point(-3, 7)), rect.tilemapPosition(Point(-3, 7)));
}

TEST(Grid, HexNeighbors)
{
  for (const Grid& grid : all_hex_grids()) {
    SCOPED_TRACE(describe(grid));
    for (int v = -2; v <= 2; ++v) {
      for (int u = -2; u <= 2; ++u) {
        const Point tile(u, v);

        // Tiles owning a pixel 4-connected to one of the tile pixels
        std::vector<Point> touching;
        const Rect bounds = grid.tileBoundsInCanvas(tile);
        for (int y = bounds.y - 1; y <= bounds.y2(); ++y) {
          for (int x = bounds.x - 1; x <= bounds.x2(); ++x) {
            if (grid.canvasToTile(Point(x, y)) != tile)
              continue;
            for (const Point& d : { Point(1, 0), Point(-1, 0), Point(0, 1), Point(0, -1) }) {
              const Point other = grid.canvasToTile(Point(x, y) + d);
              if (other != tile)
                touching.push_back(other);
            }
          }
        }
        std::sort(touching.begin(), touching.end(), row_major_less);
        touching.erase(std::unique(touching.begin(), touching.end()), touching.end());

        const auto nb = hex_neighbors(tile, grid);
        std::vector<Point> neighbors(nb.begin(), nb.end());
        std::sort(neighbors.begin(), neighbors.end(), row_major_less);
        ASSERT_EQ(neighbors, touching) << "tile " << tile;
      }
    }
  }
}

TEST(Grid, HexCustomMask)
{
  const Size sz(16, 16);
  const auto type = Grid::Type::HexPointyTop;

  // A valid custom mask: a sharper top edge (4 rows: 4, 8, 12, 16 wide)
  // and the matching bottom edge (the complement of the shifted top
  // edge: 12, 8, 4, 0 wide).
  const std::vector<int> starts = { 6, 4, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 4, 6, 8 };
  const std::vector<int> ends = { 10, 12, 14, 16, 16, 16, 16, 16, 16, 16, 16, 16, 14, 12, 10, 8 };
  ImageRef custom = make_mask(sz, starts, ends);
  Grid::TessellationError err;
  Grid grid = Grid::MakeHex(sz, type, Grid::Parity::Odd, 12, custom, &err);
  EXPECT_EQ(Grid::TessellationError::Kind::None, err.kind);
  EXPECT_EQ(custom.get(), grid.mask().get());
  EXPECT_TRUE(grid.validateTessellation());
  // Symmetric on X but not on Y
  EXPECT_EQ(tile_f_xflip, grid.validTileFlags());
  for (int y = -20; y < 40; ++y) {
    for (int x = -20; x < 40; ++x) {
      const auto owners = owners_of(grid, Point(x, y));
      ASSERT_EQ(1, owners.size());
      ASSERT_EQ(owners[0], grid.canvasToTile(Point(x, y)));
    }
  }

  // Same mask in the other parity and orientation (transposed)
  grid.parity(Grid::Parity::Even);
  EXPECT_TRUE(grid.validateTessellation());
  ImageRef transposed(Image::create(IMAGE_BITMAP, 16, 16));
  for (int y = 0; y < 16; ++y)
    for (int x = 0; x < 16; ++x)
      transposed->putPixel(y, x, custom->getPixel(x, y));
  EXPECT_TRUE(Grid::MakeHex(sz, Grid::Type::HexFlatTop, Grid::Parity::Odd, 12, transposed, &err)
                .validateTessellation());
  EXPECT_EQ(Grid::TessellationError::Kind::None, err.kind);

  // A valid mask that isn't symmetric on any axis: move the top row
  // one pixel to the left and fix the bottom row that fits it (the
  // complement of the top row shifted by half a tile).
  ImageRef skewed(Image::createCopy(custom.get()));
  for (int x = 0; x < 16; ++x)
    skewed->putPixel(x, 0, (x >= 5 && x < 9) ? 1 : 0);
  for (int x = 0; x < 16; ++x)
    skewed->putPixel(x, 12, skewed->getPixel((x + 8) % 16, 0) ? 0 : 1);
  Grid skewedGrid = Grid::MakeHex(sz, type, Grid::Parity::Odd, 12, skewed, &err);
  EXPECT_EQ(Grid::TessellationError::Kind::None, err.kind);
  EXPECT_EQ(skewed.get(), skewedGrid.mask().get());
  EXPECT_EQ(0, skewedGrid.validTileFlags());

  // A mask whose bottom edge doesn't fit the top edge of the tiles
  // below is rejected
  const std::vector<int> starts2 = { 7, 4, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 4, 6, 9 };
  const std::vector<int> ends2 = { 11, 12, 14, 16, 16, 16, 16, 16, 16, 16, 16, 16, 15, 12, 10, 8 };
  ImageRef asym = make_mask(sz, starts2, ends2);
  Grid asymGrid = Grid::MakeHex(sz, type, Grid::Parity::Odd, 12, asym, &err);
  EXPECT_NE(Grid::TessellationError::Kind::None, err.kind);
  EXPECT_NE(asym.get(), asymGrid.mask().get());

  // Invalid: the full rectangle overlaps the rows above/below
  ImageRef full(Image::create(IMAGE_BITMAP, 16, 16));
  full->clear(1);
  grid = Grid::MakeHex(sz, type, Grid::Parity::Odd, 12, full, &err);
  EXPECT_EQ(Grid::TessellationError::Kind::Overlap, err.kind);
  EXPECT_EQ(2, err.coverage);
  EXPECT_GT(err.count, 0);
  // The automatic mask is used instead
  EXPECT_NE(full.get(), grid.mask().get());
  EXPECT_TRUE(grid.validateTessellation());

  // Invalid: empty mask (gaps everywhere)
  ImageRef empty(Image::create(IMAGE_BITMAP, 16, 16));
  empty->clear(0);
  Grid::MakeHex(sz, type, Grid::Parity::Odd, 12, empty, &err);
  EXPECT_EQ(Grid::TessellationError::Kind::Gap, err.kind);
  EXPECT_EQ(0, err.coverage);

  // Invalid: remove one pixel of a valid mask, the gap is reported at
  // that pixel of some tile
  ImageRef holed(Image::createCopy(custom.get()));
  holed->putPixel(5, 7, 0);
  grid = Grid::MakeHex(sz, type, Grid::Parity::Odd, 12, ImageRef(), nullptr);
  grid.setMask(holed);
  EXPECT_FALSE(grid.validateTessellation(&err));
  EXPECT_EQ(Grid::TessellationError::Kind::Gap, err.kind);
  bool found = false;
  for (int v = -5; v <= 5 && !found; ++v)
    for (int u = -5; u <= 5 && !found; ++u)
      if (grid.tileToCanvas(Point(u, v)) + Point(5, 7) == err.pos)
        found = true;
  EXPECT_TRUE(found) << "gap reported at " << err.pos;

  // Invalid: add one pixel in a corner (overlap with a neighbor)
  ImageRef extra(Image::createCopy(custom.get()));
  extra->putPixel(0, 0, 1);
  grid.setMask(extra);
  EXPECT_FALSE(grid.validateTessellation(&err));
  EXPECT_EQ(Grid::TessellationError::Kind::Overlap, err.kind);

  // Invalid: wrong size or pixel format
  ImageRef small(Image::create(IMAGE_BITMAP, 8, 8));
  small->clear(1);
  Grid::MakeHex(sz, type, Grid::Parity::Odd, 12, small, &err);
  EXPECT_EQ(Grid::TessellationError::Kind::BadMask, err.kind);
  ImageRef rgb(Image::create(IMAGE_RGB, 16, 16));
  Grid::MakeHex(sz, type, Grid::Parity::Odd, 12, rgb, &err);
  EXPECT_EQ(Grid::TessellationError::Kind::BadMask, err.kind);
}

TEST(Grid, HexEquality)
{
  const Grid a = Grid::MakeHex(Size(16, 16), Grid::Type::HexPointyTop);
  EXPECT_EQ(a, Grid::MakeHex(Size(16, 16), Grid::Type::HexPointyTop));
  EXPECT_NE(a, Grid::MakeRect(Size(16, 16)));
  EXPECT_NE(a, Grid::MakeHex(Size(16, 16), Grid::Type::HexFlatTop));
  EXPECT_NE(a, Grid::MakeHex(Size(16, 16), Grid::Type::HexPointyTop, Grid::Parity::Even));
  EXPECT_NE(a, Grid::MakeHex(Size(16, 16), Grid::Type::HexPointyTop, Grid::Parity::Odd, 13));
}

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
