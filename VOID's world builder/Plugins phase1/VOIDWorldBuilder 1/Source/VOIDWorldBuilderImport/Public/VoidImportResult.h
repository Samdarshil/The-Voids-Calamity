// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Data/VoidDesignPackage.h"
#include "Data/VoidValidationReport.h"
#include "VoidImportResult.generated.h"

/**
 * Result of running a design package through the full import pipeline
 * (parse -> map -> validate). Bundles the package with its validation
 * report so callers always have both together and can't accidentally use
 * a package without checking whether it validated.
 */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERIMPORT_API FVoidImportResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	FVoidDesignPackage Package;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	FVoidValidationReport ValidationReport;

	/** Convenience, equivalent to ValidationReport.bIsValid, so call sites read naturally: if (Result.WasSuccessful()) */
	bool WasSuccessful() const
	{
		return ValidationReport.bIsValid;
	}
};
