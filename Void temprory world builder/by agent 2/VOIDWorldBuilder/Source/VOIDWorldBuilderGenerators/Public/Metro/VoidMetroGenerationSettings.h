// Copyright VOID Engineering Studio. Internal tool - not for shipping content.
// Added by Agent 2 (Phase 6 Metro Generator).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Types/VoidMetroTypes.h"
#include "VoidMetroGenerationSettings.generated.h"

/**
 * UVoidMetroGenerationSettings
 *
 * Project-wide Metro Generator configuration (Project Settings -> Plugins ->
 * VOID World Builder Metro Generation). Mirrors UVoidRoadGenerationSettings.
 *
 * IMPORTANT -- PLACEHOLDER LAYOUT VALUES.
 * Meridian's package contains NO coordinates by design (Meridian_Master.json
 * coordinate_policy). Every default in the "Placeholder Layout" category is a
 * generator-side production placeholder chosen so a first-look scene has
 * something to show; none is canon and none comes from a Meridian file except
 * where noted (district -> radial band names, which are copied from
 * DistrictRegistry.json). Real coordinates supplied through the authored
 * "metro" block always win over these values.
 */
UCLASS(Config = VOIDWorldBuilder, DefaultConfig, meta = (DisplayName = "VOID World Builder Metro Generation"))
class VOIDWORLDBUILDERGENERATORS_API UVoidMetroGenerationSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UVoidMetroGenerationSettings();

	// --- Ownership / cleanup ---------------------------------------------

	/** Every generated metro actor is stamped with this owner key. Regeneration destroys all loaded actors with the same key first, so re-running never accumulates duplicates. */
	UPROPERTY(Config, EditAnywhere, Category = "Ownership")
	FName OwnerKey = TEXT("MeridianMetro");

	// --- Placeholder layout ------------------------------------------------

	/** If false, anything without authored coordinates is skipped (with a warning) instead of getting a placeholder position. */
	UPROPERTY(Config, EditAnywhere, Category = "Placeholder Layout")
	bool bAllowPlaceholderLayout = true;

	/** Radius from the world origin (Olympus Spire is the canon-explicit origin: DistrictRegistry.json radial_position_type = absolute_center_origin_point_canon_explicit) per radial band. Band NAMES come from DistrictRegistry.json radial_bands_ordered; the RADII are placeholders. Units: cm. */
	UPROPERTY(Config, EditAnywhere, Category = "Placeholder Layout")
	TMap<FName, float> BandRadiusUnits;

	/** District -> radial band. Values copied from DistrictRegistry.json radial_band. */
	UPROPERTY(Config, EditAnywhere, Category = "Placeholder Layout")
	TMap<FName, FName> DistrictBand;

	/** District -> azimuth (degrees) around the origin. PLACEHOLDER (Meridian specifies no bearings; Sector 0 is explicitly 'narrative_nowhere_production_placement_only'). */
	UPROPERTY(Config, EditAnywhere, Category = "Placeholder Layout")
	TMap<FName, float> DistrictAzimuthDegrees;

	/** Angular offset between successive stations of the same district (e.g. multiple White Zones nodes). */
	UPROPERTY(Config, EditAnywhere, Category = "Placeholder Layout", meta = (ClampMin = "0.1"))
	float NodeAngularSpacingDegrees = 6.0f;

	/** Sample count for a placeholder ring line (member station angles are inserted in addition). */
	UPROPERTY(Config, EditAnywhere, Category = "Placeholder Layout", meta = (ClampMin = "8", ClampMax = "512"))
	int32 RingSampleCount = 180;

	// --- Vertical placement ---------------------------------------------------

	/**
	 * Default grade for Live-network track/stations that carry no grade in the data.
	 * MetroNetwork.json does not say whether the grav-rail is elevated, at grade, or
	 * underground, so the default is AtGrade (it shares the surface 'spire_radial_spine').
	 * Set to Elevated for a more cinematic read -- that is a design choice, not canon.
	 * The Dead network is always Underground (it is the 'pre_council_subway_service_layer').
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Vertical Placement")
	EVoidMetroGrade LiveNetworkDefaultGrade = EVoidMetroGrade::AtGrade;

	UPROPERTY(Config, EditAnywhere, Category = "Vertical Placement", meta = (ClampMin = "100.0"))
	float ElevatedHeightUnits = 1200.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Vertical Placement", meta = (ClampMin = "100.0"))
	float UndergroundDepthUnits = 1000.0f;

	/** Length over which track ramps between two different grades (e.g. underground tunnel rising to a surface station). */
	UPROPERTY(Config, EditAnywhere, Category = "Vertical Placement", meta = (ClampMin = "100.0"))
	float GradeRampLengthUnits = 3000.0f;

	// --- Stations & entrances ---------------------------------------------------

	/** Entrances generated for a station that has none authored. Clamped by the station's MaxEntrances (Sector 0 threshold = 1, a hard Meridian constraint). */
	UPROPERTY(Config, EditAnywhere, Category = "Stations", meta = (ClampMin = "0", ClampMax = "8"))
	int32 DefaultEntrancesPerStation = 1;

	UPROPERTY(Config, EditAnywhere, Category = "Stations", meta = (ClampMin = "200.0"))
	float EntranceOffsetUnits = 1500.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Stations", meta = (ClampMin = "1000.0"))
	float PlatformLengthUnits = 6000.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Stations", meta = (ClampMin = "100.0"))
	float PlatformWidthUnits = 600.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Stations")
	float PlatformHeightUnits = 110.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Stations")
	float CanopyHeightUnits = 450.0f;

	// --- Track ------------------------------------------------------------------

	/** Live grav-rail guideway deck width. */
	UPROPERTY(Config, EditAnywhere, Category = "Track", meta = (ClampMin = "50.0"))
	float LiveGuidewayWidthUnits = 500.0f;

	/** Dead-network rail gauge (centre-to-centre). */
	UPROPERTY(Config, EditAnywhere, Category = "Track", meta = (ClampMin = "50.0"))
	float DeadRailGaugeUnits = 150.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Track", meta = (ClampMin = "40.0"))
	float SleeperSpacingUnits = 150.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Track", meta = (ClampMin = "200.0"))
	float ViaductPierSpacingUnits = 2500.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Track", meta = (ClampMin = "150.0"))
	float TunnelRadiusUnits = 600.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Track", meta = (ClampMin = "6", ClampMax = "32"))
	int32 TunnelSides = 12;

	/** Longest single straight span before the resolver subdivides a path (needed for smooth grade ramps and tunnel rings). */
	UPROPERTY(Config, EditAnywhere, Category = "Track", meta = (ClampMin = "200.0"))
	float MaxSegmentLengthUnits = 2500.0f;

	/** Draw a thin ground-level marker above underground track so the alignment is visible from the surface. */
	UPROPERTY(Config, EditAnywhere, Category = "Visibility")
	bool bDrawSurfaceTraceForUnderground = true;

	// --- Rendering ---------------------------------------------------------------

	/** Optional material for all generated metro geometry. Default is the engine's vertex-colour debug material (path assumed, see HANDOFF known-unverified list); falls back to no material if it does not load. */
	UPROPERTY(Config, EditAnywhere, Category = "Rendering", meta = (AllowedClasses = "/Script/Engine.MaterialInterface"))
	FSoftObjectPath MetroMaterial;

	/** Flip triangle winding for metro meshes. Same purpose as the Road Generator's bFlipWindingForVerification: if metro surfaces look inside-out on first in-editor check, enable this. Not verifiable without an editor. */
	UPROPERTY(Config, EditAnywhere, Category = "Rendering")
	bool bFlipTriangleWinding = false;

	/** Draw persistent debug lines along resolved track and spheres at stations/entrances/portals. */
	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bDrawDebugVisualization = false;
};
