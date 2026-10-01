// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "District/VoidDistrictLayoutParams.h"
#include "VoidDistrictGenerationSettings.generated.h"

/**
 * Edit -> Project Settings -> Plugins -> VOID World Builder District Generation.
 * Same UDeveloperSettings pattern as the Import and Road settings.
 */
UCLASS(Config = VOIDWorldBuilder, DefaultConfig, meta = (DisplayName = "VOID World Builder District Generation"))
class VOIDWORLDBUILDERGENERATORS_API UVoidDistrictGenerationSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UVoidDistrictGenerationSettings();

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/** Folder containing Meridian_Master.json and the registries it references. */
	UPROPERTY(Config, EditAnywhere, Category = "Meridian Data", meta = (RelativeToGameDir = false))
	FDirectoryPath MeridianDataDirectory;

	/** Optional VoidDistrictLayoutOverrides.json (boundary polygons / landmark positions). Empty = none. */
	UPROPERTY(Config, EditAnywhere, Category = "Meridian Data", meta = (FilePathFilter = "json"))
	FFilePath LayoutOverridesFile;

	UPROPERTY(Config, EditAnywhere, Category = "Layout")
	FVoidDistrictLayoutParams Layout;

	/** Per-district character overrides keyed by district id (e.g. white_zones). */
	UPROPERTY(Config, EditAnywhere, Category = "Character")
	TMap<FName, FVoidDistrictCharacterOverride> CharacterOverrides;

	/** If a generator registered as "Building" exists, hand it the planned buildings instead of using built-in greybox instancing. */
	UPROPERTY(Config, EditAnywhere, Category = "Integration")
	bool bPreferRegisteredBuildingGenerator = true;

	/** Generate even when validation reports Errors (never recommended; for debugging a plan). */
	UPROPERTY(Config, EditAnywhere, Category = "Integration")
	bool bContinueOnValidationErrors = false;

	/** Organise generated actors into Outliner folders (Meridian/<district>/<Roads|Buildings|Landmarks>). Data Layers / World Partition assignment is a later phase. */
	UPROPERTY(Config, EditAnywhere, Category = "Integration")
	bool bUseRegionFolders = true;
};
