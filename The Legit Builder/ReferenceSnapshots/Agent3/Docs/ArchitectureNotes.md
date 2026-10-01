# Architecture Notes — Phase 2

This document explains the *why* behind Phase 2's structural decisions.
For the *what* (rule list, field reference), see `Docs/ValidationRules.md`
and `Docs/DeveloperGuide.md`.

## The Reader Registry: how new formats get added without touching anything else

`IVoidPackageReader` (Core) is defined entirely in terms of Core-native
types: `FString`, `FVoidDesignPackage`, `FVoidImportContext`,
`FVoidValidationReport`. It does not mention JSON, YAML, or any parsing
library. `FVoidJsonPackageReader` (Import module) is the only
implementation today, registered against the extension `"json"` in
`FVOIDWorldBuilderImportModule::StartupModule()`.

`FVoidDesignPackageImporter::LoadFromFile` never assumes JSON. It asks
`FVoidPackageReaderRegistry::Get().FindReaderForFile(FilePath)` for a
reader by extension and delegates to whatever it gets back. Adding YAML
support later means:

1. Write `FVoidYamlPackageReader : public IVoidPackageReader` in a new
   (or the same) module, implementing `TryRead` to map YAML into an
   `FVoidDesignPackage`.
2. Register it: `FVoidPackageReaderRegistry::Get().RegisterReader(MakeShared<FVoidYamlPackageReader>());`
   for extension `"yaml"`.

Nothing in `FVoidDesignPackageImporter`, the Editor panel, the
commandlet, `FVoidPackageValidator`, or any future generator changes.
This is the same pattern already established for generators
(`IVoidGenerator` / `FVoidGeneratorRegistry`) -- deliberately, for
consistency.

A Remote API "reader" is a slightly different shape (it wouldn't have a
file extension in the same sense), which is why `IVoidPackageReader`
takes a file path rather than, say, a `TSharedPtr<FArchive>` -- a Remote
reader would most likely fetch to a temp file first and reuse the same
interface, or the interface would grow a second method at that point.
Deferred until there's a concrete remote source to design against, not
invented speculatively now.

## Why `FVoidDistrictData` is its own type, nested inside `FVoidDesignPackage`

Phase 1's `FVoidDesignPackage` held `DistrictId`/`Buildings`/`Roads`
directly. Phase 2 introduces `FVoidDistrictData` as its own struct and
moves those three fields into it, because:

- The Phase 2 spec explicitly names `FVoidDistrictData` as a required
  production data structure.
- It generalizes correctly: a future multi-district batch package format
  (Phase 5+) could hold `TArray<FVoidDistrictData> Districts` without
  `FVoidDistrictData` itself changing -- only the envelope around it
  would. Under the Phase 1 shape, that same change would have meant
  redefining `FVoidDesignPackage` itself and touching every piece of code
  that read `Package.Buildings` directly.

This is the one architecture revision in Phase 2 (the JSON schema's
`district` nesting follows from it). It's called out explicitly here
rather than silently changed, per the "immutable unless explicitly
revised" rule -- the Phase 2 brief itself is what explicitly revises it,
by naming the type.

## Why there are two log categories

`LogVoidWorldBuilder` (Core, plugin-wide) covers cross-cutting, low-volume
concerns: module startup/shutdown, generator/reader registration.
`LogVoidImport` (Import-specific) covers the import pipeline itself,
which is comparatively high-volume once Verbose logging is on (per-field
mapping trace, per-issue detail). A designer debugging a failed import
wants to filter to `LogVoidImport` in isolation; a plugin maintainer
checking "did all four modules load" wants `LogVoidWorldBuilder` without
import noise. Splitting them serves both without either drowning in the
other's output.

## Why there's no Asset Registry integration yet

The Phase 2 brief lists "Asset Registry Integration (where appropriate)."
Nothing in Phase 2 generates a `UAsset` or persistent level content --
there is no generated Data Asset, no spawned actor, nothing with a
package path to register or query. Building a "check the Asset Registry"
call today would either be a no-op or would have to invent a content-path
convention this tool hasn't earned yet. The appropriate point for this is
**Phase 4** (Building Generator), once generated content actually exists
under a real `/Game/...` path -- at which point a "does this district
already have generated content" check against the Asset Registry becomes
a real, useful integration rather than a placeholder.

## Why `_`-prefixed fields are the "comment" convention

Standard JSON (RFC 8259), which is what `FJsonSerializer` implements, has
no comment syntax. This plugin will not strip `//`/`/* */` with a regex,
because that risks corrupting any string field that legitimately contains
those characters (a building's `buildingType` tag, a source document
name). Instead, any object key starting with `_` is treated as an
intentional annotation and silently ignored everywhere unknown-field
detection runs -- see `VoidJsonPackageReaderPrivate::ReportUnknownFields`
in `VoidJsonPackageReader.cpp`. This gives package authors a working, if
unofficial, way to leave notes in a `DesignPackage.json` without touching
the parser's correctness.

## Why severity has 4 levels and Fatal short-circuits

`EVoidValidationSeverity` is Info / Warning / Error / Fatal.
`FVoidValidationReport::bIsValid` is false for Error *or* Fatal --
both block generation -- but Fatal additionally means the pipeline
stops looking for more problems. `FVoidPackageValidator::Validate`
returns immediately if the incoming report already has a Fatal issue
(unreadable file, unparseable JSON), and again immediately after its own
schema-version check if that produces a Fatal. Continuing to validate
buildings and roads against data that couldn't be trusted to mean what
it says would just be noise on top of the real problem.
