-- Copyright (C) 2026-present  Igara Studio S.A.
--
-- This file is released under the terms of the MIT license.
-- Read LICENSE.txt for more information.

-- Writes a sprite with a tilemap layer whose tileset has the
-- hexagonal grid properties (hex_props.lua). Any version of Aseprite
-- can write it; this fork loads it as a hexagonal tileset.
--
-- Usage: aseprite -b --script-param out=file.aseprite --script hex_make.lua

local props = dofile('./hex_props.lua')
local spr = Sprite(64, 48, ColorMode.RGB)
app.command.NewLayer{ tilemap=true }
local tilemap = spr.layers[2]
app.useTool{ tool='filled_rectangle', color=Color(255, 0, 0), layer=tilemap,
             tilesetMode=TilesetMode.STACK, points={ Point(0, 0), Point(47, 31) } }
app.useTool{ tool='pencil', color=Color(0, 0, 255), layer=tilemap,
             tilesetMode=TilesetMode.STACK, points={ Point(0, 0), Point(47, 31) } }

local ts = tilemap.tileset
ts.name = "hex"
ts.properties("aseprite-custom/hex-tilemap", props)
ts.properties.user_value = "kept"
ts.properties("other/extension", { n=5 })
spr:saveAs(app.params.out)
