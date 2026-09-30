-- Copyright (C) 2026-present  Igara Studio S.A.
--
-- This file is released under the terms of the MIT license.
-- Read LICENSE.txt for more information.

-- Sprite operations on hexagonal tilemaps behave like on square
-- tilemaps (aseprite-custom fork, see HEX_PARITY.md). The same checks
-- run for square, pointy-top and flat-top tilesets.

dofile('./test_utils.lua')

local COLORS = { Color(0, 0, 255), Color(0, 160, 0), Color(128, 0, 128), Color(0, 128, 128) }
local SHAPES = { GridShape.SQUARE, GridShape.HEX_POINTY_TOP, GridShape.HEX_FLAT_TOP }

-- A sprite with a tilemap layer that covers the whole canvas
local function make(shape, w, h)
  local spr = Sprite(w or 96, h or 80, ColorMode.RGB)
  local ts = spr:newTileset(Grid{ tileSize=Size(16, 16), shape=shape }, 1)
  for _, c in ipairs(COLORS) do
    local tile = spr:newTile(ts)
    tile.image:clear(c)
    tile.image:drawPixel(5, 5, Color(255, 255, 255))
  end
  app.command.NewLayer{ tilemap=true }
  local layer = app.layer
  layer.tileset = ts
  local i = 0
  for y = 0, spr.height + 10, 6 do
    for x = 0, spr.width + 14, 6 do
      i = i + 1
      app.useTool{ tool='pencil', color=Color{ tile=1 + (i * 7 + y) % 4 }, layer=layer,
                   tilemapMode=TilemapMode.TILES, points={ Point(x, y) } }
    end
  end
  return spr, layer
end

local function render(spr)
  local img = Image(spr.spec)
  img:drawSprite(spr, 1)
  return img
end

-- Checks that the region of "a" at ax,ay is equal to the region of "b"
-- at bx,by
local function expect_region(a, ax, ay, b, bx, by, w, h, msg)
  for y = 0, h-1 do
    for x = 0, w-1 do
      if a:getPixel(ax+x, ay+y) ~= b:getPixel(bx+x, by+y) then
        print(string.format("%s: pixel %d,%d is %08x, expected %08x", msg, ax+x, ay+y,
                            a:getPixel(ax+x, ay+y), b:getPixel(bx+x, by+y)))
        assert(false)
      end
    end
  end
end

for _, shape in ipairs(SHAPES) do
  local what = "shape " .. shape

  -- Canvas Size (expand)
  do
    local spr = make(shape)
    local before = render(spr)
    app.command.CanvasSize{ ui=false, left=10, top=7, right=3, bottom=2 }
    assert(spr.width == 109 and spr.height == 89)
    expect_region(render(spr), 10, 7, before, 0, 0, 96, 80, what .. " canvas size")
    spr:close()
  end

  -- Canvas Size (shrink) trimming the content outside the canvas, and
  -- undo
  for _, off in ipairs({ {1,1}, {5,13}, {13,9}, {20,17}, {9,30}, {30,5}, {17,25}, {40,40} }) do
    local spr, layer = make(shape)
    local before = render(spr)
    app.command.CanvasSize{ ui=false, left=-off[1], top=-off[2], right=-3, bottom=-2,
                            trimOutside=true }
    expect_region(render(spr), 0, 0, before, off[1], off[2], spr.width, spr.height,
                  what .. " canvas trim " .. off[1] .. "," .. off[2])
    app.undo()
    expect_region(render(spr), 0, 0, before, 0, 0, 96, 80, what .. " canvas trim undo")
    spr:close()
  end

  -- Crop
  do
    local spr = make(shape)
    local before = render(spr)
    spr:crop(21, 11, 50, 40)
    expect_region(render(spr), 0, 0, before, 21, 11, 50, 40, what .. " crop")
    spr:close()
  end

  -- Flatten
  do
    local spr = make(shape)
    local before = render(spr)
    app.command.FlattenLayers()
    assert(#spr.layers == 1)
    expect_region(render(spr), 0, 0, before, 0, 0, 96, 80, what .. " flatten")
    spr:close()
  end

  -- Duplicate a tilemap layer (it uses the same tileset)
  do
    local spr, layer = make(shape)
    local before = render(spr)
    app.layer = layer
    app.command.DuplicateLayer()
    assert(#spr.layers == 3)
    assert(spr.layers[3].isTilemap)
    assert(spr.layers[3].tileset.grid.shape == shape)
    spr.layers[2].isVisible = false
    expect_region(render(spr), 0, 0, before, 0, 0, 96, 80, what .. " duplicate")
    spr:close()
  end

  -- Merge Down doesn't support tilemaps (the same for all shapes)
  do
    local spr, layer = make(shape)
    app.layer = layer
    app.command.MergeDownLayer()
    assert(#spr.layers == 2)
    assert(spr.layers[2].isTilemap)
    spr:close()
  end

  -- Color mode: the transparent pixels (outside the hexagons) stay
  -- transparent after converting to indexed and back
  do
    local spr, layer = make(shape)
    local before = render(spr)
    app.command.ChangePixelFormat{ format="indexed", dithering="none" }
    for i = 1, #layer.tileset-1 do
      local img = layer.tileset:tile(i).image
      if shape ~= GridShape.SQUARE then
        assert(img:getPixel(0, 0) == img.spec.transparentColor, "corner of a hexagonal tile")
      end
    end
    app.command.ChangePixelFormat{ format="rgb" }
    assert(layer.tileset.grid.shape == shape)
    local after = render(spr)
    for y = 0, after.height-1 do
      for x = 0, after.width-1 do
        assert(app.pixelColor.rgbaA(after:getPixel(x, y)) ==
               app.pixelColor.rgbaA(before:getPixel(x, y)))
      end
    end
    spr:close()
  end
end

-- Snap to grid when drawing in pixels mode: the points go to the
-- positions of the tiles (the origin of their bounds), the equivalent
-- of the vertices of a square grid
do
  local spr = Sprite(96, 80, ColorMode.RGB)
  local ts = spr:newTileset(Grid{ tileSize=Size(16, 16), shape=GridShape.HEX_POINTY_TOP }, 1)
  app.command.NewLayer{ tilemap=true }
  local layer = app.layer
  layer.tileset = ts
  local docPref = app.preferences.document(spr)
  docPref.grid.snap = true
  -- 27,14 is closer to the position of the tile 1,1 (24,12) than to
  -- any other tile position
  app.useTool{ tool='pencil', color=Color(255, 0, 0), brush=Brush(1), layer=layer,
               tilemapMode=TilemapMode.PIXELS, tilesetMode=TilesetMode.AUTO,
               points={ Point(27, 14) } }
  docPref.grid.snap = false
  local img = render(spr)
  assert(Color(img:getPixel(24, 12)) == Color(255, 0, 0))
  assert(app.pixelColor.rgbaA(img:getPixel(27, 14)) == 0)
  spr:close()
end

-- Export Tileset: the tiles are exported with transparent pixels
-- outside the hexagons, and the JSON data describes the grid
do
  local spr, layer = make(GridShape.HEX_POINTY_TOP)
  local out_png = "_test_hex_parity_tileset.png"
  local out_json = "_test_hex_parity_tileset.json"
  app.layer = layer
  app.command.ExportTileset{ ui=false, type=SpriteSheetType.HORIZONTAL,
                             textureFilename=out_png, dataFilename=out_json }
  local img = Image{ fromFile=out_png }
  local f = io.open(out_json, "r")
  local data = json.decode(f:read("*a"))
  f:close()
  os.remove(out_png)
  os.remove(out_json)
  -- The 4 tiles (and the empty tile) in a row
  assert(img.height == 16)
  assert(img.width >= 16 * 4)
  local tiles = 0
  for x = 0, img.width - 16, 16 do
    local center = img:getPixel(x + 8, 8)
    if app.pixelColor.rgbaA(center) > 0 then
      tiles = tiles + 1
      assert(app.pixelColor.rgbaA(img:getPixel(x, 0)) == 0, "corner of a tile")
      assert(app.pixelColor.rgbaA(img:getPixel(x + 15, 15)) == 0, "corner of a tile")
    end
  end
  assert(tiles == 4)
  assert(data.meta.tilesets and data.meta.tilesets[1].shape == "hexPointyTop")
  spr:close()
end
