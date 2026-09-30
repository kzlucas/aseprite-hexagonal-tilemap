// Aseprite
// Copyright (C) 2019  Igara Studio S.A.
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "app/script/docobj.h"
#include "app/script/engine.h"
#include "app/script/luacpp.h"
#include "doc/grid.h"
#include "doc/image.h"

#include <cstring>
#include <string>

namespace app { namespace script {

using namespace doc;

namespace {

// Values of the GridShape and GridParity constants
enum GridShape { SQUARE = 0, HEX_POINTY_TOP = 1, HEX_FLAT_TOP = 2 };
enum GridParity { ODD = 0, EVEN = 1 };

// Characters of the rows of a hexagonal mask (Grid.mask)
const char kMaskOn = '#';
const char kMaskOff = '.';

// Returns true if the table at the given index has the fields of a
// grid (Grid{ tileSize=..., shape=... }) instead of a rectangle.
bool is_grid_table(lua_State* L, int index)
{
  if (!lua_istable(L, index))
    return false;
  const int type1 = lua_getfield(L, index, "tileSize");
  const int type2 = lua_getfield(L, index, "shape");
  lua_pop(L, 2);
  return (type1 != LUA_TNIL || type2 != LUA_TNIL);
}

// Creates a mask from a table of strings (one string per row, '#' for
// the pixels of the tile) or an image (pixels with a value != 0).
ImageRef mask_from_arg(lua_State* L, int index, const gfx::Size& tileSize)
{
  ImageRef mask(Image::create(IMAGE_BITMAP, tileSize.w, tileSize.h));
  mask->clear(0);

  if (const Image* image = may_get_image_from_arg(L, index)) {
    if (image->size() != tileSize)
      luaL_error(L, "the mask image must have the size of the tiles");
    for (int y = 0; y < tileSize.h; ++y)
      for (int x = 0; x < tileSize.w; ++x)
        mask->putPixel(x, y, (image->getPixel(x, y) != 0 ? 1 : 0));
    return mask;
  }

  if (!lua_istable(L, index))
    luaL_error(L, "the mask must be a table of strings or an image");
  if (int(luaL_len(L, index)) != tileSize.h)
    luaL_error(L, "the mask must have one string for each row of the tiles");

  for (int y = 0; y < tileSize.h; ++y) {
    lua_geti(L, index, y + 1);
    const char* row = lua_tostring(L, -1);
    if (!row || int(std::strlen(row)) != tileSize.w)
      luaL_error(L, "each row of the mask must be a string with a character for each pixel");
    for (int x = 0; x < tileSize.w; ++x) {
      if (row[x] != kMaskOn && row[x] != kMaskOff)
        luaL_error(L, "the mask must contain only '%c' and '%c' characters", kMaskOn, kMaskOff);
      mask->putPixel(x, y, (row[x] == kMaskOn ? 1 : 0));
    }
    lua_pop(L, 1);
  }
  return mask;
}

// Grid{ tileSize=Size, origin=Point, shape=GridShape, parity=GridParity,
//       step=integer, mask=table|Image }
doc::Grid grid_from_table(lua_State* L, int index)
{
  index = lua_absindex(L, index);

  gfx::Size tileSize(16, 16);
  if (lua_getfield(L, index, "tileSize") != LUA_TNIL)
    tileSize = convert_args_into_size(L, lua_absindex(L, -1));
  lua_pop(L, 1);
  if (tileSize.w < 1 || tileSize.h < 1)
    luaL_error(L, "invalid tile size");

  gfx::Point origin(0, 0);
  if (lua_getfield(L, index, "origin") != LUA_TNIL)
    origin = convert_args_into_point(L, lua_absindex(L, -1));
  lua_pop(L, 1);

  int shape = GridShape::SQUARE;
  if (lua_getfield(L, index, "shape") != LUA_TNIL)
    shape = lua_tointeger(L, -1);
  lua_pop(L, 1);

  doc::Grid grid(tileSize);
  if (shape == GridShape::HEX_POINTY_TOP || shape == GridShape::HEX_FLAT_TOP) {
    const Grid::Type type = (shape == GridShape::HEX_FLAT_TOP ? Grid::Type::HexFlatTop :
                                                                Grid::Type::HexPointyTop);

    Grid::Parity parity = Grid::Parity::Odd;
    if (lua_getfield(L, index, "parity") != LUA_TNIL)
      parity = (lua_tointeger(L, -1) == GridParity::EVEN ? Grid::Parity::Even : Grid::Parity::Odd);
    lua_pop(L, 1);

    int step = 0;
    if (lua_getfield(L, index, "step") != LUA_TNIL) {
      step = lua_tointeger(L, -1);
      const int minStep = Grid::MinHexStep(tileSize, type);
      const int maxStep = Grid::MaxHexStep(tileSize, type);
      if (step < minStep || step > maxStep)
        luaL_error(L, "the step must be between %d and %d for this tile size", minStep, maxStep);
    }
    lua_pop(L, 1);

    ImageRef mask;
    if (lua_getfield(L, index, "mask") != LUA_TNIL)
      mask = mask_from_arg(L, lua_absindex(L, -1), tileSize);
    lua_pop(L, 1);

    Grid::TessellationError error;
    grid = Grid::MakeHex(tileSize, type, parity, step, mask, &error);
    if (mask && error.kind != Grid::TessellationError::Kind::None) {
      switch (error.kind) {
        case Grid::TessellationError::Kind::Gap:
          luaL_error(L,
                     "the mask doesn't tessellate: the pixel %d,%d isn't covered by any tile",
                     error.pos.x,
                     error.pos.y);
          break;
        case Grid::TessellationError::Kind::Overlap:
          luaL_error(L,
                     "the mask doesn't tessellate: the pixel %d,%d is covered by %d tiles",
                     error.pos.x,
                     error.pos.y,
                     error.coverage);
          break;
        default: luaL_error(L, "invalid mask"); break;
      }
    }
  }
  else if (shape != GridShape::SQUARE) {
    luaL_error(L, "invalid grid shape");
  }

  grid.origin(origin);
  return grid;
}

doc::Grid Grid_new(lua_State* L, int index)
{
  doc::Grid grid;
  // Copy other size
  if (auto grid2 = may_get_obj<doc::Grid>(L, index)) {
    grid = *grid2;
  }
  // Create a grid from its fields (e.g. a hexagonal grid)
  else if (is_grid_table(L, index)) {
    grid = grid_from_table(L, index);
  }
  // Convert Rectangle into a Grid
  else if (lua_istable(L, index)) {
    gfx::Rect rect = convert_args_into_rect(L, index);
    grid = Grid(rect.size());
    grid.origin(rect.origin());
  }
  return grid;
}

int Grid_new(lua_State* L)
{
  push_obj(L, Grid_new(L, 1));
  return 1;
}

int Grid_gc(lua_State* L)
{
  get_obj<doc::Grid>(L, 1)->~Grid();
  return 0;
}

// Returns the tile (Point) that owns the given canvas pixel
int Grid_canvasToTile(lua_State* L)
{
  auto grid = get_obj<doc::Grid>(L, 1);
  const gfx::Point pt = convert_args_into_point(L, 2);
  push_obj(L, grid->canvasToTile(pt));
  return 1;
}

// Returns the canvas position of the bounds of the given tile (Point)
int Grid_tileToCanvas(lua_State* L)
{
  auto grid = get_obj<doc::Grid>(L, 1);
  const gfx::Point tile = convert_args_into_point(L, 2);
  push_obj(L, grid->tileToCanvas(tile));
  return 1;
}

// Returns a table with the tiles adjacent to the given tile (6 tiles in
// hexagonal grids, 4 tiles in square grids)
int Grid_neighbors(lua_State* L)
{
  auto grid = get_obj<doc::Grid>(L, 1);
  const gfx::Point tile = convert_args_into_point(L, 2);

  lua_newtable(L);
  int i = 0;
  if (grid->isHexagonal()) {
    for (const gfx::Point& pt : hex_neighbors(tile, *grid)) {
      push_obj(L, pt);
      lua_rawseti(L, -2, ++i);
    }
  }
  else {
    for (const gfx::Point& delta :
         { gfx::Point(1, 0), gfx::Point(0, -1), gfx::Point(-1, 0), gfx::Point(0, 1) }) {
      push_obj(L, tile + delta);
      lua_rawseti(L, -2, ++i);
    }
  }
  return 1;
}

int Grid_get_tileSize(lua_State* L)
{
  auto grid = get_obj<doc::Grid>(L, 1);
  push_obj(L, grid->tileSize());
  return 1;
}

int Grid_get_origin(lua_State* L)
{
  auto grid = get_obj<doc::Grid>(L, 1);
  push_obj(L, grid->origin());
  return 1;
}

// The origin of a Grid object can be changed (e.g. to use the grid of
// a tileset with the position of a tilemap cel)
int Grid_set_origin(lua_State* L)
{
  auto grid = get_obj<doc::Grid>(L, 1);
  grid->origin(convert_args_into_point(L, 2));
  return 0;
}

int Grid_get_shape(lua_State* L)
{
  auto grid = get_obj<doc::Grid>(L, 1);
  switch (grid->type()) {
    case Grid::Type::HexPointyTop: lua_pushinteger(L, GridShape::HEX_POINTY_TOP); break;
    case Grid::Type::HexFlatTop:   lua_pushinteger(L, GridShape::HEX_FLAT_TOP); break;
    default:                       lua_pushinteger(L, GridShape::SQUARE); break;
  }
  return 1;
}

int Grid_get_parity(lua_State* L)
{
  auto grid = get_obj<doc::Grid>(L, 1);
  lua_pushinteger(L, grid->parity() == Grid::Parity::Even ? GridParity::EVEN : GridParity::ODD);
  return 1;
}

// Distance between rows (pointy-top) or columns (flat-top) of a
// hexagonal grid, 0 for square grids
int Grid_get_step(lua_State* L)
{
  auto grid = get_obj<doc::Grid>(L, 1);
  lua_pushinteger(L, grid->hexStep());
  return 1;
}

// Returns the mask of the tiles of a hexagonal grid as a table of
// strings (one per row, '#' for the pixels of the tile), or nil for
// square grids
int Grid_get_mask(lua_State* L)
{
  auto grid = get_obj<doc::Grid>(L, 1);
  if (!grid->isHexagonal() || !grid->hasMask())
    return 0;

  const Image* mask = grid->mask().get();
  lua_newtable(L);
  for (int y = 0; y < mask->height(); ++y) {
    std::string row(mask->width(), kMaskOff);
    for (int x = 0; x < mask->width(); ++x)
      if (mask->getPixel(x, y))
        row[x] = kMaskOn;
    lua_pushstring(L, row.c_str());
    lua_rawseti(L, -2, y + 1);
  }
  return 1;
}

// Tile flags (app.pixelColor.TILE_XFLIP, etc.) that can be used with
// the tiles of this grid
int Grid_get_validFlips(lua_State* L)
{
  auto grid = get_obj<doc::Grid>(L, 1);
  lua_pushinteger(L, grid->validTileFlags());
  return 1;
}

const luaL_Reg Grid_methods[] = {
  { "__gc",         Grid_gc           },
  { "canvasToTile", Grid_canvasToTile },
  { "tileToCanvas", Grid_tileToCanvas },
  { "neighbors",    Grid_neighbors    },
  { nullptr,        nullptr           }
};

const Property Grid_properties[] = {
  { "tileSize",   Grid_get_tileSize,   nullptr         },
  { "origin",     Grid_get_origin,     Grid_set_origin },
  { "shape",      Grid_get_shape,      nullptr         },
  { "parity",     Grid_get_parity,     nullptr         },
  { "step",       Grid_get_step,       nullptr         },
  { "mask",       Grid_get_mask,       nullptr         },
  { "validFlips", Grid_get_validFlips, nullptr         },
  { nullptr,      nullptr,             nullptr         }
};

} // anonymous namespace

DEF_MTNAME(Grid);

void register_grid_class(lua_State* L)
{
  using Grid = doc::Grid;
  REG_CLASS(L, Grid);
  REG_CLASS_NEW(L, Grid);
  REG_CLASS_PROPERTIES(L, Grid);

  lua_newtable(L);
  lua_pushvalue(L, -1);
  lua_setglobal(L, "GridShape");
  setfield_integer(L, "SQUARE", GridShape::SQUARE);
  setfield_integer(L, "HEX_POINTY_TOP", GridShape::HEX_POINTY_TOP);
  setfield_integer(L, "HEX_FLAT_TOP", GridShape::HEX_FLAT_TOP);
  lua_pop(L, 1);

  lua_newtable(L);
  lua_pushvalue(L, -1);
  lua_setglobal(L, "GridParity");
  setfield_integer(L, "ODD", GridParity::ODD);
  setfield_integer(L, "EVEN", GridParity::EVEN);
  lua_pop(L, 1);
}

doc::Grid convert_args_into_grid(lua_State* L, int index)
{
  return Grid_new(L, index);
}

}} // namespace app::script
