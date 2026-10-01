// Copyright VOID Engineering Studio. Internal tool - not for shipping content.
// Agent 1 foundation tests: world space, SHA-256, road output contract.

#include "Misc/AutomationTest.h"
#include "Coordinates/VoidWorldSpace.h"
#include "Utilities/VoidSha256.h"
#include "Data/VoidRoadOutput.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidSha256KnownVectorsTest, "VOID.WorldBuilder.Core.Sha256KnownVectors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidSha256KnownVectorsTest::RunTest(const FString& Parameters)
{
	auto Hash = [](const char* Text) { return FVoidSha256::HashBytes(reinterpret_cast<const uint8*>(Text), FCStringAnsi::Strlen(Text)); };
	TestEqual(TEXT("empty"), Hash(""), FString(TEXT("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")));
	TestEqual(TEXT("abc"), Hash("abc"), FString(TEXT("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")));
	TestEqual(TEXT("two-block message"), Hash("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"), FString(TEXT("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidWorldSpaceSourceTest, "VOID.WorldBuilder.Core.WorldSpace.Source2D",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidWorldSpaceSourceTest::RunTest(const FString& Parameters)
{
	FVoidWorldSpaceConfig Cfg;
	Cfg.SourceUnitsToUU = 2.0;
	Cfg.WorldOriginOffsetUU = FVector(100.0, 200.0, 300.0);
	FVoidWorldSpace Space(Cfg);

	FVector Out;
	TestTrue(TEXT("valid point converts"), Space.TryFromSource2D(FVector2D(10.0, 20.0), 5.0, Out));
	TestTrue(TEXT("scale then offset"), Out.Equals(FVector(120.0, 240.0, 310.0), 1e-9));

	const FVector2D Back = Space.ToSource2D(Out);
	TestTrue(TEXT("round trip (XY)"), Back.Equals(FVector2D(10.0, 20.0), 1e-9));

	Cfg.Axis = EVoidSourceAxisConvention::FlipY;
	Cfg.SourceUnitsToUU = 1.0; Cfg.WorldOriginOffsetUU = FVector::ZeroVector;
	FVoidWorldSpace Flip(Cfg);
	TestTrue(TEXT("FlipY negates Y"), Flip.TryFromSource2D(FVector2D(3.0, 4.0), 0.0, Out) && Out.Equals(FVector(3.0, -4.0, 0.0)));
	TestTrue(TEXT("FlipY round trip"), Flip.ToSource2D(Out).Equals(FVector2D(3.0, 4.0), 1e-9));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidWorldSpaceRejectTest, "VOID.WorldBuilder.Core.WorldSpace.RejectsInvalid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidWorldSpaceRejectTest::RunTest(const FString& Parameters)
{
	FVoidWorldSpaceConfig Cfg; Cfg.MaxAbsCoordinateUU = 1000.0;
	FVoidWorldSpace Space(Cfg);
	FVector Out(7.0, 7.0, 7.0); FString Err;

	TestFalse(TEXT("NaN rejected"), Space.TryFromSource2D(FVector2D(NAN, 0.0), 0.0, Out, &Err));
	TestFalse(TEXT("Inf rejected"), Space.TryFromSource2D(FVector2D(0.0, INFINITY), 0.0, Out, &Err));
	TestFalse(TEXT("over limit rejected, not clamped"), Space.TryFromSource2D(FVector2D(1000.5, 0.0), 0.0, Out, &Err));
	TestFalse(TEXT("error text produced"), Err.IsEmpty());
	TestTrue(TEXT("output untouched on failure"), Out.Equals(FVector(7.0, 7.0, 7.0)));
	TestTrue(TEXT("exactly at limit accepted"), Space.TryFromSource2D(FVector2D(1000.0, -1000.0), 0.0, Out));
	TestFalse(TEXT("negative polar radius rejected"), Space.TryFromPolar(-1.0, 0.0, 0.0, Out));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidWorldSpacePolarTest, "VOID.WorldBuilder.Core.WorldSpace.Polar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidWorldSpacePolarTest::RunTest(const FString& Parameters)
{
	FVoidWorldSpace Space{FVoidWorldSpaceConfig()};
	FVector Out;
	TestTrue(TEXT("azimuth 0 -> +X"), Space.TryFromPolar(500.0, 0.0, 0.0, Out) && Out.Equals(FVector(500.0, 0.0, 0.0), 1e-6));
	TestTrue(TEXT("azimuth 90 -> +Y"), Space.TryFromPolar(500.0, 90.0, 12.0, Out) && Out.Equals(FVector(0.0, 500.0, 12.0), 1e-6));
	return true;
}

namespace VoidFoundationTestHelpers
{
	static FVoidRoadRecord MakeStraightRoad(FName Id, bool bSidewalk)
	{
		FVoidRoadRecord R;
		R.RoadId = Id; R.RoadType = EVoidRoadType::Primary; R.HierarchyTier = 1; R.WidthUU = 800.0;
		R.Centerline = { FVector(0, 0, 0), FVector(1000, 0, 0), FVector(2000, 0, 0) };
		R.bHasSidewalk = bSidewalk;
		if (bSidewalk) { R.SidewalkInnerOffsetUU = 425.0; R.SidewalkOuterOffsetUU = 575.0; }
		R.ServedDistrictIds = { FName(TEXT("olympus_spire")) };
		return R;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidRoadOutputQueryTest, "VOID.WorldBuilder.Core.RoadOutput.Queries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidRoadOutputQueryTest::RunTest(const FString& Parameters)
{
	using namespace VoidFoundationTestHelpers;
	FVoidRoadNetworkOutput Out;
	Out.AddRoad(MakeStraightRoad(TEXT("A"), true));

	FVoidRoadRecord B = MakeStraightRoad(TEXT("B"), false);
	B.RoadType = EVoidRoadType::Secondary; B.HierarchyTier = 2;
	B.Centerline = { FVector(1000, -500, 0), FVector(1000, 500, 0) };
	Out.AddRoad(B);

	FVoidIntersectionRecord I; I.IntersectionId = TEXT("X"); I.Location = FVector(1000, 0, 0); I.Kind = EVoidIntersectionKind::Crossing; I.RoadIds = { TEXT("A"), TEXT("B") };
	Out.AddIntersection(I);
	Out.RebuildConnectivity();

	TestEqual(TEXT("two roads"), Out.GetRoads().Num(), 2);
	TestEqual(TEXT("by tier"), Out.GetRoadsByTier(1).Num(), 1);
	TestEqual(TEXT("by type"), Out.GetRoadsByType(EVoidRoadType::Secondary).Num(), 1);
	TestEqual(TEXT("by district"), Out.GetRoadsServingDistrict(TEXT("olympus_spire")).Num(), 2);
	TestTrue(TEXT("connectivity A->B"), Out.FindRoad(TEXT("A"))->ConnectedRoadIds.Contains(FName(TEXT("B"))));
	TestEqual(TEXT("intersections for A"), Out.GetIntersectionsForRoad(TEXT("A")).Num(), 1);
	TestNotNull(TEXT("nearest intersection"), Out.FindNearestIntersection(FVector(1010, 5, 0), 50.0));
	TestNull(TEXT("no intersection outside radius"), Out.FindNearestIntersection(FVector(5000, 0, 0), 50.0));

	FVoidRoadSample S;
	TestTrue(TEXT("sample mid"), Out.SampleRoad(TEXT("A"), 1500.0, S));
	TestTrue(TEXT("sample position"), S.Position.Equals(FVector(1500, 0, 0), 1e-6));
	TestTrue(TEXT("sample direction +X"), S.Direction.Equals(FVector::ForwardVector, 1e-6));
	TestTrue(TEXT("clamps past end"), Out.SampleRoad(TEXT("A"), 99999.0, S) && S.Position.Equals(FVector(2000, 0, 0), 1e-6));
	TestFalse(TEXT("unknown road"), Out.SampleRoad(TEXT("nope"), 0.0, S));

	double Dist = 0.0;
	TestTrue(TEXT("nearest point"), Out.FindNearestRoadPoint(FVector(500, 300, 0), S, Dist));
	TestTrue(TEXT("nearest road is A"), S.RoadId == FName(TEXT("A")));
	TestTrue(TEXT("nearest distance"), FMath::IsNearlyEqual(Dist, 300.0, 1e-6));

	// Re-adding the same id replaces, never duplicates.
	Out.AddRoad(MakeStraightRoad(TEXT("A"), false));
	TestEqual(TEXT("still two roads"), Out.GetRoads().Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidRoadOutputSidewalkTest, "VOID.WorldBuilder.Core.RoadOutput.SidewalkBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidRoadOutputSidewalkTest::RunTest(const FString& Parameters)
{
	using namespace VoidFoundationTestHelpers;
	FVoidRoadNetworkOutput Out;
	Out.AddRoad(MakeStraightRoad(TEXT("A"), true));
	Out.AddRoad(MakeStraightRoad(TEXT("NoWalk"), false));

	TArray<FVector> Inner, Outer;
	TestTrue(TEXT("left boundary"), Out.GetSidewalkBoundary(TEXT("A"), EVoidRoadSide::Left, Inner, Outer));
	// Heading +X, Left is -Y in Unreal's left-handed frame.
	TestTrue(TEXT("left inner at -425"), FMath::IsNearlyEqual(Inner[0].Y, -425.0, 1e-6));
	TestTrue(TEXT("left outer at -575"), FMath::IsNearlyEqual(Outer[0].Y, -575.0, 1e-6));
	TestTrue(TEXT("right boundary"), Out.GetSidewalkBoundary(TEXT("A"), EVoidRoadSide::Right, Inner, Outer));
	TestTrue(TEXT("right inner at +425"), FMath::IsNearlyEqual(Inner[0].Y, 425.0, 1e-6));
	TestFalse(TEXT("no sidewalk -> no boundary (nothing fabricated)"), Out.GetSidewalkBoundary(TEXT("NoWalk"), EVoidRoadSide::Left, Inner, Outer));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidRoadOutputClosedLoopTest, "VOID.WorldBuilder.Core.RoadOutput.ClosedLoop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidRoadOutputClosedLoopTest::RunTest(const FString& Parameters)
{
	FVoidRoadRecord Ring;
	Ring.RoadId = TEXT("Ring"); Ring.bClosedLoop = true;
	Ring.Centerline = { FVector(100, 0, 0), FVector(0, 100, 0), FVector(-100, 0, 0), FVector(0, -100, 0) };
	FVoidRoadNetworkOutput Out; Out.AddRoad(Ring);

	const double Expected = 4.0 * FMath::Sqrt(2.0) * 100.0;
	TestTrue(TEXT("length includes closing segment"), FMath::IsNearlyEqual(Out.FindRoad(TEXT("Ring"))->GetLength(), Expected, 1e-6));

	FVoidRoadSample S;
	TestTrue(TEXT("wraps past the end"), Out.SampleRoad(TEXT("Ring"), Expected + 1.0, S));
	TestTrue(TEXT("wrapped position near start"), FVector::Dist(S.Position, FVector(100, 0, 0)) < 2.0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
