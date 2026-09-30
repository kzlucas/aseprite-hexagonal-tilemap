// Aseprite
// Copyright (C) 2026-present  Igara Studio S.A.
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#include "tests/app_test.h"

#include "app/cmd/set_tileset_grid.h"
#include "app/context.h"
#include "app/doc.h"
#include "app/file/file.h"
#include "doc/doc.h"
#include "doc/grid_properties.h"
#include "doc/tilesets.h"

#include <memory>
#include <string>
#include <vector>

using namespace app;

namespace {

// A valid custom mask for 16x16 pointy-top tiles with step=12
doc::ImageRef custom_mask()
{
  const int starts[16] = { 6, 4, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 4, 6, 8 };
  const int ends[16] = { 10, 12, 14, 16, 16, 16, 16, 16, 16, 16, 16, 16, 14, 12, 10, 8 };
  doc::ImageRef mask(doc::Image::create(doc::IMAGE_BITMAP, 16, 16));
  mask->clear(0);
  for (int y = 0; y < 16; ++y)
    for (int x = starts[y]; x < ends[y]; ++x)
      mask->putPixel(x, y, 1);
  return mask;
}

// Creates a document with a tilemap layer using a tileset with the
// given grid, 3 tiles with different colors, and a 3x2 tilemap cel.
Doc* make_tilemap_doc(app::Context& ctx, const doc::Grid& grid)
{
  Doc* doc = ctx.documents().add(64, 64, doc::ColorMode::RGB, 256);
  doc::Sprite* sprite = doc->sprite();

  auto tileset = new doc::Tileset(sprite, grid, 1);
  for (doc::color_t c :
       { doc::rgba(255, 0, 0, 255), doc::rgba(0, 255, 0, 255), doc::rgba(0, 0, 255, 255) }) {
    doc::ImageRef tile = tileset->makeEmptyTile();
    tile->clear(c);
    tileset->add(tile);
  }
  tileset->setName("hex tiles");
  tileset->userData().properties()["user"] = std::string("kept");
  tileset->userData().properties("other/extension")["n"] = int32_t(5);
  const doc::tileset_index tsi = sprite->tilesets()->add(tileset);

  auto layer = new doc::LayerTilemap(sprite, tsi);
  layer->setName("Tilemap");
  sprite->root()->addLayer(layer);

  doc::ImageRef tilemap(doc::Image::create(doc::IMAGE_TILEMAP, 3, 2));
  tilemap->clear(doc::notile);
  for (int v = 0; v < 2; ++v)
    for (int u = 0; u < 3; ++u)
      tilemap->putPixel(u, v, doc::tile(1 + (u + v) % 3, 0));
  auto cel = new doc::Cel(0, tilemap);
  cel->setPosition(gfx::Point(4, 8));
  layer->addCel(cel);
  return doc;
}

doc::LayerTilemap* tilemap_layer(Doc* doc)
{
  for (doc::Layer* layer : doc->sprite()->allLayers())
    if (layer->isTilemap())
      return static_cast<doc::LayerTilemap*>(layer);
  return nullptr;
}

} // anonymous namespace

TEST(HexFile, SaveAndLoad)
{
  app::Context ctx;
  const std::vector<doc::Grid> grids = {
    doc::Grid::MakeHex(gfx::Size(16, 16), doc::Grid::Type::HexPointyTop),
    doc::Grid::MakeHex(gfx::Size(16, 16),
                       doc::Grid::Type::HexPointyTop,
                       doc::Grid::Parity::Even,
                       12,
                       custom_mask()),
    doc::Grid::MakeHex(gfx::Size(18, 14), doc::Grid::Type::HexFlatTop, doc::Grid::Parity::Odd, 13),
    doc::Grid::MakeHex(gfx::Size(15, 16), doc::Grid::Type::HexPointyTop),
  };
  ASSERT_EQ(custom_mask()->getPixel(6, 0), grids[1].mask()->getPixel(6, 0));

  for (const doc::Grid& grid : grids) {
    const std::string fn = "test_hex.aseprite";
    doc::UserData expectedUserData;
    gfx::Rect expectedCelBounds;
    std::vector<doc::ImageRef> expectedTiles;
    {
      std::unique_ptr<Doc> doc(make_tilemap_doc(ctx, grid));
      doc->setFilename(fn);
      doc::LayerTilemap* layer = tilemap_layer(doc.get());
      expectedUserData = layer->tileset()->userData();
      expectedCelBounds = layer->cel(0)->bounds();
      for (const auto& tile : *layer->tileset())
        expectedTiles.push_back(doc::ImageRef(doc::Image::createCopy(tile.image.get())));

      // The cel bounds come from the hexagonal grid
      EXPECT_EQ(gfx::Rect(gfx::Point(4, 8), grid.tilemapSizeToCanvas(gfx::Size(3, 2))),
                expectedCelBounds);
      // Pixels outside the mask were cleared
      EXPECT_EQ(0, doc::rgba_geta(layer->tileset()->get(1)->getPixel(0, 0)));

      save_document(&ctx, doc.get());
      doc->close();
    }
    {
      std::unique_ptr<Doc> doc(load_document(&ctx, fn));
      ASSERT_TRUE(doc);
      doc::LayerTilemap* layer = tilemap_layer(doc.get());
      ASSERT_TRUE(layer);
      doc::Tileset* tileset = layer->tileset();

      EXPECT_EQ(grid, tileset->grid());
      EXPECT_EQ("hex tiles", tileset->name());
      // The extension properties are removed from the user data after
      // loading, other properties are kept
      EXPECT_EQ(0, tileset->userData().propertiesMaps().count(doc::kHexGridExtensionId));
      EXPECT_EQ(expectedUserData, tileset->userData());

      ASSERT_EQ(expectedTiles.size(), tileset->size());
      for (doc::tile_index ti = 0; ti < tileset->size(); ++ti)
        EXPECT_TRUE(doc::is_same_image(expectedTiles[ti].get(), tileset->get(ti).get()));

      EXPECT_EQ(expectedCelBounds, layer->cel(0)->bounds());
      doc->close();
    }
  }
}

TEST(HexFile, SquareTilesetHasNoHexProperties)
{
  app::Context ctx;
  const std::string fn = "test_square.aseprite";
  {
    std::unique_ptr<Doc> doc(make_tilemap_doc(ctx, doc::Grid::MakeRect(gfx::Size(16, 16))));
    doc->setFilename(fn);
    save_document(&ctx, doc.get());
    doc->close();
  }
  {
    std::unique_ptr<Doc> doc(load_document(&ctx, fn));
    doc::Tileset* tileset = tilemap_layer(doc.get())->tileset();
    EXPECT_FALSE(tileset->grid().isHexagonal());
    EXPECT_EQ(doc::Grid::MakeRect(gfx::Size(16, 16)), tileset->grid());
    EXPECT_EQ(0, tileset->userData().propertiesMaps().count(doc::kHexGridExtensionId));
    EXPECT_EQ(doc::rgba(255, 0, 0, 255), tileset->get(1)->getPixel(0, 0));
    doc->close();
  }

  // The file doesn't mention the extension
  FILE* f = std::fopen(fn.c_str(), "rb");
  ASSERT_TRUE(f);
  std::string bytes;
  char buf[4096];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
    bytes.append(buf, n);
  std::fclose(f);
  EXPECT_EQ(std::string::npos, bytes.find("hex-tilemap"));
}

TEST(HexFile, SetTilesetGridUndo)
{
  app::Context ctx;
  std::unique_ptr<Doc> doc(make_tilemap_doc(ctx, doc::Grid::MakeRect(gfx::Size(16, 16))));
  doc::LayerTilemap* layer = tilemap_layer(doc.get());
  doc::Tileset* tileset = layer->tileset();
  const doc::Grid square = tileset->grid();
  const gfx::Rect squareBounds = layer->cel(0)->bounds();
  std::unique_ptr<doc::Image> redTile(doc::Image::createCopy(tileset->get(1).get()));

  const doc::Grid hex = doc::Grid::MakeHex(gfx::Size(16, 16), doc::Grid::Type::HexPointyTop);
  cmd::SetTilesetGrid cmd(tileset, hex);
  cmd.execute(&ctx);

  const auto check_hex = [&]() {
    EXPECT_EQ(hex, tileset->grid());
    EXPECT_EQ(gfx::Rect(squareBounds.origin(), hex.tilemapSizeToCanvas(gfx::Size(3, 2))),
              layer->cel(0)->bounds());
    // The corner pixels were cleared, the center is still red
    EXPECT_EQ(0, doc::rgba_geta(tileset->get(1)->getPixel(0, 0)));
    EXPECT_EQ(doc::rgba(255, 0, 0, 255), tileset->get(1)->getPixel(8, 8));
  };
  check_hex();

  cmd.undo();
  EXPECT_EQ(square, tileset->grid());
  EXPECT_EQ(squareBounds, layer->cel(0)->bounds());
  EXPECT_TRUE(doc::is_same_image(redTile.get(), tileset->get(1).get()));

  cmd.redo();
  check_hex();
  doc->close();
}
