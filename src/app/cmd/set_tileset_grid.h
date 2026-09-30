// Aseprite
// Copyright (C) 2026-present  Igara Studio S.A.
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_CMD_SET_TILESET_GRID_H_INCLUDED
#define APP_CMD_SET_TILESET_GRID_H_INCLUDED
#pragma once

#include "app/cmd.h"
#include "app/cmd/with_tileset.h"
#include "doc/grid.h"
#include "doc/image_ref.h"
#include "doc/tile.h"

#include <utility>
#include <vector>

namespace app { namespace cmd {
using namespace doc;

// Changes the grid of a tileset (e.g. from orthogonal to hexagonal,
// or the hexagonal settings). The tile size must be the same. Pixels
// outside the mask of a hexagonal grid are cleared from the tiles,
// and restored on undo.
class SetTilesetGrid : public Cmd,
                       public WithTileset {
public:
  SetTilesetGrid(Tileset* tileset, const Grid& grid);

protected:
  void onExecute() override;
  void onUndo() override;
  void onFireNotifications() override;
  size_t onMemSize() const override;

private:
  void setGrid(const Grid& grid);

  Grid m_oldGrid;
  Grid m_newGrid;
  // Copies of the tiles modified by the new mask
  std::vector<std::pair<tile_index, ImageRef>> m_oldTiles;
};

}} // namespace app::cmd

#endif
