# HANDOFF — Agent 6: Environment + Props (Phases 12 and 14)

Target: Unreal Engine 5.8.2. Base: `Plugins phase 3/VOIDWorldBuilder` from `The-Voids-Calamity-main.zip` (the newest of the three phase folders; it contains Core, Import, Generators with **Road only**, Editor).

## READ THIS FIRST — verification status

| Verified | How |
|---|---|
| Placement planner logic (`VoidPlacementPlanner.cpp`): determinism, stability under additions, context rules, junction clearance, building exclusion, park/plaza/site density, commercial adjacency, facade, clusters, density monotonicity/subset, budget, bounds, category filter, polish response, cinematic priority, traffic-light counts | 33 checks, compiled with g++ against a small UE-type shim: `Tools/StandalonePlannerTests/run.sh` — **ALL PASSED** |
| Planner throughput | 120 roads + 901 areas → 24,303 placements in ~240 ms (shim `FName` = `std::string`, so real UE should be faster) |

| **NOT verified** | Why |
|---|---|
| **Compilation in UE 5.8.2**, module/plugin loading, actor spawning, (H)ISM creation, the editor panel button, the 8 automation tests | No Unreal Engine (or UBT) exists in my environment. Everything outside `VoidPlacementPlanner.cpp` is **unbuilt**. Expect a small number of compile fixes. |
| PCG | Not compiled, not tested, not depended on (see below). |

The automation tests in `Private/Tests/VoidEnvironmentPropsTests.cpp` are written and ready, but have never been run. Please treat step 1 of "Suggested first actions" as the real verification.

## Audit findings that shape this delivery

1. **Buildings and Districts generators do not exist** in the supplied Builder. Only Road (Phase 3) is implemented. Building data exists only as `FVoidBuildingSpec` (footprint corners, height, free-text `BuildingType`). The Environment generator therefore reads footprints straight from the package (as obstacles, Park/Plaza areas, Commercial/Construction use) and does not depend on any building actors. When a Building generator lands it needs no change here.
2. **Meridian data contains no environment or prop asset lists, and no geometry.** `DataLayers.json` defines `environment_layer` only as "materials, color language, and environmental storytelling assets per district"; `RoadNetwork.json` is topology without coordinates; the design PDFs mention billboards/signs/benches/a tree only as narrative beats. The only usable environment signal is the polish gradient in `Meridian_Master_Plan.md` (Undercroft → Metro Archives → White Zones → Olympus Spire; Sector 0 the dynamic exception). So: **all categories, rules, densities and district polish values are engineering placeholders, not approved design values.** Nothing was invented as Meridian canon.
3. The Meridian JSON and the Builder's `FVoidDesignPackage` JSON are **different formats**; no importer bridges them in the supplied Builder. This work consumes `FVoidDesignPackage` like the Road generator.
4. The plugin declared `EngineVersion 5.6.0` and used no PCG. Bumped to 5.8.0; PCG stays optional. Phases 1–3 code was **not** re-audited for 5.6→5.8 API changes (see limitations).
5. `BuilderRules.json` micro-pipeline places environment generation at step 8 (after roads, buildings, districts, metro, infrastructure). This generator can run any time after import; run it after roads/buildings for the intended order.

## Files added (all under `VOIDWorldBuilder/`)

`Source/VOIDWorldBuilderGenerators/Public/Environment/`
- `VoidPlacementPlanner.h` — pure planner API (namespace `VoidPlan`)
- `VoidEnvironmentTypes.h` — category ids, `FVoidAssetSlotRow`, `FVoidPlacementRuleRow`, `FVoidDistrictEnvProfileRow`, `FVoidBuildingUseTokenRow`, `FVoidDressingBucketInfo`
- `VoidEnvironmentSettings.h` — `UVoidEnvironmentSettings`
- `VoidDressingActor.h` — `AVoidDressingActor`
- `VoidDressingDefaults.h` — placeholders, built-in rules/profiles/tokens
- `VoidDressingGenerator.h` — `FVoidDressingGeneratorBase`, `FVoidEnvironmentGenerator`, `FVoidPropGenerator`, options/result structs

`Source/VOIDWorldBuilderGenerators/Private/Environment/` — matching `.cpp` files
`Source/VOIDWorldBuilderGenerators/Private/Tests/VoidEnvironmentPropsTests.cpp`
`Docs/EnvironmentPropsArchitecture.md`, `Docs/Samples/AssetSlots.sample.csv`, `Docs/Samples/DistrictProfiles.sample.csv` (sample CSVs are untested imports; verify the map-column syntax in your editor)
`Tools/StandalonePlannerTests/` (shim, tests, `run.sh`)

## Files changed

- `VOIDWorldBuilderGenerators/Private/VOIDWorldBuilderGeneratorsModule.cpp` — registers/unregisters `Environment` and `Props`.
- `VOIDWorldBuilderGenerators.Build.cs` — adds `DeveloperSettings`, `Json` (private).
- `VOIDWorldBuilder.uplugin` — `EngineVersion` 5.8.0, description.
- `VOIDWorldBuilderEditor/Public/SVoidWorldBuilderPanel.h`, `Private/SVoidWorldBuilderPanel.cpp` — **Generate Environment + Props** button (via registry, with cancel).
- `Docs/BuildInstructions.md`, `README.md` — notes. No Road, Import or Core code changed.

## Environment API

```cpp
auto Gen = FVoidGeneratorRegistry::Get().FindGenerator(TEXT("Environment")); // IVoidGenerator
Gen->Generate(Package, Context);                      // standard contract

FVoidEnvironmentGenerator G;                          // or use options + result
FVoidDressingOptions Opt;  // DensityScaleOverride, MaxInstancesOverride, SeedOverride,
                           // CategoryAllowList, bUseBounds/BoundsMin/Max, bDryRun, bForceExportJson
FVoidDressingRunResult R;  // PlanStats (per-reason rejections, per-category counts), actors/components/instances, ms
G.GenerateWithOptions(Package, Context, Opt, &R);
G.ClearGenerated(World, DistrictId);                  // NAME_None = all districts
FVoidDressingGeneratorBase::OnInstancesPlanned();     // delegate(Domain, TArray<VoidPlan::FInstance>)
```
Output: `AVoidDressingActor` (per generator/district/cell) with `Buckets[]`, `FindBucketComponent`, `SetBucketMesh`.
Built-in categories: Tree, Grass, Bush, Rock, Planter, Bench, Streetlight, Pole, TrafficLight, SidewalkDetail, StreetProp (+ shared Sign, Barrier, Bin, Trash).

## Props API

Same shape; id `Props`, class `FVoidPropGenerator`. Categories: Vehicle, Container, Crate, Pipe, Billboard, ConstructionAsset, plus Sign/Barrier/Bin/Trash rules in the Props domain. A rule's `Domain` field (`Environment`/`Props`) decides which generator runs it; categories are `FName`s, so new ones are data only (add a rule row + slot row).

## Asset requirements

**None.** Every category has a `/Engine/BasicShapes` composite placeholder (1–2 parts, real-world scale, ground-contact pivot). To replace: add `FVoidAssetSlotRow` (same `Category`, a `Mesh`, optional materials/offset/weight/cull/`ReplacementTag`) to `AssetSlotTable` in Project Settings. Vehicles/streetlights/trees swap independently. Recommended real-asset conventions: pivot at ground contact; +X forward (vehicles/benches face +X = along traffic / toward road); meters→cm scale.

## PCG integration

Deliberately **no PCG module dependency**. Integration is a data seam: `OnInstancesPlanned()` and `bExportPlacementJson` (`Saved/VOIDWorldBuilder/Placements/<district>_<domain>.json`, schema `void_placement_points_v1`). A PCG graph or project module can consume the same points, and a category can be zeroed here (district `CategoryMultipliers` or `CategoryAllowList`) so a graph owns it. Not compiled or exercised against 5.8's PCG.

## Performance decisions

One actor per cell (default 25,600 uu), one HISM/ISM per Category/Slot/Part, static mobility, no collision/overlap/nav, no Tick, shared BasicShapes meshes, per-category cull distances, hash-based slot variation (no per-instance components), importance-weighted density + hard budget (`MaxInstancesPerRun`, default 200,000) dropping lowest priority first, planning is pure/allocation-light and cancelable before any world mutation.

## Integration dependencies

Reads: `FVoidDesignPackage` (roads incl. `bHasSidewalk/bHasMedian/bIsBridge/bIsTunnel/RoadType/WidthUnits`, buildings incl. `BuildingType/HeightUnits/FootprintCorners`), Road builders/profile table/road settings (`FVoidRoadSplineBuilder`, `FVoidRoadBridgeTunnelBuilder`, `FVoidRoadIntersectionBuilder`, `FVoidRoadTypeProfileLibrary`, `UVoidRoadGenerationSettings`), `IVoidGenerator`/registry, `FVoidGenerationContext`. Modules added: `DeveloperSettings`, `Json`. Roads must exist in the *package*, not necessarily as spawned actors.

## Known limitations

1. **Unbuilt in UE** (above). Highest-risk spots: `AddInstances` / `SetCullDistances` signatures in 5.8, `TArray::Sort` lambdas on `TPair`, `UDataTable::GetRowMap`, `FTableRowBase` row structs containing `TMap`, test automation flags. Phases 1–3 were written for 5.6; I did not audit them for 5.8 deprecations.
2. All rule/density/polish numbers are placeholders (see Audit #2). Polish values for the four Meridian districts follow only the *order* in `Meridian_Master_Plan.md`; Sector 0 is neutral.
3. Building `Use` comes from substring tokens on free-text `BuildingType`; unmatched buildings get no commercial/construction dressing.
4. Placement uses 2D footprints; no terrain/ground snapping (Z from road elevation / building height). Bridge decks: props are skipped only where a rule sets `bSkipOnBridge`; no bridge-railing logic.
5. Roundabouts are treated as closed loops; no island planting/props. No crosswalk/parking-lane awareness; parked vehicles are lane-edge placements.
6. `InsideBuilding` is O(areas) per candidate with a bounds early-out — fine for hundreds of buildings, should get a grid for tens of thousands. Planning runs on the game thread.
7. Regeneration removes the whole (generator, district) output and rebuilds; there is no incremental per-cell update, and user hand-edits inside `AVoidDressingActor`s are lost on regenerate.
8. Cancel during *spawn* leaves a partial generation (re-run completes it); cancel during *planning* leaves the old output intact.
9. Traffic-light "AlongTraffic" yaw faces oncoming traffic; whether that matches the final asset's forward axis is unverified.
10. No World Partition/Data Layer assignment; cell actors are per-cell so a later pass can assign them.
11. Nanite/Lumen, wind, LODs, and materials are asset concerns and not touched.

## Suggested first actions

1. Build 5.8.2 Development Editor; fix any compile errors; run `VOID.WorldBuilder.Environment` tests.
2. Import a package with roads + a few `BuildingType` values (`Commercial_*`, `Park_*`, `Construction_*`), click **Generate Environment + Props**, inspect the log summary.
3. Re-run to confirm no duplicates; toggle `DensityScale` and `GlobalSeed`.
4. Author the first `AssetSlotTable` rows for Tree/Streetlight/Vehicle.
