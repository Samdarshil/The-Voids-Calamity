# HANDOFF — Agent 1 (Core Foundation + Import Pipeline + Road System)

Target: **Unreal Engine 5.8.2** · Plugin: `VOIDWorldBuilder` v0.2.0 (upgraded from the Phase 3 snapshot)

---

## 0. READ FIRST — verification status and two brief-vs-reality findings

### 0.1 What was and was NOT verified

| Item | Status |
|---|---|
| C++ compilation against UE 5.8.2 | **NOT DONE** — no Unreal install in the authoring environment (and no network). |
| UnrealHeaderTool (UHT) pass | **NOT DONE.** |
| Plugin load / module startup | **NOT DONE.** |
| Automation tests (`VOID.WorldBuilder.*`) | **Written, NOT RUN.** |
| Road actors / meshes in an editor world | **NOT DONE** — never spawned. |
| `FVoidSha256` | **Verified** — compiled with g++ against NIST vectors (empty, "abc", 2-block, 1M×"a") and padding-boundary lengths 55/56/63/64/119/120, all matching Python `hashlib`. (The compiled harness used a tiny shim for `FString`; the algorithm body is byte-identical to the shipped file.) |
| Meridian rules (schema conformance, references, order, spatial scan, checksums) | **Verified in Python** via `Tools/meridian_reference_check.py` against the real package: 12/12 schemas conform, 0 errors. The C++ importer mirrors these rules but **the C++ itself has not run**. |
| Bracket balance of every source file | Checked by script (not a substitute for compiling). |

**Treat the first compile as the first real test.** Expect possible small fixes. Specific places most likely to need attention are listed in §11.

### 0.2 The brief assumes Meridian has coordinates and road geometry. It does not.

I read every Meridian JSON. **Meridian contains no coordinates, units, widths, elevations, transforms, splines, sidewalks, curbs, intersections, crosswalks, alleys, highways or roundabouts.** This is deliberate and self-declared:

- `Meridian_Master.json → builder_configuration.coordinate_policy = "no_fabricated_coordinates_no_fabricated_transforms_no_fabricated_mesh_ids"`
- `geometry_calculation_authority = "unreal_engine_builder_runtime"`
- `ValidationSchema.json` rule VR-012 forbids fabricated precision; a script over all registries found **zero** spatial/unit fields.

Consequences, stated plainly:

- **Step 6 (“how Meridian coordinates map to Unreal”)** — there are none to map. I could not determine axis/units/origin *from Meridian*. What exists instead: (a) `FVoidWorldSpace`, one documented conversion used for legacy design-package coordinates, and (b) a polar “layout frame” where the **Builder** places Meridian’s *named radial bands*. The band radii are Builder-authored **placeholder greybox values** (`UVoidWorldBuilderSettings::BandRadii`), logged as a warning on every use. They are not design figures.
- **Step 7/8 (road types list)** — from Meridian I generate only what it states: a **radial arterial** (spine) and a **ring road**, plus a spine×ring crossing computed analytically. Service routes (Dead Network), tunnels and the skybridge are recorded as **topology only**. Highways, alleys, sidewalks, curbs, crosswalks, bridges-as-roads, tunnels-as-geometry, roundabouts and traffic islands are **not generated from Meridian** because Meridian does not describe them. Sidewalks/curbs/crosswalks/highways/roundabouts/bridges/tunnels **remain fully supported through the legacy design-package path** (existing behaviour preserved). **Traffic islands are not implemented anywhere** (neither Meridian nor the legacy schema defines them).
- If the design team later adds coordinates, that requires a schema bump; the importer already **errors** on spatial data (VR-012) so this cannot slip in silently.

---

## 1. Architecture discovered (Phase 3 snapshot)

Plugin `VOIDWorldBuilder`, 4 modules:

| Module | Type | Purpose | Key contents |
|---|---|---|---|
| `VOIDWorldBuilderCore` | Runtime | Shared data + contracts | `FVoidDesignPackage`/`FVoidRoadSpec`, `IVoidGenerator`, `FVoidGenerationContext`, `FVoidValidationReport`, `FVoidSchemaVersion`, log category |
| `VOIDWorldBuilderImport` | Editor | Legacy package import | `FVoidDesignPackageImporter`, `FVoidJsonPackageReader`, `FVoidPackageValidator`, reader registry, `UVoidImportSettings` |
| `VOIDWorldBuilderGenerators` | Editor | Generators | `FVoidGeneratorRegistry`, `FVoidRoadGenerator` (+ spline/mesh/intersection/bridge-tunnel builders, validator, `AVoidRoadActor`, `AVoidRoadJunctionActor`) |
| `VOIDWorldBuilderEditor` | Editor | UI + CLI | Slate panel, `UVoidWorldBuilderCommandlet`, menu |

Phases 1 and 2 in the ZIP are earlier snapshots; **Phase 3 is the superset** and is what I upgraded. The existing importer expects a **district-centric “design package”** (one district, 2-D explicit coordinates). That is a *different* format from Meridian; both are now supported side by side (see §6).

## 2. Discrepancies found (documentation vs implementation vs data)

| # | Finding | Handling |
|---|---|---|
| D1 | Legacy design-package schema (explicit 2-D coordinates, one district) ≠ Meridian (relationships only). | Kept legacy importer untouched; added `FVoidMeridianImporter` for the real schema. |
| D2 | Brief lists highways/sidewalks/curbs/intersections/crosswalks/etc. as Meridian data; Meridian has none of them. | See §0.2. |
| D3 | `RoadNetwork.json` “bridge” `spire_tower_skybridges` is an **elevated pedestrian bridge between buildings** (flagged for sign-off), not a road bridge. Its `connects` ids (`olympus_spire_primary_tower`, …) live in `Olympus_Spire_data.json`, which is **not in the ZIP**. | Not a road; recorded, unresolvable ids reported as a Warning; skipped unless `bAllowFlaggedContent`. |
| D4 | `PackageManifest.json` lists **19 locked inputs; only 5 are in the ZIP** (Master Plan + 4 Metro Archives files). 14 absent: Olympus Spire ×4, Sector 0 ×4, White Zones ×4, Undercroft ×2. | Checksum mismatch = Error under strict canon lock; **absent = Warning only** (cannot verify ≠ modified). All 27 package files + the 5 present locked files hash correctly. |
| D5 | Only `Metro_Archives_data.json` of five district data files is present; `void_world_location_schema_v1` has no schema file (CompatibilityMatrix §5 admits this). | District data not typed by Agent 1. Raw registries kept. |
| D6 | `Meridian README.md` status table marks most files “⏳ Pending” although they exist and validate. | Docs stale; data is authoritative. |
| D7 | Meridian pins **no** engine version (“UE5 generically”); project requires 5.8.2. | No conflict; `.uplugin` now `EngineVersion 5.8.0`. |
| D8 | `build_order_index` (authoring order) ≠ `generation_priority` ≠ `generation_order` steps. | Kept as distinct fields; never conflated. |
| D9 | Undercroft `radial_band = inner_mid_outer_continuous` is not a member of `world_scale_metadata.radial_bands`. | Composite by design; importer logs Info, not error. |
| D10 | Builder bug: `VoidRoadMeshBuilder.h` (public) includes `ProceduralMeshComponent.h`, but the dependency was **Private** and the `.uplugin` did not list the ProceduralMeshComponent plugin. | Fixed (Public dependency + `.uplugin` `Plugins` entry). |
| D11 | Builder bug: `VoidJsonReader.h` (public) includes Json headers but `Json` was a Private dependency. | Fixed (Public). |
| D12 | Legacy road mesh builder names its perpendicular “Right” but it points to Unreal’s **left** (heading +X → −Y). Cosmetic (sidewalks are symmetric). | Left as-is. New `IVoidRoadQuery` defines Left/Right in true Unreal terms. |
| D13 | Legacy `FVoidPackageValidator` requires `widthUnits > 0` while the generator falls back to profile width when 0. | Left as-is; Meridian roads use width 0 → profile. |
| D14 | The two ZIPs are named confusingly: `…-main.zip` = Builder, `…-master.zip` = Meridian data. | FYI. |

## 3. Files

### 3.1 Modified (existing files, additive unless noted)
- `VOIDWorldBuilder.uplugin` — v0.2.0, `EngineVersion: 5.8.0`, `Plugins: [ProceduralMeshComponent]`.
- `Core/VOIDWorldBuilderCore.Build.cs` — + `DeveloperSettings`.
- `Import/VOIDWorldBuilderImport.Build.cs` — `Json` → Public.
- `Generators/VOIDWorldBuilderGenerators.Build.cs` — `ProceduralMeshComponent` → Public, + `DeveloperSettings`.
- `Core/Public/Types/VoidWorldBuilderTypes.h` — + `EVoidGeneratorStatus`, `EVoidNetworkKind`, `EVoidRoadSource`.
- `Core/Public/Data/VoidDesignPackage.h` — + `FVoidRoadSpec::bClosedLoop` (default false).
- `Core/Public/Interfaces/IVoidGenerator.h` — extended (see §5). `Generate()` signature **unchanged**.
- `Import/Private/VoidJsonPackageReader.cpp` — reads `closedLoop`.
- `Import/Private/VoidPackageValidator.cpp` — + finite/range coordinate validation via `FVoidWorldSpace`.
- `Generators/Public|Private/Road/VoidRoadGenerator.*` — **refactored** (same mesh/spline/junction algorithms; see §7).
- `Generators/Private/Road/VoidRoadIntersectionBuilder.cpp` — closed loops excluded from endpoint clustering.
- `Editor/Private/VoidWorldBuilderCommandlet.cpp` — + `-Meridian=` mode.

### 3.2 Added
- Core: `Public/Data/VoidMeridianData.h`, `Public/Data/VoidRoadOutput.h` + `Private/Data/VoidRoadOutput.cpp`, `Public/Coordinates/VoidWorldSpace.h` + cpp, `Public/Settings/VoidWorldBuilderSettings.h` + cpp, `Public/Utilities/VoidSha256.h` + cpp, `Private/Tests/VoidFoundationTests.cpp`.
- Import: `Public/VoidMeridianImporter.h` + cpp, `Public/VoidJsonSchemaLite.h` + cpp, `Private/Tests/VoidMeridianImporterTests.cpp`.
- Generators: `Public/Road/VoidMeridianRoadPlanner.h` + cpp, `Public/VoidGeneratorPipeline.h` + cpp, `Private/Tests/VoidMeridianRoadPlannerTests.cpp`.
- `Tools/meridian_reference_check.py`.

No public class was renamed or removed. Nothing was deleted.

## 4. Meridian schema as implemented (all mapped from real files)

Import rules (`FVoidMeridianImporter::LoadFromMasterFile`, options: `bStrictCanonLock`, `bVerifyChecksums`):
1. Open **only** `Meridian_Master.json`; resolve everything else through `registry_references` in `generation_pipeline.import_order` (the package’s own rule).
2. `$schema` must be `void_meridian_master_schema_v1` (major ≠ 1 ⇒ Fatal). Each registry must declare `void_<key>_schema_v1`.
3. Missing/unparseable **required** registry ⇒ Fatal, halt (BuilderRules `missing_reference_error`).
4. Each file validated against its own `*.schema.json` (`FVoidJsonSchemaLite`: type, required, properties, additionalProperties:false, items, enum, const, min/maxItems, minimum — the exact subset these schemas use).
5. Road network: duplicate ids across categories/routes/tunnels/bridges; `road_hierarchy` entries exist and ascend by tier; route `category`, districts, `traverses_bands`/`radial_band` (vs `world_scale_metadata.radial_bands`), `generation_dependency` (district **or** route), tunnel (exactly 2 districts), traversal edges (`via` ∈ routes∪tunnels, nodes exist), `district_connectivity`, and `unreachable_pairs_by_design` vs edges (contradiction = Error).
6. `import_order` must satisfy `module_dependencies`; `generation_order.depends_on` must reference earlier steps.
7. **VR-012:** any numeric spatial/unit field (`x,y,z,coord*,position,transform,location,elevation,altitude,lat/long,unit(s),cm,meters,km,feet,width,radius` as a key token) or bare 2–3-number tuple ⇒ Error `VOID.Meridian.FabricatedSpatialData`.
8. **VR-011:** SHA-256 of every `PackageManifest.json` entry (`package_files`, `locked_inputs`). Mismatch: Error if strict, else Warning. Absent locked file: Warning.

Typed output (`FVoidMeridianWorld`): `Manifest`, `Districts[5]`, `RoadNetwork` (categories, routes, bridges, tunnels, traversal nodes/edges), plus `RawRegistries` (json text for the other 9 registries — loaded, schema-checked, **not yet typed**; Agents 2/3+ add typed mapping in the importer’s step list, no loader changes needed).

Real-package result (reference checker): 0 errors; warnings only for D3/D4 above.

## 5. Shared generator contract (`IVoidGenerator`, all additions have safe defaults)

```
GetGeneratorId()                      (existing)
Generate(FVoidDesignPackage&, Ctx)    (existing, pure virtual)
GetDisplayName()                      default = id
GetDependencies() -> TArray<FName>    default = none   ("Road" for anything that needs roads)
SupportsPackageInput()                default true
SupportsMeridianInput()               default false
GenerateFromMeridian(World, Ctx)      default = clean failure with an error message (never silent)
Reset(Ctx)                            default no-op; must destroy own actors / clear published data
```
`FVoidGenerationContext` additions: `const FVoidMeridianWorld* Meridian`, `FVoidWorldSpace WorldSpace`, `TSharedPtr<FVoidRoadNetworkOutput> RoadOutput`, `TMap<FName,FVoidGeneratorResult> Results`, `FVoidValidationReport Diagnostics`, `ReportWarning/Error/Fatal()`, `HasSucceeded(Id)`.
`FVoidGeneratorResult { Status(EVoidGeneratorStatus), Message, NumCreated, NumSkipped, ElapsedMilliseconds }`.

`FVoidGeneratorPipeline::RunWithMeridian / RunWithPackage(Ids, input, Ctx, Options)`: resolves ids via the registry, auto-includes dependencies, topologically sorts (deterministic), detects cycles/unknown ids (nothing runs), calls `Reset` then the generator, records per-generator results, **skips generators whose dependency did not succeed**, honours cancellation, optionally aborts on Fatal.

## 6. Coordinate system (single, central)

`FVoidWorldSpace` (Core) — built from `UVoidWorldBuilderSettings` (snapshot struct `FVoidWorldSpaceConfig`).
- Units 1 UU = 1 cm; axes +X forward, +Y right, +Z up (Unreal, left-handed). Double precision throughout.
- **Legacy packages:** `TryFromSource2D(pt, elev)` = `axis(pt) * SourceUnitsToUU + WorldOriginOffsetUU`. Defaults (scale 1, UnrealNative, offset 0) make it an **identity** for all existing packages. Optional `FlipY` for right-handed sources.
- **Meridian:** `TryFromPolar(radius, azimuthDeg, elevation)`; azimuth 0° = +X, increasing toward +Y; origin = the Spire (Meridian’s “radial and vertical origin anchor”) + `WorldOriginOffsetUU`.
- **Rejected, never altered:** non-finite values, or |X|,|Y|,|Z| > `MaxAbsCoordinateUU` (default 2,097,152 UU ≈ 21 km — a conservative single-precision-safe limit, raise deliberately). Generators must not convert coordinates themselves.
- Elevation: Meridian has none; vertical placement is Z = `WorldOriginOffsetUU.Z`; legacy uses `elevationUnits`.
- Precision: Meridian has no numbers to round. Legacy floats (`FVoidRoadSpec`) are widened to double on entry.

## 7. Road Generator (upgraded, not replaced)

Same spline/ribbon/junction/bridge/tunnel algorithms. Changes: shared `BuildRoads()` core for both inputs; legacy points converted via `WorldSpace`; `GenerateFromMeridian` via `FVoidMeridianRoadPlanner`; closed-loop support; per-actor local origin (**large-world safety**: each road/junction actor sits at its own bounding-box centre with geometry relative to it, so float32 vertex buffers never hold large coordinates); actor tags (`VOID.Generated`, `VOID.Generator.Road`, `VOID.Road.<id>`, `VOID.District.<id>`); `Reset()`; publishes `FVoidRoadNetworkOutput`; errors go to `Context.Diagnostics`.

**Meridian → roads (`FVoidMeridianRoadPlanner`)**

| Meridian | Result |
|---|---|
| `radial_arterial`, Live (`spire_radial_spine`) | Straight radial centerline from inner edge of its first band to outer edge of its last band, at the route’s azimuth (`RouteAzimuthDegrees` or `DefaultRadialAzimuthDegrees`). Type Primary (tier 1). |
| `ring_road`, Live (`mid_tier_ring_road`) | Closed circle at the **middle of its band**, `RingRoadSegments` vertices, vertex 0 on the azimuth of the route it depends on. Type Secondary (tier 2). Hard constraint `must_never_route_through_metro_archives_or_undercroft` is enforced against district bands. |
| spine × ring | Analytic **Crossing** intersection (pad + record) at (ring radius, spine azimuth); lies exactly on a ring vertex. |
| `service_maintenance_route`, Dead | Topology-only record, `bHasGeometry=false`. |
| tunnels | Topology-only records. |
| skybridge | Not a road; skipped (flagged) / reported. |

Not produced for Meridian roads: width (Builder default profile; `bWidthIsBuilderDefault=true`), sidewalks, curbs, medians, crosswalks, lane counts. Builder-chosen, placeholder: band radii, azimuths, segment counts, sample spacing.

## 8. Road output contract (query via `Context.RoadOutput`, type `IVoidRoadQuery`)

```
GetRoads(), GetIntersections(), FindRoad(id)
GetRoadsByType(EVoidRoadType), GetRoadsByTier(int), GetRoadsServingDistrict(districtId)
SampleRoad(id, distance, out{Position, Direction(unit tangent), DistanceAlong})   // clamps open roads, wraps loops
FindNearestRoadPoint(worldPos, out sample, out distance2D)
GetSidewalkBoundary(id, Left|Right, outInnerEdge, outOuterEdge)                    // false if the road has no sidewalk
GetIntersectionsForRoad(id), FindNearestIntersection(pos, maxDist2D)
```
`FVoidRoadRecord`: RoadId, Source (Legacy/MeridianDerived), RoadType, HierarchyTier (Meridian tier; legacy: Highway/Primary/Roundabout 1, Secondary 2, Local 3, Service/Alley 4), Network (Live/Dead), CategoryId, WidthUU (+`bWidthIsBuilderDefault`), LaneCount, flags (closed/bridge/tunnel/sidewalk/median/`bHasGeometry`), sidewalk inner/outer offsets from centerline, world-space `Centerline`, `ServedDistrictIds`, `ConnectedRoadIds`, weak `Actor`. `FVoidIntersectionRecord`: id, location, kind (DeadEnd/CulDeSac/TwoWayJoin/TJunction/FourWayJunction/Complex/RoundaboutSpur/**Crossing**), road ids, pad radius. Left/Right are true Unreal sense (heading +X, Left = −Y).

## 9. Build dependencies (UE 5.8.2)

- Core (Runtime): Core, CoreUObject, Engine, **DeveloperSettings**.
- Import (Editor): + Core module, **Json** (Public), DeveloperSettings.
- Generators (Editor): Core, CoreUObject, Engine, VOIDWorldBuilderCore, VOIDWorldBuilderImport, DeveloperSettings, **ProceduralMeshComponent (Public)**; Private: UnrealEd.
- Editor (Editor): unchanged.
- `.uplugin` must enable the **ProceduralMeshComponent** plugin. **Confirm it is still present/supported in 5.8.2** (I could not check); if it was removed or replaced, `VoidRoadMeshBuilder` is the only place that needs re-pointing.
- I deliberately did not use any API I could not confirm in 5.8.2. All UE calls used are long-standing (`FJsonObject`, `FPaths`, `FFileHelper`, `GetDefault<UDeveloperSettings>`, `SpawnActor`, `FScopedTransaction`, `UProceduralMeshComponent`).

## 10. How to use

```
# headless validation of the real package (strict canon lock on by default)
UnrealEditor-Cmd.exe <proj>.uproject -run=VoidWorldBuilder -Meridian="<...>/Meridian Master" [-NoStrictCanonLock] [-PlanRoads]
# legacy
... -run=VoidWorldBuilder -Package="<design package.json>"
# tests
... -ExecCmds="Automation RunTests VOID.WorldBuilder; Quit" -VoidMeridianPath="<...>/Meridian Master"
# independent cross-check (no Unreal needed)
python3 Tools/meridian_reference_check.py "<...>/Meridian Master" --strict-canon-lock
```
(`-run=` name follows the existing commandlet class `UVoidWorldBuilderCommandlet`.)

Code:
```cpp
FVoidMeridianImportResult R = FVoidMeridianImporter::LoadFromPath(Path);
if (!R.WasSuccessful()) { /* R.ValidationReport.Issues */ return; }
FVoidGenerationContext Ctx; Ctx.TargetWorld = World;
FVoidGeneratorPipeline::RunWithMeridian({ "Road" }, R.World, Ctx);
const IVoidRoadQuery& Roads = *Ctx.RoadOutput;
```

## 11. Known issues / most likely first-compile friction

1. **Not compiled** (§0.1). Likely small items: `TestEqual` overloads, `FString::JoinBy`, `TJsonReader` usage in tests, UHT on `FVoidBandRadius` `Config` properties, `TMap<FName,double>` config property, `UENUM` placement.
2. ProceduralMeshComponent plugin availability in 5.8.2 (§9).
3. Road mesh triangle winding was analysed by hand (front face up) but never rendered.
4. `FVoidWorldSpace WorldSpace = FromSettings()` is initialised in every `FVoidGenerationContext` constructor (touches the settings CDO). Fine in editor/commandlet; do not construct contexts during static init.
5. Ring-road chord sag: with 96 segments at 75,000 UU ≈ 40 UU. Raise `RingRoadSegments` for smoother rings.
6. Placeholder layout scale is **not** design-approved. The warning `VOID.RoadPlan.PlaceholderLayout` fires on every Meridian run by design.
7. The Slate editor panel remains legacy-package-only (I did not add Meridian UI without being able to compile Slate). Meridian runs via commandlet / API / pipeline.
8. `Generate()` for legacy packages still uses `FVoidRoadValidator`; its results still populate `GenerationValidationReport` (not `Diagnostics`).
9. Only the Road Generator implements `GenerateFromMeridian`. Undo: road spawn is wrapped in `FScopedTransaction`; `Reset()` uses `Destroy()`.
10. Tunnels have no geometry anywhere for Meridian (topology only); legacy tunnel portals unchanged.

## 12. Integration instructions for Agents 2 and 3

- **Do not** parse Meridian JSON. Take `const FVoidMeridianWorld&` (districts, road network, flagged items, raw registries) and `Context.RoadOutput`.
- **Register** your generator in `FVOIDWorldBuilderGeneratorsModule::StartupModule/ShutdownModule` (one `RegisterGenerator`/`UnregisterGenerator` line each — the only shared-file edit you need).
- **Declare** `GetDependencies()` (`{"Road"}` for Building/District/Environment), override `SupportsMeridianInput()` + `GenerateFromMeridian()`, implement `Reset()`.
- **Never convert coordinates yourself**: use `Context.WorldSpace`, and read positions from `IVoidRoadQuery` (world space already). Mark Builder-derived placement as such, as the Road planner does.
- **Respect Meridian rules**: skip content tied to `World.HasFlaggedOpenItem(...)` unless `bAllowFlaggedContent`; obey district `DependsOn` and `Manifest.GenerationOrder`.
- **Typing more registries**: add a mapping block in `FVoidMeridianImporter::LoadFromMasterFile` after the road-network block and a struct in `VoidMeridianData.h` (additive). Raw JSON is already in `World.RawRegistries`.
- **Report problems** with `Context.ReportWarning/Error/Fatal` (feeds `Diagnostics`); the pipeline uses them to decide skip/abort.
- **Tests**: copy the patterns in `VoidMeridianRoadPlannerTests.cpp` (pure-function planner tests need no world).
- Phases 4+ (Building, District…) are **not** started here.
