-- Copyright (C) 2026-present  Igara Studio S.A.
--
-- This file is released under the terms of the MIT license.
-- Read LICENSE.txt for more information.

-- Hexagonal tileset settings used by the round-trip tests: a 16x16
-- pointy-top grid with even parity, step=12, and a custom mask (a
-- sharper top edge and the matching bottom edge).
return {
  version=1,
  type="pointy",
  parity="even",
  step=12,
  tileSize=Size(16, 16),
  mask={
    "......####......",
    "....########....",
    "..############..",
    "################",
    "################",
    "################",
    "################",
    "################",
    "################",
    "################",
    "################",
    "################",
    "..############..",
    "....########....",
    "......####......",
    "................",
  }
}
