// Aseprite Document Library
// Copyright (c) 2026-present  Igara Studio S.A.
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifndef DOC_GRID_PROPERTIES_H_INCLUDED
#define DOC_GRID_PROPERTIES_H_INCLUDED
#pragma once

#include "doc/user_data.h"
#include "gfx/size.h"

namespace doc {

class Grid;
class Tileset;

// Extension ID of the properties map that stores the settings of a
// hexagonal tileset grid in the tileset user data (see "Hexagonal
// Tilesets" in docs/ase-file-specs.md). Other versions of Aseprite
// keep and save these properties as they are.
extern const char* kHexGridExtensionId;

// Returns the properties that describe the given hexagonal grid.
UserData::Properties hex_grid_properties(const Grid& grid);

// Creates a hexagonal grid for tiles of the given size from the
// properties. Returns false if the properties don't describe a known
// grid. If the tile size doesn't match the size stored in the
// properties (e.g. the tileset was resized in other program), the
// step is scaled and the automatic mask is used.
bool grid_from_hex_properties(const UserData::Properties& props,
                              const gfx::Size& tileSize,
                              Grid& grid);

// Returns the user data of the tileset to be saved in a file: the
// same user data plus the hexagonal grid properties if the tileset
// grid is hexagonal.
UserData tileset_user_data_with_grid(const Tileset* tileset);

// Called after loading the user data of a tileset from a file: if it
// contains hexagonal grid properties, they are removed from the user
// data and applied to the tileset grid. Returns true if the grid was
// changed.
bool apply_tileset_grid_properties(Tileset* tileset);

} // namespace doc

#endif
