#!/bin/sh
# Standalone (no Unreal) check of the pure-logic district code: layout planner + validator + geometry,
# run against the real Meridian registries (real_data.inc is generated from the delivered Meridian ZIP).
# inc/ is a tiny stand-in for the parts of UE Core the planner uses. It is a test harness only, NOT part of the plugin.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$HERE/../../Source"
G="$SRC/VOIDWorldBuilderGenerators/Private/District"
g++ -std=c++17 -O1 -I"$HERE/inc" -I"$SRC/VOIDWorldBuilderCore/Public" -I"$SRC/VOIDWorldBuilderImport/Public" \
    -I"$SRC/VOIDWorldBuilderGenerators/Public" -I"$HERE" \
    "$HERE/test_main.cpp" "$G/VoidDistrictGeometry.cpp" "$G/VoidDistrictLayoutBuilder.cpp" "$G/VoidDistrictValidator.cpp" \
    -o "$HERE/districttest"
"$HERE/districttest"
