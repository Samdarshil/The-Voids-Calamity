// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VoidJsonSchemaLite.h"

FString FVoidJsonSchemaLite::TypeName(const TSharedPtr<FJsonValue>& Value)
{
	if (!Value.IsValid()) { return TEXT("null"); }
	switch (Value->Type)
	{
		case EJson::Null:    return TEXT("null");
		case EJson::String:  return TEXT("string");
		case EJson::Boolean: return TEXT("boolean");
		case EJson::Array:   return TEXT("array");
		case EJson::Object:  return TEXT("object");
		case EJson::Number:
		{
			const double N = Value->AsNumber();
			return (N == FMath::FloorToDouble(N)) ? TEXT("integer") : TEXT("number");
		}
		default: return TEXT("unknown");
	}
}

bool FVoidJsonSchemaLite::TypeMatches(const TSharedPtr<FJsonValue>& Value, const FString& SchemaType)
{
	const FString Actual = TypeName(Value);
	if (Actual == SchemaType) { return true; }
	return SchemaType == TEXT("number") && Actual == TEXT("integer"); // every integer is a number
}

bool FVoidJsonSchemaLite::Validate(const TSharedPtr<FJsonValue>& Value, const TSharedPtr<FJsonObject>& Schema, const FString& RootPath, TArray<FString>& OutViolations)
{
	const int32 Before = OutViolations.Num();
	if (Schema.IsValid())
	{
		ValidateInternal(Value, Schema, RootPath, OutViolations);
	}
	return OutViolations.Num() == Before;
}

void FVoidJsonSchemaLite::ValidateInternal(const TSharedPtr<FJsonValue>& Value, const TSharedPtr<FJsonObject>& Schema, const FString& Path, TArray<FString>& Out)
{
	if (!Value.IsValid()) { return; }

	// const
	if (Schema->HasField(TEXT("const")))
	{
		const TSharedPtr<FJsonValue> Expected = Schema->TryGetField(TEXT("const"));
		if (Expected.IsValid() && !FJsonValue::CompareEqual(*Expected, *Value))
		{
			Out.Add(FString::Printf(TEXT("%s: value does not equal the schema's required constant."), *Path));
		}
	}

	// enum
	const TArray<TSharedPtr<FJsonValue>>* EnumValues = nullptr;
	if (Schema->TryGetArrayField(TEXT("enum"), EnumValues))
	{
		bool bFound = false;
		for (const TSharedPtr<FJsonValue>& Candidate : *EnumValues)
		{
			if (Candidate.IsValid() && FJsonValue::CompareEqual(*Candidate, *Value)) { bFound = true; break; }
		}
		if (!bFound) { Out.Add(FString::Printf(TEXT("%s: value is not one of the schema's allowed enum values."), *Path)); }
	}

	// type (string or array of strings)
	bool bTypeOk = true;
	FString SingleType;
	const TArray<TSharedPtr<FJsonValue>>* TypeArray = nullptr;
	if (Schema->TryGetStringField(TEXT("type"), SingleType))
	{
		bTypeOk = TypeMatches(Value, SingleType);
		if (!bTypeOk) { Out.Add(FString::Printf(TEXT("%s: expected type '%s' but found '%s'."), *Path, *SingleType, *TypeName(Value))); }
	}
	else if (Schema->TryGetArrayField(TEXT("type"), TypeArray))
	{
		bTypeOk = false;
		TArray<FString> Allowed;
		for (const TSharedPtr<FJsonValue>& T : *TypeArray)
		{
			FString S;
			if (T.IsValid() && T->TryGetString(S)) { Allowed.Add(S); if (TypeMatches(Value, S)) { bTypeOk = true; } }
		}
		if (!bTypeOk) { Out.Add(FString::Printf(TEXT("%s: type '%s' is not one of [%s]."), *Path, *TypeName(Value), *FString::Join(Allowed, TEXT(", ")))); }
	}
	if (!bTypeOk) { return; }

	// minimum
	double Minimum = 0.0;
	if (Value->Type == EJson::Number && Schema->TryGetNumberField(TEXT("minimum"), Minimum) && Value->AsNumber() < Minimum)
	{
		Out.Add(FString::Printf(TEXT("%s: %g is below the minimum %g."), *Path, Value->AsNumber(), Minimum));
	}

	if (Value->Type == EJson::Object)
	{
		const TSharedPtr<FJsonObject> Obj = Value->AsObject();

		const TArray<TSharedPtr<FJsonValue>>* Required = nullptr;
		if (Schema->TryGetArrayField(TEXT("required"), Required))
		{
			for (const TSharedPtr<FJsonValue>& R : *Required)
			{
				FString Name;
				if (R.IsValid() && R->TryGetString(Name) && !Obj->HasField(Name))
				{
					Out.Add(FString::Printf(TEXT("%s: missing required property '%s'."), *Path, *Name));
				}
			}
		}

		const TSharedPtr<FJsonObject>* Props = nullptr;
		const bool bHasProps = Schema->TryGetObjectField(TEXT("properties"), Props);

		bool bAdditional = true;
		Schema->TryGetBoolField(TEXT("additionalProperties"), bAdditional);
		if (!bAdditional)
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Obj->Values)
			{
				if (!bHasProps || !(*Props)->HasField(Pair.Key))
				{
					Out.Add(FString::Printf(TEXT("%s: unexpected property '%s'."), *Path, *Pair.Key));
				}
			}
		}

		if (bHasProps)
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Props)->Values)
			{
				const TSharedPtr<FJsonObject>* SubSchema = nullptr;
				if (Obj->HasField(Pair.Key) && Pair.Value.IsValid() && Pair.Value->TryGetObject(SubSchema))
				{
					ValidateInternal(Obj->TryGetField(Pair.Key), *SubSchema, Path + TEXT(".") + Pair.Key, Out);
				}
			}
		}
	}

	if (Value->Type == EJson::Array)
	{
		const TArray<TSharedPtr<FJsonValue>>& Arr = Value->AsArray();
		double Bound = 0.0;
		if (Schema->TryGetNumberField(TEXT("minItems"), Bound) && Arr.Num() < static_cast<int32>(Bound))
		{
			Out.Add(FString::Printf(TEXT("%s: %d item(s) but minItems is %d."), *Path, Arr.Num(), static_cast<int32>(Bound)));
		}
		if (Schema->TryGetNumberField(TEXT("maxItems"), Bound) && Arr.Num() > static_cast<int32>(Bound))
		{
			Out.Add(FString::Printf(TEXT("%s: %d item(s) but maxItems is %d."), *Path, Arr.Num(), static_cast<int32>(Bound)));
		}
		const TSharedPtr<FJsonObject>* ItemSchema = nullptr;
		if (Schema->TryGetObjectField(TEXT("items"), ItemSchema))
		{
			for (int32 i = 0; i < Arr.Num(); ++i)
			{
				ValidateInternal(Arr[i], *ItemSchema, FString::Printf(TEXT("%s[%d]"), *Path, i), Out);
			}
		}
	}
}
