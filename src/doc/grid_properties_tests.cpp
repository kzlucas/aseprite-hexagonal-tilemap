// Aseprite Document Library
// Copyright (c) 2026-present  Igara Studio S.A.
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include <gtest/gtest.h>

#include "doc/cel.h"
#include "doc/grid.h"
#include "doc/grid_properties.h"
#include "doc/image.h"
#include "doc/layer_tilemap.h"
#include "doc/primitives.h"
#include "doc/sprite.h"
#include "doc/tile_primitives.h"
#include "doc/tileset.h"
#include "doc/tileset_io.h"
#include "doc/tilesets.h"

#include <memory>
#include <sstream>

using namespace doc;
using namespace gfx;

namespace {

// A valid custom mask for 16x16 pointy-top tiles with step=12: a
// sharper top edge and the matching bottom edge.
ImageRef custom_mask()
{
  const int starts[16] = { 6, 4, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 4, 6, 8 };
  const int ends[16] = { 10, 12, 14, 16, 16, 16, 16, 16, 16, 16, 16, 16, 14, 12, 10, 8 };
  ImageRef mask(Image::create(IMAGE_BITMAP, 16, 16));
  mask->clear(0);
  for (int y = 0; y < 16; ++y)
    for (int x = starts[y]; x < ends[y]; ++x)
      mask->putPixel(x, y, 1);
  return mask;
}

// A tile full of opaque pixels
ImageRef full_tile(const Sprite* sprite, const color_t color)
{
  ImageRef tile(Image::create(sprite->pixelFormat(), 16, 16));
  tile->setMaskColor(sprite->transparentColor());
  tile->clear(color);
  return tile;
}

// True if every pixel outside the mask is transparent
bool is_masked(const Image* tile, const Grid& grid)
{
  for (int y = 0; y < tile->height(); ++y)
    for (int x = 0; x < tile->width(); ++x)
      if (!grid.mask()->getPixel(x, y) && tile->getPixel(x, y) != tile->maskColor())
        return false;
  return true;
}

} // anonymous namespace

TEST(GridProperties, RoundTrip)
{
  const Size sz(16, 16);
  std::vector<Grid> grids = {
    Grid::MakeHex(sz, Grid::Type::HexPointyTop),
    Grid::MakeHex(sz, Grid::Type::HexPointyTop, Grid::Parity::Even, 10),
    Grid::MakeHex(sz, Grid::Type::HexFlatTop, Grid::Parity::Odd, 13),
    Grid::MakeHex(Size(15, 9), Grid::Type::HexPointyTop),
    Grid::MakeHex(sz, Grid::Type::HexPointyTop, Grid::Parity::Odd, 12, custom_mask()),
  };
  ASSERT_EQ(custom_mask()->getPixel(6, 0), grids.back().mask()->getPixel(6, 0));

  for (const Grid& grid : grids) {
    const UserData::Properties props = hex_grid_properties(grid);
    Grid result;
    ASSERT_TRUE(grid_from_hex_properties(props, grid.tileSize(), result));
    EXPECT_EQ(grid, result);
  }

  // The automatic mask isn't stored, a custom mask is stored as rows
  EXPECT_EQ(0, hex_grid_properties(grids[0]).count("mask"));
  const UserData::Properties props = hex_grid_properties(grids.back());
  ASSERT_EQ(1, props.count("mask"));
  const auto& rows = get_value<UserData::Vector>(props.at("mask"));
  ASSERT_EQ(16, rows.size());
  EXPECT_EQ("......####......", get_value<std::string>(rows[0]));
  EXPECT_EQ("................", get_value<std::string>(rows[15]));
  EXPECT_EQ("pointy", get_value<std::string>(props.at("type")));
  EXPECT_EQ("odd", get_value<std::string>(props.at("parity")));
  EXPECT_EQ(12, get_value<int32_t>(props.at("step")));
  EXPECT_EQ(sz, get_value<Size>(props.at("tileSize")));
  EXPECT_EQ(1, get_value<int32_t>(props.at("version")));
}

TEST(GridProperties, AnyIntegerTypeAndDefaults)
{
  UserData::Properties props;
  props["type"] = std::string("flat");
  props["step"] = uint8_t(10);
  Grid grid;
  ASSERT_TRUE(grid_from_hex_properties(props, Size(16, 16), grid));
  EXPECT_EQ(Grid::Type::HexFlatTop, grid.type());
  EXPECT_EQ(Grid::Parity::Odd, grid.parity());
  EXPECT_EQ(10, grid.hexStep());

  props["step"] = int64_t(11);
  props["parity"] = std::string("even");
  ASSERT_TRUE(grid_from_hex_properties(props, Size(16, 16), grid));
  EXPECT_EQ(Grid::Parity::Even, grid.parity());
  EXPECT_EQ(11, grid.hexStep());

  // Without a step, the default one is used
  props.erase("step");
  ASSERT_TRUE(grid_from_hex_properties(props, Size(16, 16), grid));
  EXPECT_EQ(Grid::DefaultHexStep(Size(16, 16), Grid::Type::HexFlatTop), grid.hexStep());

  // Unknown type
  props["type"] = std::string("triangle");
  EXPECT_FALSE(grid_from_hex_properties(props, Size(16, 16), grid));
  props.erase("type");
  EXPECT_FALSE(grid_from_hex_properties(props, Size(16, 16), grid));
}

TEST(GridProperties, ResizedTileset)
{
  // Properties saved for 16x16 tiles, but the tileset now has 32x24
  // tiles (e.g. resized in other program): the step is scaled along
  // the stagger axis and the custom mask is ignored.
  UserData::Properties props = hex_grid_properties(
    Grid::MakeHex(Size(16, 16), Grid::Type::HexPointyTop, Grid::Parity::Even, 12, custom_mask()));
  ASSERT_EQ(1, props.count("mask"));

  Grid grid;
  ASSERT_TRUE(grid_from_hex_properties(props, Size(32, 24), grid));
  EXPECT_EQ(Size(32, 24), grid.tileSize());
  EXPECT_EQ(Grid::Parity::Even, grid.parity());
  EXPECT_EQ(18, grid.hexStep());
  EXPECT_EQ(Grid::MakeHex(Size(32, 24), Grid::Type::HexPointyTop, Grid::Parity::Even, 18), grid);

  // A mask with the wrong size is ignored too
  props["tileSize"] = Size(32, 24);
  ASSERT_TRUE(grid_from_hex_properties(props, Size(32, 24), grid));
  EXPECT_EQ(Grid::MakeHex(Size(32, 24), Grid::Type::HexPointyTop, Grid::Parity::Even, 12), grid);
}

TEST(GridProperties, TilesetUserData)
{
  std::unique_ptr<Sprite> sprite(Sprite::MakeStdSprite(ImageSpec(ColorMode::RGB, 64, 64)));
  auto tileset = std::make_unique<Tileset>(sprite.get(), Grid::MakeRect(Size(16, 16)), 1);
  tileset->userData().properties()["user"] = std::string("value");
  tileset->userData().properties("other/extension")["n"] = int32_t(5);

  // Orthogonal tilesets save the same user data
  EXPECT_EQ(tileset->userData(), tileset_user_data_with_grid(tileset.get()));
  EXPECT_FALSE(apply_tileset_grid_properties(tileset.get()));

  // Hexagonal tilesets add the extension properties
  const Grid hex =
    Grid::MakeHex(Size(16, 16), Grid::Type::HexPointyTop, Grid::Parity::Even, 12, custom_mask());
  tileset->setGrid(hex);
  UserData saved = tileset_user_data_with_grid(tileset.get());
  ASSERT_EQ(1, saved.propertiesMaps().count(kHexGridExtensionId));
  EXPECT_EQ(0, tileset->userData().propertiesMaps().count(kHexGridExtensionId));

  // Loading: the properties are removed and applied to the grid
  auto loaded = std::make_unique<Tileset>(sprite.get(), Grid::MakeRect(Size(16, 16)), 1);
  loaded->setUserData(saved);
  EXPECT_TRUE(apply_tileset_grid_properties(loaded.get()));
  EXPECT_EQ(hex, loaded->grid());
  EXPECT_EQ(tileset->userData(), loaded->userData());
  EXPECT_EQ("value", get_value<std::string>(loaded->userData().properties()["user"]));

  // Unknown properties are kept to be saved again
  auto unknown = std::make_unique<Tileset>(sprite.get(), Grid::MakeRect(Size(16, 16)), 1);
  unknown->userData().properties(kHexGridExtensionId)["type"] = std::string("future");
  EXPECT_FALSE(apply_tileset_grid_properties(unknown.get()));
  EXPECT_FALSE(unknown->grid().isHexagonal());
  EXPECT_EQ(1, unknown->userData().propertiesMaps().count(kHexGridExtensionId));
  EXPECT_EQ(unknown->userData(), tileset_user_data_with_grid(unknown.get()));
}

TEST(GridProperties, TilePixelsOutsideMaskAreTransparent)
{
  std::unique_ptr<Sprite> sprite(Sprite::MakeStdSprite(ImageSpec(ColorMode::RGB, 64, 64)));
  const color_t red = rgba(255, 0, 0, 255);

  // Orthogonal tilesets keep all the pixels
  auto square = std::make_unique<Tileset>(sprite.get(), Grid::MakeRect(Size(16, 16)), 1);
  const tile_index ti = square->add(full_tile(sprite.get(), red));
  EXPECT_EQ(red, square->get(ti)->getPixel(0, 0));

  // Changing to a hexagonal grid clears the pixels outside the mask
  const Grid hex = Grid::MakeHex(Size(16, 16), Grid::Type::HexPointyTop);
  square->setGrid(hex);
  EXPECT_TRUE(is_masked(square->get(ti).get(), hex));
  EXPECT_EQ(red, square->get(ti)->getPixel(8, 8));

  // add(), insert(), set() and notifyTileContentChange() clear them
  // in hexagonal tilesets
  auto tileset = std::make_unique<Tileset>(sprite.get(), hex, 1);
  const tile_index a = tileset->add(full_tile(sprite.get(), red));
  tileset->insert(1, full_tile(sprite.get(), red));
  tileset->set(1, full_tile(sprite.get(), red));
  for (tile_index i : { a, tile_index(1) })
    EXPECT_TRUE(is_masked(tileset->get(i).get(), hex));

  tileset->get(a)->putPixel(0, 0, red);
  tileset->notifyTileContentChange(a);
  EXPECT_TRUE(is_masked(tileset->get(a).get(), hex));

  // Masked tiles are found by the hash table
  tile_index found;
  ImageRef query(Image::createCopy(tileset->get(a).get()));
  EXPECT_TRUE(tileset->findTileIndex(query, found));
  EXPECT_TRUE(is_same_image(query.get(), tileset->get(found).get()));
}

TEST(GridProperties, TilesetEquality)
{
  std::unique_ptr<Sprite> sprite(Sprite::MakeStdSprite(ImageSpec(ColorMode::RGB, 64, 64)));
  Tileset a(sprite.get(), Grid::MakeRect(Size(16, 16)), 1);
  Tileset b(sprite.get(), Grid::MakeRect(Size(16, 16)), 1);
  EXPECT_TRUE(a == b);

  // Orthogonal tilesets are compared by tile size only (upstream
  // behavior)
  Grid other = Grid::MakeRect(Size(16, 16));
  other.tileCenter(Point(1, 1));
  b.setGrid(other);
  EXPECT_TRUE(a == b);

  // A hexagonal grid is compared completely
  b.setGrid(Grid::MakeHex(Size(16, 16), Grid::Type::HexPointyTop));
  EXPECT_FALSE(a == b);
  a.setGrid(Grid::MakeHex(Size(16, 16), Grid::Type::HexPointyTop));
  EXPECT_TRUE(a == b);
  a.setGrid(Grid::MakeHex(Size(16, 16), Grid::Type::HexPointyTop, Grid::Parity::Even));
  EXPECT_FALSE(a == b);
}

TEST(GridProperties, TilesetStream)
{
  // Crash backups and the native clipboard use write/read_tileset()
  std::unique_ptr<Sprite> sprite(Sprite::MakeStdSprite(ImageSpec(ColorMode::RGB, 64, 64)));
  const Grid hex =
    Grid::MakeHex(Size(16, 16), Grid::Type::HexFlatTop, Grid::Parity::Even, 11, ImageRef());
  auto tileset = std::make_unique<Tileset>(sprite.get(), hex, 1);
  tileset->add(full_tile(sprite.get(), rgba(0, 0, 255, 255)));
  tileset->userData().properties()["user"] = int32_t(7);

  for (Sprite* spr : { sprite.get(), (Sprite*)nullptr }) {
    std::stringstream stream;
    ASSERT_TRUE(write_tileset(stream, tileset.get()));
    std::unique_ptr<Tileset> copy(read_tileset(stream, spr, false));
    ASSERT_TRUE(copy);
    EXPECT_EQ(hex, copy->grid());
    EXPECT_EQ(tileset->userData(), copy->userData());
    ASSERT_EQ(tileset->size(), copy->size());
    for (tile_index ti = 0; ti < tileset->size(); ++ti)
      EXPECT_TRUE(is_same_image(tileset->get(ti).get(), copy->get(ti).get()));
  }
}

TEST(GridProperties, GetTilePixel)
{
  // Eyedropper in tiles mode: the tile under a canvas pixel
  std::unique_ptr<Sprite> sprite(Sprite::MakeStdSprite(ImageSpec(ColorMode::RGB, 64, 64)));
  const Grid hex = Grid::MakeHex(Size(16, 16), Grid::Type::HexPointyTop);
  auto tileset = new Tileset(sprite.get(), hex, 1);
  tileset->add(full_tile(sprite.get(), rgba(255, 0, 0, 255)));
  tileset->add(full_tile(sprite.get(), rgba(0, 0, 255, 255)));
  auto layer = new LayerTilemap(sprite.get(), sprite->tilesets()->add(tileset));
  sprite->root()->addLayer(layer);

  // A 3x3 tilemap whose first tile is the tile -1,-2 of the grid
  ImageRef tilemap(Image::create(IMAGE_TILEMAP, 3, 3));
  tilemap->clear(notile);
  tilemap->putPixel(1, 2, tile(1, 0)); // Tile 0,0 of the grid
  tilemap->putPixel(0, 1, tile(2, 0)); // Tile -1,-1 of the grid
  auto cel = new Cel(0, tilemap);
  cel->setPosition(hex.tilemapPosition(Point(-1, -2)));
  layer->addCel(cel);

  tile_index ti;
  tile_flags tf;
  color_t color;
  ASSERT_TRUE(get_tile_pixel(cel, gfx::PointF(8, 8), ti, tf, color));
  EXPECT_EQ(1, ti);
  EXPECT_EQ(rgba(255, 0, 0, 255), color);
  // The corner of the bounds of the tile 0,0 is a pixel of -1,-1
  ASSERT_TRUE(get_tile_pixel(cel, gfx::PointF(0, 0), ti, tf, color));
  EXPECT_EQ(2, ti);
  EXPECT_EQ(rgba(0, 0, 255, 255), color);
}

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
