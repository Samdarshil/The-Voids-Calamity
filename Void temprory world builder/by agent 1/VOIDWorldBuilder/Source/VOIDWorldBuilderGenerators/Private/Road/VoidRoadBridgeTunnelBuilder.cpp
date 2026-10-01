// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Road/VoidRoadBridgeTunnelBuilder.h"

TArray<float> FVoidRoadBridgeTunnelBuilder::ComputeCumulativeLengths(const TArray<FVector>& Points)
{
	TArray<float> CumulativeLengths;
	CumulativeLengths.Reserve(Points.Num());

	float Running = 0.0f;
	CumulativeLengths.Add(0.0f);

	for (int32 Index = 1; Index < Points.Num(); ++Index)
	{
		Running += FVector::Dist(Points[Index - 1], Points[Index]);
		CumulativeLengths.Add(Running);
	}

	return CumulativeLengths;
}

void FVoidRoadBridgeTunnelBuilder::ApplyElevationRamp(const FVoidRoadSpec& RoadSpec, TArray<FVector>& Points, float RampLengthUnits, float BridgeHeightUnits, float TunnelDepthUnits)
{
	if (Points.Num() < 2 || (!RoadSpec.bIsBridge && !RoadSpec.bIsTunnel))
	{
		return;
	}

	const float Sign = RoadSpec.bIsBridge ? 1.0f : -1.0f;
	const float TargetOffset = RoadSpec.bIsBridge ? BridgeHeightUnits : TunnelDepthUnits;
	const float SafeRampLength = FMath::Max(RampLengthUnits, KINDA_SMALL_NUMBER);

	const TArray<float> CumulativeLengths = ComputeCumulativeLengths(Points);
	const float TotalLength = CumulativeLengths.Last();

	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		const float DistFromStart = CumulativeLengths[Index];
		const float DistFromEnd = TotalLength - DistFromStart;
		const float RampFactor = FMath::Clamp(FMath::Min(DistFromStart, DistFromEnd) / SafeRampLength, 0.0f, 1.0f);

		Points[Index].Z += Sign * TargetOffset * RampFactor;
	}
}

TArray<FVector> FVoidRoadBridgeTunnelBuilder::ComputeBridgePierPositions(const FVoidRoadSpec& RoadSpec, const TArray<FVector>& Points, float RampLengthUnits, float PierSpacingUnits)
{
	TArray<FVector> PierPositions;

	if (!RoadSpec.bIsBridge || Points.Num() < 2 || PierSpacingUnits <= 0.0f)
	{
		return PierPositions;
	}

	const TArray<float> CumulativeLengths = ComputeCumulativeLengths(Points);
	const float TotalLength = CumulativeLengths.Last();
	const float SafeRampLength = FMath::Max(RampLengthUnits, KINDA_SMALL_NUMBER);

	// Piers only belong in the fully-elevated middle span, not on the ramps.
	const float SpanStart = SafeRampLength;
	const float SpanEnd = TotalLength - SafeRampLength;

	if (SpanEnd <= SpanStart)
	{
		return PierPositions; // Span too short for the ramps to leave any flat middle -- no piers, ramps alone carry the bridge.
	}

	for (float TargetDistance = SpanStart; TargetDistance <= SpanEnd; TargetDistance += PierSpacingUnits)
	{
		// Find the segment [Index-1, Index] that TargetDistance falls within and lerp.
		for (int32 Index = 1; Index < CumulativeLengths.Num(); ++Index)
		{
			if (TargetDistance <= CumulativeLengths[Index] || Index == CumulativeLengths.Num() - 1)
			{
				const float SegmentStart = CumulativeLengths[Index - 1];
				const float SegmentEnd = CumulativeLengths[Index];
				const float SegmentLength = FMath::Max(SegmentEnd - SegmentStart, KINDA_SMALL_NUMBER);
				const float Alpha = FMath::Clamp((TargetDistance - SegmentStart) / SegmentLength, 0.0f, 1.0f);

				PierPositions.Add(FMath::Lerp(Points[Index - 1], Points[Index], Alpha));
				break;
			}
		}
	}

	return PierPositions;
}

TArray<FVector> FVoidRoadBridgeTunnelBuilder::ComputeTunnelPortalPositions(const FVoidRoadSpec& RoadSpec, const TArray<FVector>& Points)
{
	TArray<FVector> PortalPositions;

	if (!RoadSpec.bIsTunnel || Points.Num() < 2)
	{
		return PortalPositions;
	}

	PortalPositions.Add(Points[0]);
	PortalPositions.Add(Points.Last());
	return PortalPositions;
}
