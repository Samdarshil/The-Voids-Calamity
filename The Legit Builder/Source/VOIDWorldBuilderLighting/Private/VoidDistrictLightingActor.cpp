// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VoidDistrictLightingActor.h"
#include "VoidLightingLog.h"

namespace VoidDistrictLightingPrivate
{
	static FLinearColor Kelvin(float K)
	{
		return FLinearColor::MakeFromColorTemperature(FMath::Clamp(K, 1500.0f, 12000.0f));
	}
}

void AVoidDistrictLightingActor::RefreshPalettes()
{
	using namespace VoidDistrictLightingPrivate;

	float Shift = 0.0f;
	if (Profile.States.IsValidIndex(ActiveStateIndex))
	{
		Shift = Profile.States[ActiveStateIndex].TemperatureShiftK;
	}

	SetElementPalette(EVoidLightingElement::StreetLamp, Kelvin(Profile.StreetLampTempA + Shift), Kelvin(Profile.StreetLampTempB + Shift));
	SetElementPalette(EVoidLightingElement::WindowResidential, Kelvin(Profile.WindowResidentialTempA + Shift), Kelvin(Profile.WindowResidentialTempB + Shift));
	SetElementPalette(EVoidLightingElement::WindowCommercial, Kelvin(Profile.WindowCommercialTempA + Shift), Kelvin(Profile.WindowCommercialTempB + Shift));
	SetElementPalette(EVoidLightingElement::WindowIndustrial, Kelvin(Profile.WindowCommercialTempA + Shift), Kelvin(Profile.WindowCommercialTempB + Shift));
	SetElementPalette(EVoidLightingElement::WindowCivic, Kelvin(Profile.WindowCivicTempA + Shift), Kelvin(Profile.WindowCivicTempB + Shift));
	SetElementPalette(EVoidLightingElement::WindowLandmark, Kelvin(Profile.WindowCivicTempA + Shift), Kelvin(Profile.WindowCivicTempB + Shift));
	SetElementPalette(EVoidLightingElement::NeonSign, Profile.NeonColorA, Profile.NeonColorB);
	// Traffic signals, pedestrian signals and beacons keep the fixed palettes set at generation.
}

void AVoidDistrictLightingActor::SetStateIndex(int32 NewStateIndex)
{
	ActiveStateIndex = Profile.States.IsValidIndex(NewStateIndex) ? NewStateIndex : -1;
	if (bHasLastPreset)
	{
		ApplyPreset(LastPreset, LastEmergencyMul);
	}
	else
	{
		RefreshPalettes();
	}
}

void AVoidDistrictLightingActor::ApplyPreset(const FVoidLightingPreset& Preset, float EmergencyMul)
{
	LastPreset = Preset;
	LastEmergencyMul = EmergencyMul;
	bHasLastPreset = true;
	AppliedPresetId = Preset.Id;

	RefreshPalettes();

	float StateIntensity = 1.0f;
	float StateLit = 1.0f;
	if (Profile.States.IsValidIndex(ActiveStateIndex))
	{
		StateIntensity = Profile.States[ActiveStateIndex].IntensityMul;
		StateLit = Profile.States[ActiveStateIndex].LitFractionMul;
	}

	const FVoidLocalLightingSettings& L = Preset.Local;

	// Below-grade districts have no sun or sky, so their lights don't follow the time-of-day preset.
	const float StreetEmissive = Profile.bBelowGrade ? Profile.BelowGradeStreetEmissive : L.StreetLampEmissive;
	const float StreetLit = Profile.bBelowGrade ? Profile.BelowGradeStreetLitFraction : L.StreetLampOnFraction;
	const float WindowEmissive = Profile.bBelowGrade ? Profile.BelowGradeWindowEmissive : L.WindowEmissive;
	const float WindowLit = (Profile.bBelowGrade ? Profile.BelowGradeWindowLitFraction : L.WindowLitFraction) * Profile.WindowLitMul;
	const float RealLightScale = Profile.bBelowGrade ? 1.0f : L.StreetLightMul;

	SetElementState(EVoidLightingElement::StreetLamp, StreetEmissive * Profile.StreetLampIntensityMul * StateIntensity, StreetLit * StateLit);

	const float WindowIntensity = WindowEmissive * Profile.WindowIntensityMul * StateIntensity;
	const float WindowLitFinal = WindowLit * StateLit;
	SetElementState(EVoidLightingElement::WindowResidential, WindowIntensity, WindowLitFinal);
	SetElementState(EVoidLightingElement::WindowCommercial, WindowIntensity, WindowLitFinal);
	SetElementState(EVoidLightingElement::WindowCivic, WindowIntensity, WindowLitFinal);
	SetElementState(EVoidLightingElement::WindowIndustrial, WindowIntensity, WindowLitFinal);
	SetElementState(EVoidLightingElement::WindowLandmark, WindowIntensity, WindowLitFinal);

	SetElementState(EVoidLightingElement::NeonSign, L.NeonEmissive * Profile.NeonIntensityMul * StateIntensity, L.NeonOnFraction * StateLit);

	SetElementState(EVoidLightingElement::TrafficRed, L.TrafficEmissive, 1.0f);
	SetElementState(EVoidLightingElement::TrafficAmber, L.TrafficEmissive, 1.0f);
	SetElementState(EVoidLightingElement::TrafficGreen, L.TrafficEmissive, 1.0f);
	SetElementState(EVoidLightingElement::PedestrianSignal, L.TrafficEmissive, 1.0f);
	SetElementState(EVoidLightingElement::EmergencyBeacon, L.EmergencyEmissive * FMath::Max(0.0f, EmergencyMul), 1.0f);

	const float LightScale = RealLightScale * Profile.StreetLampIntensityMul * StateIntensity;
	SetRoleScale(EVoidRealLightRole::Street, LightScale);
	SetRoleScale(EVoidRealLightRole::Intersection, LightScale);

	UE_LOG(LogVoidLighting, VeryVerbose, TEXT("%s applied preset '%s' (state %d)."), *GetName(), *Preset.Id.ToString(), ActiveStateIndex);
}
