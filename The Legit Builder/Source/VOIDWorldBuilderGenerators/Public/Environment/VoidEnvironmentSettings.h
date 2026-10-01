// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/DataTable.h"
#include "VoidEnvironmentSettings.generated.h"

/**
 * UVoidEnvironmentSettings
 *
 * Project-wide configuration for the Environment (Phase 12) and Prop
 * (Phase 14) generators. Same pattern as UVoidRoadGenerationSettings /
 * UVoidImportSettings: a UDeveloperSettings class, config in
 * DefaultVOIDWorldBuilder.ini, shown in Project Settings -> Plugins.
 *
 * Everything works with every table left empty: built-in rules, built-in
 * district profiles and engine-BasicShapes placeholders are used. Tables
 * are an enhancement, never a prerequisite -- the same contract as the Road
 * type profile table.
 */
UCLASS(Config = VOIDWorldBuilder, DefaultConfig, meta = (DisplayName = "VOID World Builder Environment And Props"))
class VOIDWORLDBUILDERGENERATORS_API UVoidEnvironmentSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UVoidEnvironmentSettings();

	// ---- Determinism ---------------------------------------------------

	/** Global seed. Same package + same seed + same settings => same city. Change it to get a different (but again stable) variation. */
	UPROPERTY(Config, EditAnywhere, Category = "Determinism")
	int32 GlobalSeed = 1447252036; // 'VOID'

	// ---- Density / budget ------------------------------------------------

	/** Global density multiplier applied on top of every rule's probability and every district profile. */
	UPROPERTY(Config, EditAnywhere, Category = "Density", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float DensityScale = 1.0f;

	/** Fraction of full density kept at importance 0 (far from roads' tier and cinematic focus). 1 = ignore importance. */
	UPROPERTY(Config, EditAnywhere, Category = "Density", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinImportanceDensity = 0.35f;

	/** Hard cap per generator run. When exceeded, lowest-priority instances (least cinematic-relevant) are dropped first. */
	UPROPERTY(Config, EditAnywhere, Category = "Density", meta = (ClampMin = "0"))
	int32 MaxInstancesPerRun = 200000;

	// ---- Cinematic priority ---------------------------------------------

	/** Extra cinematic focus points (world space): camera areas, landmarks. Plazas and 3+ way junctions are added automatically. */
	UPROPERTY(Config, EditAnywhere, Category = "Cinematic")
	TArray<FVector> CinematicFocusPoints;

	UPROPERTY(Config, EditAnywhere, Category = "Cinematic", meta = (ClampMin = "100.0"))
	float FocusRadiusUnits = 6000.0f;

	// ---- Instancing --------------------------------------------------------

	/** Side of the square streaming cell each output actor covers. Also the HISM culling / World Partition granularity. */
	UPROPERTY(Config, EditAnywhere, Category = "Instancing", meta = (ClampMin = "1000.0"))
	float CellSizeUnits = 25600.0f;

	/** Shoulder width used for roads with no sidewalk (roadside props stand here). */
	UPROPERTY(Config, EditAnywhere, Category = "Instancing", meta = (ClampMin = "0.0"))
	float ShoulderWidthUnits = 150.0f;

	// ---- Traffic ----------------------------------------------------------

	UPROPERTY(Config, EditAnywhere, Category = "Traffic")
	bool bLeftHandTraffic = false;

	// ---- Data tables (all optional) ------------------------------------------

	/** Rows of FVoidPlacementRuleRow. If set, REPLACES the built-in rule set (designer is authoritative). */
	UPROPERTY(Config, EditAnywhere, Category = "Data", meta = (RequiredAssetDataTags = "RowStructure=/Script/VOIDWorldBuilderGenerators.VoidPlacementRuleRow"))
	TSoftObjectPtr<UDataTable> PlacementRuleTable;

	/** Rows of FVoidAssetSlotRow. The replacement hook: real meshes per Category. Missing categories fall back to placeholders. */
	UPROPERTY(Config, EditAnywhere, Category = "Data", meta = (RequiredAssetDataTags = "RowStructure=/Script/VOIDWorldBuilderGenerators.VoidAssetSlotRow"))
	TSoftObjectPtr<UDataTable> AssetSlotTable;

	/** Rows of FVoidDistrictEnvProfileRow keyed by district id. Unlisted districts use built-in Meridian defaults, else neutral. */
	UPROPERTY(Config, EditAnywhere, Category = "Data", meta = (RequiredAssetDataTags = "RowStructure=/Script/VOIDWorldBuilderGenerators.VoidDistrictEnvProfileRow"))
	TSoftObjectPtr<UDataTable> DistrictProfileTable;

	/** Rows of FVoidBuildingUseTokenRow: how BuildingType text maps to Commercial / Park / Plaza / Construction. */
	UPROPERTY(Config, EditAnywhere, Category = "Data", meta = (RequiredAssetDataTags = "RowStructure=/Script/VOIDWorldBuilderGenerators.VoidBuildingUseTokenRow"))
	TSoftObjectPtr<UDataTable> BuildingUseTokenTable;

	// ---- PCG hook ------------------------------------------------------------

	/** Writes every planned instance (id, category, context, transform, importance) to JSON so a PCG graph / external tool can consume the same points. No PCG dependency. */
	UPROPERTY(Config, EditAnywhere, Category = "PCG Hook")
	bool bExportPlacementJson = false;

	/** Relative to the project's Saved dir when not absolute. */
	UPROPERTY(Config, EditAnywhere, Category = "PCG Hook")
	FString ExportDirectory = TEXT("VOIDWorldBuilder/Placements");
};
