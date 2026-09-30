#! /bin/bash
# Copyright (C) 2026-present  Igara Studio S.A.
#
# Regenerates hex/square-ref/ with the upstream version of Aseprite
# this fork is based on (built from the same commit/tag, without the
# hexagonal tilemap changes). These files are the references of
# ../cli/hex-square-regression.sh, so they must be regenerated after
# rebasing the fork on a new upstream version.
#
# Usage: ASEPRITE_UPSTREAM=path/to/upstream/aseprite bash tests/hex/make-square-refs.sh

if [[ "$ASEPRITE_UPSTREAM" == "" ]] ; then
    echo "ASEPRITE_UPSTREAM env var must point to the upstream executable"
    exit 1
fi

cd "$(dirname "$0")/.."
export ASEPRITE_USER_FOLDER=$(mktemp -d)
source hex/square_outputs.sh
rm -rf hex/square-ref
make_square_outputs "$ASEPRITE_UPSTREAM" hex/square-ref || exit 1
rm -rf $ASEPRITE_USER_FOLDER
echo "Done: $(find hex/square-ref -type f | wc -l) files"
