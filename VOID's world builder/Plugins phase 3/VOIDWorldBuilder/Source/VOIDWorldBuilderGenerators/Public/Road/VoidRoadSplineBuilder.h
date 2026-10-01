// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Data/VoidDesignPackage.h"

class USplineComponent;

/**
 * FVoidRoadSplineBuilder
 *
 * Converts a road spec's 2D design-space centerline into 3D world-space
 * points and applies them to a USplineComponent. "Curvature" support (per
 * the Phase 3 spec) is implemented here: every point is set to
 * ESplinePointType::Curve, giving the spline smooth auto-computed
 * tangents through way points rather than sharp linear segments -- there
 * is no separate authored "curvature" field in the design package, since
 * a spline's shape is already fully determined by its way points plus
 * tangent mode. See Docs/RoadGeneratorArchitecture.md for the reasoning.
 *
 * Elevation here is the road's flat, uniform ElevationUnits only. Bridge
 * and tunnel ramp profiles are a separate post-process step -- see
 * FVoidRoadBridgeTunnelBuilder -- kept out of this class so a plain road's
 * point generation stays simple and so bridge/tunnel logic can change
 * without touching centerline/roundabout math.
 */
class VOIDWORLDBUILDERGENERATORS_API FVoidRoadSplineBuilder
{
public:
	/** Converts RoadSpec.CenterlinePoints (2D, design space) into 3D local-space points at RoadSpec.ElevationUnits. */
	static TArray<FVector> BuildCenterlinePoints(const FVoidRoadSpec& RoadSpec);

	/** For a Roundabout-type road: treats CenterlinePoints[0] as the center and generates NumSegments points around a circle of RoadSpec.RoundaboutRadiusUnits. Returns an empty array if CenterlinePoints is empty or RoundaboutRadiusUnits <= 0. */
	static TArray<FVector> BuildRoundaboutLoopPoints(const FVoidRoadSpec& RoadSpec, int32 NumSegments = 24);

	/** Clears and repopulates SplineComponent with Points, set to smooth (Curve) tangents, optionally as a closed loop (for roundabouts). */
	static void ApplyPointsToSpline(USplineComponent* SplineComponent, const TArray<FVector>& Points, bool bClosedLoop);
};
