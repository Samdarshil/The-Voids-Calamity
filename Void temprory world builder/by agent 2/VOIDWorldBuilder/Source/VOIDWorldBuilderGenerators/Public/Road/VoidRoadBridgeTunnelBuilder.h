// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Data/VoidDesignPackage.h"

/**
 * FVoidRoadBridgeTunnelBuilder
 *
 * Post-processes a road's already-built 3D centerline points (from
 * FVoidRoadSplineBuilder) to add a bridge/tunnel elevation ramp, and
 * computes where along the span to place pier (bridge) or portal
 * (tunnel) marker points.
 *
 * This is deliberately a simple greybox model, consistent with the
 * project-wide "blockout, not final art" scope: a bridge lifts off at
 * RampLengthUnits from each end, holds a flat elevated deck across the
 * middle, and comes back down at the far end; a tunnel does the mirror
 * image downward. There is no real-world grade/clearance engineering
 * here -- see Docs/RoadGeneratorArchitecture.md.
 *
 * Takes plain float parameters rather than UVoidRoadGenerationSettings
 * directly so this class has no UObject/settings dependency and is
 * trivially unit-testable; FVoidRoadGenerator reads the settings object
 * once and passes its values down.
 */
class VOIDWORLDBUILDERGENERATORS_API FVoidRoadBridgeTunnelBuilder
{
public:
	/**
	 * Applies a bridge (positive) or tunnel (negative) elevation ramp to
	 * Points in place. Does nothing if RoadSpec has neither bIsBridge nor
	 * bIsTunnel set, or if Points has fewer than 2 entries.
	 */
	static void ApplyElevationRamp(const FVoidRoadSpec& RoadSpec, TArray<FVector>& Points, float RampLengthUnits, float BridgeHeightUnits, float TunnelDepthUnits);

	/**
	 * Returns world-space (local actor space) positions for pier markers
	 * along a bridge's elevated middle span, spaced by PierSpacingUnits.
	 * Points must already have the elevation ramp applied. Returns an
	 * empty array for a non-bridge road.
	 */
	static TArray<FVector> ComputeBridgePierPositions(const FVoidRoadSpec& RoadSpec, const TArray<FVector>& Points, float RampLengthUnits, float PierSpacingUnits);

	/**
	 * Returns the two tunnel portal marker positions (first and last
	 * point of the span). Returns an empty array for a non-tunnel road or
	 * if Points has fewer than 2 entries.
	 */
	static TArray<FVector> ComputeTunnelPortalPositions(const FVoidRoadSpec& RoadSpec, const TArray<FVector>& Points);

private:
	/** Cumulative arc length (straight-line, point-to-point) at each index; CumulativeLengths[0] == 0. */
	static TArray<float> ComputeCumulativeLengths(const TArray<FVector>& Points);
};
