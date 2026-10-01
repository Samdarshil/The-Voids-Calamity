// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VoidWorldBuilderSettings.generated.h"

/** How a legacy design package's 2D [x, y] source coordinates map onto Unreal's X/Y axes. Meridian itself has no coordinates - this applies to FVoidDesignPackage data only. */
UENUM(BlueprintType)
enum class EVoidSourceAxisConvention : uint8
{
	/** Source +X -> Unreal +X, source +Y -> Unreal +Y, Z up. (Unreal is left-handed: +X forward, +Y right, +Z up.) */
	UnrealNative UMETA(DisplayName = "Unreal Native (X fwd, Y right, Z up)"),

	/** Source +X -> Unreal +X, source +Y -> Unreal -Y. Use for right-handed, Y-north CAD/GIS style sources. */
	FlipY UMETA(DisplayName = "Flip Y (right-handed source)")
};

/** One radial band's extent, in Unreal units from the Spire origin. Builder-authored - NOT Meridian data. */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERCORE_API FVoidBandRadius
{
	GENERATED_BODY()

	/** Must match a name in Meridian_Master.json world_scale_metadata.radial_bands (e.g. "mid_tier_rings"). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Layout")
	FName BandId;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "0.0"))
	double InnerRadiusUU = 0.0;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "0.0"))
	double OuterRadiusUU = 0.0;

	FVoidBandRadius() = default;
	FVoidBandRadius(FName InBand, double InInner, double InOuter) : BandId(InBand), InnerRadiusUU(InInner), OuterRadiusUU(InOuter) {}
};

/**
 * UVoidWorldBuilderSettings
 *
 * The single, central configuration object for shared Builder concerns: world space, Meridian layout,
 * and import policy. (UVoidImportSettings and UVoidRoadGenerationSettings remain for their own
 * module-specific options and share the same config file, Config/DefaultVOIDWorldBuilder.ini.)
 *
 * IMPORTANT - why the layout values exist: Meridian intentionally provides only relative, qualitative
 * radial bands and states that "the Builder calculates all implementation geometry at generation time"
 * (Meridian_Master.json builder_configuration.geometry_calculation_authority). The band radii below
 * are therefore the Builder's own greybox scale. The shipped defaults are PLACEHOLDERS for a
 * blockout, not design figures; every generation that uses them logs a warning. Replace them with
 * approved values when the design team supplies a scale.
 */
UCLASS(Config = VOIDWorldBuilder, DefaultConfig, meta = (DisplayName = "VOID World Builder"))
class VOIDWORLDBUILDERCORE_API UVoidWorldBuilderSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UVoidWorldBuilderSettings();

	// ---- World space (the ONE coordinate conversion; see FVoidWorldSpace) ----

	/** Multiplier from legacy design-package units to Unreal units (1 UU = 1 cm). 1.0 because FVoidDesignPackage is documented as "Unreal units". */
	UPROPERTY(Config, EditAnywhere, Category = "World Space", meta = (ClampMin = "0.000001"))
	double SourceUnitsToUnrealUnits = 1.0;

	UPROPERTY(Config, EditAnywhere, Category = "World Space")
	EVoidSourceAxisConvention SourceAxisConvention = EVoidSourceAxisConvention::UnrealNative;

	/** Where the Spire (radial + vertical origin anchor) sits in the level, in Unreal units. Added centrally to every converted position. Default: level origin. */
	UPROPERTY(Config, EditAnywhere, Category = "World Space")
	FVector WorldOriginOffsetUU = FVector::ZeroVector;

	/**
	 * Largest absolute X/Y/Z magnitude (Unreal units) any generated position may have. Conservative
	 * default of 2,097,152 (~21 km) is the classic single-precision-safe limit; positions beyond it
	 * are rejected, never silently altered. Raise deliberately for large-world use.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "World Space", meta = (ClampMin = "1.0"))
	double MaxAbsCoordinateUU = 2097152.0;

	// ---- Meridian layout (Builder-authored greybox scale - NOT Meridian data) ----

	/** Radial band extents from the Spire origin. Bands not listed here cannot be placed on the radial gradient. */
	UPROPERTY(Config, EditAnywhere, Category = "Meridian Layout")
	TArray<FVoidBandRadius> BandRadii;

	/** Azimuth (degrees) used for a radial route when it has no entry in RouteAzimuthDegrees. 0 = +X, increasing toward +Y. */
	UPROPERTY(Config, EditAnywhere, Category = "Meridian Layout")
	double DefaultRadialAzimuthDegrees = 0.0;

	/** Per-route azimuth overrides, keyed by Meridian route id (e.g. "spire_radial_spine"). */
	UPROPERTY(Config, EditAnywhere, Category = "Meridian Layout")
	TMap<FName, double> RouteAzimuthDegrees;

	/** Segments used to approximate a ring road's circle. */
	UPROPERTY(Config, EditAnywhere, Category = "Meridian Layout", meta = (ClampMin = "8", ClampMax = "512"))
	int32 RingRoadSegments = 96;

	/** Spacing of centerline samples along a radial route. */
	UPROPERTY(Config, EditAnywhere, Category = "Meridian Layout", meta = (ClampMin = "100.0"))
	double RadialSampleSpacingUU = 2500.0;

	// ---- Meridian import policy ----

	/** Meridian's --strict-canon-lock (default enabled per BuilderManifest.json): a locked input whose SHA-256 differs from PackageManifest.json is an Error. A locked input that is simply absent is only a Warning (it cannot be verified, which is not the same as differing). */
	UPROPERTY(Config, EditAnywhere, Category = "Meridian Import")
	bool bStrictCanonLock = true;

	/** Meridian's --allow-flagged-content (default disabled per BuilderManifest.json). If false, content tied to an unresolved flagged_open_item (e.g. the skybridge awaiting sign-off) is skipped. */
	UPROPERTY(Config, EditAnywhere, Category = "Meridian Import")
	bool bAllowFlaggedContent = false;

	/** Look up a band's extents. Returns false if the band has no configured radius. */
	bool FindBandRadius(FName BandId, double& OutInner, double& OutOuter) const;

	/** Convenience: settings CDO, never null. */
	static const UVoidWorldBuilderSettings* Get();
};
