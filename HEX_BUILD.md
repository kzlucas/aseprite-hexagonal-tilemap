# Hexagonal tilemaps: build, tests and maintenance

This fork adds hexagonal tilemaps to Aseprite (branch `hex-tilemap`, based on upstream `e5ed241eb`). This file explains how to build it, run the tests, share the changes, and rebase them on a newer upstream version.

## Building

The build is the same as upstream (see `INSTALL.md`). This workspace uses a prebuilt Skia (`aseprite-m124`) in `../deps/skia-m124`:

```bash
cd aseprite && mkdir -p build && cd build
CC=clang CXX=clang++ cmake -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_CXX_FLAGS:STRING=-stdlib=libstdc++ \
  -DCMAKE_EXE_LINKER_FLAGS:STRING=-stdlib=libstdc++ \
  -DLAF_BACKEND=skia \
  -DSKIA_DIR=/path/to/deps/skia-m124 \
  -DSKIA_LIBRARY_DIR=/path/to/deps/skia-m124/out/Release-x64 \
  -DSKIA_LIBRARY=/path/to/deps/skia-m124/out/Release-x64/libskia.a \
  -DENABLE_TESTS=ON -DLAF_WITH_TESTS=ON \
  ..
ninja
./bin/aseprite
```

Two things to watch:
- `ninja <library or test target>` doesn't relink `bin/aseprite`. Run a full `ninja` before running the Lua/CLI tests, or they will use an old executable.
- New data or widget files are found when CMake configures. After adding one, run `cmake ..` again.

## Tests

| Suite | Command | What it covers |
|---|---|---|
| C++ unit tests | `cd build && ctest -j1` | `grid_tests`, `grid_properties_tests`, `hex_tiles_tests`, `hex_file_tests`, `render_tests` (hex render), `fill_selection_tests`, and all the upstream tests |
| Lua and CLI tests | `cd tests && ASEPRITE=$PWD/../build/bin/aseprite bash run-tests.sh` | `scripts/hex_*.lua` (tiles mode, pixels mode, canvas flip, Lua API, parity), `cli/hex-render.sh`, `cli/hex-square-regression.sh`, and all the upstream tests |
| Round trip through official Aseprite | `ASEPRITE=... ASEPRITE_UPSTREAM=... bash tests/hex/upstream-roundtrip.sh` | Hex settings survive being opened and saved by the upstream version |
| Manual GUI checks | `HEX_MANUAL_TESTS.md` | Editor interactions |

Use `ctest -j1`: `app_tests` and `keyboard_shortcuts_tests` crash about 20% of the time under parallel ctest, in upstream too (`HEX_BASELINE.md`).

### Reference files

- `tests/hex/square-ref/` holds files created by the **upstream** version (built with the same configuration). `hex-square-regression.sh` checks that square tilemaps are saved, rendered and exported byte by byte like upstream. Regenerate these only with an upstream build: `ASEPRITE_UPSTREAM=... bash tests/hex/make-square-refs.sh`.
- `tests/hex/render-ref/` holds hexagonal sprites rendered and exported by **this fork**. Regenerate them (`ASEPRITE=... bash tests/hex/make-render-refs.sh`) only after checking that the new output is right. `render_tests` checks the render pixel by pixel.
- The `render-ref/resave/*.aseprite` files are also the test files of `HEX_MANUAL_TESTS.md`. Don't save them when testing: the render test would then fail. Copy them first.

### Building the upstream reference executable

```bash
git archive e5ed241eb | tar -x -C /tmp/upstream-src
# copy the submodules at the same commits
git submodule foreach --recursive --quiet 'echo $displaypath' | while read p; do
  mkdir -p /tmp/upstream-src/$p
  (cd $p && git archive HEAD | tar -x -C /tmp/upstream-src/$p)
done
mkdir /tmp/upstream-build && cd /tmp/upstream-build
cmake -G Ninja <the same options as the fork, with ENABLE_TESTS=OFF> /tmp/upstream-src
ninja aseprite
```

A CLI-only build (`-DLAF_BACKEND=none`) is enough for the round-trip test. The square references need the same Skia configuration as the fork, because the compressed PNG data differs between configurations.

## Formatting

The code follows `.clang-format` (clang-format **19**). With `pip install clang-format==19.1.5`, format only the changed lines:

```bash
git add -u && git-clang-format --binary /path/to/clang-format HEAD
```

## Sharing the changes

To share the work without binaries, export the branch as patches:

```bash
git format-patch e5ed241eb..hex-tilemap -o hex-tilemap-patches/
```

To apply them on a checkout of the same upstream version: `git am hex-tilemap-patches/*.patch`.

## Rebasing on a newer upstream version

```bash
git fetch origin
git rebase origin/main hex-tilemap
```

The branch modifies 73 upstream files. Changes to square tilemaps are guarded by `isHexagonal()` checks, so most conflicts are local. The files most likely to conflict:

| Area | Files | Notes |
|---|---|---|
| Grid model | `src/doc/grid.h/.cpp` | `Grid::Type`, `Parity`, hex geometry, `originToTile`, `alignTilesToStagger`, `tilemapPosition`. See the `isometric-grid` note below. |
| File format | `src/dio/aseprite_encoder.cpp`, `aseprite_decoder.cpp`, `src/doc/tileset_io.cpp` | Hex settings in tileset user-data properties (D5). |
| Render | `src/render/render.cpp`, `src/app/render/shader_renderer.cpp` | `renderHexTilemap`, `compose_hex_tilemap`. |
| Editor | `src/app/ui/editor/editor.cpp`, `pixels_movement.cpp`, `brush_preview.cpp`, `tool_loop_impl.cpp`, `moving_*_state.cpp`, `transform_handles.cpp` | Hex grid drawing, moving tiles, previews, snapping. |
| Tools | `src/app/tools/point_shapes.h`, `inks.h`, `ink_processing.h`, `controllers.h`, `intertwiners.h`, `tool_loop_manager.cpp` | Tile under a point, selections, flood fill, pixel-perfect preview. |
| Tilemap edits | `src/app/util/cel_ops.cpp`, `expand_cel_canvas.cpp`, `new_image_from_mask.cpp`, `clipboard.cpp`, `src/app/cmd/*_cel.cpp`, `clear_*.cpp`, `src/app/doc_api.cpp` | Stagger alignment of tilemaps. |
| UI | `src/app/ui/tileset_selector.cpp`, `data/widgets/tileset_selector.xml`, `data/strings/en.ini` | Hex options of the tileset dialog, new strings. |
| Scripting | `src/app/script/grid_class.cpp`, `tileset_class.cpp`, `sprite_class.cpp`, `api_version.h` | Hex API. The fork sets `API_VERSION` to 42. If upstream also reaches 42 for other features, bump the fork's version above upstream's, and keep documenting `GridShape ~= nil` as the feature check. |
| Export | `src/app/doc_exporter.cpp` | `meta.tilesets` (D8). |

After rebasing:

1. Build upstream's new version with the same configuration, and regenerate `tests/hex/square-ref/` with it (`make-square-refs.sh`). Square tilemaps must stay byte-identical to the new upstream.
2. Run all the test suites and the round trip with the new upstream executable.
3. If upstream changed the tilemap code, check `HEX_INVENTORY.md` (section C) for the call sites that assume rectangular tiles. Any new upstream code that converts between canvas and tile coordinates needs the hex idioms:
   - `originToTile` (the tile that starts at a position);
   - `alignTilesToStagger` + `tilemapPosition` (tilemaps that keep the stagger);
   - the center rule for selections.

### Upstream `isometric-grid` branch

Upstream has an unmerged branch (`isometric-grid`, head `c41682a09`, unchanged since Phase 1) that adds isometric sprite grids. It uses the same `Grid::Type` enum as this fork, with other values. If it's merged:

- Merge the enum values (`Orthogonal`, `IsometricSharedEdges`, `IsometricThickEdges`, `HexPointyTop`, `HexFlatTop`).
- In that branch, `Site::grid()` calls `grid.type(gridType())` for tilemap layers too. That overwrites the tileset's hex type with the sprite grid type, so it must skip tilemap layers.
- Its `snap_to_grid(const doc::Grid&, ...)` replaces the `gfx::Rect` version. Merge it with this fork's `snap_to_hex_tile_position()` and `snap_to_hex_stagger_period()`.
