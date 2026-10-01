// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Data/VoidDesignPackage.h"
#include "Data/VoidValidationReport.h"

/**
 * FVoidRoadValidator
 *
 * Generation-time validation for roads: "can the Road Generator actually
 * build this," as opposed to Phase 2's FVoidPackageValidator, which only
 * checks "is this data shaped correctly" (required fields present,
 * non-degenerate geometry). A road can pass import-time validation and
 * still be un-generatable -- e.g. flagged as both a bridge and a tunnel
 * simultaneously, which is structurally valid data but a contradiction
 * no generator can act on.
 *
 * Runs once per district, immediately before generation, using the same
 * FVoidValidationReport/severity model as import so the Editor panel can
 * display both with one shared list-view component.
 */
class VOIDWORLDBUILDERGENERATORS_API FVoidRoadValidator
{
public:
	static FVoidValidationReport Validate(const FVoidDistrictData& District);

private:
	static void ValidateBridgeTunnelExclusivity(const FVoidDistrictData& District, FVoidValidationReport& Report);
	static void ValidateConnectionIdsResolve(const FVoidDistrictData& District, FVoidValidationReport& Report);
	static void ValidateRoundabouts(const FVoidDistrictData& District, FVoidValidationReport& Report);
};
