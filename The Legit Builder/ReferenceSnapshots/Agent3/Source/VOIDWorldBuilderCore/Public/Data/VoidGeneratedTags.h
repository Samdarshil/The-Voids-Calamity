// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"

class AActor;

/**
 * FVoidGeneratedTags
 *
 * Standard actor-tag convention that lets ANY generator's output be found,
 * attributed and de-duplicated by the world validator without the validator
 * knowing the generator's actor classes:
 *
 *   "VOID.Generated"            marker (present on every generated actor)
 *   "VOID.Generator.<Id>"       owning generator id, e.g. VOID.Generator.Road
 *   "VOID.Id.<ObjectId>"        design-object id the actor was built from
 *
 * Every generator should call Apply() on each actor it spawns. Until the Road
 * Generator does (it does not in the Phase 3 snapshot -- see
 * INTEGRATION_RISK_REGISTER R-07), the validation module recognises
 * AVoidRoadActor / AVoidRoadJunctionActor through an explicit adapter.
 */
class VOIDWORLDBUILDERCORE_API FVoidGeneratedTags
{
public:
	static FName MarkerTag();
	static void Apply(AActor* Actor, FName GeneratorId, const FString& ObjectId);

	/** @return true if Actor carries the marker; OutObjectId may be empty if only the marker/generator tags are present. */
	static bool TryRead(const AActor* Actor, FName& OutGeneratorId, FString& OutObjectId);
};
