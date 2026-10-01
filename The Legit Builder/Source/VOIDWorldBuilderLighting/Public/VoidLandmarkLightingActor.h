// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "VoidLightingActorBase.h"
#include "VoidLandmarkLightingActor.generated.h"

/**
 * AVoidLandmarkLightingActor
 *
 * Presentation lighting for ONE building that the Meridian
 * LandmarkRegistry (via a binding or an id/type match) identifies as a
 * landmark. It never creates a landmark: LandmarkId always comes from the
 * registry. Contents (all bounded, count fixed by tier):
 *   - facade uplights          (spot lights at the footprint, aimed up the facade)
 *   - one back-light / halo    (behind the landmark relative to the world centre;
 *                               gives silhouette separation and, with volumetric
 *                               fog on, an atmospheric glow)
 *   - one emissive top beacon  (instanced sphere, no light)
 */
UCLASS()
class VOIDWORLDBUILDERLIGHTING_API AVoidLandmarkLightingActor : public AVoidLightingActorBase
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Landmark")
	FName LandmarkId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Landmark")
	FName BuildingId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Landmark")
	FName DistrictId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Landmark")
	int32 Tier = 4;

	/** Tier rule captured at generation (LightingProfiles.json landmarkTiers). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Landmark")
	FVoidLandmarkTierRule TierRule;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Landmark")
	FVector Centroid = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Landmark")
	float Radius = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Landmark")
	float Height = 0.0f;

	/** Presentation dials. Edit in the Details panel or key them in Sequencer, then call Refresh (or re-apply the preset). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Landmark")
	FVoidLandmarkLightingControls Controls;

	UFUNCTION(BlueprintCallable, Category = "VOID Landmark")
	void ApplyPreset(const FVoidLightingPreset& Preset, float GlobalEmphasis = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "VOID Landmark")
	void SetControls(const FVoidLandmarkLightingControls& NewControls);

	UFUNCTION(BlueprintCallable, Category = "VOID Landmark")
	void Refresh();

	/** Builds the lights/beacon from the stored geometry. Called by the generator after setting the fields above. */
	void BuildLighting(const TArray<FVector>& FootprintCorners, const FVector& RimDirection, bool bCastShadows);

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	FVoidLightingPreset LastPreset;
	float LastGlobalEmphasis = 1.0f;
	bool bHasLastPreset = false;
};
