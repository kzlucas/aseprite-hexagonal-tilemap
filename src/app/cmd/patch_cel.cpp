// Aseprite
// Copyright (C) 2020  Igara Studio S.A.
// Copyright (C) 2016  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "app/cmd/patch_cel.h"

#include "app/cmd/copy_region.h"
#include "app/cmd/crop_cel.h"
#include "app/cmd/trim_cel.h"
#include "doc/cel.h"
#include "doc/layer_tilemap.h"

namespace app { namespace cmd {

using namespace doc;

PatchCel::PatchCel(doc::Cel* dstCel,
                   const doc::Image* patch,
                   const gfx::Region& patchedRegion,
                   const gfx::Point& patchPos)
  : WithCel(dstCel)
  , m_patch(patch)
  , m_region(patchedRegion)
  , m_pos(patchPos)
{
  ASSERT(!patchedRegion.isEmpty());
}

PatchCel::PatchCel(doc::Cel* dstCel,
                   const doc::Image* patch,
                   const gfx::Region& patchedTiles,
                   const gfx::Point& patchPos,
                   Tiles)
  : WithCel(dstCel)
  , m_patch(patch)
  , m_region(m_tilesRegion)
  , m_pos(patchPos)
  , m_tiles(true)
  , m_tilesRegion(patchedTiles)
{
  ASSERT(!patchedTiles.isEmpty());
  ASSERT(dstCel->image()->pixelFormat() == IMAGE_TILEMAP);
}

void PatchCel::onExecute()
{
  Cel* cel = this->cel();

  // Hexagonal tilemaps are patched in tiles
  if (m_tiles) {
    // Position of the patch in the tiles of the cel
    const Grid grid = cel->grid();
    const gfx::Point patchTile = grid.originToTile(m_pos);

    const gfx::Rect oldTiles = cel->image()->bounds();
    const gfx::Rect newTiles = grid.alignTilesToStagger(
      oldTiles | gfx::Rect(m_tilesRegion.bounds()).offset(patchTile));
    if (newTiles != oldTiles)
      executeAndAdd(new CropCel(cel, newTiles, CropCel::Tiles()));

    executeAndAdd(
      new CopyRegion(cel->image(), m_patch, m_tilesRegion, patchTile - newTiles.origin()));
    executeAndAdd(new TrimCel(cel));
    m_patch = nullptr;
    return;
  }

  gfx::Rect newBounds;
  gfx::Region regionInTiles;
  doc::Grid grid;
  if (cel->image()->pixelFormat() == IMAGE_TILEMAP) {
    newBounds = cel->bounds() | m_region.bounds();
    auto tileset = static_cast<LayerTilemap*>(cel->layer())->tileset();
    grid = tileset->grid();
    grid.origin(m_pos);
    regionInTiles = grid.canvasToTile(m_region);
  }
  else {
    newBounds = cel->bounds() | gfx::Rect(m_region.bounds()).offset(m_pos);
  }

  if (cel->bounds() != newBounds)
    executeAndAdd(new CropCel(cel, newBounds));

  if (cel->image()->pixelFormat() == IMAGE_TILEMAP) {
    executeAndAdd(
      new CopyRegion(cel->image(), m_patch, regionInTiles, -grid.canvasToTile(cel->position())));
  }
  else {
    executeAndAdd(new CopyRegion(cel->image(), m_patch, m_region, m_pos - cel->position()));
  }

  executeAndAdd(new TrimCel(cel));

  m_patch = nullptr;
}

}} // namespace app::cmd
