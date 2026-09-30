-- Copyright (C) 2026-present  Igara Studio S.A.
--
-- This file is released under the terms of the MIT license.
-- Read LICENSE.txt for more information.

-- Checks (with a version of Aseprite without hexagonal tilesets) that
-- a file saved by this fork contains the hexagonal grid properties
-- of hex_props.lua and the other user properties.
--
-- Usage: aseprite -b --script-param in=file.aseprite --script hex_props_check.lua

local expected = dofile('./hex_props.lua')
local spr = app.open(app.params["in"])
local ts = spr.layers[2].tileset
local p = ts.properties("aseprite-custom/hex-tilemap")
assert(p.version == expected.version)
assert(p.type == expected.type)
assert(p.parity == expected.parity)
assert(p.step == expected.step)
assert(p.tileSize == expected.tileSize)
assert(#p.mask == #expected.mask)
for i = 1, #expected.mask do
  assert(p.mask[i] == expected.mask[i], "mask row " .. i)
end
assert(ts.name == "hex")
assert(ts.properties.user_value == "kept")
assert(ts.properties("other/extension").n == 5)
print("OK: hexagonal grid properties intact")
