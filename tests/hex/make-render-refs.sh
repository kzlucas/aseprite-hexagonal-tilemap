#! /bin/bash
# Copyright (C) 2026-present  Igara Studio S.A.
#
# Regenerates hex/render-ref/, the references of ../cli/hex-render.sh,
# with this fork. Only regenerate them after checking that the new
# renders are correct: the render of hexagonal tilemaps is verified
# pixel by pixel in src/render/render_tests.cpp (Render.HexTilemaps).
#
# Usage: ASEPRITE=path/to/fork/aseprite bash tests/hex/make-render-refs.sh

if [[ "$ASEPRITE" == "" ]] ; then
    echo "ASEPRITE env var must point to the executable"
    exit 1
fi

cd "$(dirname "$0")/.."
export ASEPRITE_USER_FOLDER=$(mktemp -d)
source hex/render_outputs.sh
rm -rf hex/render-ref
make_hex_render_outputs "$ASEPRITE" $PWD/hex/render-ref || exit 1
rm -rf $ASEPRITE_USER_FOLDER
echo "Done: $(find hex/render-ref -type f | wc -l) files"
