// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VoidGeneratorPipeline.h"
#include "VoidGeneratorRegistry.h"
#include "VoidWorldBuilderGeneratorsLog.h"
#include "HAL/PlatformTime.h"

bool FVoidGeneratorPipeline::ResolveOrder(const TArray<FName>& RequestedIds, const FVoidPipelineOptions& Options, TArray<TSharedRef<IVoidGenerator>>& OutOrdered, FString& OutError)
{
	OutOrdered.Reset();
	OutError.Reset();

	const FVoidGeneratorRegistry& Registry = FVoidGeneratorRegistry::Get();

	// 1. Collect the set (requested order preserved), adding dependencies on demand.
	TArray<FName> Ids;
	TMap<FName, TSharedRef<IVoidGenerator>> ById;

	TFunction<bool(FName, int32)> Add = [&](FName Id, int32 Depth) -> bool
	{
		if (Depth > 64) { OutError = FString::Printf(TEXT("Dependency chain through '%s' is too deep (cycle?)."), *Id.ToString()); return false; }
		if (ById.Contains(Id)) { return true; }
		const TSharedPtr<IVoidGenerator> Gen = Registry.FindGenerator(Id);
		if (!Gen.IsValid()) { OutError = FString::Printf(TEXT("No generator registered with id '%s'."), *Id.ToString()); return false; }
		ById.Add(Id, Gen.ToSharedRef());
		Ids.Add(Id);
		for (const FName Dep : Gen->GetDependencies())
		{
			if (!ById.Contains(Dep) && !Options.bAutoIncludeDependencies)
			{
				OutError = FString::Printf(TEXT("Generator '%s' depends on '%s', which was not requested."), *Id.ToString(), *Dep.ToString());
				return false;
			}
			if (!Add(Dep, Depth + 1)) { return false; }
		}
		return true;
	};

	for (const FName Id : RequestedIds)
	{
		if (!Add(Id, 0)) { return false; }
	}

	// 2. Kahn's algorithm; among ready nodes keep discovery order for determinism.
	TMap<FName, int32> InDegree;
	for (const FName Id : Ids) { InDegree.Add(Id, 0); }
	for (const FName Id : Ids)
	{
		for (const FName Dep : ById[Id]->GetDependencies()) { if (Dep == Id) { OutError = FString::Printf(TEXT("Generator '%s' depends on itself."), *Id.ToString()); return false; } InDegree[Id] += 1; }
	}

	TArray<FName> Ready;
	for (const FName Id : Ids) { if (InDegree[Id] == 0) { Ready.Add(Id); } }

	TArray<FName> Sorted;
	while (Ready.Num() > 0)
	{
		const FName Next = Ready[0];
		Ready.RemoveAt(0);
		Sorted.Add(Next);
		for (const FName Other : Ids)
		{
			if (ById[Other]->GetDependencies().Contains(Next))
			{
				if (--InDegree[Other] == 0) { Ready.Add(Other); }
			}
		}
	}

	if (Sorted.Num() != Ids.Num())
	{
		TArray<FString> Stuck;
		for (const FName Id : Ids) { if (!Sorted.Contains(Id)) { Stuck.Add(Id.ToString()); } }
		OutError = FString::Printf(TEXT("Generator dependency cycle among: %s."), *FString::Join(Stuck, TEXT(", ")));
		return false;
	}

	for (const FName Id : Sorted) { OutOrdered.Add(ById[Id]); }
	return true;
}

template <typename TRunFn>
bool FVoidGeneratorPipeline::RunInternal(const TArray<FName>& RequestedIds, FVoidGenerationContext& Context, const FVoidPipelineOptions& Options, bool bMeridian, TRunFn&& RunOne)
{
	TArray<TSharedRef<IVoidGenerator>> Ordered;
	FString Error;
	if (!ResolveOrder(RequestedIds, Options, Ordered, Error))
	{
		Context.ReportFatal(Error, FString(), TEXT("VOID.Pipeline.ResolveFailed"), TEXT("Check generator ids and GetDependencies() declarations."));
		UE_LOG(LogVoidGenerators, Error, TEXT("Pipeline resolve failed: %s"), *Error);
		return false;
	}

	bool bAllOk = true;

	for (const TSharedRef<IVoidGenerator>& Gen : Ordered)
	{
		const FName Id = Gen->GetGeneratorId();
		FVoidGeneratorResult& Result = Context.Results.FindOrAdd(Id);
		Result = FVoidGeneratorResult();

		if (Context.IsCancelled())
		{
			Result.Status = EVoidGeneratorStatus::Cancelled;
			Result.Message = TEXT("Cancelled before start.");
			bAllOk = false;
			continue;
		}

		if (bMeridian ? !Gen->SupportsMeridianInput() : !Gen->SupportsPackageInput())
		{
			Result.Status = EVoidGeneratorStatus::Skipped;
			Result.Message = bMeridian ? TEXT("Generator does not support Meridian input.") : TEXT("Generator does not support design-package input.");
			Context.ReportWarning(FString::Printf(TEXT("Skipped '%s': %s"), *Id.ToString(), *Result.Message), FString(), TEXT("VOID.Pipeline.InputUnsupported"));
			bAllOk = false;
			continue;
		}

		FName FailedDep = NAME_None;
		for (const FName Dep : Gen->GetDependencies())
		{
			if (!Context.HasSucceeded(Dep)) { FailedDep = Dep; break; }
		}
		if (!FailedDep.IsNone())
		{
			Result.Status = EVoidGeneratorStatus::Skipped;
			Result.Message = FString::Printf(TEXT("Skipped: dependency '%s' did not succeed."), *FailedDep.ToString());
			Context.ReportWarning(FString::Printf(TEXT("'%s': %s"), *Id.ToString(), *Result.Message), FString(), TEXT("VOID.Pipeline.DependencyFailed"));
			bAllOk = false;
			continue;
		}

		if (Options.bResetBeforeRun) { Gen->Reset(Context); Context.Results.FindOrAdd(Id) = FVoidGeneratorResult(); }

		const int32 ErrorsBefore = Context.Diagnostics.NumErrors() + Context.Diagnostics.NumFatal();
		const int32 WarningsBefore = Context.Diagnostics.NumWarnings();
		const double Start = FPlatformTime::Seconds();

		const bool bOk = RunOne(*Gen);

		FVoidGeneratorResult& Final = Context.Results.FindOrAdd(Id);
		Final.ElapsedMilliseconds = (FPlatformTime::Seconds() - Start) * 1000.0;
		const bool bNewErrors = (Context.Diagnostics.NumErrors() + Context.Diagnostics.NumFatal()) > ErrorsBefore;
		const bool bNewWarnings = Context.Diagnostics.NumWarnings() > WarningsBefore;

		if (!bOk)
		{
			Final.Status = EVoidGeneratorStatus::Failed;
			Final.Message = TEXT("Generator reported failure.");
			bAllOk = false;
		}
		else
		{
			// A generator that returned true but raised Errors has partially failed: still usable, but not clean.
			Final.Status = (bNewErrors || bNewWarnings) ? EVoidGeneratorStatus::SucceededWithWarnings : EVoidGeneratorStatus::Succeeded;
			Final.Message = bNewErrors ? TEXT("Completed with errors (some items skipped).") : (bNewWarnings ? TEXT("Completed with warnings.") : TEXT("Completed."));
			if (bNewErrors) { bAllOk = false; }
		}

		UE_LOG(LogVoidGenerators, Log, TEXT("Pipeline: '%s' -> %s (%.2f ms)"), *Id.ToString(), *Final.Message, Final.ElapsedMilliseconds);

		if (Options.bStopOnFatal && Context.Diagnostics.HasFatalIssue())
		{
			Context.Log(TEXT("Pipeline aborted: a Fatal issue was reported."));
			bAllOk = false;
			break;
		}
	}

	return bAllOk;
}

bool FVoidGeneratorPipeline::RunWithPackage(const TArray<FName>& RequestedIds, const FVoidDesignPackage& Package, FVoidGenerationContext& Context, const FVoidPipelineOptions& Options)
{
	return RunInternal(RequestedIds, Context, Options, /*bMeridian*/ false,
		[&](IVoidGenerator& Gen) { return Gen.Generate(Package, Context); });
}

bool FVoidGeneratorPipeline::RunWithMeridian(const TArray<FName>& RequestedIds, const FVoidMeridianWorld& World, FVoidGenerationContext& Context, const FVoidPipelineOptions& Options)
{
	const FVoidMeridianWorld* Previous = Context.Meridian;
	Context.Meridian = &World;
	const bool bOk = RunInternal(RequestedIds, Context, Options, /*bMeridian*/ true,
		[&](IVoidGenerator& Gen) { return Gen.GenerateFromMeridian(World, Context); });
	Context.Meridian = Previous;
	return bOk;
}
