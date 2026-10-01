// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IVoidGenerator.h"

/** Options for one pipeline run. */
struct VOIDWORLDBUILDERGENERATORS_API FVoidPipelineOptions
{
	/** Call IVoidGenerator::Reset before each generator so re-runs start clean. */
	bool bResetBeforeRun = true;

	/** Automatically add generators that a requested generator depends on (transitively). If false, a missing dependency is an error. */
	bool bAutoIncludeDependencies = true;

	/** Stop running remaining generators as soon as one reports a Fatal issue. Non-fatal failures only skip dependents. */
	bool bStopOnFatal = true;
};

/**
 * FVoidGeneratorPipeline
 *
 * The single way to run several generators in a correct order. It:
 *   1. Resolves requested ids through FVoidGeneratorRegistry (unknown id = error, nothing runs).
 *   2. Orders them topologically by IVoidGenerator::GetDependencies() (cycle = error, nothing runs).
 *      Ties keep the requested order, so the result is deterministic.
 *   3. Runs Reset() then Generate()/GenerateFromMeridian() for each, recording an FVoidGeneratorResult in
 *      Context.Results, and SKIPS any generator whose dependency did not succeed.
 *   4. Honors cancellation and, optionally, aborts on Fatal.
 *
 * Future generators only need to register with the registry and declare GetDependencies(); the pipeline needs
 * no change. Example: Building declares { "Road" } and then reads Context.RoadOutput.
 */
class VOIDWORLDBUILDERGENERATORS_API FVoidGeneratorPipeline
{
public:
	/** Resolve + order without running. OutOrdered is empty on failure and OutError says why. */
	static bool ResolveOrder(const TArray<FName>& RequestedIds, const FVoidPipelineOptions& Options, TArray<TSharedRef<IVoidGenerator>>& OutOrdered, FString& OutError);

	/** Legacy design-package input. Returns true iff every generator that ran succeeded and none was skipped. */
	static bool RunWithPackage(const TArray<FName>& RequestedIds, const FVoidDesignPackage& Package, FVoidGenerationContext& Context, const FVoidPipelineOptions& Options = FVoidPipelineOptions());

	/** Meridian input. Sets Context.Meridian for the duration of the run. Precondition: the import was successful. */
	static bool RunWithMeridian(const TArray<FName>& RequestedIds, const FVoidMeridianWorld& World, FVoidGenerationContext& Context, const FVoidPipelineOptions& Options = FVoidPipelineOptions());

private:
	template <typename TRunFn>
	static bool RunInternal(const TArray<FName>& RequestedIds, FVoidGenerationContext& Context, const FVoidPipelineOptions& Options, bool bMeridian, TRunFn&& RunOne);
};
