// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Data/VoidDesignPackage.h"
#include "Data/VoidValidationReport.h"

/**
 * FVoidPackageValidator
 *
 * Structural, semantic, and schema-compatibility validation for an
 * already-mapped FVoidDesignPackage. Runs after JSON mapping, not during
 * it, so that "the JSON didn't parse" and "the JSON parsed fine but the
 * design data is invalid" stay clearly separate failure categories.
 *
 * Validation stages run in order and short-circuit on Fatal: schema
 * version compatibility first (nothing else can be trusted to mean what
 * it says if the schema itself is incompatible), then metadata, then the
 * district payload. See Docs/ValidationRules.md for the full enumerated
 * rule list with error codes.
 *
 * Cross-package validation (e.g. do this district's roads line up with
 * its neighbor) is out of scope until multi-district packages exist
 * (Phase 5+).
 */
class VOIDWORLDBUILDERIMPORT_API FVoidPackageValidator
{
public:
	/**
	 * Validates a package, merging any pre-existing issues (e.g. mapping
	 * issues collected while reading) with newly discovered ones. If
	 * InReport already has a Fatal issue, validation is skipped entirely
	 * and InReport is returned unchanged -- there is nothing safe left to
	 * check.
	 * @param Package The package to validate.
	 * @param InReport An existing report to append to. Pass a default-constructed FVoidValidationReport if there is none.
	 * @return The merged, final validation report.
	 */
	static FVoidValidationReport Validate(const FVoidDesignPackage& Package, FVoidValidationReport InReport);

private:
	static void ValidateSchemaVersion(const FVoidDesignPackage& Package, FVoidValidationReport& Report);
	static void ValidateMetadata(const FVoidDesignPackage& Package, FVoidValidationReport& Report);
	static void ValidateDistrict(const FVoidDesignPackage& Package, FVoidValidationReport& Report);
	static void ValidateBuildings(const FVoidDistrictData& District, FVoidValidationReport& Report);
	static void ValidateRoads(const FVoidDistrictData& District, FVoidValidationReport& Report);
	static void ValidateIdUniqueness(const FVoidDistrictData& District, FVoidValidationReport& Report);
};
