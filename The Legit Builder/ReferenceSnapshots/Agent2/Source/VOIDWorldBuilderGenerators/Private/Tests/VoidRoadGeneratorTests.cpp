// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Misc/AutomationTest.h"
#include "Road/VoidRoadGenerator.h"
#include "Road/VoidRoadActor.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace VoidRoadGeneratorTestPrivate
{
	static UWorld* GetTestWorld()
	{
		return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	}

	/**
	 * These tests are the only thing in the automation suite that spawns
	 * AVoidRoadActor/AVoidRoadJunctionActor, so unconditionally clearing
	 * every instance of both at the start and end of each test keeps the
	 * editor level clean regardless of test run order or a previous
	 * test failing before its own cleanup ran.
	 */
	static void DestroyAllVoidRoadActors(UWorld* World)
	{
		if (!World)
		{
			return;
		}

		TArray<AActor*> ToDestroy;
		for (TActorIterator<AVoidRoadActor> It(World); It; ++It) { ToDestroy.Add(*It); }
		for (TActorIterator<AVoidRoadJunctionActor> It(World); It; ++It) { ToDestroy.Add(*It); }

		for (AActor* Actor : ToDestroy)
		{
			if (IsValid(Actor))
			{
				Actor->Destroy();
			}
		}
	}

	static int32 CountActorsOfClass(UWorld* World, UClass* Class)
	{
		int32 Count = 0;
		for (TActorIterator<AActor> It(World, Class); It; ++It)
		{
			++Count;
		}
		return Count;
	}

	static FVoidRoadSpec MakeStraightRoad(const FString& Id)
	{
		FVoidRoadSpec Road;
		Road.Id = FVoidElementId(FName(*Id));
		Road.CenterlinePoints = { FVector2D(0, 0), FVector2D(1000, 0) };
		Road.WidthUnits = 600.0f;
		return Road;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidRoadGeneratorStraightAndCurvedTest,
	"VOID.WorldBuilder.RoadGenerator.Generate.StraightAndCurvedRoadsSpawnActorsWithMeshes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidRoadGeneratorStraightAndCurvedTest::RunTest(const FString& Parameters)
{
	using namespace VoidRoadGeneratorTestPrivate;

	UWorld* World = GetTestWorld();
	if (!TestNotNull(TEXT("Editor world should be available for this test"), World))
	{
		return false;
	}

	DestroyAllVoidRoadActors(World);

	FVoidDesignPackage Package;
	Package.District.DistrictId = FVoidElementId(FName(TEXT("test_district")));

	FVoidRoadSpec Straight = MakeStraightRoad(TEXT("straight_road"));
	Package.District.Roads.Add(Straight);

	FVoidRoadSpec Curved;
	Curved.Id = FVoidElementId(FName(TEXT("curved_road")));
	Curved.CenterlinePoints = { FVector2D(2000, 0), FVector2D(2300, 400), FVector2D(3000, 200), FVector2D(3400, 900) };
	Curved.WidthUnits = 600.0f;
	Package.District.Roads.Add(Curved);

	FVoidRoadGenerator Generator;
	FVoidGenerationContext Context;
	Context.TargetWorld = World;

	const bool bResult = Generator.Generate(Package, Context);

	TestTrue(TEXT("Generation should succeed with 2 valid roads"), bResult);
	TestEqual(TEXT("Exactly 2 road actors should be spawned"), CountActorsOfClass(World, AVoidRoadActor::StaticClass()), 2);

	for (TActorIterator<AVoidRoadActor> It(World); It; ++It)
	{
		TestTrue(TEXT("Every road actor's mesh should have at least a road-surface section"), It->RoadMesh->GetNumSections() > 0);
	}

	DestroyAllVoidRoadActors(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidRoadGeneratorBridgeTunnelTest,
	"VOID.WorldBuilder.RoadGenerator.Generate.BridgeAndTunnelGetStructureMarkers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidRoadGeneratorBridgeTunnelTest::RunTest(const FString& Parameters)
{
	using namespace VoidRoadGeneratorTestPrivate;

	UWorld* World = GetTestWorld();
	if (!TestNotNull(TEXT("Editor world should be available for this test"), World))
	{
		return false;
	}

	DestroyAllVoidRoadActors(World);

	FVoidDesignPackage Package;
	Package.District.DistrictId = FVoidElementId(FName(TEXT("test_district")));

	FVoidRoadSpec Bridge;
	Bridge.Id = FVoidElementId(FName(TEXT("bridge_road")));
	Bridge.CenterlinePoints = { FVector2D(0, 0), FVector2D(1000, 0), FVector2D(2000, 0), FVector2D(3000, 0) };
	Bridge.WidthUnits = 600.0f;
	Bridge.bIsBridge = true;
	Package.District.Roads.Add(Bridge);

	FVoidRoadSpec Tunnel;
	Tunnel.Id = FVoidElementId(FName(TEXT("tunnel_road")));
	Tunnel.CenterlinePoints = { FVector2D(0, 5000), FVector2D(1000, 5000), FVector2D(2000, 5000) };
	Tunnel.WidthUnits = 600.0f;
	Tunnel.bIsTunnel = true;
	Package.District.Roads.Add(Tunnel);

	FVoidRoadGenerator Generator;
	FVoidGenerationContext Context;
	Context.TargetWorld = World;

	const bool bResult = Generator.Generate(Package, Context);
	TestTrue(TEXT("Generation should succeed"), bResult);

	bool bFoundBridgeMarkers = false;
	bool bFoundTunnelMarkers = false;

	for (TActorIterator<AVoidRoadActor> It(World); It; ++It)
	{
		if (It->RoadId.Value == FName(TEXT("bridge_road")))
		{
			bFoundBridgeMarkers = It->StructureMarkers->GetInstanceCount() > 0;
		}
		else if (It->RoadId.Value == FName(TEXT("tunnel_road")))
		{
			bFoundTunnelMarkers = (It->StructureMarkers->GetInstanceCount() == 2); // exactly 2 portals
		}
	}

	TestTrue(TEXT("Bridge road should have at least one pier instance"), bFoundBridgeMarkers);
	TestTrue(TEXT("Tunnel road should have exactly 2 portal instances"), bFoundTunnelMarkers);

	DestroyAllVoidRoadActors(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidRoadGeneratorJunctionTest,
	"VOID.WorldBuilder.RoadGenerator.Generate.TJunctionSpawnsJunctionActor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidRoadGeneratorJunctionTest::RunTest(const FString& Parameters)
{
	using namespace VoidRoadGeneratorTestPrivate;

	UWorld* World = GetTestWorld();
	if (!TestNotNull(TEXT("Editor world should be available for this test"), World))
	{
		return false;
	}

	DestroyAllVoidRoadActors(World);

	FVoidDesignPackage Package;
	Package.District.DistrictId = FVoidElementId(FName(TEXT("test_district")));

	FVoidRoadSpec RoadA;
	RoadA.Id = FVoidElementId(FName(TEXT("road_a")));
	RoadA.CenterlinePoints = { FVector2D(0, 0), FVector2D(1000, 1000) };
	RoadA.WidthUnits = 600.0f;
	Package.District.Roads.Add(RoadA);

	FVoidRoadSpec RoadB;
	RoadB.Id = FVoidElementId(FName(TEXT("road_b")));
	RoadB.CenterlinePoints = { FVector2D(2000, 1000), FVector2D(1000, 1000) };
	RoadB.WidthUnits = 600.0f;
	Package.District.Roads.Add(RoadB);

	FVoidRoadSpec RoadC;
	RoadC.Id = FVoidElementId(FName(TEXT("road_c")));
	RoadC.CenterlinePoints = { FVector2D(1000, 1000), FVector2D(1000, 2000) };
	RoadC.WidthUnits = 600.0f;
	Package.District.Roads.Add(RoadC);

	FVoidRoadGenerator Generator;
	FVoidGenerationContext Context;
	Context.TargetWorld = World;

	Generator.Generate(Package, Context);

	TestTrue(TEXT("At least one junction actor should be spawned for the T-junction"), CountActorsOfClass(World, AVoidRoadJunctionActor::StaticClass()) > 0);

	DestroyAllVoidRoadActors(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidRoadGeneratorEmptyDistrictTest,
	"VOID.WorldBuilder.RoadGenerator.Generate.EmptyDistrictFailsGracefully",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidRoadGeneratorEmptyDistrictTest::RunTest(const FString& Parameters)
{
	using namespace VoidRoadGeneratorTestPrivate;

	UWorld* World = GetTestWorld();
	if (!TestNotNull(TEXT("Editor world should be available for this test"), World))
	{
		return false;
	}

	DestroyAllVoidRoadActors(World);

	FVoidDesignPackage Package;
	Package.District.DistrictId = FVoidElementId(FName(TEXT("empty_district")));
	// Zero roads.

	FVoidRoadGenerator Generator;
	FVoidGenerationContext Context;
	Context.TargetWorld = World;

	const bool bResult = Generator.Generate(Package, Context);

	TestFalse(TEXT("An empty district should report false (nothing built), not crash"), bResult);
	TestEqual(TEXT("No road actors should be spawned"), CountActorsOfClass(World, AVoidRoadActor::StaticClass()), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidRoadGeneratorDuplicateIdDefensiveTest,
	"VOID.WorldBuilder.RoadGenerator.Generate.DuplicateRoadIdsDoNotCrash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidRoadGeneratorDuplicateIdDefensiveTest::RunTest(const FString& Parameters)
{
	using namespace VoidRoadGeneratorTestPrivate;

	// Phase 2's import-time validation already rejects duplicate ids
	// before a generator ever sees a package (see
	// VOID.WorldBuilder.Import.DuplicateIdsAreRejected). This test exists
	// because FVoidRoadGenerator can, in principle, be called directly
	// (e.g. by a future automation/CI tool bypassing import) and must not
	// crash even if that upstream guarantee is somehow violated -- it
	// should just do something sensible with duplicate ids, not assert
	// or corrupt memory.

	UWorld* World = GetTestWorld();
	if (!TestNotNull(TEXT("Editor world should be available for this test"), World))
	{
		return false;
	}

	DestroyAllVoidRoadActors(World);

	FVoidDesignPackage Package;
	Package.District.DistrictId = FVoidElementId(FName(TEXT("test_district")));
	Package.District.Roads.Add(MakeStraightRoad(TEXT("duplicate_id")));

	FVoidRoadSpec SecondRoad = MakeStraightRoad(TEXT("duplicate_id"));
	SecondRoad.CenterlinePoints = { FVector2D(5000, 5000), FVector2D(6000, 5000) };
	Package.District.Roads.Add(SecondRoad);

	FVoidRoadGenerator Generator;
	FVoidGenerationContext Context;
	Context.TargetWorld = World;

	const bool bResult = Generator.Generate(Package, Context);

	TestTrue(TEXT("Generation with duplicate ids should still complete without crashing"), bResult);
	TestEqual(TEXT("Both roads should still spawn actors even sharing an id"), CountActorsOfClass(World, AVoidRoadActor::StaticClass()), 2);

	DestroyAllVoidRoadActors(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidRoadGeneratorCancellationTest,
	"VOID.WorldBuilder.RoadGenerator.Generate.CancellationStopsEarly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidRoadGeneratorCancellationTest::RunTest(const FString& Parameters)
{
	using namespace VoidRoadGeneratorTestPrivate;

	UWorld* World = GetTestWorld();
	if (!TestNotNull(TEXT("Editor world should be available for this test"), World))
	{
		return false;
	}

	DestroyAllVoidRoadActors(World);

	FVoidDesignPackage Package;
	Package.District.DistrictId = FVoidElementId(FName(TEXT("test_district")));
	for (int32 Index = 0; Index < 10; ++Index)
	{
		FVoidRoadSpec Road = MakeStraightRoad(FString::Printf(TEXT("road_%d"), Index));
		Road.CenterlinePoints = { FVector2D(Index * 2000.0f, 0), FVector2D(Index * 2000.0f + 1000.0f, 0) };
		Package.District.Roads.Add(Road);
	}

	FVoidRoadGenerator Generator;
	FVoidGenerationContext Context;
	Context.TargetWorld = World;

	int32 CallCount = 0;
	Context.IsCancellationRequested = [&CallCount]()
	{
		++CallCount;
		return CallCount > 3; // cancel after the 3rd road
	};

	Generator.Generate(Package, Context);

	const int32 SpawnedCount = CountActorsOfClass(World, AVoidRoadActor::StaticClass());
	TestTrue(TEXT("Cancellation should stop generation before all 10 roads are built"), SpawnedCount < 10);
	TestTrue(TEXT("Cancellation should still have built at least one road before stopping"), SpawnedCount > 0);

	DestroyAllVoidRoadActors(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidRoadGeneratorLargeNetworkPerformanceTest,
	"VOID.WorldBuilder.RoadGenerator.Generate.LargeNetworkGeneratesWithinBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::PerfFilter)

bool FVoidRoadGeneratorLargeNetworkPerformanceTest::RunTest(const FString& Parameters)
{
	using namespace VoidRoadGeneratorTestPrivate;

	UWorld* World = GetTestWorld();
	if (!TestNotNull(TEXT("Editor world should be available for this test"), World))
	{
		return false;
	}

	DestroyAllVoidRoadActors(World);

	// A 10x10 grid of connected roads: 100 roads, ~121 four-way/edge junctions.
	FVoidDesignPackage Package;
	Package.District.DistrictId = FVoidElementId(FName(TEXT("perf_district")));

	constexpr int32 GridSize = 10;
	constexpr float Spacing = 1000.0f;

	for (int32 Row = 0; Row < GridSize; ++Row)
	{
		FVoidRoadSpec HorizontalRoad;
		HorizontalRoad.Id = FVoidElementId(FName(*FString::Printf(TEXT("h_road_%d"), Row)));
		HorizontalRoad.WidthUnits = 600.0f;
		for (int32 Col = 0; Col < GridSize; ++Col)
		{
			HorizontalRoad.CenterlinePoints.Add(FVector2D(Col * Spacing, Row * Spacing));
		}
		Package.District.Roads.Add(HorizontalRoad);
	}

	for (int32 Col = 0; Col < GridSize; ++Col)
	{
		FVoidRoadSpec VerticalRoad;
		VerticalRoad.Id = FVoidElementId(FName(*FString::Printf(TEXT("v_road_%d"), Col)));
		VerticalRoad.WidthUnits = 600.0f;
		for (int32 Row = 0; Row < GridSize; ++Row)
		{
			VerticalRoad.CenterlinePoints.Add(FVector2D(Col * Spacing, Row * Spacing));
		}
		Package.District.Roads.Add(VerticalRoad);
	}

	FVoidRoadGenerator Generator;
	FVoidGenerationContext Context;
	Context.TargetWorld = World;

	const double StartSeconds = FPlatformTime::Seconds();
	const bool bResult = Generator.Generate(Package, Context);
	const double ElapsedMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;

	TestTrue(TEXT("A 20-road grid network should generate successfully"), bResult);
	TestEqual(TEXT("All 20 roads (10 horizontal + 10 vertical) should spawn"), CountActorsOfClass(World, AVoidRoadActor::StaticClass()), GridSize * 2);

	// Generous budget, same rationale as Phase 2's import perf smoke test:
	// a regression guard against accidental O(n^2)+ blowups, not a
	// real-world performance characterization.
	TestTrue(FString::Printf(TEXT("Generation should complete within 5000ms (took %.2fms)"), ElapsedMs), ElapsedMs < 5000.0);

	DestroyAllVoidRoadActors(World);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
