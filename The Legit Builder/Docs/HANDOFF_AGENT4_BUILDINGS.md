# HANDOFF — Agent 4: Building Generator (Phase 4)

## Read this first

1. **Meridian does not contain building geometry.** `Meridian Master/*.json` has no footprints, heights,
   floors, parcels, per-building entrances, road geometry or coordinates (its docs defer them to an in-editor
   level-design pass; `BuilderRules.json` even halts on fabricated coordinates). It has 14 landmark records and
   5 district records at graph level only. So the generator is built on the Builder's own schema
   (`FVoidBuildingSpec` / `FVoidRoadSpec`) and **has not produced a Meridian city**. Someone must author or
   export footprints/heights/types into that schema (or into a package the importer accepts) first.
2. **Nothing here has been compiled or run inside Unreal Engine 5.8.2.** No engine was available. What *was*
   verified: the engine-independent logic (geometry, normalizer, road relationship, massing, mesh builder,
   batching) was compiled with `g++ -Wall -Wextra -Wshadow` against a small mock of UE core types and passes
   109 checks (`Verification/`). UHT, engine-API usage, plugin loading, actor spawning, the world-based regeneration
   test and rendering are **untested**. The first thing to do is build the plugin and run
   `VOID.WorldBuilder.BuildingGenerator.*` in the Session Frontend.

## Files added (`Source/VOIDWorldBuilderGenerators/`)

`Public/Building/`: `VoidBuildingTypes.h`, `VoidBuildingParams.h`, `VoidBuildingGenerationSettings.h`, `VoidBuildingGeometry.h`,
`VoidBuildingNormalizer.h`, `VoidBuildingRoadContext.h`, `VoidBuildingMassing.h`, `VoidBuildingMeshBuilder.h`, `VoidBuildingActor.h`,
`VoidBuildingGenerator.h`, `VoidBuildingTestFixture.h`
`Private/Building/`: matching `.cpp` for each, plus `VoidBuildingBatcher.cpp` (world-free half of the generator) and
`VoidBuildingRoadProfileBridge.cpp` (only file touching the Road profile types)
`Private/Tests/VoidBuildingGeneratorTests.cpp` (7 automation tests)
`Resources/TestFixtures/BuildingGenerator_TestFixture_NOT_MERIDIAN.json` (importable, Builder schema, synthetic)
`Verification/` (harness, mock headers, output log, `fixture_layout.png`), `AGENT4_BUILDING_SCHEMA.md`

## Files changed

- `VOIDWorldBuilder.uplugin`: `EngineVersion` 5.6.0 → 5.8.0; added `Plugins: ProceduralMeshComponent` (Road Generator already depended on it without declaring it).
- `VOIDWorldBuilderGenerators.Build.cs`: added `DeveloperSettings`.
- `VOIDWorldBuilderGeneratorsModule.cpp`: registers/unregisters `"Building"`.
- `SVoidWorldBuilderPanel.h/.cpp` (Editor module, minimal): the Road-only handler became `RunGenerator(Id, Text)`; added a **Generate Buildings** button beside Generate Roads. Road behaviour unchanged.

## API

- Generator id `"Building"` via `FVoidGeneratorRegistry`. `Generate(Package, Context)` → cleanup → spawn `AVoidBuildingBatchActor`s; returns true if ≥ 1 actor was created. Appends issues to `Context.GenerationValidationReport`.
- `FVoidBuildingGenerator::BuildBatches(District, Params, Report, OutBatches, OutStats, IsCancelled)` — world-free, deterministic; use it to get data without spawning.
- `FVoidBuildingGenerator::RemoveGenerated(World, DistrictId)` — the cleanup used for regeneration.
- Settings: Project Settings → Plugins → *VOID World Builder Building Generation* (`UVoidBuildingGenerationSettings`); `ToParams()` snapshots it.
- Extension points: `KeywordOverrides` (category vocabulary), `AssetOverrides` (building id → actor class), `GreyboxMaterial`.

## Input expected

`FVoidDesignPackage::District.Buildings` (`Id`, `FootprintCorners`, `HeightUnits`, `BuildingType`) and `District.Roads`. Import first via the existing importer; run Generate Roads first for visual coherence (not required for data: the generator reads `FVoidRoadSpec` directly).

## Output

One `AVoidBuildingBatchActor` per (district, grid cell, default 250 m), named `VoidBuildings_<district>_<x>_<y>`, tagged `VoidBuildingsGenerated` + `VoidDistrict:<id>`, located at the cell centre. One `UProceduralMeshComponent`, sections 0 Wall, 1 Roof, 2 Glass, 3 Trim, 4 Foundation, 5 Technical (vertex-coloured, flat-shaded; UV = world metres). `Buildings` array of `FVoidBuildingMetadata` (see schema doc). No Tick, no dynamic materials, no per-building UObjects.

## Behaviour summary

- **Massing** per category/size, deterministic: plain extrude, podium + tower, setback tiers, courtyard ring, plinth + body, landmark (podium, tower, crown, spire). Roofs: flat, parapet, pyramid (low convex residential). Rooftop stair cores/mechanical boxes, industrial stacks, landmark antenna. String courses, ribbon/curtain/punched windows with per-building bay rhythm, doors with frames, canopies, foundation skirt.
- **Roads:** corridor = half road width (+ kerb + sidewalk when `bHasSidewalk`) from the same profile table as the Road Generator; roundabouts = 32-chord ring + blocked island; bridges/tunnels ignored. Required clearance = `MinSetbackUnits` (×`LandmarkSetbackMultiplier` for landmarks). Conflicts: slide away (≤ `MaxAdjustmentUnits`) then skip, or skip only, or generate anyway. Frontage = nearest at-grade road within `MaxFrontageSearchUnits` (highways/alleys/service penalised); front edge faces it; entrance(s) on it; access point on the corridor edge; base Z from that road.
- **Determinism:** own PCG RNG seeded from a stable string hash of the id + `GlobalSeed`; output sorted by id; no `FMath::Rand`, no `GetTypeHash(FName)`. Authored height and footprint are never altered except the road slide (original kept in metadata).
- **Regeneration:** Generate destroys all actors tagged for that district, then respawns. Requested names may get a suffix if the old actor has not been garbage-collected; identity is by tag/`DistrictId`/`BatchCell`, not by name. Nothing is destroyed if the run produced zero buildings.

## Verified vs not (honest table)

| Item | Status |
|---|---|
| Pure logic compiles (g++, -Wall -Wextra -Wshadow, mock UE types) | Yes |
| 109 logic checks: geometry, classification, determinism (repeat, reversed source order, seed change), corridor clearance for every generated building, height preservation, access points on corridor edge, winding/normals, parallel arrays, detail-level triangle counts, edit-and-rebuild counts | Pass |
| 3,600-building district: 81 batches, ~1.0 M triangles (Auto windows), 366 ms in the harness | Indicative only (mock containers) |
| UE 5.8.2 compile, UHT, module/plugin load | **Not verified** |
| Actor spawn, undo, regeneration in a real world (`FVoidBuildingRegenerationTest`) | **Not verified** (test written from engine convention) |
| Visual quality in viewport, material with vertex colours, collision cook cost | **Not verified** |
| Meridian data | **Not applicable — no geometry exists** |

Likely first-build friction: `ProceduralMeshComponent` API drift, `FActorSpawnParameters::NameMode`, `UDeveloperSettings` header/category, the `UWorld::CreateWorld` test pattern, and any project warning-as-error settings (shadowing was checked; narrowing/conversion was not).

## Known limitations

- No Meridian input exists (above). The category vocabulary is generic and not derived from Meridian.
- No building–building overlap resolution. A slid building can overlap a neighbour; this is reported (`VOID.Building.OverlapAfterAdjustment`), not fixed. (Fixture: several.)
- "Facade depth" is a cue only (window/trim/band offsets of 2–6 uu); no wall bays, balconies or projecting volumes. Courtyards are rings with a floor; no interior access. No sloped-terrain adaptation beyond the foundation skirt and frontage-road Z.
- Slide direction is a simple sum of push vectors; a building squeezed between two roads is skipped, not re-shaped.
- Interior-row buildings farther than `MaxFrontageSearchUnits` from any road get no frontage (Info reported; entrance on longest wall).
- Windows are ~63% of triangles in the stress run (1.01 M with Auto vs 0.37 M without). `WindowDetail = Ribbon/None` or lowering `MaxWindowQuadsPerBuilding` trims it. No LOD, no HISM (unique footprints make merged batches cheaper than instancing here).
- Collision uses complex-as-simple on wall/roof/foundation; cook time on very large batches unmeasured. `bGenerateCollision=false` disables it.
- A batch is rebuilt whole; there is no incremental per-building update. Asset override spawns the class at centroid/yaw only.
- Road Generator (not mine) names actors with `MakeUniqueObjectName` and does not clean up, so **Generate Roads twice duplicates roads**. Left untouched.
- Roundabout ring approximated by 32 chords, whatever the Road Generator's own segment count.

## Integration instructions

1. Copy `VOIDWorldBuilder/` into the project's `Plugins/`; regenerate project files; build the Editor target.
2. Enable the plugin (also enables `ProceduralMeshComponent`). Optionally set a shared vertex-colour material in *Building Generation → Greybox Material*.
3. Import a package (e.g. `Resources/TestFixtures/BuildingGenerator_TestFixture_NOT_MERIDIAN.json`, synthetic), **Generate Roads**, **Generate Buildings**. Re-click to regenerate.
4. Run automation tests `VOID.WorldBuilder.BuildingGenerator`.
5. **District/Environment/Lighting:** find batches by tag `VoidDistrict:<id>`; read `AVoidBuildingBatchActor::Buildings` for per-building category, footprint, height, entrances, frontage; recolour via the vertex colours or replace the material.
6. **Asset replacement (later system):** query `AssetCategory` + footprint/yaw/height, or use `AssetOverrides` for the built-in spawn hook; `bGreyboxSuppressed` is recorded per building.
7. **Meridian:** produce footprints/heights/type tags in the Builder schema (the type tag must contain a recognised keyword or add `KeywordOverrides`), then run the same steps.
