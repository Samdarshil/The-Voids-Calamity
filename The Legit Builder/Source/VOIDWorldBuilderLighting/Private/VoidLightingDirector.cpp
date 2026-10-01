// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VoidLightingDirector.h"
#include "VoidDistrictLightingActor.h"
#include "VoidLandmarkLightingActor.h"
#include "VoidLightingLog.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/SkyAtmosphere.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/Texture.h"

AVoidLightingDirector::AVoidLightingDirector()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Tags.AddUnique(VoidLightingParams::GeneratedActorTag);
}

AVoidLightingDirector* AVoidLightingDirector::FindDirector(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AVoidLightingDirector> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

// ---------------------------------------------------------------------
// Presets
// ---------------------------------------------------------------------

TArray<FName> AVoidLightingDirector::GetPresetIds() const
{
	TArray<FName> Ids;
	for (const FVoidLightingPreset& P : PresetLibrary)
	{
		Ids.Add(P.Id);
	}
	return Ids;
}

TArray<FName> AVoidLightingDirector::GetPresetOptions() const
{
	return GetPresetIds();
}

bool AVoidLightingDirector::ApplyPresetById(FName PresetId)
{
	for (const FVoidLightingPreset& P : PresetLibrary)
	{
		if (P.Id == PresetId)
		{
			ApplyPreset(P);
			return true;
		}
	}
	UE_LOG(LogVoidLighting, Warning, TEXT("ApplyPresetById: no preset '%s' in library (%d presets)."), *PresetId.ToString(), PresetLibrary.Num());
	return false;
}

void AVoidLightingDirector::ApplyPreviewPreset()
{
	ApplyPresetById(PreviewPresetId);
}

void AVoidLightingDirector::ApplyPreset(const FVoidLightingPreset& P)
{
	ActivePresetId = P.Id;

	ApplySun(P);
	ApplyMoon(P);
	ApplySky(P);
	ApplyFog(P);
	ApplyPost(P);

	const float EmergencyMul = bEmergencyMode ? 1.0f : 0.0f;
	for (AVoidDistrictLightingActor* D : DistrictActors)
	{
		if (D)
		{
			D->ApplyPreset(P, EmergencyMul);
		}
	}
	for (AVoidLandmarkLightingActor* L : LandmarkActors)
	{
		if (L)
		{
			L->ApplyPreset(P, GlobalLandmarkEmphasis);
		}
	}

	UE_LOG(LogVoidLighting, Log, TEXT("Applied lighting preset '%s' to %d districts, %d landmarks."), *P.Id.ToString(), DistrictActors.Num(), LandmarkActors.Num());
}

void AVoidLightingDirector::SetEmergencyMode(bool bEnabled)
{
	bEmergencyMode = bEnabled;
	for (const FVoidLightingPreset& P : PresetLibrary)
	{
		if (P.Id == ActivePresetId)
		{
			ApplyPreset(P);
			return;
		}
	}
}

void AVoidLightingDirector::SetGlobalLandmarkEmphasis(float Emphasis)
{
	GlobalLandmarkEmphasis = FMath::Max(0.0f, Emphasis);
	for (const FVoidLightingPreset& P : PresetLibrary)
	{
		if (P.Id == ActivePresetId)
		{
			for (AVoidLandmarkLightingActor* L : LandmarkActors)
			{
				if (L)
				{
					L->ApplyPreset(P, GlobalLandmarkEmphasis);
				}
			}
			return;
		}
	}
}

// ---------------------------------------------------------------------
// Environment
// ---------------------------------------------------------------------

void AVoidLightingDirector::SnapshotEnvironment()
{
	if (OriginalSnapshot.bValid)
	{
		return; // Only the very first adoption is the "original" look.
	}

	FVoidEnvironmentSnapshot& S = OriginalSnapshot;
	if (Sun && Sun->GetLightComponent())
	{
		ULightComponent* C = Sun->GetLightComponent();
		S.SunIntensity = C->Intensity;
		S.SunTemperature = C->Temperature;
		S.SunUseTemperature = C->bUseTemperature;
		S.SunColor = C->GetLightColor();
		S.SunRotation = Sun->GetActorRotation();
		S.SunVisible = C->IsVisible();
		if (UDirectionalLightComponent* DC = Cast<UDirectionalLightComponent>(C))
		{
			S.SunSourceAngle = DC->LightSourceAngle;
		}
	}
	if (SkyLight && SkyLight->GetLightComponent())
	{
		S.SkyIntensity = SkyLight->GetLightComponent()->Intensity;
		S.SkyColor = SkyLight->GetLightComponent()->GetLightColor();
	}
	if (HeightFog && HeightFog->GetComponent())
	{
		const UExponentialHeightFogComponent* F = HeightFog->GetComponent();
		S.FogDensity = F->FogDensity;
		S.FogHeightFalloff = F->FogHeightFalloff;
		S.FogInscattering = F->FogInscatteringColor;
		S.FogVolumetric = F->bEnableVolumetricFog;
	}
	S.bValid = true;
}

void AVoidLightingDirector::EnsureEnvironmentActors(bool bSpawnPostProcess)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const bool bAdopt = (ExistingPolicy == EVoidExistingLightingPolicy::Adopt);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	auto Tag = [](AActor* A, const TCHAR* Label)
	{
		A->Tags.AddUnique(VoidLightingParams::GeneratedActorTag);
#if WITH_EDITOR
		A->SetActorLabel(Label);
#endif
	};

	// Sun: first directional light that is NOT ours-as-moon and not tagged as moon.
	if (!Sun)
	{
		for (TActorIterator<ADirectionalLight> It(World); It; ++It)
		{
			if (*It != Moon && !It->ActorHasTag(TEXT("VOID.Lighting.Moon")))
			{
				Sun = *It;
				UE_LOG(LogVoidLighting, Log, TEXT("Found existing directional light '%s' (%s)."), *Sun->GetName(), bAdopt ? TEXT("adopting") : TEXT("leaving untouched"));
				break;
			}
		}
	}
	if (!Sun)
	{
		Sun = World->SpawnActor<ADirectionalLight>(FVector(0, 0, 1000), FRotator(-55, 135, 0), Params);
		if (Sun) { Tag(Sun, TEXT("VOID_Sun")); }
	}

	if (!SkyLight)
	{
		for (TActorIterator<ASkyLight> It(World); It; ++It) { SkyLight = *It; break; }
	}
	if (!SkyLight)
	{
		SkyLight = World->SpawnActor<ASkyLight>(FVector(0, 0, 1000), FRotator::ZeroRotator, Params);
		if (SkyLight)
		{
			Tag(SkyLight, TEXT("VOID_SkyLight"));
			SkyLight->GetLightComponent()->SetMobility(EComponentMobility::Movable);
			if (USkyLightComponent* SLC = Cast<USkyLightComponent>(SkyLight->GetLightComponent()))
			{
				SLC->SetRealTimeCaptureEnabled(true);
			}
		}
	}

	if (!SkyAtmosphere)
	{
		for (TActorIterator<ASkyAtmosphere> It(World); It; ++It) { SkyAtmosphere = *It; break; }
	}
	if (!SkyAtmosphere)
	{
		SkyAtmosphere = World->SpawnActor<ASkyAtmosphere>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (SkyAtmosphere) { Tag(SkyAtmosphere, TEXT("VOID_SkyAtmosphere")); }
	}

	if (!HeightFog)
	{
		for (TActorIterator<AExponentialHeightFog> It(World); It; ++It) { HeightFog = *It; break; }
	}
	if (!HeightFog)
	{
		HeightFog = World->SpawnActor<AExponentialHeightFog>(FVector(0, 0, 0), FRotator::ZeroRotator, Params);
		if (HeightFog) { Tag(HeightFog, TEXT("VOID_HeightFog")); }
	}

	// Post-process: never adopt. Always our own, low priority, so existing/hand-made volumes and camera post-process win.
	if (bSpawnPostProcess && !PostVolume)
	{
		for (TActorIterator<APostProcessVolume> It(World); It; ++It)
		{
			if (It->ActorHasTag(VoidLightingParams::GeneratedActorTag)) { PostVolume = *It; break; }
		}
		if (!PostVolume)
		{
			PostVolume = World->SpawnActor<APostProcessVolume>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
			if (PostVolume)
			{
				Tag(PostVolume, TEXT("VOID_PostProcess"));
				PostVolume->bUnbound = true;
				PostVolume->Priority = GetDefault<UVoidLightingSettings>()->PostProcessPriority;
			}
		}
	}

	if (!Moon)
	{
		for (TActorIterator<ADirectionalLight> It(World); It; ++It)
		{
			if (It->ActorHasTag(TEXT("VOID.Lighting.Moon"))) { Moon = *It; break; }
		}
	}

	if (bAdopt)
	{
		SnapshotEnvironment();
	}
}

void AVoidLightingDirector::RestoreOriginalLighting()
{
	if (!OriginalSnapshot.bValid)
	{
		UE_LOG(LogVoidLighting, Warning, TEXT("RestoreOriginalLighting: nothing was adopted, so there is no snapshot."));
		return;
	}
	const FVoidEnvironmentSnapshot& S = OriginalSnapshot;

	if (Sun && Sun->GetLightComponent())
	{
		ULightComponent* C = Sun->GetLightComponent();
		C->SetIntensity(S.SunIntensity);
		C->SetTemperature(S.SunTemperature);
		C->SetUseTemperature(S.SunUseTemperature);
		C->SetLightColor(S.SunColor);
		C->SetVisibility(S.SunVisible);
		Sun->SetActorRotation(S.SunRotation);
		if (UDirectionalLightComponent* DC = Cast<UDirectionalLightComponent>(C))
		{
			DC->SetLightSourceAngle(S.SunSourceAngle);
		}
	}
	if (SkyLight && SkyLight->GetLightComponent())
	{
		SkyLight->GetLightComponent()->SetIntensity(S.SkyIntensity);
		SkyLight->GetLightComponent()->SetLightColor(S.SkyColor);
		if (USkyLightComponent* SLC = Cast<USkyLightComponent>(SkyLight->GetLightComponent())) { SLC->RecaptureSky(); }
	}
	if (HeightFog && HeightFog->GetComponent())
	{
		UExponentialHeightFogComponent* F = HeightFog->GetComponent();
		F->SetFogDensity(S.FogDensity);
		F->SetFogHeightFalloff(S.FogHeightFalloff);
		F->SetFogInscatteringColor(S.FogInscattering);
		F->SetVolumetricFog(S.FogVolumetric);
	}
	if (Moon)
	{
		Moon->GetLightComponent()->SetVisibility(false);
	}
	ActivePresetId = NAME_None;
}

void AVoidLightingDirector::ApplySun(const FVoidLightingPreset& P)
{
	if (!Sun || (ExistingPolicy == EVoidExistingLightingPolicy::LeaveExisting && !Sun->ActorHasTag(VoidLightingParams::GeneratedActorTag)))
	{
		return;
	}
	ULightComponent* C = Sun->GetLightComponent();
	const FVoidSunSettings& S = P.Sun;
	const bool bOn = S.IntensityLux > KINDA_SMALL_NUMBER;

	C->SetVisibility(bOn);
	C->SetUseTemperature(true);
	C->SetTemperature(S.TemperatureK);
	C->SetIntensity(S.IntensityLux);
	Sun->SetActorRotation(FRotator(S.Pitch, S.Yaw, 0.0f));
	if (UDirectionalLightComponent* DC = Cast<UDirectionalLightComponent>(C))
	{
		DC->SetLightSourceAngle(S.SourceAngleDeg);
		DC->SetVolumetricScatteringIntensity(S.VolumetricScattering);
	}
}

void AVoidLightingDirector::ApplyMoon(const FVoidLightingPreset& P)
{
	const FVoidMoonSettings& M = P.Moon;

	if (!Moon)
	{
		if (!M.bEnabled || !GetWorld())
		{
			return;
		}
		// Spawn lazily: presets without a moon never pay for a second directional light.
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Moon = GetWorld()->SpawnActor<ADirectionalLight>(FVector(0, 0, 1000), FRotator(M.Pitch, M.Yaw, 0), Params);
		if (!Moon)
		{
			return;
		}
		Moon->Tags.AddUnique(VoidLightingParams::GeneratedActorTag);
		Moon->Tags.AddUnique(TEXT("VOID.Lighting.Moon"));
#if WITH_EDITOR
		Moon->SetActorLabel(TEXT("VOID_Moon"));
#endif
		if (UDirectionalLightComponent* DC = Cast<UDirectionalLightComponent>(Moon->GetLightComponent()))
		{
			DC->SetAtmosphereSunLightIndex(1);
			DC->SetAtmosphereSunLight(true);
			DC->SetCastShadows(true);
		}
	}

	ULightComponent* C = Moon->GetLightComponent();
	C->SetVisibility(M.bEnabled);
	if (M.bEnabled)
	{
		C->SetUseTemperature(true);
		C->SetTemperature(M.TemperatureK);
		C->SetIntensity(M.IntensityLux);
		Moon->SetActorRotation(FRotator(M.Pitch, M.Yaw, 0.0f));
	}
}

void AVoidLightingDirector::ApplySky(const FVoidLightingPreset& P)
{
	const FVoidSkySettings& S = P.Sky;
	const bool bMayTouchSky = SkyLight && (ExistingPolicy == EVoidExistingLightingPolicy::Adopt || SkyLight->ActorHasTag(VoidLightingParams::GeneratedActorTag));
	if (bMayTouchSky)
	{
		if (USkyLightComponent* SLC = Cast<USkyLightComponent>(SkyLight->GetLightComponent()))
		{
			SLC->SetIntensity(S.SkyLightIntensity);
			SLC->SetLightColor(S.SkyLightColor);
			SLC->SetLowerHemisphereColor(S.LowerHemisphereColor);
			SLC->SetVolumetricScatteringIntensity(S.SkyLightVolumetricScattering);
			// A captured sky light does not know the sun moved. Real-time-capture sky lights refresh themselves; this covers the others.
			SLC->RecaptureSky();
		}
	}

	const bool bMayTouchAtmosphere = SkyAtmosphere && (ExistingPolicy == EVoidExistingLightingPolicy::Adopt || SkyAtmosphere->ActorHasTag(VoidLightingParams::GeneratedActorTag));
	if (bMayTouchAtmosphere)
	{
		if (USkyAtmosphereComponent* AC = SkyAtmosphere->GetComponent())
		{
			AC->SetSkyLuminanceFactor(S.AtmosphereLuminanceFactor);
			AC->SetMieScatteringScale(S.MieScatteringScale);
			AC->SetHeightFogContribution(S.HeightFogContribution);
		}
	}
}

void AVoidLightingDirector::ApplyFog(const FVoidLightingPreset& P)
{
	if (!HeightFog || (ExistingPolicy == EVoidExistingLightingPolicy::LeaveExisting && !HeightFog->ActorHasTag(VoidLightingParams::GeneratedActorTag)))
	{
		return;
	}
	UExponentialHeightFogComponent* F = HeightFog->GetComponent();
	if (!F)
	{
		return;
	}
	const FVoidFogSettings& S = P.Fog;
	F->SetFogDensity(S.Density);
	F->SetFogHeightFalloff(S.HeightFalloff);
	F->SetStartDistance(S.StartDistance);
	F->SetFogMaxOpacity(S.MaxOpacity);
	F->SetFogInscatteringColor(S.InscatteringColor);
	F->SetVolumetricFog(S.bVolumetric);
	if (S.bVolumetric)
	{
		F->SetVolumetricFogScatteringDistribution(S.ScatteringDistribution);
		F->SetVolumetricFogExtinctionScale(S.ExtinctionScale);
		F->SetVolumetricFogAlbedo(S.Albedo.ToFColor(true));
		F->SetVolumetricFogDistance(S.ViewDistance);
	}
}

void AVoidLightingDirector::ApplyPost(const FVoidLightingPreset& P)
{
	if (!PostVolume)
	{
		return;
	}
	const FVoidPostSettings& S = P.Post;
	FPostProcessSettings& PP = PostVolume->Settings;

	PP.bOverride_AutoExposureBias = true;
	PP.AutoExposureBias = S.ExposureBias;

	// Exposure range is opt-in: its unit (EV100 vs cd/m2) depends on the project's extended-luminance setting.
	PP.bOverride_AutoExposureMinBrightness = S.bOverrideExposureRange;
	PP.bOverride_AutoExposureMaxBrightness = S.bOverrideExposureRange;
	if (S.bOverrideExposureRange)
	{
		PP.AutoExposureMinBrightness = S.ExposureMinEV;
		PP.AutoExposureMaxBrightness = S.ExposureMaxEV;
	}

	PP.bOverride_BloomIntensity = true;
	PP.BloomIntensity = S.BloomIntensity;
	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = S.VignetteIntensity;

	PP.bOverride_ColorSaturation = true;
	PP.ColorSaturation = FVector4(S.Saturation, S.Saturation, S.Saturation, 1.0f);
	PP.bOverride_ColorContrast = true;
	PP.ColorContrast = FVector4(S.Contrast, S.Contrast, S.Contrast, 1.0f);
	PP.bOverride_ColorGain = true;
	PP.ColorGain = FVector4(S.GainTint.R, S.GainTint.G, S.GainTint.B, 1.0f);

	UTexture* Lut = S.bApplyGradingLUT ? ColorGradingLUT.LoadSynchronous() : nullptr;
	PP.bOverride_ColorGradingLUT = (Lut != nullptr);
	PP.bOverride_ColorGradingIntensity = (Lut != nullptr);
	PP.ColorGradingLUT = Lut;
	PP.ColorGradingIntensity = S.LutIntensity;

	PP.bOverride_DynamicGlobalIlluminationMethod = S.bForceLumen;
	PP.bOverride_ReflectionMethod = S.bForceLumen;
	if (S.bForceLumen)
	{
		PP.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Lumen;
		PP.ReflectionMethod = EReflectionMethod::Lumen;
	}

	// Depth of field is intentionally left un-overridden here, so a cinema camera's own DoF (and Sequencer tracks) are never fought by this volume.
	PostVolume->MarkComponentsRenderStateDirty();
}

// ---------------------------------------------------------------------
// Registration + hooks
// ---------------------------------------------------------------------

void AVoidLightingDirector::RefreshRegistrations()
{
	DistrictActors.Reset();
	LandmarkActors.Reset();
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AVoidDistrictLightingActor> It(World); It; ++It) { DistrictActors.Add(*It); }
		for (TActorIterator<AVoidLandmarkLightingActor> It(World); It; ++It) { LandmarkActors.Add(*It); }
	}
}

TArray<FVoidCinematicAnchor> AVoidLightingDirector::GetAnchorsOfType(EVoidAnchorType Type) const
{
	return Anchors.FilterByPredicate([Type](const FVoidCinematicAnchor& A) { return A.Type == Type; });
}

bool AVoidLightingDirector::FindAnchor(FName AnchorId, FVoidCinematicAnchor& OutAnchor) const
{
	if (const FVoidCinematicAnchor* Found = Anchors.FindByPredicate([AnchorId](const FVoidCinematicAnchor& A) { return A.Id == AnchorId; }))
	{
		OutAnchor = *Found;
		return true;
	}
	return false;
}

bool AVoidLightingDirector::FindLandmarkAnchor(FName LandmarkId, FVoidCinematicAnchor& OutAnchor) const
{
	if (const FVoidCinematicAnchor* Found = Anchors.FindByPredicate([LandmarkId](const FVoidCinematicAnchor& A) { return A.Type == EVoidAnchorType::Landmark && A.LandmarkId == LandmarkId; }))
	{
		OutAnchor = *Found;
		return true;
	}
	return false;
}

void AVoidLightingDirector::RemoveAnchorsForDistrict(FName DistrictId)
{
	Anchors.RemoveAll([DistrictId](const FVoidCinematicAnchor& A) { return A.DistrictId == DistrictId; });
}

void AVoidLightingDirector::AddAnchors(const TArray<FVoidCinematicAnchor>& NewAnchors)
{
	Anchors.Append(NewAnchors);
}

void AVoidLightingDirector::RecomputeWorldBounds(const FBox& AdditionalBounds)
{
	if (AdditionalBounds.IsValid)
	{
		WorldBounds += AdditionalBounds;
	}
}

FString AVoidLightingDirector::BuildStatsReport() const
{
	int32 Instances = 0;
	int32 RealLights = 0;
	for (const AVoidDistrictLightingActor* D : DistrictActors)
	{
		if (D) { Instances += D->GetNumInstances(); RealLights += D->GetNumRealLights(); }
	}
	int32 LandmarkLights = 0;
	for (const AVoidLandmarkLightingActor* L : LandmarkActors)
	{
		if (L) { Instances += L->GetNumInstances(); LandmarkLights += L->GetNumRealLights(); }
	}
	return FString::Printf(TEXT("Preset=%s Districts=%d Landmarks=%d EmissiveInstances=%d RealStreetLights=%d RealLandmarkLights=%d Anchors=%d"),
		*ActivePresetId.ToString(), DistrictActors.Num(), LandmarkActors.Num(), Instances, RealLights, LandmarkLights, Anchors.Num());
}
