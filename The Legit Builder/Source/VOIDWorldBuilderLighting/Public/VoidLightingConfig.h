// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "VoidLightingTypes.h"

/**
 * FVoidLightingConfig
 *
 * Loads and caches the data-driven lighting tables from
 * <Plugin>/Config/VOIDLighting/{LightingPresets,LightingProfiles}.json
 * (or ConfigDirectoryOverride). JSON is the single source of truth; the
 * only C++ fallbacks are the struct defaults (a "Day"-like preset, a
 * neutral district profile, a generic road rule) so that a missing file
 * degrades to something visible rather than a crash.
 *
 * Editor/generation-time use. Runtime actors carry their own copies of
 * whatever they need (see AVoidLightingDirector::PresetLibrary).
 */
class VOIDWORLDBUILDERLIGHTING_API FVoidLightingConfig
{
public:
	static FVoidLightingConfig& Get();

	/** (Re)reads both JSON files. Returns true if BOTH loaded. Safe to call repeatedly. */
	bool Reload(const FString& OverrideDirectory = FString());

	bool HasLoadedPresets() const { return bPresetsLoaded; }
	bool HasLoadedProfiles() const { return bProfilesLoaded; }

	const TArray<FVoidLightingPreset>& GetPresets() const { return Presets; }
	TArray<FName> GetPresetIds() const;
	bool FindPreset(FName Id, FVoidLightingPreset& OutPreset) const;

	/** Never fails: returns the neutral default profile when nothing matches. bOutMatched reports whether a real profile matched. */
	FVoidDistrictLightingProfile FindDistrictProfile(FName DistrictId, bool* bOutMatched = nullptr) const;

	/** Keyword match against FVoidBuildingSpec::BuildingType. Unknown types resolve to the Unknown rule (or struct defaults). */
	FVoidUsageRule ClassifyBuilding(const FString& BuildingType, bool* bOutMatched = nullptr) const;
	FVoidUsageRule GetUsageRule(EVoidLightingUsage Usage) const;

	FVoidRoadLightingRule GetRoadRule(EVoidRoadType RoadType) const;
	FVoidLandmarkTierRule GetLandmarkTierRule(int32 Tier) const;

	const FString& GetLoadedDirectory() const { return LoadedDirectory; }
	static FString GetDefaultConfigDirectory();

	/** Test hook: replace tables in memory without touching disk. */
	void SetTablesForTesting(const TArray<FVoidLightingPreset>& InPresets, const FVoidLightingProfilesFile& InProfiles);

private:
	FVoidLightingConfig() = default;

	bool bPresetsLoaded = false;
	bool bProfilesLoaded = false;
	FString LoadedDirectory;

	TArray<FVoidLightingPreset> Presets;
	FVoidLightingProfilesFile Profiles;
};
