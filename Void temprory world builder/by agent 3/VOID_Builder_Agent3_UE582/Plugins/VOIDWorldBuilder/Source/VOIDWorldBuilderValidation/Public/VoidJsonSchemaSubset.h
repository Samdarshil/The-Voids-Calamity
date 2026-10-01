// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Interfaces/IVoidValidator.h"

/**
 * FVoidJsonSchemaSubset
 *
 * Validates a parsed JSON document against a JSON Schema, supporting exactly
 * the keywords the Meridian_Master schemas use (surveyed from the real files):
 *   type (string or array), const, enum, required, properties,
 *   additionalProperties (false or a schema), items, minItems, maxItems,
 *   uniqueItems (scalars), minLength, pattern, minimum, maximum,
 *   minProperties, maxProperties.
 * Unsupported keywords ($ref, oneOf, allOf, ...) are ignored, never failed
 * on. A full validator was rejected: it would be a large dependency to
 * maintain for twelve schemas that only use this subset.
 *
 * Every violation becomes an ERROR with the JSON path in FieldPath and the
 * file label in ObjectId, so a human can go straight to the offending value.
 * Codes: VOID.Schema.{WrongType, MissingField, UnknownField, EnumMismatch,
 * ConstMismatch, MinLength, Pattern, Range, ItemCount, UniqueItems, PropertyCount}.
 */
class VOIDWORLDBUILDERVALIDATION_API FVoidJsonSchemaSubset
{
public:
	/** @return the number of violations found (including ones throttled out of the report). */
	static int32 Validate(const TSharedPtr<FJsonValue>& Instance, const TSharedPtr<FJsonObject>& Schema, const FString& FileLabel, FVoidValidationContext& Context);
};
