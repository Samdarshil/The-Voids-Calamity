// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IVoidValidator.h"
#include "Interfaces/IVoidGenerator.h"

class FVoidGeneratorRegistry;
struct FVoidImportResult;

enum class EVoidPipelineStepKind : uint8
{
	/** Run every validator registered for Stage. */
	ValidateStage,
	/** Run the generator registered under GeneratorId. */
	Generate
};

struct VOIDWORLDBUILDERVALIDATION_API FVoidPipelineStep
{
	FName StepId;
	EVoidPipelineStepKind Kind = EVoidPipelineStepKind::ValidateStage;
	FName GeneratorId;                                   // Generate steps
	EVoidValidationStage Stage = EVoidValidationStage::Final; // ValidateStage steps
	/** Generate steps only: if the generator is not registered, true = ERROR and abort; false = WARNING and skip. */
	bool bRequired = false;
};

/**
 * Ordered step list. The default is the canonical flow from the brief:
 *
 *   ImportValidation -> Road -> Validate(PostRoad) -> District -> Validate(PostDistrict)
 *   -> Building -> ... -> Environment -> Lighting -> Props -> Metro -> Final validation
 *
 * It is data, not code: adding, removing or reordering a generator is an
 * edit to this list, never to the runner.
 */
struct VOIDWORLDBUILDERVALIDATION_API FVoidPipelineDefinition
{
	TArray<FVoidPipelineStep> Steps;

	/** @param bRequireAllGenerators false (default): only Road is required so partial integration builds still run; true: final integration mode. */
	static FVoidPipelineDefinition MakeDefault(bool bRequireAllGenerators = false);
};

struct VOIDWORLDBUILDERVALIDATION_API FVoidPipelineOptions
{
	FVoidValidationOptions Validation;

	/** Stop at the first ERROR/FATAL (generation cannot safely continue). Final validation still runs on whatever was produced. */
	bool bStopOnError = true;

	/** Validate only: Generate steps are skipped, so no actors are spawned. */
	bool bValidateOnly = false;

	/** Audit the generator/validator registries first (recommended). */
	bool bAuditContracts = true;

	TSet<FName> KnownDistrictIds;

	/** Passed through to every generator's FVoidGenerationContext. */
	TFunction<bool()> IsCancellationRequested;

	/** Null = the global singletons. Tests pass isolated instances. */
	const FVoidGeneratorRegistry* GeneratorRegistry = nullptr;
	const FVoidValidatorRegistry* ValidatorRegistry = nullptr;
};

enum class EVoidPipelineStepStatus : uint8 { Passed, PassedWithWarnings, Failed, Skipped, NotRegistered };

struct VOIDWORLDBUILDERVALIDATION_API FVoidPipelineStepResult
{
	FName StepId;
	EVoidPipelineStepStatus Status = EVoidPipelineStepStatus::Skipped;
	double ElapsedMs = 0.0;
	int32 NumErrors = 0;
	int32 NumWarnings = 0;
	/** The generator's own OutputLog lines (Generate steps). */
	TArray<FString> LogLines;
};

struct VOIDWORLDBUILDERVALIDATION_API FVoidPipelineResult
{
	FVoidValidationReport Report;
	TArray<FVoidPipelineStepResult> Steps;
	bool bCompleted = false;            // every step ran (none skipped due to abort)
	FName AbortedAtStep = NAME_None;

	bool WasSuccessful() const { return bCompleted && Report.bIsValid; }
	static const TCHAR* StatusToString(EVoidPipelineStepStatus Status);
};

/**
 * FVoidValidationPipeline
 *
 * Runs generators and validators in pipeline order, aggregates everything
 * into ONE FVoidValidationReport, and never lets a generator's problems
 * vanish:
 *  - The Phase 3 Road Generator OVERWRITES Context.GenerationValidationReport
 *    on each Generate call; here every generator gets its own
 *    FVoidGenerationContext and its report is merged (attributed to the
 *    generator, de-duplicated) immediately after it returns.
 *  - A generator returning false becomes an explicit VOID.Pipeline.GeneratorFailed
 *    ERROR carrying its last log lines.
 *  - Validators come from FVoidValidatorRegistry, so other agents' validators
 *    join with no change here.
 * This is a validation runner, not the Phase 10 orchestrator: it has no UI,
 * no retry, no partial-regeneration logic.
 */
class VOIDWORLDBUILDERVALIDATION_API FVoidValidationPipeline
{
public:
	static FVoidPipelineResult Run(const FVoidPipelineDefinition& Definition, const FVoidDesignPackage* Package, UWorld* World, const FVoidPipelineOptions& Options);

	/** Same, but starts from an importer result: its report is folded in first, and a failed import aborts before any generation. */
	static FVoidPipelineResult RunFromImport(const FVoidPipelineDefinition& Definition, const FVoidImportResult& Import, UWorld* World, const FVoidPipelineOptions& Options);
};
