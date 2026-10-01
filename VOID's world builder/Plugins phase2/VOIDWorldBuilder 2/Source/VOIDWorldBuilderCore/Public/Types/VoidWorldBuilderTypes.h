// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "VoidWorldBuilderTypes.generated.h"

/**
 * Severity of a single validation issue found while importing a design
 * package.
 *
 *   Info    - Purely informational (a default was applied, a field was
 *             intentionally ignored). Never affects import success.
 *   Warning - Non-ideal but tolerable (an untyped building placeholder,
 *             a newer-minor-version schema). Never blocks generation.
 *   Error   - Blocks generation. The package can be read and inspected,
 *             but must not be generated into a level as-is.
 *   Fatal   - Blocks import itself. Something upstream of "is this
 *             design valid" failed (unreadable file, unparseable JSON,
 *             incompatible schema major version) -- further validation
 *             would just be noise about data we can't trust.
 */
UENUM(BlueprintType)
enum class EVoidValidationSeverity : uint8
{
	Info    UMETA(DisplayName = "Info"),
	Warning UMETA(DisplayName = "Warning"),
	Error   UMETA(DisplayName = "Error"),
	Fatal   UMETA(DisplayName = "Fatal")
};

/**
 * Stable identifier type for design elements (districts, buildings, roads).
 * A thin FName wrapper rather than a raw FString/FGuid so future generator
 * code passes IDs around with type-checked intent instead of an ambiguous
 * FString that could be a name, a path, or an ID.
 */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERCORE_API FVoidElementId
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	FName Value;

	FVoidElementId()
		: Value(NAME_None)
	{
	}

	explicit FVoidElementId(FName InValue)
		: Value(InValue)
	{
	}

	/**
	 * True only if this id is both set (not NAME_None) and not the empty
	 * string. Constructing an FName from an empty FString does NOT produce
	 * NAME_None -- it produces a distinct, valid-looking FName for "" --
	 * so a NAME_None check alone is not sufficient here. This matters
	 * concretely: if a missing/blank JSON "id" field ever got mapped into
	 * an FVoidElementId without the caller separately checking field
	 * presence, a NAME_None-only check would incorrectly treat that blank
	 * id as valid. Defending against it here means every current and
	 * future caller gets the correct behavior for free.
	 */
	bool IsValid() const
	{
		return Value != NAME_None && !Value.ToString().IsEmpty();
	}

	bool operator==(const FVoidElementId& Other) const
	{
		return Value == Other.Value;
	}

	friend uint32 GetTypeHash(const FVoidElementId& Id)
	{
		return GetTypeHash(Id.Value);
	}
};
