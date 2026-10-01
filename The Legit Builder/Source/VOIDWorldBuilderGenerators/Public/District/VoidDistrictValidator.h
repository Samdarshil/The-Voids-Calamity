// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "District/VoidDistrictTypes.h"
#include "District/VoidDistrictLayoutParams.h"
#include "Meridian/VoidMeridianRegistry.h"

/**
 * FVoidDistrictValidator
 *
 * Generation-time validation of a FVoidDistrictLayoutPlan, run BEFORE anything
 * is spawned. Adds issues to Plan.Report (Error blocks generation; Warning and
 * Info do not). Issue codes are stable ("VOID.District.*") so CI can filter.
 *
 * Checks (each maps to a "Avoid:" item in the Phase 5 brief or a Meridian rule):
 *   BuildingOutsideBoundary      buildings outside district boundaries
 *   BuildingOnRoad               buildings floating in roads
 *   DuplicateGeometry / Overlap  duplicate district geometry
 *   PublicSpaceOverlaps*         parks overlapping major structures
 *   PublicSpaceOnRoad / Outside  public spaces vs roads and boundaries
 *   RoadDeadEnd                  roads ending randomly
 *   RoadCrossesDistrict          roads routed through districts they must avoid
 *   BoundariesOverlap            district boundaries colliding on one layer
 *   AdjacencyNotHonored          DistrictRegistry adjacency_graph edges
 *   UnreachablePairConnected     RoadNetwork unreachable_pairs_by_design
 *   SpireSightlineBlocked        Master Plan Sec. 17 (non-blocking Warning)
 *   Sector0Sightline             Sector 0 must never have a Spire sightline
 *   DuplicateId                  stable-id uniqueness
 */
class VOIDWORLDBUILDERGENERATORS_API FVoidDistrictValidator
{
public:
	static void Validate(FVoidDistrictLayoutPlan& InOutPlan, const FVoidMeridianDataSet& Data, const FVoidDistrictLayoutParams& Params);

	/** Eye height used for the Spire sightline check (Unreal units). */
	static constexpr double SightlineEyeHeight = 170.0;

	/** Tolerance for endpoint / port coincidence (matches UVoidRoadGenerationSettings::JunctionToleranceUnits default). */
	static constexpr double EndpointTolerance = 50.0;
};
