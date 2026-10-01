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
| `VOID.Import.UnknownField` | Info by default, Error if **Fail On Unknown Fields** is enabled in Project Settings | A JSON object contains a key not in this reader's known-field list for that object type, and the key does not start with `_`. | `FVoidJsonPackageReader`'s `ReportUnknownFields` helper |

## Known-field lists (what counts as "unknown")

- **Root object:** `schemaVersion`, `metadata`, `district`
- **`metadata` object:** `sourceDocumentName`, `sourceDocumentVersion`, `isApproved`
- **`district` object:** `districtId`, `buildings`, `roads`
- **A `buildings[]` entry:** `id`, `heightUnits`, `buildingType`, `footprintCorners`
- **A `roads[]` entry:** `id`, `widthUnits`, `centerlinePoints`

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
