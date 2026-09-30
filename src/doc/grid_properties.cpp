// Aseprite Document Library
// Copyright (c) 2026-present  Igara Studio S.A.
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "doc/grid_properties.h"

#include "doc/grid.h"
#include "doc/image.h"
#include "doc/primitives.h"
#include "doc/tileset.h"

#include <cmath>
#include <string>

namespace doc {

const char* kHexGridExtensionId = "aseprite-custom/hex-tilemap";

namespace {

// Version of the properties format
const int kVersion = 1;

// Characters used to store a custom mask as a vector of strings (one
// string per row)
const char kMaskOn = '#';
const char kMaskOff = '.';

// Integers are saved in the smallest type that can hold the value, so
// we accept any integer type.
bool get_int(const UserData::Variant& value, int& result)
{
  switch (value.type()) {
    case USER_DATA_PROPERTY_TYPE_INT8:   result = get_value<int8_t>(value); return true;
    case USER_DATA_PROPERTY_TYPE_UINT8:  result = get_value<uint8_t>(value); return true;
    case USER_DATA_PROPERTY_TYPE_INT16:  result = get_value<int16_t>(value); return true;
    case USER_DATA_PROPERTY_TYPE_UINT16: result = get_value<uint16_t>(value); return true;
    case USER_DATA_PROPERTY_TYPE_INT32:  result = get_value<int32_t>(value); return true;
    case USER_DATA_PROPERTY_TYPE_UINT32: result = int(get_value<uint32_t>(value)); return true;
    case USER_DATA_PROPERTY_TYPE_INT64:  result = int(get_value<int64_t>(value)); return true;
    case USER_DATA_PROPERTY_TYPE_UINT64: result = int(get_value<uint64_t>(value)); return true;
  }
  return false;
}

std::string get_string(const UserData::Properties& props, const char* key)
{
  auto it = props.find(key);
  if (it != props.end() && it->second.type() == USER_DATA_PROPERTY_TYPE_STRING)
    return get_value<std::string>(it->second);
  return std::string();
}

ImageRef mask_from_rows(const UserData::Vector& rows, const gfx::Size& tileSize)
{
  if (int(rows.size()) != tileSize.h)
    return ImageRef();

  ImageRef mask(Image::create(IMAGE_BITMAP, tileSize.w, tileSize.h));
  for (int y = 0; y < tileSize.h; ++y) {
    if (rows[y].type() != USER_DATA_PROPERTY_TYPE_STRING)
      return ImageRef();
    const std::string& row = get_value<std::string>(rows[y]);
    if (int(row.size()) != tileSize.w)
      return ImageRef();
    for (int x = 0; x < tileSize.w; ++x)
      mask->putPixel(x, y, row[x] == kMaskOn ? 1 : 0);
  }
  return mask;
}

} // anonymous namespace

UserData::Properties hex_grid_properties(const Grid& grid)
{
  ASSERT(grid.isHexagonal());

  UserData::Properties props;
  props["version"] = int32_t(kVersion);
  props["type"] = std::string(grid.type() == Grid::Type::HexFlatTop ? "flat" : "pointy");
  props["parity"] = std::string(grid.parity() == Grid::Parity::Even ? "even" : "odd");
  props["step"] = int32_t(grid.hexStep());
  props["tileSize"] = grid.tileSize();

  // The mask is stored only if it's not the automatic one
  if (grid.hasMask()) {
    const Image* mask = grid.mask().get();
    ImageRef autoMask = Grid::MakeHexMask(grid.tileSize(), grid.type(), grid.hexStep());
    if (!is_same_image(autoMask.get(), mask)) {
      UserData::Vector rows;
      for (int y = 0; y < mask->height(); ++y) {
        std::string row(mask->width(), kMaskOff);
        for (int x = 0; x < mask->width(); ++x)
          if (mask->getPixel(x, y))
            row[x] = kMaskOn;
        rows.push_back(row);
      }
      props["mask"] = rows;
    }
  }
  return props;
}

bool grid_from_hex_properties(const UserData::Properties& props,
                              const gfx::Size& tileSize,
                              Grid& grid)
{
  Grid::Type type;
  const std::string typeStr = get_string(props, "type");
  if (typeStr == "pointy")
    type = Grid::Type::HexPointyTop;
  else if (typeStr == "flat")
    type = Grid::Type::HexFlatTop;
  else
    return false;

  const Grid::Parity parity = (get_string(props, "parity") == "even" ? Grid::Parity::Even :
                                                                       Grid::Parity::Odd);

  int step = 0;
  auto it = props.find("step");
  if (it != props.end())
    get_int(it->second, step);

  ImageRef mask;
  it = props.find("tileSize");
  const bool sameSize = (it == props.end() || it->second.type() != USER_DATA_PROPERTY_TYPE_SIZE ||
                         get_value<gfx::Size>(it->second) == tileSize);
  if (sameSize) {
    it = props.find("mask");
    if (it != props.end() && it->second.type() == USER_DATA_PROPERTY_TYPE_VECTOR)
      mask = mask_from_rows(get_value<UserData::Vector>(it->second), tileSize);
  }
  // The tileset was resized, so we scale the step (along the stagger
  // axis) and use the automatic mask.
  else if (step > 0) {
    const gfx::Size oldSize = get_value<gfx::Size>(it->second);
    const int oldH = (type == Grid::Type::HexFlatTop ? oldSize.w : oldSize.h);
    const int newH = (type == Grid::Type::HexFlatTop ? tileSize.w : tileSize.h);
    step = (oldH > 0 ? int(std::lround(double(step) * newH / oldH)) : 0);
  }

  // MakeHex() clamps the step and replaces an invalid mask with the
  // automatic one.
  grid = Grid::MakeHex(tileSize, type, parity, step, mask);
  return true;
}

UserData tileset_user_data_with_grid(const Tileset* tileset)
{
  UserData userData = tileset->userData();
  if (tileset->grid().isHexagonal())
    userData.propertiesMaps()[kHexGridExtensionId] = hex_grid_properties(tileset->grid());
  return userData;
}

bool apply_tileset_grid_properties(Tileset* tileset)
{
  auto& maps = tileset->userData().propertiesMaps();
  auto it = maps.find(kHexGridExtensionId);
  if (it == maps.end())
    return false;

  // Unknown properties (e.g. from a newer version) are kept in the
  // user data so they are saved again.
  Grid grid;
  if (!grid_from_hex_properties(it->second, tileset->grid().tileSize(), grid))
    return false;

  maps.erase(it);
  tileset->setGrid(grid);
  return true;
}

} // namespace doc
