#!/usr/bin/env bash
# Builds and runs the pure placement-planner tests WITHOUT Unreal Engine (uses a tiny UE-type shim).
# This validates VoidPlacementPlanner.cpp only -- not the UObject/actor/instancing layer, which needs the
# in-editor automation tests (VOID.WorldBuilder.Environment.*).
set -euo pipefail
cd "$(dirname "$0")"
GEN=../../Source/VOIDWorldBuilderGenerators
g++ -std=c++17 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function \
  -Ishim -I"$GEN/Public" PlannerTests.cpp "$GEN/Private/Environment/VoidPlacementPlanner.cpp" -o /tmp/void_planner_tests
/tmp/void_planner_tests
