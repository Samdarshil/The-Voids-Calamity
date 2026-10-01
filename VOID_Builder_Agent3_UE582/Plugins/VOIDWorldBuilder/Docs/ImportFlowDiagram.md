# Import Flow Diagram

## `LoadFromFile` (format-agnostic path)

```
FVoidDesignPackageImporter::LoadFromFile(FilePath)
        |
        v
MakeContext(FilePath)  ---------------------------> snapshots UVoidImportSettings
        |                                            into a fresh FVoidImportContext
        v
FVoidPackageReaderRegistry::Get()
        .FindReaderForFile(FilePath)  -------------> resolves by lowercase extension
        |
   +----+----+
   |         |
 found     not found
   |         |
   |         v
   |   AddFatal("No package reader registered
   |            for extension '.xyz'")
   |         |
   |         v
   |   [ log summary, return unsuccessful result ]
   |
   v
Reader->TryRead(FilePath, Context, OutPackage, OutReport)
        |
        v
   (FVoidJsonPackageReader in Phase 2:
      FVoidJsonReader::ReadFromFile -> parse JSON
      MapJsonObjectToPackage        -> field-by-field TryGet* mapping,
                                        unknown-field detection,
                                        schema version parsing)
        |
   +----+-----------------------------+
   |                                  |
 Fatal issue during read/map     no Fatal issue
   |                                  |
   v                                  v
[ skip validation ]         FVoidPackageValidator::Validate(Package, Report)
   |                                  |
   |                    +-------------+-------------+
   |                    |                           |
   |            ValidateSchemaVersion          (continues if no Fatal)
   |                    |                           |
   |              Fatal on major          ValidateMetadata, ValidateDistrict
   |              version mismatch         (Buildings, Roads, Id uniqueness)
   |                    |                           |
   +--------------------+---------------------------+
                         |
                         v
        Context.ElapsedMilliseconds set; Result.Context assigned
                         |
                         v
              LogImportSummary(Result)  -----------> LogVoidImport:
                         |                             Import Summary
                         v                             Validation Summary
                 return FVoidImportResult               Performance Summary
```

## `LoadFromJsonString` (JSON-specific convenience path)

Same as above from `MapJsonObjectToPackage` onward, but bypasses the
reader registry entirely and calls `FVoidJsonPackageReader::TryReadFromString`
directly -- used by automated tests and any caller that already has JSON
text in memory (e.g. a network response) rather than a file path.

## Editor panel interaction

```
User clicks "Browse..."  -> IMainFrameModule-parented native file dialog
User clicks "Import && Validate"
        |
        v
FScopedSlowTask(1.0f).MakeDialog()   -> progress dialog shown
        |
        v
FVoidDesignPackageImporter::LoadFromFile(...)
        |
        v
Panel state updated: District name, pass/fail, Info/Warn/Error/Fatal counts,
elapsed ms, and the full Issues list (bound to the Validation Panel's SListView)
        |
        v
 if NumFatal > 0: FMessageDialog::Open(...)  -> blocking error dialog
```
