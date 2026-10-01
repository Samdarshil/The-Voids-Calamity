// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Data/VoidDesignPackage.h"

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
};
