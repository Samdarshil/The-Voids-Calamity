// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VoidJsonSchemaSubset.h"
#include "Internationalization/Regex.h"

namespace
{
	struct FRun
	{
		const FString& File;
		FVoidValidationContext& Ctx;
		int32 Violations = 0;

		void Fail(const TCHAR* Code, const FString& Path, const FString& Message, const TCHAR* Fix)
		{
			++Violations;
			Ctx.Error(FName(Code), FString::Printf(TEXT("%s: %s"), *File, *Message), File, Path, Fix);
		}
	};

	TSharedPtr<FJsonValue> Field(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key)
	{
		if (!Object.IsValid())
		{
			return nullptr;
		}
		const TSharedPtr<FJsonValue>* Found = Object->Values.Find(FString(Key));
		return Found ? *Found : nullptr;
	}

	const TCHAR* JsonTypeName(EJson Type)
	{
		switch (Type)
		{
			case EJson::Object:  return TEXT("object");
			case EJson::Array:   return TEXT("array");
			case EJson::String:  return TEXT("string");
			case EJson::Number:  return TEXT("number");
			case EJson::Boolean: return TEXT("boolean");
			case EJson::Null:    return TEXT("null");
			default:             return TEXT("none");
		}
	}

	bool TypeMatches(const TSharedPtr<FJsonValue>& Value, const FString& TypeName)
	{
		const EJson Type = Value->Type;
		if (TypeName == TEXT("object"))  { return Type == EJson::Object; }
		if (TypeName == TEXT("array"))   { return Type == EJson::Array; }
		if (TypeName == TEXT("string"))  { return Type == EJson::String; }
		if (TypeName == TEXT("boolean")) { return Type == EJson::Boolean; }
		if (TypeName == TEXT("null"))    { return Type == EJson::Null; }
		if (TypeName == TEXT("number"))  { return Type == EJson::Number; }
		if (TypeName == TEXT("integer"))
		{
			if (Type != EJson::Number) { return false; }
			const double N = Value->AsNumber();
			return FMath::IsFinite(N) && FMath::FloorToDouble(N) == N;
		}
		return true; // unknown type keyword: do not fail
	}

	/** Scalar-only equality (const/enum/uniqueItems). Objects/arrays are treated as not equal. */
	bool ScalarEquals(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
	{
		if (A->Type != B->Type) { return false; }
		switch (A->Type)
		{
			case EJson::String:  return A->AsString() == B->AsString();
			case EJson::Number:  return A->AsNumber() == B->AsNumber();
			case EJson::Boolean: return A->AsBool() == B->AsBool();
			case EJson::Null:    return true;
			default:             return false;
		}
	}

	FString Describe(const TSharedPtr<FJsonValue>& V)
	{
		switch (V->Type)
		{
			case EJson::String:  return FString::Printf(TEXT("\"%s\""), *V->AsString());
			case EJson::Number:  return FString::SanitizeFloat(V->AsNumber());
			case EJson::Boolean: return V->AsBool() ? TEXT("true") : TEXT("false");
			case EJson::Null:    return TEXT("null");
			default:             return JsonTypeName(V->Type);
		}
	}

	void Node(FRun& Run, const TSharedPtr<FJsonValue>& Inst, const TSharedPtr<FJsonObject>& Schema, const FString& Path)
	{
		if (!Inst.IsValid() || !Schema.IsValid())
		{
			return;
		}

		if (const TSharedPtr<FJsonValue> Const = Field(Schema, TEXT("const")))
		{
			if (!ScalarEquals(Inst, Const))
			{
				Run.Fail(TEXT("VOID.Schema.ConstMismatch"), Path, FString::Printf(TEXT("expected constant %s but found %s"), *Describe(Const), *Describe(Inst)), TEXT("Set the value the schema requires."));
			}
		}

		if (const TSharedPtr<FJsonValue> Enum = Field(Schema, TEXT("enum")))
		{
			if (Enum->Type == EJson::Array)
			{
				bool bFound = false;
				FString Allowed;
				for (const TSharedPtr<FJsonValue>& Option : Enum->AsArray())
				{
					bFound = bFound || ScalarEquals(Inst, Option);
					Allowed += (Allowed.IsEmpty() ? TEXT("") : TEXT(", ")) + Describe(Option);
				}
				if (!bFound)
				{
					Run.Fail(TEXT("VOID.Schema.EnumMismatch"), Path, FString::Printf(TEXT("value %s is not one of [%s]"), *Describe(Inst), *Allowed), TEXT("Use one of the allowed values."));
				}
			}
		}

		if (const TSharedPtr<FJsonValue> TypeField = Field(Schema, TEXT("type")))
		{
			TArray<FString> Types;
			if (TypeField->Type == EJson::String)
			{
				Types.Add(TypeField->AsString());
			}
			else if (TypeField->Type == EJson::Array)
			{
				for (const TSharedPtr<FJsonValue>& T : TypeField->AsArray())
				{
					if (T->Type == EJson::String) { Types.Add(T->AsString()); }
				}
			}
			bool bOk = Types.Num() == 0;
			for (const FString& T : Types) { bOk = bOk || TypeMatches(Inst, T); }
			if (!bOk)
			{
				Run.Fail(TEXT("VOID.Schema.WrongType"), Path, FString::Printf(TEXT("expected %s but found %s"), *FString::Join(Types, TEXT("/")), JsonTypeName(Inst->Type)), TEXT("Change the value to the required JSON type."));
				return; // sub-keyword checks below assume the right type
			}
		}

		switch (Inst->Type)
		{
			case EJson::String:
			{
				const FString& Str = Inst->AsString();
				if (const TSharedPtr<FJsonValue> MinLen = Field(Schema, TEXT("minLength")))
				{
					if (MinLen->Type == EJson::Number && Str.Len() < static_cast<int32>(MinLen->AsNumber()))
					{
						Run.Fail(TEXT("VOID.Schema.MinLength"), Path, FString::Printf(TEXT("string is %d characters, minimum %d"), Str.Len(), static_cast<int32>(MinLen->AsNumber())), TEXT("Provide a non-empty value."));
					}
				}
				if (const TSharedPtr<FJsonValue> Pattern = Field(Schema, TEXT("pattern")))
				{
					if (Pattern->Type == EJson::String)
					{
						const FRegexPattern Regex(Pattern->AsString());
						FRegexMatcher Matcher(Regex, Str);
						if (!Matcher.FindNext())
						{
							Run.Fail(TEXT("VOID.Schema.Pattern"), Path, FString::Printf(TEXT("\"%s\" does not match pattern %s"), *Str, *Pattern->AsString()), TEXT("Use a value matching the schema pattern."));
						}
					}
				}
				break;
			}
			case EJson::Number:
			{
				const double N = Inst->AsNumber();
				const TSharedPtr<FJsonValue> Min = Field(Schema, TEXT("minimum"));
				const TSharedPtr<FJsonValue> Max = Field(Schema, TEXT("maximum"));
				if (Min && Min->Type == EJson::Number && N < Min->AsNumber())
				{
					Run.Fail(TEXT("VOID.Schema.Range"), Path, FString::Printf(TEXT("%s is below minimum %s"), *FString::SanitizeFloat(N), *FString::SanitizeFloat(Min->AsNumber())), TEXT("Raise the value to the allowed range."));
				}
				if (Max && Max->Type == EJson::Number && N > Max->AsNumber())
				{
					Run.Fail(TEXT("VOID.Schema.Range"), Path, FString::Printf(TEXT("%s is above maximum %s"), *FString::SanitizeFloat(N), *FString::SanitizeFloat(Max->AsNumber())), TEXT("Lower the value to the allowed range."));
				}
				break;
			}
			case EJson::Array:
			{
				const TArray<TSharedPtr<FJsonValue>>& Items = Inst->AsArray();
				if (const TSharedPtr<FJsonValue> MinItems = Field(Schema, TEXT("minItems")))
				{
					if (MinItems->Type == EJson::Number && Items.Num() < static_cast<int32>(MinItems->AsNumber()))
					{
						Run.Fail(TEXT("VOID.Schema.ItemCount"), Path, FString::Printf(TEXT("array has %d item(s), minimum %d"), Items.Num(), static_cast<int32>(MinItems->AsNumber())), TEXT("Add the missing entries."));
					}
				}
				if (const TSharedPtr<FJsonValue> MaxItems = Field(Schema, TEXT("maxItems")))
				{
					if (MaxItems->Type == EJson::Number && Items.Num() > static_cast<int32>(MaxItems->AsNumber()))
					{
						Run.Fail(TEXT("VOID.Schema.ItemCount"), Path, FString::Printf(TEXT("array has %d item(s), maximum %d"), Items.Num(), static_cast<int32>(MaxItems->AsNumber())), TEXT("Remove the extra entries."));
					}
				}
				if (const TSharedPtr<FJsonValue> Unique = Field(Schema, TEXT("uniqueItems")))
				{
					if (Unique->Type == EJson::Boolean && Unique->AsBool() && Items.Num() <= 4096) // scalar O(n^2) is fine at schema-scale sizes; skip enormous arrays
					{
						for (int32 i = 0; i < Items.Num(); ++i)
						{
							for (int32 j = i + 1; j < Items.Num(); ++j)
							{
								if (ScalarEquals(Items[i], Items[j]))
								{
									Run.Fail(TEXT("VOID.Schema.UniqueItems"), FString::Printf(TEXT("%s[%d]"), *Path, j), FString::Printf(TEXT("duplicate item %s"), *Describe(Items[j])), TEXT("Remove the duplicate."));
								}
							}
						}
					}
				}
				const TSharedPtr<FJsonValue> ItemSchema = Field(Schema, TEXT("items"));
				if (ItemSchema && ItemSchema->Type == EJson::Object)
				{
					const TSharedPtr<FJsonObject> ItemSchemaObject = ItemSchema->AsObject();
					for (int32 Index = 0; Index < Items.Num(); ++Index)
					{
						Node(Run, Items[Index], ItemSchemaObject, FString::Printf(TEXT("%s[%d]"), *Path, Index));
					}
				}
				break;
			}
			case EJson::Object:
			{
				const TSharedPtr<FJsonObject> Object = Inst->AsObject();
				if (const TSharedPtr<FJsonValue> Required = Field(Schema, TEXT("required")))
				{
					if (Required->Type == EJson::Array)
					{
						for (const TSharedPtr<FJsonValue>& Name : Required->AsArray())
						{
							if (Name->Type == EJson::String && !Object->Values.Contains(Name->AsString()))
							{
								Run.Fail(TEXT("VOID.Schema.MissingField"), Path, FString::Printf(TEXT("missing required field '%s'"), *Name->AsString()), TEXT("Add the required field."));
							}
						}
					}
				}
				const int32 NumProps = Object->Values.Num();
				const TSharedPtr<FJsonValue> MinProps = Field(Schema, TEXT("minProperties"));
				const TSharedPtr<FJsonValue> MaxProps = Field(Schema, TEXT("maxProperties"));
				if (MinProps && MinProps->Type == EJson::Number && NumProps < static_cast<int32>(MinProps->AsNumber()))
				{
					Run.Fail(TEXT("VOID.Schema.PropertyCount"), Path, FString::Printf(TEXT("object has %d propert(ies), minimum %d"), NumProps, static_cast<int32>(MinProps->AsNumber())), TEXT("Add the missing properties."));
				}
				if (MaxProps && MaxProps->Type == EJson::Number && NumProps > static_cast<int32>(MaxProps->AsNumber()))
				{
					Run.Fail(TEXT("VOID.Schema.PropertyCount"), Path, FString::Printf(TEXT("object has %d propert(ies), maximum %d"), NumProps, static_cast<int32>(MaxProps->AsNumber())), TEXT("Remove the extra properties."));
				}

				TSharedPtr<FJsonObject> Props;
				if (const TSharedPtr<FJsonValue> PropsField = Field(Schema, TEXT("properties")))
				{
					if (PropsField->Type == EJson::Object) { Props = PropsField->AsObject(); }
				}
				const TSharedPtr<FJsonValue> Additional = Field(Schema, TEXT("additionalProperties"));

				for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
				{
					const FString ChildPath = FString::Printf(TEXT("%s.%s"), *Path, *Pair.Key);
					const TSharedPtr<FJsonValue> PropSchema = Field(Props, *Pair.Key);
					if (PropSchema && PropSchema->Type == EJson::Object)
					{
						Node(Run, Pair.Value, PropSchema->AsObject(), ChildPath);
					}
					else if (Additional && Additional->Type == EJson::Boolean && !Additional->AsBool())
					{
						Run.Fail(TEXT("VOID.Schema.UnknownField"), Path, FString::Printf(TEXT("unexpected field '%s' (schema forbids additional properties)"), *Pair.Key), TEXT("Remove the field, or add it to the schema through the package owner."));
					}
					else if (Additional && Additional->Type == EJson::Object)
					{
						Node(Run, Pair.Value, Additional->AsObject(), ChildPath);
					}
				}
				break;
			}
			default:
				break;
		}
	}
}

int32 FVoidJsonSchemaSubset::Validate(const TSharedPtr<FJsonValue>& Instance, const TSharedPtr<FJsonObject>& Schema, const FString& FileLabel, FVoidValidationContext& Context)
{
	FRun Run{ FileLabel, Context };
	Node(Run, Instance, Schema, TEXT("$"));
	return Run.Violations;
}
