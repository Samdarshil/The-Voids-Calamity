// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Misc/AutomationTest.h"
#include "Road/VoidMeridianRoadPlanner.h"
#include "VoidGeneratorPipeline.h"
#include "VoidGeneratorRegistry.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace VoidPlannerTestHelpers
{
	/** Mirrors the REAL RoadNetwork.json structure (ids, categories, bands, dependencies). */
	static FVoidMeridianWorld MakeMeridianLikeWorld()
	{
		FVoidMeridianWorld W;
		W.Manifest.RadialBands = { TEXT("core"), TEXT("inner_rings_unbuilt"), TEXT("mid_tier_rings"), TEXT("seam_zone"), TEXT("outer_rings"), TEXT("off_gradient") };

		auto AddDistrict = [&](const TCHAR* Id, const TCHAR* Band)
		{
			FVoidMeridianDistrict D; D.Id = Id; D.RadialBand = Band; W.Districts.Add(D);
		};
		AddDistrict(TEXT("olympus_spire"), TEXT("core"));
		AddDistrict(TEXT("white_zones"), TEXT("mid_tier_rings"));
		AddDistrict(TEXT("metro_archives"), TEXT("seam_zone"));
		AddDistrict(TEXT("undercroft"), TEXT("inner_mid_outer_continuous"));
		AddDistrict(TEXT("sector_0"), TEXT("off_gradient"));

		FVoidMeridianRoadNetwork& RN = W.RoadNetwork;
		RN.Categories = {
			{ TEXT("radial_arterial"), 1, EVoidNetworkKind::Live },
			{ TEXT("ring_road"), 2, EVoidNetworkKind::Live },
			{ TEXT("service_maintenance_route"), 3, EVoidNetworkKind::Dead } };

		FVoidMeridianRoute Spine;
		Spine.Id = TEXT("spire_radial_spine"); Spine.RouteClass = EVoidMeridianRouteClass::Primary; Spine.CategoryId = TEXT("radial_arterial");
		Spine.HierarchyTier = 1; Spine.Network = EVoidNetworkKind::Live;
		Spine.Bands = { TEXT("core"), TEXT("inner_rings_unbuilt"), TEXT("mid_tier_rings") };
		Spine.ServedDistricts = { TEXT("olympus_spire"), TEXT("white_zones") };
		RN.Routes.Add(Spine);

		FVoidMeridianRoute Ring;
		Ring.Id = TEXT("mid_tier_ring_road"); Ring.RouteClass = EVoidMeridianRouteClass::Secondary; Ring.CategoryId = TEXT("ring_road");
		Ring.HierarchyTier = 2; Ring.Network = EVoidNetworkKind::Live; Ring.Bands = { TEXT("mid_tier_rings") };
		Ring.GenerationDependency = TEXT("spire_radial_spine");
		Ring.HardConstraint = TEXT("must_never_route_through_metro_archives_or_undercroft");
		Ring.ServedDistricts = { TEXT("white_zones") };
		RN.Routes.Add(Ring);

		FVoidMeridianRoute Service;
		Service.Id = TEXT("undercroft_maintenance_layer"); Service.RouteClass = EVoidMeridianRouteClass::Service; Service.CategoryId = TEXT("service_maintenance_route");
		Service.HierarchyTier = 3; Service.Network = EVoidNetworkKind::Dead;
		RN.Routes.Add(Service);

		FVoidMeridianTunnel T; T.Id = TEXT("undercroft_metro_archives_transit_seam"); T.Connects = { TEXT("undercroft"), TEXT("metro_archives") };
		RN.Tunnels.Add(T);
		FVoidMeridianBridge B; B.Id = TEXT("spire_tower_skybridges"); B.bFlaggedForSignoff = true;
		RN.Bridges.Add(B);
		return W;
	}

	static FVoidMeridianLayout MakeLayout()
	{
		FVoidMeridianLayout L;
		L.BandRadii = { FVoidBandRadius(TEXT("core"), 0, 1000), FVoidBandRadius(TEXT("inner_rings_unbuilt"), 1000, 5000),
			FVoidBandRadius(TEXT("mid_tier_rings"), 5000, 9000), FVoidBandRadius(TEXT("seam_zone"), 9000, 10000) };
		L.RingRoadSegments = 32; L.RadialSampleSpacingUU = 1000.0; L.DefaultRadialAzimuthDegrees = 0.0;
		return L;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidPlannerMeridianTest, "VOID.WorldBuilder.Generators.MeridianRoadPlanner.PlansOnlyWhatMeridianStates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidPlannerMeridianTest::RunTest(const FString& Parameters)
{
	using namespace VoidPlannerTestHelpers;
	const FVoidMeridianWorld World = MakeMeridianLikeWorld();
	FVoidMeridianRoadPlan Plan; FVoidValidationReport Report; Report.bIsValid = true;
	FVoidWorldSpace Space{FVoidWorldSpaceConfig()};

	TestTrue(TEXT("plan builds"), FVoidMeridianRoadPlanner::BuildPlan(World, Space, MakeLayout(), Plan, Report));
	TestEqual(TEXT("exactly the two Live routes are planned"), Plan.Roads.Num(), 2);
	TestTrue(TEXT("Dead Network route + tunnel are topology only"), Plan.TopologyOnly.Num() == 2);
	TestTrue(TEXT("report stays valid"), Report.bIsValid);

	const FVoidPlannedRoad* Spine = Plan.Roads.FindByPredicate([](const FVoidPlannedRoad& R) { return R.Spec.Id.Value == FName(TEXT("spire_radial_spine")); });
	const FVoidPlannedRoad* Ring = Plan.Roads.FindByPredicate([](const FVoidPlannedRoad& R) { return R.Spec.Id.Value == FName(TEXT("mid_tier_ring_road")); });
	if (!TestNotNull(TEXT("spine"), Spine) || !TestNotNull(TEXT("ring"), Ring)) { return false; }

	TestTrue(TEXT("spine tier 1 -> Primary"), Spine->Spec.RoadType == EVoidRoadType::Primary);
	TestTrue(TEXT("ring is tier 2 (below the spine)"), Spine->HierarchyTier == 1 && Ring->HierarchyTier == 2);
	TestTrue(TEXT("spine runs from the Spire origin"), FVector2D(Spine->Spec.CenterlinePoints[0]).IsNearlyZero());
	TestTrue(TEXT("spine ends at outer edge of its last band"), FMath::IsNearlyEqual(Spine->Spec.CenterlinePoints.Last().X, 9000.0, 1e-6));
	TestTrue(TEXT("ring is a closed loop"), Ring->Spec.bClosedLoop && Ring->Spec.CenterlinePoints.Num() == 32);
	for (const FVector2D& P : Ring->Spec.CenterlinePoints) { TestTrue(TEXT("ring vertex on the band-midpoint circle (7000)"), FMath::IsNearlyEqual(P.Size(), 7000.0, 1e-6)); }

	TestTrue(TEXT("nothing is fabricated: no widths, sidewalks, medians"), Spine->Spec.WidthUnits == 0.0f && !Spine->Spec.bHasSidewalk && !Spine->Spec.bHasMedian && !Ring->Spec.bHasSidewalk);

	TestEqual(TEXT("one spine x ring crossing"), Plan.Crossings.Num(), 1);
	if (Plan.Crossings.Num() == 1)
	{
		TestTrue(TEXT("crossing at ring radius on the spine azimuth"), Plan.Crossings[0].Location.Equals(FVector(7000.0, 0.0, 0.0), 1e-6));
		TestTrue(TEXT("crossing lies exactly on a ring vertex"), Ring->Spec.CenterlinePoints.ContainsByPredicate([](const FVector2D& P) { return P.Equals(FVector2D(7000.0, 0.0), 1e-6); }));
	}
	TestEqual(TEXT("flagged skybridge is skipped, not built"), Plan.SkippedFlagged.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidPlannerFailureTest, "VOID.WorldBuilder.Generators.MeridianRoadPlanner.ReportsProblemsWithoutInventing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidPlannerFailureTest::RunTest(const FString& Parameters)
{
	using namespace VoidPlannerTestHelpers;
	FVoidMeridianWorld World = MakeMeridianLikeWorld();
	FVoidWorldSpace Space{FVoidWorldSpaceConfig()};

	// Missing band radius -> route skipped with an error, other route still planned.
	{
		FVoidMeridianLayout L = MakeLayout();
		L.BandRadii.RemoveAll([](const FVoidBandRadius& B) { return B.BandId == FName(TEXT("inner_rings_unbuilt")); });
		FVoidMeridianRoadPlan Plan; FVoidValidationReport R; R.bIsValid = true;
		FVoidMeridianRoadPlanner::BuildPlan(World, Space, L, Plan, R);
		TestFalse(TEXT("missing radius is an error"), R.bIsValid);
		TestEqual(TEXT("ring still planned, spine skipped"), Plan.Roads.Num(), 1);
	}
	// Coordinates beyond the limit are rejected, never clamped.
	{
		FVoidWorldSpaceConfig Tight; Tight.MaxAbsCoordinateUU = 2000.0;
		FVoidMeridianRoadPlan Plan; FVoidValidationReport R; R.bIsValid = true;
		FVoidMeridianRoadPlanner::BuildPlan(World, FVoidWorldSpace(Tight), MakeLayout(), Plan, R);
		TestEqual(TEXT("everything out of range -> nothing planned"), Plan.Roads.Num(), 0);
		TestFalse(TEXT("reported as error"), R.bIsValid);
	}
	// Ring placed in a band of a district its hard constraint forbids.
	{
		FVoidMeridianWorld Bad = World;
		for (FVoidMeridianRoute& Rt : Bad.RoadNetwork.Routes) { if (Rt.Id == FName(TEXT("mid_tier_ring_road"))) { Rt.Bands = { TEXT("seam_zone") }; } }
		FVoidMeridianRoadPlan Plan; FVoidValidationReport R; R.bIsValid = true;
		FVoidMeridianRoadPlanner::BuildPlan(Bad, Space, MakeLayout(), Plan, R);
		TestFalse(TEXT("hard constraint violation is an error"), R.bIsValid);
		TestFalse(TEXT("violating ring not planned"), Plan.Roads.ContainsByPredicate([](const FVoidPlannedRoad& P) { return P.bIsRing; }));
	}
	return true;
}

namespace VoidPipelineTestHelpers
{
	struct FFakeGen : IVoidGenerator
	{
		FName Id; TArray<FName> Deps; bool bResult = true; TArray<FName>* Log = nullptr;
		virtual FName GetGeneratorId() const override { return Id; }
		virtual TArray<FName> GetDependencies() const override { return Deps; }
		virtual bool Generate(const FVoidDesignPackage&, FVoidGenerationContext&) override { if (Log) { Log->Add(Id); } return bResult; }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidPipelineOrderTest, "VOID.WorldBuilder.Generators.Pipeline.OrdersAndSkipsDependents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidPipelineOrderTest::RunTest(const FString& Parameters)
{
	using namespace VoidPipelineTestHelpers;
	FVoidGeneratorRegistry& Reg = FVoidGeneratorRegistry::Get();
	TArray<FName> Ran;

	auto Make = [&](const TCHAR* Id, TArray<FName> Deps, bool bOk)
	{
		TSharedRef<FFakeGen> G = MakeShared<FFakeGen>(); G->Id = Id; G->Deps = Deps; G->bResult = bOk; G->Log = &Ran; Reg.RegisterGenerator(G); return G;
	};
	Make(TEXT("TestC"), { TEXT("TestB") }, true);
	Make(TEXT("TestB"), { TEXT("TestA") }, true);
	Make(TEXT("TestA"), {}, true);

	FVoidGenerationContext Ctx;
	FVoidDesignPackage Pkg;
	TestTrue(TEXT("run succeeds"), FVoidGeneratorPipeline::RunWithPackage({ TEXT("TestC") }, Pkg, Ctx));
	TestTrue(TEXT("dependencies auto-included and ordered A,B,C"), Ran == TArray<FName>({ TEXT("TestA"), TEXT("TestB"), TEXT("TestC") }));

	// Failing dependency skips dependents.
	Reg.UnregisterGenerator(TEXT("TestA"));
	Make(TEXT("TestA"), {}, false);
	Ran.Reset(); FVoidGenerationContext Ctx2;
	TestFalse(TEXT("overall failure"), FVoidGeneratorPipeline::RunWithPackage({ TEXT("TestC") }, Pkg, Ctx2));
	TestTrue(TEXT("only A ran"), Ran == TArray<FName>({ TEXT("TestA") }));
	TestTrue(TEXT("B skipped"), Ctx2.Results.FindRef(TEXT("TestB")).Status == EVoidGeneratorStatus::Skipped);
	TestTrue(TEXT("C skipped"), Ctx2.Results.FindRef(TEXT("TestC")).Status == EVoidGeneratorStatus::Skipped);

	// Cycle and unknown id are errors; nothing runs.
	Reg.UnregisterGenerator(TEXT("TestA"));
	Make(TEXT("TestA"), { TEXT("TestC") }, true);
	Ran.Reset(); FVoidGenerationContext Ctx3;
	TestFalse(TEXT("cycle rejected"), FVoidGeneratorPipeline::RunWithPackage({ TEXT("TestC") }, Pkg, Ctx3));
	TestEqual(TEXT("cycle ran nothing"), Ran.Num(), 0);
	TestFalse(TEXT("unknown id rejected"), FVoidGeneratorPipeline::RunWithPackage({ TEXT("DoesNotExist") }, Pkg, Ctx3));

	Reg.UnregisterGenerator(TEXT("TestA")); Reg.UnregisterGenerator(TEXT("TestB")); Reg.UnregisterGenerator(TEXT("TestC"));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
