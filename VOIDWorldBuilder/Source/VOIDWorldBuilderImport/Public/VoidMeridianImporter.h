// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Data/VoidMeridianData.h"
#include "Data/VoidValidationReport.h"
#include "Data/VoidImportContext.h"
#include "VoidMeridianImporter.generated.h"

/** Options for one Meridian import. FromSettings() snapshots the project settings. */
struct VOIDWORLDBUILDERIMPORT_API FVoidMeridianImportOptions
{
	/** Meridian's --strict-canon-lock. Checksum MISMATCH is an Error when true, a Warning when false. Absent locked inputs are always only a Warning. */
	bool bStrictCanonLock = true;

	/** Skip SHA-256 verification entirely (fast unit tests / in-memory fixtures). */
	bool bVerifyChecksums = true;

	static FVoidMeridianImportOptions FromSettings();
};

/**
 * Result of importing Meridian_Master.json and every registry it references.
 * WasSuccessful() == "no Error/Fatal issues": the only state in which generators may consume World.
 */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERIMPORT_API FVoidMeridianImportResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	FVoidMeridianWorld World;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	FVoidValidationReport ValidationReport;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	FVoidImportContext Context;

	/** Registry keys actually loaded, in load (import_order) order. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	TArray<FName> LoadedRegistries;

	bool WasSuccessful() const { return ValidationReport.bIsValid; }
};

/**
 * FVoidMeridianImporter
 *
 * Imports the REAL Meridian_Master package. Follows the package's own rules:
 *   1. Only Meridian_Master.json is opened by path; every other file is resolved THROUGH it
 *      (registry_references), in generation_pipeline.import_order.
 *   2. A required registry that is missing or unparseable halts the import (BuilderRules.json
 *      error_handling.missing_reference_error).
 *   3. Each registry is checked against its own *.schema.json (VR-002), cross-references are resolved
 *      (VR-003), duplicate ids are rejected, import order must satisfy module_dependencies, and
 *      no coordinates / units / transforms may appear (VR-012).
 *   4. With bStrictCanonLock, PackageManifest.json SHA-256 checksums are verified (VR-011).
 *
 * The legacy FVoidDesignPackageImporter is untouched and still used for FVoidDesignPackage files.
 */
class VOIDWORLDBUILDERIMPORT_API FVoidMeridianImporter
{
public:
	/** MasterJsonPath must point at Meridian_Master.json (the single import file). */
	static FVoidMeridianImportResult LoadFromMasterFile(const FString& MasterJsonPath, const FVoidMeridianImportOptions& Options);

	/** Convenience: uses FVoidMeridianImportOptions::FromSettings(). */
	static FVoidMeridianImportResult LoadFromMasterFile(const FString& MasterJsonPath);

	/** Accepts either a path to Meridian_Master.json or to the folder containing it. */
	static FVoidMeridianImportResult LoadFromPath(const FString& FileOrFolder);

	/** Parses a schema id like "void_road_network_schema_v1" -> Family "void_road_network_schema", Major 1. False if not in that shape. */
	static bool TryParseSchemaId(const FString& SchemaId, FString& OutFamily, int32& OutMajor);

	/** The schema major version this importer understands for every Meridian schema (all are currently v1). */
	static int32 SupportedSchemaMajor();
};
