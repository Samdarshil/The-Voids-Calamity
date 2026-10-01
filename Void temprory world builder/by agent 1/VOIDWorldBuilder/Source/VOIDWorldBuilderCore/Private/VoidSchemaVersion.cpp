// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Data/VoidSchemaVersion.h"

bool FVoidSchemaVersion::TryParse(const FString& InString, FVoidSchemaVersion& OutVersion)
{
	FString MajorString;
	FString MinorString;
	if (!InString.Split(TEXT("."), &MajorString, &MinorString))
	{
		return false;
	}

	if (MajorString.IsEmpty() || MinorString.IsEmpty() || !MajorString.IsNumeric() || !MinorString.IsNumeric())
	{
		return false;
	}

	OutVersion.Major = FCString::Atoi(*MajorString);
	OutVersion.Minor = FCString::Atoi(*MinorString);
	return true;
}

const FVoidSchemaVersion& FVoidSchemaVersion::CurrentToolVersion()
{
	static const FVoidSchemaVersion Version(1, 0);
	return Version;
}
