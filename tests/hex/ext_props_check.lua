-- Copyright (C) 2026-present  Igara Studio S.A.
--
-- This file is released under the terms of the MIT license.
-- Read LICENSE.txt for more information.

-- Checks the file written by ext_props_write.lua (after being loaded
-- and saved by any version of Aseprite).
--
-- Usage: aseprite -b --script-param in=file.aseprite --script ext_props_check.lua

local spr = app.open(app.params["in"])
assert(spr)
local ts = spr.layers[2].tileset
assert(#ts >= 2)

local p = ts.properties("aseprite-custom/hex-tilemap")
assert(p.version == 1)
assert(p.type == "pointy")
assert(p.parity == "even")
assert(p.step == 12)
assert(p.tileSize == Size(16, 16))
assert(#p.mask == 3)
assert(p.mask[1] == "..##..")
assert(p.mask[2] == "######")
assert(p.mask[3] == "..##..")
assert(ts.properties.user_value == "kept")
print("OK: extension properties intact")
