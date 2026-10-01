// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IVoidValidator.h"

/**
 * In-memory view of a Meridian_Master package directory: file name -> raw
 * bytes. Raw bytes (not text) because VR-011 checksums are over bytes.
 * Validation takes this instead of a path so tests and future remote/zip
 * sources need no disk.
 */
struct VOIDWORLDBUILDERVALIDATION_API FVoidMeridianFileSet
{
	TMap<FString, TArray<uint8>> Files;

	void AddText(const FString& Name, const FString& Text);
	void AddBytes(const FString& Name, const TArray<uint8>& Bytes) { Files.Add(Name, Bytes); }
	bool Contains(const FString& Name) const { return Files.Contains(Name); }

	/** Loads every regular file directly inside Directory (non-recursive; the package is flat). */
	bool AddFromDirectory(const FString& Directory, FString& OutError);
};

/** Facts extracted while validating, for downstream stages (pipeline, other generators). Populated even when errors were found, as far as the data allowed. */
struct VOIDWORLDBUILDERVALIDATION_API FVoidMeridianSummary
{
	bool bManifestLoaded = false;
	int32 NumFiles = 0;
	int32 NumJsonParsed = 0;
	TSet<FName> DistrictIds;
	TArray<FName> RouteIds;
	/** Meridian_Master.generation_pipeline.generation_order targets, in order. NOT district ids ("live_network_spine" etc.). */
	TArray<FName> GenerationTargets;
	/** Ids of flagged_open_items that block generation of specific content. Builder must skip that content unless explicitly overridden. */
	TArray<FString> BlockedContentIds;
	TArray<FString> MissingFiles;
	TArray<FString> FilesWithoutSchema;
};

/**
 * FVoidMeridianValidator
 *
 * Implements the package's own rules (ValidationSchema.json VR-001..VR-012)
 * plus integration checks the package's authors listed as known failure
 * patterns (BuilderManifest.json logging_requirements.flag_precedent).
 * See Docs/ValidationRules.md for the full code table and Tools/
 * meridian_reference_validator.py for the Python oracle it is tested against.
 *
 * All rules are conditional on the relevant file being present, so a partial
 * package yields precise "X is missing" errors instead of a cascade.
 */
class VOIDWORLDBUILDERVALIDATION_API FVoidMeridianValidator
{
public:
	static void Validate(const FVoidMeridianFileSet& FileSet, FVoidValidationContext& Context, FVoidMeridianSummary* OutSummary = nullptr);

	/** Convenience: loads Directory, validates, returns a fresh report (bIsValid computed). */
	static FVoidValidationReport ValidateDirectory(const FString& Directory, const FVoidValidationOptions& Options, FVoidMeridianSummary* OutSummary = nullptr);

	/** The five canon-locked district ids (VR-007). */
	static const TArray<FString>& GetExpectedDistrictIds();

	/** Highest schema major version (the "_vN" suffix of $schema ids) this build understands. */
	static int32 GetSupportedSchemaMajor() { return 1; }
};
