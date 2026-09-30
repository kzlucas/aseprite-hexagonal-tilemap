#! /bin/bash
# Copyright (C) 2026-present  Igara Studio S.A.
#
# Checks that the hexagonal tileset settings survive being opened and
# saved in a version of Aseprite without hexagonal tilesets.
#
# Usage (from any directory):
#   ASEPRITE=path/to/this/fork/aseprite \
#   ASEPRITE_UPSTREAM=path/to/upstream/aseprite \
#   bash tests/hex/upstream-roundtrip.sh

if [[ "$ASEPRITE" == "" || "$ASEPRITE_UPSTREAM" == "" ]] ; then
    echo "ASEPRITE and ASEPRITE_UPSTREAM env vars must point to the executables"
    exit 1
fi

cd "$(dirname "$0")"
t=$(mktemp -d)
export ASEPRITE_USER_FOLDER=$t/user

function fail() {
    echo "FAILED: $*"
    exit 1
}

# 0) Any version of Aseprite keeps extension properties of tilesets
$ASEPRITE_UPSTREAM -b --script-param out=$t/claim.aseprite --script ext_props_write.lua || fail "ext_props_write"
$ASEPRITE_UPSTREAM -b $t/claim.aseprite --save-as $t/claim2.aseprite || fail "upstream re-save"
$ASEPRITE_UPSTREAM -b --script-param in=$t/claim2.aseprite --script ext_props_check.lua || fail "ext_props_check"

# 1) Write the hexagonal properties, load the file in the fork (it
#    becomes a hexagonal tileset) and save it (B)
$ASEPRITE -b --script-param out=$t/a.aseprite --script hex_make.lua || fail "hex_make"
$ASEPRITE -b --script-param in=$t/a.aseprite --script hex_fork_check.lua || fail "fork check (a)"
$ASEPRITE -b $t/a.aseprite --save-as $t/b.aseprite || fail "fork save (b)"

# 2) Open and save B in the upstream version (C)
$ASEPRITE_UPSTREAM -b --script-param in=$t/b.aseprite --script hex_props_check.lua || fail "upstream check (b)"
$ASEPRITE_UPSTREAM -b $t/b.aseprite --save-as $t/c.aseprite || fail "upstream save (c)"
$ASEPRITE_UPSTREAM -b --script-param in=$t/c.aseprite --script hex_props_check.lua || fail "upstream check (c)"

# 3) Open C in the fork: still hexagonal, and saved exactly like B
$ASEPRITE -b --script-param in=$t/c.aseprite --script hex_fork_check.lua || fail "fork check (c)"
$ASEPRITE -b $t/c.aseprite --save-as $t/d.aseprite || fail "fork save (d)"
cmp $t/b.aseprite $t/d.aseprite || fail "the file changed after a round-trip through upstream"

rm -rf $t
echo "Done"
