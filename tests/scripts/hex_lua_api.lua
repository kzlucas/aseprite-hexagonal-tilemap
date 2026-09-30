-- Copyright (C) 2026-present  Igara Studio S.A.
--
-- This file is released under the terms of the MIT license.
-- Read LICENSE.txt for more information.

-- Lua API for hexagonal grids (aseprite-custom fork): Grid{...},
-- GridShape, GridParity, Grid.shape/parity/step/mask/validFlips,
-- Grid:canvasToTile()/tileToCanvas()/neighbors(), Tileset.grid, and
-- meta.tilesets in the JSON data of sprite sheets.

dofile('./test_utils.lua')

local TILE_XFLIP = app.pixelColor.TILE_XFLIP
local TILE_YFLIP = app.pixelColor.TILE_YFLIP
local TILE_DFLIP = app.pixelColor.TILE_DFLIP

assert(app.apiVersion >= 42)

local function expect_error(f, text)
  local ok, err = pcall(f)
  assert(not ok, "expected an error: " .. text)
  assert(string.find(err, text, 1, true), "unexpected error: " .. tostring(err))
end

-- Square grids (as before)
do
  local g = Grid{ tileSize=Size(16, 8), origin=Point(2, 3) }
  assert(g.shape == GridShape.SQUARE)
  assert(g.tileSize == Size(16, 8))
  assert(g.origin == Point(2, 3))
  assert(g.step == 0)
  assert(g.mask == nil)
  assert(g.validFlips == TILE_XFLIP | TILE_YFLIP | TILE_DFLIP)
  assert(g:canvasToTile(Point(2, 3)) == Point(0, 0))
  assert(g:canvasToTile(Point(1, 2)) == Point(-1, -1))
  assert(g:tileToCanvas(Point(1, 2)) == Point(18, 19))
  local n = g:neighbors(Point(0, 0))
  assert(#n == 4)

  -- The rectangle form of Grid() is the same as before
  local r = Grid{ x=0, y=0, width=8, height=12 }
  assert(r.shape == GridShape.SQUARE)
  assert(r.tileSize == Size(8, 12))
end

-- Pointy-top grid with the default settings
do
  local g = Grid{ tileSize=Size(16, 16), shape=GridShape.HEX_POINTY_TOP }
  assert(g.shape == GridShape.HEX_POINTY_TOP)
  assert(g.parity == GridParity.ODD)
  assert(g.step == 12)
  assert(g.origin == Point(0, 0))
  assert(g.validFlips == TILE_XFLIP | TILE_YFLIP)
  local mask = g.mask
  assert(#mask == 16)
  assert(mask[1] == ".......##.......")
  assert(mask[8] == "################")
  assert(mask[16] == ".......##.......")

  -- The pixel 0,0 belongs to the tile -1,-1, and 15,13 to the tile 0,1
  -- (the hexagons overlap their bounds)
  assert(g:canvasToTile(Point(0, 0)) == Point(-1, -1))
  assert(g:canvasToTile(Point(8, 8)) == Point(0, 0))
  assert(g:canvasToTile(Point(15, 13)) == Point(0, 1))
  -- Odd rows are shifted half a tile
  assert(g:tileToCanvas(Point(0, 1)) == Point(8, 12))
  assert(g:tileToCanvas(Point(2, 2)) == Point(32, 24))

  -- Each tile is the owner of the pixel at its center
  for v = -2, 3 do
    for u = -2, 3 do
      local pt = g:tileToCanvas(Point(u, v))
      assert(g:canvasToTile(Point(pt.x + 8, pt.y + 8)) == Point(u, v))
    end
  end

  -- 6 neighbors of an even row and of an odd row
  local n = g:neighbors(Point(2, 2))
  assert(#n == 6)
  local set = {}
  for _, pt in ipairs(n) do set[pt.x .. "," .. pt.y] = true end
  for _, k in ipairs({ "3,2", "1,2", "1,1", "2,1", "1,3", "2,3" }) do
    assert(set[k], "neighbor " .. k)
  end
  set = {}
  for _, pt in ipairs(g:neighbors(Point(2, 1))) do set[pt.x .. "," .. pt.y] = true end
  for _, k in ipairs({ "3,1", "1,1", "2,0", "3,0", "2,2", "3,2" }) do
    assert(set[k], "neighbor " .. k)
  end

  -- The origin of a Grid object can be changed (e.g. to use the
  -- position of a tilemap cel)
  g.origin = Point(-16, -24)
  assert(g:canvasToTile(Point(-8, -16)) == Point(0, 0))
  assert(g:tileToCanvas(Point(0, 1)) == Point(-8, -12))
end

-- Flat-top grid, even parity, explicit step
do
  local g = Grid{ tileSize=Size(12, 18), shape=GridShape.HEX_FLAT_TOP, parity=GridParity.EVEN,
                  step=10 }
  assert(g.shape == GridShape.HEX_FLAT_TOP)
  assert(g.parity == GridParity.EVEN)
  assert(g.step == 10)
  assert(#g.mask == 18)
  assert(#g.mask[1] == 12)
  -- Even columns are shifted half a tile (vertically)
  assert(g:tileToCanvas(Point(0, 0)) == Point(0, 9))
  assert(g:tileToCanvas(Point(1, 0)) == Point(10, 0))
  assert(#g:neighbors(Point(3, 3)) == 6)
end

-- Odd widths make staggered bricks (rows of 12 pixels)
do
  local g = Grid{ tileSize=Size(15, 16), shape=GridShape.HEX_POINTY_TOP }
  assert(g.shape == GridShape.HEX_POINTY_TOP)
  assert(g.step == 12)
  assert(g.mask[1] == string.rep(".", 15))
  assert(g.mask[3] == string.rep("#", 15))
  assert(g.mask[14] == string.rep("#", 15))
  assert(g.mask[15] == string.rep(".", 15))
end

-- Invalid grids
do
  expect_error(function()
    return Grid{ tileSize=Size(16, 16), shape=GridShape.HEX_POINTY_TOP, step=30 }
  end, "the step must be between")
  expect_error(function()
    return Grid{ tileSize=Size(16, 16), shape=99 }
  end, "invalid grid shape")

  -- A full square mask overlaps its neighbors
  local rows = {}
  for i = 1, 16 do rows[i] = string.rep("#", 16) end
  expect_error(function()
    return Grid{ tileSize=Size(16, 16), shape=GridShape.HEX_POINTY_TOP, mask=rows }
  end, "doesn't tessellate")

  -- A mask with a hole
  local g = Grid{ tileSize=Size(16, 16), shape=GridShape.HEX_POINTY_TOP }
  rows = g.mask
  rows[8] = "#######.########"
  expect_error(function()
    return Grid{ tileSize=Size(16, 16), shape=GridShape.HEX_POINTY_TOP, mask=rows }
  end, "isn't covered by any tile")

  expect_error(function()
    return Grid{ tileSize=Size(16, 16), shape=GridShape.HEX_POINTY_TOP, mask={ "#" } }
  end, "one string for each row")
  rows = g.mask
  rows[3] = "x" .. string.sub(rows[3], 2)
  expect_error(function()
    return Grid{ tileSize=Size(16, 16), shape=GridShape.HEX_POINTY_TOP, mask=rows }
  end, "only '#' and '.'")
end

-- A custom mask that tessellates (the automatic mask moved one pixel
-- from the top vertex to the left half of the bottom vertex, the same
-- change in the tile below keeps the tiling), with flips allowed only
-- if the mask is symmetric
do
  local auto = Grid{ tileSize=Size(16, 16), shape=GridShape.HEX_POINTY_TOP }
  local g = Grid{ tileSize=Size(16, 16), shape=GridShape.HEX_POINTY_TOP, mask=auto.mask }
  assert(g.validFlips == TILE_XFLIP | TILE_YFLIP)
  for i, row in ipairs(auto.mask) do assert(g.mask[i] == row) end

  -- An asymmetric mask: the top vertex is moved to x=5..8, and the row
  -- 12 (where the tiles of the next row start) is its complement
  -- shifted by half a tile (the same mask as in hex_tiles_tests.cpp)
  local rows = auto.mask
  rows[1] = ".....####......."
  local row13 = {}
  for x = 0, 15 do
    local src = (x + 8) % 16
    row13[#row13 + 1] = ((src >= 5 and src <= 8) and "." or "#")
  end
  rows[13] = table.concat(row13)
  local g2 = Grid{ tileSize=Size(16, 16), shape=GridShape.HEX_POINTY_TOP, mask=rows }
  assert(g2.mask[1] == ".....####.......")
  assert(g2.validFlips & TILE_XFLIP == 0)
  assert(g2.validFlips & TILE_YFLIP == 0)
  assert(g2.validFlips & TILE_DFLIP == 0)
end

-- Tilesets with hexagonal grids
do
  local spr = Sprite(64, 64)

  -- newTileset with a Grid object and with a table
  local ts = spr:newTileset(Grid{ tileSize=Size(16, 16), shape=GridShape.HEX_FLAT_TOP,
                                   parity=GridParity.EVEN }, 3)
  assert(#ts == 3)
  assert(ts.grid.shape == GridShape.HEX_FLAT_TOP)
  assert(ts.grid.parity == GridParity.EVEN)
  assert(ts.grid.step == 12)

  local ts2 = spr:newTileset({ tileSize=Size(12, 12), shape=GridShape.HEX_POINTY_TOP }, 2)
  assert(ts2.grid.shape == GridShape.HEX_POINTY_TOP)
  assert(ts2.grid.step == 9)

  -- Tileset.grid = changes the grid (undoable), keeping the tile size
  ts2.grid = Grid{ tileSize=Size(12, 12) }
  assert(ts2.grid.shape == GridShape.SQUARE)
  app.undo()
  assert(ts2.grid.shape == GridShape.HEX_POINTY_TOP)
  app.redo()
  assert(ts2.grid.shape == GridShape.SQUARE)
  expect_error(function()
    ts2.grid = Grid{ tileSize=Size(8, 8), shape=GridShape.HEX_POINTY_TOP }
  end, "the tile size of a tileset cannot be changed")

  -- The origin of a tileset grid is always 0,0
  ts2.grid = Grid{ tileSize=Size(12, 12), origin=Point(5, 5), shape=GridShape.HEX_POINTY_TOP }
  assert(ts2.grid.origin == Point(0, 0))
  assert(ts2.grid.shape == GridShape.HEX_POINTY_TOP)

  -- Pixels outside the mask of the tiles are transparent
  local img = Image(16, 16, ColorMode.RGB)
  img:clear(Color(255, 0, 0))
  ts:tile(1).image = img
  local timg = ts:tile(1).image
  -- Flat-top 16x16 mask: the left/right vertices are in the middle rows
  assert(app.pixelColor.rgbaA(timg:getPixel(0, 0)) == 0)
  assert(app.pixelColor.rgbaA(timg:getPixel(8, 8)) == 255)
  timg:drawPixel(15, 0, Color(0, 255, 0))
  assert(app.pixelColor.rgbaA(ts:tile(1).image:getPixel(15, 0)) == 0)
  timg:drawPixel(8, 7, Color(0, 255, 0))
  assert(Color(ts:tile(1).image:getPixel(8, 7)) == Color(0, 255, 0))

  -- A tilemap layer with the hexagonal tileset
  app.command.NewLayer{ tilemap=true, gridBounds=Rectangle(0, 0, 16, 16) }
  local layer = app.layer
  layer.tileset = ts
  assert(layer.tileset.grid.shape == GridShape.HEX_FLAT_TOP)

  -- Tiles mode: place the tile 1 in the hexagon under a point, and
  -- find it with the grid of the cel (the tileset grid with the cel
  -- position as origin)
  app.useTool{ tool='pencil', color=Color{ tile=1 }, layer=layer,
               tilemapMode=TilemapMode.TILES, points={ Point(30, 30) } }
  local cel = layer:cel(1)
  local g = layer.tileset.grid
  g.origin = cel.position
  local tile = g:canvasToTile(Point(30, 30))
  assert(app.pixelColor.tileI(cel.image:getPixel(tile.x, tile.y)) == 1)

  -- Saving and loading keeps the hexagonal settings
  local fn = app.fs.joinPath(app.fs.tempPath, "_hex_lua_api.aseprite")
  spr:saveAs(fn)
  spr:close()
  spr = app.open(fn)
  os.remove(fn)
  local found = false
  for _, t in ipairs(spr.tilesets) do
    if t.grid.shape == GridShape.HEX_FLAT_TOP then
      assert(t.grid.parity == GridParity.EVEN)
      assert(t.grid.step == 12)
      found = true
    end
  end
  assert(found)
  spr:close()
end

-- JSON data of sprite sheets: meta.tilesets only when there are
-- hexagonal tilesets
local function export_json(sprite)
  local out_json = "_test_hex_lua_api.json"
  local out_png = "_test_hex_lua_api.png"
  app.sprite = sprite
  app.command.ExportSpriteSheet{
    ui=false,
    type=SpriteSheetType.PACKED,
    data=SpriteSheetDataFormat.JSON_HASH,
    dataFilename=out_json,
    textureFilename=out_png
  }
  local f = io.open(out_json, "r")
  local content = f:read("*a")
  f:close()
  os.remove(out_json)
  os.remove(out_png)
  return json.decode(content)
end

do
  local spr = Sprite(32, 32)
  app.command.NewLayer{ tilemap=true, gridBounds=Rectangle(0, 0, 8, 8) }
  local meta = export_json(spr).meta
  assert(meta.tilesets == nil, "no meta.tilesets for square tilesets")

  local layer = app.layer
  layer.tileset.grid = Grid{ tileSize=Size(8, 8), shape=GridShape.HEX_POINTY_TOP }
  layer.tileset.name = "Hexes"
  meta = export_json(spr).meta
  assert(meta.tilesets)
  assert(#meta.tilesets == 1)
  local t = meta.tilesets[1]
  assert(t.index == 0)
  assert(t.name == "Hexes")
  assert(t.tileSize.w == 8 and t.tileSize.h == 8)
  assert(t.shape == "hexPointyTop")
  assert(t.parity == "odd")
  assert(t.step == 6)
  assert(#t.mask == 8)
  assert(t.mask[1] == layer.tileset.grid.mask[1])
  assert(#t.layers == 1 and t.layers[1] == layer.name)
  spr:close()
end

-- Image:saveAs() of a hexagonal tilemap cel saves the rendered tiles
do
  local spr = Sprite(64, 48)
  local ts = spr:newTileset(Grid{ tileSize=Size(16, 16), shape=GridShape.HEX_POINTY_TOP }, 1)
  for i, c in ipairs({ Color(255, 0, 0), Color(0, 0, 255) }) do
    local tile = spr:newTile(ts)
    tile.image:clear(c)
  end
  app.command.NewLayer{ tilemap=true, gridBounds=Rectangle(0, 0, 16, 16) }
  local layer = app.layer
  layer.tileset = ts
  app.useTool{ tool='pencil', color=Color{ tile=1 }, layer=layer,
               tilemapMode=TilemapMode.TILES, points={ Point(20, 20) } }
  app.useTool{ tool='pencil', color=Color{ tile=2 }, layer=layer,
               tilemapMode=TilemapMode.TILES, points={ Point(36, 20) } }
  local cel = layer:cel(1)
  local fn = app.fs.joinPath(app.fs.tempPath, "_hex_lua_api_cel.png")
  cel.image:saveAs(fn)
  local saved = Image{ fromFile=fn }
  os.remove(fn)

  local render = Image(spr.spec)
  render:drawSprite(spr, 1)
  local n = 0
  for y = 0, saved.height-1 do
    for x = 0, saved.width-1 do
      local cx = cel.position.x + x
      local cy = cel.position.y + y
      if cx >= 0 and cy >= 0 and cx < spr.width and cy < spr.height then
        local a = saved:getPixel(x, y)
        local b = render:getPixel(cx, cy)
        if app.pixelColor.rgbaA(a) > 0 or app.pixelColor.rgbaA(b) > 0 then
          assert(a == b, "pixel " .. cx .. "," .. cy)
          n = n + 1
        end
      end
    end
  end
  assert(n > 300)
  spr:close()
end
