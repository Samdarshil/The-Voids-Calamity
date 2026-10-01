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

## Metro Codes (Agent 2, Phase 6)

Extracted from source (`FVoidMetroNetworkMapper`, `FVoidMetroValidator`, `FVoidMetroLayoutResolver`, `FVoidMetroGenerator`). Existing codes reused by the mapper (`MissingField`, `MalformedElement`, `MalformedJson`, `UnreadableFile`, `InvalidGeometry`) keep their meanings above.

| Error Code | Severity | Raised when | Where |
|---|---|---|---|
| `VOID.Import.MetroSchemaMismatch` | Warning | MetroNetwork root lacks `$schema: void_metro_network_schema_v1`. | `FVoidMetroNetworkMapper::MapMeridianMetroNetwork` |
| `VOID.Import.MetroUnknownNetwork` | Error | A network/station/line has a missing or unrecognized network id (expected `live_network` / `dead_network`). | `FVoidMetroNetworkMapper` |
| `VOID.Import.MetroUnknownTopology` | Warning | A line has no recognizable `maps_to_road_category`/`segment_model`; skipped unless a centerline is authored. | `FVoidMetroNetworkMapper` |
| `VOID.Import.MetroUnknownGrade` | Warning | Authored `grade` is not AtGrade/Elevated/Underground; ignored. | `FVoidMetroNetworkMapper::MapPackageMetroBlock` |
| `VOID.Import.MetroTunnelEndpoints` | Error | A dead-network tunnel does not connect exactly two districts. | `FVoidMetroNetworkMapper::AppendMeridianTunnelRelationships` |
| `VOID.Metro.NoContent` | Error | Metro data has no stations or lines. | `FVoidMetroValidator::Validate` |
| `VOID.Metro.DuplicateId` | Error | Duplicate/empty station or line id (ids are unique across BOTH networks). | `FVoidMetroValidator::Validate` |
| `VOID.Metro.InvalidPosition` | Error | Non-finite authored coordinate. | `FVoidMetroValidator::Validate` |
| `VOID.Metro.EntranceLimit` | Error (Warning in resolver) | A station declares more entrances than its hard limit (Sector 0 threshold = 1). | `FVoidMetroValidator::Validate`, `FVoidMetroLayoutResolver` |
| `VOID.Metro.UnknownStationRef` | Error | A line, or `district_connectivity` entry, references a station id that does not exist. | `FVoidMetroValidator::Validate` |
| `VOID.Metro.UnknownLineRef` | Error | An interchange references a line id that does not exist. | `FVoidMetroValidator::Validate` |
| `VOID.Metro.NetworkMerge` | Error | Live and Dead networks would be joined (cross-network station membership, mismatched district link, mixed interchange, authored override on a different network). | `FVoidMetroValidator`, `FVoidMetroNetworkMapper::MergeAuthoredOverrides` |
| `VOID.Metro.DeadInterchange` | Error | An interchange involves a Dead-network line (the Dead network has none). | `FVoidMetroValidator::Validate` |
| `VOID.Metro.UnresolvedTunnelLink` | Error | A dead tunnel joins a district that has no Dead-network station. | `FVoidMetroValidator::Validate` |
| `VOID.Metro.TunnelIdUnresolved` | Warning | A station's `shares_tunnel_id` has no tunnel link (RoadNetwork.json not loaded). | `FVoidMetroValidator::Validate`, editor panel |
| `VOID.Metro.NetworkOverlap` | Error | A Live and a Dead station resolve to the same 3D place. | `FVoidMetroValidator::ValidateResolved` |
| `VOID.Metro.PlaceholderLayout` | Warning | The resolver derived any position/alignment itself (Meridian carries no coordinates). | `FVoidMetroLayoutResolver::Resolve` |
| `VOID.Metro.StationUnplaced` | Warning | Station has no authored position and no placeholder could be derived; skipped. | `FVoidMetroLayoutResolver::Resolve` |
| `VOID.Metro.LineUnplaced` | Warning | Line has no authored centerline and could not be placed; skipped. | `FVoidMetroLayoutResolver::Resolve` |
| `VOID.Metro.StationOffLine` | Warning | A ring member station is >100 cm from the ring radius. | `FVoidMetroLayoutResolver::Resolve` |
| `VOID.Metro.LineDuplicateSegments` | Warning | A second legacy-segment line in one network is ignored. | `FVoidMetroLayoutResolver::Resolve` |
| `VOID.Metro.NoTunnelLinks` | Warning | Legacy line but no tunnel links loaded; no dead segments built. | `FVoidMetroLayoutResolver::Resolve` |
| `VOID.Metro.TunnelUnplaced` | Warning | Tunnel endpoint stations not found; segment skipped. | `FVoidMetroLayoutResolver::Resolve` |
| `VOID.Metro.SpawnFailed` | Warning | An actor failed to spawn. | `FVoidMetroGenerator::Generate` |
