// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IVoidValidator.h"

class FVoidGeneratorRegistry;

/**
 * FVoidGeneratorContractAuditor
 *
 * Runtime half of the cross-generator contract audit (the static half is
 * INTEGRATION_RISK_REGISTER.md). Inspects the live registries and reports
 * integration problems as ordinary validation issues (subsystem "Contract"):
 *
 *   VOID.Contract.GeneratorNotRegistered   expected generator absent (WARNING: expected until its agent's module lands)
 *   VOID.Contract.UnexpectedGenerator      id outside the canonical set (INFO)
 *   VOID.Contract.NonCanonicalGeneratorId  id looks like a canonical one with a decoration, e.g. "BuildingGenerator" (WARNING)
 *   VOID.Contract.InvalidGeneratorId       blank / whitespace in id (ERROR)
 *   VOID.Contract.UnstableGeneratorId      GetGeneratorId() differs between calls (ERROR)
 *   VOID.Contract.DuplicateGeneratorId     two modules registered the same id; one silently replaced the other (ERROR)
 *   VOID.Contract.DuplicateValidatorId     same, for validators (ERROR)
 *   VOID.Contract.StageWithoutValidator    a registered generator has no validator for its stage (INFO)
 */
class VOIDWORLDBUILDERVALIDATION_API FVoidGeneratorContractAuditor
{
public:
	/** Canonical generator ids, in canonical pipeline order: Road, District, Building, Environment, Lighting, Props, Metro. */
	static const TArray<FName>& GetExpectedGeneratorIds();

	/** The stage that follows generator GeneratorId (PostRoad for "Road", ...). NAME_None generators map to Final. */
	static EVoidValidationStage GetStageForGenerator(FName GeneratorId);

	static void Audit(const FVoidGeneratorRegistry& Generators, const FVoidValidatorRegistry& Validators, FVoidValidationContext& Context);
};
