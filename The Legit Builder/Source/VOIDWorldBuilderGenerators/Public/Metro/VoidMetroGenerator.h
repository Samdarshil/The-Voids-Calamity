// Copyright VOID Engineering Studio. Internal tool - not for shipping content.
// Added by Agent 2 (Phase 6 Metro Generator).

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IVoidGenerator.h"

/**
 * FVoidMetroGenerator
 *
 * Registered with the existing FVoidGeneratorRegistry under the id "Metro".
 * Consumes FVoidDesignPackage::Metro (normalized data from the import layer);
 * never reads JSON.
 *
 * Pipeline:  validate -> resolve layout -> (abort here on any Error, world untouched)
 *            -> transaction -> destroy previously generated actors with our OwnerKey
 *            -> spawn track chunk actors + station actors -> place per World Partition
 *            settings -> publish layout to FVoidMetroExportRegistry.
 *
 * Scope: cinematic/world representation only. No trains, passengers, ticketing,
 * signalling, schedules or transit simulation.
 */
class VOIDWORLDBUILDERGENERATORS_API FVoidMetroGenerator : public IVoidGenerator
{
public:
	virtual FName GetGeneratorId() const override;
	virtual bool Generate(const FVoidDesignPackage& Package, FVoidGenerationContext& Context) override;
};
