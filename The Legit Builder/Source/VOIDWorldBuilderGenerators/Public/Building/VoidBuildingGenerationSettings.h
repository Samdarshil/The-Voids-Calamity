// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Building/VoidBuildingParams.h"
#include "VoidBuildingGenerationSettings.generated.h"

class UMaterialInterface;
class AActor;

/**
 * UVoidBuildingGenerationSettings
 *
 * Project Settings -> Plugins -> VOID World Builder Building Generation.
 * Same pattern as UVoidRoadGenerationSettings.
 */
UCLASS(Config = VOIDWorldBuilder, DefaultConfig, meta = (DisplayName = "VOID World Builder Building Generation"))
class VOIDWORLDBUILDERGENERATORS_API UVoidBuildingGenerationSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UVoidBuildingGenerationSettings();

	UPROPERTY(Config, EditAnywhere, Category = "Determinism")
	int32 GlobalSeed = 0;

	UPROPERTY(Config, EditAnywhere, Category = "Batching", meta = (ClampMin = "2000.0"))
	float BatchCellSizeUnits = 25000.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Validation", meta = (ClampMin = "100.0"))
	float MinFootprintAreaUnits2 = 40000.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Road Relationship", meta = (ClampMin = "0.0"))
	float MinSetbackUnits = 200.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Road Relationship", meta = (ClampMin = "1.0"))
	float LandmarkSetbackMultiplier = 2.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Road Relationship")
	EVoidBuildingConflictPolicy ConflictPolicy = EVoidBuildingConflictPolicy::AdjustThenSkip;

	UPROPERTY(Config, EditAnywhere, Category = "Road Relationship", meta = (ClampMin = "0.0"))
	float MaxAdjustmentUnits = 800.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Road Relationship", meta = (ClampMin = "0.0"))
	float MaxFrontageSearchUnits = 3000.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Road Relationship")
	bool bAlignBaseToFrontageRoad = true;

	UPROPERTY(Config, EditAnywhere, Category = "Detail")
	bool bGenerateEntrances = true;

	UPROPERTY(Config, EditAnywhere, Category = "Detail")
	bool bGenerateWindows = true;

	UPROPERTY(Config, EditAnywhere, Category = "Detail")
	bool bGenerateRooftopStructures = true;

	UPROPERTY(Config, EditAnywhere, Category = "Detail")
	bool bGenerateFloorBands = true;

	UPROPERTY(Config, EditAnywhere, Category = "Detail")
	EVoidBuildingWindowDetail WindowDetail = EVoidBuildingWindowDetail::Auto;

	UPROPERTY(Config, EditAnywhere, Category = "Detail", meta = (ClampMin = "0"))
	int32 MaxWindowQuadsPerBuilding = 500;

	UPROPERTY(Config, EditAnywhere, Category = "Detail", meta = (ClampMin = "0.0"))
	float FoundationDepthUnits = 150.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Performance")
	bool bGenerateCollision = true;

	/** Single shared material applied to every batch section (reads vertex colour). Leave empty to keep the engine default. No dynamic instances are ever created. */
	UPROPERTY(Config, EditAnywhere, Category = "Materials")
	TSoftObjectPtr<UMaterialInterface> GreyboxMaterial;

	/** Extend the built-in type vocabulary: lower-case substring of buildingType -> category. Checked first. */
	UPROPERTY(Config, EditAnywhere, Category = "Classification")
	TMap<FString, EVoidBuildingCategory> KeywordOverrides;

	/** Asset-replacement hook: building id -> actor class spawned in place of the greybox. */
	UPROPERTY(Config, EditAnywhere, Category = "Asset Replacement")
	TMap<FName, TSoftClassPtr<AActor>> AssetOverrides;

	/** Fills a params snapshot. RoadProfileTable is resolved from the Road Generator's settings so corridors match. */
	FVoidBuildingGenerationParams ToParams() const;
};
