# Extension Guide

## Adding a new file format reader (e.g. YAML)

1. Implement `IVoidPackageReader` (from `VOIDWorldBuilderCore`,
   `Interfaces/IVoidPackageReader.h`):

   ```cpp
   class FVoidYamlPackageReader : public IVoidPackageReader
   {
   public:
       virtual FName GetSupportedExtension() const override { return FName(TEXT("yaml")); }

       virtual bool TryRead(const FString& FilePath, FVoidImportContext& Context,
           FVoidDesignPackage& OutPackage, FVoidValidationReport& OutReport) const override
       {
           // Read FilePath, parse YAML, map into OutPackage.
           // On any failure that makes further validation meaningless,
           // call OutReport.AddFatal(...) and return false.
           // Never throw; never crash on malformed input.
       }
   };
   ```

2. Register it, alongside the existing JSON registration, in
   `FVOIDWorldBuilderImportModule::StartupModule()`:

   ```cpp
   FVoidPackageReaderRegistry::Get().RegisterReader(MakeShared<FVoidYamlPackageReader>());
   ```

   And unregister the matching extension in `ShutdownModule()`:

   ```cpp
   FVoidPackageReaderRegistry::Get().UnregisterReader(TEXT("yaml"));
   ```

3. Add a `.Build.cs` dependency for whatever YAML parsing library you use
   (this is exactly why `IVoidPackageReader` lives in Core and doesn't
   depend on any specific parsing library -- Core stays clean regardless
   of what Import module dependencies grow to support new formats).

4. Add tests mirroring `VoidDesignPackageImporterTests.cpp` and
   `VoidPackageReaderRegistryTests.cpp` for the new format.

**Nothing else changes.** `FVoidDesignPackageImporter::LoadFromFile`,
the Editor panel, the commandlet, `FVoidPackageValidator`, and any
Phase 3+ generator all continue working unmodified -- they only ever see
an `FVoidDesignPackage`, never a format-specific representation.

## Adding a new validation rule

1. Decide which stage it belongs to: schema version, metadata, or
   district/building/road. Add the check to the matching
   `FVoidPackageValidator::Validate*` method, or add a new one and call
   it from `Validate`.
2. Choose a severity (see `Docs/DeveloperGuide.md`'s table) and a code
   following `VOID.Import.<Reason>`.
3. Always provide a concrete `SuggestedFix` for Error/Fatal issues --
   "add a non-empty X field" beats "invalid X."
4. Add the code to `Docs/ValidationRules.md`'s table in the same change.
5. Add a test: one fixture that trips the rule, one that doesn't.

## Adding a new Import Setting

Add the field to `UVoidImportSettings` (`VOIDWorldBuilderImport/Public/VoidImportSettings.h`)
with a `UPROPERTY(Config, EditAnywhere, ...)`. It will appear in Project
Settings automatically. If the setting should affect per-call behavior
(like `bFailOnUnknownFields` does), add a matching field to
`FVoidImportContext` and copy it across in
`FVoidDesignPackageImporter::MakeContext` -- readers and the validator
should read the `Context`'s copy, never `UVoidImportSettings` directly,
so Core stays decoupled from the Import module's settings class.
