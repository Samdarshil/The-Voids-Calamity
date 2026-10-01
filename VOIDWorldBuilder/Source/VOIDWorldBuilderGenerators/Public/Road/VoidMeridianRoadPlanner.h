// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Data/VoidMeridianData.h"
#include "Data/VoidDesignPackage.h"
#include "Data/VoidValidationReport.h"
#include "Coordinates/VoidWorldSpace.h"
#include "Settings/VoidWorldBuilderSettings.h"

/**
 * Plain-data snapshot of the layout settings used by the planner (testable without a settings CDO).
 * These are BUILDER-authored greybox values, never Meridian data - see UVoidWorldBuilderSettings.
 */
struct VOIDWORLDBUILDERGENERATORS_API FVoidMeridianLayout
{
	TArray<FVoidBandRadius> BandRadii;
	double DefaultRadialAzimuthDegrees = 0.0;
	TMap<FName, double> RouteAzimuthDegrees;
	int32 RingRoadSegments = 96;
	double RadialSampleSpacingUU = 2500.0;
	bool bAllowFlaggedContent = false;

	static FVoidMeridianLayout FromSettings(const UVoidWorldBuilderSettings* Settings);
	bool FindBand(FName BandId, double& OutInner, double& OutOuter) const;
	double GetAzimuthFor(FName RouteId) const;
};

/** One Meridian route turned into a buildable road. Spec points are ALREADY WORLD SPACE (converted through FVoidWorldSpace). */
struct VOIDWORLDBUILDERGENERATORS_API FVoidPlannedRoad
{
	FVoidRoadSpec Spec;
	FName CategoryId;
	int32 HierarchyTier = 0;
	EVoidNetworkKind Network = EVoidNetworkKind::Unknown;
	TArray<FName> ServedDistricts;

	/** Radial roads: azimuth of the spoke. Ring roads: azimuth of vertex 0. Used to place crossings exactly on a vertex. */
	double AzimuthDegrees = 0.0;
	/** Radial roads: min/max radius from the Spire. Ring roads: both = ring radius. */
	double MinRadiusUU = 0.0;
	double MaxRadiusUU = 0.0;
	bool bIsRing = false;
};

/** Two planned roads that cross mid-span (not at an endpoint), e.g. a radial spine crossing a ring road. */
struct VOIDWORLDBUILDERGENERATORS_API FVoidPlannedCrossing
{
	FName RoadA;
	FName RoadB;
	FVector Location = FVector::ZeroVector;
};

/** A route/tunnel/bridge that Meridian describes only topologically. It is recorded, never given invented geometry. */
struct VOIDWORLDBUILDERGENERATORS_API FVoidTopologyOnlyRoute
{
	FName Id;
	FName CategoryId;
	int32 HierarchyTier = 0;
	EVoidNetworkKind Network = EVoidNetworkKind::Unknown;
	TArray<FName> ServedDistricts;
	FString Reason;
};

struct VOIDWORLDBUILDERGENERATORS_API FVoidMeridianRoadPlan
{
	TArray<FVoidPlannedRoad> Roads;
	TArray<FVoidPlannedCrossing> Crossings;
	TArray<FVoidTopologyOnlyRoute> TopologyOnly;
	/** Ids skipped because they are tied to an unresolved flagged item and flagged content is not allowed. */
	TArray<FName> SkippedFlagged;
};

/**
 * FVoidMeridianRoadPlanner
 *
 * Turns Meridian's abstract RoadNetwork into world-space road specs, using ONLY what Meridian states:
 *
 *   radial_arterial (Live)  -> a straight radial centerline from the inner edge of its first band to the
 *                              outer edge of its last band (Meridian_Master_Plan Sec. 11: "generated
 *                              running from the Spire's Base Plaza outward").
 *   ring_road (Live)        -> a closed circle at the middle of its band ("circumferential connectors at
 *                              a given radial band").
 *   service_maintenance_route (Dead) -> topology only. Meridian gives the Dead Network no surface footprint
 *                              or position, so nothing is generated (and none is invented).
 *   bridge_relationships    -> not roads (a pedestrian skybridge between buildings); left to Building generators.
 *   tunnel_relationships    -> topology only.
 *
 * Not stated by Meridian, therefore NOT produced: widths (the Builder's default per-type profile applies and is
 * flagged bWidthIsBuilderDefault), sidewalks, curbs, crosswalks, alleys, highways, roundabouts, lane counts.
 * Not stated by Meridian and therefore Builder-chosen: band radii, spoke azimuth, ring segment count - all from
 * FVoidMeridianLayout, all logged as placeholder scale.
 *
 * Pure function of its inputs (no UWorld, no actors) so it is unit-testable.
 */
class VOIDWORLDBUILDERGENERATORS_API FVoidMeridianRoadPlanner
{
public:
	/**
	 * @return true if at least one road was planned. Problems (missing band radius, out-of-range coordinates,
	 * violated hard constraints) are appended to OutReport; the affected route is skipped, others continue.
	 */
	static bool BuildPlan(const FVoidMeridianWorld& World, const FVoidWorldSpace& WorldSpace, const FVoidMeridianLayout& Layout, FVoidMeridianRoadPlan& OutPlan, FVoidValidationReport& OutReport);

	/** Meridian hierarchy tier -> Road Generator type: 1 Primary, 2 Secondary, everything else Service. (Meridian names no Highway/Local/Alley.) */
	static EVoidRoadType MapTierToRoadType(int32 Tier);
};
