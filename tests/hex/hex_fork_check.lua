-- Copyright (C) 2026-present  Igara Studio S.A.
--
-- This file is released under the terms of the MIT license.
-- Read LICENSE.txt for more information.

-- Checks (with this fork) that a file with the hexagonal grid
-- properties of hex_props.lua is loaded as a hexagonal tileset: the
-- properties are removed from the user data, and the tilemap cel
-- bounds use the hexagonal grid.
--
-- Usage: aseprite -b --script-param in=file.aseprite --script hex_fork_check.lua

local spr = app.open(app.params["in"])
local layer = spr.layers[2]
local ts = layer.tileset
assert(ts.properties("aseprite-custom/hex-tilemap").type == nil)
assert(ts.properties.user_value == "kept")
assert(ts.properties("other/extension").n == 5)

-- 16x16 pointy-top, step=12, even parity (row 0 is staggered): a
-- tilemap of WxH tiles is W*16+8 x (H-1)*12+16 pixels.
local cel = layer:cel(1)
local w = cel.image.width
local h = cel.image.height
assert(cel.bounds.width == w*16 + 8,
       "width " .. cel.bounds.width .. " for " .. w .. " tiles")
assert(cel.bounds.height == (h-1)*12 + 16,
       "height " .. cel.bounds.height .. " for " .. h .. " tiles")
print("OK: loaded as a hexagonal tileset (" .. w .. "x" .. h .. " tiles, " .. #ts .. " tiles)")
