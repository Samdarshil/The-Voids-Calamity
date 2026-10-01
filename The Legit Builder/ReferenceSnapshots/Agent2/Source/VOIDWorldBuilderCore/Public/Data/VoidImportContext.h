// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Data/VoidSchemaVersion.h"
#include "VoidImportContext.generated.h"

/**
 * FVoidImportContext
 *
 * Carries per-call state through the import pipeline: where the data came
 * from, a snapshot of the relevant import settings, and timing data for
 * the performance summary. A USTRUCT (not a plain struct) because it's
 * exposed as a field on FVoidImportResult for the Editor panel and
 * commandlet to read.
 *
 * The settings fields here are a *copy*, taken at the start of an import
 * call, not a live reference to UVoidImportSettings. That's deliberate:
 * a long-running import shouldn't change behavior partway through if
 * someone edits Project Settings while it's in flight, and Core (where
 * this struct lives) has no business depending on the Import module's
 * UDeveloperSettings class in the first place.
 */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERCORE_API FVoidImportContext
{
	GENERATED_BODY()

	/** Where this import came from: a file path, or a descriptive placeholder for in-memory sources. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	FString SourceDescription;

	/** Snapshot of UVoidImportSettings::bFailOnUnknownFields at import time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	bool bFailOnUnknownFields = false;

	/** Snapshot of UVoidImportSettings::MinimumSupportedSchemaVersion at import time. Currently informational; enforced compatibility logic lives in FVoidPackageValidator against FVoidSchemaVersion::CurrentToolVersion(). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	FVoidSchemaVersion MinimumSupportedSchemaVersion = FVoidSchemaVersion(1, 0);

	/** Filled in by FVoidDesignPackageImporter after the full read+map+validate call completes. Readers and the validator do not set this themselves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	double ElapsedMilliseconds = 0.0;
};
