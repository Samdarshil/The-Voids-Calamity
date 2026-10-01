// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoidLightingTypes.h"
#include "VoidLightingActorBase.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMesh;
class ULocalLightComponent;

/** A real (dynamic) light owned by a lighting actor, with the base values presets scale. */
USTRUCT()
struct FVoidLightRecord
{
	GENERATED_BODY()

	UPROPERTY() TObjectPtr<ULocalLightComponent> Light = nullptr;
	UPROPERTY() EVoidRealLightRole Role = EVoidRealLightRole::Street;
	/** Lumens at preset multiplier 1. */
	UPROPERTY() float BaseIntensity = 0.0f;
	UPROPERTY() float BaseVolumetric = 1.0f;
};

/**
 * AVoidLightingActorBase
 *
 * Shared machinery for the district and landmark lighting actors:
 *   - lazily created ISM components, one per EVoidLightingElement, all
 *     sharing a single emissive master material (per-instance custom data
 *     = on/off threshold + warm/cool blend);
 *   - a bounded list of real lights;
 *   - O(elements) preset switching (a handful of MID parameter writes plus
 *     one pass over the real-light list). No Tick, no per-instance work.
 *
 * The MIDs are Transient and rebuilt in PostRegisterAllComponents from the
 * persisted per-element state, so a reloaded level / PIE start looks
 * identical to the editor without any director being present.
 */
UCLASS(Abstract, NotPlaceable)
class VOIDWORLDBUILDERLIGHTING_API AVoidLightingActorBase : public AActor
{
	GENERATED_BODY()

public:
	AVoidLightingActorBase();

	/** Parent of the per-element MIDs. Set by the generator (M_VOID_LightingEmissive or the settings override). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Lighting")
	TObjectPtr<UMaterialInterface> EmissiveParent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Lighting")
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> ElementISMs;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Lighting")
	TArray<FVoidLightRecord> Lights;

	// --- Generation-time API (used by the generator) ------------------

	/** Returns the ISM for an element, creating and registering it on first use. Null only if the engine basic-shape mesh can't be loaded. */
	UInstancedStaticMeshComponent* EnsureElement(EVoidLightingElement Element);

	/** Adds one instance. Threshold in [0,1]: the instance is lit while LitFraction > Threshold. Blend in [0,1]: ColorA -> ColorB. */
	int32 AddElementInstance(EVoidLightingElement Element, const FTransform& Transform, float Threshold, float Blend);

	/** Call once after a batch of AddElementInstance calls. */
	void FinishElement(EVoidLightingElement Element);

	void SetElementPalette(EVoidLightingElement Element, const FLinearColor& ColorA, const FLinearColor& ColorB);
	/** Multipliers that make e.g. commercial windows brighter than residential ones under the same preset value. */
	void SetElementBias(EVoidLightingElement Element, float LitBias, float IntensityBias);

	ULocalLightComponent* AddSpotLight(EVoidRealLightRole Role, const FVector& Location, const FRotator& Rotation, float Lumens, float AttenuationRadius, float OuterConeDegrees, float TemperatureK, bool bCastShadows, float VolumetricScattering);
	ULocalLightComponent* AddPointLight(EVoidRealLightRole Role, const FVector& Location, float Lumens, float AttenuationRadius, float TemperatureK, bool bCastShadows, float VolumetricScattering);

	// --- Runtime API ----------------------------------------------------

	/** Writes intensity / lit-fraction for one element's material (no-op for non-emissive or unused elements). Values are persisted for reload. */
	void SetElementState(EVoidLightingElement Element, float Intensity, float LitFraction);

	/** Scales every real light with the given role. Lights whose resulting intensity is ~0 are hidden. */
	void SetRoleScale(EVoidRealLightRole Role, float Scale, float VolumetricScale = 1.0f);

	int32 GetNumInstances() const;
	int32 GetNumRealLights() const { return Lights.Num(); }
	int32 GetElementInstanceCount(EVoidLightingElement Element) const;

	/** Destroys every generated component (used before regenerating in place). */
	void ClearGeneratedComponents();

	static bool IsEmissiveElement(EVoidLightingElement Element);

	//~ Begin AActor
	virtual void PostRegisterAllComponents() override;
	//~ End AActor

protected:
	void EnsureArrays();
	UMaterialInstanceDynamic* EnsureMID(EVoidLightingElement Element);
	void PushMaterialState(EVoidLightingElement Element);

	/** Persisted per-element state (indexed by EVoidLightingElement). */
	UPROPERTY() TArray<FLinearColor> ElementColorA;
	UPROPERTY() TArray<FLinearColor> ElementColorB;
	UPROPERTY() TArray<float> ElementIntensity;
	UPROPERTY() TArray<float> ElementLitFraction;
	UPROPERTY() TArray<float> ElementLitBias;
	UPROPERTY() TArray<float> ElementIntensityBias;

	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> ElementMIDs;

	UPROPERTY() TObjectPtr<USceneComponent> RootScene;

private:
	bool bWarnedNoMaterial = false;
};
