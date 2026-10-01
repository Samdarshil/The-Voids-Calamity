// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IVoidPackageReader.h"

class FJsonObject;

/**
 * FVoidJsonPackageReader
 *
 * Reads the "DesignPackage.json" format: parses JSON (via FVoidJsonReader),
 * then maps it into an FVoidDesignPackage field-by-field using TryGet*
 * accessors so every missing/malformed field produces a controlled
 * mapping issue in the report rather than an engine-level warning and a
 * silently wrong default.
 *
 * TryReadFromString is exposed alongside the interface's file-based
 * TryRead specifically for FVoidDesignPackageImporter::LoadFromJsonString
 * and for automated tests, both of which have JSON text already in
 * memory and shouldn't need a temp file to exercise this reader.
 */
class VOIDWORLDBUILDERIMPORT_API FVoidJsonPackageReader : public IVoidPackageReader
{
public:
	//~ Begin IVoidPackageReader
	virtual FName GetSupportedExtension() const override;
	virtual bool TryRead(const FString& FilePath, FVoidImportContext& Context, FVoidDesignPackage& OutPackage, FVoidValidationReport& OutReport) const override;
	//~ End IVoidPackageReader

	/** Same mapping pipeline as TryRead, starting from JSON text already in memory instead of a file path. */
	bool TryReadFromString(const FString& RawJson, FVoidImportContext& Context, FVoidDesignPackage& OutPackage, FVoidValidationReport& OutReport) const;

private:
	/** Shared by TryRead and TryReadFromString once each has produced a parsed root JSON object. */
	void MapJsonObjectToPackage(const TSharedPtr<FJsonObject>& JsonObject, FVoidImportContext& Context, FVoidDesignPackage& OutPackage, FVoidValidationReport& OutReport) const;
};
