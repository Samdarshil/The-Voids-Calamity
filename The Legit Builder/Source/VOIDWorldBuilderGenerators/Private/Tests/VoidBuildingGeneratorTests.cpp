// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Misc/AutomationTest.h"
#include "Building/VoidBuildingGeometry.h"
#include "Building/VoidBuildingNormalizer.h"
#include "Building/VoidBuildingRoadContext.h"
#include "Building/VoidBuildingGenerator.h"
#include "Building/VoidBuildingActor.h"
#include "Building/VoidBuildingTestFixture.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Algo/Reverse.h"
#include "HAL/PlatformTime.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using G = FVoidBuildingGeometry;

	TArray<FVector2D> Rect(double X0, double Y0, double X1, double Y1)
	{
		return { FVector2D(X0, Y0), FVector2D(X1, Y0), FVector2D(X1, Y1), FVector2D(X0, Y1) };
	}

	FString Signatures(const TArray<FVoidBuildingBatchData>& Batches)
	{
		FString Out;
		for (const FVoidBuildingBatchData& B : Batches)
		{
			Out += FString::Printf(TEXT("%d,%d:%s:%d;"), B.Cell.X, B.Cell.Y, *B.Signature, B.Mesh.NumVertices());
		}
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidBuildingGeometryTest, "VOID.WorldBuilder.BuildingGenerator.Geometry.InsetAndTriangulate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidBuildingGeometryTest::RunTest(const FString& Parameters)
{
	const TArray<FVector2D> Square = Rect(0, 0, 1000, 1000);
	TestEqual(TEXT("area"), G::SignedArea(Square), 1000000.0);

	TArray<FVector2D> Inset;
	TestTrue(TEXT("inset succeeds"), G::InsetPolygon(Square, 100.0, Inset));
	TestEqual(TEXT("inset area"), G::SignedArea(Inset), 640000.0, 1.0);
	TestFalse(TEXT("inset larger than half-width fails"), G::InsetPolygon(Square, 600.0, Inset));

	// L shape: triangulation area must equal polygon area.
	const TArray<FVector2D> L = { FVector2D(0, 0), FVector2D(300, 0), FVector2D(300, 100), FVector2D(100, 100), FVector2D(100, 300), FVector2D(0, 300) };
	TArray<int32> Idx;
	TestTrue(TEXT("triangulate L"), G::Triangulate(L, Idx));
	double Sum = 0.0;
	for (int32 I = 0; I + 2 < Idx.Num(); I += 3)
	{
		Sum += 0.5 * G::Cross(L[Idx[I + 1]] - L[Idx[I]], L[Idx[I + 2]] - L[Idx[I]]);
	}
	TestEqual(TEXT("L area preserved"), Sum, G::SignedArea(L), 0.01);

	const TArray<FVector2D> Bowtie = { FVector2D(0, 0), FVector2D(100, 100), FVector2D(100, 0), FVector2D(0, 100) };
	TestFalse(TEXT("bowtie not simple"), G::IsSimple(Bowtie));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidBuildingClassifyTest, "VOID.WorldBuilder.BuildingGenerator.Normalizer.Classification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidBuildingClassifyTest::RunTest(const FString& Parameters)
{
	const TMap<FString, EVoidBuildingCategory> None;
	TestTrue(TEXT("office"), FVoidBuildingNormalizer::Classify(TEXT("Office_Tower"), None) == EVoidBuildingCategory::Office);
	TestTrue(TEXT("landmark wins"), FVoidBuildingNormalizer::Classify(TEXT("Landmark_Office"), None) == EVoidBuildingCategory::Landmark);
	TestTrue(TEXT("unknown"), FVoidBuildingNormalizer::Classify(TEXT("Mystery_Box"), None) == EVoidBuildingCategory::Unknown);
	TestTrue(TEXT("empty"), FVoidBuildingNormalizer::Classify(TEXT(""), None) == EVoidBuildingCategory::Unknown);

	TMap<FString, EVoidBuildingCategory> Over;
	Over.Add(TEXT("mystery"), EVoidBuildingCategory::Industrial);
	TestTrue(TEXT("override"), FVoidBuildingNormalizer::Classify(TEXT("Mystery_Box"), Over) == EVoidBuildingCategory::Industrial);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidBuildingDeterminismTest, "VOID.WorldBuilder.BuildingGenerator.Determinism.SameInputSameCity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidBuildingDeterminismTest::RunTest(const FString& Parameters)
{
	FVoidBuildingGenerationParams P;
	FVoidDistrictData D = FVoidBuildingTestFixture::MakeSmallDistrict();

	FVoidValidationReport R1, R2, R3;
	TArray<FVoidBuildingBatchData> A, B, C;
	FVoidBuildingRunStats S1, S2, S3;
	FVoidBuildingGenerator::BuildBatches(D, P, R1, A, S1);
	FVoidBuildingGenerator::BuildBatches(D, P, R2, B, S2);
	TestEqual(TEXT("two runs identical"), Signatures(A), Signatures(B));

	// Source order must not matter.
	Algo::Reverse(D.Buildings);
	FVoidBuildingGenerator::BuildBatches(D, P, R3, C, S3);
	TestEqual(TEXT("order independent"), Signatures(A), Signatures(C));

	// A different global seed must change the city.
	P.GlobalSeed = 12345;
	FVoidValidationReport R4;
	TArray<FVoidBuildingBatchData> E;
	FVoidBuildingRunStats S4;
	FVoidBuildingGenerator::BuildBatches(D, P, R4, E, S4);
	TestNotEqual(TEXT("seed changes output"), Signatures(A), Signatures(E));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidBuildingRoadClearanceTest, "VOID.WorldBuilder.BuildingGenerator.Roads.NoBuildingInCorridor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidBuildingRoadClearanceTest::RunTest(const FString& Parameters)
{
	FVoidBuildingGenerationParams P;
	const FVoidDistrictData D = FVoidBuildingTestFixture::MakeSmallDistrict();

	FVoidValidationReport Report;
	TArray<FVoidBuildingBatchData> Batches;
	FVoidBuildingRunStats Stats;
	FVoidBuildingGenerator::BuildBatches(D, P, Report, Batches, Stats);

	FVoidBuildingRoadContext Roads;
	Roads.Build(D, P);
	TestTrue(TEXT("bridge road ignored"), Roads.NumSkippedGradeSeparated() == 1);

	int32 Checked = 0;
	for (const FVoidBuildingBatchData& Batch : Batches)
	{
		for (const FVoidBuildingMetadata& M : Batch.Metadata)
		{
			const float Setback = M.bIsLandmark ? P.MinSetbackUnits * P.LandmarkSetbackMultiplier : P.MinSetbackUnits;
			for (const FVoidRoadCorridorSegment& Seg : Roads.GetSegments())
			{
				const double Dist = G::PolygonSegmentDistance(M.FootprintCorners, Seg.A, Seg.B);
				TestTrue(*FString::Printf(TEXT("%s clear of %s"), *M.BuildingId.ToString(), *Seg.RoadId.ToString()), Dist >= Seg.CorridorRadius + Setback - 1.5);
			}
			++Checked;
		}
	}
	TestTrue(TEXT("buildings were checked"), Checked > 40);
	TestTrue(TEXT("straddling building skipped"), Stats.NumSkippedRoad >= 1);
	TestTrue(TEXT("near-road building adjusted"), Stats.NumAdjusted >= 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidBuildingAuthoredDataTest, "VOID.WorldBuilder.BuildingGenerator.Data.HeightAndIdsPreserved",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidBuildingAuthoredDataTest::RunTest(const FString& Parameters)
{
	FVoidBuildingGenerationParams P;
	const FVoidDistrictData D = FVoidBuildingTestFixture::MakeSmallDistrict();
	FVoidValidationReport Report;
	TArray<FVoidBuildingBatchData> Batches;
	FVoidBuildingRunStats Stats;
	FVoidBuildingGenerator::BuildBatches(D, P, Report, Batches, Stats);

	TMap<FName, float> Heights;
	for (const FVoidBuildingSpec& S : D.Buildings) { Heights.Add(S.Id.Value, S.HeightUnits); }

	for (const FVoidBuildingBatchData& Batch : Batches)
	{
		for (const FVoidBuildingMetadata& M : Batch.Metadata)
		{
			TestEqual(*FString::Printf(TEXT("%s height"), *M.BuildingId.ToString()), M.HeightUnits, Heights[M.BuildingId]);
			TestTrue(TEXT("asset category set"), !M.AssetCategory.IsNone());
		}
	}
	TestTrue(TEXT("bowtie rejected"), Report.Issues.ContainsByPredicate([](const FVoidValidationIssue& I) { return I.ErrorCode == FName(TEXT("VOID.Building.SelfIntersectingFootprint")); }));
	TestTrue(TEXT("landmarks counted"), Stats.NumLandmarks >= 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidBuildingLargeCountTest, "VOID.WorldBuilder.BuildingGenerator.Performance.ThousandsOfBuildings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidBuildingLargeCountTest::RunTest(const FString& Parameters)
{
	FVoidBuildingGenerationParams P;
	const FVoidDistrictData D = FVoidBuildingTestFixture::MakeLargeDistrict(72, 72); // ~3600 buildings
	FVoidValidationReport Report;
	TArray<FVoidBuildingBatchData> Batches;
	FVoidBuildingRunStats Stats;
	const double T0 = FPlatformTime::Seconds();
	FVoidBuildingGenerator::BuildBatches(D, P, Report, Batches, Stats);
	const double Ms = (FPlatformTime::Seconds() - T0) * 1000.0;

	AddInfo(FString::Printf(TEXT("%d buildings -> %d batches, %d tris, %.0f ms"), Stats.NumGenerated, Stats.NumBatches, Stats.NumTriangles, Ms));
	TestTrue(TEXT("thousands generated"), Stats.NumGenerated > 3000);
	TestTrue(TEXT("batched, not one actor per building"), Stats.NumBatches < Stats.NumGenerated / 20);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidBuildingRegenerationTest, "VOID.WorldBuilder.BuildingGenerator.Regeneration.NoDuplicates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidBuildingRegenerationTest::RunTest(const FString& Parameters)
{
	// NOTE: world-creation pattern below is written from engine convention and has not been run in UE 5.8.2.
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
	if (!TestNotNull(TEXT("world"), World)) { return false; }
	FWorldContext& Ctx = GEngine->CreateNewWorldContext(EWorldType::Editor);
	Ctx.SetCurrentWorld(World);

	FVoidDesignPackage Package;
	Package.District = FVoidBuildingTestFixture::MakeSmallDistrict();

	auto Count = [World]()
	{
		int32 N = 0;
		for (TActorIterator<AVoidBuildingBatchActor> It(World); It; ++It) { ++N; }
		return N;
	};

	FVoidBuildingGenerator Generator;
	FVoidGenerationContext Context;
	Context.TargetWorld = World;
	TestTrue(TEXT("first run"), Generator.Generate(Package, Context));
	const int32 First = Count();
	TestTrue(TEXT("actors exist"), First > 0);

	FVoidGenerationContext Context2;
	Context2.TargetWorld = World;
	TestTrue(TEXT("second run"), Generator.Generate(Package, Context2));
	TestEqual(TEXT("no duplicates after regeneration"), Count(), First);

	// Modify source, regenerate: old result replaced.
	Package.District.Buildings.RemoveAt(0);
	FVoidGenerationContext Context3;
	Context3.TargetWorld = World;
	Generator.Generate(Package, Context3);
	int32 TotalBuildings = 0;
	for (TActorIterator<AVoidBuildingBatchActor> It(World); It; ++It) { TotalBuildings += It->Buildings.Num(); }
	FVoidValidationReport R; TArray<FVoidBuildingBatchData> B; FVoidBuildingRunStats S;
	FVoidBuildingGenerator::BuildBatches(Package.District, FVoidBuildingGenerationParams(), R, B, S);
	TestEqual(TEXT("building count tracks source"), TotalBuildings, S.NumGenerated);

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
