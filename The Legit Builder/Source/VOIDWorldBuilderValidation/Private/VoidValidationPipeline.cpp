// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VoidValidationPipeline.h"
#include "VoidGeneratorContractAuditor.h"
#include "VoidGeneratorRegistry.h"
#include "VoidImportResult.h"
#include "VoidValidationLog.h"
#include "HAL/PlatformTime.h"

const TCHAR* FVoidPipelineResult::StatusToString(EVoidPipelineStepStatus Status)
{
	switch (Status)
	{
		case EVoidPipelineStepStatus::Passed:             return TEXT("PASSED");
		case EVoidPipelineStepStatus::PassedWithWarnings: return TEXT("PASSED (warnings)");
		case EVoidPipelineStepStatus::Failed:             return TEXT("FAILED");
		case EVoidPipelineStepStatus::Skipped:            return TEXT("SKIPPED");
		case EVoidPipelineStepStatus::NotRegistered:      return TEXT("NOT REGISTERED");
	}
	return TEXT("?");
}

FVoidPipelineDefinition FVoidPipelineDefinition::MakeDefault(bool bRequireAllGenerators)
{
	FVoidPipelineDefinition Def;
	auto AddValidate = [&Def](const TCHAR* Id, EVoidValidationStage Stage)
	{
		FVoidPipelineStep S; S.StepId = Id; S.Kind = EVoidPipelineStepKind::ValidateStage; S.Stage = Stage;
		Def.Steps.Add(S);
	};
	auto AddGenerate = [&Def, bRequireAllGenerators](const TCHAR* Generator, EVoidValidationStage Post, bool bAlwaysRequired)
	{
		FVoidPipelineStep G; G.StepId = FName(*FString::Printf(TEXT("Generate.%s"), Generator)); G.Kind = EVoidPipelineStepKind::Generate;
		G.GeneratorId = Generator; G.bRequired = bAlwaysRequired || bRequireAllGenerators;
		Def.Steps.Add(G);
		FVoidPipelineStep V; V.StepId = FName(*FString::Printf(TEXT("Validate.%s"), VoidValidationStageToString(Post))); V.Kind = EVoidPipelineStepKind::ValidateStage; V.Stage = Post;
		Def.Steps.Add(V);
	};

	AddValidate(TEXT("Validate.PostImport"), EVoidValidationStage::PostImport);
	AddGenerate(TEXT("Road"), EVoidValidationStage::PostRoad, true);
	AddGenerate(TEXT("District"), EVoidValidationStage::PostDistrict, false);
	AddGenerate(TEXT("Building"), EVoidValidationStage::PostBuilding, false);
	AddGenerate(TEXT("Environment"), EVoidValidationStage::PostEnvironment, false);
	AddGenerate(TEXT("Lighting"), EVoidValidationStage::PostLighting, false);
	AddGenerate(TEXT("Props"), EVoidValidationStage::PostProps, false);
	AddGenerate(TEXT("Metro"), EVoidValidationStage::PostMetro, false);
	AddValidate(TEXT("Validate.Final"), EVoidValidationStage::Final);
	return Def;
}

namespace
{
	/** Merge with attribution + de-duplication (a generator's own validator and ours may report the same fact). */
	void MergeDeduped(FVoidValidationReport& Dst, const FVoidValidationReport& Src, FName DefaultSubsystem, const FString& DefaultSource, TSet<FString>& Seen)
	{
		for (const FVoidValidationIssue& Issue : Src.Issues)
		{
			const FString Key = FString::Printf(TEXT("%s|%s|%s|%s"), *Issue.ErrorCode.ToString(), *Issue.ObjectId, *Issue.FieldPath, *Issue.Message);
			if (Seen.Contains(Key)) { continue; }
			Seen.Add(Key);
			FVoidValidationIssue Copy = Issue;
			if (Copy.Subsystem.IsNone()) { Copy.Subsystem = DefaultSubsystem; }
			if (Copy.Source.IsEmpty()) { Copy.Source = DefaultSource; }
			Dst.AddIssue(MoveTemp(Copy));
		}
	}

	void AddPipelineIssue(FVoidValidationReport& Report, EVoidValidationSeverity Sev, const TCHAR* Code, const FString& Msg, const FString& Object, const FString& Fix)
	{
		FVoidValidationIssue Issue(Sev, Msg, FString(), FName(Code), Fix);
		Issue.Subsystem = TEXT("Pipeline");
		Issue.Source = TEXT("FVoidValidationPipeline");
		Issue.ObjectId = Object;
		Report.AddIssue(MoveTemp(Issue));
	}
}

static FVoidPipelineResult RunImpl(const FVoidPipelineDefinition& Definition, const FVoidDesignPackage* Package, UWorld* World, const FVoidPipelineOptions& Options, const FVoidValidationReport* ImportReport, const FString& ImportSource);

FVoidPipelineResult FVoidValidationPipeline::RunFromImport(const FVoidPipelineDefinition& Definition, const FVoidImportResult& Import, UWorld* World, const FVoidPipelineOptions& Options)
{
	return RunImpl(Definition, &Import.Package, World, Options, &Import.ValidationReport, Import.Context.SourceDescription);
}

FVoidPipelineResult FVoidValidationPipeline::Run(const FVoidPipelineDefinition& Definition, const FVoidDesignPackage* Package, UWorld* World, const FVoidPipelineOptions& Options)
{
	return RunImpl(Definition, Package, World, Options, nullptr, FString());
}

static FVoidPipelineResult RunImpl(const FVoidPipelineDefinition& Definition, const FVoidDesignPackage* Package, UWorld* World, const FVoidPipelineOptions& Options, const FVoidValidationReport* ImportReport, const FString& ImportSource)
{
	FVoidPipelineResult Result;
	Result.Report.bIsValid = true;
	TSet<FString> Seen;

	const FVoidGeneratorRegistry& Generators = Options.GeneratorRegistry ? *Options.GeneratorRegistry : FVoidGeneratorRegistry::Get();
	const FVoidValidatorRegistry& Validators = Options.ValidatorRegistry ? *Options.ValidatorRegistry : FVoidValidatorRegistry::Get();

	FVoidValidationInput Input;
	Input.Package = Package;
	Input.World = World;
	Input.KnownDistrictIds = Options.KnownDistrictIds;

	if (Options.bAuditContracts)
	{
		FVoidValidationReport Audit;
		Audit.bIsValid = true;
		FVoidValidationContext Ctx(Audit, Options.Validation, TEXT("Contract"), TEXT("Registry audit"));
		FVoidGeneratorContractAuditor::Audit(Generators, Validators, Ctx);
		MergeDeduped(Result.Report, Audit, TEXT("Contract"), FString(), Seen);
	}

	bool bAborted = false;

	if (ImportReport)
	{
		// The importer's report goes first, attributed to "Import". A blocking import means the package must not be generated.
		FVoidValidationReport Ordered;
		Ordered.bIsValid = true;
		MergeDeduped(Ordered, *ImportReport, TEXT("Import"), ImportSource, Seen);
		MergeDeduped(Ordered, Result.Report, NAME_None, FString(), Seen);
		Result.Report = MoveTemp(Ordered);
		if (ImportReport->NumBlocking() > 0)
		{
			AddPipelineIssue(Result.Report, EVoidValidationSeverity::Error, TEXT("VOID.Pipeline.ImportFailed"),
				FString::Printf(TEXT("Import produced %d blocking issue(s); no generator will be run."), ImportReport->NumBlocking()), ImportSource, TEXT("Fix the import issues listed under subsystem 'Import', then re-import."));
			bAborted = true;
			Result.AbortedAtStep = TEXT("Import");
		}
	}

	for (const FVoidPipelineStep& Step : Definition.Steps)
	{
		FVoidPipelineStepResult StepResult;
		StepResult.StepId = Step.StepId;
		const double Start = FPlatformTime::Seconds();
		const int32 ErrorsBefore = Result.Report.NumBlocking();
		const int32 WarningsBefore = Result.Report.NumWarnings();

		const bool bIsFinalValidation = (Step.Kind == EVoidPipelineStepKind::ValidateStage && Step.Stage == EVoidValidationStage::Final);
		if (bAborted && !bIsFinalValidation)
		{
			StepResult.Status = EVoidPipelineStepStatus::Skipped;
			Result.Steps.Add(StepResult);
			continue;
		}

		if (Step.Kind == EVoidPipelineStepKind::ValidateStage)
		{
			FVoidValidationReport StageReport;
			StageReport.bIsValid = true;
			Validators.RunStage(Step.Stage, Input, Options.Validation, StageReport);
			MergeDeduped(Result.Report, StageReport, NAME_None, FString(), Seen);
			if (Result.Report.NumBlocking() > ErrorsBefore && Options.bStopOnError && !bIsFinalValidation)
			{
				AddPipelineIssue(Result.Report, EVoidValidationSeverity::Error, TEXT("VOID.Pipeline.StageValidationFailed"),
					FString::Printf(TEXT("Validation stage %s found %d blocking issue(s); generation cannot safely continue."), VoidValidationStageToString(Step.Stage), Result.Report.NumBlocking() - ErrorsBefore),
					VoidValidationStageToString(Step.Stage), TEXT("Fix the ERROR issues listed for this stage, then re-run."));
				bAborted = true;
				Result.AbortedAtStep = Step.StepId;
			}
		}
		else // Generate
		{
			if (Options.bValidateOnly)
			{
				StepResult.Status = EVoidPipelineStepStatus::Skipped;
				Result.Steps.Add(StepResult);
				continue;
			}

			const TSharedPtr<IVoidGenerator> Generator = Generators.FindGenerator(Step.GeneratorId);
			if (!Generator.IsValid())
			{
				StepResult.Status = EVoidPipelineStepStatus::NotRegistered;
				if (Step.bRequired)
				{
					AddPipelineIssue(Result.Report, EVoidValidationSeverity::Error, TEXT("VOID.Pipeline.RequiredGeneratorMissing"),
						FString::Printf(TEXT("Required generator '%s' is not registered; the pipeline cannot run."), *Step.GeneratorId.ToString()), Step.GeneratorId.ToString(),
						TEXT("Ensure the module that owns this generator is enabled and calls FVoidGeneratorRegistry::RegisterGenerator in StartupModule."));
					bAborted = true;
					Result.AbortedAtStep = Step.StepId;
				}
				else
				{
					AddPipelineIssue(Result.Report, EVoidValidationSeverity::Warning, TEXT("VOID.Pipeline.GeneratorSkipped"),
						FString::Printf(TEXT("Generator '%s' is not registered; its step was skipped and its output will be absent from the world."), *Step.GeneratorId.ToString()), Step.GeneratorId.ToString(),
						TEXT("Expected until that generator is integrated."));
				}
			}
			else if (!Package || !World)
			{
				AddPipelineIssue(Result.Report, EVoidValidationSeverity::Error, TEXT("VOID.Pipeline.MissingInput"),
					FString::Printf(TEXT("Cannot run generator '%s': %s."), *Step.GeneratorId.ToString(), !Package ? TEXT("no design package was supplied") : TEXT("no target world was supplied")), Step.GeneratorId.ToString(),
					TEXT("Import a valid package and pass the editor world (or use bValidateOnly)."));
				StepResult.Status = EVoidPipelineStepStatus::Failed;
				bAborted = true;
				Result.AbortedAtStep = Step.StepId;
			}
			else
			{
				FVoidGenerationContext GenContext;
				GenContext.TargetWorld = World;
				GenContext.IsCancellationRequested = Options.IsCancellationRequested;

				const bool bOk = Generator->Generate(*Package, GenContext);
				Input.ExecutedGenerators.Add(Step.GeneratorId);
				StepResult.LogLines = GenContext.OutputLog;

				// Capture BEFORE the next generator can overwrite it (see class comment).
				MergeDeduped(Result.Report, GenContext.GenerationValidationReport, Step.GeneratorId, FString::Printf(TEXT("Generator:%s"), *Step.GeneratorId.ToString()), Seen);

				if (GenContext.IsCancelled())
				{
					AddPipelineIssue(Result.Report, EVoidValidationSeverity::Warning, TEXT("VOID.Pipeline.Cancelled"), FString::Printf(TEXT("Generation was cancelled during generator '%s'; the world is partially generated."), *Step.GeneratorId.ToString()), Step.GeneratorId.ToString(), TEXT("Delete the generated actors before re-running."));
					bAborted = true;
					Result.AbortedAtStep = Step.StepId;
				}
				else if (!bOk)
				{
					FString Tail;
					const int32 From = FMath::Max(0, GenContext.OutputLog.Num() - 3);
					for (int32 i = From; i < GenContext.OutputLog.Num(); ++i) { Tail += (Tail.IsEmpty() ? TEXT("") : TEXT(" | ")) + GenContext.OutputLog[i]; }
					AddPipelineIssue(Result.Report, EVoidValidationSeverity::Error, TEXT("VOID.Pipeline.GeneratorFailed"),
						FString::Printf(TEXT("Generator '%s' reported failure. Last log lines: %s"), *Step.GeneratorId.ToString(), Tail.IsEmpty() ? TEXT("(generator wrote no log)") : *Tail), Step.GeneratorId.ToString(),
						TEXT("Read the generator's log (LogVoidGenerators) and the issues attributed to it above; a generator that builds nothing also returns false."));
					if (Options.bStopOnError || Step.bRequired)
					{
						bAborted = true;
						Result.AbortedAtStep = Step.StepId;
					}
				}
			}
		}

		StepResult.ElapsedMs = (FPlatformTime::Seconds() - Start) * 1000.0;
		StepResult.NumErrors = Result.Report.NumBlocking() - ErrorsBefore;
		StepResult.NumWarnings = Result.Report.NumWarnings() - WarningsBefore;
		if (StepResult.Status == EVoidPipelineStepStatus::Skipped) // default value: it actually ran
		{
			StepResult.Status = StepResult.NumErrors > 0 ? EVoidPipelineStepStatus::Failed : (StepResult.NumWarnings > 0 ? EVoidPipelineStepStatus::PassedWithWarnings : EVoidPipelineStepStatus::Passed);
		}
		Result.Steps.Add(StepResult);
	}

	Result.Report.RecomputeValidity();
	Result.bCompleted = !bAborted;

	const FString AbortText = Result.AbortedAtStep.IsNone() ? FString() : FString::Printf(TEXT(" (aborted at %s)"), *Result.AbortedAtStep.ToString());
	UE_LOG(LogVoidValidation, Log, TEXT("Pipeline finished: %s. Errors %d, Warnings %d, Info %d%s"),
		Result.WasSuccessful() ? TEXT("SUCCESS") : TEXT("FAILED"), Result.Report.NumBlocking(), Result.Report.NumWarnings(), Result.Report.NumInfo(), *AbortText);
	return Result;
}
