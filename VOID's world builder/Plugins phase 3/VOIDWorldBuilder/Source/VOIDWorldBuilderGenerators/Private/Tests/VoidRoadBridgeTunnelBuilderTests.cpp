// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Misc/AutomationTest.h"
#include "Road/VoidRoadBridgeTunnelBuilder.h"
#include "Road/VoidRoadSplineBuilder.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidRoadBridgeElevationTest,
	"VOID.WorldBuilder.RoadGenerator.BridgeTunnel.BridgeRampsUpThenFlattensInMiddle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidRoadBridgeElevationTest::RunTest(const FString& Parameters)
{
	FVoidRoadSpec RoadSpec;
	RoadSpec.CenterlinePoints = { FVector2D(0, 0), FVector2D(500, 0), FVector2D(1000, 0), FVector2D(1500, 0), FVector2D(2000, 0) };
	RoadSpec.bIsBridge = true;

	TArray<FVector> Points = FVoidRoadSplineBuilder::BuildCenterlinePoints(RoadSpec);
	FVoidRoadBridgeTunnelBuilder::ApplyElevationRamp(RoadSpec, Points, /*RampLengthUnits=*/400.0f, /*BridgeHeightUnits=*/300.0f, /*TunnelDepthUnits=*/300.0f);

	TestEqual(TEXT("The very first point (distance 0 from start) should be at ground level"), Points[0].Z, 0.0f);
	TestTrue(TEXT("A point in the middle of the span should be elevated"), Points[2].Z > 100.0f);
	TestTrue(TEXT("Elevation should never go negative for a bridge"), Points[2].Z >= 0.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidRoadTunnelElevationTest,
	"VOID.WorldBuilder.RoadGenerator.BridgeTunnel.TunnelDipsDownThenFlattensInMiddle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidRoadTunnelElevationTest::RunTest(const FString& Parameters)
{
	FVoidRoadSpec RoadSpec;
	RoadSpec.CenterlinePoints = { FVector2D(0, 0), FVector2D(500, 0), FVector2D(1000, 0), FVector2D(1500, 0), FVector2D(2000, 0) };
	RoadSpec.bIsTunnel = true;

	TArray<FVector> Points = FVoidRoadSplineBuilder::BuildCenterlinePoints(RoadSpec);
	FVoidRoadBridgeTunnelBuilder::ApplyElevationRamp(RoadSpec, Points, 400.0f, 300.0f, 300.0f);

	TestEqual(TEXT("The very first point should be at ground level"), Points[0].Z, 0.0f);
	TestTrue(TEXT("A point in the middle of the span should dip below ground"), Points[2].Z < -100.0f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidRoadPlainRoadUnaffectedTest,
	"VOID.WorldBuilder.RoadGenerator.BridgeTunnel.PlainRoadIsUnaffectedByRampLogic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidRoadPlainRoadUnaffectedTest::RunTest(const FString& Parameters)
{
	FVoidRoadSpec RoadSpec;
	RoadSpec.CenterlinePoints = { FVector2D(0, 0), FVector2D(1000, 0) };
	// Neither bIsBridge nor bIsTunnel set.

	TArray<FVector> Points = FVoidRoadSplineBuilder::BuildCenterlinePoints(RoadSpec);
	const TArray<FVector> OriginalPoints = Points;
	FVoidRoadBridgeTunnelBuilder::ApplyElevationRamp(RoadSpec, Points, 400.0f, 300.0f, 300.0f);

	TestEqual(TEXT("A plain road's points should be unchanged by ApplyElevationRamp"), Points[0].Z, OriginalPoints[0].Z);
	TestEqual(TEXT("A plain road's points should be unchanged by ApplyElevationRamp"), Points[1].Z, OriginalPoints[1].Z);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidRoadBridgePierPlacementTest,
	"VOID.WorldBuilder.RoadGenerator.BridgeTunnel.PiersOnlyPlacedInElevatedMiddleSpan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidRoadBridgePierPlacementTest::RunTest(const FString& Parameters)
{
	FVoidRoadSpec RoadSpec;
	RoadSpec.CenterlinePoints = { FVector2D(0, 0), FVector2D(1000, 0), FVector2D(2000, 0), FVector2D(3000, 0) };
	RoadSpec.bIsBridge = true;

	TArray<FVector> Points = FVoidRoadSplineBuilder::BuildCenterlinePoints(RoadSpec);
	FVoidRoadBridgeTunnelBuilder::ApplyElevationRamp(RoadSpec, Points, 400.0f, 300.0f, 300.0f);

	const TArray<FVector> Piers = FVoidRoadBridgeTunnelBuilder::ComputeBridgePierPositions(RoadSpec, Points, 400.0f, 500.0f);

	TestTrue(TEXT("A 3000-unit bridge with 400-unit ramps should have room for at least one pier"), Piers.Num() > 0);

	for (const FVector& Pier : Piers)
	{
		TestTrue(TEXT("Every pier should be at least 400 units from the start"), Pier.X >= 400.0f - KINDA_SMALL_NUMBER);
		TestTrue(TEXT("Every pier should be at least 400 units from the end"), Pier.X <= 3000.0f - 400.0f + KINDA_SMALL_NUMBER);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidRoadTunnelPortalPlacementTest,
	"VOID.WorldBuilder.RoadGenerator.BridgeTunnel.TunnelHasExactlyTwoPortals",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidRoadTunnelPortalPlacementTest::RunTest(const FString& Parameters)
{
	FVoidRoadSpec RoadSpec;
	RoadSpec.CenterlinePoints = { FVector2D(0, 0), FVector2D(1000, 0), FVector2D(2000, 0) };
	RoadSpec.bIsTunnel = true;

	const TArray<FVector> Points = FVoidRoadSplineBuilder::BuildCenterlinePoints(RoadSpec);
	const TArray<FVector> Portals = FVoidRoadBridgeTunnelBuilder::ComputeTunnelPortalPositions(RoadSpec, Points);

	TestEqual(TEXT("A tunnel should have exactly 2 portal markers"), Portals.Num(), 2);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
