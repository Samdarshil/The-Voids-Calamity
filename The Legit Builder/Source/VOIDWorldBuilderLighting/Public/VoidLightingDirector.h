// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoidLightingTypes.h"
#include "VoidLightingSettings.h"
#include "VoidLightingDirector.generated.h"

class ADirectionalLight;
class ASkyLight;
class ASkyAtmosphere;
class AExponentialHeightFog;
class APostProcessVolume;
class AVoidDistrictLightingActor;
class AVoidLandmarkLightingActor;

/** Values captured from an adopted, pre-existing environment actor so the original look can be restored. */
USTRUCT()
struct FVoidEnvironmentSnapshot
{
	GENERATED_BODY()

	UPROPERTY() bool bValid = false;
	UPROPERTY() float SunIntensity = 10.0f;
	UPROPERTY() float SunTemperature = 6500.0f;
	UPROPERTY() bool SunUseTemperature = false;
	UPROPERTY() FLinearColor SunColor = FLinearColor::White;
	UPROPERTY() FRotator SunRotation = FRotator::ZeroRotator;
	UPROPERTY() float SunSourceAngle = 0.5357f;
	UPROPERTY() bool SunVisible = true;
	UPROPERTY() float SkyIntensity = 1.0f;
	UPROPERTY() FLinearColor SkyColor = FLinearColor::White;
	UPROPERTY() float FogDensity = 0.02f;
	UPROPERTY() float FogHeightFalloff = 0.2f;
	UPROPERTY() FLinearColor FogInscattering = FLinearColor::White;
	UPROPERTY() bool FogVolumetric = false;
};

/**
 * AVoidLightingDirector
 *
 * The one object the first-look team talks to. Exactly one per world
 * (the generator finds-or-spawns it). Responsibilities:
 *   - holds a copy of the preset library (so a saved level works with the
 *     JSON files absent) and switches presets;
 *   - drives Sun / Moon / SkyLight / SkyAtmosphere / HeightFog / a
 *     low-priority unbound PostProcessVolume, adopting existing actors per
 *     UVoidLightingSettings::ExistingLightingPolicy;
 *   - fans the preset out to every district + landmark lighting actor;
 *   - exposes cinematic anchors and world bounds (data only).
 *
 * No Tick. A preset switch is: a few component setter calls, one
 * SkyLight recapture, and one small loop over the lighting actors.
 */
UCLASS()
class VOIDWORLDBUILDERLIGHTING_API AVoidLightingDirector : public AActor
{
	GENERATED_BODY()

public:
	AVoidLightingDirector();

	// --- Presets --------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VOID Lighting|Presets")
	TArray<FVoidLightingPreset> PresetLibrary;

	/** Choose a preset and press "Apply Preview Preset" in the Details panel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Lighting|Presets", meta = (GetOptions = "GetPresetOptions"))
	FName PreviewPresetId = TEXT("Day");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Lighting|Presets")
	FName ActivePresetId;

	UFUNCTION()
	TArray<FName> GetPresetOptions() const;

	UFUNCTION(BlueprintCallable, Category = "VOID Lighting")
	TArray<FName> GetPresetIds() const;

	/** @return false if no preset with that id exists (nothing is changed). */
	UFUNCTION(BlueprintCallable, Category = "VOID Lighting")
	bool ApplyPresetById(FName PresetId);

	UFUNCTION(CallInEditor, Category = "VOID Lighting|Presets", meta = (DisplayName = "Apply Preview Preset"))
	void ApplyPreviewPreset();

	UFUNCTION(BlueprintCallable, Category = "VOID Lighting")
	void ApplyPreset(const FVoidLightingPreset& Preset);

	// --- Modes ----------------------------------------------------------

	/** Lights the emergency beacons (tunnel portals). Off by default. */
	UFUNCTION(BlueprintCallable, Category = "VOID Lighting")
	void SetEmergencyMode(bool bEnabled);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Lighting|Modes")
	bool bEmergencyMode = false;

	/** Scales every landmark's lights on top of per-landmark controls. */
	UFUNCTION(BlueprintCallable, Category = "VOID Lighting")
	void SetGlobalLandmarkEmphasis(float Emphasis);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VOID Lighting|Modes")
	float GlobalLandmarkEmphasis = 1.0f;

	// --- Environment ------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VOID Lighting|Environment")
	EVoidExistingLightingPolicy ExistingPolicy = EVoidExistingLightingPolicy::Adopt;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VOID Lighting|Environment")
	TObjectPtr<ADirectionalLight> Sun;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VOID Lighting|Environment")
	TObjectPtr<ADirectionalLight> Moon;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VOID Lighting|Environment")
	TObjectPtr<ASkyLight> SkyLight;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VOID Lighting|Environment")
	TObjectPtr<ASkyAtmosphere> SkyAtmosphere;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VOID Lighting|Environment")
	TObjectPtr<AExponentialHeightFog> HeightFog;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VOID Lighting|Environment")
	TObjectPtr<APostProcessVolume> PostVolume;

	/** Optional LUT for presets with bApplyGradingLUT. Copied from settings at generation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VOID Lighting|Environment")
	TSoftObjectPtr<UTexture> ColorGradingLUT;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Lighting|Environment")
	FVoidEnvironmentSnapshot OriginalSnapshot;

	/**
	 * Finds (Adopt policy: adopts) or spawns Sun, SkyLight, SkyAtmosphere, HeightFog, PostProcessVolume.
	 * Existing actors are only modified under Adopt, and a snapshot is stored first. Spawned actors are tagged VOID.Lighting.Generated.
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VOID Lighting|Environment")
	void EnsureEnvironmentActors(bool bSpawnPostProcess = true);

	/** Puts adopted sun/sky/fog back the way they were when first adopted. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VOID Lighting|Environment")
	void RestoreOriginalLighting();

	// --- Registration -------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Lighting|Registry")
	TArray<TObjectPtr<AVoidDistrictLightingActor>> DistrictActors;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Lighting|Registry")
	TArray<TObjectPtr<AVoidLandmarkLightingActor>> LandmarkActors;

	/** Re-scans the world for lighting actors. Call after generating/deleting any of them. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "VOID Lighting|Registry")
	void RefreshRegistrations();

	// --- Cinematic hooks ------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Lighting|Cinematic")
	TArray<FVoidCinematicAnchor> Anchors;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Lighting|Cinematic")
	FBox WorldBounds = FBox(ForceInit);

	UFUNCTION(BlueprintCallable, Category = "VOID Lighting|Cinematic")
	TArray<FVoidCinematicAnchor> GetAnchorsOfType(EVoidAnchorType Type) const;

	UFUNCTION(BlueprintCallable, Category = "VOID Lighting|Cinematic")
	bool FindAnchor(FName AnchorId, FVoidCinematicAnchor& OutAnchor) const;

	UFUNCTION(BlueprintCallable, Category = "VOID Lighting|Cinematic")
	bool FindLandmarkAnchor(FName LandmarkId, FVoidCinematicAnchor& OutAnchor) const;

	UFUNCTION(BlueprintCallable, Category = "VOID Lighting|Cinematic")
	FVector GetWorldCenter() const { return WorldBounds.IsValid ? WorldBounds.GetCenter() : FVector::ZeroVector; }

	UFUNCTION(BlueprintCallable, Category = "VOID Lighting|Cinematic")
	FVector GetWorldExtent() const { return WorldBounds.IsValid ? WorldBounds.GetExtent() : FVector::ZeroVector; }

	/** Removes every anchor belonging to the district (regeneration). */
	void RemoveAnchorsForDistrict(FName DistrictId);
	void AddAnchors(const TArray<FVoidCinematicAnchor>& NewAnchors);
	void RecomputeWorldBounds(const FBox& AdditionalBounds);

	/** One-line-per-item performance summary (instance / real-light counts). */
	UFUNCTION(BlueprintCallable, Category = "VOID Lighting")
	FString BuildStatsReport() const;

	static AVoidLightingDirector* FindDirector(UWorld* World);

private:
	void SnapshotEnvironment();
	void ApplySun(const FVoidLightingPreset& P);
	void ApplyMoon(const FVoidLightingPreset& P);
	void ApplySky(const FVoidLightingPreset& P);
	void ApplyFog(const FVoidLightingPreset& P);
	void ApplyPost(const FVoidLightingPreset& P);
};
