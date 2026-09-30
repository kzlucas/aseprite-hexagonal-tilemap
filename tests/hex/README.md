# Hexagonal tilemap tests

Tests of the hexagonal tilemaps of the `aseprite-custom` fork that
need files or other executables. The C++ unit tests live next to the
code (`src/doc/grid_tests.cpp`, `src/doc/grid_properties_tests.cpp`,
`src/app/file/hex_file_tests.cpp`).

## Square tilemaps don't change

`../cli/hex-square-regression.sh` runs with the regular test suite
(`run-tests.sh`). It creates square tilemaps (`make_square_tilemap.lua`),
re-saves every sprite in `../sprites/`, and renders and exports them
(`square_outputs.sh`). It then compares the results with
`square-ref/`: `.aseprite` and `.json` files byte by byte, and images
by their pixels (`compare_png.lua`).

`square-ref/` was created by the upstream version this fork is based
on, built with the same configuration as the fork. After rebasing on a
new upstream version, build that upstream version and regenerate the
references:

    ASEPRITE_UPSTREAM=/path/to/upstream/aseprite bash make-square-refs.sh

## Round-trip through official Aseprite

`upstream-roundtrip.sh` needs an upstream executable. It checks that
official Aseprite keeps tileset extension properties
(`ext_props_write.lua`, `ext_props_check.lua`). It also checks that a
hexagonal tileset saved by the fork (`hex_make.lua`, `hex_props.lua`)
keeps its settings after being opened and saved in upstream
(`hex_props_check.lua`), and that the fork loads it again as
hexagonal (`hex_fork_check.lua`) and saves the same bytes.

    ASEPRITE=/path/to/fork/aseprite \
    ASEPRITE_UPSTREAM=/path/to/upstream/aseprite \
    bash upstream-roundtrip.sh

To build the upstream executable (a CLI-only build is enough for this
test), export the base commit and build it as in `INSTALL.md`:

    git archive <base-commit> | tar -x -C /tmp/upstream-src
    # copy the submodules (laf, third_party/*, src/*) from this checkout
    cmake -G Ninja -DLAF_BACKEND=none -DCMAKE_BUILD_TYPE=RelWithDebInfo /tmp/upstream-src
    ninja aseprite
