// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Misc/AutomationTest.h"
#include "District/VoidDistrictGeometry.h"
#include "District/VoidDistrictLayoutBuilder.h"
#include "District/VoidDistrictValidator.h"
#include "District/VoidDistrictGenerator.h"
#include "District/VoidDistrictActors.h"
#include "District/VoidDistrictQueryLibrary.h"
#include "Meridian/VoidMeridianRegistryReader.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Road/VoidRoadActor.h"

#include "VoidDistrictTestData.inc"

namespace VoidDistrictTestPrivate
{
	static bool LoadTestData(FVoidMeridianDataSet& OutData, FVoidValidationReport& OutReport)
	{
		OutReport.bIsValid = true;
		TArray<FString> Required;
		bool bOk = FVoidMeridianRegistryReader::ParseMasterManifest(FString(ANSI_TO_TCHAR(GVoidTestMasterJson)), OutData, Required, OutReport);
		bOk = bOk && FVoidMeridianRegistryReader::ParseDistrictRegistry(FString(ANSI_TO_TCHAR(GVoidTestDistrictRegistryJson)), OutData, OutReport);
		bOk = bOk && FVoidMeridianRegistryReader::ParseRoadNetwork(FString(ANSI_TO_TCHAR(GVoidTestRoadNetworkJson)), OutData, OutReport);
		bOk = bOk && FVoidMeridianRegistryReader::ParseLandmarkRegistry(FString(ANSI_TO_TCHAR(GVoidTestLandmarkRegistryJson)), OutData, OutReport);
		return bOk;
	}

	static int32 CountIssues(const FVoidValidationReport& Report, const TCHAR* Code)
	{
		int32 Count = 0;
		for (const FVoidValidationIssue& Issue : Report.Issues)
		{
			if (Issue.ErrorCode == FName(Code)) { ++Count; }
		}
		return Count;
	}

	static UWorld* CreateTestWorld()
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Editor);
		Context.SetCurrentWorld(World);
		return World;
	}

	static void DestroyTestWorld(UWorld* World)
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	}

	static int32 CountGenerated(UWorld* World)
	{
		int32 Count = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->Tags.Contains(VoidDistrictTags::Generated)) { ++Count; }
		}
		return Count;
	}
}

// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidDistrictGeometryTest, "VOID.WorldBuilder.Districts.Geometry.BoxesAndShapes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidDistrictGeometryTest::RunTest(const FString& Parameters)
{
	FVoidBox2D A; A.Center = FVector2D(0, 0); A.HalfExtent = FVector2D(100, 100);
	FVoidBox2D B = A; B.Center = FVector2D(150, 0);
	FVoidBox2D Far = A; Far.Center = FVector2D(500, 0);
	TestTrue(TEXT("Overlapping boxes overlap"), FVoidDistrictGeometry::DoBoxesOverlap(A, B, 0.0));
	TestFalse(TEXT("Distant boxes do not overlap"), FVoidDistrictGeometry::DoBoxesOverlap(A, Far, 0.0));

	FVoidBox2D Rotated = A; Rotated.YawRadians = PI * 0.25; Rotated.Center = FVector2D(250, 0);
	TestFalse(TEXT("Rotated box beyond its diagonal does not overlap"), FVoidDistrictGeometry::DoBoxesOverlap(A, Rotated, 0.0));

	const TArray<FVector2D> Circle = FVoidDistrictGeometry::MakeCirclePolygon(FVector2D::ZeroVector, 1000.0, 96);
	TestTrue(TEXT("Centre is inside the circle"), FVoidDistrictGeometry::IsPointInShape(FVector2D(0, 0), Circle, FVector2D::ZeroVector, 0.0));
	TestFalse(TEXT("Centre is outside once a hole is cut"), FVoidDistrictGeometry::IsPointInShape(FVector2D(0, 0), Circle, FVector2D::ZeroVector, 300.0));
	TestTrue(TEXT("Ring point is inside an annulus"), FVoidDistrictGeometry::IsPointInShape(FVector2D(600, 0), Circle, FVector2D::ZeroVector, 300.0));

	FVoidBox2D Small; Small.HalfExtent = FVector2D(50, 50); Small.Center = FVector2D(600, 0);
	TestTrue(TEXT("Box within an annulus is inside"), FVoidDistrictGeometry::IsBoxInsideShape(Small, Circle, FVector2D::ZeroVector, 300.0));
	Small.Center = FVector2D(1000, 0);
	TestFalse(TEXT("Box straddling the outer edge is not inside"), FVoidDistrictGeometry::IsBoxInsideShape(Small, Circle, FVector2D::ZeroVector, 300.0));

	FVoidDistrictRng R1(42), R2(42), R3(43);
	TestEqual(TEXT("RNG is deterministic"), R1.NextUInt(), R2.NextUInt());
	TestNotEqual(TEXT("Different seeds diverge"), FVoidDistrictRng(42).NextUInt(), R3.NextUInt());
	return true;
}

// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidMeridianReaderTest, "VOID.WorldBuilder.Districts.Reader.ParsesRealRegistries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidMeridianReaderTest::RunTest(const FString& Parameters)
{
	FVoidMeridianDataSet Data;
	FVoidValidationReport Report;
	TestTrue(TEXT("Registries parse cleanly"), VoidDistrictTestPrivate::LoadTestData(Data, Report));
	TestEqual(TEXT("Five districts, no sixth"), Data.Districts.Num(), 5);
	TestEqual(TEXT("Landmark count matches LandmarkRegistry.json"), Data.Landmarks.Num(), 14);
	TestTrue(TEXT("Spine route present"), Data.FindRoute(VoidDistrictNames::SpireRadialSpine) != nullptr);
	TestTrue(TEXT("Unreachable pairs read"), Data.UnreachablePairs.Num() > 0);
	TestTrue(TEXT("Macro order read"), Data.MacroGenerationOrder.Num() > 0);
	if (const FVoidMeridianRoute* Layer = Data.FindRoute(FName(TEXT("undercroft_maintenance_layer"))))
	{
		TestEqual(TEXT("Undercroft has three belts"), Layer->SubBeltsServed.Num(), 3);
	}
	else
	{
		AddError(TEXT("undercroft_maintenance_layer missing"));
	}

	// A landmark that names a district not in the registry must be rejected (BuilderRules: no sixth district).
	FVoidValidationReport Bad;
	Bad.bIsValid = true;
	FVoidMeridianRegistryReader::ParseLandmarkRegistry(TEXT("{\"landmarks\":[{\"id\":\"x\",\"district\":\"sixth\",\"type\":\"t\",\"visibility_tier\":1}]}"), Data, Bad);
	TestFalse(TEXT("Unknown-district landmark is an error"), Bad.bIsValid);
	return true;
}

// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidDistrictPlanTest, "VOID.WorldBuilder.Districts.Layout.PlanIsValidAndDeterministic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidDistrictPlanTest::RunTest(const FString& Parameters)
{
	using namespace VoidDistrictTestPrivate;

	FVoidMeridianDataSet Data;
	FVoidValidationReport LoadReport;
	if (!LoadTestData(Data, LoadReport)) { AddError(TEXT("Test data failed to load")); return false; }

	FVoidDistrictLayoutInput Input;
	Input.Data = &Data;

	FVoidDistrictLayoutPlan Plan = FVoidDistrictLayoutBuilder::Build(Input);
	FVoidDistrictValidator::Validate(Plan, Data, Input.Params);
	TestEqual(TEXT("Default plan has zero errors"), Plan.Report.NumErrors(), 0);
	TestEqual(TEXT("All five districts planned"), Plan.Districts.Num(), 5);
	TestEqual(TEXT("Every registry landmark placed"), CountIssues(Plan.Report, TEXT("VOID.District.LandmarkUnplaced")), 0);

	const FVoidPlannedDistrict* WhiteZones = Plan.FindDistrict(VoidDistrictNames::WhiteZones);
	if (TestNotNull(TEXT("White Zones planned"), WhiteZones))
	{
		TestEqual(TEXT("One node per sector"), WhiteZones->Nodes.Num(), Input.Params.SpokeCount);
		TestTrue(TEXT("Exactly one flagship node"), WhiteZones->Nodes.FilterByPredicate([](const FVoidPlannedNode& N) { return N.bFlagship; }).Num() == 1);
		for (const FVoidPlannedBuilding& Building : WhiteZones->Buildings)
		{
			TestTrue(TEXT("White Zones respects the 2-4 story ceiling"), Building.Spec.HeightUnits <= 4.0f * Input.Params.StoryHeight + KINDA_SMALL_NUMBER);
		}
	}

	const FVoidPlannedDistrict* Sector0 = Plan.FindDistrict(VoidDistrictNames::Sector0);
	if (TestNotNull(TEXT("Sector 0 planned"), Sector0))
	{
		TestEqual(TEXT("Sector 0 has near-zero density: no buildings"), Sector0->Buildings.Num(), 0);
	}

	// Districts must not all look alike.
	const FVoidPlannedDistrict* Spire = Plan.FindDistrict(VoidDistrictNames::OlympusSpire);
	if (Spire && WhiteZones)
	{
		TestNotEqual(TEXT("Spire and White Zones differ in coverage"), Spire->Profile.BuildingCoverage, WhiteZones->Profile.BuildingCoverage);
	}

	FVoidDistrictLayoutPlan Again = FVoidDistrictLayoutBuilder::Build(Input);
	bool bSame = Again.Districts.Num() == Plan.Districts.Num();
	for (int32 I = 0; bSame && I < Plan.Districts.Num(); ++I)
	{
		bSame = Plan.Districts[I].Buildings.Num() == Again.Districts[I].Buildings.Num() && Plan.Districts[I].Roads.Num() == Again.Districts[I].Roads.Num();
		for (int32 J = 0; bSame && J < Plan.Districts[I].Buildings.Num(); ++J)
		{
			bSame = Plan.Districts[I].Buildings[J].Spec.Id.Value == Again.Districts[I].Buildings[J].Spec.Id.Value
				&& Plan.Districts[I].Buildings[J].Center.Equals(Again.Districts[I].Buildings[J].Center, 0.001);
		}
	}
	TestTrue(TEXT("Planning is deterministic"), bSame);
	return true;
}

// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidDistrictValidatorCatchesTest, "VOID.WorldBuilder.Districts.Validator.CatchesBrokenPlans",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidDistrictValidatorCatchesTest::RunTest(const FString& Parameters)
{
	using namespace VoidDistrictTestPrivate;

	FVoidMeridianDataSet Data;
	FVoidValidationReport LoadReport;
	if (!LoadTestData(Data, LoadReport)) { AddError(TEXT("Test data failed to load")); return false; }
	FVoidDistrictLayoutInput Input;
	Input.Data = &Data;

	{
		FVoidDistrictLayoutPlan Plan = FVoidDistrictLayoutBuilder::Build(Input);
		Plan.FindDistrict(VoidDistrictNames::WhiteZones)->Buildings[0].Center = FVector2D::ZeroVector;
		FVoidDistrictValidator::Validate(Plan, Data, Input.Params);
		TestTrue(TEXT("Building outside its boundary is reported"), CountIssues(Plan.Report, TEXT("VOID.District.BuildingOutsideBoundary")) > 0);
	}
	{
		FVoidDistrictLayoutPlan Plan = FVoidDistrictLayoutBuilder::Build(Input);
		Plan.FindDistrict(VoidDistrictNames::OlympusSpire)->Roads.Last().Spec.CenterlinePoints[0] = FVector2D(9000, 9000);
		FVoidDistrictValidator::Validate(Plan, Data, Input.Params);
		TestTrue(TEXT("Dangling road endpoint is reported"), CountIssues(Plan.Report, TEXT("VOID.District.RoadDeadEnd")) > 0);
	}
	{
		FVoidDistrictLayoutPlan Plan = FVoidDistrictLayoutBuilder::Build(Input);
		Plan.FindDistrict(VoidDistrictNames::OlympusSpire)->PublicSpaces[0].HoleRadius = 0.0;
		FVoidDistrictValidator::Validate(Plan, Data, Input.Params);
		TestTrue(TEXT("Plaza over the Spire tower is reported"), CountIssues(Plan.Report, TEXT("VOID.District.PublicSpaceOverlapsMajorStructure")) > 0);
	}
	{
		FVoidDistrictLayoutPlan Plan = FVoidDistrictLayoutBuilder::Build(Input);
		FVoidPlannedDistrict* WhiteZones = Plan.FindDistrict(VoidDistrictNames::WhiteZones);
		FVoidPlannedBuilding Copy = WhiteZones->Buildings[0];
		Copy.Spec.Id = FVoidElementId(FName(TEXT("duplicate")));
		WhiteZones->Buildings.Add(Copy);
		FVoidDistrictValidator::Validate(Plan, Data, Input.Params);
		TestTrue(TEXT("Duplicate geometry is reported"), CountIssues(Plan.Report, TEXT("VOID.District.DuplicateGeometry")) > 0);
	}
	return true;
}

// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidDistrictWorldGenerationTest, "VOID.WorldBuilder.Districts.Generator.OwnershipAndRegeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidDistrictWorldGenerationTest::RunTest(const FString& Parameters)
{
	using namespace VoidDistrictTestPrivate;

	FVoidMeridianDataSet Data;
	FVoidValidationReport LoadReport;
	if (!LoadTestData(Data, LoadReport)) { AddError(TEXT("Test data failed to load")); return false; }
	FVoidDistrictLayoutInput Input;
	Input.Data = &Data;

	UWorld* World = CreateTestWorld();
	if (!World) { AddError(TEXT("Could not create a test world")); return false; }

	FVoidDistrictSpawnOptions Options;
	Options.bPreferRegisteredBuildingGenerator = false;

	FVoidGenerationContext Context;
	Context.TargetWorld = World;
	FVoidDistrictGenerationStats First;
	const bool bFirst = FVoidDistrictGenerator::GenerateFromInput(Input, Options, nullptr, Context, &First);
	TestTrue(TEXT("First generation succeeds"), bFirst);

	if (bFirst)
	{
		const int32 GeneratedAfterFirst = CountGenerated(World);
		TestEqual(TEXT("One district actor per district"), First.NumDistricts, 5);
		TestEqual(TEXT("One Meridian root"), [&]() { int32 N = 0; for (TActorIterator<AVoidMeridianRootActor> It(World); It; ++It) { ++N; } return N; }(), 1);
		TestTrue(TEXT("Roads were generated and adopted"), First.NumRoadActors > 0);
		TestTrue(TEXT("Landmarks were generated"), First.NumLandmarks >= 14);

		// Ownership: every road/junction/landmark actor is attached under a district actor or the root.
		int32 Orphans = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (!It->Tags.Contains(VoidDistrictTags::Generated) || It->IsA<AVoidMeridianRootActor>()) { continue; }
			if (!It->GetAttachParentActor()) { ++Orphans; }
		}
		TestEqual(TEXT("No orphaned generated actors"), Orphans, 0);

		// Query surface used by other agents.
		TestNotNull(TEXT("Spire silhouette is discoverable"), UVoidDistrictQueryLibrary::FindLandmark(World, VoidDistrictNames::SpireSilhouette));
		TestTrue(TEXT("Skyline landmarks are queryable"), UVoidDistrictQueryLibrary::GetLandmarks(World, NAME_None, true).Num() > 0);

		// Regeneration: same counts, nothing duplicated.
		FVoidGenerationContext Context2;
		Context2.TargetWorld = World;
		FVoidDistrictGenerationStats Second;
		TestTrue(TEXT("Second generation succeeds"), FVoidDistrictGenerator::GenerateFromInput(Input, Options, nullptr, Context2, &Second));
		TestEqual(TEXT("Regeneration removes the previous run"), Second.NumRemovedActors, GeneratedAfterFirst);
		TestEqual(TEXT("Regeneration yields the same actor count"), CountGenerated(World), GeneratedAfterFirst);
		TestEqual(TEXT("Same road actors"), Second.NumRoadActors, First.NumRoadActors);
	}

	DestroyTestWorld(World);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
