// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Data/VoidDesignPackage.h"
#include "Data/VoidValidationReport.h"

/**
 * Pure-data types for the District Generator (Phase 5).
 *
 * Deliberately NOT USTRUCTs: the layout plan is an intermediate, generation-
 * time structure (Meridian data + settings -> plan -> actors). Keeping it
 * plain C++ means (a) no UHT surface for a structure nobody serializes, and
 * (b) the whole planning stage (profile resolution, layout, validation) is
 * engine-independent logic that can be unit tested without spawning a world.
 *
 * The plan reuses Core's FVoidRoadSpec / FVoidBuildingSpec so that anything
 * the plan produces can be handed to the existing Road generator (and to a
 * future Building generator) through the existing FVoidDesignPackage contract
 * without any new interface.
 */

namespace VoidDistrictNames
{
	/** Pseudo-owner for the Live Network spine / ring roads. Meridian_Master_Plan.md Sec. 11 calls these infrastructure, not a district. */
	inline const FName LiveNetworkOwner(TEXT("live_network"));

	inline const FName OlympusSpire(TEXT("olympus_spire"));
	inline const FName WhiteZones(TEXT("white_zones"));
	inline const FName MetroArchives(TEXT("metro_archives"));
	inline const FName Undercroft(TEXT("undercroft"));
	inline const FName Sector0(TEXT("sector_0"));

	/** Landmark / route / tunnel ids that the generator keys behaviour on. All taken verbatim from the Meridian registries. */
	inline const FName SpireSilhouette(TEXT("olympus_spire_silhouette"));
	inline const FName ContinuityMonument(TEXT("continuity_monument"));
	inline const FName EliteResidentialTowers(TEXT("elite_residential_towers"));
	inline const FName AtriumOfPerfectMemory(TEXT("atrium_of_perfect_memory"));
	inline const FName SpireServerHub(TEXT("olympus_spire_server_hub"));
	inline const FName WeaveDisplayColumn(TEXT("white_zones_weave_display_column"));
	inline const FName MetroArchivesBuilding(TEXT("metro_archives_building"));
	inline const FName MetroArchivesReadingRoom(TEXT("metro_archives_reading_room"));
	inline const FName MetroArchivesExhibitCase(TEXT("metro_archives_sealed_exhibit_case"));
	inline const FName Sector0Threshold(TEXT("sector_0_entrance_threshold"));
	inline const FName Sector0EchoRelay(TEXT("sector_0_echo_relay_station"));
	inline const FName SpireRadialSpine(TEXT("spire_radial_spine"));
	inline const FName MidTierRingRoad(TEXT("mid_tier_ring_road"));
	inline const FName UndercroftArchivesSeam(TEXT("undercroft_metro_archives_transit_seam"));
	inline const FName UndercroftSector0Threshold(TEXT("undercroft_sector_0_entrance_threshold"));

	/** BuilderRules.json error_handling.highest_priority_flag id. Attached to the server hub landmark so later systems do not generate encounter content. */
	inline const FName ServerHubFlag(TEXT("spire_server_hub_nyx_discrepancy"));
}

enum class EVoidDistrictDensity : uint8
{
	NearZero,
	Sparse,
	Moderate,
	High
};

enum class EVoidPublicSpaceType : uint8
{
	Plaza,
	Park,
	Courtyard,
	CivicSpace,
	PedestrianZone,
	MajorOpenSpace
};

/**
 * Per-district generation character. Every value carries a provenance line so
 * the handoff and the Output Log can say, for each number, whether Meridian
 * data drove it or a designer-tunable default filled a gap the data leaves
 * open. ("The generator should consume data rather than invent arbitrary lore.")
 */
struct VOIDWORLDBUILDERGENERATORS_API FVoidDistrictProfile
{
	FName DistrictId;
	FString StructuralModel;
	FString RadialBand;
	FString VerticalTier;

	EVoidDistrictDensity Density = EVoidDistrictDensity::Moderate;

	/** Fraction (0..1) of a lot's buildable area covered by the building footprint. */
	double BuildingCoverage = 0.5;

	int32 MinStories = 1;
	int32 MaxStories = 4;
	double StoryHeightUnits = 400.0;

	/** Target fraction (0..1) of a district's grid cells that become public space instead of lots. */
	double OpenSpaceRatio = 0.15;

	/** Road density: grid block edge length. Smaller = denser road network. */
	double BlockSizeUnits = 6000.0;

	/** 0 = purely residential/civic, 1 = purely commercial. Negative = Meridian data is silent; the generator then emits untyped-balance fabric. */
	double CommercialShare = -1.0;

	/** 0..1, exposed to the Environment agent. Also scales the greybox tree markers placed in parks. */
	double VegetationDensity = 0.3;

	/** Plaza cells per White-Zone-style node (or per district for singular districts). */
	int32 PlazaCellsPerNode = 1;

	/** Landmarks the registry assigns to this district (filled in by the layout builder). */
	int32 LandmarkCount = 0;

	/** "<parameter> = <value> (<source>)" lines. Source is either a Meridian file/section or "Default". */
	TArray<FString> Provenance;

	double MinHeightUnits() const { return MinStories * StoryHeightUnits; }
	double MaxHeightUnits() const { return MaxStories * StoryHeightUnits; }
	bool IsCommercialKnown() const { return CommercialShare >= 0.0; }
};

struct VOIDWORLDBUILDERGENERATORS_API FVoidPlannedBoundary
{
	FName Id;

	/** Outer polygon, CCW, world XY. */
	TArray<FVector2D> Polygon;

	/** Optional circular hole (annulus boundaries such as the Undercroft substrate). HoleRadius <= 0 means no hole. */
	FVector2D HoleCenter = FVector2D::ZeroVector;
	double HoleRadius = 0.0;

	/** World Z of the boundary outline (below-grade districts sit under grade). */
	double Z = 0.0;

	bool bFromOverride = false;
};

struct VOIDWORLDBUILDERGENERATORS_API FVoidPlannedRoad
{
	FVoidRoadSpec Spec;
	FName OwnerId;
	TArray<FName> ServesDistricts;
	FName RouteId;

	/** True when a free (unconnected) endpoint is intentional (cul-de-sac). Everything else must connect to another road. */
	bool bFreeEndpointsAllowed = false;
};

struct VOIDWORLDBUILDERGENERATORS_API FVoidPlannedBuilding
{
	FVoidBuildingSpec Spec;
	FName OwnerId;
	FName CategoryId;

	/** Oriented footprint (the FootprintCorners in Spec are derived from these). */
	FVector2D Center = FVector2D::ZeroVector;
	FVector2D HalfExtent = FVector2D::ZeroVector;
	double YawRadians = 0.0;
	double BaseZ = 0.0;

	/** Landmark-scale or civic structure. Public spaces must never overlap these. */
	bool bMajorStructure = false;
};

struct VOIDWORLDBUILDERGENERATORS_API FVoidPlannedPublicSpace
{
	FName Id;
	FName OwnerId;
	EVoidPublicSpaceType Type = EVoidPublicSpaceType::Plaza;

	TArray<FVector2D> Polygon;
	FVector2D HoleCenter = FVector2D::ZeroVector;
	double HoleRadius = 0.0;

	FVector2D Center = FVector2D::ZeroVector;
	double Z = 0.0;
	int32 NodeIndex = INDEX_NONE;
};

struct VOIDWORLDBUILDERGENERATORS_API FVoidPlannedLandmark
{
	/** Stable instance id. Equals RegistryId for single-instance landmarks; "<RegistryId>.node_<n>" for per-node instanced fixtures. */
	FName Id;

	/** LandmarkRegistry.json id, verbatim (never renamed). */
	FName RegistryId;
	FName OwnerId;

	/** Non-empty when this landmark IS one of the district's buildings (e.g. the Spire tower). The landmark actor then has no proxy mesh of its own -- no duplicate geometry. */
	FName BackingBuildingId;

	FString Type;
	int32 VisibilityTier = INDEX_NONE;
	bool bInterior = false;
	bool bBelowGrade = false;

	FVector Location = FVector::ZeroVector;
	double YawDegrees = 0.0;

	/** Footprint half extents and height of the landmark volume (0 for anchors with no volume, e.g. interiors). */
	FVector2D HalfExtent = FVector2D::ZeroVector;
	double Height = 0.0;

	/** True when the location is a production-side placeholder (Meridian data has no coordinates by policy) and not from a VoidDistrictLayoutOverrides file. */
	bool bPositionIsPlaceholder = true;

	/** Non-empty when BuilderRules.json blocks encounter content for this landmark. */
	FName OpenFlagId;

	TArray<FName> SightlineVisibleFrom;
	TArray<FName> SightlineExcluded;

	int32 NodeIndex = INDEX_NONE;
};

/** Connection points future systems (Phase 2 connective tissue, Metro, Navigation) attach to. Never geometry. */
struct VOIDWORLDBUILDERGENERATORS_API FVoidPlannedPort
{
	FName Id;
	FName OwnerId;
	FString Kind;
	FVector Location = FVector::ZeroVector;
	TArray<FName> Connects;
};

/** A distributed node of a typology district (White Zones network node). */
struct VOIDWORLDBUILDERGENERATORS_API FVoidPlannedNode
{
	int32 Index = INDEX_NONE;
	bool bFlagship = false;
	FVector2D Center = FVector2D::ZeroVector;
	double AngleRadians = 0.0;
	FString WorldPartitionRegion;
	TArray<FVector2D> Footprint;
};

struct VOIDWORLDBUILDERGENERATORS_API FVoidPlannedDistrict
{
	FName Id;
	FString DisplayName;
	FVoidDistrictProfile Profile;

	FVoidPlannedBoundary Boundary;

	/** Sub-regions such as the Undercroft's three belts. */
	TArray<FVoidPlannedBoundary> SubRegions;

	TArray<FVoidPlannedRoad> Roads;
	TArray<FVoidPlannedBuilding> Buildings;
	TArray<FVoidPlannedPublicSpace> PublicSpaces;
	TArray<FVoidPlannedLandmark> Landmarks;
	TArray<FVoidPlannedPort> Ports;
	TArray<FVoidPlannedNode> Nodes;

	/** True when geometry came from an explicit FVoidDesignPackage instead of synthesis. */
	bool bFromExplicitPackage = false;
};

struct VOIDWORLDBUILDERGENERATORS_API FVoidDistrictLayoutPlan
{
	/** Districts in Meridian macro generation order. */
	TArray<FVoidPlannedDistrict> Districts;

	/** Live Network spine + ring roads. Owned by VoidDistrictNames::LiveNetworkOwner, not by a district. */
	TArray<FVoidPlannedRoad> LiveNetworkRoads;

	FVoidValidationReport Report;

	const FVoidPlannedDistrict* FindDistrict(FName Id) const
	{
		return Districts.FindByPredicate([Id](const FVoidPlannedDistrict& D) { return D.Id == Id; });
	}

	FVoidPlannedDistrict* FindDistrict(FName Id)
	{
		return Districts.FindByPredicate([Id](const FVoidPlannedDistrict& D) { return D.Id == Id; });
	}

	/** All roads in generation order: LiveNetwork first only if the macro order says so is handled by the builder; this is simply district roads then live network roads appended in plan order. */
	TArray<const FVoidPlannedRoad*> GatherAllRoads() const
	{
		TArray<const FVoidPlannedRoad*> Result;
		for (const FVoidPlannedDistrict& District : Districts)
		{
			for (const FVoidPlannedRoad& Road : District.Roads)
			{
				Result.Add(&Road);
			}
		}
		for (const FVoidPlannedRoad& Road : LiveNetworkRoads)
		{
			Result.Add(&Road);
		}
		return Result;
	}
};
