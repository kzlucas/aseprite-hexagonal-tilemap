#! /bin/bash
# Copyright (C) 2026-present  Igara Studio S.A.
#
# Square tilemaps must be saved, rendered and exported exactly like
# the upstream version this fork is based on. Compares the outputs of
# make_square_outputs (hex/square_outputs.sh) with the ones created by
# the upstream version in hex/square-ref (see hex/make-square-refs.sh):
# .aseprite and .json files byte by byte, images by their content.

source hex/square_outputs.sh

d=$t/hex-square
make_square_outputs $ASEPRITE $d > $t/hex-square.log 2>&1 || \
    fail "make_square_outputs failed (see $t/hex-square.log)"

ref=hex/square-ref
(cd $ref && find . -type f | sort) > $t/hex-square-ref.list
(cd $d && find . -type f | sort) > $t/hex-square.list
diff -u $t/hex-square-ref.list $t/hex-square.list || fail "different list of output files"

rm -f $t/hex-square-pairs.txt
for f in $(cat $t/hex-square-ref.list) ; do
    if [[ $f == *.png ]] ; then
        echo "$ref/$f $d/$f" >> $t/hex-square-pairs.txt
    elif ! cmp -s $ref/$f $d/$f ; then
        fail "$f is different from the upstream version"
    fi
done
$ASEPRITE -b --script-param list=$t/hex-square-pairs.txt --script hex/compare_png.lua || \
    fail "images rendered differently than the upstream version"
