// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Data/VoidDesignPackage.h"
#include "Data/VoidValidationReport.h"
#include "Data/VoidImportContext.h"
#include "VoidImportResult.generated.h"

/**
 * Result of running a design package through the full import pipeline
 * (detect format -> read -> map -> validate). Bundles the package, its
 * validation report, and the import context (source, settings snapshot,
 * timing) so callers always have all three together.
 */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERIMPORT_API FVoidImportResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	FVoidDesignPackage Package;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	FVoidValidationReport ValidationReport;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	FVoidImportContext Context;

	/** Convenience, equivalent to ValidationReport.bIsValid, so call sites read naturally: if (Result.WasSuccessful()) */
	bool WasSuccessful() const
	{
		return ValidationReport.bIsValid;
	}
};
