// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

/**
 * FVoidJsonSchemaLite
 *
 * Validates a parsed JSON value against a JSON-Schema (2020-12) document, supporting exactly the
 * keyword subset the Meridian_Master schemas use: type (string or array), required, properties,
 * additionalProperties:false, items, enum, const, minItems, maxItems, minimum.
 *
 * Unsupported keywords ($ref, oneOf, pattern, ...) are ignored, not guessed at - and the Meridian
 * schemas do not use them (verified against every *.schema.json in the package). If a future schema
 * needs more, extend here; do not pull in a second validator.
 */
class VOIDWORLDBUILDERIMPORT_API FVoidJsonSchemaLite
{
public:
	/**
	 * Appends one human-readable violation per problem, each prefixed with a dotted path rooted at RootPath.
	 * @return true if no violations were found.
	 */
	static bool Validate(const TSharedPtr<FJsonValue>& Value, const TSharedPtr<FJsonObject>& Schema, const FString& RootPath, TArray<FString>& OutViolations);

private:
	static void ValidateInternal(const TSharedPtr<FJsonValue>& Value, const TSharedPtr<FJsonObject>& Schema, const FString& Path, TArray<FString>& Out);
	static FString TypeName(const TSharedPtr<FJsonValue>& Value);
	static bool TypeMatches(const TSharedPtr<FJsonValue>& Value, const FString& SchemaType);
};
