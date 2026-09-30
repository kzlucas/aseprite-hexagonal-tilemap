-- Copyright (C) 2026-present  Igara Studio S.A.
--
-- This file is released under the terms of the MIT license.
-- Read LICENSE.txt for more information.

-- Flip and rotate the canvas with hexagonal tilemaps (aseprite-custom
-- fork). The positions of the tiles are mirrored around the axis of
-- the grid closest to the center of the canvas (keeping the stagger
-- of the grid), and the tiles themselves aren't flipped (like with
-- orthogonal tilemaps).

dofile('./test_utils.lua')

local COLORS = { Color(255, 0, 0), Color(0, 255, 0), Color(0, 0, 255), Color(255, 255, 0) }

-- Creates a sprite with a hexagonal tilemap layer (16x16, odd parity)
-- and 4 solid tiles. The hexagonal settings are applied saving and
-- loading the file.
local function hex_sprite(type, w, h)
  local spr = Sprite(w, h, ColorMode.RGB)
  app.command.NewLayer{ tilemap=true, gridBounds=Rectangle(0, 0, 16, 16) }
  local layer = spr.layers[2]
  local ts = layer.tileset
  for _, c in ipairs(COLORS) do
    local tile = spr:newTile(ts)
    tile.image:clear(c)
  end
  ts.properties("aseprite-custom/hex-tilemap", {
    version=1, type=type, parity="odd", tileSize=Size(16, 16) })

  hex_sprite_count = (hex_sprite_count or 0) + 1
  local fn = app.fs.joinPath(app.fs.tempPath,
                             "_hex_canvas_flip_" .. hex_sprite_count .. ".aseprite")
  spr:saveAs(fn)
  spr:close()
  spr = app.open(fn)
  os.remove(fn)
  layer = spr.layers[2]
  assert(layer.tileset.properties("aseprite-custom/hex-tilemap").type == nil)
  app.layer = layer

  -- Some tiles in staggered and non-staggered rows/columns
  local points = { Point(8, 8), Point(30, 20), Point(40, 40), Point(12, 50), Point(50, 8) }
  for i, pt in ipairs(points) do
    app.useTool{ tool='pencil', color=Color{ tile=1 + (i % 4) }, layer=layer,
                 tilemapMode=TilemapMode.TILES, points={ pt } }
  end
  return spr, layer
end

local function render(spr)
  local img = Image(spr.spec)
  img:drawSprite(spr, 1)
  return img
end

-- Checks that img(x,y) == ref(f(x,y)) for all pixels of img where
-- f(x,y) is inside ref
local function expect_mapped(img, ref, f, msg)
  local n = 0
  for y = 0, img.height-1 do
    for x = 0, img.width-1 do
      local u, v = f(x, y)
      if u >= 0 and v >= 0 and u < ref.width and v < ref.height then
        if img:getPixel(x, y) ~= ref:getPixel(u, v) then
          print(string.format("%s: pixel %d,%d is %08x, expected %08x (from %d,%d)",
                              msg, x, y, img:getPixel(x, y), ref:getPixel(u, v), u, v))
          assert(false)
        end
        n = n + 1
      end
    end
  end
  assert(n > 0)
end

local function same(x, y) return x, y end

for _, type in ipairs({ "pointy", "flat" }) do
  -- The center of a 64x64 canvas is an axis of the grid for both flips
  do
    local spr = hex_sprite(type, 64, 64)
    local before = render(spr)
    assert(not before:isEmpty())

    app.command.Flip{ target="canvas", orientation="horizontal" }
    assert(not render(spr):isEqual(before))
    expect_mapped(render(spr), before, function(x, y) return 63-x, y end, type .. " flip h")
    app.undo()
    expect_mapped(render(spr), before, same, type .. " undo flip h")

    app.command.Flip{ target="canvas", orientation="vertical" }
    assert(not render(spr):isEqual(before))
    expect_mapped(render(spr), before, function(x, y) return x, 63-y end, type .. " flip v")
    app.undo()

    app.command.Rotate{ target="canvas", angle=180 }
    assert(not render(spr):isEqual(before))
    expect_mapped(render(spr), before, function(x, y) return 63-x, 63-y end, type .. " rotate 180")
    app.undo()
    expect_mapped(render(spr), before, same, type .. " undo rotate 180")

    -- Flipping twice gives the original tilemap
    app.command.Flip{ target="canvas", orientation="horizontal" }
    app.command.Flip{ target="canvas", orientation="horizontal" }
    expect_mapped(render(spr), before, same, type .. " flip h twice")

    -- Hexagonal tiles can't be rotated 90 degrees: the tiles stay in
    -- their place
    app.command.Rotate{ target="canvas", angle=90 }
    expect_mapped(render(spr), before, same, type .. " rotate 90")
  end

  -- The center of a 72 pixels wide canvas isn't an axis of the grid:
  -- the closest axis is used
  do
    local spr = hex_sprite(type, 72, 64)
    local before = render(spr)
    app.command.Flip{ target="canvas", orientation="horizontal" }
    local after = render(spr)
    if type == "pointy" then
      -- Across the stagger: axes every 8 pixels, x=36 is in the middle
      -- of x=32 and x=40, and the one with an even index (x=40) is used
      expect_mapped(after, before, function(x, y) return 79-x, y end, "pointy flip h (72)")
    else
      -- Along the stagger: columns keep their stagger with axes every
      -- 12 pixels (x=8, 20, 32, 44...), the closest one to x=36 is 32
      expect_mapped(after, before, function(x, y) return 63-x, y end, "flat flip h (72)")
    end
  end
end
