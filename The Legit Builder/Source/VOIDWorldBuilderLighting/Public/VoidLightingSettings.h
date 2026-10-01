// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "VoidLightingTypes.h"
#include "VoidLightingSettings.generated.h"

class UTexture;
class UMaterialInterface;

/** Explicit landmark -> building association, for packages whose building ids/types don't literally equal a Meridian landmark id. */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERLIGHTING_API FVoidLandmarkBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Binding") FName LandmarkId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Binding") FName BuildingId;
};

/**
 * Project Settings -> Plugins -> VOID World Builder Lighting.
 *
 * Budgets are hard caps enforced at generation time (see the handoff's
 * performance section). Nothing here is read per frame.
 */
UCLASS(Config = VOIDWorldBuilder, DefaultConfig, meta = (DisplayName = "VOID World Builder Lighting"))
class VOIDWORLDBUILDERLIGHTING_API UVoidLightingSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UVoidLightingSettings();

	/** Folder containing the Meridian Master JSON files (DistrictRegistry.json, LandmarkRegistry.json). Leave empty to skip registry metadata (landmarks are then only bound through LandmarkBindings). */
	UPROPERTY(Config, EditAnywhere, Category = "Metadata", meta = (DisplayName = "Meridian Metadata Directory"))
	FDirectoryPath MeridianMetadataDirectory;

	/** Explicit landmark -> building bindings, applied before automatic id/type matching. */
	UPROPERTY(Config, EditAnywhere, Category = "Metadata")
	TArray<FVoidLandmarkBinding> LandmarkBindings;

	/** Optional folder overriding <Plugin>/Config/VOIDLighting (LightingPresets.json, LightingProfiles.json). */
	UPROPERTY(Config, EditAnywhere, Category = "Presets")
	FDirectoryPath ConfigDirectoryOverride;

	/** Preset applied at the end of generation when the director has no active preset yet. */
	UPROPERTY(Config, EditAnywhere, Category = "Presets")
	FName DefaultPresetId = TEXT("Day");

	UPROPERTY(Config, EditAnywhere, Category = "Existing Lighting")
	EVoidExistingLightingPolicy ExistingLightingPolicy = EVoidExistingLightingPolicy::Adopt;

	UPROPERTY(Config, EditAnywhere, Category = "Existing Lighting")
	bool bGeneratePostProcessVolume = true;

	/** Lower priority than typical hand-authored volumes and cinematic camera post-process, so those still win. */
	UPROPERTY(Config, EditAnywhere, Category = "Existing Lighting")
	float PostProcessPriority = -10.0f;

	/** Optional colour-grade LUT used by presets that set bApplyGradingLUT. */
	UPROPERTY(Config, EditAnywhere, Category = "Post Process")
	TSoftObjectPtr<UTexture> ColorGradingLUT;

	/** Content path where the emissive master material is created if no override is set. */
	UPROPERTY(Config, EditAnywhere, Category = "Materials")
	FString GeneratedMaterialPath = TEXT("/Game/VOID/Lighting/M_VOID_LightingEmissive");

	/** Optional replacement. It must expose vector params LightColorA/LightColorB, scalar params Intensity/LitFraction and read per-instance custom data 0 (threshold) and 1 (blend). */
	UPROPERTY(Config, EditAnywhere, Category = "Materials")
	TSoftObjectPtr<UMaterialInterface> EmissiveMaterialOverride;

	/** Z of the ground under building footprints. The supplied package format has no building elevation and no Building generator exists yet, so this is the single assumption used for facade lights. */
	UPROPERTY(Config, EditAnywhere, Category = "Metadata")
	float BuildingGroundZ = 0.0f;

	// --- Budgets -------------------------------------------------------

	/** Cap on real (dynamic) street + intersection lights per generated district. Everything else is an emissive lamp head. */
	UPROPERTY(Config, EditAnywhere, Category = "Budgets", meta = (ClampMin = "0"))
	int32 MaxRealStreetLightsPerDistrict = 256;

	/** Cap on real lights per landmark actor is fixed by tier (see LightingProfiles.json UplightCount + 1 halo). This caps how many buildings of one landmark cluster get an actor. */
	UPROPERTY(Config, EditAnywhere, Category = "Budgets", meta = (ClampMin = "1"))
	int32 MaxLandmarkBuildingsPerLandmark = 6;

	UPROPERTY(Config, EditAnywhere, Category = "Budgets", meta = (ClampMin = "0"))
	int32 MaxWindowInstancesPerDistrict = 80000;

	UPROPERTY(Config, EditAnywhere, Category = "Budgets", meta = (ClampMin = "0"))
	int32 MaxWindowInstancesPerBuilding = 1200;

	/** Cap per street element kind (poles, lamp heads, traffic parts) per district. */
	UPROPERTY(Config, EditAnywhere, Category = "Budgets", meta = (ClampMin = "0"))
	int32 MaxStreetInstancesPerElement = 30000;

	/** 0 = no culling. Otherwise instances beyond this distance are culled -- also removes far skyline windows, so leave 0 if skyline readability matters. */
	UPROPERTY(Config, EditAnywhere, Category = "Budgets", meta = (ClampMin = "0"))
	float InstanceEndCullDistance = 0.0f;

	/** Real street lights are shadowless by default (a virtual-shadow-map cost per light otherwise). */
	UPROPERTY(Config, EditAnywhere, Category = "Budgets")
	bool bRealStreetLightsCastShadows = false;

	// --- Cinematic hooks -------------------------------------------------

	UPROPERTY(Config, EditAnywhere, Category = "Cinematic Hooks", meta = (ClampMin = "0"))
	int32 SkylinePointCount = 8;

	UPROPERTY(Config, EditAnywhere, Category = "Cinematic Hooks", meta = (ClampMin = "0"))
	float SkylineMinSeparationUnits = 4000.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bVerboseLogging = false;

	//~ Begin UDeveloperSettings
	virtual FName GetCategoryName() const override { return FName(TEXT("Plugins")); }
	//~ End UDeveloperSettings
};
