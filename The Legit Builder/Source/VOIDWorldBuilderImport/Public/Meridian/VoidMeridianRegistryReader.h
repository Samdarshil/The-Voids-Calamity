// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Data/VoidValidationReport.h"
#include "Meridian/VoidMeridianRegistry.h"

/**
 * FVoidMeridianRegistryReader
 *
 * Reads the Meridian registry set. Follows BuilderRules.json:
 *   - Meridian_Master.json is the single manifest; every registry_references
 *     entry marked required must exist on disk or import is Fatal
 *     (BuilderRules missing_reference_error: HALT at step 1).
 *   - Per-district *_data.json files are OPTIONAL here: a missing one is a
 *     Warning (the district is generated from registry-level data only), not
 *     a halt, because the authoritative per-district exports are not always
 *     shipped alongside the registries.
 *   - No coordinates are ever read from Meridian files (none exist by
 *     policy). Coordinates only enter through LoadOverridesFromFile.
 *
 * All Parse* functions take JSON text so automation tests need no files.
 */
class VOIDWORLDBUILDERIMPORT_API FVoidMeridianRegistryReader
{
public:
	/** Loads Meridian_Master.json and every registry the District Generator consumes from Directory. */
	static bool LoadFromDirectory(const FString& Directory, FVoidMeridianDataSet& OutData, FVoidValidationReport& OutReport);

	/** Loads VoidDistrictLayoutOverrides.json. A missing file is not an error (returns true with empty overrides) unless bRequired. */
	static bool LoadOverridesFromFile(const FString& FilePath, FVoidMeridianLayoutOverrides& OutOverrides, FVoidValidationReport& OutReport, bool bRequired = false);

	static bool ParseMasterManifest(const FString& Json, FVoidMeridianDataSet& InOutData, TArray<FString>& OutRequiredFiles, FVoidValidationReport& OutReport);
	static bool ParseDistrictRegistry(const FString& Json, FVoidMeridianDataSet& InOutData, FVoidValidationReport& OutReport);
	static bool ParseLandmarkRegistry(const FString& Json, FVoidMeridianDataSet& InOutData, FVoidValidationReport& OutReport);
	static bool ParseRoadNetwork(const FString& Json, FVoidMeridianDataSet& InOutData, FVoidValidationReport& OutReport);

	/** Applies one per-district data file to the matching entry in InOutData. */
	static bool ParseDistrictData(const FString& Json, FVoidMeridianDataSet& InOutData, FVoidValidationReport& OutReport);

	static bool ParseLayoutOverrides(const FString& Json, FVoidMeridianLayoutOverrides& OutOverrides, FVoidValidationReport& OutReport);
};
