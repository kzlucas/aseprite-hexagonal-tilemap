#! /bin/bash
# Copyright (C) 2026-present  Igara Studio S.A.
#
# Defines make_square_outputs, which creates files with square
# tilemaps using the given Aseprite executable: generated sprites,
# re-saved sprites, rendered frames and sprite sheets. Used to check
# that square tilemaps are saved, rendered and exported exactly like
# the upstream version (see ../cli/hex-square-regression.sh and
# make-square-refs.sh).
#
# Usage (from the tests/ directory):
#   source hex/square_outputs.sh
#   make_square_outputs /path/to/aseprite output_dir

function make_square_outputs() {
    local exe=$1
    local d=$2
    mkdir -p $d/gen $d/resave $d/png $d/sheet || return 1

    for mode in rgb indexed ; do
        $exe -b --script-param out=$d/gen/$mode.aseprite \
             --script-param mode=$mode \
             --script hex/make_square_tilemap.lua || return 1
    done

    local f n
    for f in sprites/*.aseprite $d/gen/*.aseprite ; do
        n=$(basename $f .aseprite)
        $exe -b $f --save-as $d/resave/$n.aseprite || return 1
        $exe -b $f --save-as "$d/png/${n}_{frame}.png" || return 1
        $exe -b $f --sheet $d/sheet/$n.png --data $d/sheet/$n.json \
             --format json-array --list-layers --list-tags --list-slices || return 1
        # The version of the program is not part of the comparison
        sed -i -e '/"version":/d' $d/sheet/$n.json || return 1
    done
    return 0
}
