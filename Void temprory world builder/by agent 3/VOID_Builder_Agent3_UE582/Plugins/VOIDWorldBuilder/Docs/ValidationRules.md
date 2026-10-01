# Validation Rules Reference

Every `ErrorCode` currently emitted by the import pipeline, extracted
directly from source (not hand-maintained from memory -- keep it that
way; if you add a new `AddError`/`AddWarning`/`AddInfo`/`AddFatal` call
with a new code, add it here in the same change).

Convention: codes follow `VOID.Import.<Reason>`. Severity is chosen per
`Docs/DeveloperGuide.md`'s table (Info = fyi, Warning = tolerable, Error
= blocks generation, Fatal = blocks import and stops further validation).

| Error Code | Severity | Raised when | Where |
|---|---|---|---|
| `VOID.Import.UnreadableFile` | Fatal | The file at the given path could not be opened/read from disk. | `FVoidJsonPackageReader::TryRead` |
| `VOID.Import.MalformedJson` | Fatal | The JSON text failed to parse (malformed syntax). | `FVoidJsonPackageReader::TryRead` / `TryReadFromString` |
| `VOID.Import.UnsupportedFormat` | Fatal | No `IVoidPackageReader` is registered for the file's extension. | `FVoidDesignPackageImporter::LoadFromFile` |
| `VOID.Import.IncompatibleSchemaMajor` | Fatal | Package `schemaVersion`'s major component doesn't match this build's supported major version (either direction). | `FVoidPackageValidator::ValidateSchemaVersion` |
| `VOID.Import.NewerSchemaMinor` | Warning | Package `schemaVersion`'s minor component is newer than this build's; newer optional fields may be silently ignored. | `FVoidPackageValidator::ValidateSchemaVersion` |
| `VOID.Import.MissingSchemaVersion` | Warning | Package has no `schemaVersion` field at all; assumed to be the current tool version. | `FVoidJsonPackageReader::MapJsonObjectToPackage` |
| `VOID.Import.UnparseableSchemaVersion` | Warning | Package has a `schemaVersion` field but it isn't in `"Major.Minor"` form. | `FVoidJsonPackageReader::MapJsonObjectToPackage` |
| `VOID.Import.MissingField` | Error (Info for a few clearly-optional cases -- see source) | A required field is absent: `metadata` object, `district` object, `districtId`, a building's `id`/`heightUnits`/`footprintCorners`, a road's `id`/`widthUnits`/`centerlinePoints`. Used at Info severity only for `metadata.isApproved` (defaults to false, itself then caught by `NotApproved`), a building's `buildingType` (defaults to an untyped placeholder), and empty `buildings`/`roads` arrays. | `FVoidJsonPackageReader::MapJsonObjectToPackage`, `FVoidPackageValidator::ValidateMetadata` / `ValidateDistrict` / `ValidateBuildings` / `ValidateRoads` |
| `VOID.Import.MalformedElement` | Error | An entry in `buildings` or `roads` is present but isn't a JSON object. | `FVoidJsonPackageReader::MapJsonObjectToPackage` |
| `VOID.Import.InvalidGeometry` | Error | A building has fewer than 3 footprint corners or non-positive height; a road has fewer than 2 centerline points or non-positive width. | `FVoidPackageValidator::ValidateBuildings` / `ValidateRoads` |
| `VOID.Import.DuplicateId` | Error | Two elements (building/road/district) share the same id. The district's own id is reserved and treated as already "seen." | `FVoidPackageValidator::ValidateIdUniqueness` |
| `VOID.Import.NotApproved` | Error | `metadata.isApproved` is false (or absent, defaulting to false). | `FVoidPackageValidator::ValidateMetadata` |
| `VOID.Import.UnrecognizedRoadType` | Warning | A road's `roadType` string doesn't match any `EVoidRoadType` value; defaults to `Local`. | `FVoidJsonPackageReader::MapJsonObjectToPackage` |
| `VOID.Import.UnknownField` | Info by default, Error if **Fail On Unknown Fields** is enabled in Project Settings | A JSON object contains a key not in this reader's known-field list for that object type, and the key does not start with `_`. | `FVoidJsonPackageReader`'s `ReportUnknownFields` helper |

## Road Generation-Time Rules (Phase 3, `FVoidRoadValidator`)

Distinct from the import-time rules above: these run only when the Road
Generator runs, check "can this actually be built" rather than "is this
data shaped correctly", and use their own `VOID.RoadGen.<Reason>` code
namespace. See `Docs/RoadGeneratorArchitecture.md` for why these are
kept separate from import-time validation.

| Error Code | Severity | Raised when | Where |
|---|---|---|---|
| `VOID.RoadGen.BridgeTunnelConflict` | Error | A road has both `isBridge` and `isTunnel` set to `true`. | `FVoidRoadValidator::ValidateBridgeTunnelExclusivity` |
| `VOID.RoadGen.UnresolvableConnection` | Error | A road's `connectionIds` references an id that doesn't match any road or the district itself. | `FVoidRoadValidator::ValidateConnectionIdsResolve` |
| `VOID.RoadGen.InvalidRoundabout` | Error | A `Roundabout`-type road has `roundaboutRadiusUnits <= 0`, or no `centerlinePoints` to use as its center. | `FVoidRoadValidator::ValidateRoundabouts` |
| `VOID.RoadGen.UnconnectedRoundabout` | Warning | No other road's `connectionIds` references this roundabout (it may still connect via coincident endpoints, but explicit connection is recommended). | `FVoidRoadValidator::ValidateRoundabouts` |

## Known-field lists (what counts as "unknown")

- **Root object:** `schemaVersion`, `metadata`, `district`
- **`metadata` object:** `sourceDocumentName`, `sourceDocumentVersion`, `isApproved`
- **`district` object:** `districtId`, `buildings`, `roads`
- **A `buildings[]` entry:** `id`, `heightUnits`, `buildingType`, `footprintCorners`
- **A `roads[]` entry:** `id`, `widthUnits`, `centerlinePoints`, `roadType`, `laneCount`, `speedLimitUnits`, `elevationUnits`, `hasSidewalk`, `hasMedian`, `isBridge`, `isTunnel`, `culDeSacAtEnd`, `roundaboutRadiusUnits`, `connectionIds`

Any key starting with `_` at any level is always ignored, never flagged
as unknown -- this is the documented "comment" convention (see
`Docs/ArchitectureNotes.md`).

## Cross-reference validation (current scope)

The only cross-reference rule implemented today is id uniqueness across
the whole district, including the district's own id (`VOID.Import.DuplicateId`).
Deeper parent/child relationship validation (e.g. a road referencing a
building it connects to) is deferred until the schema actually defines
such a relationship field -- there is nothing to validate yet, and
inventing a relationship the schema doesn't have would be exactly the
kind of unrequested invention this tool's studio rules forbid.

---

# Agent 3 additions: first-look validation & integration rules

Severity semantics (names unchanged from Phase 1, now precisely defined):

| Severity | Meaning for the pipeline |
|---|---|
| **Fatal / Error** | *Blocking.* Generation cannot safely continue (`FVoidValidationIssue::IsBlocking()`). The pipeline aborts (unless `bStopOnError=false`). |
| **Warning** | Generation can continue but something is suspicious. Never changes `bIsValid`. |
| **Info** | Diagnostic information. Never changes `bIsValid`. |

Every issue carries: severity, subsystem, code, object id, source, field path, optional location, description and suggested fix. `Error*` = Error normally, Warning when `bAllowMissingDistrictPackages` is set.

The tables below are **generated from the source** by scanning `VOIDWorldBuilderValidation/Private/*.cpp`, so they cannot drift from the code. Codes `VOID.RoadGen.*` and `VOID.Import.*` are the Phase 2/3 codes documented above; the road network validator re-runs `FVoidRoadValidator` at PostImport and relays its codes unchanged.


## Building - Building hook checks (footprints, overlaps, road conflicts)

| Code | Severity | Raised in |
|---|---|---|
| `VOID.Building.DuplicateCorner` | Warning | VoidPackageValidators.cpp |
| `VOID.Building.EncroachesRoad` | Warning | VoidPackageValidators.cpp |
| `VOID.Building.FootprintDegenerate` | Error | VoidPackageValidators.cpp |
| `VOID.Building.FootprintSelfIntersects` | Error | VoidPackageValidators.cpp |
| `VOID.Building.FootprintTooComplex` | Warning | VoidPackageValidators.cpp |
| `VOID.Building.FootprintTooLarge` | Warning | VoidPackageValidators.cpp |
| `VOID.Building.FootprintWinding` | Warning | VoidPackageValidators.cpp |
| `VOID.Building.HeightImplausible` | Warning | VoidPackageValidators.cpp |
| `VOID.Building.NoPackage` | Info | VoidPackageValidators.cpp |
| `VOID.Building.OnRoad` | Error | VoidPackageValidators.cpp |
| `VOID.Building.OverlapsBuilding` | Warning | VoidPackageValidators.cpp |

## Contract - Registry/contract audit

| Code | Severity | Raised in |
|---|---|---|
| `VOID.Contract.DuplicateGeneratorId` | Error | VoidGeneratorContractAuditor.cpp |
| `VOID.Contract.DuplicateValidatorId` | Error | VoidGeneratorContractAuditor.cpp |
| `VOID.Contract.GeneratorNotRegistered` | Warning | VoidGeneratorContractAuditor.cpp |
| `VOID.Contract.InvalidGeneratorId` | Error | VoidGeneratorContractAuditor.cpp |
| `VOID.Contract.NonCanonicalGeneratorId` | Warning | VoidGeneratorContractAuditor.cpp |
| `VOID.Contract.StageWithoutValidator` | Info | VoidGeneratorContractAuditor.cpp |
| `VOID.Contract.UnexpectedGenerator` | Info | VoidGeneratorContractAuditor.cpp |
| `VOID.Contract.UnstableGeneratorId` | Error | VoidGeneratorContractAuditor.cpp |

## Data - Package value checks (finite/in-range numbers, id syntax, district id)

| Code | Severity | Raised in |
|---|---|---|
| `VOID.Data.CoordinateOutOfRange` | Error | VoidPackageValidators.cpp |
| `VOID.Data.IdSyntax` | Warning | VoidPackageValidators.cpp |
| `VOID.Data.NoPackage` | Info | VoidPackageValidators.cpp |
| `VOID.Data.NonFiniteValue` | Error | VoidPackageValidators.cpp |
| `VOID.Data.UnknownDistrict` | Error | VoidPackageValidators.cpp |

## Meridian - Meridian_Master package rules (ValidationSchema.json VR-001..VR-012 + integration checks)

| Code | Severity | Raised in |
|---|---|---|
| `VOID.Meridian.AmbiguousOrderFields` | Info | VoidMeridianValidator.cpp |
| `VOID.Meridian.ArithmeticMismatch` | Error | VoidMeridianValidator.cpp |
| `VOID.Meridian.CellContainsUnknownSubLocation` | Warning | VoidMeridianValidator.cpp |
| `VOID.Meridian.ChecksumMismatch` | Error | VoidMeridianValidator.cpp |
| `VOID.Meridian.CoordinateShapedArray` | Warning | VoidMeridianValidator.cpp |
| `VOID.Meridian.DependencyCycle` | Error | VoidMeridianValidator.cpp |
| `VOID.Meridian.DependencyMismatch` | Error | VoidMeridianValidator.cpp |
| `VOID.Meridian.DirectoryMissing` | Fatal | VoidMeridianValidator.cpp |
| `VOID.Meridian.DistrictSetMismatch` | Error | VoidMeridianValidator.cpp |
| `VOID.Meridian.DuplicateId` | Error | VoidMeridianValidator.cpp |
| `VOID.Meridian.DuplicateRelationshipPair` | Error | VoidMeridianValidator.cpp |
| `VOID.Meridian.FabricatedGeometry` | Error | VoidMeridianValidator.cpp |
| `VOID.Meridian.FlaggedOpenItem` | Info | VoidMeridianValidator.cpp |
| `VOID.Meridian.GenerationOrderViolation` | Error | VoidMeridianValidator.cpp |
| `VOID.Meridian.ImportOrderViolation` | Error | VoidMeridianValidator.cpp |
| `VOID.Meridian.InvalidJson` | Error | VoidMeridianValidator.cpp |
| `VOID.Meridian.LineEndingChecksum` | Warning | VoidMeridianValidator.cpp |
| `VOID.Meridian.LockedInputMissing` | Error* | VoidMeridianValidator.cpp |
| `VOID.Meridian.ManifestMissing` | Fatal | VoidMeridianValidator.cpp |
| `VOID.Meridian.ManifestTotalsStale` | Warning | VoidMeridianValidator.cpp |
| `VOID.Meridian.MissingRelationshipPair` | Error | VoidMeridianValidator.cpp |
| `VOID.Meridian.MissingRequiredFile` | Error | VoidMeridianValidator.cpp |
| `VOID.Meridian.NoGeometrySource` | Info | VoidMeridianValidator.cpp |
| `VOID.Meridian.NoSchemaForFile` | Info | VoidMeridianValidator.cpp |
| `VOID.Meridian.PolymorphicDistrictField` | Warning | VoidMeridianValidator.cpp |
| `VOID.Meridian.SchemaIdMismatch` | Error | VoidMeridianValidator.cpp |
| `VOID.Meridian.SubLocationCheckSkipped` | Info | VoidMeridianValidator.cpp |
| `VOID.Meridian.SubLocationUncovered` | Error | VoidMeridianValidator.cpp |
| `VOID.Meridian.UnlistedFile` | Warning | VoidMeridianValidator.cpp |
| `VOID.Meridian.UnresolvedReference` | Error | VoidMeridianValidator.cpp |
| `VOID.Meridian.UnsupportedSchemaVersion` | Error | VoidMeridianValidator.cpp |

## Pipeline - Pipeline runner

| Code | Severity | Raised in |
|---|---|---|
| `VOID.Pipeline.Cancelled` | Warning | VoidValidationPipeline.cpp |
| `VOID.Pipeline.GeneratorFailed` | Error | VoidValidationPipeline.cpp |
| `VOID.Pipeline.GeneratorSkipped` | Warning | VoidValidationPipeline.cpp |
| `VOID.Pipeline.ImportFailed` | Error | VoidValidationPipeline.cpp |
| `VOID.Pipeline.MissingInput` | Error | VoidValidationPipeline.cpp |
| `VOID.Pipeline.RequiredGeneratorMissing` | Error | VoidValidationPipeline.cpp |
| `VOID.Pipeline.StageValidationFailed` | Error | VoidValidationPipeline.cpp |

## Road - Road geometry, connections, topology

| Code | Severity | Raised in |
|---|---|---|
| `VOID.Road.AcuteJunction` | Warning | VoidPackageValidators.cpp |
| `VOID.Road.ConnectionToNonRoad` | Warning | VoidPackageValidators.cpp |
| `VOID.Road.ConnectionTooFar` | Warning | VoidPackageValidators.cpp |
| `VOID.Road.CrossingWithoutJunction` | Warning | VoidPackageValidators.cpp |
| `VOID.Road.DanglingEndpoints` | Info | VoidPackageValidators.cpp |
| `VOID.Road.DegenerateSegment` | Warning | VoidPackageValidators.cpp |
| `VOID.Road.DisconnectedNetwork` | Warning | VoidPackageValidators.cpp |
| `VOID.Road.DuplicateConnection` | Warning | VoidPackageValidators.cpp |
| `VOID.Road.ElevatedWithoutStructure` | Warning | VoidPackageValidators.cpp |
| `VOID.Road.GradeSeparatedCrossings` | Info | VoidPackageValidators.cpp |
| `VOID.Road.InvalidLaneOrSpeed` | Error | VoidPackageValidators.cpp |
| `VOID.Road.IsolatedRoad` | Warning | VoidPackageValidators.cpp |
| `VOID.Road.NoPackage` | Info | VoidPackageValidators.cpp |
| `VOID.Road.NoRoads` | Warning | VoidPackageValidators.cpp |
| `VOID.Road.OverloadedJunction` | Warning | VoidPackageValidators.cpp |
| `VOID.Road.SelfConnection` | Error | VoidPackageValidators.cpp |
| `VOID.Road.SharpTurn` | Warning | VoidPackageValidators.cpp |
| `VOID.Road.UnsplitTJunction` | Warning | VoidPackageValidators.cpp |
| `VOID.Road.WidthImplausible` | Warning | VoidPackageValidators.cpp |
| `VOID.Road.ZeroLength` | Error | VoidPackageValidators.cpp |

## Schema - JSON-Schema-subset conformance (VR-002)

| Code | Severity | Raised in |
|---|---|---|
| `VOID.Schema.ConstMismatch` | Error | VoidJsonSchemaSubset.cpp |
| `VOID.Schema.EnumMismatch` | Error | VoidJsonSchemaSubset.cpp |
| `VOID.Schema.ItemCount` | Error | VoidJsonSchemaSubset.cpp |
| `VOID.Schema.MinLength` | Error | VoidJsonSchemaSubset.cpp |
| `VOID.Schema.MissingField` | Error | VoidJsonSchemaSubset.cpp |
| `VOID.Schema.Pattern` | Error | VoidJsonSchemaSubset.cpp |
| `VOID.Schema.PropertyCount` | Error | VoidJsonSchemaSubset.cpp |
| `VOID.Schema.Range` | Error | VoidJsonSchemaSubset.cpp |
| `VOID.Schema.UniqueItems` | Error | VoidJsonSchemaSubset.cpp |
| `VOID.Schema.UnknownField` | Error | VoidJsonSchemaSubset.cpp |
| `VOID.Schema.WrongType` | Error | VoidJsonSchemaSubset.cpp |

## Validation - Validation infrastructure

| Code | Severity | Raised in |
|---|---|---|
| `VOID.Validation.IssuesSuppressed` | Info | VoidValidation.cpp (Core) |

## World - Generated-world snapshot checks

| Code | Severity | Raised in |
|---|---|---|
| `VOID.World.DanglingReference` | Error | VoidGeneratedWorldValidator.cpp |
| `VOID.World.DuplicateActor` | Error | VoidGeneratedWorldValidator.cpp |
| `VOID.World.EmptyGeometry` | Error | VoidGeneratedWorldValidator.cpp |
| `VOID.World.FloatingGeometry` | Warning | VoidGeneratedWorldValidator.cpp |
| `VOID.World.GeneratorProducedNothing` | Error/Warning | VoidGeneratedWorldValidator.cpp |
| `VOID.World.InvalidSpline` | Error | VoidGeneratedWorldValidator.cpp |
| `VOID.World.InvalidTransform` | Error/Warning | VoidGeneratedWorldValidator.cpp |
| `VOID.World.MissingActor` | Error | VoidGeneratedWorldValidator.cpp |
| `VOID.World.MissingAsset` | Error | VoidGeneratedWorldValidator.cpp |
| `VOID.World.NoWorld` | Info | VoidGeneratedWorldValidator.cpp |
| `VOID.World.OrphanActor` | Warning | VoidGeneratedWorldValidator.cpp |
| `VOID.World.UntaggedActors` | Info | VoidGeneratedWorldValidator.cpp |

## Not automatable (by design)

ValidationSchema.json rules VR-006 (canon re-verification beyond checksums), VR-008 (canon-locked content) and narrative rules cannot be checked mechanically. `flagged_open_items` are surfaced as `VOID.Meridian.FlaggedOpenItem` Info issues naming the flag id (BuilderManifest `logging_requirements.flag_precedent`); their `blocks` lists are exposed in `FVoidMeridianSummary::BlockedContentIds` for the pipeline/generators to honour.

## Performance rules (by design; guarded by the `Validation.Performance.*` automation test)

1. No all-pairs loops over roads, segments or buildings. Crossings, unsplit T-junctions, junction clustering, building/road conflicts and building overlaps query a uniform spatial hash (`FSpatialHash2D`) whose cell size derives from the data's own average segment/footprint size; items spanning more than 1024 cells go to an oversized list instead of exploding the index.
2. Junction clustering and connectivity use union-find: near-linear.
3. The only quadratic loops are bounded by a small constant: footprint self-intersection (capped by `MaxFootprintCorners`, default 256), angle tests inside one junction cluster (skipped above 32 endpoints), `uniqueItems` on schema arrays (skipped above 4096 items).
4. Early rejection: unusable roads/buildings (non-finite, too few points) are reported once and excluded from topology passes.
5. Staged: import-stage data validation runs before any generation; a blocking result stops the pipeline before any actor is spawned.
6. Output throttling: `MaxIssuesPerCode` (default 200) caps stored issues per code; suppressed blocking issues still invalidate the report, and one Info summary records how many were suppressed.

