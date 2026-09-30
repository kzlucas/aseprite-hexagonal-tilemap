-- Copyright (C) 2026-present  Igara Studio S.A.
--
-- This file is released under the terms of the MIT license.
-- Read LICENSE.txt for more information.

-- Tools in tiles mode with hexagonal tilemaps (aseprite-custom fork).
-- The results are checked in the rendered sprite, so they don't
-- depend on how the tilemap is stored.

dofile('./test_utils.lua')

local RED = Color(255, 0, 0)
local GREEN = Color(0, 255, 0)
local BLUE = Color(0, 0, 255)
local EMPTY = Color(0, 0, 0, 0)

-- Creates a sprite with a hexagonal tilemap layer (16x16 pointy-top
-- with odd parity by default) and 3 solid tiles (red, green, blue).
-- The hexagonal settings are applied saving and loading the file.
local function hex_sprite(props, w, h)
  props = props or {}
  local tw = props.tw or 16
  local th = props.th or 16
  local spr = Sprite(w or 96, h or 80, ColorMode.RGB)
  app.command.NewLayer{ tilemap=true, gridBounds=Rectangle(0, 0, tw, th) }
  local layer = spr.layers[2]
  local ts = layer.tileset
  for _, c in ipairs({ RED, GREEN, BLUE }) do
    local tile = spr:newTile(ts)
    tile.image:clear(c)
  end
  ts.properties("aseprite-custom/hex-tilemap", {
    version=1, type=props.type or "pointy", parity=props.parity or "odd",
    step=props.step, tileSize=Size(tw, th) })

  -- A different file each time (app.open() returns the sprite if the
  -- file is already open)
  hex_sprite_count = (hex_sprite_count or 0) + 1
  local fn = app.fs.joinPath(app.fs.tempPath,
                             "_hex_tiles_mode_" .. hex_sprite_count .. ".aseprite")
  spr:saveAs(fn)
  spr:close()
  spr = app.open(fn)
  os.remove(fn)
  layer = spr.layers[2]
  -- It's a hexagonal tileset (the properties were applied and removed)
  assert(layer.tileset.properties("aseprite-custom/hex-tilemap").type == nil)
  app.layer = layer
  return spr, layer
end

local function render(spr)
  local img = Image(spr.spec)
  img:drawSprite(spr, app.frame or 1)
  return img
end

local function pixel(img, x, y)
  return Color(img:getPixel(x, y))
end

local function expect_pixel(img, x, y, expected, msg)
  local c = pixel(img, x, y)
  if c ~= expected then
    print(string.format("pixel %d,%d is %s, expected %s (%s)",
                        x, y, tostring(c.rgbaPixel), tostring(expected.rgbaPixel), msg or ""))
    assert(false)
  end
end

local function pencil(layer, tile, points, tool)
  app.useTool{ tool=tool or 'pencil', color=Color{ tile=tile }, layer=layer,
               tilemapMode=TilemapMode.TILES, points=points }
end

-- Pencil: one click places the tile in the hexagon under the pixel
do
  local spr, layer = hex_sprite()
  pencil(layer, 1, { Point(8, 8) })
  local img = render(spr)
  expect_pixel(img, 8, 8, RED)
  expect_pixel(img, 7, 0, RED, "top vertex of tile 0,0")
  expect_pixel(img, 8, 15, RED, "bottom vertex of tile 0,0")
  expect_pixel(img, 0, 0, EMPTY, "owned by tile -1,-1")
  expect_pixel(img, 15, 13, EMPTY, "owned by tile 0,1")
  expect_pixel(img, 20, 8, EMPTY, "tile 1,0")

  -- A click in the corner of the bounds of a tile places the tile
  -- that owns that pixel (tile 0,1 at 8,12)
  pencil(layer, 2, { Point(15, 13) })
  img = render(spr)
  expect_pixel(img, 15, 13, GREEN)
  expect_pixel(img, 16, 20, GREEN, "center of tile 0,1")
  expect_pixel(img, 8, 8, RED, "tile 0,0 is still red")

  -- Tiles partially outside the sprite (tile -1,-1 grows the tilemap
  -- up/left, it must keep the stagger)
  pencil(layer, 3, { Point(0, 0) })
  img = render(spr)
  expect_pixel(img, 0, 0, BLUE)
  expect_pixel(img, 3, 1, BLUE, "tile -1,-1")
  expect_pixel(img, 8, 8, RED, "tile 0,0 didn't move")
  expect_pixel(img, 16, 20, GREEN, "tile 0,1 didn't move")

  -- Undo/redo
  app.undo()
  img = render(spr)
  expect_pixel(img, 0, 0, EMPTY)
  expect_pixel(img, 8, 8, RED)
  app.redo()
  img = render(spr)
  expect_pixel(img, 0, 0, BLUE)
  expect_pixel(img, 16, 20, GREEN)

  -- Eraser
  pencil(layer, 0, { Point(8, 8) }, 'eraser')
  img = render(spr)
  expect_pixel(img, 8, 8, EMPTY)
  expect_pixel(img, 16, 20, GREEN, "neighbor not erased")
  expect_pixel(img, 0, 0, BLUE, "neighbor not erased")
end

-- Pixels of a stroke drawn by a tool (in pixels mode on a regular
-- sprite) with a 1px brush
local function stroke_pixels(w, h, tool, points)
  local spr = Sprite(w, h, ColorMode.RGB)
  app.useTool{ tool=tool, color=RED, brush=Brush(1), points=points,
               layer=spr.layers[1] }
  local pixels = {}
  local img = render(spr)
  for it in img:pixels() do
    if app.pixelColor.rgbaA(it()) > 0 then
      table.insert(pixels, Point(it.x, it.y))
    end
  end
  spr:close()
  return pixels
end

-- Line and shape tools: the hexagons touched by the stroke are painted, so
-- every pixel of the stroke is inside a painted hexagon (a
-- continuous line of hexagons, without gaps)
for _, props in ipairs({ { type="pointy", parity="odd" },
                         { type="pointy", parity="even" },
                         { type="flat", parity="odd" },
                         { type="flat", parity="even" } }) do
  for _, tool in ipairs({ 'pencil', 'line', 'rectangle', 'filled_rectangle', 'ellipse',
                          'filled_ellipse' }) do
    local spr, layer = hex_sprite(props)
    local points = { Point(5, 7), Point(70, 61) }
    local expected = stroke_pixels(spr.width, spr.height, tool, points)
    assert(#expected > 60, tool)
    app.sprite = spr
    pencil(layer, 1, points, tool)
    local img = render(spr)
    local what = props.type .. "/" .. props.parity .. " " .. tool
    for _, pt in ipairs(expected) do
      expect_pixel(img, pt.x, pt.y, RED, what)
    end
    -- Pixels far from the line are empty
    expect_pixel(img, 90, 5, EMPTY, what)
    expect_pixel(img, 3, 75, EMPTY, what)
    spr:close()
  end
end

-- Center of the tile u,v of a 16x16 pointy-top grid (odd parity)
local function center(u, v)
  return Point(16*u + (v % 2 == 1 and 8 or 0) + 8, 12*v + 8)
end

local function set_tiles(layer, tile, tiles)
  for _, t in ipairs(tiles) do
    pencil(layer, tile, { center(t[1], t[2]) })
  end
end

local function fill_all(spr, layer, tile)
  pencil(layer, tile, { Point(0, 0), Point(spr.width-1, spr.height-1) }, 'filled_rectangle')
end

-- The 6 neighbors of the tile 2,2 (an even row, not staggered)
local NEIGHBORS_22 = { {3,2}, {1,2}, {1,1}, {2,1}, {1,3}, {2,3} }

-- A stroke doesn't clear the neighbors of the painted tiles (the
-- neighbor 2,1 overlaps the bounds of the tiles 1,1 and 2,0, and it
-- was patched with an empty tile)
do
  local spr, layer = hex_sprite()
  fill_all(spr, layer, 1)
  pencil(layer, 3, { center(2, 1) })
  pencil(layer, 2, { center(1, 1), center(2, 0) })
  local img = render(spr)
  expect_pixel(img, center(1, 1).x, center(1, 1).y, GREEN, "tile 1,1")
  expect_pixel(img, center(2, 0).x, center(2, 0).y, GREEN, "tile 2,0")
  expect_pixel(img, center(2, 1).x, center(2, 1).y, BLUE, "tile 2,1 wasn't cleared")
  -- The tile 2,1 in the tilemap (the whole tile, not only its center)
  local cel = layer:cel(1)
  local u = 2 - cel.position.x // 16
  local v = 1 - cel.position.y // 12
  assert(app.pixelColor.tileI(cel.image:getPixel(u, v)) == 3)
end

-- Paint bucket: the fill is contained by the 6 neighbors of a tile
do
  local spr, layer = hex_sprite()
  fill_all(spr, layer, 1)
  set_tiles(layer, 3, NEIGHBORS_22)
  pencil(layer, 2, { center(2, 2) }, 'paint_bucket')
  local img = render(spr)
  expect_pixel(img, center(2, 2).x, center(2, 2).y, GREEN)
  -- 3,1 and 3,3 are diagonal neighbors in the tilemap image, but they
  -- aren't neighbors of the hexagon 2,2 (8-connected fill would leak)
  expect_pixel(img, center(3, 1).x, center(3, 1).y, RED, "3,1 isn't a neighbor")
  expect_pixel(img, center(3, 3).x, center(3, 3).y, RED, "3,3 isn't a neighbor")
  expect_pixel(img, center(0, 0).x, center(0, 0).y, RED)
  for _, t in ipairs(NEIGHBORS_22) do
    expect_pixel(img, center(t[1], t[2]).x, center(t[1], t[2]).y, BLUE)
  end

  -- Non-contiguous fill replaces all the red tiles
  pencil(layer, 2, { center(0, 0) }, 'paint_bucket')
  app.undo()
  app.useTool{ tool='paint_bucket', color=Color{ tile=2 }, layer=layer, contiguous=false,
               tilemapMode=TilemapMode.TILES, points={ center(0, 0) } }
  img = render(spr)
  expect_pixel(img, center(3, 1).x, center(3, 1).y, GREEN)
  expect_pixel(img, center(4, 5).x, center(4, 5).y, GREEN)
  expect_pixel(img, center(1, 1).x, center(1, 1).y, BLUE)
end

-- Paint bucket: the diagonal neighbors 1,1 and 1,3 of the hexagon 2,2
-- connect it with the rest (a 4-connected fill wouldn't)
do
  local spr, layer = hex_sprite()
  fill_all(spr, layer, 1)
  set_tiles(layer, 3, { {3,2}, {1,2}, {2,1}, {2,3} })
  pencil(layer, 2, { center(2, 2) }, 'paint_bucket')
  local img = render(spr)
  expect_pixel(img, center(2, 2).x, center(2, 2).y, GREEN)
  expect_pixel(img, center(1, 1).x, center(1, 1).y, GREEN)
  expect_pixel(img, center(0, 5).x, center(0, 5).y, GREEN)
  expect_pixel(img, center(2, 1).x, center(2, 1).y, BLUE)
end

-- Paint bucket inside a selection: only tiles with the center inside
do
  local spr, layer = hex_sprite()
  fill_all(spr, layer, 1)
  spr.selection = Selection(Rectangle(30, 25, 30, 15))
  pencil(layer, 2, { center(2, 2) }, 'paint_bucket')
  local img = render(spr)
  expect_pixel(img, center(2, 2).x, center(2, 2).y, GREEN)
  expect_pixel(img, center(3, 2).x, center(3, 2).y, GREEN, "center 56,32 is selected")
  expect_pixel(img, center(1, 2).x, center(1, 2).y, RED, "center 24,32 isn't selected")
  expect_pixel(img, center(2, 1).x, center(2, 1).y, RED, "center 48,20 isn't selected")
  spr.selection:deselect()
end

-- Magic wand: selects the hexagon (6-connected) with its shape
do
  local spr, layer = hex_sprite()
  fill_all(spr, layer, 1)
  set_tiles(layer, 3, NEIGHBORS_22)
  app.useTool{ tool='magic_wand', layer=layer, tilemapMode=TilemapMode.TILES,
               points={ center(2, 2) } }
  local sel = spr.selection
  assert(sel:contains(center(2, 2)))
  assert(sel:contains(32 + 7, 24), "top vertex of the hexagon 2,2")
  assert(not sel:contains(32, 24), "corner of the bounds, pixel of a neighbor")
  assert(not sel:contains(center(3, 1)))
  assert(not sel:contains(center(3, 2)))
  sel:deselect()
end

-- Magic wand on a tilemap that doesn't start at the first tile of the
-- canvas (the tilemap was flooded with a displacement)
do
  local spr, layer = hex_sprite()
  set_tiles(layer, 1, { {2,2}, {3,2}, {3,3}, {5,4} })
  assert(layer:cel(1).position ~= Point(0, 0))
  app.useTool{ tool='magic_wand', layer=layer, tilemapMode=TilemapMode.TILES,
               points={ center(3, 2) } }
  local sel = spr.selection
  assert(sel:contains(center(2, 2)))
  assert(sel:contains(center(3, 2)))
  assert(sel:contains(center(3, 3)))
  assert(not sel:contains(center(5, 4)), "not connected")
  assert(not sel:contains(center(4, 2)), "empty tile")
  sel:deselect()
end

-- Rectangular marquee: selects the hexagons with their centers inside,
-- with their shape
do
  local spr, layer = hex_sprite()
  fill_all(spr, layer, 1)
  -- From the hexagon 1,1 to the hexagon 2,2 (the corners are snapped
  -- to the centers of the hexagons under them)
  app.useTool{ tool='rectangular_marquee', layer=layer, tilemapMode=TilemapMode.TILES,
               points={ Point(29, 18), Point(44, 34) } }
  local sel = spr.selection
  assert(sel:contains(center(1, 1)))
  assert(sel:contains(center(2, 2)))
  assert(not sel:contains(center(1, 2)), "center 24,32 is outside")
  assert(not sel:contains(center(2, 1)), "center 48,20 is outside")
  assert(sel:contains(32 + 7, 24), "top vertex of 2,2")
  assert(not sel:contains(47, 24), "corner of the bounds of 2,2 (pixel of 2,1)")

  -- A click selects the hexagon under the mouse
  app.useTool{ tool='rectangular_marquee', layer=layer, tilemapMode=TilemapMode.TILES,
               points={ Point(50, 36) } }
  sel = spr.selection
  assert(sel:contains(center(3, 2)))
  assert(not sel:contains(center(2, 2)))
  assert(not sel:contains(center(3, 3)))
  sel:deselect()
end

-- The eyedropper can't be used from scripts (it's a command of the
-- editor): its tile picking (doc::get_tile_pixel) is tested in
-- src/doc/grid_properties_tests.cpp (GridProperties.GetTilePixel)

-- Lasso: selects the hexagons with their centers inside
do
  local spr, layer = hex_sprite()
  fill_all(spr, layer, 1)
  app.useTool{ tool='lasso', layer=layer, tilemapMode=TilemapMode.TILES,
               points={ Point(34, 26), Point(62, 26), Point(62, 38), Point(34, 38) } }
  local sel = spr.selection
  assert(sel:contains(center(2, 2)), "40,32 is inside")
  assert(sel:contains(center(3, 2)), "56,32 is inside")
  assert(not sel:contains(center(1, 2)), "24,32 is outside")
  assert(not sel:contains(center(2, 1)), "48,20 is outside")
  assert(not sel:contains(47, 24), "pixel of 2,1")
  assert(sel:contains(39, 24), "top vertex of 2,2")
  sel:deselect()
end
