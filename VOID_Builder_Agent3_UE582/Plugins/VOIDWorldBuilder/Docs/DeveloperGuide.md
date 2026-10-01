# Developer Guide — Import Pipeline

## Calling the importer

```cpp
#include "VoidDesignPackageImporter.h"

// From a file on disk -- format detected by extension.
const FVoidImportResult Result = FVoidDesignPackageImporter::LoadFromFile(TEXT("C:/Exports/district_04.json"));

// From JSON text already in memory (network response, test fixture, etc.).
const FVoidImportResult Result = FVoidDesignPackageImporter::LoadFromJsonString(JsonText);

if (Result.WasSuccessful())
{
	const FVoidDistrictData& District = Result.Package.District;
	// ... hand off to a generator (Phase 3+) ...
}
else
{
	for (const FVoidValidationIssue& Issue : Result.ValidationReport.Issues)
	{
		// Issue.Severity, Issue.Message, Issue.FieldPath, Issue.ErrorCode, Issue.SuggestedFix
	}
}
```

`FVoidImportResult` always contains a fully-formed (possibly empty)
`Package`, a `ValidationReport`, and a `Context` (source description,
settings snapshot, elapsed time) -- regardless of success or failure.
Nothing in this pipeline throws or crashes on malformed input; every
failure mode is a `ValidationReport` entry.

## Severity levels and what to do with each

| Severity | Blocks generation? | Typical cause | What a caller should do |
|---|---|---|---|
| Info | No | A default was applied; an unknown field was ignored | Nothing required; useful for troubleshooting |
| Warning | No | Untyped building placeholder; newer schema minor version | Surface to the user; safe to proceed |
| Error | Yes | Missing required field, invalid geometry, duplicate id | Must be fixed in the source package before generation |
| Fatal | Yes, and stops further validation | Unreadable file, unparseable JSON, incompatible schema major version | Cannot proceed at all; the Editor panel pops a blocking dialog for these |

Check `Result.WasSuccessful()` for the single "can I generate from this"
answer; don't re-derive it by scanning `Issues` yourself.

## Import Settings

`UVoidImportSettings` (Edit → Project Settings → Plugins → VOID World
Builder Import):

- **Fail On Unknown Fields** (default off) -- when on, an unrecognized
  JSON field becomes an Error instead of an Info.
- **Minimum Supported Schema Version** -- currently informational,
  reserved for a future migration system.

These are snapshotted into `FVoidImportContext` at the start of each
import call (`FVoidDesignPackageImporter::MakeContext`), so a setting
change mid-import never changes behavior partway through a single call.

## Adding a new validation rule

1. Add the check to the appropriate `FVoidPackageValidator::Validate*`
   method (or add a new one, called from `Validate`).
2. Pick a severity per the table above.
3. Give it a stable error code following the `VOID.Import.<Reason>`
   convention (see `Docs/ValidationRules.md`) and a concrete
   `SuggestedFix`.
4. Add a test in `VoidDesignPackageImporterTests.cpp` covering both the
   failing and passing case.
5. Register the new code in `Docs/ValidationRules.md`.

## Adding a new file format

See `Docs/ExtensionGuide.md`.

## Logging

- `LogVoidWorldBuilder`: plugin-wide, low-volume (module lifecycle,
  reader/generator registration).
- `LogVoidImport`: import pipeline. Default level shows the Import /
  Validation / Performance summary and every issue. Enable `Verbose` (via
  the Output Log category filter, or `-LogCmds="LogVoidImport Verbose"`)
  for per-field mapping trace.
