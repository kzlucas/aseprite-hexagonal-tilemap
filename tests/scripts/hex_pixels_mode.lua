-- Copyright (C) 2026-present  Igara Studio S.A.
--
-- This file is released under the terms of the MIT license.
-- Read LICENSE.txt for more information.

-- Drawing pixels on hexagonal tilemaps (aseprite-custom fork). In
-- Auto and Stack modes the result must look exactly like drawing on a
-- regular layer. In Manual mode the tiles of the tileset are modified.

dofile('./test_utils.lua')

local RED = Color(255, 0, 0)
local YELLOW = Color(255, 255, 0)
local EMPTY = Color(0, 0, 0, 0)

local COLORS = { Color(0, 0, 255), Color(0, 160, 0), Color(128, 0, 128), Color(0, 128, 128) }

-- Creates a sprite with a hexagonal tilemap layer and solid tiles.
-- The hexagonal settings are applied saving and loading the file.
local function hex_sprite(props)
  props = props or {}
  local tw = props.tw or 16
  local th = props.th or 16
  local spr = Sprite(props.w or 96, props.h or 80, ColorMode.RGB)
  app.command.NewLayer{ tilemap=true, gridBounds=Rectangle(0, 0, tw, th) }
  local layer = spr.layers[2]
  local ts = layer.tileset
  for _, c in ipairs(COLORS) do
    local tile = spr:newTile(ts)
    tile.image:clear(c)
    -- A pixel to see flips
    tile.image:drawPixel(tw // 2 - 2, th // 2 - 2, Color(255, 255, 255))
  end
  ts.properties("aseprite-custom/hex-tilemap", {
    version=1, type=props.type or "pointy", parity=props.parity or "odd",
    step=props.step, tileSize=Size(tw, th) })

  hex_sprite_count = (hex_sprite_count or 0) + 1
  local fn = app.fs.joinPath(app.fs.tempPath,
                             "_hex_pixels_mode_" .. hex_sprite_count .. ".aseprite")
  spr:saveAs(fn)
  spr:close()
  spr = app.open(fn)
  os.remove(fn)
  layer = spr.layers[2]
  assert(layer.tileset.properties("aseprite-custom/hex-tilemap").type == nil)
  app.layer = layer
  return spr, layer
end

-- Fills the tilemap with the 4 tiles (in tiles mode)
local function fill_tiles(spr, layer)
  local i = 0
  for y = 0, spr.height + 16, 6 do
    for x = 0, spr.width + 16, 6 do
      i = i + 1
      app.useTool{ tool='pencil', color=Color{ tile=1 + (i * 7 + y) % 4 }, layer=layer,
                   tilemapMode=TilemapMode.TILES, points={ Point(x, y) } }
    end
  end
end

local function render(spr)
  local img = Image(spr.spec)
  img:drawSprite(spr, 1)
  return img
end

-- The same stroke on a regular layer with the given image
local function reference(img, tool, color, points)
  local spr = Sprite(img.width, img.height, ColorMode.RGB)
  spr.cels[1].image = img
  app.useTool{ tool=tool, color=color, brush=Brush(1), layer=spr.layers[1], points=points }
  local result = render(spr)
  spr:close()
  return result
end

local function expect_same(a, b, msg)
  for y = 0, a.height-1 do
    for x = 0, a.width-1 do
      if a:getPixel(x, y) ~= b:getPixel(x, y) then
        print(string.format("%s: pixel %d,%d is %08x, expected %08x",
                            msg, x, y, a:getPixel(x, y), b:getPixel(x, y)))
        assert(false)
      end
    end
  end
end

-- Auto and Stack modes look like drawing on a regular layer
local strokes = {
  { tool='pencil', points={ Point(4, 30), Point(90, 30) } },
  { tool='pencil', points={ Point(2, 3), Point(70, 75) } },
  { tool='pencil', points={ Point(0, 0) } },
  { tool='pencil', points={ Point(95, 79), Point(60, 10), Point(10, 60) } },
  { tool='filled_ellipse', points={ Point(20, 12), Point(70, 60) } },
  { tool='line', points={ Point(90, 2), Point(3, 77) } },
}
for _, props in ipairs({ { type="pointy", parity="odd" },
                         { type="pointy", parity="even" },
                         { type="flat", parity="odd" },
                         { type="flat", parity="even", tw=12, th=18, step=10 },
                         { type="pointy", parity="odd", tw=14, th=12, step=10 } }) do
  for _, mode in ipairs({ TilesetMode.AUTO, TilesetMode.STACK }) do
    for i, s in ipairs(strokes) do
      local what = props.type .. "/" .. props.parity .. " mode=" .. mode .. " stroke " .. i

      -- On an empty tilemap
      local spr, layer = hex_sprite(props)
      app.useTool{ tool=s.tool, color=RED, brush=Brush(1), layer=layer,
                   tilemapMode=TilemapMode.PIXELS, tilesetMode=mode, points=s.points }
      expect_same(render(spr), reference(Image(spr.spec), s.tool, RED, s.points),
                  what .. " (empty)")
      spr:close()

      -- On a tilemap with tiles
      spr, layer = hex_sprite(props)
      fill_tiles(spr, layer)
      local before = render(spr)
      local ntiles = #layer.tileset
      app.useTool{ tool=s.tool, color=RED, brush=Brush(1), layer=layer,
                   tilemapMode=TilemapMode.PIXELS, tilesetMode=mode, points=s.points }
      expect_same(render(spr), reference(before, s.tool, RED, s.points), what)

      -- Undo restores the tiles and the tileset
      app.sprite = spr
      app.undo()
      expect_same(render(spr), before, what .. " (undo)")
      assert(#layer.tileset == ntiles)
      spr:close()
    end
  end
end

-- Eraser in Auto mode
do
  local spr, layer = hex_sprite()
  fill_tiles(spr, layer)
  local before = render(spr)
  local points = { Point(10, 20), Point(80, 50) }
  app.useTool{ tool='eraser', brush=Brush(3), layer=layer, tilemapMode=TilemapMode.PIXELS,
               tilesetMode=TilesetMode.AUTO, points=points }
  local ref = Sprite(before.width, before.height, ColorMode.RGB)
  ref.cels[1].image = before
  app.useTool{ tool='eraser', brush=Brush(3), layer=ref.layers[1], points=points }
  local expected = render(ref)
  ref:close()
  expect_same(render(spr), expected, "eraser")
  spr:close()
end

-- Manual mode modifies the tiles of the tileset (all the instances
-- of the tile), and it doesn't change the tilemap
do
  local spr, layer = hex_sprite()
  fill_tiles(spr, layer)
  local cel = layer:cel(1)
  local tilemapBefore = cel.image:clone()
  local ntiles = #layer.tileset
  local before = render(spr)

  -- A point in the center of the tile 2,2 (40,32)
  app.useTool{ tool='pencil', color=YELLOW, brush=Brush(1), layer=layer,
               tilemapMode=TilemapMode.PIXELS, tilesetMode=TilesetMode.MANUAL,
               points={ Point(40, 32) } }
  local after = render(spr)
  assert(Color(after:getPixel(40, 32)) == YELLOW)
  assert(cel.image:isEqual(tilemapBefore), "the tilemap wasn't modified")
  assert(#layer.tileset == ntiles, "no new tiles")

  -- The same pixel changed in all the instances of the tile (and
  -- only that pixel changed in each instance)
  local t = cel.image:getPixel(2 - cel.position.x // 16, 2 - cel.position.y // 12)
  local changed = 0
  for y = 0, after.height-1 do
    for x = 0, after.width-1 do
      if after:getPixel(x, y) ~= before:getPixel(x, y) then
        changed = changed + 1
        assert(Color(after:getPixel(x, y)) == YELLOW)
      end
    end
  end
  local instances = 0
  for it in cel.image:pixels() do
    if app.pixelColor.tileI(it()) == app.pixelColor.tileI(t) then
      local px = cel.position.x + 16 * it.x + (it.y % 2 == 1 and 8 or 0) + 8
      local py = cel.position.y + 12 * it.y + 8
      if px >= 0 and py >= 0 and px < spr.width and py < spr.height then
        instances = instances + 1
      end
    end
  end
  assert(instances > 1)
  assert(changed == instances, "changed=" .. changed .. " instances=" .. instances)

  app.sprite = spr
  app.undo()
  expect_same(render(spr), before, "manual undo")
  spr:close()
end

-- Manual mode doesn't modify the pixels of the neighbor hexagons (the
-- stroke crosses the edges of several tiles)
do
  local spr, layer = hex_sprite()
  fill_tiles(spr, layer)
  local before = render(spr)
  local points = { Point(26, 28), Point(58, 36) }
  app.useTool{ tool='pencil', color=YELLOW, brush=Brush(1), layer=layer,
               tilemapMode=TilemapMode.PIXELS, tilesetMode=TilesetMode.MANUAL,
               points=points }
  local after = render(spr)
  -- Each pixel of the stroke is yellow
  local stroke = reference(Image(spr.spec), 'pencil', YELLOW, points)
  for it in stroke:pixels() do
    if app.pixelColor.rgbaA(it()) > 0 then
      assert(Color(after:getPixel(it.x, it.y)) == YELLOW)
    end
  end
  spr:close()
end

-- Filters (Manual mode in scripts) modify every visible pixel, also
-- when the tilemap starts at a negative position (it used to leave an
-- empty area that was copied into the tiles)
do
  local spr, layer = hex_sprite()
  fill_tiles(spr, layer)
  assert(layer:cel(1).position.x < 0 and layer:cel(1).position.y < 0)
  local before = render(spr)
  app.command.InvertColor{ ui=false, channels=FilterChannels.RGB }
  local after = render(spr)
  for y = 0, after.height-1 do
    for x = 0, after.width-1 do
      local a = after:getPixel(x, y)
      local b = before:getPixel(x, y)
      assert(app.pixelColor.rgbaA(a) == app.pixelColor.rgbaA(b))
      assert(app.pixelColor.rgbaR(a) == 255 - app.pixelColor.rgbaR(b))
    end
  end
  spr:close()
end

-- Edit > Clear (Manual mode in scripts) clears the selected pixels of
-- the tiles (so the same pixels of other instances of the tiles too)
for _, rc in ipairs({ Rectangle(20, 17, 33, 21), Rectangle(0, 0, 30, 20),
                      Rectangle(70, 60, 26, 20) }) do
  local spr, layer = hex_sprite()
  fill_tiles(spr, layer)
  local before = render(spr)
  spr.selection = Selection(rc)
  app.command.Clear()
  local after = render(spr)
  for y = 0, after.height-1 do
    for x = 0, after.width-1 do
      local a = after:getPixel(x, y)
      if rc:contains(Point(x, y)) then
        assert(app.pixelColor.rgbaA(a) == 0)
      elseif a ~= before:getPixel(x, y) then
        assert(app.pixelColor.rgbaA(a) == 0)
      end
    end
  end
  spr:close()
end

-- Pixel perfect in Manual mode: the stroke is painted, and only
-- instances of the modified tiles change (it used to copy pixels of
-- neighbor hexagons)
do
  local spr, layer = hex_sprite()
  fill_tiles(spr, layer)
  local before = render(spr)
  local points = { Point(10, 20), Point(11, 20), Point(12, 21), Point(13, 21), Point(14, 22),
                   Point(30, 30), Point(31, 31), Point(32, 31), Point(45, 40) }
  app.useTool{ tool='pencil', color=YELLOW, brush=Brush(1), layer=layer, freehandAlgorithm=1,
               tilemapMode=TilemapMode.PIXELS, tilesetMode=TilesetMode.MANUAL, points=points }
  local after = render(spr)
  for y = 0, after.height-1 do
    for x = 0, after.width-1 do
      if after:getPixel(x, y) ~= before:getPixel(x, y) then
        assert(Color(after:getPixel(x, y)) == YELLOW)
      end
    end
  end
  local ref = Sprite(spr.width, spr.height, ColorMode.RGB)
  app.useTool{ tool='pencil', color=YELLOW, brush=Brush(1), layer=ref.layers[1],
               freehandAlgorithm=1, points=points }
  local stroke = render(ref)
  ref:close()
  for it in stroke:pixels() do
    if app.pixelColor.rgbaA(it()) > 0 then
      assert(Color(after:getPixel(it.x, it.y)) == YELLOW)
    end
  end
  spr:close()
end

-- Convert a hexagonal tilemap to a regular layer
do
  local spr, layer = hex_sprite()
  fill_tiles(spr, layer)
  local before = render(spr)
  app.command.ConvertLayer{ to="layer" }
  assert(not spr.layers[2].isTilemap)
  expect_same(render(spr), before, "convert to layer")
  spr:close()
end

-- Paint bucket in pixels mode with "stop at grid": the fill stops at
-- the edges of the hexagon under the point
do
  local spr, layer = hex_sprite()
  -- All tiles with the same color, so the fill would cover the whole
  -- tilemap without stopping at the grid
  app.useTool{ tool='filled_rectangle', color=Color{ tile=1 }, layer=layer,
               tilemapMode=TilemapMode.TILES, points={ Point(0, 0), Point(95, 79) } }
  local before = render(spr)
  local pref = app.preferences.tool('paint_bucket').floodfill
  local old = pref.stop_at_grid
  pref.stop_at_grid = 2 -- Always
  -- The center of the tile 2,2 (40,32)
  app.useTool{ tool='paint_bucket', color=YELLOW, layer=layer, tilemapMode=TilemapMode.PIXELS,
               tilesetMode=TilesetMode.AUTO, points={ Point(40, 32) } }
  pref.stop_at_grid = old
  local after = render(spr)
  -- The hexagon 2,2 of a 16x16 pointy-top grid (odd parity) starts at
  -- 32,24, its left and right edges are at x=32 and x=47 (rows 28..35),
  -- and its top/bottom vertices are at 39..40, 24 and 39..40, 39
  local changed = 0
  for y = 0, after.height-1 do
    for x = 0, after.width-1 do
      if after:getPixel(x, y) ~= before:getPixel(x, y) then
        changed = changed + 1
        assert(Color(after:getPixel(x, y)) == YELLOW)
        assert(x >= 32 and x < 48 and y >= 24 and y < 40, "outside the hexagon " .. x .. "," .. y)
      end
    end
  end
  assert(Color(after:getPixel(32, 30)) == YELLOW, "left edge")
  assert(Color(after:getPixel(47, 30)) == YELLOW, "right edge")
  assert(Color(after:getPixel(39, 24)) == YELLOW, "top vertex")
  assert(Color(after:getPixel(32, 24)) ~= YELLOW, "corner of the bounds (neighbor)")
  assert(changed > 150 and changed < 16*16, "changed=" .. changed)
  spr:close()
end
