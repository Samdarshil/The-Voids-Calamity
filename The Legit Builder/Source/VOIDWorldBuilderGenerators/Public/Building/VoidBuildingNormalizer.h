// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Data/VoidDesignPackage.h"
#include "Data/VoidValidationReport.h"
#include "Building/VoidBuildingTypes.h"
#include "Building/VoidBuildingParams.h"

/**
 * FVoidBuildingNormalizer
 *
 * Turns the imported FVoidBuildingSpec list into FVoidNormalizedBuilding
 * (the generator's internal working form). It does not re-read JSON: the
 * input is the FVoidDistrictData the existing VOID importer already produced.
 *
 * Output order is by building id (ordinal, case-insensitive), so results do
 * not depend on the order buildings appear in the source file.
 */
class VOIDWORLDBUILDERGENERATORS_API FVoidBuildingNormalizer
{
public:
	static void Normalize(
		const FVoidDistrictData& District,
		const FVoidBuildingGenerationParams& Params,
		FVoidValidationReport& InOutReport,
		TArray<FVoidNormalizedBuilding>& OutBuildings);

	/** Maps a free-form buildingType tag to a category via keyword overrides, then the built-in vocabulary. */
	static EVoidBuildingCategory Classify(const FString& TypeTag, const TMap<FString, EVoidBuildingCategory>& KeywordOverrides);

	/** Nominal storey height for a category; the real per-building FloorHeight is HeightUnits / FloorCount. */
	static float NominalFloorHeight(EVoidBuildingCategory Category);

	/** Stable per-building seed from (Id, GlobalSeed). */
	static int32 MakeSeed(const FName& Id, int32 GlobalSeed);

	/** "Building.Office.HighRise", "Building.Landmark", ... */
	static FName MakeAssetCategory(const FVoidNormalizedBuilding& Building);

	static const TCHAR* CategoryToString(EVoidBuildingCategory Category);
};
