// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Types/VoidWorldBuilderTypes.h"
#include "VoidLightingTypes.generated.h"

/**
 * Data model for the Agent 7 lighting system.
 *
 * Everything here is a plain USTRUCT so that (a) presets/profiles can be
 * authored as JSON in Config/VOIDLighting/ and loaded through
 * FJsonObjectConverter (JSON key == property name with a lower-case first
 * letter), and (b) the same structs can be copied onto the director actor
 * so a saved level keeps working with the JSON files absent (packaged
 * builds, other machines).
 *
 * Unit conventions: distances are Unreal units (cm). Directional-light
 * intensity is in the engine's lux convention (UE default sun = 10).
 * Local-light intensity is in lumens. Emissive intensity is a unitless
 * multiplier on the emissive colour of M_VOID_LightingEmissive.
 * None of the numeric defaults have been validated in a running editor --
 * they are starting points for the first-look lighting pass.
 */

UENUM(BlueprintType)
enum class EVoidLightingUsage : uint8
{
	Residential,
	Commercial,
	Civic,
	Industrial,
	Landmark,
	Unknown
};

UENUM(BlueprintType)
enum class EVoidAnchorType : uint8
{
	Landmark,
	DistrictCenter,
	MajorRoad,
	CameraInterest,
	SkylinePoint
};

/** What to do when the target level already contains sun/sky/fog/atmosphere actors. */
UENUM(BlueprintType)
enum class EVoidExistingLightingPolicy : uint8
{
	/** Drive the first existing actor of each type (a snapshot is stored so it can be restored). Post-process is never adopted -- we always add our own low-priority volume. */
	Adopt,
	/** Never touch existing sun/sky/fog/atmosphere. Presets then only drive generated street/window/landmark lighting and the post-process volume. */
	LeaveExisting
};

/** Instanced element kinds a lighting actor can own. One ISM component per kind, created lazily. */
UENUM(BlueprintType)
enum class EVoidLightingElement : uint8
{
	StreetPole,
	StreetLamp,
	TrafficHousing,
	TrafficRed,
	TrafficAmber,
	TrafficGreen,
	PedestrianSignal,
	WindowResidential,
	WindowCommercial,
	WindowCivic,
	WindowIndustrial,
	WindowLandmark,
	NeonSign,
	EmergencyBeacon,
	LandmarkBeacon,
	Count UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EVoidRealLightRole : uint8
{
	Street,
	Intersection,
	LandmarkUplight,
	LandmarkHalo
};

// ---------------------------------------------------------------------
// Presets
// ---------------------------------------------------------------------

USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERLIGHTING_API FVoidSunSettings
{
	GENERATED_BODY()

	/** Pitch of the light direction. Negative = light shines downward (sun above horizon). Positive = below horizon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun") float Pitch = -55.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun") float Yaw = 135.0f;
	/** 0 disables the sun component entirely (no shadow cost). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun") float IntensityLux = 10.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun") float TemperatureK = 6500.0f;
	/** Apparent sun diameter in degrees. Larger = softer shadows (overcast uses a large value). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun") float SourceAngleDeg = 0.5357f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun") float VolumetricScattering = 1.0f;
};

USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERLIGHTING_API FVoidMoonSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon") bool bEnabled = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon") float Pitch = -35.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon") float Yaw = 200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon") float IntensityLux = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Moon") float TemperatureK = 11000.0f;
};

USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERLIGHTING_API FVoidSkySettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky") float SkyLightIntensity = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky") FLinearColor SkyLightColor = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky") FLinearColor LowerHemisphereColor = FLinearColor(0.09f, 0.09f, 0.10f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky") float SkyLightVolumetricScattering = 1.0f;
	/** Multiplies the sky-atmosphere luminance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky") FLinearColor AtmosphereLuminanceFactor = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky") float MieScatteringScale = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky") float HeightFogContribution = 1.0f;
};

USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERLIGHTING_API FVoidFogSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog") float Density = 0.003f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog") float HeightFalloff = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog") float StartDistance = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog") float MaxOpacity = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog") FLinearColor InscatteringColor = FLinearColor(0.447f, 0.638f, 1.0f, 1.0f);

	/** Volumetric fog is the main per-frame cost here; day/overcast keep it off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog|Volumetric") bool bVolumetric = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog|Volumetric") float ScatteringDistribution = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog|Volumetric") float ExtinctionScale = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog|Volumetric") FLinearColor Albedo = FLinearColor(0.9f, 0.9f, 0.9f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog|Volumetric") float ViewDistance = 12000.0f;
};

USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERLIGHTING_API FVoidPostSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post") float ExposureBias = 0.0f;
	/** Off by default: the unit of the min/max range depends on the project's extended-luminance setting. Turn on per preset after checking the project. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post") bool bOverrideExposureRange = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post") float ExposureMinEV = 9.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post") float ExposureMaxEV = 13.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post") float BloomIntensity = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post") float VignetteIntensity = 0.15f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post") float Saturation = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post") float Contrast = 1.0f;
	/** Colour-grade hook: multiplicative tint on gain. Neutral = white. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post") FLinearColor GainTint = FLinearColor::White;
	/** If true and Settings->ColorGradingLUT is set, the LUT is applied at LutIntensity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post") bool bApplyGradingLUT = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post") float LutIntensity = 1.0f;
	/** Overrides GI + reflections to Lumen in the volume. Off by default so a project's own choice is respected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Post") bool bForceLumen = false;
};

USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERLIGHTING_API FVoidLocalLightingSettings
{
	GENERATED_BODY()

	/** Time-of-day weight used by the landmark night controls (Day 0 .. Night 1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Local") float NightWeight = 0.0f;

	/** Multiplier on the (few, budgeted) real street/intersection lights. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Local") float StreetLightMul = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Local") float StreetLampEmissive = 0.0f;
	/** Fraction of lamp heads switched on (per-instance threshold in the material). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Local") float StreetLampOnFraction = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Local") float WindowEmissive = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Local") float WindowLitFraction = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Local") float NeonEmissive = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Local") float NeonOnFraction = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Local") float TrafficEmissive = 6.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Local") float EmergencyEmissive = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Local") float LandmarkLightMul = 0.1f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Local") float LandmarkHaloMul = 0.05f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Local") float LandmarkBeaconEmissive = 4.0f;
};

USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERLIGHTING_API FVoidLightingPreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset") FName Id = TEXT("Day");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset") FString DisplayName = TEXT("Day");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset") FString Description;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset") FString IntendedUse;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset") FVoidSunSettings Sun;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset") FVoidMoonSettings Moon;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset") FVoidSkySettings Sky;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset") FVoidFogSettings Fog;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset") FVoidPostSettings Post;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preset") FVoidLocalLightingSettings Local;
};

USTRUCT()
struct VOIDWORLDBUILDERLIGHTING_API FVoidLightingPresetFile
{
	GENERATED_BODY()

	UPROPERTY() FString Schema;
	UPROPERTY() TArray<FVoidLightingPreset> Presets;
};

// ---------------------------------------------------------------------
// District / usage / road / landmark profiles (data-driven)
// ---------------------------------------------------------------------

/** One entry of a district's optional state list (used by Sector 0's story-driven arc; usable by any district). */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERLIGHTING_API FVoidDistrictLightingState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State") FName Name;
	/** True while the values are placeholders awaiting an approved design source. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State") bool bProvisional = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State") float TemperatureShiftK = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State") float IntensityMul = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State") float LitFractionMul = 1.0f;
};

USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERLIGHTING_API FVoidDistrictLightingProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "District") FName Id = TEXT("default");
	/** Package/registry district ids this profile applies to (exact FName match, case-insensitive). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "District") TArray<FName> MatchIds;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "District") FString Character;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "District") FString SourceNote;

	/** Below-grade districts have no sun/sky; their lights ignore the time-of-day preset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "District") bool bBelowGrade = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "District") float BelowGradeStreetEmissive = 25.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "District") float BelowGradeWindowEmissive = 8.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "District") float BelowGradeStreetLitFraction = 0.9f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "District") float BelowGradeWindowLitFraction = 0.6f;

	// Colour pairs (Kelvin). Each instance picks a random blend between A and B, so a wide pair = "mixed/uneven", a narrow pair = "uniform".
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour") float StreetLampTempA = 3000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour") float StreetLampTempB = 4500.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour") float WindowResidentialTempA = 2700.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour") float WindowResidentialTempB = 4000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour") float WindowCommercialTempA = 3800.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour") float WindowCommercialTempB = 6000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour") float WindowCivicTempA = 4500.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour") float WindowCivicTempB = 6000.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour") FLinearColor NeonColorA = FLinearColor(1.0f, 0.2f, 0.6f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour") FLinearColor NeonColorB = FLinearColor(0.2f, 0.8f, 1.0f, 1.0f);

	// Density / intensity multipliers.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Density") float StreetLampIntensityMul = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Density") float StreetLampSpacingMul = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Density") float WindowIntensityMul = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Density") float WindowLitMul = 1.0f;
	/** Fraction of window candidates generated at all (thins the facade at generation time). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Density") float WindowDensityMul = 1.0f;
	/** Neon sign hooks per 1500 units of commercial frontage. 0 = none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Density") float NeonDensity = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Density") float NeonIntensityMul = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State") TArray<FVoidDistrictLightingState> States;
};

USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERLIGHTING_API FVoidUsageRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Usage") EVoidLightingUsage Usage = EVoidLightingUsage::Unknown;
	/** Lower-case substrings matched against FVoidBuildingSpec::BuildingType. First matching rule (file order) wins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Usage") TArray<FString> Keywords;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Usage") float BaySpacingUnits = 400.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Usage") float FloorHeightUnits = 400.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Usage") float LitMul = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Usage") float IntensityMul = 1.0f;
};

USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERLIGHTING_API FVoidRoadLightingRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road") EVoidRoadType RoadType = EVoidRoadType::Local;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road") bool bEnabled = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road") float SpacingUnits = 2400.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road") float PoleHeightUnits = 650.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road") float ArmLengthUnits = 250.0f;
	/** true = a pole on both sides at every station; false = alternate sides (staggered). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road") bool bBothSides = false;
	/** Every Nth pole (per road) also gets a real light. 0 = emissive lamp head only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road") int32 RealLightEveryN = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road") float RealLightLumens = 4500.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road") float RealLightRadiusUnits = 1300.0f;
	/** Higher = keeps its real lights first when the per-district real-light budget is exceeded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road") int32 Priority = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road") bool bTrafficLights = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Road") bool bPedestrianSignals = false;
};

USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERLIGHTING_API FVoidLandmarkTierRule
{
	GENERATED_BODY()

	/** Matches LandmarkRegistry.json visibility_tier (1 = highest). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmark") int32 Tier = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmark") float LightMul = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmark") float HaloMul = 0.15f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmark") bool bBeacon = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmark") float WindowLitBoost = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmark") int32 UplightCount = 1;
};

USTRUCT()
struct VOIDWORLDBUILDERLIGHTING_API FVoidLightingProfilesFile
{
	GENERATED_BODY()

	UPROPERTY() FString Schema;
	UPROPERTY() TArray<FVoidDistrictLightingProfile> Districts;
	UPROPERTY() TArray<FVoidUsageRule> UsageRules;
	UPROPERTY() TArray<FVoidRoadLightingRule> RoadRules;
	UPROPERTY() TArray<FVoidLandmarkTierRule> LandmarkTiers;
};

// ---------------------------------------------------------------------
// Landmark presentation controls + cinematic hooks
// ---------------------------------------------------------------------

/** Per-landmark presentation dials. Live on each landmark lighting actor (Details panel / Sequencer) and are multiplied with the active preset. */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERLIGHTING_API FVoidLandmarkLightingControls
{
	GENERATED_BODY()

	/** Stronger lighting: scales the facade uplights. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmark") float Emphasis = 1.0f;
	/** Night illumination: extra multiplier blended in by the preset's NightWeight. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmark") float NightIllumination = 1.0f;
	/** Silhouette readability: scales the back-light behind the landmark. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmark") float SilhouetteReadability = 1.0f;
	/** Atmospheric emphasis: scales the back-light's volumetric scattering (visible only with volumetric fog on). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmark") float AtmosphericEmphasis = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landmark") bool bBeaconEnabled = true;
};

/** A named world-space reference the first-look camera script can use. Purely data; nothing here moves a camera. */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERLIGHTING_API FVoidCinematicAnchor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor") FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor") EVoidAnchorType Type = EVoidAnchorType::CameraInterest;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor") FName DistrictId;
	/** Meridian LandmarkRegistry id for Landmark anchors (empty otherwise). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor") FName LandmarkId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor") FVector Location = FVector::ZeroVector;
	/** Forward direction where meaningful (road tangent, landmark facing centre); otherwise zero. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor") FRotator Rotation = FRotator::ZeroRotator;
	/** Bounding radius of the subject (landmark/district) for framing math. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor") float Radius = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor") float Height = 0.0f;
	/** Landmark visibility tier (1 = Spire); 0 for non-landmarks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor") int32 Tier = 0;
	/** Suggested camera position and look-at for a hero shot of this anchor. Hints only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor") FVector SuggestedCameraLocation = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anchor") FVector SuggestedLookAt = FVector::ZeroVector;
};

/** Placeholder for a neon sign a later art pass can replace with a real mesh/material. */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERLIGHTING_API FVoidNeonHook
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neon") FName BuildingId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neon") FTransform Transform;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Neon") FVector2D SizeUnits = FVector2D(300.0, 120.0);
};

/** Parameter names shared by the generated emissive material and the actors that drive it. */
namespace VoidLightingParams
{
	static const FName ColorA(TEXT("LightColorA"));
	static const FName ColorB(TEXT("LightColorB"));
	static const FName Intensity(TEXT("Intensity"));
	static const FName LitFraction(TEXT("LitFraction"));

	/** Per-instance custom data layout (NumCustomDataFloats = 2). */
	constexpr int32 CustomDataThreshold = 0;
	constexpr int32 CustomDataBlend = 1;
	constexpr int32 NumCustomData = 2;

	static const FName GeneratedActorTag(TEXT("VOID.Lighting.Generated"));
}
