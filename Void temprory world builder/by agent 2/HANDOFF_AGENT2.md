# HANDOFF — Agent 2: Metro Generator + World Partition Support

Target: Unreal Engine 5.8.2 · Base: supplied Builder, **Phase 3** snapshot (`Plugins phase 3/VOIDWorldBuilder`) · Plugin folder in this ZIP: `VOIDWorldBuilder/`

## 0. Read this first — verification status (honest)

| Item | Status |
|---|---|
| Compiles in UE 5.8.2 | **NOT VERIFIED.** No Unreal Engine was available. Nothing was compiled. |
| UHT (`GENERATED_BODY`, `UPROPERTY`) | **NOT VERIFIED.** Statically checked only: every header with `GENERATED_BODY` includes its matching `.generated.h` as the last include. |
| Plugin loads / generator registers | **NOT VERIFIED.** Registration code mirrors the Road generator exactly (`RegisterGenerator` / `UnregisterGenerator` in the Generators module). |
| Metro geometry / actor placement in-editor | **NOT VERIFIED.** Never rendered. Winding, materials and visual proportions are unconfirmed. |
| World Partition behaviour | **NOT VERIFIED.** Never run against a partitioned map. |
| Automation tests | **Written, not run.** 11 tests, filter `VOID.WorldBuilder.Metro` / `VOID.WorldBuilder.WorldPartition` (see §10). |
| Brace/paren balance, include resolution, `.generated.h` ordering (28 files) | **Checked** with a script: 0 problems. |
| Mapper/validator assumptions vs the real Meridian JSON | **Checked** with a script against the shipped `MetroNetwork.json` / `RoadNetwork.json`: all field names resolve; counts (2 networks, 3 lines, 5 stations, 1 interchange, 5 district links, 2 dead tunnels) match; real data passes every validator rule. This is a Python port of the extraction rules, not the C++ itself. |
| UE 5.8.2 API compatibility | I confirmed only that UE 5.8 (and a 5.8.1 hotfix) exist. I could not confirm any 5.8.2 API detail. Treat every engine API below as *assumed*; §9 lists exactly which ones to check first. |

## 1. The finding that shapes everything

**Meridian's metro data contains no coordinates, no track geometry, no platforms, no entrances, and no elevated/underground sections.**
`MetroNetwork.json` is topology only (2 networks; 3 lines; 5 stations; 1 interchange; district links). `Meridian_Master.json` states
`coordinate_policy: no_fabricated_coordinates_no_fabricated_transforms_no_fabricated_mesh_ids`, and `RoadNetwork.json` says the Builder
resolves geometry at generation time. Your instruction says "do not fabricate Meridian transit data". Those two requirements only reconcile if
data and layout are kept apart, so:

* **Authored data wins and is never touched.** An optional `metro` block supplies real positions/grades/entrances/centerlines.
* **Where absent, a deterministic *placeholder layout* is derived and labelled everywhere** (`bPlaceholder`, `bIsPlaceholderLayout` on the actor,
  warning `VOID.Metro.PlaceholderLayout`, panel log line). Only band **names** (from `DistrictRegistry.json`) and the Spire-at-origin rule are canon;
  **all radii and azimuths are placeholders** in Project Settings.
* **Switch it off** (`bAllowPlaceholderLayout = false`) and unplaceable items are skipped with warnings — nothing is guessed silently.

Same for grade: Dead network = `Underground` (supported: it is the subway service layer). Live network grade is **not stated in the data**, so the default
is `AtGrade`. Elevated/tunnel/portal/viaduct code is fully implemented but only triggered by authored grades (or by setting `LiveNetworkDefaultGrade = Elevated`).

**Decision for you:** the first-look scene will show *placeholder-positioned* metro until real coordinates are authored. If that is unacceptable, disable placeholders; the generator will then build nothing from Meridian's files alone.

## 2. Files changed (shared-interface modifications — all additive)

| File | Change | Why |
|---|---|---|
| `Source/VOIDWorldBuilderCore/Public/Data/VoidDesignPackage.h` | + `#include "Data/VoidMetroData.h"`; + member `FVoidMetroData Metro;` on `FVoidDesignPackage` (defaults empty) | The only place normalized metro data can ride the existing pipeline to generators. Packages without a `metro` block are unchanged. |
| `Source/VOIDWorldBuilderImport/Private/VoidJsonPackageReader.cpp` | + include; + `"metro"` in `RootKnownFields`; + 6-line hook calling `FVoidMetroNetworkMapper::MapPackageMetroBlock` before the district block | Reuses the existing reader for an embedded `metro` block. Avoids a second importer. |
| `Source/VOIDWorldBuilderGenerators/Private/VOIDWorldBuilderGeneratorsModule.cpp` | + include; + `RegisterGenerator(MakeShared<FVoidMetroGenerator>())`; + matching `UnregisterGenerator("Metro")` | Registry registration, same pattern as Road. |
| `Source/VOIDWorldBuilderGenerators/VOIDWorldBuilderGenerators.Build.cs` | + `"DeveloperSettings"` to `PublicDependencyModuleNames` | Already reachable transitively via Import; made explicit. |
| `Source/VOIDWorldBuilderEditor/Public/SVoidWorldBuilderPanel.h` / `Private/SVoidWorldBuilderPanel.cpp` | + "Load Meridian Metro" and "Generate Metro" buttons, 3 handlers, 2 members (`MetroPackage`, `bMetroLoaded`). Road/import handlers untouched. | Gives a way to run the generator. Uses `FVoidGeneratorRegistry` like Roads. |
| `VOIDWorldBuilder.uplugin` | `EngineVersion` 5.6.0 → **5.8.0**; + `"Plugins": [ProceduralMeshComponent]` dependency | Target engine. The Road generator already depends on ProceduralMeshComponent but the descriptor never declared it. |
| `Docs/ValidationRules.md` | + "Metro Codes" table (codes extracted from source) | Project rule: new codes are documented in the same change. |

## 3. Files added

**Core** — `Public/Types/VoidMetroTypes.h`, `Public/Data/VoidMetroData.h`
**Import** — `Public/VoidMetroNetworkMapper.h`, `Private/VoidMetroNetworkMapper.cpp`
**Generators / Metro** — `Public/Metro/`: `VoidMetroGenerator.h`, `VoidMetroGenerationSettings.h`, `VoidMetroLayoutResolver.h`, `VoidMetroValidator.h`, `VoidMetroMeshBuilder.h`, `VoidMetroActors.h`, `VoidMetroExports.h`; `Private/Metro/`: matching `.cpp` files
**Generators / World Partition** — `Public/WorldPartition/VoidWorldPartitionSettings.h`, `VoidWorldPartitionHelper.h`; `Private/WorldPartition/`: matching `.cpp` files
**Tests** — `Generators/Private/Tests/VoidMetroTests.cpp`
**Docs** — `Docs/MetroGeneratorArchitecture.md`, `Docs/Samples/MetroAuthoredBlock.example.json` (format example only; all values illustrative), this file, `AGENT2_INTEGRATION_CHECKLIST.md`

~4,300 lines of C++ (headers + sources + tests) under `Metro`/`WorldPartition` paths.

## 4. Interfaces

**Generator** — `FVoidMetroGenerator : IVoidGenerator`, id `"Metro"`. Call `FVoidGeneratorRegistry::Get().FindGenerator("Metro")->Generate(Package, Context)`. Reads only `Package.Metro`; ignores `Package.District`. Fills `Context.GenerationValidationReport` and `Context.OutputLog`; honours `Context.IsCancellationRequested`.

**Normalized data** (Core) — `FVoidMetroData { Networks, Stations, Lines, Interchanges, TunnelLinks, DistrictLinks }`; every spatial field optional (`bHasPosition`, `bHasGradeOverride`, `CenterlinePoints` empty = unspecified).

**Import** (`FVoidMetroNetworkMapper`, no second importer — uses `FVoidJsonReader`):
`LoadMeridianMetroNetworkFile`, `LoadMeridianTunnelRelationshipsFile` (only `dead_network` tunnels kept), `MapMeridianMetroNetwork`, `AppendMeridianTunnelRelationships`, `MapPackageMetroBlock`, `MergeAuthoredOverrides` (authored wins by id; never loosens an entrance limit; refuses cross-network merges).

**Layout** — `FVoidMetroLayoutResolver::Resolve(Data, Params, Report) → FVoidMetroResolvedLayout` (pure/deterministic). `Params` is plain data, built by `FVoidMetroLayoutParams::FromSettings` or `MakeBuiltInDefaults`.

**Read-only export for other systems** — `FVoidMetroExportRegistry::Get()` (`Layout()`, `FindStation`, `OnLayoutChanged`): station coordinates, entrances, track alignment, tunnel portals, elevated spans, interchange hubs. Editor-thread only. `FVoidRoadGenerator` is not modified; consumers pull.

**World Partition** — `FVoidWorldPartitionHelper` (generator-agnostic): `IsWorldPartitioned`, `GetCellForLocation`, `SplitPathByCells`, `MakeFolderPath`, `ApplyPlacement`.

## 5. Expected input

1. **Meridian topology:** `MetroNetwork.json` (+ sibling `RoadNetwork.json` for dead-network tunnel endpoints). Loaded via panel "Load Meridian Metro" (Browse to `MetroNetwork.json` first) or programmatically through the mapper.
2. **Authored geometry (optional, recommended):** a `metro` object in a Design Package, or `MapPackageMetroBlock` + `MergeAuthoredOverrides`. Shape: see `Docs/Samples/MetroAuthoredBlock.example.json`. Note: a package still needs a valid `district` to pass the *existing* package importer; the metro-only path is the panel/mapper route above.
3. **Agent 1's foundation was not in the ZIP.** Assumption: whatever Meridian loader Agent 1 provides will call `FVoidMetroNetworkMapper` (or fill `FVoidDesignPackage::Metro` directly) and hand the package to the registry. If Agent 1 introduced its own normalized metro struct, add a one-function adapter to `FVoidMetroData`; do not add a second generator.

## 6. Generated outputs

Actors (no Tick, all static): `AVoidMetroTrackActor` (procedural mesh ≤4 sections, `USplineComponent`, batched-instance sleepers/piers) and `AVoidMetroStationActor` (platforms, canopy/podium, entrance headhouses, escalator/elevator placeholders, service placeholder for Live only). Interchanges attach to an existing station at the crossing, otherwise a stand-alone hub station is built. Details table: `Docs/MetroGeneratorArchitecture.md`.
With the placeholder layout on real Meridian data: 5 stations, 3 spokes/tunnels + 1 closed ring (~25 km, ≈98 chunk actors at 25 600 cm cells), 0 portals (dead track is entirely underground and dead stations are underground), 0 elevated spans (Live defaults to at-grade), Sector 0 threshold has exactly 1 entrance.

## 7. Coordinate assumptions

* Unreal units = **cm**, world space, +Z up, X forward / Y right. **No coordinate-conversion layer exists in the supplied Builder** (I searched), so authored coordinates are used as-is; the Road generator does the same.
* World origin = Olympus Spire (`DistrictRegistry.json`: `absolute_center_origin_point_canon_explicit`).
* Station Z = rail level for its grade: at grade 0, elevated `+ElevatedHeightUnits` (1200), underground `−UndergroundDepthUnits` (1000). Entrances are ground-level (Z = 0).
* Yaw = direction of the nearest track through the station. Right vector = `(−sin yaw, cos yaw)`.
* Grade is uniform per line/segment; it changes only at stations (with smoothstep ramps of `GradeRampLengthUnits`). Mid-line grade changes need a future `spans` field.

## 8. World Partition assumptions

* Placement only. Does not create/convert a partitioned map, define runtime grids, use data layers, or stream anything.
* `ChunkCellSizeUnits` (25 600) should equal your map's runtime-grid cell size; it only sizes actor chunks.
* Named runtime grids (`LiveNetworkRuntimeGrid`, `DeadNetworkRuntimeGrid`) must already exist in World Settings; default `None` = map default.
* Actors are pivoted at their chunk/station centre so bounds and cell assignment are sane.
* Cleanup finds only **loaded** actors. Regenerate with the area loaded, or unloaded-cell actors from earlier runs will be left behind.
* HLOD: hook only. Procedural-mesh components do not contribute to HLOD proxies.

## 9. Engine APIs assumed for 5.8.2 — check these first when compiling

1. `AActor::SetFolderPath(FName)`, `SetIsSpatiallyLoaded(bool)`, `CanChangeIsSpatiallyLoadedFlag()`, `SetRuntimeGrid(FName)`, `bEnableAutoLODGeneration` (all editor-only), used in `FVoidWorldPartitionHelper::ApplyPlacement`.
2. `UWorld::GetWorldPartition()` (pointer compare only, no WP header included).
3. HLOD layer is set by reflection on a property named `HLODLayer`; if it does not exist the assignment silently no-ops.
4. `UInstancedStaticMeshComponent::AddInstances(const TArray<FTransform>&, bool)` signature.
5. `/Engine/EngineDebugMaterials/VertexColorMaterial` exists (default `MetroMaterial`). If it does not load, geometry uses the default material and **vertex colours (the only network differentiation) will not show**.
6. ProceduralMeshComponent is still shipped as an engine plugin in 5.8 (Road depends on it too) — I did not verify.
7. `TMap::operator=(std::initializer_list)` with `{ TEXT("a"), 1.0f }` pairs (settings constructor and `MakeBuiltInDefaults`).
8. Triangle winding: quads self-correct to the intended normal, using the same "front = (B−A)×(C−A)" relationship the Road ribbon uses. If metro surfaces are inside-out, enable `bFlipTriangleWinding` (mirrors the Road generator's own verification flag). Unverified without an editor.
9. `DefaultVOIDWorldBuilder.ini` persistence of `TMap<FName,float>` settings.

## 10. Tests (written, not run)

`VOID.WorldBuilder.Metro.Mapper.*` (2), `…Metro.Resolver.*` (5), `…Metro.Validator.*` (2), `…WorldPartition.Helper.CellsAndChunks`, `…Metro.Settings.DefaultsMatchResolverDefaults`. Fixtures are copied from the real Meridian metro/tunnel data. They cover: topology mapping, authored-overrides-win, placeholder flagging, Spire at origin, ring passes through station, interchange attaches to the station, dead tunnels underground and separate, Sector 0 single-entrance clamp, no-placeholder mode invents nothing, determinism, grade ramps/portals/elevated runs/contacts, every validator rule, cell/chunk continuity.

## 11. Known limitations

* Placeholder radii/azimuths are not canon (§1). Sector 0's position is explicitly "narrative nowhere" in Meridian; its bearing here is arbitrary.
* White Zones stations are "generated per network node, not individually enumerated". Only one node station exists in the data, so one is built; more require authored stations (the resolver already spaces same-district stations by `NodeAngularSpacingDegrees`).
* Undercroft has "multiple implied" access points; one is built. Tunnel links attach to it.
* Entrances are not in Meridian data except Sector 0's single point. Other stations get `DefaultEntrancesPerStation` (1) **placeholder** entrances, flagged as such.
* Underground stations use platforms sized to fit the bore, no separate cavern.
* Tunnel geometry is an inward-facing open tube: correct from inside, invisible from outside by design; the surface trace strip marks the alignment from above (`bDrawSurfaceTraceForUnderground`). The tube does not carve terrain.
* Near-vertical track spans are skipped by the box builder.
* Greybox only: no authored materials, no LODs, no collision tuning beyond complex-as-simple on structural sections.
* Cleanup covers loaded actors only (§8).
* Panel: the existing "Generation Summary" text still says "roads" when showing a metro run; the log lines are correct.
* **Pre-existing, not changed (out of scope):** `FVoidRoadGenerator` names actors uniquely and never destroys earlier ones, so regenerating roads accumulates duplicates. Metro does not have this problem. Recommended follow-up: give road actors an owner key and reuse `DestroyOwnedActors`-style cleanup.
* Files from other phases/agents were not visible; conflicts in the shared files of §2 are possible.

## 12. Integration

See `AGENT2_INTEGRATION_CHECKLIST.md` (exact steps, patch snippets for each shared file, and a smoke test).
