// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "District/VoidDistrictActors.h"
#include "VoidDistrictQueryLibrary.generated.h"

/**
 * Read-only lookups over generated Meridian districts. This is the public
 * surface Environment, Lighting, the Cinematic camera and Asset Replacement
 * agents use; they never need to know how districts were generated.
 */
UCLASS()
class VOIDWORLDBUILDERGENERATORS_API UVoidDistrictQueryLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "VOID Meridian", meta = (WorldContext = "WorldContextObject"))
	static AVoidMeridianRootActor* GetMeridianRoot(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "VOID Meridian", meta = (WorldContext = "WorldContextObject"))
	static AVoidDistrictActor* FindDistrict(const UObject* WorldContextObject, FName DistrictId);

	/** DistrictId = NAME_None returns landmarks of every district. Sorted by visibility tier (skyline first), then id. */
	UFUNCTION(BlueprintCallable, Category = "VOID Meridian", meta = (WorldContext = "WorldContextObject"))
	static TArray<AVoidDistrictLandmarkActor*> GetLandmarks(const UObject* WorldContextObject, FName DistrictId, bool bSkylineOnly = false);

	/** First instance with this registry id. Per-node fixtures have several; use GetLandmarks for those. */
	UFUNCTION(BlueprintCallable, Category = "VOID Meridian", meta = (WorldContext = "WorldContextObject"))
	static AVoidDistrictLandmarkActor* FindLandmark(const UObject* WorldContextObject, FName LandmarkRegistryId);

	UFUNCTION(BlueprintCallable, Category = "VOID Meridian", meta = (WorldContext = "WorldContextObject"))
	static bool GetDistrictProfile(const UObject* WorldContextObject, FName DistrictId, FVoidDistrictProfileData& OutProfile);

	UFUNCTION(BlueprintCallable, Category = "VOID Meridian", meta = (WorldContext = "WorldContextObject"))
	static TArray<FVoidDistrictPortInfo> GetPorts(const UObject* WorldContextObject, FName DistrictId);
};
