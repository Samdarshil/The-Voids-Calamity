// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Misc/AutomationTest.h"
#include "VoidPackageValidators.h"
#include "VoidTestHelpers.h"
#include "HAL/PlatformTime.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace VoidTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidDataValidatorTest, "VOID.WorldBuilder.Validation.Data.InvalidCoordinatesAndReferences",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidDataValidatorTest::RunTest(const FString&)
{
	FVoidDesignPackage P = MakePackage();
	P.District.Roads.Add(MakeRoad(TEXT("ok"), { FVector2D(0, 0), FVector2D(1000, 0) }));
	P.District.Roads.Add(MakeRoad(TEXT("nan"), { FVector2D(0, 0), FVector2D(NAN, 0) }));
	P.District.Roads.Add(MakeRoad(TEXT("huge"), { FVector2D(0, 0), FVector2D(1.0e12, 0) }));
	P.District.Roads.Add(MakeRoad(TEXT("bad id!"), { FVector2D(0, 0), FVector2D(1000, 0) }));
	P.District.Buildings.Add(MakeBuilding(TEXT("b_nan"), { FVector2D(0, 0), FVector2D(10, 0), FVector2D(10, NAN) }, INFINITY));

	TSet<FName> Known; Known.Add(TEXT("some_other_district"));
	const FVoidValidationReport R = Run<FVoidPackageDataValidator>(P, FVoidValidationOptions(), Known);
	TestTrue(TEXT("NaN coordinate is an ERROR"), HasCodeAt(R, TEXT("VOID.Data.NonFiniteValue"), EVoidValidationSeverity::Error));
	TestTrue(TEXT("Absurd coordinate is an ERROR"), HasCodeAt(R, TEXT("VOID.Data.CoordinateOutOfRange"), EVoidValidationSeverity::Error));
	TestTrue(TEXT("Infinite height caught"), CountCode(R, TEXT("VOID.Data.NonFiniteValue")) >= 2);
	TestTrue(TEXT("Bad id syntax warned"), HasCodeAt(R, TEXT("VOID.Data.IdSyntax"), EVoidValidationSeverity::Warning));
	TestTrue(TEXT("Unknown district vs Meridian registry"), HasCodeAt(R, TEXT("VOID.Data.UnknownDistrict"), EVoidValidationSeverity::Error));
	TestFalse(TEXT("Report invalid"), R.bIsValid);
	for (const FVoidValidationIssue& I : R.Issues) { if (I.Severity == EVoidValidationSeverity::Error && I.SuggestedFix.IsEmpty()) { AddError(FString::Printf(TEXT("Error %s has no suggested fix"), *I.ErrorCode.ToString())); } }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidRoadGeometryTest, "VOID.WorldBuilder.Validation.Road.GeometryAndConnections",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidRoadGeometryTest::RunTest(const FString&)
{
	FVoidDesignPackage P = MakePackage();
	P.District.Roads.Add(MakeRoad(TEXT("zero"), { FVector2D(0, 0), FVector2D(0, 0) }));
	P.District.Roads.Add(MakeRoad(TEXT("dup_pts"), { FVector2D(0, 5000), FVector2D(1000, 5000), FVector2D(1000, 5000), FVector2D(2000, 5000) }));
	P.District.Roads.Add(MakeRoad(TEXT("hairpin"), { FVector2D(0, 8000), FVector2D(3000, 8000), FVector2D(0, 8100) }));
	P.District.Roads.Add(MakeRoad(TEXT("wide"), { FVector2D(0, 12000), FVector2D(3000, 12000) }, 90000.0f));
	FVoidRoadSpec Floating = MakeRoad(TEXT("floating"), { FVector2D(0, 16000), FVector2D(3000, 16000) });
	Floating.ElevationUnits = 900.0f;
	P.District.Roads.Add(Floating);
	FVoidRoadSpec SelfRef = MakeRoad(TEXT("selfref"), { FVector2D(0, 20000), FVector2D(3000, 20000) });
	SelfRef.ConnectionIds.Add(FVoidElementId(TEXT("selfref")));
	SelfRef.ConnectionIds.Add(FVoidElementId(TEXT("far_target")));
	SelfRef.ConnectionIds.Add(FVoidElementId(TEXT("far_target")));
	P.District.Roads.Add(SelfRef);
	P.District.Roads.Add(MakeRoad(TEXT("far_target"), { FVector2D(0, 60000), FVector2D(3000, 60000) }));
	FVoidRoadSpec NegLane = MakeRoad(TEXT("neg"), { FVector2D(0, 24000), FVector2D(3000, 24000) });
	NegLane.LaneCount = -2;
	P.District.Roads.Add(NegLane);

	const FVoidValidationReport R = Run<FVoidRoadNetworkValidator>(P);
	TestTrue(TEXT("Zero-length road is an ERROR"), HasCodeAt(R, TEXT("VOID.Road.ZeroLength"), EVoidValidationSeverity::Error));
	TestTrue(TEXT("Duplicate consecutive points warned"), HasCodeAt(R, TEXT("VOID.Road.DegenerateSegment"), EVoidValidationSeverity::Warning));
	TestTrue(TEXT("Hairpin warned"), HasCodeAt(R, TEXT("VOID.Road.SharpTurn"), EVoidValidationSeverity::Warning));
	TestTrue(TEXT("Implausible width warned"), HasCodeAt(R, TEXT("VOID.Road.WidthImplausible"), EVoidValidationSeverity::Warning));
	TestTrue(TEXT("Floating (elevated, no structure) warned"), HasCodeAt(R, TEXT("VOID.Road.ElevatedWithoutStructure"), EVoidValidationSeverity::Warning));
	TestTrue(TEXT("Self connection is an ERROR"), HasCodeAt(R, TEXT("VOID.Road.SelfConnection"), EVoidValidationSeverity::Error));
	TestTrue(TEXT("Duplicate connection warned"), HasCodeAt(R, TEXT("VOID.Road.DuplicateConnection"), EVoidValidationSeverity::Warning));
	TestTrue(TEXT("Connection to a road 40 km away warned"), HasCodeAt(R, TEXT("VOID.Road.ConnectionTooFar"), EVoidValidationSeverity::Warning));
	TestTrue(TEXT("Negative lane count is an ERROR"), HasCodeAt(R, TEXT("VOID.Road.InvalidLaneOrSpeed"), EVoidValidationSeverity::Error));
	TestTrue(TEXT("Isolated roads reported"), HasCode(R, TEXT("VOID.Road.IsolatedRoad")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidRoadTopologyTest, "VOID.WorldBuilder.Validation.Road.CrossingsJunctionsAndConnectivity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidRoadTopologyTest::RunTest(const FString&)
{
	{
		FVoidDesignPackage P = MakePackage();
		P.District.Roads.Add(MakeRoad(TEXT("ew"), { FVector2D(-5000, 0), FVector2D(5000, 0) }));
		P.District.Roads.Add(MakeRoad(TEXT("ns"), { FVector2D(0, -5000), FVector2D(0, 5000) }));
		const FVoidValidationReport R = Run<FVoidRoadNetworkValidator>(P);
		TestEqual(TEXT("An at-grade X crossing is reported exactly once"), CountCode(R, TEXT("VOID.Road.CrossingWithoutJunction")), 1);
		const FVoidValidationIssue* Issue = R.Issues.FindByPredicate([](const FVoidValidationIssue& I) { return I.ErrorCode == FName(TEXT("VOID.Road.CrossingWithoutJunction")); });
		TestTrue(TEXT("Crossing carries a location at the intersection"), Issue && Issue->bHasLocation && Issue->Location.Equals(FVector(0, 0, 0), 1.0));
	}
	{
		FVoidDesignPackage P = MakePackage();
		P.District.Roads.Add(MakeRoad(TEXT("ew"), { FVector2D(-5000, 0), FVector2D(5000, 0) }));
		FVoidRoadSpec Bridge = MakeRoad(TEXT("ns_bridge"), { FVector2D(0, -5000), FVector2D(0, 5000) });
		Bridge.bIsBridge = true;
		P.District.Roads.Add(Bridge);
		const FVoidValidationReport R = Run<FVoidRoadNetworkValidator>(P);
		TestFalse(TEXT("Grade-separated crossing is not flagged"), HasCode(R, TEXT("VOID.Road.CrossingWithoutJunction")));
		TestTrue(TEXT("...but is counted as Info"), HasCodeAt(R, TEXT("VOID.Road.GradeSeparatedCrossings"), EVoidValidationSeverity::Info));
	}
	{
		// A proper T: endpoint of "stem" lands mid-way on "bar" -> the generator's endpoint-only junction builder would miss it.
		FVoidDesignPackage P = MakePackage();
		P.District.Roads.Add(MakeRoad(TEXT("bar"), { FVector2D(-5000, 0), FVector2D(5000, 0) }));
		P.District.Roads.Add(MakeRoad(TEXT("stem"), { FVector2D(0, 5000), FVector2D(0, 10) }));
		const FVoidValidationReport R = Run<FVoidRoadNetworkValidator>(P);
		TestTrue(TEXT("Unsplit T-junction warned"), HasCodeAt(R, TEXT("VOID.Road.UnsplitTJunction"), EVoidValidationSeverity::Warning));
		TestFalse(TEXT("A T-junction is not a crossing"), HasCode(R, TEXT("VOID.Road.CrossingWithoutJunction")));
		TestFalse(TEXT("The T joins the roads, so no isolation warning"), HasCode(R, TEXT("VOID.Road.IsolatedRoad")));
	}
	{
		// Two roads sharing an endpoint within tolerance: a valid junction, one network.
		FVoidDesignPackage P = MakePackage();
		P.District.Roads.Add(MakeRoad(TEXT("a"), { FVector2D(0, 0), FVector2D(5000, 0) }));
		P.District.Roads.Add(MakeRoad(TEXT("b"), { FVector2D(5010, 5), FVector2D(5010, 5000) }));
		const FVoidValidationReport R = Run<FVoidRoadNetworkValidator>(P);
		TestFalse(TEXT("Endpoint junction within tolerance: connected"), HasCode(R, TEXT("VOID.Road.IsolatedRoad")) || HasCode(R, TEXT("VOID.Road.DisconnectedNetwork")));
	}
	{
		FVoidDesignPackage P = MakePackage();
		P.District.Roads.Add(MakeRoad(TEXT("a"), { FVector2D(0, 0), FVector2D(5000, 0) }));
		P.District.Roads.Add(MakeRoad(TEXT("b"), { FVector2D(5000, 0), FVector2D(10000, 0) }));
		P.District.Roads.Add(MakeRoad(TEXT("c"), { FVector2D(0, 20000), FVector2D(5000, 20000) }));
		P.District.Roads.Add(MakeRoad(TEXT("d"), { FVector2D(5000, 20000), FVector2D(10000, 20000) }));
		const FVoidValidationReport R = Run<FVoidRoadNetworkValidator>(P);
		TestTrue(TEXT("Two separate 2-road networks: disconnected"), HasCodeAt(R, TEXT("VOID.Road.DisconnectedNetwork"), EVoidValidationSeverity::Warning));
	}
	{
		FVoidDesignPackage P = MakePackage();
		P.District.Roads.Add(MakeRoad(TEXT("a"), { FVector2D(0, 0), FVector2D(5000, 0) }));
		P.District.Roads.Add(MakeRoad(TEXT("b"), { FVector2D(5000, 0), FVector2D(10000, 300) })); // continues onwards
		P.District.Roads.Add(MakeRoad(TEXT("c"), { FVector2D(5000, 0), FVector2D(0, 100) }));      // folds back along "a", leaving the shared junction ~1.1 degrees from a's direction
		const FVoidValidationReport R = Run<FVoidRoadNetworkValidator>(P);
		TestTrue(TEXT("Two roads leaving one junction at a tiny angle overlap"), HasCodeAt(R, TEXT("VOID.Road.AcuteJunction"), EVoidValidationSeverity::Warning));
	}
	{
		FVoidDesignPackage P = MakePackage();
		const FVoidValidationReport R = Run<FVoidRoadNetworkValidator>(P);
		TestTrue(TEXT("Empty road list explains why the generator will 'fail'"), HasCodeAt(R, TEXT("VOID.Road.NoRoads"), EVoidValidationSeverity::Warning));
	}
	{
		// Phase 3 validator is re-run early: bridge+tunnel and unknown connectionId surface at PostImport.
		FVoidDesignPackage P = MakePackage();
		FVoidRoadSpec Both = MakeRoad(TEXT("both"), { FVector2D(0, 0), FVector2D(1000, 0) });
		Both.bIsBridge = true; Both.bIsTunnel = true;
		Both.ConnectionIds.Add(FVoidElementId(TEXT("ghost")));
		P.District.Roads.Add(Both);
		const FVoidValidationReport R = Run<FVoidRoadNetworkValidator>(P);
		TestTrue(TEXT("Bridge+tunnel conflict (Phase 3 rule) surfaces early"), HasCodeAt(R, TEXT("VOID.RoadGen.BridgeTunnelConflict"), EVoidValidationSeverity::Error));
		TestTrue(TEXT("Unknown connectionId (Phase 3 rule) surfaces early"), HasCodeAt(R, TEXT("VOID.RoadGen.UnresolvableConnection"), EVoidValidationSeverity::Error));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidBuildingHookTest, "VOID.WorldBuilder.Validation.Building.FootprintsOverlapsAndRoadConflicts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidBuildingHookTest::RunTest(const FString&)
{
	FVoidDesignPackage P = MakePackage();
	P.District.Roads.Add(MakeRoad(TEXT("main"), { FVector2D(-20000, 0), FVector2D(20000, 0) }, 1000.0f)); // half-width 500

	P.District.Buildings.Add(MakeBuilding(TEXT("fine"),      { FVector2D(0, 2000), FVector2D(1000, 2000), FVector2D(1000, 3000), FVector2D(0, 3000) }));
	P.District.Buildings.Add(MakeBuilding(TEXT("on_road"),   { FVector2D(4000, -300), FVector2D(5000, -300), FVector2D(5000, 300), FVector2D(4000, 300) }));
	P.District.Buildings.Add(MakeBuilding(TEXT("encroach"),  { FVector2D(8000, 200), FVector2D(9000, 200), FVector2D(9000, 1500), FVector2D(8000, 1500) })); // centroid off-road, corners inside surface
	P.District.Buildings.Add(MakeBuilding(TEXT("bowtie"),    { FVector2D(0, 6000), FVector2D(1000, 7000), FVector2D(1000, 6000), FVector2D(0, 7500) }));
	P.District.Buildings.Add(MakeBuilding(TEXT("flat"),      { FVector2D(0, 9000), FVector2D(500, 9000), FVector2D(1000, 9000) }));
	P.District.Buildings.Add(MakeBuilding(TEXT("clockwise"), { FVector2D(0, 12000), FVector2D(0, 13000), FVector2D(1000, 13000), FVector2D(1000, 12000) }));
	P.District.Buildings.Add(MakeBuilding(TEXT("ov_a"),      { FVector2D(0, 15000), FVector2D(2000, 15000), FVector2D(2000, 17000), FVector2D(0, 17000) }));
	P.District.Buildings.Add(MakeBuilding(TEXT("ov_b"),      { FVector2D(1000, 16000), FVector2D(3000, 16000), FVector2D(3000, 18000), FVector2D(1000, 18000) }));
	P.District.Buildings.Add(MakeBuilding(TEXT("tall"),      { FVector2D(0, 20000), FVector2D(1000, 20000), FVector2D(1000, 21000), FVector2D(0, 21000) }, 9.0e6f));

	const FVoidValidationReport R = Run<FVoidBuildingHookValidator>(P);
	auto IssueFor = [&R](const TCHAR* Code, const TCHAR* Object) { return R.Issues.ContainsByPredicate([=](const FVoidValidationIssue& I) { return I.ErrorCode == FName(Code) && I.ObjectId == Object; }); };

	TestTrue(TEXT("Centre on road => ERROR"), IssueFor(TEXT("VOID.Building.OnRoad"), TEXT("on_road")));
	TestTrue(TEXT("Corner inside road surface => WARNING"), IssueFor(TEXT("VOID.Building.EncroachesRoad"), TEXT("encroach")));
	TestTrue(TEXT("Bow-tie footprint (non-zero net area, so not "degenerate") => ERROR"), IssueFor(TEXT("VOID.Building.FootprintSelfIntersects"), TEXT("bowtie")));
	TestTrue(TEXT("Collinear footprint => ERROR"), IssueFor(TEXT("VOID.Building.FootprintDegenerate"), TEXT("flat")));
	TestTrue(TEXT("Clockwise winding => WARNING"), IssueFor(TEXT("VOID.Building.FootprintWinding"), TEXT("clockwise")));
	TestTrue(TEXT("Overlapping footprints reported"), IssueFor(TEXT("VOID.Building.OverlapsBuilding"), TEXT("ov_a")));
	TestTrue(TEXT("Absurd height warned"), IssueFor(TEXT("VOID.Building.HeightImplausible"), TEXT("tall")));
	TestFalse(TEXT("Well-placed building is clean"), R.Issues.ContainsByPredicate([](const FVoidValidationIssue& I) { return I.ObjectId == TEXT("fine"); }));
	TestFalse(TEXT("Bridges/tunnels do not conflict with buildings"), [&]()
	{
		FVoidDesignPackage Q = MakePackage();
		FVoidRoadSpec Bridge = MakeRoad(TEXT("br"), { FVector2D(-5000, 0), FVector2D(5000, 0) }, 1000.0f);
		Bridge.bIsBridge = true;
		Q.District.Roads.Add(Bridge);
		Q.District.Buildings.Add(MakeBuilding(TEXT("under"), { FVector2D(0, -300), FVector2D(1000, -300), FVector2D(1000, 300), FVector2D(0, 300) }));
		return HasCode(Run<FVoidBuildingHookValidator>(Q), TEXT("VOID.Building.OnRoad"));
	}());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidValidationScaleTest, "VOID.WorldBuilder.Validation.Performance.GridOfSeveralThousandRoads",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidValidationScaleTest::RunTest(const FString&)
{
	// A 70x70 street grid: ~9,700 roads and ~4,800 buildings. Every road meets its neighbours at shared endpoints.
	// Correct spatial indexing means: zero false crossings, one connected network, and it finishes quickly.
	// (An all-pairs implementation would do ~10^8 segment tests here.)
	constexpr int32 N = 70;
	constexpr double Step = 4000.0;
	FVoidDesignPackage P = MakePackage();
	for (int32 y = 0; y < N; ++y)
	{
		for (int32 x = 0; x < N; ++x)
		{
			if (x + 1 < N) { P.District.Roads.Add(MakeRoad(*FString::Printf(TEXT("h_%d_%d"), x, y), { FVector2D(x * Step, y * Step), FVector2D((x + 1) * Step, y * Step) })); }
			if (y + 1 < N) { P.District.Roads.Add(MakeRoad(*FString::Printf(TEXT("v_%d_%d"), x, y), { FVector2D(x * Step, y * Step), FVector2D(x * Step, (y + 1) * Step) })); }
			if (x + 1 < N && y + 1 < N)
			{
				const double X0 = x * Step + 1200.0, Y0 = y * Step + 1200.0;
				P.District.Buildings.Add(MakeBuilding(*FString::Printf(TEXT("b_%d_%d"), x, y), { FVector2D(X0, Y0), FVector2D(X0 + 1500, Y0), FVector2D(X0 + 1500, Y0 + 1500), FVector2D(X0, Y0 + 1500) }));
			}
		}
	}
	FVoidValidationOptions Options;
	Options.MaxIssuesPerCode = 50;

	const double T0 = FPlatformTime::Seconds();
	const FVoidValidationReport RoadReport = Run<FVoidRoadNetworkValidator>(P, Options);
	const FVoidValidationReport BuildingReport = Run<FVoidBuildingHookValidator>(P, Options);
	const FVoidValidationReport DataReport = Run<FVoidPackageDataValidator>(P, Options);
	const double Seconds = FPlatformTime::Seconds() - T0;
	AddInfo(FString::Printf(TEXT("Validated %d roads + %d buildings in %.2f s"), P.District.Roads.Num(), P.District.Buildings.Num(), Seconds));

	TestEqual(TEXT("No false crossings on a clean grid"), CountCode(RoadReport, TEXT("VOID.Road.CrossingWithoutJunction")), 0);
	TestFalse(TEXT("Grid is one connected network"), HasCode(RoadReport, TEXT("VOID.Road.DisconnectedNetwork")) || HasCode(RoadReport, TEXT("VOID.Road.IsolatedRoad")));
	TestEqual(TEXT("No false building/road conflicts"), CountCode(BuildingReport, TEXT("VOID.Building.OnRoad")) + CountCode(BuildingReport, TEXT("VOID.Building.EncroachesRoad")), 0);
	TestEqual(TEXT("No false building overlaps"), CountCode(BuildingReport, TEXT("VOID.Building.OverlapsBuilding")), 0);
	TestTrue(TEXT("Data validator clean"), DataReport.NumBlocking() == 0);
	TestTrue(TEXT("Completes well inside a generous budget (catches accidental O(N^2))"), Seconds < 30.0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
