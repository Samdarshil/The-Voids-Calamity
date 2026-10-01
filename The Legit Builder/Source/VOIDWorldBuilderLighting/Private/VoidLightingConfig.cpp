// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VoidLightingConfig.h"
#include "VoidLightingLog.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h"
#include "Interfaces/IPluginManager.h"

namespace VoidLightingConfigPrivate
{
	template <typename StructType>
	static bool LoadStructFromJsonFile(const FString& FilePath, StructType& OutStruct)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *FilePath))
		{
			UE_LOG(LogVoidLighting, Warning, TEXT("Lighting config not found: %s"), *FilePath);
			return false;
		}

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			UE_LOG(LogVoidLighting, Error, TEXT("Lighting config is not valid JSON: %s"), *FilePath);
			return false;
		}

		// CheckFlags = 0, SkipFlags = 0: accept every property. Unknown JSON keys are ignored by the converter.
		if (!FJsonObjectConverter::JsonObjectToUStruct(Root.ToSharedRef(), StructType::StaticStruct(), &OutStruct, 0, 0))
		{
			UE_LOG(LogVoidLighting, Error, TEXT("Lighting config could not be mapped to %s: %s"), *StructType::StaticStruct()->GetName(), *FilePath);
			return false;
		}
		return true;
	}
}

FVoidLightingConfig& FVoidLightingConfig::Get()
{
	static FVoidLightingConfig Instance;
	return Instance;
}

FString FVoidLightingConfig::GetDefaultConfigDirectory()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("VOIDWorldBuilder"));
	if (Plugin.IsValid())
	{
		return FPaths::Combine(Plugin->GetBaseDir(), TEXT("Config"), TEXT("VOIDLighting"));
	}
	return FString();
}

bool FVoidLightingConfig::Reload(const FString& OverrideDirectory)
{
	using namespace VoidLightingConfigPrivate;

	LoadedDirectory = OverrideDirectory.IsEmpty() ? GetDefaultConfigDirectory() : OverrideDirectory;
	bPresetsLoaded = false;
	bProfilesLoaded = false;
	Presets.Reset();
	Profiles = FVoidLightingProfilesFile();

	if (LoadedDirectory.IsEmpty())
	{
		UE_LOG(LogVoidLighting, Error, TEXT("Could not resolve the VOIDWorldBuilder plugin directory; lighting config not loaded."));
		return false;
	}

	FVoidLightingPresetFile PresetFile;
	if (LoadStructFromJsonFile(FPaths::Combine(LoadedDirectory, TEXT("LightingPresets.json")), PresetFile))
	{
		Presets = MoveTemp(PresetFile.Presets);
		bPresetsLoaded = Presets.Num() > 0;
	}

	bProfilesLoaded = LoadStructFromJsonFile(FPaths::Combine(LoadedDirectory, TEXT("LightingProfiles.json")), Profiles);

	UE_LOG(LogVoidLighting, Log, TEXT("Lighting config from '%s': %d presets, %d district profiles, %d usage rules, %d road rules, %d landmark tiers."),
		*LoadedDirectory, Presets.Num(), Profiles.Districts.Num(), Profiles.UsageRules.Num(), Profiles.RoadRules.Num(), Profiles.LandmarkTiers.Num());

	return bPresetsLoaded && bProfilesLoaded;
}

void FVoidLightingConfig::SetTablesForTesting(const TArray<FVoidLightingPreset>& InPresets, const FVoidLightingProfilesFile& InProfiles)
{
	Presets = InPresets;
	Profiles = InProfiles;
	bPresetsLoaded = Presets.Num() > 0;
	bProfilesLoaded = true;
}

TArray<FName> FVoidLightingConfig::GetPresetIds() const
{
	TArray<FName> Ids;
	Ids.Reserve(Presets.Num());
	for (const FVoidLightingPreset& Preset : Presets)
	{
		Ids.Add(Preset.Id);
	}
	return Ids;
}

bool FVoidLightingConfig::FindPreset(FName Id, FVoidLightingPreset& OutPreset) const
{
	for (const FVoidLightingPreset& Preset : Presets)
	{
		if (Preset.Id == Id)
		{
			OutPreset = Preset;
			return true;
		}
	}
	return false;
}

FVoidDistrictLightingProfile FVoidLightingConfig::FindDistrictProfile(FName DistrictId, bool* bOutMatched) const
{
	if (bOutMatched)
	{
		*bOutMatched = false;
	}

	for (const FVoidDistrictLightingProfile& Profile : Profiles.Districts)
	{
		if (Profile.Id == DistrictId || Profile.MatchIds.Contains(DistrictId))
		{
			if (bOutMatched)
			{
				*bOutMatched = true;
			}
			return Profile;
		}
	}
	return FVoidDistrictLightingProfile();
}

FVoidUsageRule FVoidLightingConfig::ClassifyBuilding(const FString& BuildingType, bool* bOutMatched) const
{
	if (bOutMatched)
	{
		*bOutMatched = false;
	}

	const FString Lower = BuildingType.ToLower();
	if (!Lower.IsEmpty())
	{
		for (const FVoidUsageRule& Rule : Profiles.UsageRules)
		{
			for (const FString& Keyword : Rule.Keywords)
			{
				if (!Keyword.IsEmpty() && Lower.Contains(Keyword))
				{
					if (bOutMatched)
					{
						*bOutMatched = true;
					}
					return Rule;
				}
			}
		}
	}
	return GetUsageRule(EVoidLightingUsage::Unknown);
}

FVoidUsageRule FVoidLightingConfig::GetUsageRule(EVoidLightingUsage Usage) const
{
	for (const FVoidUsageRule& Rule : Profiles.UsageRules)
	{
		if (Rule.Usage == Usage)
		{
			return Rule;
		}
	}
	FVoidUsageRule Fallback;
	Fallback.Usage = Usage;
	return Fallback;
}

FVoidRoadLightingRule FVoidLightingConfig::GetRoadRule(EVoidRoadType RoadType) const
{
	for (const FVoidRoadLightingRule& Rule : Profiles.RoadRules)
	{
		if (Rule.RoadType == RoadType)
		{
			return Rule;
		}
	}
	FVoidRoadLightingRule Fallback;
	Fallback.RoadType = RoadType;
	return Fallback;
}

FVoidLandmarkTierRule FVoidLightingConfig::GetLandmarkTierRule(int32 Tier) const
{
	for (const FVoidLandmarkTierRule& Rule : Profiles.LandmarkTiers)
	{
		if (Rule.Tier == Tier)
		{
			return Rule;
		}
	}
	FVoidLandmarkTierRule Fallback;
	Fallback.Tier = Tier;
	return Fallback;
}
