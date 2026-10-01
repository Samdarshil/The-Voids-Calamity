// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Data/VoidDesignPackage.h"
#include "Road/VoidRoadMeshBuilder.h"

/** A road together with its already-built 3D points (after elevation/bridge/tunnel processing) -- what FVoidRoadIntersectionBuilder needs to find where roads meet. */
struct VOIDWORLDBUILDERGENERATORS_API FVoidBuiltRoad
{
	FVoidRoadSpec Spec;
	TArray<FVector> Points;
};

/** Classification of a detected junction, driving what kind of pad geometry gets generated. */
enum class EVoidRoadJunctionType : uint8
{
	DeadEnd,          // One road endpoint, connected to nothing else.
	CulDeSac,         // One road endpoint, connected to nothing else, with bCulDeSacAtEnd set.
	TwoWayJoin,       // Exactly two different roads meeting (a simple join, not a true branching intersection).
	TJunction,        // Exactly three different roads meeting.
	FourWayJunction,  // Exactly four different roads meeting.
	Complex,          // Five or more different roads meeting.
	RoundaboutSpur    // A road connecting into a Roundabout-type road's ring, handled as a simplified edge pad rather than blended into the ring geometry.
};

/** One detected junction: where it is, what kind, which roads meet there, and how big a pad to generate. */
struct VOIDWORLDBUILDERGENERATORS_API FVoidRoadJunction
{
	FVector Location = FVector::ZeroVector;
	EVoidRoadJunctionType Type = EVoidRoadJunctionType::DeadEnd;
	TArray<FVoidElementId> ConnectedRoadIds;
	float PadRadius = 0.0f;
};

/**
 * FVoidRoadIntersectionBuilder
 *
 * Builds the junction graph for a district's roads by clustering
 * coincident endpoints (within a tolerance) and/or explicit
 * FVoidRoadSpec::ConnectionIds, then classifies each cluster.
 *
 * Deliberately simple for Phase 3 greybox scope: junctions get a flat
 * polygonal pad sized to the widest connected road, not a proper
 * per-approach blended intersection mesh. Roundabout ring/spur blending
 * is similarly simplified to a pad at the ring's edge rather than true
 * geometric blending -- see Docs/RoadGeneratorArchitecture.md.
 */
class VOIDWORLDBUILDERGENERATORS_API FVoidRoadIntersectionBuilder
{
public:
	/** Finds and classifies every junction among BuiltRoads. Roundabout-type roads are excluded from endpoint clustering (they have no simple start/end) but are matched against as RoundaboutSpur targets for other roads' ConnectionIds/coincident endpoints against their center point. */
	static TArray<FVoidRoadJunction> BuildJunctionGraph(const TArray<FVoidBuiltRoad>& BuiltRoads, float ToleranceUnits);

	/** A flat NumSegments-gon disc at Junction.Location, radius Junction.PadRadius. */
	static FVoidRoadMeshSection BuildJunctionPad(const FVoidRoadJunction& Junction, const FLinearColor& VertexColor, int32 NumSegments = 12);

	/** A simple alternating-color striped strip crossing one approach at a junction -- the greybox stand-in for a crosswalk. */
	static FVoidRoadMeshSection BuildCrosswalkStripe(const FVector& JunctionCenter, const FVector2D& ApproachDirection2D, float RoadWidth, float StripeZoneLength, const FLinearColor& StripeColorA, const FLinearColor& StripeColorB, int32 NumStripes = 5);
};
