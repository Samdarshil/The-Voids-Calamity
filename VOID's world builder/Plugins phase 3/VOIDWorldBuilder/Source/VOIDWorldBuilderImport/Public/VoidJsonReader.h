// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * FVoidJsonReader
 *
 * Thin wrapper around Unreal's Json module. Isolated into its own class
 * (rather than calling FJsonSerializer directly from the importer) so
 * that if the design package source ever changes format -- a database, a
 * web endpoint, a binary format -- only this class and its call sites in
 * the importer need to change, not the importer's mapping logic.
 */
class VOIDWORLDBUILDERIMPORT_API FVoidJsonReader
{
public:
	/**
	 * Reads and parses a JSON file from disk.
	 * @param FilePath Absolute or engine-relative path to the .json file.
	 * @param OutJsonObject Receives the parsed root object on success.
	 * @param OutError Receives a human-readable error message on failure.
	 * @return true if the file was read and parsed as valid JSON.
	 */
	static bool ReadFromFile(const FString& FilePath, TSharedPtr<FJsonObject>& OutJsonObject, FString& OutError);

	/**
	 * Parses a raw JSON string already in memory.
	 * @param RawJson The JSON text to parse.
	 * @param OutJsonObject Receives the parsed root object on success.
	 * @param OutError Receives a human-readable error message on failure.
	 * @return true if the string was valid JSON.
	 */
	static bool ParseString(const FString& RawJson, TSharedPtr<FJsonObject>& OutJsonObject, FString& OutError);
};
