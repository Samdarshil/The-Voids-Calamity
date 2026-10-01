// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Types/VoidWorldBuilderTypes.h"
#include "VoidValidationReport.generated.h"

/**
 * A single validation finding: what was wrong, how severe, where it came
 * from, a stable machine-filterable code, and an actionable suggested
 * fix. Kept as plain data so it can be displayed in the Editor UI,
 * logged, or serialized for CI output without any dependency on how
 * validation itself was performed.
 */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERCORE_API FVoidValidationIssue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	EVoidValidationSeverity Severity = EVoidValidationSeverity::Info;

	/** Human-readable description of the issue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	FString Message;

	/** Dot/bracket path to the offending field, e.g. "district.buildings[3].footprintCorners". Empty if not field-specific. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	FString FieldPath;

	/**
	 * Stable, filterable identifier for this class of issue, e.g.
	 * "VOID.Import.MissingField". An FName rather than a closed enum
	 * because the set of codes is expected to keep growing as validation
	 * rules grow -- see Docs/ValidationRules.md for the registered list.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	FName ErrorCode;

	/** Actionable, human-readable suggestion for resolving the issue. Should not be blank for Error/Fatal issues -- see Docs/ValidationRules.md's authoring convention. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	FString SuggestedFix;

	FVoidValidationIssue() = default;

	FVoidValidationIssue(EVoidValidationSeverity InSeverity, FString InMessage, FString InFieldPath, FName InErrorCode, FString InSuggestedFix)
		: Severity(InSeverity)
		, Message(MoveTemp(InMessage))
		, FieldPath(MoveTemp(InFieldPath))
		, ErrorCode(InErrorCode)
		, SuggestedFix(MoveTemp(InSuggestedFix))
	{
	}
};

/**
 * Aggregate result of validating a design package. bIsValid is true only
 * when there are zero Error- and zero Fatal-severity issues; Info and
 * Warning never affect it. Import and (later) generation code must treat
 * bIsValid as the single source of truth for "is this package safe to
 * generate from" -- do not re-derive that by scanning Issues manually
 * elsewhere.
 *
 * A Fatal issue additionally means the pipeline should stop looking for
 * more problems: something upstream of "is this data valid" already
 * failed (unreadable file, unparseable JSON, incompatible schema major
 * version), so further validation would just be noise about data that
 * can't be trusted in the first place. Callers should check
 * HasFatalIssue() and short-circuit rather than pushing on to the next
 * validation stage.
 */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERCORE_API FVoidValidationReport
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	bool bIsValid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	TArray<FVoidValidationIssue> Issues;

	void AddInfo(FString Message, FString FieldPath = FString(), FName ErrorCode = NAME_None, FString SuggestedFix = FString())
	{
		Issues.Add(FVoidValidationIssue(EVoidValidationSeverity::Info, MoveTemp(Message), MoveTemp(FieldPath), ErrorCode, MoveTemp(SuggestedFix)));
	}

	void AddWarning(FString Message, FString FieldPath = FString(), FName ErrorCode = NAME_None, FString SuggestedFix = FString())
	{
		Issues.Add(FVoidValidationIssue(EVoidValidationSeverity::Warning, MoveTemp(Message), MoveTemp(FieldPath), ErrorCode, MoveTemp(SuggestedFix)));
	}

	void AddError(FString Message, FString FieldPath = FString(), FName ErrorCode = NAME_None, FString SuggestedFix = FString())
	{
		Issues.Add(FVoidValidationIssue(EVoidValidationSeverity::Error, MoveTemp(Message), MoveTemp(FieldPath), ErrorCode, MoveTemp(SuggestedFix)));
		bIsValid = false;
	}

	void AddFatal(FString Message, FString FieldPath = FString(), FName ErrorCode = NAME_None, FString SuggestedFix = FString())
	{
		Issues.Add(FVoidValidationIssue(EVoidValidationSeverity::Fatal, MoveTemp(Message), MoveTemp(FieldPath), ErrorCode, MoveTemp(SuggestedFix)));
		bIsValid = false;
	}

	int32 NumBySeverity(EVoidValidationSeverity Severity) const
	{
		int32 Count = 0;
		for (const FVoidValidationIssue& Issue : Issues)
		{
			if (Issue.Severity == Severity)
			{
				++Count;
			}
		}
		return Count;
	}

	int32 NumInfo() const     { return NumBySeverity(EVoidValidationSeverity::Info); }
	int32 NumWarnings() const { return NumBySeverity(EVoidValidationSeverity::Warning); }
	int32 NumErrors() const   { return NumBySeverity(EVoidValidationSeverity::Error); }
	int32 NumFatal() const    { return NumBySeverity(EVoidValidationSeverity::Fatal); }

	bool HasFatalIssue() const
	{
		return NumFatal() > 0;
	}
};
