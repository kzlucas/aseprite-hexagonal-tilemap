-- Copyright (C) 2026-present  Igara Studio S.A.
--
-- This file is released under the terms of the MIT license.
-- Read LICENSE.txt for more information.

-- Creates a sprite with square tilemaps drawn in all tileset modes,
-- used to check that square tilemaps are saved exactly like in the
-- upstream version (see ../cli/hex-square-regression.sh).
--
-- Usage: aseprite -b --script-param out=file.aseprite
--                    --script-param mode=rgb|indexed --script make_square_tilemap.lua

local colorMode = (app.params.mode == "indexed" and ColorMode.INDEXED or ColorMode.RGB)
local spr = Sprite(64, 48, colorMode)
local c1 = (colorMode == ColorMode.RGB and Color(255, 0, 0) or 1)
local c2 = (colorMode == ColorMode.RGB and Color(0, 128, 255) or 2)
local c3 = (colorMode == ColorMode.RGB and Color(20, 200, 40, 128) or 3)

-- 16x16 tilemap drawn in Auto, Stack and Manual modes
app.command.NewLayer{ tilemap=true }
local tm16 = spr.layers[2]
app.useTool{ tool='pencil', color=c1, layer=tm16, tilesetMode=TilesetMode.AUTO,
             points={ Point(1, 1), Point(40, 20) } }
app.useTool{ tool='filled_ellipse', color=c2, layer=tm16, tilesetMode=TilesetMode.STACK,
             points={ Point(10, 10), Point(50, 40) } }
app.useTool{ tool='pencil', color=c3, layer=tm16, tilesetMode=TilesetMode.MANUAL,
             points={ Point(2, 30), Point(60, 30) } }

-- Tile flags and tileset/tile user data
local cel = tm16:cel(1)
local img = cel.image:clone()
img:drawPixel(0, 0, app.pixelColor.tile(1, 0x80000000))
img:drawPixel(1, 0, app.pixelColor.tile(2, 0x40000000))
cel.image = img
local ts = tm16.tileset
ts.name = "square16"
ts.properties.kind = "square"
ts.properties("other/extension", { n=5, s="text" })
ts:tile(1).properties.weight = 3

-- 8x8 tilemap with a grid origin offset, and a second frame
app.command.NewLayer{ tilemap=true, gridBounds=Rectangle(3, 5, 8, 8) }
local tm8 = spr.layers[3]
app.useTool{ tool='line', color=c2, layer=tm8, tilesetMode=TilesetMode.AUTO,
             points={ Point(0, 47), Point(63, 0) } }
spr:newFrame()
app.useTool{ tool='filled_rectangle', color=c1, layer=tm8, frame=spr.frames[2],
             tilesetMode=TilesetMode.AUTO, points={ Point(20, 20), Point(35, 30) } }

spr:saveAs(app.params.out)
