// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Misc/AutomationTest.h"
#include "Environment/VoidDressingGenerator.h"
#include "Environment/VoidDressingActor.h"
#include "Environment/VoidPlacementPlanner.h"
#include "Environment/VoidEnvironmentSettings.h"
#include "Environment/VoidDressingDefaults.h"
#include "VoidGeneratorRegistry.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Editor.h"
#include "EngineUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace VoidEnvTestPrivate
{
	static UWorld* GetTestWorld() { return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr; }

	static void DestroyAllDressing(UWorld* World)
	{
		TArray<AVoidDressingActor*> ToDestroy;
		for (TActorIterator<AVoidDressingActor> It(World); It; ++It) { ToDestroy.Add(*It); }
		for (AVoidDressingActor* A : ToDestroy) { if (IsValid(A)) { A->Destroy(); } }
	}

	static int32 CountDressing(UWorld* World, FName GeneratorId = NAME_None)
	{
		int32 N = 0;
		for (TActorIterator<AVoidDressingActor> It(World); It; ++It) { if (GeneratorId.IsNone() || It->GeneratorId == GeneratorId) { ++N; } }
		return N;
	}

	/** Collects (StableId -> location) for every instance across all dressing actors of a generator. */
	static TMap<int64, FVector> Snapshot(UWorld* World, FName GeneratorId)
	{
		TMap<int64, FVector> Out;
		for (TActorIterator<AVoidDressingActor> It(World); It; ++It)
		{
			if (It->GeneratorId != GeneratorId) { continue; }
			for (const FVoidDressingBucketInfo& B : It->Buckets)
			{
				if (B.PartIndex != 0 || !B.Component) { continue; }
				for (int32 I = 0; I < B.InstanceIds.Num() && I < B.Component->GetInstanceCount(); ++I)
				{
					FTransform T;
					B.Component->GetInstanceTransform(I, T, /*bWorldSpace=*/true);
					Out.Add(B.InstanceIds[I], T.GetLocation());
				}
			}
		}
		return Out;
	}

	static FVoidDesignPackage MakePackage(const TCHAR* DistrictId = TEXT("white_zones"))
	{
		FVoidDesignPackage P;
		P.District.DistrictId = FVoidElementId(FName(DistrictId));

		FVoidRoadSpec Main;
		Main.Id = FVoidElementId(FName(TEXT("main")));
		Main.RoadType = EVoidRoadType::Primary;
		Main.CenterlinePoints = { FVector2D(0, 0), FVector2D(12000, 0) };
		Main.bHasSidewalk = true;
		Main.bHasMedian = true;
		P.District.Roads.Add(Main);

		FVoidRoadSpec Cross;
		Cross.Id = FVoidElementId(FName(TEXT("cross")));
		Cross.RoadType = EVoidRoadType::Secondary;
		Cross.CenterlinePoints = { FVector2D(6000, -6000), FVector2D(6000, 0), FVector2D(6000, 6000) };
		Cross.bHasSidewalk = true;
		P.District.Roads.Add(Cross);

		FVoidRoadSpec Alley;
		Alley.Id = FVoidElementId(FName(TEXT("alley")));
		Alley.RoadType = EVoidRoadType::Alley;
		Alley.CenterlinePoints = { FVector2D(0, 5000), FVector2D(9000, 5000) };
		P.District.Roads.Add(Alley);

		FVoidBuildingSpec Shop;
		Shop.Id = FVoidElementId(FName(TEXT("shop")));
		Shop.BuildingType = TEXT("Commercial_Retail");
		Shop.HeightUnits = 2400.0f;
		Shop.FootprintCorners = { FVector2D(1000, 1200), FVector2D(3000, 1200), FVector2D(3000, 3200), FVector2D(1000, 3200) };
		P.District.Buildings.Add(Shop);

		FVoidBuildingSpec Park;
		Park.Id = FVoidElementId(FName(TEXT("park")));
		Park.BuildingType = TEXT("Park_Green");
		Park.HeightUnits = 0.0f;
		Park.FootprintCorners = { FVector2D(7000, 1500), FVector2D(11000, 1500), FVector2D(11000, 4500), FVector2D(7000, 4500) };
		P.District.Buildings.Add(Park);
		return P;
	}
}

// ---------------------------------------------------------------------------
// Pure planner (no world needed)
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidPlannerDeterminismTest,
	"VOID.WorldBuilder.Environment.Planner.SameInputSameOutputAndStableUnderAdditions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidPlannerDeterminismTest::RunTest(const FString& Parameters)
{
	using namespace VoidPlan;
	auto MakeRoad = [](const TCHAR* Id, double Y)
	{
		FRoad R; R.Id = FName(Id); R.Tier = 2; R.bHasSidewalk = true;
		R.Points = { FVector(0, Y, 0), FVector(10000, Y, 0) };
		return R;
	};
	FInputs In;
	In.DistrictId = FName(TEXT("d"));
	In.MinImportanceDensity = 1.0f;
	In.Roads.Add(MakeRoad(TEXT("a"), 0.0));

	FRule Tree; Tree.RuleId = FName(TEXT("tree")); Tree.Category = FName(TEXT("Tree")); Tree.Context = FName(TEXT("Sidewalk")); Tree.SpacingUnits = 700; Tree.Probability = 0.7f;
	TArray<FRule> Rules; Rules.Add(Tree);

	const TArray<FInstance> A = Plan(In, Rules);
	const TArray<FInstance> B = Plan(In, Rules);
	TestTrue(TEXT("Planner produces instances"), A.Num() > 0);
	TestEqual(TEXT("Repeat run yields the same count"), A.Num(), B.Num());
	bool bSame = A.Num() == B.Num();
	for (int32 I = 0; bSame && I < A.Num(); ++I) { bSame = A[I].StableId == B[I].StableId && A[I].Location.Equals(B[I].Location, 0.001); }
	TestTrue(TEXT("Repeat run yields identical ids and locations"), bSame);

	FInputs In2 = In;
	In2.Roads.Add(MakeRoad(TEXT("b"), 4000.0));
	const TArray<FInstance> C = Plan(In2, Rules);
	int32 Missing = 0;
	for (const FInstance& X : A)
	{
		bool bFound = false;
		for (const FInstance& Y : C) { if (Y.StableId == X.StableId && Y.Location.Equals(X.Location, 0.001)) { bFound = true; break; } }
		if (!bFound) { ++Missing; }
	}
	TestEqual(TEXT("Adding a road never disturbs existing road's instances"), Missing, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidPlannerContextRulesTest,
	"VOID.WorldBuilder.Environment.Planner.ContextRulesAreRespected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidPlannerContextRulesTest::RunTest(const FString& Parameters)
{
	using namespace VoidPlan;
	FInputs In;
	In.DistrictId = FName(TEXT("d"));
	In.MinImportanceDensity = 1.0f;

	FRoad Walk; Walk.Id = FName(TEXT("walk")); Walk.Tier = 2; Walk.bHasSidewalk = true; Walk.HalfWidth = 300; Walk.BandInner = 325; Walk.BandOuter = 475;
	Walk.Points = { FVector(0, 0, 0), FVector(8000, 0, 0) };
	FRoad Bare = Walk; Bare.Id = FName(TEXT("bare")); Bare.bHasSidewalk = false; Bare.Points = { FVector(0, 3000, 0), FVector(8000, 3000, 0) };
	FRoad Tunnel = Walk; Tunnel.Id = FName(TEXT("tunnel")); Tunnel.bTunnel = true; Tunnel.Points = { FVector(0, 6000, 0), FVector(8000, 6000, 0) };
	In.Roads = { Walk, Bare, Tunnel };

	FRule Tree; Tree.RuleId = FName(TEXT("t")); Tree.Category = FName(TEXT("Tree")); Tree.Context = FName(TEXT("Sidewalk")); Tree.SpacingUnits = 500;
	TArray<FRule> Rules; Rules.Add(Tree);
	const TArray<FInstance> Trees = Plan(In, Rules);
	bool bOnlyWalk = Trees.Num() > 0, bInBand = true;
	for (const FInstance& T : Trees)
	{
		if (T.SourceId != FName(TEXT("walk"))) { bOnlyWalk = false; }
		const double D = FMath::Abs(T.Location.Y);
		if (D < 324.0 || D > 476.0) { bInBand = false; }
	}
	TestTrue(TEXT("Sidewalk trees only on roads with sidewalks (never tunnels)"), bOnlyWalk);
	TestTrue(TEXT("Sidewalk trees stay inside the sidewalk band"), bInBand);

	FRule Light; Light.RuleId = FName(TEXT("l")); Light.Category = FName(TEXT("Streetlight")); Light.Context = FName(TEXT("Roadside")); Light.SpacingUnits = 500;
	Rules.Reset(); Rules.Add(Light);
	bool bBare = false, bTunnel = false;
	for (const FInstance& L : Plan(In, Rules)) { bBare |= (L.SourceId == FName(TEXT("bare"))); bTunnel |= (L.SourceId == FName(TEXT("tunnel"))); }
	TestTrue(TEXT("Roadside props work on roads without sidewalks"), bBare);
	TestFalse(TEXT("No roadside props inside tunnels"), bTunnel);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidPlannerDensityControlsTest,
	"VOID.WorldBuilder.Environment.Planner.DensityBudgetAndFilteringWork",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidPlannerDensityControlsTest::RunTest(const FString& Parameters)
{
	using namespace VoidPlan;
	FInputs In;
	In.DistrictId = FName(TEXT("d"));
	In.MinImportanceDensity = 1.0f;
	FRoad R; R.Id = FName(TEXT("r")); R.Tier = 2; R.bHasSidewalk = true; R.Points = { FVector(0, 0, 0), FVector(10000, 0, 0) };
	In.Roads.Add(R);
	FRule Tree; Tree.RuleId = FName(TEXT("t")); Tree.Category = FName(TEXT("Tree")); Tree.Context = FName(TEXT("Sidewalk")); Tree.SpacingUnits = 300;
	TArray<FRule> Rules; Rules.Add(Tree);

	FInputs Lo = In; Lo.DensityScale = 0.25f;
	FInputs Mid = In; Mid.DensityScale = 0.5f;
	const TArray<FInstance> ALo = Plan(Lo, Rules), AMid = Plan(Mid, Rules), AHi = Plan(In, Rules);
	TestTrue(TEXT("Density scale is monotonic"), ALo.Num() < AMid.Num() && AMid.Num() < AHi.Num());
	int32 NotSubset = 0;
	for (const FInstance& L : ALo) { bool bF = false; for (const FInstance& M : AMid) { if (M.StableId == L.StableId) { bF = true; break; } } if (!bF) { ++NotSubset; } }
	TestEqual(TEXT("Lowering density thins the same set (no reshuffle)"), NotSubset, 0);

	FInputs Budget = In; Budget.MaxInstances = 10; FStats Stats;
	TestEqual(TEXT("Instance budget enforced"), Plan(Budget, Rules, &Stats).Num(), 10);
	TestTrue(TEXT("Budget drops are reported"), Stats.RejectedBudget > 0);

	FInputs Filter = In; Filter.CategoryAllowList.Add(FName(TEXT("Bench")));
	TestEqual(TEXT("Category allow-list filters categories"), Plan(Filter, Rules).Num(), 0);
	return true;
}

// ---------------------------------------------------------------------------
// Generators in the editor world
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidGeneratorsRegisteredTest,
	"VOID.WorldBuilder.Environment.Registry.EnvironmentAndPropsAreRegistered",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidGeneratorsRegisteredTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("'Environment' generator registered"), FVoidGeneratorRegistry::Get().FindGenerator(TEXT("Environment")).IsValid());
	TestTrue(TEXT("'Props' generator registered"), FVoidGeneratorRegistry::Get().FindGenerator(TEXT("Props")).IsValid());
	TestTrue(TEXT("Road generator is still registered alongside them"), FVoidGeneratorRegistry::Get().FindGenerator(TEXT("Road")).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidEnvironmentGenerateInstancesTest,
	"VOID.WorldBuilder.Environment.Generate.SpawnsInstancedCellActorsNotPerPropActors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidEnvironmentGenerateInstancesTest::RunTest(const FString& Parameters)
{
	using namespace VoidEnvTestPrivate;
	UWorld* World = GetTestWorld();
	if (!TestNotNull(TEXT("Editor world should be available"), World)) { return false; }
	DestroyAllDressing(World);

	FVoidEnvironmentGenerator Gen;
	FVoidGenerationContext Ctx; Ctx.TargetWorld = World;
	FVoidDressingRunResult Result;
	FVoidDressingOptions Opt; Opt.DensityScaleOverride = 1.0f;
	const bool bOk = Gen.GenerateWithOptions(MakePackage(), Ctx, Opt, &Result);

	TestTrue(TEXT("Generation succeeds"), bOk);
	TestTrue(TEXT("Logical placements were planned"), Result.LogicalInstances > 0);
	TestTrue(TEXT("Fewer actors than placements (instancing, not one actor per prop)"), Result.ActorsSpawned > 0 && Result.ActorsSpawned < Result.LogicalInstances);
	TestEqual(TEXT("Actor count matches the world"), CountDressing(World, TEXT("Environment")), Result.ActorsSpawned);
	TestTrue(TEXT("Placeholders reported as such"), Result.PlaceholderCategoriesUsed > 0);
	TestEqual(TEXT("Environment run created no Props actors"), CountDressing(World, TEXT("Props")), 0);

	for (TActorIterator<AVoidDressingActor> It(World); It; ++It)
	{
		TestFalse(TEXT("Dressing actors never tick"), It->PrimaryActorTick.bCanEverTick);
		TestTrue(TEXT("Buckets record placeholder status for the replacement hook"), It->Buckets.Num() > 0);
	}
	DestroyAllDressing(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidEnvironmentRegenerationTest,
	"VOID.WorldBuilder.Environment.Generate.RegenerationReplacesInsteadOfDuplicating",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidEnvironmentRegenerationTest::RunTest(const FString& Parameters)
{
	using namespace VoidEnvTestPrivate;
	UWorld* World = GetTestWorld();
	if (!TestNotNull(TEXT("Editor world should be available"), World)) { return false; }
	DestroyAllDressing(World);

	FVoidEnvironmentGenerator Env;
	FVoidPropGenerator Props;
	const FVoidDesignPackage Package = MakePackage();

	FVoidGenerationContext Ctx; Ctx.TargetWorld = World;
	Env.Generate(Package, Ctx);
	Props.Generate(Package, Ctx);
	const int32 ActorsAfterFirst = CountDressing(World);
	const TMap<int64, FVector> EnvFirst = Snapshot(World, TEXT("Environment"));
	const TMap<int64, FVector> PropFirst = Snapshot(World, TEXT("Props"));
	TestTrue(TEXT("First generation produced content"), ActorsAfterFirst > 0 && EnvFirst.Num() > 0 && PropFirst.Num() > 0);

	FVoidGenerationContext Ctx2; Ctx2.TargetWorld = World;
	FVoidDressingRunResult Result;
	Env.GenerateWithOptions(Package, Ctx2, FVoidDressingOptions(), &Result);
	Props.Generate(Package, Ctx2);
	TestEqual(TEXT("No duplicate actors after regeneration"), CountDressing(World), ActorsAfterFirst);
	TestTrue(TEXT("Previous output was removed by the run"), Result.ActorsRemoved > 0);

	const TMap<int64, FVector> EnvSecond = Snapshot(World, TEXT("Environment"));
	TestEqual(TEXT("Same seed regenerates the same placement count"), EnvSecond.Num(), EnvFirst.Num());
	int32 Moved = 0;
	for (const TPair<int64, FVector>& P : EnvFirst) { const FVector* Other = EnvSecond.Find(P.Key); if (!Other || !Other->Equals(P.Value, 0.01)) { ++Moved; } }
	TestEqual(TEXT("Same ids at the same locations after regeneration"), Moved, 0);

	// Regenerating one generator must not remove the other's output.
	Env.ClearGenerated(World, Package.District.DistrictId.Value);
	TestEqual(TEXT("Clearing Environment leaves Props intact"), CountDressing(World, TEXT("Props")) > 0, true);
	DestroyAllDressing(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidEnvironmentDistrictAndDensityTest,
	"VOID.WorldBuilder.Environment.Generate.DistrictFilteringDensityAndDryRun",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidEnvironmentDistrictAndDensityTest::RunTest(const FString& Parameters)
{
	using namespace VoidEnvTestPrivate;
	UWorld* World = GetTestWorld();
	if (!TestNotNull(TEXT("Editor world should be available"), World)) { return false; }
	DestroyAllDressing(World);

	FVoidEnvironmentGenerator Env;
	FVoidPropGenerator Props;

	// Two districts coexist; regenerating one leaves the other untouched.
	FVoidGenerationContext Ctx; Ctx.TargetWorld = World;
	Env.Generate(MakePackage(TEXT("white_zones")), Ctx);
	Env.Generate(MakePackage(TEXT("undercroft")), Ctx);
	int32 WhiteBefore = 0, UnderBefore = 0;
	for (TActorIterator<AVoidDressingActor> It(World); It; ++It) { (It->DistrictId == FName(TEXT("white_zones")) ? WhiteBefore : UnderBefore) += 1; }
	TestTrue(TEXT("Both districts generated"), WhiteBefore > 0 && UnderBefore > 0);
	Env.Generate(MakePackage(TEXT("white_zones")), Ctx);
	int32 WhiteAfter = 0, UnderAfter = 0;
	for (TActorIterator<AVoidDressingActor> It(World); It; ++It) { (It->DistrictId == FName(TEXT("white_zones")) ? WhiteAfter : UnderAfter) += 1; }
	TestEqual(TEXT("Regenerating white_zones keeps its actor count"), WhiteAfter, WhiteBefore);
	TestEqual(TEXT("Regenerating white_zones leaves undercroft untouched"), UnderAfter, UnderBefore);
	DestroyAllDressing(World);

	// Density control.
	FVoidDressingRunResult Lo, Hi;
	FVoidGenerationContext C2; C2.TargetWorld = World;
	FVoidDressingOptions OLo; OLo.DensityScaleOverride = 0.2f; OLo.bDryRun = true;
	FVoidDressingOptions OHi; OHi.DensityScaleOverride = 1.0f; OHi.bDryRun = true;
	Env.GenerateWithOptions(MakePackage(), C2, OLo, &Lo);
	Env.GenerateWithOptions(MakePackage(), C2, OHi, &Hi);
	TestTrue(TEXT("Lower density plans fewer placements"), Lo.LogicalInstances < Hi.LogicalInstances);
	TestEqual(TEXT("Dry run spawns nothing"), CountDressing(World), 0);

	// Category filter.
	FVoidDressingRunResult OnlyBench;
	FVoidDressingOptions OB; OB.CategoryAllowList.Add(FName(TEXT("Bench"))); OB.bDryRun = true; OB.DensityScaleOverride = 4.0f;
	Env.GenerateWithOptions(MakePackage(), C2, OB, &OnlyBench);
	bool bOnlyBench = OnlyBench.PlanStats.KeptPerCategory.Num() <= 1;
	for (const TPair<FName, int32>& P : OnlyBench.PlanStats.KeptPerCategory) { bOnlyBench &= (P.Key == FName(TEXT("Bench"))); }
	TestTrue(TEXT("Category allow-list restricts what is planned"), bOnlyBench);

	// Props generator handles its own domain only.
	FVoidDressingRunResult PropRes; FVoidDressingOptions PO; PO.bDryRun = true;
	Props.GenerateWithOptions(MakePackage(), C2, PO, &PropRes);
	bool bNoEnvCategory = true;
	for (const TPair<FName, int32>& P : PropRes.PlanStats.KeptPerCategory) { bNoEnvCategory &= (P.Key != FName(TEXT("Tree")) && P.Key != FName(TEXT("Streetlight"))); }
	TestTrue(TEXT("Props run never plans environment-domain categories"), bNoEnvCategory);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidDressingReplacementHookTest,
	"VOID.WorldBuilder.Environment.Hooks.BucketMeshSwapKeepsInstances",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidDressingReplacementHookTest::RunTest(const FString& Parameters)
{
	using namespace VoidEnvTestPrivate;
	UWorld* World = GetTestWorld();
	if (!TestNotNull(TEXT("Editor world should be available"), World)) { return false; }
	DestroyAllDressing(World);

	FVoidEnvironmentGenerator Env;
	FVoidGenerationContext Ctx; Ctx.TargetWorld = World;
	FVoidDressingOptions Opt; Opt.DensityScaleOverride = 2.0f;
	Env.GenerateWithOptions(MakePackage(), Ctx, Opt);

	AVoidDressingActor* Found = nullptr; const FVoidDressingBucketInfo* Bucket = nullptr;
	for (TActorIterator<AVoidDressingActor> It(World); It && !Bucket; ++It)
	{
		for (const FVoidDressingBucketInfo& B : It->Buckets) { if (B.bPlaceholder && B.PartIndex == 0 && B.Component) { Found = *It; Bucket = &B; break; } }
	}
	if (!TestNotNull(TEXT("A placeholder bucket exists"), Bucket)) { DestroyAllDressing(World); return false; }

	TestTrue(TEXT("Component carries the VOID.Placeholder tag"), Bucket->Component->ComponentTags.Contains(FName(TEXT("VOID.Placeholder"))));
	TestTrue(TEXT("Component carries a category tag"), Bucket->Component->ComponentTags.Contains(FName(*FString::Printf(TEXT("VOID.Category.%s"), *Bucket->Category.ToString()))));
	const int32 Count = Bucket->Component->GetInstanceCount();
	UStaticMesh* Mesh = Bucket->Component->GetStaticMesh();
	TestTrue(TEXT("Swapping the mesh succeeds"), Found->SetBucketMesh(Bucket->Category, Bucket->SlotId, Mesh, 0));
	TestEqual(TEXT("Swapping the mesh keeps instance count"), Bucket->Component->GetInstanceCount(), Count);
	TestFalse(TEXT("Placeholder tag is removed after swap"), Bucket->Component->ComponentTags.Contains(FName(TEXT("VOID.Placeholder"))));
	DestroyAllDressing(World);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
