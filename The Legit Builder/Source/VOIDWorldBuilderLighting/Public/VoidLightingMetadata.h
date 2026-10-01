// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"

/** The subset of DistrictRegistry.json this system reads. */
struct VOIDWORLDBUILDERLIGHTING_API FVoidMeridianDistrictInfo
{
	FName Id;
	FString DisplayName;
	FString RadialBand;
	FString VerticalTier;
	int32 GenerationPriority = 0;

	/** Registry vertical tiers below grade (below_grade_*, absolute_bottom_*) contribute no skyline and get no sun/sky. */
	bool IsBelowGrade() const
	{
		return VerticalTier.Contains(TEXT("below_grade")) || VerticalTier.Contains(TEXT("absolute_bottom"));
	}
};

/** The subset of LandmarkRegistry.json this system reads. */
struct VOIDWORLDBUILDERLIGHTING_API FVoidMeridianLandmarkInfo
{
	FName Id;
	FName DistrictId;
	FString Type;
	FString RecognitionPriority;
	FString NavigationImportance;
	/** 1..4 for skyline landmarks; 0 when the registry marks it interior / not applicable to the skyline. */
	int32 VisibilityTier = 0;

	bool HasSkylinePresence() const { return VisibilityTier > 0; }
};

/**
 * FVoidMeridianLightingMetadata
 *
 * Read-only view over the Meridian registries. It never invents
 * landmarks: a landmark exists here only if LandmarkRegistry.json lists
 * it. Missing files are not an error -- generation continues with no
 * landmark presentation and says so in the log.
 */
class VOIDWORLDBUILDERLIGHTING_API FVoidMeridianLightingMetadata
{
public:
	/** @return true if at least one registry file was read. */
	bool LoadFromDirectory(const FString& Directory, TArray<FString>* OutMessages = nullptr);

	/** Parse-only entry points (used by tests and by LoadFromDirectory). */
	bool ParseDistrictRegistry(const FString& JsonText, TArray<FString>* OutMessages = nullptr);
	bool ParseLandmarkRegistry(const FString& JsonText, TArray<FString>* OutMessages = nullptr);

	const FVoidMeridianDistrictInfo* FindDistrict(FName Id) const;
	const FVoidMeridianLandmarkInfo* FindLandmark(FName Id) const;

	const TArray<FVoidMeridianDistrictInfo>& GetDistricts() const { return Districts; }
	const TArray<FVoidMeridianLandmarkInfo>& GetLandmarks() const { return Landmarks; }

private:
	TArray<FVoidMeridianDistrictInfo> Districts;
	TArray<FVoidMeridianLandmarkInfo> Landmarks;
};
