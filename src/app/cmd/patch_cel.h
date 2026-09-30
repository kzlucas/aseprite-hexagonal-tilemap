// Aseprite
// Copyright (C) 2001-2016  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_CMD_PATCH_CEL_H_INCLUDED
#define APP_CMD_PATCH_CEL_H_INCLUDED
#pragma once

#include "app/cmd/with_cel.h"
#include "app/cmd_sequence.h"
#include "gfx/fwd.h"
#include "gfx/point.h"
#include "gfx/region.h"

namespace doc {
class Cel;
class Image;
} // namespace doc

namespace app { namespace cmd {

class PatchCel : public CmdSequence,
                 public WithCel {
public:
  PatchCel(doc::Cel* dstCel,
           const doc::Image* patch,
           const gfx::Region& patchedRegion,
           const gfx::Point& patchPos);

  // Patches a tilemap cel of a hexagonal tileset: "patchedTiles" is
  // the region in tiles of the "patch" tilemap, which is at the
  // "patchPos" position in the canvas (see Grid::tilemapPosition()).
  struct Tiles {};
  PatchCel(doc::Cel* dstCel,
           const doc::Image* patch,
           const gfx::Region& patchedTiles,
           const gfx::Point& patchPos,
           Tiles);

protected:
  void onExecute() override;

  const doc::Image* m_patch;
  const gfx::Region& m_region;
  gfx::Point m_pos;
  bool m_tiles = false;
  gfx::Region m_tilesRegion;
};

}} // namespace app::cmd

#endif
