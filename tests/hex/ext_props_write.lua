-- Copyright (C) 2026-present  Igara Studio S.A.
--
-- This file is released under the terms of the MIT license.
-- Read LICENSE.txt for more information.

-- Writes a sprite with a tilemap whose tileset has extension
-- properties with the same shape used by hexagonal tilesets.
--
-- Usage: aseprite -b --script-param out=file.aseprite --script ext_props_write.lua

local spr = Sprite(32, 32, ColorMode.RGB)
app.command.NewLayer{ tilemap=true }
local tilemap = spr.layers[2]
app.useTool{ tool='pencil', color=Color(255, 0, 0), layer=tilemap,
             tilesetMode=TilesetMode.AUTO, points={ Point(1, 1), Point(20, 20) } }

local ts = tilemap.tileset
ts.properties("aseprite-custom/hex-tilemap", {
  version=1, type="pointy", parity="even", step=12, tileSize=Size(16, 16),
  mask={ "..##..", "######", "..##.." } })
ts.properties.user_value = "kept"
spr:saveAs(app.params.out)
