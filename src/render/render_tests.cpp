// Aseprite Render Library
// Copyright (c) 2019-2023 Igara Studio S.A.
// Copyright (c) 2001-2018 David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include <gtest/gtest.h>

#include "render/render.h"

#include "doc/cel.h"
#include "doc/document.h"
#include "doc/grid.h"
#include "doc/image.h"
#include "doc/layer.h"
#include "doc/layer_tilemap.h"
#include "doc/palette.h"
#include "doc/primitives.h"
#include "doc/tileset.h"
#include "doc/tilesets.h"

#include <memory>
#include <string>

using namespace doc;
using namespace render;

template<typename T>
class RenderAllModes : public testing::Test {
protected:
  RenderAllModes() {}
};

typedef testing::Types<RgbTraits, GrayscaleTraits, IndexedTraits> ImageAllTraits;
TYPED_TEST_SUITE(RenderAllModes, ImageAllTraits);

// a b
// c d
#define EXPECT_2X2_PIXELS(image, a, b, c, d)                                                       \
  EXPECT_EQ(a, get_pixel(image, 0, 0));                                                            \
  EXPECT_EQ(b, get_pixel(image, 1, 0));                                                            \
  EXPECT_EQ(c, get_pixel(image, 0, 1));                                                            \
  EXPECT_EQ(d, get_pixel(image, 1, 1))

// a b c d
// e f g h
// i j k l
// m n o p
#define EXPECT_4X4_PIXELS(image, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p)                   \
  EXPECT_EQ(a, get_pixel(image, 0, 0));                                                            \
  EXPECT_EQ(b, get_pixel(image, 1, 0));                                                            \
  EXPECT_EQ(c, get_pixel(image, 2, 0));                                                            \
  EXPECT_EQ(d, get_pixel(image, 3, 0));                                                            \
  EXPECT_EQ(e, get_pixel(image, 0, 1));                                                            \
  EXPECT_EQ(f, get_pixel(image, 1, 1));                                                            \
  EXPECT_EQ(g, get_pixel(image, 2, 1));                                                            \
  EXPECT_EQ(h, get_pixel(image, 3, 1));                                                            \
  EXPECT_EQ(i, get_pixel(image, 0, 2));                                                            \
  EXPECT_EQ(j, get_pixel(image, 1, 2));                                                            \
  EXPECT_EQ(k, get_pixel(image, 2, 2));                                                            \
  EXPECT_EQ(l, get_pixel(image, 3, 2));                                                            \
  EXPECT_EQ(m, get_pixel(image, 0, 3));                                                            \
  EXPECT_EQ(n, get_pixel(image, 1, 3));                                                            \
  EXPECT_EQ(o, get_pixel(image, 2, 3));                                                            \
  EXPECT_EQ(p, get_pixel(image, 3, 3))

TEST(Render, Basic)
{
  std::shared_ptr<Document> doc = std::make_shared<Document>();
  doc->sprites().add(Sprite::MakeStdSprite(ImageSpec(ColorMode::INDEXED, 2, 2)));

  Image* src = doc->sprite()->root()->firstLayer()->cel(0)->image();
  clear_image(src, 2);

  std::unique_ptr<Image> dst(Image::create(IMAGE_INDEXED, 2, 2));
  clear_image(dst.get(), 1);
  EXPECT_2X2_PIXELS(dst.get(), 1, 1, 1, 1);

  Render render;
  render.renderSprite(dst.get(), doc->sprite(), frame_t(0));
  EXPECT_2X2_PIXELS(dst.get(), 2, 2, 2, 2);
}

TYPED_TEST(RenderAllModes, CheckDefaultBackgroundMode)
{
  typedef TypeParam ImageTraits;

  std::shared_ptr<Document> doc = std::make_shared<Document>();
  doc->sprites().add(Sprite::MakeStdSprite(ImageSpec(ImageTraits::color_mode, 2, 2)));

  EXPECT_TRUE(!doc->sprite()->root()->firstLayer()->isBackground());
  Image* src = doc->sprite()->root()->firstLayer()->cel(0)->image();
  clear_image(src, 0);
  put_pixel(src, 1, 1, 1);

  std::unique_ptr<Image> dst(Image::create(ImageTraits::pixel_format, 2, 2));
  clear_image(dst.get(), 1);
  EXPECT_2X2_PIXELS(dst.get(), 1, 1, 1, 1);

  Render render;
  render.renderSprite(dst.get(), doc->sprite(), frame_t(0));
  // Default background mode is to set all pixels to transparent color
  EXPECT_2X2_PIXELS(dst.get(), 0, 0, 0, 1);
}

TEST(Render, DefaultBackgroundModeWithNonzeroTransparentIndex)
{
  std::shared_ptr<Document> doc = std::make_shared<Document>();
  doc->sprites().add(Sprite::MakeStdSprite(ImageSpec(ColorMode::INDEXED, 2, 2)));
  doc->sprite()->setTransparentColor(2); // Transparent color is index 2

  EXPECT_TRUE(!doc->sprite()->root()->firstLayer()->isBackground());
  Image* src = doc->sprite()->root()->firstLayer()->cel(0)->image();
  clear_image(src, 2);
  put_pixel(src, 1, 1, 1);

  std::unique_ptr<Image> dst(Image::create(IMAGE_INDEXED, 2, 2));
  clear_image(dst.get(), 1);
  EXPECT_2X2_PIXELS(dst.get(), 1, 1, 1, 1);

  Render render;
  render.renderSprite(dst.get(), doc->sprite(), frame_t(0));
  EXPECT_2X2_PIXELS(dst.get(), 2, 2, 2, 1); // Indexed transparent

  dst.reset(Image::create(IMAGE_RGB, 2, 2));
  clear_image(dst.get(), 1);
  EXPECT_2X2_PIXELS(dst.get(), 1, 1, 1, 1);
  render.renderSprite(dst.get(), doc->sprite(), frame_t(0));
  color_t c1 = doc->sprite()->palette(0)->entry(1);
  EXPECT_NE(0, c1);
  EXPECT_2X2_PIXELS(dst.get(), 0, 0, 0, c1); // RGB transparent
}

TEST(Render, CheckeredBackground)
{
  std::shared_ptr<Document> doc = std::make_shared<Document>();
  doc->sprites().add(Sprite::MakeStdSprite(ImageSpec(ColorMode::INDEXED, 4, 4)));

  std::unique_ptr<Image> dst(Image::create(IMAGE_INDEXED, 4, 4));
  clear_image(dst.get(), 0);

  Render render;
  BgOptions bg;
  bg.type = BgType::CHECKERED;
  bg.zoom = true;
  bg.colorPixelFormat = IMAGE_INDEXED;
  bg.color1 = 1;
  bg.color2 = 2;

  bg.stripeSize = gfx::Size(1, 1);
  render.setBgOptions(bg);
  render.renderSprite(dst.get(), doc->sprite(), frame_t(0));
  EXPECT_4X4_PIXELS(dst.get(), 1, 2, 1, 2, 2, 1, 2, 1, 1, 2, 1, 2, 2, 1, 2, 1);

  bg.stripeSize = gfx::Size(2, 2);
  render.setBgOptions(bg);
  render.renderSprite(dst.get(), doc->sprite(), frame_t(0));
  EXPECT_4X4_PIXELS(dst.get(), 1, 1, 2, 2, 1, 1, 2, 2, 2, 2, 1, 1, 2, 2, 1, 1);

  bg.stripeSize = gfx::Size(3, 3);
  render.setBgOptions(bg);
  render.renderSprite(dst.get(), doc->sprite(), frame_t(0));
  EXPECT_4X4_PIXELS(dst.get(), 1, 1, 1, 2, 1, 1, 1, 2, 1, 1, 1, 2, 2, 2, 2, 1);

  bg.stripeSize = gfx::Size(1, 1);
  render.setProjection(Projection(PixelRatio(1, 1), Zoom(2, 1)));
  render.setBgOptions(bg);
  render.renderSprite(dst.get(), doc->sprite(), frame_t(0));
  EXPECT_4X4_PIXELS(dst.get(), 1, 1, 2, 2, 1, 1, 2, 2, 2, 2, 1, 1, 2, 2, 1, 1);
}

TEST(Render, ZoomAndDstBounds)
{
  // Create this image:
  // 0 0 0
  // 0 4 4
  // 0 4 4
  std::shared_ptr<Document> doc = std::make_shared<Document>();
  doc->sprites().add(Sprite::MakeStdSprite(ImageSpec(ColorMode::INDEXED, 3, 3)));
  Image* src = doc->sprite()->root()->firstLayer()->cel(0)->image();
  clear_image(src, 0);
  fill_rect(src, 1, 1, 2, 2, 4);

  std::unique_ptr<Image> dst(Image::create(IMAGE_INDEXED, 4, 4));
  clear_image(dst.get(), 0);

  Render render;
  BgOptions bg;
  bg.type = BgType::CHECKERED;
  bg.zoom = true;
  bg.colorPixelFormat = IMAGE_INDEXED;
  bg.color1 = 1;
  bg.color2 = 2;
  bg.stripeSize = gfx::Size(1, 1);
  render.setBgOptions(bg);

  render.renderSprite(dst.get(), doc->sprite(), frame_t(0), gfx::Clip(1, 1, 0, 0, 2, 2));
  EXPECT_4X4_PIXELS(dst.get(), 0, 0, 0, 0, 0, 1, 2, 0, 0, 2, 4, 0, 0, 0, 0, 0);
}

TEST(Render, BugWithMultiplesOf3ZoomFactors)
{
  std::shared_ptr<Document> doc = std::make_shared<Document>();
  doc->sprites().add(Sprite::MakeStdSprite(ImageSpec(ColorMode::RGB, 4, 4)));
  Image* src = doc->sprite()->root()->firstLayer()->cel(0)->image();
  clear_image(src, 0);
  draw_line(src, 0, 0, 3, 3, rgba(255, 0, 0, 255));

  // Added other factors (like 1, 2, 4, etc.) too
  int zooms[] = { 1,  2,  3,  4,  6,  8,  9,  12, 15, 16, 18, 21, 24, 27, 30, 32, 33,
                  36, 39, 42, 45, 48, 51, 54, 57, 60, 63, 66, 69, 72, 75, 78, 81 };
  for (int zoom : zooms) {
    std::unique_ptr<Image> dst(Image::create(IMAGE_RGB, 4 * zoom, 4 * zoom));
    clear_image(dst.get(), 0);

    Render render;
    BgOptions bg;
    bg.type = BgType::CHECKERED;
    bg.zoom = false;
    bg.colorPixelFormat = IMAGE_RGB;
    bg.color1 = rgba(128, 128, 128, 255);
    bg.color2 = rgba(64, 64, 64, 255);
    bg.stripeSize = gfx::Size(2, 2);
    render.setBgOptions(bg);
    render.setProjection(Projection(PixelRatio(1, 1), Zoom(zoom, 1)));
    render.renderSprite(dst.get(),
                        doc->sprite(),
                        frame_t(0),
                        gfx::Clip(0, 0, 0, 0, 4 * zoom, 4 * zoom));

    for (int y = 0; y < dst->height(); ++y) {
      for (int x = 0; x < dst->width(); ++x) {
        color_t c = get_pixel(dst.get(), x, y);

        if (x / zoom == y / zoom) {
          EXPECT_EQ(c, rgba(255, 0, 0, 255)) << " zoom=" << zoom << " x=" << x << " y=" << y;
        }
        else {
          EXPECT_NE(c, rgba(255, 0, 0, 255)) << " zoom=" << zoom << " x=" << x << " y=" << y;

          int gridBg = ((x / 2) + (y / 2)) % 2;
          if (gridBg == 0) {
            EXPECT_EQ(c, rgba(128, 128, 128, 255)) << " zoom=" << zoom << " x=" << x << " y=" << y;
          }
          else {
            EXPECT_EQ(c, rgba(64, 64, 64, 255)) << " zoom=" << zoom << " x=" << x << " y=" << y;
          }
        }
      }
    }
  }
}

namespace {

// A tile with a gradient (so flips are visible)
ImageRef make_gradient_tile(const gfx::Size& sz, const int seed)
{
  ImageRef tile(Image::create(IMAGE_RGB, sz.w, sz.h));
  for (int y = 0; y < sz.h; ++y)
    for (int x = 0; x < sz.w; ++x)
      tile->putPixel(
        x,
        y,
        rgba((x * 16 + seed * 40) & 255, (y * 16 + seed * 70) & 255, (seed * 50) & 255, 255));
  return tile;
}

// Creates a sprite with a hexagonal tilemap cel at "celPos", and a
// second sprite with a regular layer with the pixels that the tilemap
// must render: each canvas pixel is the (flipped) pixel of the tile
// that owns it.
void make_hex_sprites(Document* doc,
                      const Grid& grid,
                      const gfx::Point& celPos,
                      Sprite*& hexSprite,
                      Sprite*& refSprite)
{
  const ImageSpec spec(ColorMode::RGB, 96, 72);
  hexSprite = Sprite::MakeStdSprite(spec);
  refSprite = Sprite::MakeStdSprite(spec);
  doc->sprites().add(hexSprite);
  doc->sprites().add(refSprite);

  auto tileset = new Tileset(hexSprite, grid, 1);
  for (int i = 1; i <= 5; ++i)
    tileset->add(make_gradient_tile(grid.tileSize(), i));
  auto layer = new LayerTilemap(hexSprite, hexSprite->tilesets()->add(tileset));
  hexSprite->root()->addLayer(layer);

  const tile_flags validFlags = grid.validTileFlags();
  ImageRef tilemap(Image::create(IMAGE_TILEMAP, 6, 5));
  for (int v = 0; v < tilemap->height(); ++v) {
    for (int u = 0; u < tilemap->width(); ++u) {
      const int i = (u * 3 + v * 2) % 6; // 0 = empty tile
      tile_flags flags = 0;
      if ((u + v) % 3 == 0)
        flags |= tile_f_xflip;
      if ((u * v) % 4 == 1)
        flags |= tile_f_yflip;
      tilemap->putPixel(u, v, tile(i, flags & validFlags));
    }
  }
  auto cel = new Cel(0, tilemap);
  cel->setPosition(celPos);
  layer->addCel(cel);

  // The reference is a regular cel with the same bounds of the tilemap cel
  Grid celGrid = grid;
  celGrid.origin(celPos);
  const gfx::Rect celBounds(celPos, grid.tilemapSizeToCanvas(tilemap->size()));
  ASSERT_EQ(celBounds, cel->bounds());
  ImageRef refImage(Image::create(IMAGE_RGB, celBounds.w, celBounds.h));
  clear_image(refImage.get(), 0);
  auto refLayer = new LayerImage(refSprite);
  refSprite->root()->addLayer(refLayer);
  auto refCel = new Cel(0, refImage);
  refCel->setPosition(celPos);
  refLayer->addCel(refCel);

  for (int y = celBounds.y; y < celBounds.y2(); ++y) {
    for (int x = celBounds.x; x < celBounds.x2(); ++x) {
      const gfx::Point tilePt = celGrid.canvasToTile(gfx::Point(x, y));
      if (!tilemap->bounds().contains(tilePt))
        continue;
      const tile_t t = tilemap->getPixel(tilePt.x, tilePt.y);
      if (tile_geti(t) == notile)
        continue;
      const ImageRef tileImage = tileset->get(tile_geti(t));
      gfx::Point local = gfx::Point(x, y) - celGrid.tileToCanvas(tilePt);
      if (tile_getf(t) & tile_f_xflip)
        local.x = tileImage->width() - 1 - local.x;
      if (tile_getf(t) & tile_f_yflip)
        local.y = tileImage->height() - 1 - local.y;
      refImage->putPixel(x - celPos.x, y - celPos.y, tileImage->getPixel(local.x, local.y));
    }
  }
}

std::string first_difference(const Image* a, const Image* b)
{
  for (int y = 0; y < a->height(); ++y)
    for (int x = 0; x < a->width(); ++x)
      if (a->getPixel(x, y) != b->getPixel(x, y))
        return "pixel " + std::to_string(x) + "," + std::to_string(y);
  return std::string();
}

} // anonymous namespace

// A hexagonal tilemap must render exactly like a regular layer with
// the pixels of the tiles that own each canvas pixel, at any zoom
// level and for any rendered area.
TEST(Render, HexTilemaps)
{
  ImageRef customMask(Image::create(IMAGE_BITMAP, 16, 16));
  {
    const int starts[16] = { 6, 4, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 4, 6, 8 };
    const int ends[16] = { 10, 12, 14, 16, 16, 16, 16, 16, 16, 16, 16, 16, 14, 12, 10, 8 };
    customMask->clear(0);
    for (int y = 0; y < 16; ++y)
      for (int x = starts[y]; x < ends[y]; ++x)
        customMask->putPixel(x, y, 1);
  }
  const Grid grids[] = {
    Grid::MakeHex(gfx::Size(16, 16), Grid::Type::HexPointyTop),
    Grid::MakeHex(gfx::Size(14, 12), Grid::Type::HexPointyTop, Grid::Parity::Even, 10),
    Grid::MakeHex(gfx::Size(16, 16), Grid::Type::HexFlatTop),
    Grid::MakeHex(gfx::Size(12, 18), Grid::Type::HexFlatTop, Grid::Parity::Even),
    Grid::MakeHex(gfx::Size(15, 16), Grid::Type::HexPointyTop),
    Grid::MakeHex(gfx::Size(16, 16), Grid::Type::HexPointyTop, Grid::Parity::Odd, 12, customMask),
  };
  const gfx::Point celPositions[] = { gfx::Point(0, 0), gfx::Point(5, -7), gfx::Point(-9, 4) };
  const Zoom zooms[] = { Zoom(1, 1), Zoom(2, 1), Zoom(3, 1), Zoom(1, 2), Zoom(1, 3) };

  for (const Grid& grid : grids) {
    for (const gfx::Point& celPos : celPositions) {
      auto doc = std::make_shared<Document>();
      Sprite* hexSprite;
      Sprite* refSprite;
      make_hex_sprites(doc.get(), grid, celPos, hexSprite, refSprite);

      for (const Zoom& zoom : zooms) {
        const int w = zoom.apply(96);
        const int h = zoom.apply(72);
        // Full sprite and partial areas (scrolled editor)
        const gfx::Clip areas[] = {
          gfx::Clip(0, 0, 0, 0, w, h),
          gfx::Clip(0, 0, zoom.apply(13), zoom.apply(9), zoom.apply(40), zoom.apply(30)),
          gfx::Clip(3, 2, zoom.apply(31), zoom.apply(27), zoom.apply(17), zoom.apply(19)),
        };
        for (const gfx::Clip& area : areas) {
          std::unique_ptr<Image> hexDst(Image::create(IMAGE_RGB, w, h));
          std::unique_ptr<Image> refDst(Image::create(IMAGE_RGB, w, h));
          clear_image(hexDst.get(), 0);
          clear_image(refDst.get(), 0);

          Render render;
          render.setProjection(Projection(PixelRatio(1, 1), zoom));
          render.renderSprite(hexDst.get(), hexSprite, frame_t(0), area);
          render.renderSprite(refDst.get(), refSprite, frame_t(0), area);

          ASSERT_EQ("", first_difference(refDst.get(), hexDst.get()))
            << "type=" << int(grid.type()) << " size=" << grid.tileSize().w << "x"
            << grid.tileSize().h << " parity=" << int(grid.parity()) << " cel=" << celPos.x << ","
            << celPos.y << " zoom=" << zoom.scale() << " area=" << area.src.x << "," << area.src.y
            << " " << area.size.w << "x" << area.size.h;
        }
      }
    }
  }
}

int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
