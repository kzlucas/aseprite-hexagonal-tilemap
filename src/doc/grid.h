// Aseprite Document Library
// Copyright (c) 2019-present  Igara Studio S.A.
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifndef DOC_GRID_H_INCLUDED
#define DOC_GRID_H_INCLUDED
#pragma once

#include "doc/image_ref.h"
#include "doc/tile.h"
#include "gfx/fwd.h"
#include "gfx/point.h"
#include "gfx/size.h"

#include <array>
#include <vector>

namespace doc {

class Grid {
public:
  // Hexagonal grids stagger every other row (pointy-top) or column
  // (flat-top) by half a tile, their tiles overlap, and the mask
  // decides which tile owns each canvas pixel.
  enum class Type {
    Orthogonal,
    HexPointyTop,
    HexFlatTop,
  };

  // Rows (pointy-top) or columns (flat-top) shifted by half a tile.
  enum class Parity {
    Odd,
    Even,
  };

  // Result of validateTessellation().
  struct TessellationError {
    enum class Kind {
      None,
      BadGrid, // Empty tile size or step
      BadMask, // The mask isn't a bitmap of tileSize()
      Gap,     // A pixel isn't covered by any tile
      Overlap, // A pixel is covered by two or more tiles
    };
    Kind kind = Kind::None;
    gfx::Point pos;   // First wrong pixel (canvas coordinates)
    int coverage = 1; // Number of tiles covering "pos"
    int count = 0;    // Number of wrong pixels in the tested area
  };

  explicit Grid(const gfx::Size& sz = gfx::Size(16, 16))
    : m_tileSize(sz)
    , m_origin(0, 0)
    , m_tileCenter(sz.w / 2, sz.h / 2)
    , m_tileOffset(sz)
    , m_oddRowOffset(0, 0)
    , m_oddColOffset(0, 0)
    , m_mask(nullptr)
  {
  }

  explicit Grid(const gfx::Rect& rc)
    : m_tileSize(rc.size())
    , m_origin(rc.origin())
    , m_tileCenter(m_tileSize.w / 2, m_tileSize.h / 2)
    , m_tileOffset(m_tileSize)
    , m_oddRowOffset(0, 0)
    , m_oddColOffset(0, 0)
    , m_mask(nullptr)
  {
  }

  static Grid MakeRect(const gfx::Size& sz);
  static Grid MakeRect(const gfx::Rect& rc);

  // Creates a hexagonal grid. tileCenter() is always a pixel of the
  // mask (the nearest one to the center with a custom mask). "step"
  // is the distance between rows
  // (pointy-top) or columns (flat-top), 0 means DefaultHexStep(), and
  // it's clamped to [MinHexStep(), MaxHexStep()]. If "mask" is
  // nullptr, the mask is generated with MakeHexMask(). A custom mask
  // that doesn't tessellate is replaced by the generated one, and
  // "error" (if it's not nullptr) receives the reason.
  static Grid MakeHex(const gfx::Size& sz,
                      Type type,
                      Parity parity = Parity::Odd,
                      int step = 0,
                      const ImageRef& mask = ImageRef(),
                      TessellationError* error = nullptr);
  static int DefaultHexStep(const gfx::Size& sz, Type type);
  static int MinHexStep(const gfx::Size& sz, Type type);
  static int MaxHexStep(const gfx::Size& sz, Type type);

  // Generates the pixel shape of a hexagon that tessellates without
  // gaps or overlaps. A true hexagon needs an even width (pointy-top)
  // or height (flat-top) so the half-tile stagger is exact; with an
  // odd size the shape falls back to staggered bricks.
  static ImageRef MakeHexMask(const gfx::Size& sz, Type type, int step);

  static bool isHexagonal(Type t) { return (t == Type::HexPointyTop || t == Type::HexFlatTop); }

  bool isEmpty() const { return m_tileSize.w == 0 || m_tileSize.h == 0; }

  gfx::Size tileSize() const { return m_tileSize; }
  Type type() const { return m_type; }
  Parity parity() const { return m_parity; }
  gfx::Point origin() const { return m_origin; }
  gfx::Point tileCenter() const { return m_tileCenter; }
  gfx::Point tileOffset() const { return m_tileOffset; }
  gfx::Point oddRowOffset() const { return m_oddRowOffset; }
  gfx::Point oddColOffset() const { return m_oddColOffset; }

  void tileSize(const gfx::Size& tileSize) { m_tileSize = tileSize; }
  void type(const Type t) { m_type = t; }
  void parity(const Parity p) { m_parity = p; }
  void origin(const gfx::Point& origin) { m_origin = origin; }
  void tileCenter(const gfx::Point& tileCenter) { m_tileCenter = tileCenter; }
  void tileOffset(const gfx::Point& tileOffset) { m_tileOffset = tileOffset; }
  void oddRowOffset(const gfx::Point& oddRowOffset) { m_oddRowOffset = oddRowOffset; }
  void oddColOffset(const gfx::Point& oddColOffset) { m_oddColOffset = oddColOffset; }

  bool isHexagonal() const { return isHexagonal(m_type); }

  // True if the given row (or column) index is shifted by
  // oddRowOffset() (or oddColOffset()), depending on parity().
  bool isStaggered(int i) const;

  // Distance between rows (pointy-top) or columns (flat-top) of a
  // hexagonal grid, 0 for orthogonal grids.
  int hexStep() const;

  // Converts a tile position into a canvas position
  gfx::Point tileToCanvas(const gfx::Point& tile) const;
  gfx::Rect tileToCanvas(const gfx::Rect& tileBounds) const;
  gfx::Region tileToCanvas(const gfx::Region& tileRgn);

  // Converts a canvas position/rectangle into a tile position
  gfx::Point canvasToTile(const gfx::Point& canvasPoint) const;
  gfx::Rect canvasToTile(const gfx::Rect& canvasBounds) const;
  gfx::Region canvasToTile(const gfx::Region& canvasRgn);

  // Returns the tile whose position (tileToCanvas() without the
  // stagger offset) is at or before the given canvas point. It's the
  // inverse of tileToCanvas() for positions of tiles (e.g. a cel
  // position), where canvasToTile() could return a neighbor tile that
  // owns that pixel in a hexagonal grid.
  gfx::Point originToTile(const gfx::Point& canvasPoint) const;

  // Tilemaps keep the stagger of the grid in their first row/column
  // (the tilemap image is re-indexed only by the stagger period).
  // staggerPeriod() is (1,1) for orthogonal grids, (1,2) for
  // pointy-top, and (2,1) for flat-top grids. alignTilesToStagger()
  // expands a rectangle of tiles so its origin is at a multiple of the
  // period, and tilemapPosition() returns the position of a tilemap
  // cel whose first tile is the given (aligned) tile of this grid
  // (it's tileToCanvas() without the stagger offset).
  gfx::Size staggerPeriod() const;
  gfx::Rect alignTilesToStagger(const gfx::Rect& tiles) const;
  gfx::Point tilemapPosition(const gfx::Point& firstTile) const;

  gfx::Size tilemapSizeToCanvas(const gfx::Size& tilemapSize) const;
  gfx::Rect tileBoundsInCanvas(const gfx::Point& tile) const;
  gfx::Rect alignBounds(const gfx::Rect& bounds) const;

  // Returns the canvas pixels owned by the given tile(s)
  gfx::Region tileRegionInCanvas(const gfx::Point& tile) const;
  gfx::Region tilesRegionInCanvas(const std::vector<gfx::Point>& tiles) const;

  bool hasMask() const { return m_mask.get() != nullptr; }
  doc::ImageRef mask() const { return m_mask; }
  void setMask(const doc::ImageRef& mask) { m_mask = mask; }

  // Returns an array of tile positions that are touching the given region in the canvas
  std::vector<gfx::Point> tilesInCanvasRegion(const gfx::Region& rgn) const;

  // Returns true if the tiles of this grid cover each canvas pixel
  // exactly once (simulating 2x2 repetitions of the tiling pattern,
  // which repeats every one or two tiles).
  bool validateTessellation(TessellationError* error = nullptr) const;

  // Tile flags (flips) that can be used with tiles of this grid: X/Y
  // flips of hexagons are valid only if the mask is symmetric on that
  // axis, and diagonal flips are never valid.
  tile_flags validTileFlags() const;

  bool operator==(const Grid& other) const;
  bool operator!=(const Grid& other) const { return !operator==(other); }

private:
  gfx::Size m_tileSize;
  Type m_type = Type::Orthogonal;
  Parity m_parity = Parity::Odd;
  gfx::Point m_origin;
  gfx::Point m_tileCenter;
  gfx::Point m_tileOffset;
  gfx::Point m_oddRowOffset;
  gfx::Point m_oddColOffset;
  doc::ImageRef m_mask;
};

// Returns the 6 tiles adjacent to the given one in a hexagonal grid:
// the two tiles in the same row (pointy-top) or column (flat-top),
// and the two tiles in each of the adjacent staggered rows/columns.
std::array<gfx::Point, 6> hex_neighbors(const gfx::Point& tile, const Grid& grid);

} // namespace doc

#endif
