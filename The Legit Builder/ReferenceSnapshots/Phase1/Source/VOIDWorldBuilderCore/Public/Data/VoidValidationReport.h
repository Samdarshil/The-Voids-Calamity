// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Types/VoidWorldBuilderTypes.h"
#include "VoidValidationReport.generated.h"

/**
 * A single validation finding: what was wrong, how severe, and where it
 * came from. Kept as plain data so it can be displayed in the Editor UI,
 * logged, or serialized for CI output without any dependency on how
 * validation itself was performed.
 */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERCORE_API FVoidValidationIssue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	EVoidValidationSeverity Severity = EVoidValidationSeverity::Warning;

	/** Human-readable description of the issue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	FString Message;

	/** Dot/bracket path to the offending field, e.g. "buildings[3].footprintCorners". Empty if not field-specific. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	FString FieldPath;

	FVoidValidationIssue() = default;

	FVoidValidationIssue(EVoidValidationSeverity InSeverity, FString InMessage, FString InFieldPath = FString())
		: Severity(InSeverity)
		, Message(MoveTemp(InMessage))
		, FieldPath(MoveTemp(InFieldPath))
	{
	}
};

/**
 * Aggregate result of validating a design package. bIsValid is true only
 * when there are zero Error-severity issues; Warnings never affect it.
 * Import and (later) generation code must treat bIsValid as the single
 * source of truth for "is this package safe to generate from" -- do not
 * re-derive that by scanning Issues manually elsewhere.
 */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERCORE_API FVoidValidationReport
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	bool bIsValid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	TArray<FVoidValidationIssue> Issues;

	void AddError(FString Message, FString FieldPath = FString())
	{
		Issues.Add(FVoidValidationIssue(EVoidValidationSeverity::Error, MoveTemp(Message), MoveTemp(FieldPath)));
		bIsValid = false;
	}

	void AddWarning(FString Message, FString FieldPath = FString())
	{
		Issues.Add(FVoidValidationIssue(EVoidValidationSeverity::Warning, MoveTemp(Message), MoveTemp(FieldPath)));
	}

	int32 NumErrors() const
	{
		int32 Count = 0;
		for (const FVoidValidationIssue& Issue : Issues)
		{
			if (Issue.Severity == EVoidValidationSeverity::Error)
			{
				++Count;
			}
		}
		return Count;
	}

	int32 NumWarnings() const
	{
		return Issues.Num() - NumErrors();
	}
};
