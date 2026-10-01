// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Misc/AutomationTest.h"
#include "Road/VoidRoadSplineBuilder.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidRoadSplineBuilderStraightRoadTest,
	"VOID.WorldBuilder.RoadGenerator.SplineBuilder.StraightRoadProducesCollinearPoints",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidRoadSplineBuilderStraightRoadTest::RunTest(const FString& Parameters)
{
	FVoidRoadSpec RoadSpec;
	RoadSpec.CenterlinePoints = { FVector2D(0, 0), FVector2D(500, 0), FVector2D(1000, 0) };
	RoadSpec.ElevationUnits = 42.0f;

	const TArray<FVector> Points = FVoidRoadSplineBuilder::BuildCenterlinePoints(RoadSpec);

	TestEqual(TEXT("Should produce one 3D point per 2D input point"), Points.Num(), 3);
	TestEqual(TEXT("Every point should sit at ElevationUnits"), Points[1].Z, 42.0f);
	TestEqual(TEXT("X should pass through unchanged"), Points[1].X, 500.0f);
	TestEqual(TEXT("Y should pass through unchanged"), Points[1].Y, 0.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidRoadSplineBuilderCurvedRoadTest,
	"VOID.WorldBuilder.RoadGenerator.SplineBuilder.CurvedRoadPreservesWayPoints",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidRoadSplineBuilderCurvedRoadTest::RunTest(const FString& Parameters)
{
	FVoidRoadSpec RoadSpec;
	RoadSpec.CenterlinePoints = { FVector2D(0, 0), FVector2D(300, 400), FVector2D(1000, 200), FVector2D(1400, 900) };

	const TArray<FVector> Points = FVoidRoadSplineBuilder::BuildCenterlinePoints(RoadSpec);

	TestEqual(TEXT("Should produce one 3D point per 2D input way point"), Points.Num(), 4);
	TestEqual(TEXT("Way points are preserved exactly, not resampled"), FVector2D(Points[2].X, Points[2].Y), FVector2D(1000, 200));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidRoadSplineBuilderRoundaboutTest,
	"VOID.WorldBuilder.RoadGenerator.SplineBuilder.RoundaboutProducesCircleAtRadius",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidRoadSplineBuilderRoundaboutTest::RunTest(const FString& Parameters)
{
	FVoidRoadSpec RoadSpec;
	RoadSpec.RoadType = EVoidRoadType::Roundabout;
	RoadSpec.CenterlinePoints = { FVector2D(1000, 1000) };
	RoadSpec.RoundaboutRadiusUnits = 500.0f;

	const TArray<FVector> Points = FVoidRoadSplineBuilder::BuildRoundaboutLoopPoints(RoadSpec, 16);

	TestEqual(TEXT("Should produce NumSegments points"), Points.Num(), 16);

	for (const FVector& Point : Points)
	{
		const float DistFromCenter = FVector2D(Point.X - 1000.0f, Point.Y - 1000.0f).Size();
		TestTrue(TEXT("Every point should sit at RoundaboutRadiusUnits from the center"), FMath::IsNearlyEqual(DistFromCenter, 500.0f, 1.0f));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidRoadSplineBuilderRoundaboutMissingRadiusTest,
	"VOID.WorldBuilder.RoadGenerator.SplineBuilder.RoundaboutWithoutRadiusProducesNoPoints",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidRoadSplineBuilderRoundaboutMissingRadiusTest::RunTest(const FString& Parameters)
{
	FVoidRoadSpec RoadSpec;
	RoadSpec.RoadType = EVoidRoadType::Roundabout;
	RoadSpec.CenterlinePoints = { FVector2D(0, 0) };
	// RoundaboutRadiusUnits left at its default (0) -- must fail gracefully, not crash or divide by zero.

	const TArray<FVector> Points = FVoidRoadSplineBuilder::BuildRoundaboutLoopPoints(RoadSpec);

	TestEqual(TEXT("A roundabout with no radius should produce zero points, not crash"), Points.Num(), 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
