// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IVoidValidator.h"

/**
 * Data-level validators for an imported FVoidDesignPackage. All three run at
 * EVoidValidationStage::PostImport (they need no world), are registered by the
 * module, and are also directly callable via Run() from tests/tools.
 *
 * Complexity: O(N) in roads/buildings/segments plus the number of *reported*
 * candidate pairs. Every all-pairs question (crossings, conflicts, overlaps,
 * junction clustering) goes through a uniform spatial hash; see
 * Docs/ValidationRules.md "Performance".
 */

/** Value-level checks: non-finite / out-of-range numbers, id syntax, district id vs Meridian registry. */
class VOIDWORLDBUILDERVALIDATION_API FVoidPackageDataValidator : public IVoidValidator
{
public:
	virtual FName GetValidatorId() const override { return TEXT("VOID.Package.Data"); }
	virtual FName GetSubsystem() const override { return TEXT("Data"); }
	virtual bool AppliesToStage(EVoidValidationStage Stage) const override { return Stage == EVoidValidationStage::PostImport; }
	virtual int32 GetOrder() const override { return 10; }
	virtual void Validate(EVoidValidationStage Stage, const FVoidValidationInput& Input, FVoidValidationContext& Context) const override;

	static void Run(const FVoidDesignPackage& Package, const FVoidValidationInput& Input, FVoidValidationContext& Context);
};

/** Road geometry + network topology: degenerate/hairpin geometry, explicit connection sanity, unsplit T-junctions, crossings without junctions, isolated roads, disconnected networks, acute/overloaded junctions. Also re-runs the Phase 3 FVoidRoadValidator early. */
class VOIDWORLDBUILDERVALIDATION_API FVoidRoadNetworkValidator : public IVoidValidator
{
public:
	virtual FName GetValidatorId() const override { return TEXT("VOID.Package.Roads"); }
	virtual FName GetSubsystem() const override { return TEXT("Road"); }
	virtual bool AppliesToStage(EVoidValidationStage Stage) const override { return Stage == EVoidValidationStage::PostImport; }
	virtual int32 GetOrder() const override { return 20; }
	virtual void Validate(EVoidValidationStage Stage, const FVoidValidationInput& Input, FVoidValidationContext& Context) const override;

	static void Run(const FVoidDesignPackage& Package, const FVoidValidationInput& Input, FVoidValidationContext& Context);
};

/**
 * Hooks for the future Building Generator (not implemented here). Validates
 * FVoidBuildingSpec data: footprint simplicity/area/winding/extent, height
 * plausibility, footprint overlaps, and severe road/building conflicts.
 * A Building Generator agent extends coverage by registering its own
 * IVoidValidator for PostBuilding; nothing here needs to change.
 */
class VOIDWORLDBUILDERVALIDATION_API FVoidBuildingHookValidator : public IVoidValidator
{
public:
	virtual FName GetValidatorId() const override { return TEXT("VOID.Package.Buildings"); }
	virtual FName GetSubsystem() const override { return TEXT("Building"); }
	virtual bool AppliesToStage(EVoidValidationStage Stage) const override { return Stage == EVoidValidationStage::PostImport; }
	virtual int32 GetOrder() const override { return 30; }
	virtual void Validate(EVoidValidationStage Stage, const FVoidValidationInput& Input, FVoidValidationContext& Context) const override;

	static void Run(const FVoidDesignPackage& Package, const FVoidValidationInput& Input, FVoidValidationContext& Context);
};
