#! /bin/bash
# Copyright (C) 2026-present  Igara Studio S.A.
#
# Renders and exports hexagonal tilemaps (hex/render_outputs.sh) and
# compares the results with hex/render-ref: .aseprite and .json files
# byte by byte, images by their content.

source hex/render_outputs.sh

d=$t/hex-render
make_hex_render_outputs $ASEPRITE $d > $t/hex-render.log 2>&1 || \
    fail "make_hex_render_outputs failed (see $t/hex-render.log)"

ref=hex/render-ref
(cd $ref && find . -type f | sort) > $t/hex-render-ref.list
(cd $d && find . -type f | sort) > $t/hex-render.list
diff -u $t/hex-render-ref.list $t/hex-render.list || fail "different list of output files"

rm -f $t/hex-render-pairs.txt
for f in $(cat $t/hex-render-ref.list) ; do
    if [[ $f == *.png || $f == *.gif ]] ; then
        echo "$ref/$f $d/$f" >> $t/hex-render-pairs.txt
    elif ! cmp -s $ref/$f $d/$f ; then
        fail "$f is different from the reference"
    fi
done
$ASEPRITE -b --script-param list=$t/hex-render-pairs.txt --script hex/compare_png.lua || \
    fail "images rendered differently than the reference"
