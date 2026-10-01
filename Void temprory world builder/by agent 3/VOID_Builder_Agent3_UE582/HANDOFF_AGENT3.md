# HANDOFF - Agent 3: Validation + Cross-Generator Integration Hardening

Target: **Unreal Engine 5.8.2**. Base: `Plugins phase 3/VOIDWorldBuilder` (the only snapshot with the Road Generator).
Deliverable: `Plugins/VOIDWorldBuilder/` (drop-in replacement of that plugin folder), plus `Tools/`, `Reports/`,
`INTEGRATION_RISK_REGISTER.md` (24 risks).

## 0. Read this first - verification status (honest)

| Claim | Status |
|---|---|
| Compiles / passes UHT on UE 5.8.2 | **NOT VERIFIED.** No engine or network in the build environment. |
| Plugin / module loads, automation tests executed | **NOT VERIFIED.** 20 new automation tests are written, not run. |
| Static consistency (braces, includes resolve, Build.cs deps, module cycles, `.generated.h` last, API macros, test guards) | **Verified** - `Tools/static_lint.py`: clean (and proven to catch seeded defects). |
| Engine-independent algorithms (SHA-256, geometry) | **Verified** - compiled `g++ -std=c++17 -Wall -Wextra -Werror` and run; NIST SHA-256 vectors incl. 1,000,000 x 'a'; crossing/overlap/winding/self-intersection cases. |
| Meridian rules on real data | **Verified via Python oracle** (`Tools/meridian_reference_validator.py`): 14 Error / 10 Warning / 12 Info, all errors = missing locked files. 15 seeded defects all detected. C++ tests were cross-checked against this oracle (same fixture, same mutations, same codes). |
| Road/building/world validator logic | Hand-verified test geometry (areas, angles, intersection points recomputed numerically); **C++ not executed**. One test-data bug (zero-area "bow-tie") was found this way and fixed. |

**First thing to do on integration: build, then `Automation RunTests VOID.WorldBuilder`.** If UBT complains, the APIs most
likely to need a touch-up for 5.8.2 are listed in risk R-14.

## 1. Validation architecture

See `Plugins/VOIDWorldBuilder/Docs/ValidationArchitecture.md` (diagram, pipeline, "add a validator" recipe). In brief:

- **Core** (additive): `IVoidValidator`, `FVoidValidatorRegistry` (rejects duplicate ids, deterministic order), `FVoidValidationContext`
  (stamps subsystem/source, throttles per code), `FVoidValidationOptions`, `EVoidValidationStage`, `FVoidGeneratedTags`;
  `FVoidValidationIssue` gains Subsystem / ObjectId / Source / Location; `FVoidValidationReport` gains `AddIssue`, `Merge`,
  `NumBlocking`, `RecomputeValidity`, `SeverityToString`.
- **New module `VOIDWorldBuilderValidation`** (Editor, leaf - nothing may depend on it): Meridian validator + schema-subset
  checker + SHA-256, package Data / Road / Building validators, generated-world validator (snapshot based), contract auditor,
  pipeline runner, report exporter (text/Markdown/JSON), `-run=VoidValidate` commandlet.
- **Severity contract:** ERROR/FATAL = generation cannot safely continue (blocking); WARNING = suspicious, continues; INFO = diagnostic.
- **Pipeline:** `ImportValidation -> Road -> Validate(PostRoad) -> District -> ... -> Metro -> Final`, as data
  (`FVoidPipelineDefinition`); Final validation always runs; a failed import or blocking stage stops generation before anything spawns.
- Every issue: severity, subsystem, code, object id, source, field path, location (when known), description, suggested fix.

## 2. Files changed (pre-existing; all edits additive)

| File | Change |
|---|---|
| `VOIDWorldBuilder.uplugin` | `EngineVersion` 5.6.0 -> **5.8.0**; `Plugins: ProceduralMeshComponent`; `VOIDWorldBuilderValidation` module entry (Editor / Default). |
| `Source/VOIDWorldBuilderCore/Public/Data/VoidValidationReport.h` | +5 defaulted fields and 4 fluent setters on `FVoidValidationIssue`; +`AddIssue`, `NumBlocking`, `RecomputeValidity`, `Merge`, `SeverityToString` on `FVoidValidationReport`. All existing signatures and behaviour unchanged. |
| `Source/VOIDWorldBuilderGenerators/Public/VoidGeneratorRegistry.h` + `Private/VoidGeneratorRegistry.cpp` | Records duplicate-id registrations (`GetCollidedIds`), adds `GetRegisteredIdsSorted`. `RegisterGenerator` still overwrites, as before. |
| `Docs/ValidationRules.md` | Appended a generated 106-code table, severity contract, performance rules. |

**Not touched:** Core (rest), Import, RoadGenerator, Editor module/panel, any test of earlier phases.

## 3. Files added

Core: `Public/Interfaces/IVoidValidator.h`, `Public/Data/VoidGeneratedTags.h`, `Private/VoidValidation.cpp`, `Private/Tests/VoidValidatorRegistryTests.cpp`.

`VOIDWorldBuilderValidation/` (25 files, ~4,400 lines incl. ~780 test lines): `Build.cs`; Public: `VoidValidationLog.h`, `VoidJsonSchemaSubset.h`,
`VoidMeridianValidator.h`, `VoidPackageValidators.h`, `VoidGeneratedWorldValidator.h`, `VoidGeneratorContractAuditor.h`, `VoidValidationPipeline.h`,
`VoidValidationReportExporter.h`, `VoidValidateCommandlet.h`, `VOIDWorldBuilderValidationModule.h`; Private: matching `.cpp`s, `VoidSha256.h`,
`VoidValidationMath.h`; `Private/Tests/`: 3 test files + helper header.

Docs: `Docs/ValidationArchitecture.md`. Repo root: `INTEGRATION_RISK_REGISTER.md`, `HANDOFF_AGENT3.md`, `Reports/MeridianFirstLookReport.{md,json}`,
`Tools/{meridian_reference_validator.py, static_lint.py, StandaloneTests/standalone_tests.cpp}`.

## 4. Validation rules implemented

Full generated table: `Docs/ValidationRules.md` (106 codes). Coverage against the brief:

- **Data:** invalid JSON (line/column reported), missing fields, wrong types, unknown fields, enum/const/range/pattern/count via schema subset; duplicate ids (per collection - Meridian repeats district ids across sections legitimately), unresolved references (district, route, tunnel/bridge, station, region, cell, module), NaN/Inf/out-of-range coordinates, invalid dimensions (width, height, lane/speed), **unsupported schema versions** (`$schema` `_vN` suffix), schema-id mismatch, id syntax.
- **Meridian package (ValidationSchema VR-001..VR-012):** syntax, schema, reference resolution, sub-location coverage, declared totals, district set (canon five), dependency DAGs (district + module), generation/import order, pair coverage, sha-256 checksums (CRLF-tolerant), fabricated-geometry scan, unlisted files/stale totals, flagged open items logged by id with their `blocks` exposed.
- **Roads:** invalid endpoints/geometry (zero-length, duplicate points, hairpins), width plausibility, elevated-without-structure, bad/self/duplicate/too-far connections, **unsplit T-junctions**, **crossings without a junction** (grade-separated ones counted, not flagged), isolated roads, disconnected networks, acute and overloaded junctions; Phase 3 `FVoidRoadValidator` re-run early.
- **Building hooks (no generator implemented):** footprint simplicity / degeneracy / winding / extent / corner duplicates, height plausibility, footprint overlaps, **building-on-road** (ERROR) and encroachment (WARNING). Future generators add their own `IVoidValidator`.
- **Generated world:** missing / duplicate / orphan actors, failed or empty generators, invalid transforms, empty meshes, invalid splines, missing assets, dangling junction references, floating buildings.
- **Contract/pipeline:** unregistered, non-canonical, duplicate, unstable or invalid generator ids; duplicate validator ids; generator failure with its last log lines; required-vs-optional generators; cancellation; missing inputs.

## 5. Generator contracts inspected

`IVoidGenerator` (id + `Generate(Package, Context)`), `FVoidGenerationContext`, `FVoidGeneratorRegistry`, Road generator
(validator, spline/mesh/bridge builders, intersection builder, actors), importer/reader/validator, Editor panel + commandlet.
Conflicts found and their handling are in `INTEGRATION_RISK_REGISTER.md`; the ones that matter most:
R-01 no geometry source for Meridian - R-10 shared report slot overwritten - R-15 district-local vs world coordinates -
R-08 non-idempotent generation - R-09 panel bypasses validation - R-24 O(N^2) junction clustering in the Road Generator.
No other generator's code was rewritten; where a fix belongs to someone else it is documented, not applied.

## 6. Performance

No all-pairs loops over datasets: uniform spatial hash (cell size from data; oversized-item escape hatch), union-find for
junction clustering/connectivity, early rejection, capped quadratic work (footprint self-intersection <= 256 corners, per-junction pairs <= 32 endpoints),
per-code output throttling, staged execution. A 70x70 grid (~9,700 roads, ~4,800 buildings) test asserts correctness (no false
crossings/conflicts, one network) and a generous 30 s ceiling - **timing itself is unmeasured** until the test runs in-engine.

## 7. Test coverage (20 automation tests, prefix `VOID.WorldBuilder.`)

`Core.Report.{SeveritySemantics, MergeAttributesLegacyIssues}`, `Core.Context.ThrottlesButStillInvalidates`, `Core.ValidatorRegistry.RegistrationOrderAndDuplicates`;
`Validation.Meridian.{CleanFixture..., MutationsProduceExpectedErrors (15 cases), ManifestChecksumsAndInventory, Sha256KnownVectors}`, `Validation.Schema.SubsetKeywords`,
`Validation.Data.InvalidCoordinatesAndReferences`, `Validation.Road.{GeometryAndConnections, CrossingsJunctionsAndConnectivity}`,
`Validation.Building.FootprintsOverlapsAndRoadConflicts`, `Validation.Performance.GridOfSeveralThousandRoads`,
`Validation.Pipeline.{RunsInCanonicalOrder..., FailureModesAbortWithExplicitErrors, FailedImportBlocksGeneration}`, `Validation.Contract.RegistryAudit`,
`Validation.World.SnapshotChecks`, `Validation.Report.ExportContainsEveryDebuggingField`.
Covers the brief's list: malformed JSON, missing fields, duplicate ids, invalid references, invalid coordinates, generator registration, report generation, severity.
**Not covered:** `BuildSnapshot(UWorld*)` (needs a live world; kept deliberately thin), commandlet, file-system loader `AddFromDirectory`.

## 8. Unresolved issues (need a human decision)

1. **R-01** Who builds the Meridian -> `FVoidDesignPackage` layout step? Without it nothing generates from Meridian.
2. **R-03** 14 locked Meridian files (4 of 5 districts) are absent from the supplied package.
3. **R-15** One agreed district-origin transform for all generators.
4. **R-09** Wire the Editor panel to the pipeline; **R-08** add cleanup/idempotency; **R-07** tag actors; **R-24** fix O(N^2) clustering.
5. **R-14** Actual UE 5.8.2 build/test run (and confirm `ProceduralMeshComponent` still ships in 5.8.2).

## 9. Exact integration requirements

1. **Merge:** replace `Plugins/VOIDWorldBuilder` with this folder, or apply the 5 modified files (section 2) onto the combined plugin. Union `.uplugin` `Modules` and `Plugins`. If another agent also changed `VoidValidationReport.h` or `VoidGeneratorRegistry.*`, keep both sets of additions.
2. **Module graph must stay** `Core <- Import <- Generators <- Validation`. No generator module may depend on `VOIDWorldBuilderValidation`. Run `python Tools/static_lint.py Plugins/VOIDWorldBuilder` after merging.
3. **Each generator agent:** register with `FVoidGeneratorRegistry` using the exact canonical id (`Road`, `District`, `Building`, `Environment`, `Lighting`, `Props`, `Metro`); tag every spawned actor with `FVoidGeneratedTags::Apply(Actor, Id, ObjectId)`; do not overwrite `Context.GenerationValidationReport` (append, or rely on the pipeline capturing it); register an `IVoidValidator` for your stage; prefix your codes `VOID.<Area>.` and avoid reusing this module's prefixes without coordination (R-18).
4. **Orchestrator / UI:** call `FVoidValidationPipeline::RunFromImport` (use `FVoidPipelineDefinition::MakeDefault(true)` once all seven generators exist), decide success by `Result.WasSuccessful()` / `Report.NumBlocking()`, never by `bIsValid` from legacy reports.
5. **CI (no editor):** `-run=VoidValidate -Meridian=... [-Package=...] [-Strict]` (exit 0/1/2), or `Tools/meridian_reference_validator.py`.
6. **Meridian data owner:** supply the 14 missing files; regenerate `PackageManifest.json` (7 uninventoried files, stale totals); add `* text eol=lf` for the package.
7. **Final integration run** must use `bAllowMissingDistrictPackages = false` and require all generators.
