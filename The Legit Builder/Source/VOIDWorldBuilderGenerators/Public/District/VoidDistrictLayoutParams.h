// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "VoidDistrictLayoutParams.generated.h"

/**
 * Optional per-district character overrides (Project Settings). Unset values
 * (negative / zero) mean "use the density-band default". Every value that ends
 * up in a district profile is logged with its source.
 */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERGENERATORS_API FVoidDistrictCharacterOverride
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
	double OpenSpaceRatio = -1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
	double VegetationDensity = -1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
	double BlockSize = -1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
	double CommercialShare = -1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
	int32 MinStories = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
	int32 MaxStories = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
	int32 PlazaCellsPerNode = 0;
};

/**
 * FVoidDistrictLayoutParams
 *
 * Every number the District Generator needs that the Meridian data
 * deliberately does NOT provide. Meridian_Master.json states
 * coordinate_policy = "no_fabricated_coordinates" and
 * fabricated_values_policy = "none_used_relative_qualitative_bands_only", and
 * Meridian_Master_Plan.md Sec. 11 says "No literal road names, widths, or
 * coordinates are specified". Real-world scale therefore has to come from the
 * production side, not the design data. This struct is that production side:
 * one clearly-labelled, designer-editable block (Project Settings -> Plugins
 * -> VOID World Builder District Generation -> Layout), never hidden in code.
 *
 * Anything the Meridian data DOES say (district set, radial-band order,
 * vertical tiers, adjacency, 2-4 story White Zone ceiling, landmark
 * visibility ordering, ...) is read from the data and is NOT a parameter here.
 * The Layout Builder logs which is which (FVoidDistrictProfile::Provenance).
 *
 * Units are Unreal units (1 uu = 1 cm). Origin (0,0) is the Olympus Spire
 * (DistrictRegistry: radial_position_type absolute_center_origin_point_canon_explicit).
 */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERGENERATORS_API FVoidDistrictLayoutParams
{
	GENERATED_BODY()

	// --- Radial bands (outer radius of each band). Order comes from Meridian data; sizes are production choices. ---

	/** Outer radius of the "core" band (the Olympus Spire district). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radial Bands", meta = (ClampMin = "1000.0"))
	double CoreRadius = 40000.0;

	/** Outer radius of "inner_rings_unbuilt" (connective tissue; nothing is generated here, only spine roads pass through). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radial Bands", meta = (ClampMin = "1000.0"))
	double InnerRingsOuterRadius = 90000.0;

	/** Outer radius of "mid_tier_rings" (White Zones band). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radial Bands", meta = (ClampMin = "1000.0"))
	double MidTierOuterRadius = 210000.0;

	/** Outer radius of "seam_zone" (Metro Archives). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radial Bands", meta = (ClampMin = "1000.0"))
	double SeamOuterRadius = 240000.0;

	/** Outer radius of "outer_rings" (outer edge of the Undercroft substrate). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Radial Bands", meta = (ClampMin = "1000.0"))
	double OuterRingsOuterRadius = 400000.0;

	// --- Vertical tiers (Z below grade). Tier ORDER comes from data. ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vertical Tiers", meta = (ClampMin = "0.0"))
	double BelowGradeLivingDepth = 1500.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vertical Tiers", meta = (ClampMin = "0.0"))
	double BelowGradeAgingDepth = 3000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vertical Tiers", meta = (ClampMin = "0.0"))
	double AbsoluteBottomDepth = 6000.0;

	// --- Live Network (spine + ring). Topology comes from RoadNetwork.json; counts and widths are production choices. ---

	/** Number of radial arterials (and White Zone nodes: one node sits between each pair of neighbouring arterials). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Live Network", meta = (ClampMin = "1", ClampMax = "16"))
	int32 SpokeCount = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Live Network")
	double SpokeBaseAngleDegrees = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Live Network", meta = (ClampMin = "100.0"))
	double PrimaryRoadWidth = 1000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Live Network", meta = (ClampMin = "100.0"))
	double SecondaryRoadWidth = 800.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Live Network", meta = (ClampMin = "100.0"))
	double LocalRoadWidth = 600.0;

	/** Extra clearance around every road corridor (sidewalk + curb + margin) that buildings and public spaces must keep. Road generator sidewalk+curb is 175 per side. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Live Network", meta = (ClampMin = "0.0"))
	double RoadClearance = 250.0;

	/** Max angular step (degrees) between vertices of curved (ring / arc) roads. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Live Network", meta = (ClampMin = "1.0", ClampMax = "30.0"))
	double ArcStepDegrees = 3.0;

	// --- Olympus Spire ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Olympus Spire", meta = (ClampMin = "500.0"))
	double BasePlazaRadius = 12000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Olympus Spire", meta = (ClampMin = "500.0"))
	double SpireTowerHalfFootprint = 3000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Olympus Spire", meta = (ClampMin = "100.0"))
	double SpireHeight = 60000.0;

	/** The registry says "elite residential towers" is a cluster but not how many. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Olympus Spire", meta = (ClampMin = "1", ClampMax = "12"))
	int32 EliteTowerCount = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Olympus Spire", meta = (ClampMin = "100.0"))
	double EliteTowerHalfFootprint = 1500.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Olympus Spire", meta = (ClampMin = "100.0"))
	double EliteTowerHeight = 30000.0;

	// --- White Zones ---

	/** Half-width, in grid cells, of a node along the ring direction. A node is (2 * this) cells wide. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "White Zones", meta = (ClampMin = "1", ClampMax = "4"))
	int32 NodeHalfCellsAlongRing = 2;

	/** Half-depth, in grid cells, of a node across the ring direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "White Zones", meta = (ClampMin = "1", ClampMax = "4"))
	int32 NodeHalfCellsAcrossRing = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "White Zones", meta = (ClampMin = "500.0"))
	double LotWidth = 2400.0;

	// --- Metro Archives ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metro Archives")
	double SeamAngleDegrees = 22.5;

	/** Half extents of the bespoke building footprint: X = radial direction, Y = tangential. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metro Archives")
	FVector2D ArchivesBuildingHalfExtent = FVector2D(6000.0, 4000.0);

	/** "Mid-rise" per LandmarkRegistry visibility tier 3. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metro Archives", meta = (ClampMin = "100.0"))
	double ArchivesHeight = 6400.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metro Archives", meta = (ClampMin = "500.0"))
	double ArchivesForecourtDepth = 6000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Metro Archives", meta = (ClampMin = "1000.0"))
	double ArchivesBoundaryHalfSize = 15000.0;

	// --- Sector 0 ("narrative nowhere, production placement only" per DistrictRegistry) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sector 0")
	double Sector0AngleDegrees = 202.5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sector 0", meta = (ClampMin = "1000.0"))
	double Sector0HalfSize = 20000.0;

	// --- Character defaults for qualitative bands the data states as words, not numbers ---

	/** Footprint coverage by density band (Meridian_Master_Plan.md Sec. 22 gives the band, not the number). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character", meta = (ClampMin = "0.05", ClampMax = "0.95"))
	double CoverageHigh = 0.70;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character", meta = (ClampMin = "0.05", ClampMax = "0.95"))
	double CoverageModerate = 0.50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character", meta = (ClampMin = "0.05", ClampMax = "0.95"))
	double CoverageSparse = 0.30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character", meta = (ClampMin = "0.05", ClampMax = "0.95"))
	double CoverageNearZero = 0.05;

	/** Metres-per-storey equivalent. White Zones are "2-4 stories, hard ceiling" (Master Plan Sec. 5). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character", meta = (ClampMin = "100.0"))
	double StoryHeight = 400.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character", meta = (ClampMin = "1000.0"))
	double BlockSize = 6000.0;

	// --- Greybox ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox")
	bool bGenerateBoundaryOutlines = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox")
	bool bGenerateBelowGradeBoundaries = true;

	/** Greybox tree markers inside parks, scaled by the district's vegetation density. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox")
	bool bGenerateTreeMarkers = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox", meta = (ClampMin = "200.0"))
	double TreeSpacing = 1200.0;

	/** Deterministic seed. Same seed + same data = identical districts on every regeneration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Determinism")
	int32 Seed = 1;
};
