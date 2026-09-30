// Aseprite
// Copyright (C) 2019-2020  Igara Studio S.A.
// Copyright (C) 2001-2016  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "app/snap_to_grid.h"

#include "doc/grid.h"
#include "gfx/point.h"
#include "gfx/rect.h"

#include <climits>
#include <cmath>
#include <cstdlib>

namespace app {

gfx::Point snap_to_grid(const gfx::Rect& grid, const gfx::Point& point, const PreferSnapTo prefer)
{
  if (grid.isEmpty())
    return point;

  div_t d, dx, dy;
  dx = std::div(grid.x, grid.w);
  dy = std::div(grid.y, grid.h);

  gfx::Point newPoint(point.x - dx.rem, point.y - dy.rem);
  if (prefer != PreferSnapTo::ClosestGridVertex) {
    if (newPoint.x < 0)
      newPoint.x -= grid.w;
    if (newPoint.y < 0)
      newPoint.y -= grid.h;
  }

  switch (prefer) {
    case PreferSnapTo::ClosestGridVertex:
      d = std::div(newPoint.x, grid.w);
      newPoint.x = dx.rem + d.quot * grid.w + ((d.rem > grid.w / 2) ? grid.w : 0);

      d = std::div(newPoint.y, grid.h);
      newPoint.y = dy.rem + d.quot * grid.h + ((d.rem > grid.h / 2) ? grid.h : 0);
      break;

    case PreferSnapTo::BoxOrigin:
    case PreferSnapTo::FloorGrid:
      d = std::div(newPoint.x, grid.w);
      newPoint.x = dx.rem + d.quot * grid.w;

      d = std::div(newPoint.y, grid.h);
      newPoint.y = dy.rem + d.quot * grid.h;
      break;

    case PreferSnapTo::CeilGrid:
      d = std::div(newPoint.x, grid.w);
      newPoint.x = d.rem ? dx.rem + (d.quot + 1) * grid.w : newPoint.x;

      d = std::div(newPoint.y, grid.h);
      newPoint.y = d.rem ? dy.rem + (d.quot + 1) * grid.h : newPoint.y;
      break;

    case PreferSnapTo::BoxEnd:
      d = std::div(newPoint.x, grid.w);
      newPoint.x = dx.rem + (d.quot + 1) * grid.w;

      d = std::div(newPoint.y, grid.h);
      newPoint.y = dy.rem + (d.quot + 1) * grid.h;
      break;
  }

  return newPoint;
}

gfx::Point snap_to_hex_tile_position(const doc::Grid& grid, const gfx::Point& point)
{
  // The closest tile position is one of the positions of the tiles
  // around the tile that starts at or before the point
  const gfx::Point tile0 = grid.originToTile(point);
  gfx::Point result = point;
  int bestDist = INT_MAX;
  for (int dv = -1; dv <= 1; ++dv) {
    for (int du = -1; du <= 1; ++du) {
      const gfx::Point pos = grid.tileToCanvas(tile0 + gfx::Point(du, dv));
      const int dist = (pos.x - point.x) * (pos.x - point.x) +
                       (pos.y - point.y) * (pos.y - point.y);
      if (dist < bestDist) {
        bestDist = dist;
        result = pos;
      }
    }
  }
  return result;
}

gfx::Point snap_to_hex_stagger_period(const doc::Grid& grid, const gfx::Point& delta)
{
  const gfx::Size period = grid.staggerPeriod();
  const int px = period.w * grid.tileOffset().x;
  const int py = period.h * grid.tileOffset().y;
  return gfx::Point(int(std::round(double(delta.x) / px)) * px,
                    int(std::round(double(delta.y) / py)) * py);
}

} // namespace app
