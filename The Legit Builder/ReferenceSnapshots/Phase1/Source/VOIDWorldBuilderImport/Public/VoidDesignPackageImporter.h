// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "VoidImportResult.h"

class FJsonObject;

/**
 * FVoidDesignPackageImporter
 *
 * Top-level entry point for the import pipeline: JSON in, validated
 * FVoidDesignPackage out. Composes FVoidJsonReader (parsing) and
 * FVoidPackageValidator (validation) so calling code -- the Editor
 * panel, the commandlet, automated tests -- has a single call to make
 * rather than needing to know the pipeline's internal steps.
 */
class VOIDWORLDBUILDERIMPORT_API FVoidDesignPackageImporter
{
public:
	/** Loads, maps, and validates a design package from a JSON file on disk. */
	static FVoidImportResult LoadFromFile(const FString& FilePath);

	/** Loads, maps, and validates a design package from an in-memory JSON string. Kept separate from LoadFromFile so automated tests don't depend on the filesystem. */
	static FVoidImportResult LoadFromJsonString(const FString& RawJson);

private:
	/** Maps a parsed JSON object into an FVoidDesignPackage. Does not validate -- validation is a separate, explicit step run by the caller. */
	static FVoidDesignPackage MapJsonToPackage(const TSharedPtr<FJsonObject>& JsonObject, TArray<FString>& OutMappingWarnings);
};
