// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VoidJsonReader.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"

bool FVoidJsonReader::ReadFromFile(const FString& FilePath, TSharedPtr<FJsonObject>& OutJsonObject, FString& OutError)
{
	FString FileContents;
	if (!FFileHelper::LoadFileToString(FileContents, *FilePath))
	{
		OutError = FString::Printf(TEXT("Could not read file: %s"), *FilePath);
		return false;
	}

	return ParseString(FileContents, OutJsonObject, OutError);
}

bool FVoidJsonReader::ParseString(const FString& RawJson, TSharedPtr<FJsonObject>& OutJsonObject, FString& OutError)
{
	const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(RawJson);

	if (!FJsonSerializer::Deserialize(JsonReader, OutJsonObject) || !OutJsonObject.IsValid())
	{
		OutError = TEXT("Failed to parse JSON: input is malformed or not a JSON object.");
		return false;
	}

	OutError.Empty();
	return true;
}
