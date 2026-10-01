// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Misc/AutomationTest.h"
#include "Interfaces/IVoidValidator.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	class FFakeValidator : public IVoidValidator
	{
	public:
		FFakeValidator(FName InId, EVoidValidationStage InStage, int32 InOrder, EVoidValidationSeverity InEmit)
			: Id(InId), Stage(InStage), Order(InOrder), Emit(InEmit) {}
		virtual FName GetValidatorId() const override { return Id; }
		virtual FName GetSubsystem() const override { return TEXT("Fake"); }
		virtual bool AppliesToStage(EVoidValidationStage S) const override { return S == Stage; }
		virtual int32 GetOrder() const override { return Order; }
		virtual void Validate(EVoidValidationStage, const FVoidValidationInput&, FVoidValidationContext& Ctx) const override
		{
			Ctx.Emit(Emit, FName(*FString::Printf(TEXT("VOID.Fake.%s"), *Id.ToString())), TEXT("fake issue"), TEXT("obj"), TEXT("path"), TEXT("fix"));
		}
	private:
		FName Id; EVoidValidationStage Stage; int32 Order; EVoidValidationSeverity Emit;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidReportSeverityTest, "VOID.WorldBuilder.Core.Report.SeveritySemantics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidReportSeverityTest::RunTest(const FString&)
{
	FVoidValidationReport R;
	R.bIsValid = true;
	R.AddInfo(TEXT("i"));
	R.AddWarning(TEXT("w"));
	TestTrue(TEXT("Info+Warning never invalidate"), R.bIsValid);
	TestEqual(TEXT("NumBlocking"), R.NumBlocking(), 0);

	FVoidValidationIssue Issue(EVoidValidationSeverity::Error, TEXT("e"), TEXT("p"), TEXT("VOID.X"), TEXT("fix"));
	Issue.WithSubsystem(TEXT("Road")).WithObject(TEXT("road_1")).WithSource(TEXT("file.json")).WithLocation(FVector(1, 2, 3));
	R.AddIssue(Issue);
	TestFalse(TEXT("AddIssue(Error) invalidates"), R.bIsValid);
	TestEqual(TEXT("NumBlocking"), R.NumBlocking(), 1);
	TestTrue(TEXT("Location kept"), R.Issues.Last().bHasLocation && R.Issues.Last().Location == FVector(1, 2, 3));

	FVoidValidationReport Fatal;
	Fatal.bIsValid = true;
	Fatal.AddFatal(TEXT("f"));
	TestTrue(TEXT("Fatal is blocking"), Fatal.HasFatalIssue() && Fatal.NumBlocking() == 1);
	// Pre-existing AddFatal/AddError (Phase 1) do NOT touch bIsValid; consumers must recompute. See INTEGRATION_RISK_REGISTER R-12.
	TestTrue(TEXT("Legacy AddFatal leaves bIsValid alone"), Fatal.bIsValid);
	Fatal.RecomputeValidity();
	TestFalse(TEXT("RecomputeValidity fixes it"), Fatal.bIsValid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidReportMergeTest, "VOID.WorldBuilder.Core.Report.MergeAttributesLegacyIssues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidReportMergeTest::RunTest(const FString&)
{
	// A "legacy" report as produced by Phase 2/3 code: no subsystem, no source.
	FVoidValidationReport Legacy;
	Legacy.bIsValid = true;
	Legacy.AddError(TEXT("legacy error"), TEXT("district.roads[0]"), TEXT("VOID.RoadGen.X"), TEXT("fix"));

	FVoidValidationReport Target;
	Target.bIsValid = true;
	Target.AddWarning(TEXT("already here"));
	Target.Merge(Legacy, TEXT("Road"), TEXT("Generator:Road"));

	TestEqual(TEXT("Issue count"), Target.Issues.Num(), 2);
	TestEqual(TEXT("Subsystem inherited"), Target.Issues[1].Subsystem, FName(TEXT("Road")));
	TestEqual(TEXT("Source inherited"), Target.Issues[1].Source, FString(TEXT("Generator:Road")));
	TestFalse(TEXT("Validity recomputed to false"), Target.bIsValid);

	FVoidValidationReport Explicit;
	Explicit.bIsValid = true;
	FVoidValidationIssue I(EVoidValidationSeverity::Warning, TEXT("w"), FString(), TEXT("VOID.W"), FString());
	I.Subsystem = TEXT("Building");
	Explicit.AddIssue(I);
	Target.Merge(Explicit, TEXT("Road"));
	TestEqual(TEXT("Existing subsystem is not overwritten"), Target.Issues.Last().Subsystem, FName(TEXT("Building")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidContextThrottleTest, "VOID.WorldBuilder.Core.Context.ThrottlesButStillInvalidates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidContextThrottleTest::RunTest(const FString&)
{
	FVoidValidationOptions Options;
	Options.MaxIssuesPerCode = 3;
	FVoidValidationReport R;
	R.bIsValid = true;
	FVoidValidationContext Ctx(R, Options, TEXT("Road"), TEXT("src"));
	for (int32 i = 0; i < 10; ++i) { Ctx.Warn(TEXT("VOID.Spam"), TEXT("w")); }
	TestEqual(TEXT("Only 3 stored"), R.Issues.Num(), 3);
	TestTrue(TEXT("Warnings do not invalidate"), R.bIsValid);

	for (int32 i = 0; i < 10; ++i) { Ctx.Error(TEXT("VOID.Bad"), TEXT("e")); }
	TestEqual(TEXT("3 warnings + 3 errors stored"), R.Issues.Num(), 6);
	TestFalse(TEXT("A suppressed error still invalidates"), R.bIsValid);

	Ctx.FlushSuppressionSummary();
	TestEqual(TEXT("One summary Info per throttled code"), R.NumInfo(), 2);
	Ctx.FlushSuppressionSummary();
	TestEqual(TEXT("Flush is idempotent"), R.NumInfo(), 2);
	TestEqual(TEXT("Subsystem stamped"), R.Issues[0].Subsystem, FName(TEXT("Road")));
	TestEqual(TEXT("Source stamped"), R.Issues[0].Source, FString(TEXT("src")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidValidatorRegistryTest, "VOID.WorldBuilder.Core.ValidatorRegistry.RegistrationOrderAndDuplicates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidValidatorRegistryTest::RunTest(const FString&)
{
	AddExpectedError(TEXT("already registered"), EAutomationExpectedErrorFlags::Contains, 1); // the deliberate duplicate below logs an Error
	FVoidValidatorRegistry Registry; // isolated instance
	TestTrue(TEXT("first registration accepted"), Registry.Register(MakeShared<FFakeValidator>(TEXT("B"), EVoidValidationStage::PostRoad, 20, EVoidValidationSeverity::Warning)));
	TestTrue(TEXT("second accepted"), Registry.Register(MakeShared<FFakeValidator>(TEXT("A"), EVoidValidationStage::PostRoad, 20, EVoidValidationSeverity::Warning)));
	TestTrue(TEXT("third accepted"), Registry.Register(MakeShared<FFakeValidator>(TEXT("C"), EVoidValidationStage::PostRoad, 5, EVoidValidationSeverity::Warning)));
	TestFalse(TEXT("duplicate id rejected"), Registry.Register(MakeShared<FFakeValidator>(TEXT("A"), EVoidValidationStage::Final, 0, EVoidValidationSeverity::Error)));
	TestEqual(TEXT("duplicate recorded"), Registry.GetRejectedDuplicateIds().Num(), 1);
	TestEqual(TEXT("original kept"), Registry.Num(), 3);

	const TArray<TSharedRef<IVoidValidator>> ForRoad = Registry.GetForStage(EVoidValidationStage::PostRoad);
	TestEqual(TEXT("three apply to PostRoad"), ForRoad.Num(), 3);
	TestEqual(TEXT("order: lowest Order first"), ForRoad[0]->GetValidatorId(), FName(TEXT("C")));
	TestEqual(TEXT("order: ties broken by id"), ForRoad[1]->GetValidatorId(), FName(TEXT("A")));
	TestEqual(TEXT("then B"), ForRoad[2]->GetValidatorId(), FName(TEXT("B")));
	TestEqual(TEXT("none for Final"), Registry.GetForStage(EVoidValidationStage::Final).Num(), 0);

	FVoidValidationReport Report;
	Report.bIsValid = true;
	Registry.RunStage(EVoidValidationStage::PostRoad, FVoidValidationInput(), FVoidValidationOptions(), Report);
	TestEqual(TEXT("each validator emitted once"), Report.Issues.Num(), 3);
	TestEqual(TEXT("subsystem stamped from validator"), Report.Issues[0].Subsystem, FName(TEXT("Fake")));
	TestTrue(TEXT("source names the validator"), Report.Issues[0].Source.Contains(TEXT("Validator:C")));

	Registry.Unregister(TEXT("C"));
	TestEqual(TEXT("unregister works"), Registry.Num(), 2);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
