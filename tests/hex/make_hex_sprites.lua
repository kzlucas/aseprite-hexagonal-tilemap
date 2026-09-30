-- Copyright (C) 2026-present  Igara Studio S.A.
--
-- This file is released under the terms of the MIT license.
-- Read LICENSE.txt for more information.

-- Creates sprites with hexagonal tilemaps in several configurations
-- (orientation, parity, step, odd sizes, custom mask) to test how they
-- are rendered and exported (see ../cli/hex-render.sh). Each tile has
-- a different color and a darker diagonal line (so flips are visible),
-- and some tiles are flipped.
--
-- The hexagonal settings are saved as the extension properties of the
-- tileset (see docs/ase-file-specs.md NOTE.7), so the file is loaded
-- as a hexagonal tileset by this fork.
--
-- Usage: aseprite -b --script-param dir=output_dir --script make_hex_sprites.lua

local customMask = dofile('./hex_props.lua').mask

local configs = {
  { name="pointy-odd",  w=16, h=16, type="pointy", parity="odd" },
  { name="pointy-even", w=16, h=16, type="pointy", parity="even", step=12, mask=customMask },
  { name="pointy-14x12", w=14, h=12, type="pointy", parity="odd", step=10 },
  { name="flat-odd",    w=16, h=16, type="flat",   parity="odd" },
  { name="flat-even",   w=12, h=18, type="flat",   parity="even", step=10 },
  { name="pointy-15x16-brick", w=15, h=16, type="pointy", parity="odd" },
}

for _, c in ipairs(configs) do
  local cols, rows = 5, 4
  local spr = Sprite(cols*c.w + c.w, rows*c.h + c.h, ColorMode.RGB)
  app.command.NewLayer{ tilemap=true, gridBounds=Rectangle(0, 0, c.w, c.h) }
  local layer = spr.layers[2]

  -- One tile per cell (drawn while the grid is still square)
  for v = 0, rows-1 do
    for u = 0, cols-1 do
      local x, y = u*c.w, v*c.h
      local color = Color{ r=40 + u*45, g=40 + v*60, b=200 - u*30, a=255 }
      app.useTool{ tool='filled_rectangle', color=color, layer=layer,
                   tilesetMode=TilesetMode.STACK,
                   points={ Point(x, y), Point(x + c.w - 1, y + c.h - 1) } }
      local dark = Color{ r=color.red // 3, g=color.green // 3, b=color.blue // 3, a=255 }
      app.useTool{ tool='line', color=dark, layer=layer,
                   tilesetMode=TilesetMode.MANUAL,
                   points={ Point(x + 2, y + 3), Point(x + c.w - 3, y + c.h // 2) } }
    end
  end

  -- Flip some tiles. Only X and Y flips are valid for hexagons, and
  -- only if the mask is symmetric on that axis: the custom mask isn't
  -- symmetric vertically, so its Y flips are invalid on purpose (the
  -- flipped tiles must still be clipped to the mask when rendered).
  local cel = layer:cel(1)
  local img = cel.image:clone()
  for it in img:pixels() do
    local t = it()
    local i = app.pixelColor.tileI(t)
    if i > 0 then
      local flags = 0
      if (it.x + it.y) % 3 == 0 then flags = flags | 0x80000000 end
      if (it.x * it.y) % 4 == 1 then flags = flags | 0x40000000 end
      it(app.pixelColor.tile(i, flags))
    end
  end
  cel.image = img

  local props = { version=1, type=c.type, parity=c.parity, tileSize=Size(c.w, c.h) }
  if c.step then props.step = c.step end
  if c.mask then props.mask = c.mask end
  layer.tileset.properties("aseprite-custom/hex-tilemap", props)

  spr:saveAs(app.params.dir .. "/" .. c.name .. ".aseprite")
  spr:close()
end
