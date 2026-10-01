// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IVoidValidator.h"

class UWorld;

/**
 * Plain-data description of one generated actor. The world validator's logic
 * runs on snapshots, never on live UObjects, so (a) it is unit-testable
 * without a UWorld, (b) it cannot mutate the level, (c) new generators add
 * support by filling a snapshot, not by changing the checks.
 */
struct VOIDWORLDBUILDERVALIDATION_API FVoidActorSnapshot
{
	FString ActorName;
	FName ClassName;

	/** Owning generator ("Road", "Building"...), from VOID.Generator.* tags or a class adapter. NAME_None = unattributed. */
	FName GeneratorId;
	/** "Road" (a spline road actor), "Junction", or NAME_None. Only used by the built-in Road checks. */
	FName Role;
	FString ObjectId;

	bool bTagged = false;         // carries the VOID.Generated marker
	bool bVoidClass = false;      // class name starts with "Void"/"AVoid"

	FVector Location = FVector::ZeroVector;
	FVector Scale = FVector::OneVector;
	bool bTransformFinite = true;

	FBox Bounds = FBox(ForceInit);
	bool bBoundsValid = false;

	/** -1 = not applicable for this actor. */
	int32 NumMeshSections = -1;
	int32 NumSplinePoints = -1;
	int32 NumInstances = -1;
	bool bInstancesMissingMesh = false;

	/** Ids of other design objects this actor points at (junction -> roads). */
	TArray<FString> ReferencedIds;
};

struct VOIDWORLDBUILDERVALIDATION_API FVoidWorldSnapshot
{
	TArray<FVoidActorSnapshot> Actors;
};

/**
 * FVoidGeneratedWorldValidator
 *
 * Answers "did generation produce what the package asked for, and is it
 * sane?": missing/orphan/duplicate actors, invalid transforms, empty
 * geometry, missing assets, dangling references, floating buildings,
 * generators that ran but produced nothing.
 *
 * Runs at PostRoad (road checks only) and Final (everything). Cost is one
 * pass over the level's actors plus hash lookups: O(A + R).
 */
class VOIDWORLDBUILDERVALIDATION_API FVoidGeneratedWorldValidator : public IVoidValidator
{
public:
	virtual FName GetValidatorId() const override { return TEXT("VOID.World.Generated"); }
	virtual FName GetSubsystem() const override { return TEXT("World"); }
	virtual bool AppliesToStage(EVoidValidationStage Stage) const override { return Stage == EVoidValidationStage::PostRoad || Stage == EVoidValidationStage::Final; }
	virtual int32 GetOrder() const override { return 50; }
	virtual void Validate(EVoidValidationStage Stage, const FVoidValidationInput& Input, FVoidValidationContext& Context) const override;

	/** Pure logic; this is what the automation tests call. */
	static void ValidateSnapshot(EVoidValidationStage Stage, const FVoidWorldSnapshot& Snapshot, const FVoidValidationInput& Input, FVoidValidationContext& Context);

	/**
	 * Thin extraction layer over live actors. Recognises the Phase 3 Road
	 * actors through an explicit adapter (they are not tagged) and any actor
	 * carrying FVoidGeneratedTags. NOT covered by automated tests (needs a
	 * UWorld); kept deliberately small for that reason.
	 */
	static FVoidWorldSnapshot BuildSnapshot(UWorld* World);
};
