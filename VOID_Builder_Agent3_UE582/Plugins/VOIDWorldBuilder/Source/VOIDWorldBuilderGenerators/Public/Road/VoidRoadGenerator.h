// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IVoidGenerator.h"

/**
 * FVoidRoadGenerator
 *
 * Registered with FVoidGeneratorRegistry under the id "Road" by
 * FVOIDWorldBuilderGeneratorsModule::StartupModule(). Consumes an
 * already-imported, already-validated FVoidDesignPackage and spawns:
 *
 *   - One AVoidRoadActor per road (spline + road surface + optional
 *     sidewalk/curb/median ribbons + bridge piers / tunnel portals).
 *   - One AVoidRoadJunctionActor per detected junction (dead end,
 *     cul-de-sac, T/four-way, roundabout spur), with a pad and, for
 *     T/four-way junctions, crosswalk stripes.
 *
 * Runs generation-time validation (FVoidRoadValidator, distinct from
 * Phase 2's import-time validation) first and surfaces it via
 * Context.GenerationValidationReport; does not spawn anything if that
 * reports a Fatal issue (it doesn't today, but the check is defensive
 * against that changing).
 */
class VOIDWORLDBUILDERGENERATORS_API FVoidRoadGenerator : public IVoidGenerator
{
public:
	//~ Begin IVoidGenerator
	virtual FName GetGeneratorId() const override;
	virtual bool Generate(const FVoidDesignPackage& Package, FVoidGenerationContext& Context) override;
	//~ End IVoidGenerator
};
