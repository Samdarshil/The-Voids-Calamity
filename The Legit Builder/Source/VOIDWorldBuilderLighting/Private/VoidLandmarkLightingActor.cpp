// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VoidLandmarkLightingActor.h"
#include "VoidLightingLog.h"

namespace VoidLandmarkLightingPrivate
{
	// Base values at multiplier 1 (lumens). Scaled by building height so a 40 m block and a 300 m spire both read.
	static float UplightLumens(float Height)
	{
		return 30000.0f * FMath::Clamp(Height / 4000.0f, 0.5f, 4.0f);
	}

	static float HaloLumens(float Height)
	{
		return 60000.0f * FMath::Clamp(Height / 4000.0f, 0.5f, 4.0f);
	}
}

void AVoidLandmarkLightingActor::BuildLighting(const TArray<FVector>& FootprintCorners, const FVector& RimDirection, bool bCastShadows)
{
	using namespace VoidLandmarkLightingPrivate;

	const int32 NumCorners = FootprintCorners.Num();
	const int32 NumUplights = FMath::Clamp(TierRule.UplightCount, 0, FMath::Max(NumCorners, 1));
	const float H = FMath::Max(Height, 500.0f);
	const float UpRadius = FMath::Clamp(H * 0.9f, 1500.0f, 12000.0f);

	// Uplights: spread over the footprint corners (evenly strided), pushed 300 units outside so they sit in front of the facade,
	// aimed at 60% of the way up the building. Shadowless: with shadows off the light passes through geometry, so they must sit outside.
	for (int32 i = 0; i < NumUplights; ++i)
	{
		const int32 CornerIndex = (NumCorners > 0) ? (i * NumCorners) / NumUplights : 0;
		const FVector Corner = (NumCorners > 0) ? FootprintCorners[CornerIndex] : Centroid;
		FVector Out = (Corner - Centroid);
		Out.Z = 0.0f;
		Out = Out.GetSafeNormal();
		const FVector Location = Corner + Out * 300.0f + FVector(0.0f, 0.0f, 50.0f);
		const FVector Target = FVector(Centroid.X, Centroid.Y, Centroid.Z + H * 0.6f);
		const FRotator Aim = (Target - Location).Rotation();
		AddSpotLight(EVoidRealLightRole::LandmarkUplight, Location, Aim, UplightLumens(H), UpRadius, 55.0f, 3200.0f, bCastShadows, 0.0f);
	}

	// Back-light / halo: behind the landmark, seen from the city side. Point light, big radius, strong volumetric scatter.
	{
		const FVector Dir = RimDirection.IsNearlyZero() ? FVector(1.0f, 0.0f, 0.0f) : RimDirection.GetSafeNormal();
		const FVector Location = Centroid + Dir * (Radius + 600.0f) + FVector(0.0f, 0.0f, H * 0.55f);
		AddPointLight(EVoidRealLightRole::LandmarkHalo, Location, HaloLumens(H), FMath::Clamp(H * 1.2f, 2500.0f, 20000.0f), 5200.0f, false, 4.0f);
	}

	// Top beacon (emissive only).
	if (TierRule.bBeacon)
	{
		const float BeaconSize = FMath::Clamp(H * 0.004f, 0.6f, 4.0f);
		AddElementInstance(EVoidLightingElement::LandmarkBeacon, FTransform(FRotator::ZeroRotator, FVector(Centroid.X, Centroid.Y, Centroid.Z + H + 80.0f), FVector(BeaconSize)), 0.0f, 0.0f);
		FinishElement(EVoidLightingElement::LandmarkBeacon);
		SetElementPalette(EVoidLightingElement::LandmarkBeacon, FLinearColor(1.0f, 0.55f, 0.15f), FLinearColor(1.0f, 0.8f, 0.5f));
	}
}

void AVoidLandmarkLightingActor::SetControls(const FVoidLandmarkLightingControls& NewControls)
{
	Controls = NewControls;
	Refresh();
}

void AVoidLandmarkLightingActor::Refresh()
{
	if (bHasLastPreset)
	{
		ApplyPreset(LastPreset, LastGlobalEmphasis);
	}
}

void AVoidLandmarkLightingActor::ApplyPreset(const FVoidLightingPreset& Preset, float GlobalEmphasis)
{
	LastPreset = Preset;
	LastGlobalEmphasis = GlobalEmphasis;
	bHasLastPreset = true;

	const FVoidLocalLightingSettings& L = Preset.Local;

	// Night illumination only matters in proportion to how night-like the preset is.
	const float NightFactor = FMath::Lerp(1.0f, Controls.NightIllumination, FMath::Clamp(L.NightWeight, 0.0f, 1.0f));

	const float Uplight = L.LandmarkLightMul * TierRule.LightMul * Controls.Emphasis * NightFactor * GlobalEmphasis;
	const float Halo = L.LandmarkHaloMul * TierRule.HaloMul * Controls.SilhouetteReadability * NightFactor * GlobalEmphasis;

	SetRoleScale(EVoidRealLightRole::LandmarkUplight, Uplight);
	SetRoleScale(EVoidRealLightRole::LandmarkHalo, Halo, Controls.AtmosphericEmphasis);

	const float Beacon = Controls.bBeaconEnabled ? L.LandmarkBeaconEmissive * NightFactor * GlobalEmphasis : 0.0f;
	SetElementState(EVoidLightingElement::LandmarkBeacon, Beacon, Controls.bBeaconEnabled ? 1.0f : 0.0f);
}

#if WITH_EDITOR
void AVoidLandmarkLightingActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	Refresh();
}
#endif
