// Aseprite
// Copyright (C) 2019-2020  Igara Studio S.A.
// Copyright (C) 2016  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "app/cmd/trim_cel.h"

#include "app/cmd/crop_cel.h"
#include "app/cmd/remove_cel.h"
#include "doc/algorithm/shrink_bounds.h"
#include "doc/cel.h"
#include "doc/layer.h"
#include "doc/layer_tilemap.h"
#include "doc/sprite.h"
#include "doc/tileset.h"

namespace app { namespace cmd {

using namespace doc;

TrimCel::TrimCel(Cel* cel)
{
  gfx::Rect newBounds = cel->bounds();
  bool trimmed = false;

  // Hexagonal tilemaps are trimmed in tiles (keeping the stagger of
  // the grid)
  if (cel->layer()->isTilemap() &&
      static_cast<LayerTilemap*>(cel->layer())->tileset()->grid().isHexagonal()) {
    gfx::Rect tiles;
    trimmed =
      algorithm::shrink_bounds(cel->image(), cel->image()->maskColor(), cel->layer(), tiles);
    if (trimmed) {
      Grid grid = cel->grid();
      tiles = grid.alignTilesToStagger(tiles);
      if (tiles != cel->image()->bounds())
        add(new cmd::CropCel(cel, tiles, CropCel::Tiles()));
    }
  }
  else if ((trimmed = algorithm::shrink_cel_bounds(cel, cel->image()->maskColor(), newBounds))) {
    if (cel->bounds() != newBounds)
      add(new cmd::CropCel(cel, newBounds));
  }

  if (!trimmed) {
    // Delete the given "cel" and all its links.
    Sprite* sprite = cel->sprite();
    Layer* layer = cel->layer();
    CelData* celData = cel->dataRef().get();

    for (frame_t fr = sprite->totalFrames() - 1; fr >= 0; --fr) {
      Cel* c = layer->cel(fr);
      if (c && c->dataRef().get() == celData)
        add(new cmd::RemoveCel(c));
    }
  }
}

}} // namespace app::cmd
