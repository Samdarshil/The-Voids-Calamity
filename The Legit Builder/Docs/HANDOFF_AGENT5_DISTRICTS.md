# HANDOFF - Agent 5: District Generator (Phase 5)

Plugin: `VOIDWorldBuilder` v0.5.0, target UE 5.8.2 (`EngineVersion` 5.8.0 in the .uplugin).
Inputs audited: stabilized VOID World Builder ZIP (Phase 3 plugin: Core / Import / Generators with Road / Editor) and the Meridian design/data ZIP.

## 0. Verification status (read this first)

The sandbox has no Unreal Engine, so **nothing below has been compiled by UBT/UHT, loaded as a plugin, or run in the editor.** What was actually verified:

| Verified | How |
|---|---|
| Layout planner, validator and geometry logic | Compiled with g++ (C++17) against a small UE-Core stand-in, plus the *real* `VoidDesignPackage.h` / `VoidValidationReport.h`, and run on a dataset generated from the real Meridian JSON (`Tools/StandaloneDistrictCheck/build_and_run.sh`) |
| Zero validation errors/warnings on default settings | Same harness. Variants also pass: 3 spokes, 6 spokes + 6 elite towers, Archives seam at 45 deg. A 1500-unit block size correctly errors instead of crashing |
| Validator catches broken plans | Harness negatives: building outside boundary, dangling road end, plaza over the Spire tower, duplicate geometry, Sector 0 at grade, blocked Spire sightline |
| Determinism and identical White Zones nodes | Harness: two builds identical; all four nodes have the same height typology |

**Not verified (expect to iterate on first compile):** UHT/UBT, every engine-touching file (`VoidDistrictGenerator.cpp`, `VoidDistrictActors.cpp`, `VoidDistrictQueryLibrary.cpp`, `VoidDistrictGenerationSettings.*`, `VoidMeridianRegistryReader.cpp`, panel/commandlet edits), the automation tests (`VOID.WorldBuilder.Districts.*`), UE 5.8 API drift (written against the UE 5.x API in use by the existing plugin), plugin load, and actual spawning. The JSON reader was only cross-checked by an independent Python parse of the same files (5 districts, 14 landmarks, 4 routes, 2 tunnels, 6 unreachable pairs).

Also unverified, inherited from Phase 3: the Road mesh builder documents that triangle winding may need flipping on first in-editor look (`bFlipWindingForVerification`). District greybox meshes use the same winding convention, so the same flip applies to them.

## 1. Audit findings

- **Architecture:** Core (Runtime: `IVoidGenerator`, `FVoidGenerationContext`, `FVoidDesignPackage`, validation report) / Import (Editor) / Generators (Editor: registry + Road) / Editor (panel, commandlet). Generators register by id in `FVoidGeneratorRegistry`.
- **No Building generator exists** in the delivered ZIP (Agent 4's output was not part of the inputs). `FVoidBuildingSpec` exists in Core and is the contract used here.
- **Meridian data is a rules/relationship manifest, not geometry.** `Meridian_Master.json` sets `coordinate_policy: no_fabricated_coordinates`, and the master plan says no road names, widths or coordinates are specified. It cannot be forced into the single-district `FVoidDesignPackage` envelope.
- **Only one per-district data file ships** (`Metro_Archives_data.json`). The other four districts' `data_reference` files are absent, so those districts run from registry-level data.
- **Five districts, 14 landmarks.** No sixth district is invented; the reader rejects landmarks or routes that name unknown districts.
- **Stabilization gaps found and fixed (small):** the .uplugin did not declare the `ProceduralMeshComponent` plugin dependency that the Road generator's module already uses; `EngineVersion` was 5.6.0.

## 2. What was built

```
Meridian data --> FVoidMeridianRegistryReader --> FVoidDistrictLayoutBuilder (plan)
              --> FVoidDistrictValidator (blocks on Error, before touching the level)
              --> FVoidDistrictGenerator (actors)
```

Pure planning (no UWorld) is separate from actor spawning so the planning half is testable and CI-runnable (`-MeridianDir=` commandlet mode).

### District layout (all from data or a labelled, tunable parameter)
| District | Layout | Data source of the shape |
|---|---|---|
| `olympus_spire` | Origin tower (tier-1 landmark), base plaza (annulus around tower), plaza ring road, inner spokes, elite tower cluster, Live Network stop port | registry: `absolute_center_origin_point`, singular civic centerpiece; landmarks |
| `white_zones` | One identical typology node per sector between spokes (4x4 grid: through-roads, split perimeter, ring connector, 1 plaza, parks, 1 trust-center, 2-4 story lots), node 0 flagged flagship | registry: `distributed_network_nodes`, WP flagship/template regions; landmark instancing policy `generated_per_network_node_identical_citywide`; 2-4 story ceiling |
| `metro_archives` | Bounded seam facility: bespoke mid-rise building, civic forecourt, sub-basement seam port | registry seam band; landmark tier 3; its data file (`commercial_presence: false`) |
| `undercroft` | Below-grade annulus, three belts from the route's `sub_belts_served`, service-road maintenance layer, seam + threshold tunnels, 3 landmark anchors. **No building fabric** (see known issues) | registry: `continuous_substrate_multi_ring`, `internal_three_belt_gradient`; RoadNetwork service routes and tunnels |
| `sector_0` | Enclosed below-grade boundary, threshold + echo relay anchors, near-zero density | registry adjacency (shared threshold with Undercroft); no sightline |
| Live Network | Spine arterials (owner `live_network`) + mid-tier ring road, split at node connectors so every endpoint is shared | RoadNetwork routes and hard constraint |

### District character (`FVoidDistrictProfile`, exposed on `AVoidDistrictActor::Profile`)
Density band per district (Master Plan Sec. 22), coverage, story range, open-space ratio, vegetation density, road density (block size), commercial share (only where the district's data says; otherwise `-1` = unknown), plaza cells per node, landmark count. **Every value carries a provenance line** naming the Meridian source or "Default (density-band table)" / "Settings character override". Result on real data: Spire (Sparse, 0.30 coverage, 0.55 open) vs White Zones (High, 0.70 coverage, 0.25 open, 2-4 stories) vs Sector 0 (NearZero) - they do not look alike.

### Ownership and stable IDs
```
Meridian (AVoidMeridianRootActor)
 +- District_<id> (AVoidDistrictActor)      profile, ports, nodes, counts
 |    +- Roads      road + junction actors from the existing Road generator
 |    +- Buildings  ISM components "Buildings_<category>" (or Building-generator actors)
 |    +- Public spaces  PublicSpaceMesh (vertex-coloured by type) + TreeMarkers
 |    +- Landmarks  AVoidDistrictLandmarkActor (registry id verbatim)
 +- Meridian/LiveNetwork (live_network road actors)
```
Ids: `<district>.road.*` style prefixes, e.g. `white_zones.node_2.line_r_1_0`, `olympus_spire.bld.elite_tower_3`; landmarks use the registry id (per-node fixtures: `white_zones_weave_display_column.node_2`). Every generated actor has tag `VoidMeridianGenerated`, `VoidDistrict:<id>` and `VoidLayer:<layer>`. **Regeneration destroys exactly the tagged actors, then rebuilds** (inside one undo transaction). Duplicate ids are a validation Error.

### Integration with Road / Building
- Road generator is consumed unchanged, by id (`"Road"`), in **two calls**: surface/live roads and the below-grade dead network. This keeps live and dead networks from ever sharing a junction graph (Meridian rule: never merge). Spawned road/junction actors are diffed against a pre-call snapshot and adopted under their district.
- Missing Building interface -> smallest adapter: if a generator registered as `"Building"` exists (and `bPreferRegisteredBuildingGenerator`), each district's `FVoidBuildingSpec` list is handed to it in a per-district `FVoidDesignPackage`; new actors are adopted. If none exists, or it fails, built-in greybox instancing runs (one shared cube mesh, one ISM per category). No fork of any contract.
- Explicit-package adapter: `FVoidDistrictLayoutBuilder::ApplyExplicitPackage` lets an existing `FVoidDesignPackage` for one district replace its synthesized roads/buildings.

### Landmarks exposed to Environment / Lighting / Cinematic / Asset Replacement
`UVoidDistrictQueryLibrary` (Blueprint-callable): `GetMeridianRoot`, `FindDistrict`, `GetLandmarks(district, skylineOnly)` (sorted by tier), `FindLandmark`, `GetDistrictProfile`, `GetPorts`. Landmark actors carry: registry id, tier, type, interior/below-grade flags, sightline visible-from/excluded, bounds (half extent + height), placeholder flag, `OpenFlagId` (server hub carries `spire_server_hub_nyx_discrepancy`: architecture yes, encounter content no), and `BackingBuildingId`. Building-backed landmarks (Spire tower, Archives building) have **no** proxy mesh, so no duplicate geometry. Asset replacement: swap the mesh on a `Buildings_<category>` component, or replace a landmark's proxy.

### Validation (`VOID.District.*`, blocks generation on Error)
BuildingOutsideBoundary, BuildingOnRoad, BuildingsOverlap / DuplicateGeometry, PublicSpaceOverlapsMajorStructure / OverlapsBuilding / OnRoad / OutsideBoundary, RoadDeadEnd, RoadCrossesDistrict (surface road through Archives; non-White-Zones road through a node; ring below grade), BoundariesOverlap (same layer), AdjacencyNotHonored (Warning), UnreachablePairConnected, Sector0Sightline, DuplicateId, BadProfile; Warning `SpireSightlineBlocked` (non-blocking per Master Plan Sec. 17).

## 3. Files

**Added - Import module**
`Public/Meridian/VoidMeridianRegistry.h`, `Public/Meridian/VoidMeridianRegistryReader.h`, `Private/Meridian/VoidMeridianRegistryReader.cpp`

**Added - Generators module** (`Public/District/`, `Private/District/`)
`VoidDistrictTypes.h`, `VoidDistrictLayoutParams.h`, `VoidDistrictGeometry.h/.cpp`, `VoidDistrictLayoutBuilder.h/.cpp`, `VoidDistrictValidator.h/.cpp`, `VoidDistrictActors.h/.cpp`, `VoidDistrictQueryLibrary.h/.cpp`, `VoidDistrictGenerationSettings.h/.cpp`, `VoidDistrictGenerator.h/.cpp`; tests `Private/Tests/VoidDistrictTests.cpp`, `Private/Tests/VoidDistrictTestData.inc` (trimmed real registries)

**Added - other:** `Samples/VoidDistrictLayoutOverrides.example.json`, `Tools/StandaloneDistrictCheck/*` (harness, not part of the plugin build), this handoff

**Modified**
- `VOIDWorldBuilder.uplugin`: EngineVersion 5.8.0, version 0.5.0, `ProceduralMeshComponent` plugin dependency
- `VOIDWorldBuilderGenerators.Build.cs`: `DeveloperSettings`
- `VOIDWorldBuilderGeneratorsModule.cpp`: register/unregister `FVoidDistrictGenerator` ("District")
- `SVoidWorldBuilderPanel.h/.cpp`: "Generate Districts" button (clone of the roads handler; works without a package import)
- `VoidWorldBuilderCommandlet.cpp`: `-MeridianDir=` mode
- Core, Road, and all existing Import files: **untouched.**

## 4. Interfaces, inputs, output

**Input requirements**
1. Folder with `Meridian_Master.json` and the registries it marks required (DistrictRegistry, RoadNetwork, LandmarkRegistry, ...). Missing required file = Fatal (BuilderRules: halt). Missing per-district `*_data.json` = Warning.
2. Set in Project Settings > Plugins > *VOID World Builder District Generation*: Meridian Data Directory (required), Layout (radii, spoke count, sizes), Character Overrides, optional Layout Overrides File.
3. Optional `VoidDistrictLayoutOverrides.json` (see Samples) for real boundary polygons / landmark positions.

**Output:** the actor tree above, a `FVoidValidationReport` merged into `FVoidGenerationContext.GenerationValidationReport`, and log lines in `Context.OutputLog`.

**Programmatic:** `FVoidDistrictGenerator::GenerateFromInput(Input, Options, ExplicitPackage*, Context, Stats*)`; planning only: `FVoidDistrictLayoutBuilder::Build` + `FVoidDistrictValidator::Validate`.

**CI:** `UnrealEditor-Cmd <Project> -run=VoidWorldBuilder -MeridianDir="<folder>" [-Overrides="<json>"]` -> exit 0 if the plan has no errors.

**Dependencies:** Core, Import, Road generator (by id, falls back to constructing it), UnrealEd, ProceduralMeshComponent plugin, DeveloperSettings. Optional: a `"Building"` generator.

**Generation order:** macro order from `Meridian_Master.json generation_order` (mapping `white_zones_network_nodes` -> white_zones and `undercroft_substrate` -> undercroft): spire, live network, white zones, undercroft, metro archives, sector 0. Actor spawn order: root -> district actors -> roads (live, then dead) -> per district: meshes, buildings, landmarks.

## 5. Coordinates: the important caveat

Meridian gives relationships, not coordinates. All positions come from `FVoidDistrictLayoutParams` (radial band radii, spoke count, sizes) around the Spire at the origin, in Unreal units (1 uu = 1 cm). **They are production placeholders** and every landmark is flagged `bPositionIsPlaceholder` until a layout-overrides file supplies a position. Radii/sizes are defaults I chose (e.g. core 400 m, mid-tier ring at 1.5 km, White Zone block 60 m); they are not design decisions. Please have design confirm or override them before the cinematic camera relies on them.

## 6. Known issues / gaps

1. **Not compiled in UE** (see section 0). Expect small fixes.
2. **Undercroft has no building fabric.** The data gives density only qualitatively (High, informal) and the Undercroft data file was not delivered. Boundaries, belts, maintenance roads, tunnels and landmark anchors exist.
3. **Simplified routes:** `pre_council_subway_service_layer` is not separately generated; its role is covered by the Undercroft maintenance layer and the two tunnel roads. Spire skybridges (`flagged_for_signoff`) are deliberately not generated. `bridge_relationships` is not parsed.
4. **Commercial/residential balance is data-silent** for four of five districts (`CommercialShare = -1`), so White Zones lots are typed `Residential_MidTier` plus one `CommunityTrustCenter` per node (both named in the master plan) and no commercial fabric is invented.
5. **Landmark placements inside interiors** (atrium, server hub, reading room, exhibit case) are anchors at their building's centre, not interior layout claims. Undercroft landmarks are placeholders, one per belt.
6. **Sightline check is 2D + height** against building boxes only; it is a Warning by design.
7. **Interpretation:** "ring road never routes through the Undercroft" is enforced as "ring roads are never below grade", because the Undercroft substrate lies beneath every band in plan view.
8. **Actors live in an Editor module** (as the Road actors already do). Fine for editor/Sequencer cinematics; they will not exist in a packaged game.
9. Greybox building colours: ISM components share one cube mesh and default material; colour/asset per category is set on the `Buildings_<category>` component (public spaces and boundaries use a vertex-colour debug material, if `/Engine/EngineDebugMaterials/VertexColorMaterial` exists in 5.8).
10. Running the old Road button first leaves untagged road actors that district regeneration will not remove; use one path or the other in a level.
11. No World Partition / Data Layer assignment yet; `FVoidDistrictNodeInfo.WorldPartitionRegion` and the registry region strings are exposed for the next phase. Actors are organised in Outliner folders only.

## 7. Integration steps

1. Copy `VOIDWorldBuilder/` into `<Project>/Plugins/`, regenerate project files, build (Editor target). Fix any first-compile issues.
2. Enable the plugin (ProceduralMeshComponent is now pulled in automatically).
3. Project Settings > Plugins > VOID World Builder District Generation: set **Meridian Data Directory** to the folder with `Meridian_Master.json`.
4. Run `Automation RunTests VOID.WorldBuilder.Districts` (and the existing `VOID.WorldBuilder.RoadGenerator` set).
5. Open the VOID World Builder panel > **Generate Districts** (or call `FVoidDistrictGenerator::GenerateFromInput`). Regenerate at will.
6. Check Output Log for `[District ...]` lines and the profile provenance; check the Outliner tree under `Meridian/`.

**Agent 4 (Buildings):** register your generator under id `"Building"`; it will receive one `FVoidDesignPackage` per district (footprints in world space, `BuildingType` in `{SpireTower, EliteResidentialTower, Residential_MidTier, CommunityTrustCenter, MetroArchivesBuilding}`), and its spawned actors are adopted automatically.
**Agent 6 / 7 (Lighting, Environment, Cinematic):** use `UVoidDistrictQueryLibrary`; read `Profile` for character, `GetLandmarks(NAME_None, true)` for the skyline, `BackingBuildingId`/`ProxyMesh` for replacement, `Ports` for Metro/Live Network stop anchors.
