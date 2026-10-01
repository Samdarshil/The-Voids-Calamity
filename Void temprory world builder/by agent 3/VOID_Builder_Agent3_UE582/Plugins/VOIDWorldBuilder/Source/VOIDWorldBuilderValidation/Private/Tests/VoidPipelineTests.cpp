// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Misc/AutomationTest.h"
#include "VoidValidationPipeline.h"
#include "VoidGeneratorContractAuditor.h"
#include "VoidGeneratedWorldValidator.h"
#include "VoidValidationReportExporter.h"
#include "VoidGeneratorRegistry.h"
#include "VoidPackageValidators.h"
#include "VoidTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace VoidTest;

namespace
{
	/** Records the order generators ran in, can fail, and can leave its own report/log behind like the real Road generator does. */
	class FFakeGenerator : public IVoidGenerator
	{
	public:
		FFakeGenerator(FName InId, TArray<FName>& InRunOrder, bool bInSucceed = true) : Id(InId), RunOrder(InRunOrder), bSucceed(bInSucceed) {}
		virtual FName GetGeneratorId() const override { return Id; }
		virtual bool Generate(const FVoidDesignPackage&, FVoidGenerationContext& Ctx) override
		{
			RunOrder.Add(Id);
			Ctx.Log(FString::Printf(TEXT("%s running"), *Id.ToString()));
			// Like FVoidRoadGenerator: writes (overwrites) the single report slot.
			Ctx.GenerationValidationReport = FVoidValidationReport();
			Ctx.GenerationValidationReport.bIsValid = true;
			Ctx.GenerationValidationReport.AddWarning(FString::Printf(TEXT("warning from %s"), *Id.ToString()), TEXT("some.path"), FName(*FString::Printf(TEXT("VOID.Fake.%s"), *Id.ToString())));
			if (!bSucceed) { Ctx.Log(TEXT("could not build anything")); }
			return bSucceed;
		}
	private:
		FName Id; TArray<FName>& RunOrder; bool bSucceed;
	};

	class FFakeStageValidator : public IVoidValidator
	{
	public:
		FFakeStageValidator(FName InId, EVoidValidationStage InStage, bool bInFail) : Id(InId), Stage(InStage), bFail(bInFail) {}
		virtual FName GetValidatorId() const override { return Id; }
		virtual FName GetSubsystem() const override { return TEXT("Fake"); }
		virtual bool AppliesToStage(EVoidValidationStage S) const override { return S == Stage; }
		virtual void Validate(EVoidValidationStage, const FVoidValidationInput&, FVoidValidationContext& Ctx) const override
		{
			if (bFail) { Ctx.Error(TEXT("VOID.Fake.StageError"), TEXT("stage validator says no"), TEXT("obj"), TEXT("path"), TEXT("fix")); }
		}
	private:
		FName Id; EVoidValidationStage Stage; bool bFail;
	};

	FVoidPipelineOptions IsolatedOptions(const FVoidGeneratorRegistry& G, const FVoidValidatorRegistry& V)
	{
		FVoidPipelineOptions O;
		O.GeneratorRegistry = &G;
		O.ValidatorRegistry = &V;
		return O;
	}

	// A non-null World pointer is required by the runner's precondition; fakes never dereference it.
	UWorld* FakeWorld() { return reinterpret_cast<UWorld*>(static_cast<uintptr_t>(0x10)); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidPipelineOrderTest, "VOID.WorldBuilder.Validation.Pipeline.RunsInCanonicalOrderAndKeepsEveryGeneratorsReport",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidPipelineOrderTest::RunTest(const FString&)
{
	FVoidGeneratorRegistry Generators;
	FVoidValidatorRegistry Validators;
	TArray<FName> Ran;
	// Register in scrambled order: the pipeline must not depend on registration or TMap order.
	for (const TCHAR* Id : { TEXT("Metro"), TEXT("Road"), TEXT("Props"), TEXT("Building"), TEXT("District"), TEXT("Lighting"), TEXT("Environment") })
	{
		Generators.RegisterGenerator(MakeShared<FFakeGenerator>(FName(Id), Ran));
	}
	FVoidDesignPackage P = MakePackage();
	P.District.Roads.Add(MakeRoad(TEXT("a"), { FVector2D(0, 0), FVector2D(1000, 0) }));

	const FVoidPipelineResult Result = FVoidValidationPipeline::Run(FVoidPipelineDefinition::MakeDefault(true), &P, FakeWorld(), IsolatedOptions(Generators, Validators));

	const TArray<FName> Expected = { TEXT("Road"), TEXT("District"), TEXT("Building"), TEXT("Environment"), TEXT("Lighting"), TEXT("Props"), TEXT("Metro") };
	TestTrue(TEXT("Generators ran in canonical order"), Ran == Expected);
	TestTrue(TEXT("Run completed"), Result.bCompleted && Result.AbortedAtStep.IsNone());
	for (const FName& G : Expected)
	{
		TestTrue(*FString::Printf(TEXT("Report from '%s' survived (Road's overwrite-the-slot behaviour cannot erase it)"), *G.ToString()), HasCode(Result.Report, *FString::Printf(TEXT("VOID.Fake.%s"), *G.ToString())));
	}
	const FVoidValidationIssue* Issue = Result.Report.Issues.FindByPredicate([](const FVoidValidationIssue& I) { return I.ErrorCode == FName(TEXT("VOID.Fake.Building")); });
	TestTrue(TEXT("Legacy issue attributed to its generator"), Issue && Issue->Subsystem == FName(TEXT("Building")) && Issue->Source == TEXT("Generator:Building"));
	TestTrue(TEXT("Warnings never invalidate"), Result.Report.bIsValid && Result.WasSuccessful());
	TestTrue(TEXT("Generator logs captured per step"), Result.Steps.ContainsByPredicate([](const FVoidPipelineStepResult& S) { return S.StepId == FName(TEXT("Generate.Metro")) && S.LogLines.Num() == 1; }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidPipelineFailureTest, "VOID.WorldBuilder.Validation.Pipeline.FailureModesAbortWithExplicitErrors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidPipelineFailureTest::RunTest(const FString&)
{
	FVoidDesignPackage P = MakePackage();
	P.District.Roads.Add(MakeRoad(TEXT("a"), { FVector2D(0, 0), FVector2D(1000, 0) }));

	{	// generator returns false
		FVoidGeneratorRegistry G; FVoidValidatorRegistry V; TArray<FName> Ran;
		G.RegisterGenerator(MakeShared<FFakeGenerator>(TEXT("Road"), Ran, false));
		G.RegisterGenerator(MakeShared<FFakeGenerator>(TEXT("District"), Ran));
		const FVoidPipelineResult R = FVoidValidationPipeline::Run(FVoidPipelineDefinition::MakeDefault(), &P, FakeWorld(), IsolatedOptions(G, V));
		TestTrue(TEXT("GeneratorFailed ERROR raised"), HasCodeAt(R.Report, TEXT("VOID.Pipeline.GeneratorFailed"), EVoidValidationSeverity::Error));
		const FVoidValidationIssue* I = R.Report.Issues.FindByPredicate([](const FVoidValidationIssue& X) { return X.ErrorCode == FName(TEXT("VOID.Pipeline.GeneratorFailed")); });
		TestTrue(TEXT("...and carries the generator's own last log line"), I && I->Message.Contains(TEXT("could not build anything")));
		TestFalse(TEXT("Later generators did not run"), Ran.Contains(FName(TEXT("District"))));
		TestEqual(TEXT("Aborted at the failing step"), R.AbortedAtStep, FName(TEXT("Generate.Road")));
		TestFalse(TEXT("Not successful"), R.WasSuccessful());
		TestTrue(TEXT("Final validation still ran on the partial world"), R.Steps.ContainsByPredicate([](const FVoidPipelineStepResult& S) { return S.StepId == FName(TEXT("Validate.Final")) && S.Status != EVoidPipelineStepStatus::Skipped; }));
	}
	{	// required generator missing vs optional generator missing
		FVoidGeneratorRegistry G; FVoidValidatorRegistry V; TArray<FName> Ran;
		const FVoidPipelineResult NoRoad = FVoidValidationPipeline::Run(FVoidPipelineDefinition::MakeDefault(), &P, FakeWorld(), IsolatedOptions(G, V));
		TestTrue(TEXT("Missing required Road generator is an ERROR"), HasCodeAt(NoRoad.Report, TEXT("VOID.Pipeline.RequiredGeneratorMissing"), EVoidValidationSeverity::Error));

		G.RegisterGenerator(MakeShared<FFakeGenerator>(TEXT("Road"), Ran));
		const FVoidPipelineResult Partial = FVoidValidationPipeline::Run(FVoidPipelineDefinition::MakeDefault(), &P, FakeWorld(), IsolatedOptions(G, V));
		TestTrue(TEXT("Missing optional generators are WARNINGS"), HasCodeAt(Partial.Report, TEXT("VOID.Pipeline.GeneratorSkipped"), EVoidValidationSeverity::Warning));
		TestTrue(TEXT("...and the run still succeeds"), Partial.WasSuccessful());
		TestEqual(TEXT("Six optional generators reported skipped"), CountCode(Partial.Report, TEXT("VOID.Pipeline.GeneratorSkipped")), 6);

		const FVoidPipelineResult Strict = FVoidValidationPipeline::Run(FVoidPipelineDefinition::MakeDefault(true), &P, FakeWorld(), IsolatedOptions(G, V));
		TestTrue(TEXT("Final-integration mode requires every generator"), HasCodeAt(Strict.Report, TEXT("VOID.Pipeline.RequiredGeneratorMissing"), EVoidValidationSeverity::Error));
	}
	{	// blocking import-stage validation prevents any generation
		FVoidGeneratorRegistry G; FVoidValidatorRegistry V; TArray<FName> Ran;
		G.RegisterGenerator(MakeShared<FFakeGenerator>(TEXT("Road"), Ran));
		V.Register(MakeShared<FFakeStageValidator>(TEXT("VOID.Fake.Import"), EVoidValidationStage::PostImport, true));
		const FVoidPipelineResult R = FVoidValidationPipeline::Run(FVoidPipelineDefinition::MakeDefault(), &P, FakeWorld(), IsolatedOptions(G, V));
		TestEqual(TEXT("No generator ran after a failed import validation"), Ran.Num(), 0);
		TestTrue(TEXT("StageValidationFailed raised"), HasCodeAt(R.Report, TEXT("VOID.Pipeline.StageValidationFailed"), EVoidValidationSeverity::Error));
		TestTrue(TEXT("The validator's own error is kept"), HasCode(R.Report, TEXT("VOID.Fake.StageError")));

		FVoidPipelineOptions KeepGoing = IsolatedOptions(G, V);
		KeepGoing.bStopOnError = false;
		FVoidGeneratorRegistry G2; TArray<FName> Ran2; G2.RegisterGenerator(MakeShared<FFakeGenerator>(TEXT("Road"), Ran2));
		KeepGoing.GeneratorRegistry = &G2;
		FVoidValidationPipeline::Run(FVoidPipelineDefinition::MakeDefault(), &P, FakeWorld(), KeepGoing);
		TestEqual(TEXT("bStopOnError=false lets generation continue"), Ran2.Num(), 1);
	}
	{	// validate-only never generates
		FVoidGeneratorRegistry G; FVoidValidatorRegistry V; TArray<FName> Ran;
		G.RegisterGenerator(MakeShared<FFakeGenerator>(TEXT("Road"), Ran));
		FVoidPipelineOptions O = IsolatedOptions(G, V);
		O.bValidateOnly = true;
		const FVoidPipelineResult R = FVoidValidationPipeline::Run(FVoidPipelineDefinition::MakeDefault(), &P, nullptr, O);
		TestEqual(TEXT("Validate-only spawns nothing"), Ran.Num(), 0);
		TestTrue(TEXT("...and needs no world"), R.WasSuccessful());
	}
	{	// missing inputs
		FVoidGeneratorRegistry G; FVoidValidatorRegistry V; TArray<FName> Ran;
		G.RegisterGenerator(MakeShared<FFakeGenerator>(TEXT("Road"), Ran));
		const FVoidPipelineResult R = FVoidValidationPipeline::Run(FVoidPipelineDefinition::MakeDefault(), &P, nullptr, IsolatedOptions(G, V));
		TestTrue(TEXT("No world => explicit MissingInput ERROR"), HasCodeAt(R.Report, TEXT("VOID.Pipeline.MissingInput"), EVoidValidationSeverity::Error));
		TestEqual(TEXT("Nothing ran"), Ran.Num(), 0);
	}
	{	// cancellation
		FVoidGeneratorRegistry G; FVoidValidatorRegistry V; TArray<FName> Ran;
		G.RegisterGenerator(MakeShared<FFakeGenerator>(TEXT("Road"), Ran));
		G.RegisterGenerator(MakeShared<FFakeGenerator>(TEXT("District"), Ran));
		FVoidPipelineOptions O = IsolatedOptions(G, V);
		O.IsCancellationRequested = []() { return true; };
		const FVoidPipelineResult R = FVoidValidationPipeline::Run(FVoidPipelineDefinition::MakeDefault(), &P, FakeWorld(), O);
		TestTrue(TEXT("Cancellation is reported and stops the run"), HasCode(R.Report, TEXT("VOID.Pipeline.Cancelled")) && !Ran.Contains(FName(TEXT("District"))));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidPipelineImportGateTest, "VOID.WorldBuilder.Validation.Pipeline.FailedImportBlocksGeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidPipelineImportGateTest::RunTest(const FString&)
{
	FVoidGeneratorRegistry G; FVoidValidatorRegistry V; TArray<FName> Ran;
	G.RegisterGenerator(MakeShared<FFakeGenerator>(TEXT("Road"), Ran));

	FVoidImportResult Import;
	Import.Package = MakePackage();
	Import.ValidationReport.bIsValid = false;
	Import.ValidationReport.AddError(TEXT("bad thing"), TEXT("district.roads[0]"), TEXT("VOID.Import.Bad"), TEXT("fix it"));
	Import.Context.SourceDescription = TEXT("D:/x.json");

	const FVoidPipelineResult R = FVoidValidationPipeline::RunFromImport(FVoidPipelineDefinition::MakeDefault(), Import, FakeWorld(), IsolatedOptions(G, V));
	TestEqual(TEXT("No generator ran"), Ran.Num(), 0);
	TestTrue(TEXT("ImportFailed raised"), HasCodeAt(R.Report, TEXT("VOID.Pipeline.ImportFailed"), EVoidValidationSeverity::Error));
	const FVoidValidationIssue* I = R.Report.Issues.FindByPredicate([](const FVoidValidationIssue& X) { return X.ErrorCode == FName(TEXT("VOID.Import.Bad")); });
	TestTrue(TEXT("Importer issue kept, attributed to Import with its source file"), I && I->Subsystem == FName(TEXT("Import")) && I->Source == TEXT("D:/x.json"));
	TestEqual(TEXT("Aborted at Import"), R.AbortedAtStep, FName(TEXT("Import")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidContractAuditorTest, "VOID.WorldBuilder.Validation.Contract.RegistryAudit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidContractAuditorTest::RunTest(const FString&)
{
	AddExpectedWarning(TEXT("already registered"), EAutomationExpectedErrorFlags::Contains, 1);  // generator registry warns on the deliberate duplicate
	AddExpectedError(TEXT("already registered"), EAutomationExpectedErrorFlags::Contains, 1);    // validator registry errors on the deliberate duplicate
	FVoidGeneratorRegistry G; FVoidValidatorRegistry V; TArray<FName> Ran;
	G.RegisterGenerator(MakeShared<FFakeGenerator>(TEXT("Road"), Ran));
	G.RegisterGenerator(MakeShared<FFakeGenerator>(TEXT("Road"), Ran));               // two modules claim "Road"
	G.RegisterGenerator(MakeShared<FFakeGenerator>(TEXT("BuildingGenerator"), Ran));   // decorated id
	G.RegisterGenerator(MakeShared<FFakeGenerator>(TEXT("Weather"), Ran));             // unknown id
	G.RegisterGenerator(MakeShared<FFakeGenerator>(TEXT("Bad Id"), Ran));              // whitespace
	V.Register(MakeShared<FFakeStageValidator>(TEXT("VOID.Fake.V"), EVoidValidationStage::PostRoad, false));
	V.Register(MakeShared<FFakeStageValidator>(TEXT("VOID.Fake.V"), EVoidValidationStage::PostRoad, false)); // duplicate validator

	FVoidValidationReport R; R.bIsValid = true;
	FVoidValidationOptions O;
	FVoidValidationContext Ctx(R, O);
	FVoidGeneratorContractAuditor::Audit(G, V, Ctx);

	TestTrue(TEXT("Duplicate generator id => ERROR"), HasCodeAt(R, TEXT("VOID.Contract.DuplicateGeneratorId"), EVoidValidationSeverity::Error));
	TestTrue(TEXT("Duplicate validator id => ERROR"), HasCodeAt(R, TEXT("VOID.Contract.DuplicateValidatorId"), EVoidValidationSeverity::Error));
	TestTrue(TEXT("Decorated id => WARNING with a rename suggestion"), R.Issues.ContainsByPredicate([](const FVoidValidationIssue& I) { return I.ErrorCode == FName(TEXT("VOID.Contract.NonCanonicalGeneratorId")) && I.SuggestedFix.Contains(TEXT("'Building'")); }));
	TestTrue(TEXT("Unknown id => INFO"), HasCodeAt(R, TEXT("VOID.Contract.UnexpectedGenerator"), EVoidValidationSeverity::Info));
	TestTrue(TEXT("Whitespace id => ERROR"), HasCodeAt(R, TEXT("VOID.Contract.InvalidGeneratorId"), EVoidValidationSeverity::Error));
	TestEqual(TEXT("Six canonical generators are absent (Road is present)"), CountCode(R, TEXT("VOID.Contract.GeneratorNotRegistered")), 6);
	TestFalse(TEXT("Road has a PostRoad validator, so no StageWithoutValidator note is raised"), HasCode(R, TEXT("VOID.Contract.StageWithoutValidator")));
	TestEqual(TEXT("Canonical ids in pipeline order"), FVoidGeneratorContractAuditor::GetExpectedGeneratorIds().Num(), 7);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidWorldSnapshotTest, "VOID.WorldBuilder.Validation.World.SnapshotChecks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidWorldSnapshotTest::RunTest(const FString&)
{
	FVoidDesignPackage P = MakePackage();
	P.District.Roads.Add(MakeRoad(TEXT("r1"), { FVector2D(0, 0), FVector2D(1000, 0) }));
	P.District.Roads.Add(MakeRoad(TEXT("r2"), { FVector2D(0, 500), FVector2D(1000, 500) }));
	P.District.Roads.Add(MakeRoad(TEXT("r3"), { FVector2D(0, 900), FVector2D(1000, 900) }));

	auto Road = [](const TCHAR* Id, int32 Sections = 1)
	{
		FVoidActorSnapshot A;
		A.ActorName = FString::Printf(TEXT("VoidRoad_%s"), Id); A.ClassName = TEXT("VoidRoadActor");
		A.GeneratorId = TEXT("Road"); A.Role = TEXT("Road"); A.ObjectId = Id; A.bTagged = true; A.bVoidClass = true;
		A.NumMeshSections = Sections; A.NumSplinePoints = 2; A.Location = FVector(100, 100, 0);
		return A;
	};

	FVoidWorldSnapshot S;
	S.Actors.Add(Road(TEXT("r1")));
	S.Actors.Add(Road(TEXT("r1")));                                   // duplicate of r1
	FVoidActorSnapshot Empty = Road(TEXT("r2"), 0);                   // empty mesh
	S.Actors.Add(Empty);
	// r3 is missing entirely
	S.Actors.Add(Road(TEXT("stale_road")));                           // not in package
	FVoidActorSnapshot NaNActor = Road(TEXT("r1")); NaNActor.bTransformFinite = false; NaNActor.ObjectId = TEXT("nan_one");
	S.Actors.Add(NaNActor);
	FVoidActorSnapshot Junction;
	Junction.ActorName = TEXT("VoidJunction_0"); Junction.GeneratorId = TEXT("Road"); Junction.Role = TEXT("Junction"); Junction.bTagged = true; Junction.NumMeshSections = 1;
	Junction.ReferencedIds = { TEXT("r1"), TEXT("ghost_road") };
	S.Actors.Add(Junction);
	FVoidActorSnapshot Floating;
	Floating.ActorName = TEXT("Tower"); Floating.GeneratorId = TEXT("Building"); Floating.ObjectId = TEXT("tower_1"); Floating.bTagged = true;
	Floating.Bounds = FBox(FVector(0, 0, 5000), FVector(100, 100, 6000)); Floating.bBoundsValid = true;
	S.Actors.Add(Floating);
	FVoidActorSnapshot BadMesh = Road(TEXT("r1"), 1); BadMesh.ObjectId = TEXT("marker_road"); BadMesh.bInstancesMissingMesh = true; BadMesh.NumInstances = 4;
	S.Actors.Add(BadMesh);

	FVoidValidationInput Input;
	Input.Package = &P;
	Input.ExecutedGenerators.Add(TEXT("Road"));
	Input.ExecutedGenerators.Add(TEXT("Lighting"));                   // ran, but produced no tagged actors

	FVoidValidationReport R; R.bIsValid = true;
	FVoidValidationOptions O;
	FVoidValidationContext Ctx(R, O, TEXT("World"));
	FVoidGeneratedWorldValidator::ValidateSnapshot(EVoidValidationStage::Final, S, Input, Ctx);

	TestTrue(TEXT("Duplicate actor claim is an ERROR"), HasCodeAt(R, TEXT("VOID.World.DuplicateActor"), EVoidValidationSeverity::Error));
	TestTrue(TEXT("Missing generated road is an ERROR naming the id"), R.Issues.ContainsByPredicate([](const FVoidValidationIssue& I) { return I.ErrorCode == FName(TEXT("VOID.World.MissingActor")) && I.ObjectId == TEXT("r3"); }));
	TestFalse(TEXT("Present road r1 is not reported missing"), R.Issues.ContainsByPredicate([](const FVoidValidationIssue& I) { return I.ErrorCode == FName(TEXT("VOID.World.MissingActor")) && I.ObjectId == TEXT("r1"); }));
	TestTrue(TEXT("Empty procedural mesh is an ERROR"), HasCodeAt(R, TEXT("VOID.World.EmptyGeometry"), EVoidValidationSeverity::Error));
	TestTrue(TEXT("Orphan actor is a WARNING"), R.Issues.ContainsByPredicate([](const FVoidValidationIssue& I) { return I.ErrorCode == FName(TEXT("VOID.World.OrphanActor")) && I.ObjectId == TEXT("stale_road") && I.Severity == EVoidValidationSeverity::Warning; }));
	TestTrue(TEXT("NaN transform is an ERROR"), HasCodeAt(R, TEXT("VOID.World.InvalidTransform"), EVoidValidationSeverity::Error));
	TestTrue(TEXT("Junction pointing at a nonexistent road is an ERROR"), R.Issues.ContainsByPredicate([](const FVoidValidationIssue& I) { return I.ErrorCode == FName(TEXT("VOID.World.DanglingReference")) && I.Message.Contains(TEXT("ghost_road")); }));
	TestFalse(TEXT("...but the valid reference is fine"), R.Issues.ContainsByPredicate([](const FVoidValidationIssue& I) { return I.ErrorCode == FName(TEXT("VOID.World.DanglingReference")) && I.Message.Contains(TEXT("'r1'")); }));
	TestTrue(TEXT("Floating building is a WARNING"), HasCodeAt(R, TEXT("VOID.World.FloatingGeometry"), EVoidValidationSeverity::Warning));
	TestTrue(TEXT("Missing marker mesh asset is an ERROR"), HasCodeAt(R, TEXT("VOID.World.MissingAsset"), EVoidValidationSeverity::Error));
	TestTrue(TEXT("Generator that ran but left nothing is reported"), R.Issues.ContainsByPredicate([](const FVoidValidationIssue& I) { return I.ErrorCode == FName(TEXT("VOID.World.GeneratorProducedNothing")) && I.ObjectId == TEXT("Lighting"); }));

	// Road generator ran but the world is empty.
	FVoidValidationReport R2; R2.bIsValid = true;
	FVoidValidationContext Ctx2(R2, O, TEXT("World"));
	FVoidGeneratedWorldValidator::ValidateSnapshot(EVoidValidationStage::PostRoad, FVoidWorldSnapshot(), Input, Ctx2);
	TestTrue(TEXT("Road generator ran but produced nothing => ERROR"), HasCodeAt(R2, TEXT("VOID.World.GeneratorProducedNothing"), EVoidValidationSeverity::Error));
	TestEqual(TEXT("...plus one MissingActor per expected road"), CountCode(R2, TEXT("VOID.World.MissingActor")), 3);
	TestFalse(TEXT("PostRoad does not run generic Final-only checks"), HasCode(R2, TEXT("VOID.World.UntaggedActors")));

	// Nothing executed => no expectations => silent.
	FVoidValidationInput NothingRan;
	NothingRan.Package = &P;
	FVoidValidationReport R3; R3.bIsValid = true;
	FVoidValidationContext Ctx3(R3, O, TEXT("World"));
	FVoidGeneratedWorldValidator::ValidateSnapshot(EVoidValidationStage::Final, FVoidWorldSnapshot(), NothingRan, Ctx3);
	TestEqual(TEXT("A pipeline that generated nothing yet reports no missing-actor noise"), R3.Issues.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidReportExportTest, "VOID.WorldBuilder.Validation.Report.ExportContainsEveryDebuggingField",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidReportExportTest::RunTest(const FString&)
{
	FVoidValidationReport R; R.bIsValid = true;
	R.AddIssue(FVoidValidationIssue(EVoidValidationSeverity::Info, TEXT("just info"), FString(), TEXT("VOID.Z.Info"), FString()));
	FVoidValidationIssue Err(EVoidValidationSeverity::Error, TEXT("Road 'x' is broken"), TEXT("district.roads[3]"), TEXT("VOID.Road.Broken"), TEXT("Fix the points"));
	Err.WithSubsystem(TEXT("Road")).WithObject(TEXT("x")).WithSource(TEXT("Generator:Road")).WithLocation(FVector(10, 20, 30));
	R.AddIssue(Err);
	R.AddIssue(FVoidValidationIssue(EVoidValidationSeverity::Warning, TEXT("suspicious"), FString(), TEXT("VOID.A.Warn"), FString()));

	const TArray<const FVoidValidationIssue*> Sorted = FVoidValidationReportExporter::GetSortedIssues(R);
	TestTrue(TEXT("Most severe first, Info last"), Sorted[0]->Severity == EVoidValidationSeverity::Error && Sorted.Last()->Severity == EVoidValidationSeverity::Info);

	const FString Text = FVoidValidationReportExporter::ToText(R, TEXT("T"));
	for (const TCHAR* Needle : { TEXT("ERROR"), TEXT("VOID.Road.Broken"), TEXT("{Road}"), TEXT("object: x"), TEXT("source: Generator:Road"), TEXT("path:   district.roads[3]"), TEXT("(10, 20, 30)"), TEXT("Road 'x' is broken"), TEXT("fix:    Fix the points"), TEXT("FAIL") })
	{
		TestTrue(*FString::Printf(TEXT("Text report contains '%s'"), Needle), Text.Contains(Needle));
	}
	const FString Json = FVoidValidationReportExporter::ToJson(R, TEXT("T"));
	TestTrue(TEXT("JSON has structured fields"), Json.Contains(TEXT("\"subsystem\"")) && Json.Contains(TEXT("\"suggestedFix\"")) && Json.Contains(TEXT("\"location\"")) && Json.Contains(TEXT("\"objectId\"")));
	TestTrue(TEXT("Markdown table"), FVoidValidationReportExporter::ToMarkdown(R, TEXT("T")).Contains(TEXT("| ERROR | Road |")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
