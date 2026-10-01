// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Road/VoidRoadSplineBuilder.h"
#include "Components/SplineComponent.h"

TArray<FVector> FVoidRoadSplineBuilder::BuildCenterlinePoints(const FVoidRoadSpec& RoadSpec)
{
	TArray<FVector> Points;
	Points.Reserve(RoadSpec.CenterlinePoints.Num());

	for (const FVector2D& Point2D : RoadSpec.CenterlinePoints)
	{
		Points.Add(FVector(Point2D.X, Point2D.Y, RoadSpec.ElevationUnits));
	}

	return Points;
}

TArray<FVector> FVoidRoadSplineBuilder::BuildRoundaboutLoopPoints(const FVoidRoadSpec& RoadSpec, int32 NumSegments)
{
	TArray<FVector> Points;

	if (RoadSpec.CenterlinePoints.Num() == 0 || RoadSpec.RoundaboutRadiusUnits <= 0.0f || NumSegments < 3)
	{
		return Points;
	}

	const FVector2D Center = RoadSpec.CenterlinePoints[0];
	Points.Reserve(NumSegments);

	for (int32 Index = 0; Index < NumSegments; ++Index)
	{
		const float Angle = (2.0f * PI * Index) / static_cast<float>(NumSegments);
		const float X = Center.X + RoadSpec.RoundaboutRadiusUnits * FMath::Cos(Angle);
		const float Y = Center.Y + RoadSpec.RoundaboutRadiusUnits * FMath::Sin(Angle);
		Points.Add(FVector(X, Y, RoadSpec.ElevationUnits));
	}

	return Points;
}

void FVoidRoadSplineBuilder::ApplyPointsToSpline(USplineComponent* SplineComponent, const TArray<FVector>& Points, bool bClosedLoop)
{
	if (!SplineComponent)
	{
		return;
	}

	SplineComponent->ClearSplinePoints(false);

	for (const FVector& Point : Points)
	{
		SplineComponent->AddSplinePoint(Point, ESplineCoordinateSpace::Local, false);
	}

	for (int32 Index = 0; Index < SplineComponent->GetNumberOfSplinePoints(); ++Index)
	{
		// Curve (not Linear) gives smooth, auto-computed tangents through
		// each way point -- this is the entirety of "curvature" support
		// for Phase 3 (see this class's header comment).
		SplineComponent->SetSplinePointType(Index, ESplinePointType::Curve, false);
	}

	SplineComponent->SetClosedLoop(bClosedLoop, false);
	SplineComponent->UpdateSpline();
}
