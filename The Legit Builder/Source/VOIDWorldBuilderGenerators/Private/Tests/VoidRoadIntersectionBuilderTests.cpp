// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Misc/AutomationTest.h"
#include "Road/VoidRoadIntersectionBuilder.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace VoidRoadIntersectionTestPrivate
{
	static FVoidBuiltRoad MakeRoad(const FString& Id, FVector2D Start, FVector2D End)
	{
		FVoidBuiltRoad Road;
		Road.Spec.Id = FVoidElementId(FName(*Id));
		Road.Spec.WidthUnits = 600.0f;
		Road.Points = { FVector(Start.X, Start.Y, 0.0f), FVector(End.X, End.Y, 0.0f) };
		return Road;
	}

	static int32 CountJunctionsOfType(const TArray<FVoidRoadJunction>& Junctions, EVoidRoadJunctionType Type)
	{
		int32 Count = 0;
		for (const FVoidRoadJunction& Junction : Junctions)
		{
			if (Junction.Type == Type) { ++Count; }
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidRoadIntersectionDeadEndTest,
	"VOID.WorldBuilder.RoadGenerator.Intersections.UnconnectedEndpointIsDeadEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidRoadIntersectionDeadEndTest::RunTest(const FString& Parameters)
{
	using namespace VoidRoadIntersectionTestPrivate;

	TArray<FVoidBuiltRoad> Roads;
	Roads.Add(MakeRoad(TEXT("road_a"), FVector2D(0, 0), FVector2D(1000, 0)));

	const TArray<FVoidRoadJunction> Junctions = FVoidRoadIntersectionBuilder::BuildJunctionGraph(Roads, 50.0f);

	TestEqual(TEXT("A single isolated road should produce exactly 2 dead-end junctions"), CountJunctionsOfType(Junctions, EVoidRoadJunctionType::DeadEnd), 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidRoadIntersectionCulDeSacTest,
	"VOID.WorldBuilder.RoadGenerator.Intersections.FlaggedEndpointIsCulDeSac",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidRoadIntersectionCulDeSacTest::RunTest(const FString& Parameters)
{
	using namespace VoidRoadIntersectionTestPrivate;

	TArray<FVoidBuiltRoad> Roads;
	FVoidBuiltRoad Road = MakeRoad(TEXT("road_a"), FVector2D(0, 0), FVector2D(1000, 0));
	Road.Spec.bCulDeSacAtEnd = true;
	Roads.Add(Road);

	const TArray<FVoidRoadJunction> Junctions = FVoidRoadIntersectionBuilder::BuildJunctionGraph(Roads, 50.0f);

	TestEqual(TEXT("The start point (bCulDeSacAtEnd only applies to the end) should still be a plain dead end"), CountJunctionsOfType(Junctions, EVoidRoadJunctionType::DeadEnd), 1);
	TestEqual(TEXT("The end point should be classified as a cul-de-sac"), CountJunctionsOfType(Junctions, EVoidRoadJunctionType::CulDeSac), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidRoadIntersectionTJunctionTest,
	"VOID.WorldBuilder.RoadGenerator.Intersections.ThreeCoincidentRoadsAreTJunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidRoadIntersectionTJunctionTest::RunTest(const FString& Parameters)
{
	using namespace VoidRoadIntersectionTestPrivate;

	TArray<FVoidBuiltRoad> Roads;
	Roads.Add(MakeRoad(TEXT("road_a"), FVector2D(0, 0), FVector2D(1000, 1000)));
	Roads.Add(MakeRoad(TEXT("road_b"), FVector2D(2000, 1000), FVector2D(1000, 1000)));
	Roads.Add(MakeRoad(TEXT("road_c"), FVector2D(1000, 1000), FVector2D(1000, 2000)));

	const TArray<FVoidRoadJunction> Junctions = FVoidRoadIntersectionBuilder::BuildJunctionGraph(Roads, 50.0f);

	TestEqual(TEXT("Three roads meeting at one coincident point should form exactly one T-junction"), CountJunctionsOfType(Junctions, EVoidRoadJunctionType::TJunction), 1);
	TestEqual(TEXT("The other three endpoints should each be dead ends"), CountJunctionsOfType(Junctions, EVoidRoadJunctionType::DeadEnd), 3);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidRoadIntersectionFourWayTest,
	"VOID.WorldBuilder.RoadGenerator.Intersections.FourCoincidentRoadsAreFourWayJunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidRoadIntersectionFourWayTest::RunTest(const FString& Parameters)
{
	using namespace VoidRoadIntersectionTestPrivate;

	TArray<FVoidBuiltRoad> Roads;
	Roads.Add(MakeRoad(TEXT("north"), FVector2D(1000, 1000), FVector2D(1000, 2000)));
	Roads.Add(MakeRoad(TEXT("south"), FVector2D(1000, 1000), FVector2D(1000, 0)));
	Roads.Add(MakeRoad(TEXT("east"), FVector2D(1000, 1000), FVector2D(2000, 1000)));
	Roads.Add(MakeRoad(TEXT("west"), FVector2D(1000, 1000), FVector2D(0, 1000)));

	const TArray<FVoidRoadJunction> Junctions = FVoidRoadIntersectionBuilder::BuildJunctionGraph(Roads, 50.0f);

	TestEqual(TEXT("Four roads meeting at one point should form exactly one four-way junction"), CountJunctionsOfType(Junctions, EVoidRoadJunctionType::FourWayJunction), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidRoadIntersectionToleranceTest,
	"VOID.WorldBuilder.RoadGenerator.Intersections.NearlyCoincidentEndpointsMergeWithinTolerance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidRoadIntersectionToleranceTest::RunTest(const FString& Parameters)
{
	using namespace VoidRoadIntersectionTestPrivate;

	TArray<FVoidBuiltRoad> Roads;
	Roads.Add(MakeRoad(TEXT("road_a"), FVector2D(0, 0), FVector2D(1000, 1000)));
	// Slightly off (10 units) from road_a's endpoint -- should still merge within a 50-unit tolerance.
	Roads.Add(MakeRoad(TEXT("road_b"), FVector2D(1010, 1000), FVector2D(2000, 1000)));

	const TArray<FVoidRoadJunction> Junctions = FVoidRoadIntersectionBuilder::BuildJunctionGraph(Roads, 50.0f);

	TestEqual(TEXT("Two roads with nearly-coincident endpoints should merge into one two-way join"), CountJunctionsOfType(Junctions, EVoidRoadJunctionType::TwoWayJoin), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidRoadIntersectionRoundaboutSpurTest,
	"VOID.WorldBuilder.RoadGenerator.Intersections.SpurIntoRoundaboutIsClassifiedSeparately",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidRoadIntersectionRoundaboutSpurTest::RunTest(const FString& Parameters)
{
	using namespace VoidRoadIntersectionTestPrivate;

	TArray<FVoidBuiltRoad> Roads;

	FVoidBuiltRoad Roundabout;
	Roundabout.Spec.Id = FVoidElementId(FName(TEXT("roundabout_1")));
	Roundabout.Spec.RoadType = EVoidRoadType::Roundabout;
	Roundabout.Spec.RoundaboutRadiusUnits = 500.0f;
	Roundabout.Points = { FVector(1000, 1000, 0) }; // center only; BuildJunctionGraph reads Points[0] as the center.
	Roads.Add(Roundabout);

	FVoidBuiltRoad Spur = MakeRoad(TEXT("spur_road"), FVector2D(1000, 1000), FVector2D(2000, 1000));
	Spur.Spec.ConnectionIds.Add(FVoidElementId(FName(TEXT("roundabout_1"))));
	Roads.Add(Spur);

	const TArray<FVoidRoadJunction> Junctions = FVoidRoadIntersectionBuilder::BuildJunctionGraph(Roads, 50.0f);

	TestEqual(TEXT("A road connecting via connectionIds to a roundabout should be one RoundaboutSpur junction"), CountJunctionsOfType(Junctions, EVoidRoadJunctionType::RoundaboutSpur), 1);
	TestEqual(TEXT("The spur's far end (not connected to anything) should still be a plain dead end"), CountJunctionsOfType(Junctions, EVoidRoadJunctionType::DeadEnd), 1);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
