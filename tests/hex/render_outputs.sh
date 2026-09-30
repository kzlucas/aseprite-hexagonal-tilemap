#! /bin/bash
# Copyright (C) 2026-present  Igara Studio S.A.
#
# Defines make_hex_render_outputs, which creates sprites with
# hexagonal tilemaps (make_hex_sprites.lua) and renders and exports
# them with the given Aseprite executable (re-saved .aseprite, PNG at
# 1x and 3x, animated GIF, and sprite sheet + JSON). Used by
# ../cli/hex-render.sh and make-render-refs.sh.
#
# Usage (from the tests/ directory):
#   source hex/render_outputs.sh
#   make_hex_render_outputs /path/to/aseprite output_dir

function make_hex_render_outputs() {
    local exe=$1
    local d=$2
    local src=$d.src
    mkdir -p $src $d/resave $d/png $d/sheet || return 1

    (cd hex && $exe -b --script-param dir=$src --script make_hex_sprites.lua) || return 1

    local f n
    for f in $src/*.aseprite ; do
        n=$(basename $f .aseprite)
        $exe -b $f --save-as $d/resave/$n.aseprite || return 1
        $exe -b $f --save-as $d/png/$n.png || return 1
        $exe -b $f --scale 3 --save-as $d/png/$n-x3.png || return 1
        $exe -b $f --save-as $d/png/$n.gif || return 1
        $exe -b $f --sheet $d/sheet/$n.png --data $d/sheet/$n.json \
             --format json-array --list-layers || return 1
        sed -i -e '/"version":/d' $d/sheet/$n.json || return 1
    done
    rm -rf $src
    return 0
}
