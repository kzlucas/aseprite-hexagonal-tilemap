// Aseprite Document Library
// Copyright (c) 2026-present  Igara Studio S.A.
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include <gtest/gtest.h>

#include "doc/grid.h"
#include "doc/hex_tiles.h"
#include "doc/image.h"
#include "doc/mask.h"
#include "doc/tile.h"
#include "gfx/region.h"

#include <algorithm>
#include <vector>

using namespace doc;
using namespace gfx;

namespace {

std::vector<Grid> test_grids()
{
  std::vector<Grid> grids;
  for (auto type : { Grid::Type::HexPointyTop, Grid::Type::HexFlatTop })
    for (auto parity : { Grid::Parity::Odd, Grid::Parity::Even })
      grids.push_back(Grid::MakeHex(Size(16, 16), type, parity));
  grids.push_back(Grid::MakeHex(Size(14, 12), Grid::Type::HexPointyTop, Grid::Parity::Odd, 10));
  return grids;
}

// Tiles with 5 non-empty tiles in a 3x4 tilemap
ImageRef make_tiles()
{
  ImageRef tiles(Image::create(IMAGE_TILEMAP, 3, 4));
  tiles->clear(notile);
  tiles->putPixel(0, 0, tile(1, 0));
  tiles->putPixel(2, 0, tile(2, 0));
  tiles->putPixel(1, 1, tile(3, 0));
  tiles->putPixel(0, 3, tile(4, 0));
  tiles->putPixel(2, 2, tile(5, 0));
  return tiles;
}

// Selection of the non-empty tiles
Mask make_mask(const Image* tiles, const Point& origin, const Grid& grid)
{
  std::vector<Point> pts;
  for (int v = 0; v < tiles->height(); ++v)
    for (int u = 0; u < tiles->width(); ++u)
      if (tiles->getPixel(u, v) != notile)
        pts.push_back(origin + Point(u, v));
  Mask mask;
  const Region rgn = grid.tilesRegionInCanvas(pts);
  mask.replace(rgn.bounds());
  mask.bitmap()->clear(0);
  for (const Rect& rc : rgn)
    for (int y = rc.y; y < rc.y2(); ++y)
      for (int x = rc.x; x < rc.x2(); ++x)
        mask.bitmap()->putPixel(x - rgn.bounds().x, y - rgn.bounds().y, 1);
  return mask;
}

// Canvas positions of the non-empty tiles, with their values
std::vector<std::pair<Point, tile_t>> positions(const Image* tiles,
                                                const Point& origin,
                                                const Grid& grid)
{
  std::vector<std::pair<Point, tile_t>> result;
  for (int v = 0; v < tiles->height(); ++v)
    for (int u = 0; u < tiles->width(); ++u)
      if (tiles->getPixel(u, v) != notile)
        result.push_back(
          std::make_pair(grid.tileToCanvas(origin + Point(u, v)), tiles->getPixel(u, v)));
  return result;
}

bool same_mask(const Mask& a, const Mask& b)
{
  const Rect bounds = a.bounds() | b.bounds();
  for (int y = bounds.y; y < bounds.y2(); ++y)
    for (int x = bounds.x; x < bounds.x2(); ++x)
      if (a.containsPoint(x, y) != b.containsPoint(x, y))
        return false;
  return true;
}

} // anonymous namespace

TEST(HexTiles, Delta)
{
  const Grid grid = Grid::MakeHex(Size(16, 16), Grid::Type::HexPointyTop);
  EXPECT_EQ(Point(8, 12), hex_tiles_delta(grid, Point(8, 8), Point(16, 20)));
  EXPECT_EQ(Point(-8, 12), hex_tiles_delta(grid, Point(8, 8), Point(0, 20)));
  EXPECT_EQ(Point(16, 0), hex_tiles_delta(grid, Point(8, 8), Point(24, 8)));
  EXPECT_EQ(Point(0, 0), hex_tiles_delta(grid, Point(8, 8), Point(9, 9)));
}

TEST(HexTiles, MoveTiles)
{
  for (const Grid& grid : test_grids()) {
    const ImageRef tiles = make_tiles();
    const Point origin(0, -2);
    const auto srcPos = positions(tiles.get(), origin, grid);

    // Move by several displacements between tiles (odd and even rows
    // and columns)
    for (int dv = -3; dv <= 3; ++dv) {
      for (int du = -3; du <= 3; ++du) {
        const Point delta = grid.tileToCanvas(Point(du, dv)) - grid.tileToCanvas(Point(0, 0));
        const Rect bounds = moved_hex_tiles_bounds(tiles.get(), origin, grid, delta);
        ASSERT_FALSE(bounds.isEmpty());

        ImageRef dst(Image::create(IMAGE_TILEMAP, bounds.w, bounds.h));
        dst->clear(notile);
        copy_moved_hex_tiles(dst.get(), bounds.origin(), tiles.get(), origin, grid, delta);

        // Each tile is moved exactly by "delta" in the canvas
        auto dstPos = positions(dst.get(), bounds.origin(), grid);
        ASSERT_EQ(srcPos.size(), dstPos.size());
        for (const auto& src : srcPos) {
          bool found = false;
          for (const auto& dst : dstPos)
            if (dst.first == src.first + delta && dst.second == src.second)
              found = true;
          ASSERT_TRUE(found) << "du=" << du << " dv=" << dv;
        }
      }
    }
  }
}

TEST(HexTiles, FindOrigin)
{
  for (const Grid& grid : test_grids()) {
    const ImageRef tiles = make_tiles();
    for (const Point& origin : { Point(0, 0), Point(-3, 2), Point(4, -6), Point(-2, -2) }) {
      const Point aligned = grid.alignTilesToStagger(Rect(origin, Size(1, 1))).origin();
      const Mask mask = make_mask(tiles.get(), aligned, grid);
      Point found;
      ASSERT_TRUE(find_hex_tiles_origin(tiles.get(), &mask, grid, found));
      EXPECT_EQ(aligned, found);
    }
  }
}

TEST(HexTiles, Flip)
{
  for (const Grid& grid : test_grids()) {
    for (auto flipType : { algorithm::FlipHorizontal, algorithm::FlipVertical }) {
      const ImageRef original = make_tiles();
      const Point originalOrigin(2, -2);
      const Mask originalMask = make_mask(original.get(), originalOrigin, grid);

      ImageRef tiles = original;
      Point origin = originalOrigin;
      Mask mask(originalMask);
      ASSERT_TRUE(flip_hex_tiles(tiles, origin, mask, grid, flipType));

      // The flipped mask is the selection of the flipped tiles
      EXPECT_TRUE(same_mask(make_mask(tiles.get(), origin, grid), mask));
      // All the tiles are still there
      EXPECT_EQ(positions(original.get(), originalOrigin, grid).size(),
                positions(tiles.get(), origin, grid).size());
      // The flipped mask is close to the original one
      EXPECT_LE(std::abs(mask.bounds().x - originalMask.bounds().x), grid.tileSize().w);
      EXPECT_LE(std::abs(mask.bounds().y - originalMask.bounds().y), grid.tileSize().h);

      // Flipping again gives the original tiles
      ASSERT_TRUE(flip_hex_tiles(tiles, origin, mask, grid, flipType));
      EXPECT_TRUE(same_mask(originalMask, mask));
      auto a = positions(original.get(), originalOrigin, grid);
      auto b = positions(tiles.get(), origin, grid);
      std::sort(a.begin(), a.end(), [](auto& x, auto& y) { return x.second < y.second; });
      std::sort(b.begin(), b.end(), [](auto& x, auto& y) { return x.second < y.second; });
      EXPECT_EQ(a, b);
    }
  }

  // A mask that isn't symmetric can't be flipped
  ImageRef custom = Grid::MakeHexMask(Size(16, 16), Grid::Type::HexPointyTop, 12);
  for (int x = 0; x < 16; ++x)
    custom->putPixel(x, 0, (x >= 5 && x < 9) ? 1 : 0);
  for (int x = 0; x < 16; ++x)
    custom->putPixel(x, 12, custom->getPixel((x + 8) % 16, 0) ? 0 : 1);
  const Grid asym =
    Grid::MakeHex(Size(16, 16), Grid::Type::HexPointyTop, Grid::Parity::Odd, 12, custom);
  ASSERT_EQ(custom.get(), asym.mask().get());
  ImageRef tiles = make_tiles();
  Point origin(0, 0);
  Mask mask = make_mask(tiles.get(), origin, asym);
  EXPECT_FALSE(flip_hex_tiles(tiles, origin, mask, asym, algorithm::FlipHorizontal));
  EXPECT_FALSE(flip_hex_tiles(tiles, origin, mask, asym, algorithm::FlipVertical));
  EXPECT_FALSE(flip_hex_tiles(tiles, origin, mask, asym, algorithm::FlipDiagonal));
}

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
