-- Copyright (C) 2026-present  Igara Studio S.A.
--
-- This file is released under the terms of the MIT license.
-- Read LICENSE.txt for more information.

-- Compares the rendered content of pairs of image files (size, color
-- mode, palette and pixels), ignoring how the file was compressed.
--
-- Usage: aseprite -b --script-param list=pairs.txt --script compare_png.lua
-- where each line of pairs.txt is "expected_file actual_file".

local failed = 0
for line in io.lines(app.params.list) do
  local a, b = line:match("^(%S+)%s+(%S+)$")
  if a then
    local sa = Sprite{ fromFile=a }
    local sb = Sprite{ fromFile=b }
    local same = (sa.width == sb.width and sa.height == sb.height and
                  sa.colorMode == sb.colorMode and #sa.palettes[1] == #sb.palettes[1])
    if same then
      for i = 0, #sa.palettes[1]-1 do
        if sa.palettes[1]:getColor(i) ~= sb.palettes[1]:getColor(i) then
          same = false
        end
      end
      local ia = Image(sa.spec)
      local ib = Image(sb.spec)
      ia:drawSprite(sa, 1)
      ib:drawSprite(sb, 1)
      same = same and ia:isEqual(ib)
    end
    if not same then
      print("DIFFERENT: " .. a .. " " .. b)
      failed = failed + 1
    end
    sa:close()
    sb:close()
  end
end
assert(failed == 0, failed .. " image(s) are different")
