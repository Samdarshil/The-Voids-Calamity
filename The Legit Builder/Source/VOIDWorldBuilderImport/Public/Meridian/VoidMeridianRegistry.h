// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"

/**
 * Plain (non-UObject, non-USTRUCT) mirror of the Meridian design registries
 * (DistrictRegistry.json, LandmarkRegistry.json, RoadNetwork.json, and the
 * optional per-district *_data.json files). Added in Phase 5 as an ADDITIVE
 * Import-module type: nothing in FVoidDesignPackage / FVoidDistrictData
 * changed. Meridian data is a relationship-and-rules manifest set, not a
 * geometry format (Meridian_Master.json builder_configuration.coordinate_policy
 * = "no_fabricated_coordinates"), so it cannot be forced into the single-
 * district FVoidDesignPackage envelope; this is the smallest adapter that
 * lets the District Generator consume it.
 *
 * Every field below is read verbatim from the Meridian files. Nothing here is
 * invented; anything the files do not state is left empty / INDEX_NONE /
 * unset (TOptional) so downstream code can tell "data says X" from "data is
 * silent".
 */

struct VOIDWORLDBUILDERIMPORT_API FVoidMeridianDistrictEntry
{
	FName Id;
	FString DisplayName;
	FString Status;
	FString RadialBand;
	FString RadialPositionType;
	FString VerticalTier;
	FString StructuralModel;
	int32 GenerationPriority = INDEX_NONE;
	int32 BuildOrderIndex = INDEX_NONE;
	TArray<FName> DependsOn;

	/** From DistrictRegistry.json streaming_relationships. */
	FString WorldPartitionRegion;
	FString WorldPartitionTemplate;
	FString Instancing;

	/** True if the district's own <data_reference> file was found and parsed. Only Metro_Archives_data.json ships in the current Meridian ZIP. */
	bool bHasDistrictData = false;
	FString DistrictDataFile;

	/** From the district's own data file, when present. Unset = the data is silent. */
	TOptional<bool> bCommercialPresence;
	TOptional<bool> bCorporatePresence;
	FString VerticalStratum;
	TArray<FName> SubLocationIds;
};

struct VOIDWORLDBUILDERIMPORT_API FVoidMeridianLandmarkEntry
{
	FName Id;
	FName DistrictId;
	FName SubLocationRef;
	FString Type;
	FString CanonStatus;

	/** Numeric visibility tier (1 = most dominant skyline element), or INDEX_NONE when the registry gives a text label instead (interior / below-grade). */
	int32 VisibilityTier = INDEX_NONE;
	FString VisibilityTierLabel;

	FString RecognitionPriority;
	FString GameplayImportance;
	FString NavigationImportance;
	FString InstancingPolicy;
	TArray<FName> SightlineVisibleFrom;
	TArray<FName> SightlineExcluded;

	bool IsSkylineLandmark() const { return VisibilityTier != INDEX_NONE; }
	bool IsInterior() const { return VisibilityTierLabel.Contains(TEXT("interior")); }
	bool IsBelowGrade() const { return VisibilityTierLabel.Contains(TEXT("below_grade")); }
};

struct VOIDWORLDBUILDERIMPORT_API FVoidMeridianRoute
{
	FName Id;
	FString Category;          // radial_arterial | ring_road | service_maintenance_route
	FString Network;           // live_network | dead_network (from the category table)
	int32 HierarchyTier = INDEX_NONE;
	FName OriginDistrict;
	FName OriginSubLocation;
	FName OwningDistrict;
	FString RadialBand;
	FString HardConstraint;
	TArray<FString> TraversesBands;
	TArray<FName> ServesDistricts;
	TArray<FName> SharedByDistricts;
	TArray<FName> SubBeltsServed;
	FName GenerationDependency;
};

struct VOIDWORLDBUILDERIMPORT_API FVoidMeridianTunnel
{
	FName Id;
	FString Type;
	TArray<FName> Connects;
	FString Gate;
};

struct VOIDWORLDBUILDERIMPORT_API FVoidMeridianAdjacencyEdge
{
	FName From;
	FName To;
	FString AdjacencyType;
};

struct VOIDWORLDBUILDERIMPORT_API FVoidMeridianDataSet
{
	FString RootDirectory;

	/** Districts in registry order (NOT generation order; see MacroGenerationOrder). */
	TArray<FVoidMeridianDistrictEntry> Districts;
	TArray<FVoidMeridianLandmarkEntry> Landmarks;
	TArray<FVoidMeridianRoute> Routes;
	TArray<FVoidMeridianTunnel> Tunnels;
	TArray<FVoidMeridianAdjacencyEdge> AdjacencyEdges;

	/** DistrictRegistry.json connectivity_graph.connective_tissue_required_between. */
	TArray<TPair<FName, FName>> ConnectiveTissuePairs;

	/** RoadNetwork.json traversal_graph.unreachable_pairs_by_design. */
	TArray<TPair<FName, FName>> UnreachablePairs;

	/** DistrictRegistry.json hierarchy.radial_bands_ordered. */
	TArray<FString> RadialBandsOrdered;

	/** Meridian_Master.json generation_pipeline.generation_order[].target, in step order. Contains district ids AND infrastructure steps (e.g. "live_network_spine"). */
	TArray<FName> MacroGenerationOrder;

	const FVoidMeridianDistrictEntry* FindDistrict(FName Id) const
	{
		return Districts.FindByPredicate([Id](const FVoidMeridianDistrictEntry& E) { return E.Id == Id; });
	}

	const FVoidMeridianRoute* FindRoute(FName Id) const
	{
		return Routes.FindByPredicate([Id](const FVoidMeridianRoute& R) { return R.Id == Id; });
	}

	const FVoidMeridianTunnel* FindTunnel(FName Id) const
	{
		return Tunnels.FindByPredicate([Id](const FVoidMeridianTunnel& T) { return T.Id == Id; });
	}

	bool AreAdjacent(FName A, FName B) const
	{
		for (const FVoidMeridianAdjacencyEdge& Edge : AdjacencyEdges)
		{
			if ((Edge.From == A && Edge.To == B) || (Edge.From == B && Edge.To == A))
			{
				return true;
			}
		}
		return false;
	}
};

/**
 * Optional, designer-authored placement overrides (VoidDistrictLayoutOverrides.json).
 * The Meridian data contains no coordinates by policy; when the team decides
 * real boundaries / landmark positions, they go here (never into the locked
 * Meridian files). Every entry is optional.
 */
struct VOIDWORLDBUILDERIMPORT_API FVoidMeridianBoundaryOverride
{
	FName DistrictId;
	TArray<FVector2D> Polygon;
};

struct VOIDWORLDBUILDERIMPORT_API FVoidMeridianLandmarkOverride
{
	FName LandmarkId;
	FVector Location = FVector::ZeroVector;
	double YawDegrees = 0.0;
};

struct VOIDWORLDBUILDERIMPORT_API FVoidMeridianLayoutOverrides
{
	TArray<FVoidMeridianBoundaryOverride> Boundaries;
	TArray<FVoidMeridianLandmarkOverride> Landmarks;

	const FVoidMeridianBoundaryOverride* FindBoundary(FName DistrictId) const
	{
		return Boundaries.FindByPredicate([DistrictId](const FVoidMeridianBoundaryOverride& B) { return B.DistrictId == DistrictId; });
	}

	const FVoidMeridianLandmarkOverride* FindLandmark(FName LandmarkId) const
	{
		return Landmarks.FindByPredicate([LandmarkId](const FVoidMeridianLandmarkOverride& L) { return L.LandmarkId == LandmarkId; });
	}
};
