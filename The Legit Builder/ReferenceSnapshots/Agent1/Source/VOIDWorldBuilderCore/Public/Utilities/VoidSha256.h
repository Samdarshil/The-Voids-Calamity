// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"

/**
 * FVoidSha256
 *
 * Minimal FIPS 180-4 SHA-256, used only to verify Meridian's PackageManifest.json checksums
 * (--strict-canon-lock). Self-contained on purpose: it does not depend on any engine hashing API
 * whose availability/shape could differ across engine versions. Output is lowercase hex, matching
 * the manifest's format.
 */
class VOIDWORLDBUILDERCORE_API FVoidSha256
{
public:
	/** Hash of a byte buffer, as 64 lowercase hex characters. */
	static FString HashBytes(const uint8* Data, uint64 NumBytes);

	static FString HashBytes(const TArray<uint8>& Data) { return HashBytes(Data.GetData(), static_cast<uint64>(Data.Num())); }

	/** Reads the file and hashes its raw bytes. Returns false if the file can't be read. */
	static bool HashFile(const FString& FilePath, FString& OutHexDigest);
};
