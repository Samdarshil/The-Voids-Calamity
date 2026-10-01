// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IVoidGenerator.h"
#include "Environment/VoidPlacementPlanner.h"

/** Optional per-run overrides. Defaults mean "use UVoidEnvironmentSettings as configured". */
struct VOIDWORLDBUILDERGENERATORS_API FVoidDressingOptions
{
	float DensityScaleOverride = -1.0f;        // < 0 = use settings
	int32 MaxInstancesOverride = -1;           // < 0 = use settings
	bool bOverrideSeed = false;
	int32 SeedOverride = 0;
	TArray<FName> CategoryAllowList;           // empty = every category of the generator's domain
	bool bUseBounds = false;                   // spatial filter (2D world space)
	FVector2D BoundsMin = FVector2D::ZeroVector;
	FVector2D BoundsMax = FVector2D::ZeroVector;
	bool bDryRun = false;                      // plan only: nothing is destroyed or spawned
	bool bForceExportJson = false;
};

struct VOIDWORLDBUILDERGENERATORS_API FVoidDressingRunResult
{
	bool bSucceeded = false;
	bool bCancelled = false;
	VoidPlan::FStats PlanStats;
	int32 ActorsRemoved = 0;
	int32 ActorsSpawned = 0;
	int32 ComponentsCreated = 0;
	int32 InstancesCreated = 0;                // counts every instance of every component (multi-part placeholders count once per part)
	int32 LogicalInstances = 0;                // planned placements (what the city "contains")
	int32 PlaceholderCategoriesUsed = 0;
	int32 MissingCategories = 0;               // categories with neither slot nor placeholder (skipped)
	double ElapsedMs = 0.0;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FVoidDressingPlannedDelegate, FName /*Domain*/, const TArray<VoidPlan::FInstance>& /*Instances*/);

/**
 * FVoidDressingGeneratorBase
 *
 * Shared implementation of the two Agent 6 generators. They are ordinary
 * IVoidGenerator implementations registered with the existing
 * FVoidGeneratorRegistry (ids "Environment" and "Props") -- no second
 * architecture. Pipeline per run:
 *
 *   package -> plan inputs (roads/junctions/areas, same geometry as the Road
 *   generator) -> VoidPlan::Plan (pure, deterministic) -> [optional JSON
 *   export + OnInstancesPlanned delegate = PCG hook] -> destroy previously
 *   owned actors -> spawn one AVoidDressingActor per streaming cell with
 *   (H)ISM buckets.
 *
 * Nothing is destroyed until planning has finished uncancelled, so a cancelled
 * or failed run leaves the previous generation intact.
 */
class VOIDWORLDBUILDERGENERATORS_API FVoidDressingGeneratorBase : public IVoidGenerator
{
public:
	FVoidDressingGeneratorBase(FName InGeneratorId, FName InDomain);

	//~ Begin IVoidGenerator
	virtual FName GetGeneratorId() const override { return GeneratorId; }
	virtual bool Generate(const FVoidDesignPackage& Package, FVoidGenerationContext& Context) override;
	//~ End IVoidGenerator

	/** Same as Generate() with explicit overrides and a detailed result. */
	bool GenerateWithOptions(const FVoidDesignPackage& Package, FVoidGenerationContext& Context, const FVoidDressingOptions& Options, FVoidDressingRunResult* OutResult = nullptr);

	/** Destroys every dressing actor this generator owns for DistrictId (all districts if NAME_None). Returns how many were removed. */
	int32 ClearGenerated(UWorld* World, FName DistrictId = NAME_None) const;

	FName GetDomain() const { return Domain; }

	/** PCG hook: broadcast after planning, before spawning, on every run (including dry runs). Subscribers must not mutate the world synchronously. */
	static FVoidDressingPlannedDelegate& OnInstancesPlanned();

private:
	FName GeneratorId;
	FName Domain;
};

/** Phase 12: trees, grass, bushes, rocks, planters, benches, street furniture, poles, traffic lights, sidewalk details. Registered as "Environment". */
class VOIDWORLDBUILDERGENERATORS_API FVoidEnvironmentGenerator : public FVoidDressingGeneratorBase
{
public:
	FVoidEnvironmentGenerator() : FVoidDressingGeneratorBase(FName(TEXT("Environment")), FName(TEXT("Environment"))) {}
};

/** Phase 14: vehicles, containers, crates, pipes, barriers, signs, billboards, trash, construction assets. Registered as "Props". */
class VOIDWORLDBUILDERGENERATORS_API FVoidPropGenerator : public FVoidDressingGeneratorBase
{
public:
	FVoidPropGenerator() : FVoidDressingGeneratorBase(FName(TEXT("Props")), FName(TEXT("Props"))) {}
};
