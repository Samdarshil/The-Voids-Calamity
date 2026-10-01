// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Data/VoidDesignPackage.h"
#include "Data/VoidValidationReport.h"
#include "Data/VoidMeridianData.h"
#include "Data/VoidRoadOutput.h"
#include "Coordinates/VoidWorldSpace.h"
#include "Types/VoidWorldBuilderTypes.h"

/**
 * FVoidGeneratorResult
 *
 * What one generator run reports back. Stored per generator id in FVoidGenerationContext::Results so
 * later generators (and the pipeline) can see whether their dependencies actually succeeded.
 */
struct VOIDWORLDBUILDERCORE_API FVoidGeneratorResult
{
	EVoidGeneratorStatus Status = EVoidGeneratorStatus::NotRun;
	/** One-line human-readable outcome ("Built 2 roads, 1 intersection", "Skipped: dependency 'Road' failed"). */
	FString Message;
	int32 NumCreated = 0;
	int32 NumSkipped = 0;
	double ElapsedMilliseconds = 0.0;

	bool DidSucceed() const
	{
		return Status == EVoidGeneratorStatus::Succeeded || Status == EVoidGeneratorStatus::SucceededWithWarnings;
	}
};

/**
 * FVoidGenerationContext
 *
 * Carries the mutable state a generator needs during a run: the target
 * world/level to write into, and an output log of what was created, so
 * the future orchestrator (Phase 10) and the Editor UI can report a
 * summary without every generator inventing its own reporting mechanism.
 *
 * A plain struct, not a UObject, so generators can be called from both
 * editor UI code and automation/commandlet code without worrying about
 * object lifetime or garbage collection.
 */
struct VOIDWORLDBUILDERCORE_API FVoidGenerationContext
{
	/** World the generator should write actors/components into. Never null when passed to a generator. */
	UWorld* TargetWorld = nullptr;

	/** Human-readable summary lines appended by generators as they work; surfaced in the Editor panel and CI logs. */
	TArray<FString> OutputLog;

	/**
	 * Optional. If bound, a generator should call this periodically during
	 * a long-running loop (e.g. once per road/building) and stop early if
	 * it returns true. Left unbound by callers that don't offer
	 * cancellation (e.g. the commandlet) -- IsCancelled() below treats an
	 * unbound callback as "never cancelled".
	 */
	TFunction<bool()> IsCancellationRequested;

	/**
	 * Populated by a generator with its own generation-time validation
	 * findings (distinct from Phase 2's import-time FVoidPackageValidator,
	 * which only checks data shape -- a generator's validator checks
	 * "can this actually be built", e.g. a road flagged as both a bridge
	 * and a tunnel). Left at its default (empty, bIsValid = false) by
	 * generators that have nothing generation-specific to validate.
	 */
	FVoidValidationReport GenerationValidationReport;

	// ---- Added by Agent 1: shared foundation consumed by every generator ----

	/**
	 * Normalized Meridian data (null when running a legacy FVoidDesignPackage). Read-only; produced by
	 * FVoidMeridianImporter. Owned by the caller and must outlive the run.
	 */
	const FVoidMeridianWorld* Meridian = nullptr;

	/** THE coordinate conversion. Generators must not convert coordinates themselves. Defaults to the project settings snapshot. */
	FVoidWorldSpace WorldSpace = FVoidWorldSpace::FromSettings();

	/**
	 * Road output published by the Road Generator (null until it has run). Building/District/Environment
	 * generators should declare a dependency on "Road" and query this instead of touching raw data.
	 */
	TSharedPtr<FVoidRoadNetworkOutput> RoadOutput;

	/** Per-generator outcomes, keyed by GetGeneratorId(). Filled by FVoidGeneratorPipeline (or by callers that run generators directly). */
	TMap<FName, FVoidGeneratorResult> Results;

	/**
	 * Non-fatal + fatal problems raised DURING generation (as opposed to GenerationValidationReport,
	 * which is a generator's pre-generation "can this be built" check). bIsValid is true until an
	 * Error/Fatal is added. The pipeline treats Fatal here as "abort remaining generators".
	 */
	FVoidValidationReport Diagnostics;

	FVoidGenerationContext()
	{
		Diagnostics.bIsValid = true;
	}

	void ReportWarning(const FString& Message, const FString& FieldPath = FString(), FName Code = NAME_None, const FString& Fix = FString())
	{
		Diagnostics.AddWarning(Message, FieldPath, Code, Fix);
		Log(FString::Printf(TEXT("WARNING: %s"), *Message));
	}

	void ReportError(const FString& Message, const FString& FieldPath = FString(), FName Code = NAME_None, const FString& Fix = FString())
	{
		Diagnostics.AddError(Message, FieldPath, Code, Fix);
		Log(FString::Printf(TEXT("ERROR: %s"), *Message));
	}

	void ReportFatal(const FString& Message, const FString& FieldPath = FString(), FName Code = NAME_None, const FString& Fix = FString())
	{
		Diagnostics.AddFatal(Message, FieldPath, Code, Fix);
		Log(FString::Printf(TEXT("FATAL: %s"), *Message));
	}

	/** True if generator Id ran and succeeded in this context. */
	bool HasSucceeded(FName GeneratorId) const
	{
		const FVoidGeneratorResult* R = Results.Find(GeneratorId);
		return R && R->DidSucceed();
	}

	bool IsCancelled() const
	{
		return IsCancellationRequested && IsCancellationRequested();
	}

	void Log(const FString& Line)
	{
		OutputLog.Add(Line);
	}
};

/**
 * IVoidGenerator
 *
 * Contract every generator module (Road, Building, District, Navigation,
 * World Partition, Data Layer, Gameplay Volume -- Phases 3 through 9)
 * will implement. Defined here in Phase 1 with zero implementations so
 * that:
 *
 *   1. The Editor module and the future orchestrator (Phase 10) can be
 *      written against a stable contract before any generator exists.
 *   2. Each generator module can be developed and tested independently
 *      without this interface changing underneath it.
 *
 * Per the studio's incremental build order, no class implements this
 * interface yet -- that begins in Phase 3.
 */
class VOIDWORLDBUILDERCORE_API IVoidGenerator
{
public:
	virtual ~IVoidGenerator() = default;

	/** Short, stable identifier for this generator, e.g. "Road", "Building". Used for logging and registry lookup. */
	virtual FName GetGeneratorId() const = 0;

	/**
	 * Runs generation against an already-validated design package.
	 * Implementations must not be called with a package whose
	 * FVoidValidationReport::bIsValid is false -- callers are
	 * responsible for validating first.
	 *
	 * @return true if generation completed without a fatal error.
	 */
	virtual bool Generate(const FVoidDesignPackage& Package, FVoidGenerationContext& Context) = 0;

	// ------------------------------------------------------------------------------------------
	// Added by Agent 1. Every member below has a safe default so pre-existing generators (and any
	// generator written against the original contract) keep compiling and behaving exactly as before.
	// ------------------------------------------------------------------------------------------

	/** Human-readable name for UI/logs. Defaults to the id. */
	virtual FText GetDisplayName() const { return FText::FromName(GetGeneratorId()); }

	/**
	 * Ids of generators that must have SUCCEEDED before this one runs (e.g. Building depends on "Road").
	 * FVoidGeneratorPipeline orders runs topologically by this and skips a generator whose dependency failed.
	 */
	virtual TArray<FName> GetDependencies() const { return TArray<FName>(); }

	/** True if Generate(const FVoidDesignPackage&, ...) is meaningful for this generator. */
	virtual bool SupportsPackageInput() const { return true; }

	/** True if GenerateFromMeridian is implemented. */
	virtual bool SupportsMeridianInput() const { return false; }

	/**
	 * Runs generation from the normalized Meridian world (Context.WorldSpace / Context.RoadOutput /
	 * Context.Meridian are set by the caller or pipeline). Default: reports that this generator has no
	 * Meridian path and fails cleanly - never crashes, never silently no-ops.
	 * Precondition: the import result was valid (FVoidMeridianImportResult::WasSuccessful()).
	 */
	virtual bool GenerateFromMeridian(const FVoidMeridianWorld& World, FVoidGenerationContext& Context)
	{
		Context.ReportError(
			FString::Printf(TEXT("Generator '%s' has no Meridian input path."), *GetGeneratorId().ToString()),
			FString(), TEXT("VOID.Generator.MeridianUnsupported"),
			TEXT("Implement GenerateFromMeridian and override SupportsMeridianInput() in this generator."));
		return false;
	}

	/**
	 * Undoes this generator's previous output (destroys spawned actors, clears published data) so a
	 * re-run starts clean. Called by the pipeline before each run and by tools on request. Must be
	 * safe to call when nothing has been generated.
	 */
	virtual void Reset(FVoidGenerationContext& Context) {}
};
