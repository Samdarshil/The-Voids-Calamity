// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "VoidLightingActorBase.h"
#include "VoidDistrictLightingActor.generated.h"

/**
 * AVoidDistrictLightingActor
 *
 * One per generated district. Owns that district's street furniture
 * (poles, lamp heads, traffic + pedestrian signals), window plates, neon
 * hooks and emergency beacons as instanced meshes, plus a budgeted set of
 * real lights. The district's profile is copied onto the actor, so preset
 * switching needs no lookup and works in packaged builds.
 */
UCLASS()
class VOIDWORLDBUILDERLIGHTING_API AVoidDistrictLightingActor : public AVoidLightingActorBase
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Lighting")
	FName DistrictId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Lighting")
	FVoidDistrictLightingProfile Profile;

	/** -1 = no state override. Otherwise index into Profile.States. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VOID Lighting")
	int32 ActiveStateIndex = -1;

	/** Placeholder signs a later art pass can replace. World-space transforms. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Lighting")
	TArray<FVoidNeonHook> NeonHooks;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Lighting")
	FBox DistrictBounds = FBox(ForceInit);

	/** Last preset applied (for display / debugging). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Lighting")
	FName AppliedPresetId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Lighting")
	FString GenerationSummary;

	/** Applies a preset. EmergencyMul scales beacon emissive (director's emergency mode). */
	UFUNCTION(BlueprintCallable, Category = "VOID Lighting")
	void ApplyPreset(const FVoidLightingPreset& Preset, float EmergencyMul = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "VOID Lighting")
	void SetStateIndex(int32 NewStateIndex);

	/** Recolours palette-driven elements from Profile + the active state. Called by ApplyPreset. */
	void RefreshPalettes();

private:
	FVoidLightingPreset LastPreset;
	float LastEmergencyMul = 1.0f;
	bool bHasLastPreset = false;
};
