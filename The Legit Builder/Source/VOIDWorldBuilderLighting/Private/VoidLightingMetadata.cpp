// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VoidLightingMetadata.h"
#include "VoidLightingLog.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace VoidLightingMetadataPrivate
{
	static bool ParseRoot(const FString& JsonText, TSharedPtr<FJsonObject>& OutRoot)
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		return FJsonSerializer::Deserialize(Reader, OutRoot) && OutRoot.IsValid();
	}

	static FString GetString(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
	{
		FString Value;
		Obj->TryGetStringField(Field, Value);
		return Value;
	}
}

bool FVoidMeridianLightingMetadata::ParseDistrictRegistry(const FString& JsonText, TArray<FString>* OutMessages)
{
	using namespace VoidLightingMetadataPrivate;

	TSharedPtr<FJsonObject> Root;
	if (!ParseRoot(JsonText, Root))
	{
		if (OutMessages) { OutMessages->Add(TEXT("DistrictRegistry.json is not valid JSON.")); }
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* DistrictArray = nullptr;
	if (!Root->TryGetArrayField(TEXT("districts"), DistrictArray))
	{
		if (OutMessages) { OutMessages->Add(TEXT("DistrictRegistry.json has no 'districts' array.")); }
		return false;
	}

	Districts.Reset();
	for (const TSharedPtr<FJsonValue>& Value : *DistrictArray)
	{
		const TSharedPtr<FJsonObject> Obj = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!Obj.IsValid())
		{
			continue;
		}

		FVoidMeridianDistrictInfo Info;
		Info.Id = FName(*GetString(Obj, TEXT("id")));
		Info.DisplayName = GetString(Obj, TEXT("display_name"));
		Info.RadialBand = GetString(Obj, TEXT("radial_band"));
		Info.VerticalTier = GetString(Obj, TEXT("vertical_tier"));
		double Priority = 0.0;
		if (Obj->TryGetNumberField(TEXT("generation_priority"), Priority))
		{
			Info.GenerationPriority = static_cast<int32>(Priority);
		}
		if (Info.Id != NAME_None)
		{
			Districts.Add(MoveTemp(Info));
		}
	}
	return true;
}

bool FVoidMeridianLightingMetadata::ParseLandmarkRegistry(const FString& JsonText, TArray<FString>* OutMessages)
{
	using namespace VoidLightingMetadataPrivate;

	TSharedPtr<FJsonObject> Root;
	if (!ParseRoot(JsonText, Root))
	{
		if (OutMessages) { OutMessages->Add(TEXT("LandmarkRegistry.json is not valid JSON.")); }
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* LandmarkArray = nullptr;
	if (!Root->TryGetArrayField(TEXT("landmarks"), LandmarkArray))
	{
		if (OutMessages) { OutMessages->Add(TEXT("LandmarkRegistry.json has no 'landmarks' array.")); }
		return false;
	}

	Landmarks.Reset();
	for (const TSharedPtr<FJsonValue>& Value : *LandmarkArray)
	{
		const TSharedPtr<FJsonObject> Obj = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!Obj.IsValid())
		{
			continue;
		}

		FVoidMeridianLandmarkInfo Info;
		Info.Id = FName(*GetString(Obj, TEXT("id")));
		Info.DistrictId = FName(*GetString(Obj, TEXT("district")));
		Info.Type = GetString(Obj, TEXT("type"));
		Info.RecognitionPriority = GetString(Obj, TEXT("recognition_priority"));
		Info.NavigationImportance = GetString(Obj, TEXT("navigation_importance"));

		// visibility_tier is a number for skyline landmarks and a string
		// ("interior_not_applicable_to_skyline") for interiors. A string
		// therefore means "no skyline presence", not "unparseable".
		double Tier = 0.0;
		if (Obj->TryGetNumberField(TEXT("visibility_tier"), Tier))
		{
			Info.VisibilityTier = static_cast<int32>(Tier);
		}

		if (Info.Id != NAME_None)
		{
			Landmarks.Add(MoveTemp(Info));
		}
	}
	return true;
}

bool FVoidMeridianLightingMetadata::LoadFromDirectory(const FString& Directory, TArray<FString>* OutMessages)
{
	bool bAny = false;
	if (Directory.IsEmpty())
	{
		return false;
	}

	FString Text;
	const FString DistrictPath = FPaths::Combine(Directory, TEXT("DistrictRegistry.json"));
	if (FFileHelper::LoadFileToString(Text, *DistrictPath))
	{
		bAny |= ParseDistrictRegistry(Text, OutMessages);
	}
	else if (OutMessages)
	{
		OutMessages->Add(FString::Printf(TEXT("Not found: %s"), *DistrictPath));
	}

	const FString LandmarkPath = FPaths::Combine(Directory, TEXT("LandmarkRegistry.json"));
	if (FFileHelper::LoadFileToString(Text, *LandmarkPath))
	{
		bAny |= ParseLandmarkRegistry(Text, OutMessages);
	}
	else if (OutMessages)
	{
		OutMessages->Add(FString::Printf(TEXT("Not found: %s"), *LandmarkPath));
	}

	UE_LOG(LogVoidLighting, Log, TEXT("Meridian metadata from '%s': %d districts, %d landmarks."), *Directory, Districts.Num(), Landmarks.Num());
	return bAny;
}

const FVoidMeridianDistrictInfo* FVoidMeridianLightingMetadata::FindDistrict(FName Id) const
{
	return Districts.FindByPredicate([Id](const FVoidMeridianDistrictInfo& D) { return D.Id == Id; });
}

const FVoidMeridianLandmarkInfo* FVoidMeridianLightingMetadata::FindLandmark(FName Id) const
{
	return Landmarks.FindByPredicate([Id](const FVoidMeridianLandmarkInfo& L) { return L.Id == Id; });
}
