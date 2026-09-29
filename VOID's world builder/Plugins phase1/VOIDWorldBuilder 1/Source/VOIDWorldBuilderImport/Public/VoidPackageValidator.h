// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Data/VoidDesignPackage.h"
#include "Data/VoidValidationReport.h"

/**
 * FVoidPackageValidator
 *
 * Structural and semantic validation for an already-mapped
 * FVoidDesignPackage. Runs after JSON mapping, not during it, so that
 * "the JSON didn't parse" and "the JSON parsed fine but the design data
 * is invalid" stay clearly separate failure categories with separate
 * messages.
 *
 * Phase 1 validation rules are intentionally conservative: required
 * fields present, IDs non-empty and unique, geometry non-degenerate.
 * Cross-package validation (e.g. does this district's roads line up with
 * its neighbor) is out of scope until multi-district packages exist
 * (Phase 5+).
 */
class VOIDWORLDBUILDERIMPORT_API FVoidPackageValidator
{
public:
	/**
	 * Validates a package, merging any pre-existing issues (e.g. mapping
	 * warnings collected while reading JSON) with newly discovered ones.
	 * @param Package The package to validate.
	 * @param InReport An existing report to append to. Pass a default-constructed FVoidValidationReport if there is none.
	 * @return The merged, final validation report.
	 */
	static FVoidValidationReport Validate(const FVoidDesignPackage& Package, FVoidValidationReport InReport);

private:
	static void ValidateMetadata(const FVoidDesignPackage& Package, FVoidValidationReport& Report);
	static void ValidateBuildings(const FVoidDesignPackage& Package, FVoidValidationReport& Report);
	static void ValidateRoads(const FVoidDesignPackage& Package, FVoidValidationReport& Report);
	static void ValidateIdUniqueness(const FVoidDesignPackage& Package, FVoidValidationReport& Report);
};
