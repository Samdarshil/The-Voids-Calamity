# INTEGRATION RISK REGISTER - VOID World Builder (Agent 3)

Target: Unreal Engine 5.8.2. Base: Plugins phase 3 (Road Generator). Audit date: 2026-10-01.

Status vocabulary: **FIXED** (changed here) - **MITIGATED** (risk reduced, residual noted) - **DETECTED** (validation reports it; fix belongs to someone else) - **OPEN** (needs a decision/change) - **UNVERIFIED** (cannot be confirmed without UE 5.8.2) - **ACCEPTED** - **INFO**.

| ID | Risk | Status |
|---|---|---|
| R-01 | Meridian_Master cannot feed the Builder: no geometry source exists | OPEN |
| R-02 | Identifier namespaces do not line up (districts vs generator targets vs package ids) | MITIGATED |
| R-03 | Supplied Meridian package is incomplete: 14 locked input files missing (4 of 5 districts) | OPEN |
| R-04 | PackageManifest.json is stale and does not inventory 7 shipped files | DETECTED |
| R-05 | `district` is a polymorphic field in GameplayGraph | MITIGATED |
| R-06 | Two different 'order' fields and two different granularities of ordering | MITIGATED |
| R-07 | Road Generator does not tag the actors it spawns | MITIGATED |
| R-08 | Road generation is not idempotent: running it twice duplicates every road | DETECTED |
| R-09 | Editor panel bypasses validation and misreads results | OPEN |
| R-10 | One shared report slot: Context.GenerationValidationReport is overwritten | MITIGATED |
| R-11 | Generator registry: silent overwrite and unspecified iteration order | MITIGATED |
| R-12 | `bIsValid` is unreliable on legacy report paths | MITIGATED |
| R-13 | Plugin descriptor: missing plugin dependency and stale engine version | FIXED |
| R-14 | Nothing here has been compiled against Unreal Engine 5.8.2 | UNVERIFIED |
| R-15 | Coordinate space mismatch: 'district-local' design data is spawned at world identity | OPEN |
| R-16 | Merge hotspots: files this agent modified that other agents may also edit | OPEN |
| R-17 | Dependency direction and cycles | MITIGATED |
| R-18 | Duplicate classes / naming collisions between agents | OPEN |
| R-19 | Generators cannot declare stage or dependencies | OPEN |
| R-20 | Junction builder only clusters endpoints: T-junctions and mid-road crossings get no junction | DETECTED |
| R-21 | Checksum verification vs line endings | MITIGATED |
| R-22 | Validation limitations (what it cannot see) | ACCEPTED |
| R-23 | Project-specific: two source snapshots and layout in the supplied ZIPs | INFO |
| R-24 | Road Generator's own junction clustering is O(N^2) in endpoints | DETECTED |

---

## R-01 - Meridian_Master cannot feed the Builder: no geometry source exists

**RISK:** Meridian_Master cannot feed the Builder: no geometry source exists

**LOCATION:** Meridian Master/RoadNetwork.json, Meridian_Master.json `builder_configuration.coordinate_policy` vs Source/VOIDWorldBuilderCore/Public/Data/VoidDesignPackage.h (`FVoidRoadSpec::CenterlinePoints`, `FVoidBuildingSpec::FootprintCorners`), Source/.../Import/VoidJsonPackageReader.cpp

**WHY IT MATTERS:** Meridian is a coordinate-free relational graph (routes, edges, districts, cells) and explicitly forbids literal coordinates. The Builder's importer only reads the single-district `schemaVersion/metadata/district{roads,buildings}` format, and the Road Generator returns *false* when a package has no roads. There is no adapter and no layout stage. Fed Meridian today the Builder imports nothing; fed a hand-made package it is unrelated to Meridian's ids.

**AFFECTED SYSTEM:** Import, RoadGenerator, every later generator (all need geometry), Metro

**RECOMMENDED FIX:** Decide and assign ownership of a Meridian -> FVoidDesignPackage 'layout' step that synthesises centerlines/footprints deterministically from the graph (seeded, so regeneration is stable). Until then the pipeline reports `VOID.Meridian.NoGeometrySource` (Info) and `VOID.Road.NoRoads` (Warning).

**STATUS:** OPEN - highest-priority integration decision; not solvable by validation

## R-02 - Identifier namespaces do not line up (districts vs generator targets vs package ids)

**RISK:** Identifier namespaces do not line up (districts vs generator targets vs package ids)

**LOCATION:** Meridian_Master.json `generation_pipeline.generation_order[].target` (e.g. `live_network_spine`, `white_zones_network_nodes`, `undercroft_substrate`) vs DistrictRegistry ids (`olympus_spire`...) vs Builder generator ids (`Road`, ...) vs `FVoidDistrictData::DistrictId`; district data files use `location_id`, the Builder uses `districtId`

**WHY IT MATTERS:** Three different id namespaces describe 'what to build next'. One `FVoidDesignPackage` holds exactly one district, Meridian has five plus shared infrastructure (live network spine) that belongs to no single district. Anything that maps by string equality will silently miss.

**AFFECTED SYSTEM:** Import, pipeline orchestration, District generator, Metro

**RECOMMENDED FIX:** Keep the mapping in one table owned by the layout step (R-01). Validation already rejects a package district id that is not in the registry (`VOID.Data.UnknownDistrict`) and extracts `GenerationTargets` separately from `DistrictIds` (`FVoidMeridianSummary`).

**STATUS:** MITIGATED (detected + documented); mapping table OPEN

## R-03 - Supplied Meridian package is incomplete: 14 locked input files missing (4 of 5 districts)

**RISK:** Supplied Meridian package is incomplete: 14 locked input files missing (4 of 5 districts)

**LOCATION:** Meridian Master/ (present: Metro_Archives*.md/json; absent: Olympus_Spire*, Sector_0*, White_Zones*, VOID_Location_01_*Undercroft*)

**WHY IT MATTERS:** PackageManifest lists them as locked inputs; they are not in the zip. VR-004 (sub-location coverage) and VR-011 cannot be verified for those districts and none of them can be generated. Strict canon-lock mode refuses to run, correctly.

**AFFECTED SYSTEM:** Import, District/Building generators, WorldPartition validation

**RECOMMENDED FIX:** Supply the 14 files. For partial dev runs use `-AllowMissingDistricts` / `bAllowMissingDistrictPackages` (downgrades to Warning; must not be used for the final integration run).

**STATUS:** OPEN (external input). Reported as 14x `VOID.Meridian.LockedInputMissing` ERROR

## R-04 - PackageManifest.json is stale and does not inventory 7 shipped files

**RISK:** PackageManifest.json is stale and does not inventory 7 shipped files

**LOCATION:** Meridian Master/PackageManifest.json (`totals.total_files` = 46; `package_files` = 27 entries)

**WHY IT MATTERS:** Manifest predates CompatibilityMatrix.md, DependencyGraph.json, EngineeringHandoff.md, Meridian_Master_Index.md, PackageManifest.json (self), ValidationReport.md, ValidationSchema.json, so those are outside checksum protection and totals are wrong.

**AFFECTED SYSTEM:** Import integrity (VR-011)

**RECOMMENDED FIX:** Regenerate the manifest so it inventories every file including itself (or document the self-exclusion).

**STATUS:** DETECTED (`VOID.Meridian.UnlistedFile`, `ManifestTotalsStale` Warnings); fix OPEN (data owner)

## R-05 - `district` is a polymorphic field in GameplayGraph

**RISK:** `district` is a polymorphic field in GameplayGraph

**LOCATION:** Meridian Master/GameplayGraph.json `mission_relationships[*].district` (8 of 24 values are sub-locations/transitions, e.g. `the_core_gate`, `undercroft_to_sector_0`)

**WHY IT MATTERS:** A generic 'every `district` value must be a district id' check produces 8 false errors; a check that ignores the field would miss real typos.

**AFFECTED SYSTEM:** Gameplay data consumers, reference validation

**RECOMMENDED FIX:** Split into `district` + `location` in the next schema revision. Until then the validator special-cases this one path and downgrades to Warning.

**STATUS:** MITIGATED

## R-06 - Two different 'order' fields and two different granularities of ordering

**RISK:** Two different 'order' fields and two different granularities of ordering

**LOCATION:** DistrictRegistry.json `build_order_index` (authoring order) vs `generation_priority`; Meridian_Master.json `generation_order` (module/step level) vs the Builder pipeline (generator level: Road, District, Building...)

**WHY IT MATTERS:** Using the wrong field regenerates districts in the wrong order (e.g. Olympus Spire is build-index 5 but generation step 1). The brief's flow puts Metro last as a *generator*, while Meridian orders *districts/modules* (metro_archives is step 5 of 6). These are orthogonal axes; the orchestrator must iterate one inside the other.

**AFFECTED SYSTEM:** Orchestrator (Phase 10), District, Metro, Environment

**RECOMMENDED FIX:** Orchestrator outer loop = Meridian `generation_order`; inner loop = `FVoidPipelineDefinition`. Never read `build_order_index` for generation.

**STATUS:** MITIGATED (`VOID.Meridian.AmbiguousOrderFields` Info, generation order checked for dependency-consistency); orchestration OPEN

## R-07 - Road Generator does not tag the actors it spawns

**RISK:** Road Generator does not tag the actors it spawns

**LOCATION:** Source/VOIDWorldBuilderGenerators/Private/Road/VoidRoadGenerator.cpp (SpawnActor sites ~L155, ~L255)

**WHY IT MATTERS:** Without a generated-actor marker, tooling cannot attribute, de-duplicate or clean up generator output, and every later generator will repeat the same gap.

**AFFECTED SYSTEM:** World validation, cleanup/regeneration, all generators

**RECOMMENDED FIX:** Add `FVoidGeneratedTags::Apply(Actor, TEXT("Road"), RoadId)` after each SpawnActor (2 lines; not applied - Road Generator is not mine to rewrite). Every new generator should do the same.

**STATUS:** MITIGATED via explicit adapter in `FVoidGeneratedWorldValidator::BuildSnapshot` (recognises AVoidRoadActor/AVoidRoadJunctionActor); recommended patch OPEN

## R-08 - Road generation is not idempotent: running it twice duplicates every road

**RISK:** Road generation is not idempotent: running it twice duplicates every road

**LOCATION:** VoidRoadGenerator.cpp `MakeUniqueObjectName`; Editor/SVoidWorldBuilderPanel.cpp `OnGenerateRoadsClicked` (button stays enabled after success)

**WHY IT MATTERS:** Each click or pipeline re-run spawns a second full set of road/junction actors (unique names hide the collision). Overlapping duplicate road surfaces z-fight, and junctions double.

**AFFECTED SYSTEM:** RoadGenerator, Editor panel, pipeline re-runs

**RECOMMENDED FIX:** Generators should delete their previous output (found via `FVoidGeneratedTags`) inside an undo transaction before spawning, or the orchestrator should own cleanup.

**STATUS:** DETECTED (`VOID.World.DuplicateActor` ERROR at PostRoad/Final); cleanup OPEN

## R-09 - Editor panel bypasses validation and misreads results

**RISK:** Editor panel bypasses validation and misreads results

**LOCATION:** Source/VOIDWorldBuilderEditor/Private/SVoidWorldBuilderPanel.cpp `OnGenerateRoadsClicked` (~L269-330)

**WHY IT MATTERS:** It calls the Road generator directly, so none of the new data/road/world validation or pipeline gating runs from the UI; it counts `NumErrors()` only (a Fatal is invisible to the counter); it shows only the generator's own report; it would not show subsystem/object/location. The panel is exactly where a human would look.

**AFFECTED SYSTEM:** Editor UI, user-visible diagnostics

**RECOMMENDED FIX:** Replace the direct call with `FVoidValidationPipeline::RunFromImport` and bind the list to `Result.Report` sorted by `FVoidValidationReportExporter::GetSortedIssues`; use `NumBlocking()` for counters.

**STATUS:** OPEN (Editor module deliberately untouched)

## R-10 - One shared report slot: Context.GenerationValidationReport is overwritten

**RISK:** One shared report slot: Context.GenerationValidationReport is overwritten

**LOCATION:** VoidRoadGenerator.cpp L86 (`Context.GenerationValidationReport = FVoidRoadValidator::Validate(District);`) and the `FVoidGenerationContext` design in Core/Public/Interfaces/IVoidGenerator.h

**WHY IT MATTERS:** With seven generators sharing one context the last writer erases every earlier generator's findings - failures would simply vanish from the final report.

**AFFECTED SYSTEM:** All generators, reporting

**RECOMMENDED FIX:** Pipeline gives each generator its own context and merges its report (attributed, de-duplicated) right after it returns. Longer term: change the contract to *append*, and let the pipeline own the aggregate.

**STATUS:** MITIGATED inside the pipeline (tested with fakes that overwrite the slot); direct callers remain exposed

## R-11 - Generator registry: silent overwrite and unspecified iteration order

**RISK:** Generator registry: silent overwrite and unspecified iteration order

**LOCATION:** Source/VOIDWorldBuilderGenerators/{Public,Private}/VoidGeneratorRegistry.*

**WHY IT MATTERS:** `RegisterGenerator` overwrites on a duplicate id with only a log warning (two agents both registering `Building` => one silently dies). ids are FNames, compared case-insensitively. `GetAllGenerators()` iterates a TMap, so any consumer that relies on its order is non-deterministic.

**AFFECTED SYSTEM:** Registry, all generator modules, orchestrator

**RECOMMENDED FIX:** Additive change made: collisions are recorded (`GetCollidedIds`) and `GetRegisteredIdsSorted` added; behaviour of existing calls unchanged. Auditor reports `VOID.Contract.DuplicateGeneratorId` (ERROR). Longer term: make RegisterGenerator return bool and reject.

**STATUS:** MITIGATED

## R-12 - `bIsValid` is unreliable on legacy report paths

**RISK:** `bIsValid` is unreliable on legacy report paths

**LOCATION:** Core/Public/Data/VoidValidationReport.h (`AddError`/`AddFatal` do not touch `bIsValid`; default is `false`); FVoidGenerationContext comment says default report is 'empty, bIsValid = false'

**WHY IT MATTERS:** A report containing Fatal issues can say `bIsValid == true`; an empty untouched report says `false`. Consumers that trust the flag mis-decide.

**AFFECTED SYSTEM:** Every consumer of a report

**RECOMMENDED FIX:** Decide blocking by `NumBlocking()`/`IsBlocking()`; pipeline recomputes validity after every merge. New `AddIssue` keeps the flag consistent.

**STATUS:** MITIGATED in new code; legacy helpers unchanged (by design - not rewritten)

## R-13 - Plugin descriptor: missing plugin dependency and stale engine version

**RISK:** Plugin descriptor: missing plugin dependency and stale engine version

**LOCATION:** VOIDWorldBuilder.uplugin (`EngineVersion` 5.6.0; no `Plugins` entry) while Generators.Build.cs links `ProceduralMeshComponent`

**WHY IT MATTERS:** ProceduralMeshComponent ships as an engine *plugin*; linking its module without declaring the plugin fails (or depends on the host project happening to enable it). EngineVersion 5.6.0 misstates the target.

**AFFECTED SYSTEM:** Build/startup of Generators and Validation

**RECOMMENDED FIX:** Applied: `EngineVersion` -> `5.8.0`; added `Plugins: [{ProceduralMeshComponent, Enabled}]`; registered the new module. Confirm the plugin still ships in 5.8.2 (not verifiable here); if it was removed/renamed, the road meshes need a different component.

**STATUS:** FIXED in descriptor; existence of the plugin in 5.8.2 UNVERIFIED

## R-14 - Nothing here has been compiled against Unreal Engine 5.8.2

**RISK:** Nothing here has been compiled against Unreal Engine 5.8.2

**LOCATION:** All new C++ (Core additions, Validation module, tests)

**WHY IT MATTERS:** No engine install or network was available. Verification performed: static lint (braces, include resolution, module dependencies/cycles, UHT ordering, API macros, test guards), standalone g++ -Wall -Wextra -Werror build + run of the engine-independent algorithms (SHA-256 NIST vectors, geometry), Python oracle run on real Meridian data with mutation tests. NOT verified: UBT compilation, UHT, module/plugin load, automation test execution. APIs most worth a first look if UBT complains: `TArray::SetNum(int32, EAllowShrinking)`, `TJsonReader::GetErrorMessage/GetLineNumber`, `FRegexPattern/FRegexMatcher`, `UProceduralMeshComponent::GetNumSections`, `FBox::IsValid`, `FPlatformTime::Seconds`, `AddExpectedError/Warning`.

**AFFECTED SYSTEM:** Everything new

**RECOMMENDED FIX:** Run the first build, UHT, module-load and `Automation RunTests VOID.WorldBuilder` in a 5.8.2 project; send compiler output to whoever owns the failing file.

**STATUS:** UNVERIFIED

## R-15 - Coordinate space mismatch: 'district-local' design data is spawned at world identity

**RISK:** Coordinate space mismatch: 'district-local' design data is spawned at world identity

**LOCATION:** Core/Public/Data/VoidDesignPackage.h comments (district-local space) vs VoidRoadGenerator.cpp (`SpawnActor(..., FTransform::Identity)` with points used as world positions)

**WHY IT MATTERS:** With five Meridian districts every district's local (0,0) lands on the world origin - districts stack on top of each other. All generators must agree on the same mapping or roads and buildings from the same district will be offset from one another.

**AFFECTED SYSTEM:** Road, Building, District, Metro, WorldPartition, Environment

**RECOMMENDED FIX:** Define one `DistrictOrigin` (world transform) carried in the package/context and applied by *every* generator (Road currently ignores it). Validation uses design-space values and bounds them (`MaxCoordinateAbs`); it does not translate.

**STATUS:** OPEN - needs a cross-agent decision

## R-16 - Merge hotspots: files this agent modified that other agents may also edit

**RISK:** Merge hotspots: files this agent modified that other agents may also edit

**LOCATION:** Core/Public/Data/VoidValidationReport.h; Generators/{Public,Private}/VoidGeneratorRegistry.*; VOIDWorldBuilder.uplugin; Core/Private/Tests (new file only)

**WHY IT MATTERS:** Six other agents edit the same plugin. These are the only pre-existing files touched. All edits are additive (new fields with defaults, new methods, one new module entry).

**AFFECTED SYSTEM:** Integration merge

**RECOMMENDED FIX:** Take Agent 3's version of VoidValidationReport.h and merge any other additions into it; union the `.uplugin` Modules/Plugins lists; see HANDOFF_AGENT3.md section 'Files changed' for the exact hunks.

**STATUS:** OPEN (merge coordination)

## R-17 - Dependency direction and cycles

**RISK:** Dependency direction and cycles

**LOCATION:** VOIDWorldBuilderValidation.Build.cs

**WHY IT MATTERS:** Validation depends on Generators (registry, Road actor classes). If any generator module ever depends on Validation the plugin no longer links (Generators <-> Validation cycle).

**AFFECTED SYSTEM:** Module graph

**RECOMMENDED FIX:** Generators register validators through Core's `FVoidValidatorRegistry`; never reference the Validation module. `Tools/static_lint.py` fails on cycles.

**STATUS:** MITIGATED (lint-enforced)

## R-18 - Duplicate classes / naming collisions between agents

**RISK:** Duplicate classes / naming collisions between agents

**LOCATION:** New public symbols: FVoidValidationContext, FVoidValidationOptions, FVoidValidationInput, IVoidValidator, FVoidValidatorRegistry, EVoidValidationStage, FVoidGeneratedTags, FVoidActorSnapshot, FVoidWorldSnapshot, FVoidPipelineDefinition/Step/Options/Result, FVoidValidationPipeline, FVoidValidationReportExporter, FVoidGeneratorContractAuditor, FVoidMeridianValidator/FileSet/Summary, FVoidJsonSchemaSubset, UVoidValidateCommandlet, LogVoidValidation; code prefixes `VOID.Meridian.*`, `VOID.Data.*`, `VOID.Road.*`, `VOID.Building.*`, `VOID.World.*`, `VOID.Pipeline.*`, `VOID.Contract.*`, `VOID.Schema.*`

**WHY IT MATTERS:** Another agent independently inventing a 'validation context', 'pipeline' or 'report exporter' would create duplicate types (ODR/UHT errors) or split the diagnostics. `VOID.RoadGen.*` belongs to Phase 3 and `VOID.Import.*` to Phase 2 - a Building agent using `VOID.Building.*` will collide with this table.

**AFFECTED SYSTEM:** All agents

**RECOMMENDED FIX:** Building/District/etc. agents: reuse these types and prefix your own codes `VOID.<YourArea>.` with a *different* second segment, or coordinate before using `VOID.Building.*` (this module already emits 11 codes there for data-level footprint checks).

**STATUS:** OPEN (WATCH at merge; compile will expose real duplicates)

## R-19 - Generators cannot declare stage or dependencies

**RISK:** Generators cannot declare stage or dependencies

**LOCATION:** Core/Public/Interfaces/IVoidGenerator.h (only `GetGeneratorId` + `Generate`)

**WHY IT MATTERS:** Order and canonical ids live in the pipeline definition and `FVoidGeneratorContractAuditor::GetExpectedGeneratorIds` instead of with the generator. A generator with a non-canonical id is simply never run by the default pipeline (auditor warns).

**AFFECTED SYSTEM:** Pipeline, all generators

**RECOMMENDED FIX:** Optionally add a defaulted `virtual FVoidGeneratorDescriptor Describe() const` (id, stage, depends-on) later. Not done: it would touch a contract every agent implements.

**STATUS:** OPEN (design note; workaround in place)

## R-20 - Junction builder only clusters endpoints: T-junctions and mid-road crossings get no junction

**RISK:** Junction builder only clusters endpoints: T-junctions and mid-road crossings get no junction

**LOCATION:** Generators/Private/Road/VoidRoadIntersectionBuilder.cpp (endpoint clustering)

**WHY IT MATTERS:** A side street ending on the middle of another road produces no junction pad or crosswalk; two roads crossing at grade overlap and z-fight. Data that looks fine renders wrong.

**AFFECTED SYSTEM:** RoadGenerator output quality

**RECOMMENDED FIX:** Split roads at T-points/crossings in the layout step (R-01). Validation flags both (`VOID.Road.UnsplitTJunction`, `VOID.Road.CrossingWithoutJunction`) using spatial hashing.

**STATUS:** DETECTED

## R-21 - Checksum verification vs line endings

**RISK:** Checksum verification vs line endings

**LOCATION:** PackageManifest sha256 values vs a git checkout on Windows (`core.autocrlf`)

**WHY IT MATTERS:** CRLF conversion changes every hash; a naive checker would report all files tampered and block generation.

**AFFECTED SYSTEM:** Import integrity

**RECOMMENDED FIX:** Validator compares LF-normalised bytes on mismatch and downgrades to `VOID.Meridian.LineEndingChecksum` Warning. Add `* text eol=lf` for the package in .gitattributes.

**STATUS:** MITIGATED

## R-22 - Validation limitations (what it cannot see)

**RISK:** Validation limitations (what it cannot see)

**LOCATION:** FVoidRoadNetworkValidator / FVoidBuildingHookValidator

**WHY IT MATTERS:** Roundabout rings are excluded from crossing tests (only spur-to-ring connectivity is checked). Building 'floating' checks need generated bounds (world stage) - package data has no terrain. Meridian Live vs Dead network separation will surface as `DisconnectedNetwork` Warnings even though it is intentional. VR-006/VR-008 are narrative/canon rules and not automatable.

**AFFECTED SYSTEM:** Reviewers reading the report

**RECOMMENDED FIX:** Treat those Warnings as reviewed, not blocking. Extend validators if a future spec adds terrain, ring geometry or explicit network ids.

**STATUS:** ACCEPTED

## R-23 - Project-specific: two source snapshots and layout in the supplied ZIPs

**RISK:** Project-specific: two source snapshots and layout in the supplied ZIPs

**LOCATION:** The-Voids-Calamity-main.zip (phase1/phase2/phase3 snapshot folders, phase 2 folder named `VOIDWorldBuilder 2`) and -master.zip (Meridian Master)

**WHY IT MATTERS:** The zips hold three historical plugin snapshots. Building from the wrong one silently drops the Road Generator.

**AFFECTED SYSTEM:** Integration

**RECOMMENDED FIX:** This deliverable is based on `Plugins phase 3/VOIDWorldBuilder` (the latest, with RoadGenerator). Confirm the other six agents used the same base.

**STATUS:** INFO

## R-24 - Road Generator's own junction clustering is O(N^2) in endpoints

**RISK:** Road Generator's junction clustering is an all-pairs loop over endpoints (and `FindRoadEndpointIndices` is a linear scan per call).

**LOCATION:** Source/VOIDWorldBuilderGenerators/Private/Road/VoidRoadIntersectionBuilder.cpp `BuildJunctionGraph` (Pass 1: `for A ... for B = A + 1 ...`, ~L81-86; lookup lambda ~L93-97)

**WHY IT MATTERS:** ~10,000 roads = ~20,000 endpoints = ~2x10^8 distance checks for clustering alone, and worse if the spur pass repeats linear scans. Fine for the Phase 3 test packages, a stall or editor freeze at Meridian scale. The validators added here avoid this pattern (spatial hash + union-find), which is exactly why they can run first and cheaply - but generation itself would still be the bottleneck.

**AFFECTED SYSTEM:** RoadGenerator runtime on large districts; any pipeline timing budget.

**RECOMMENDED FIX:** Bucket endpoints in a uniform grid (cell = tolerance) and compare only against the 3x3 neighbourhood; build an endpoint index per road once instead of scanning. `FSpatialHash2D` in `VoidPackageValidators.cpp` is a working reference (move to Core if the Road owner wants to share it). Not changed here: the brief forbids rewriting the Road Generator.

**STATUS:** DETECTED (by code audit; not measured - no engine available). Fix OPEN (Road Generator owner).

