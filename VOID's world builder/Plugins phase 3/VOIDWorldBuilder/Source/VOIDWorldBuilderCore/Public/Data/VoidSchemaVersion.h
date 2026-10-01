// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "VoidSchemaVersion.generated.h"

/**
 * FVoidSchemaVersion
 *
 * Major.Minor version of the design-package JSON schema itself (not the
 * source design document's own version, which is a separate concept
 * tracked in FVoidPackageMetadata::SourceDocumentVersion).
 *
 * Compatibility policy, enforced by FVoidPackageValidator:
 *   - Package Major > tool's Major  -> Fatal (too new, cannot safely import).
 *   - Package Major < tool's Major  -> Fatal (too old, no migration path yet).
 *   - Package Minor > tool's Minor  -> Warning (newer optional fields may be ignored).
 *   - Package Minor <= tool's Minor -> fine.
 */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERCORE_API FVoidSchemaVersion
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	int32 Major = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	int32 Minor = 0;

	FVoidSchemaVersion() = default;

	FVoidSchemaVersion(int32 InMajor, int32 InMinor)
		: Major(InMajor)
		, Minor(InMinor)
	{
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("%d.%d"), Major, Minor);
	}

	/**
	 * Parses a "Major.Minor" string (e.g. "1.0"). Returns false and leaves
	 * OutVersion untouched if the string isn't in that exact shape --
	 * callers are expected to fall back to a sensible default and record a
	 * validation issue, not to crash.
	 */
	static bool TryParse(const FString& InString, FVoidSchemaVersion& OutVersion);

	/** The schema version this build of the importer understands. */
	static const FVoidSchemaVersion& CurrentToolVersion();

	bool operator==(const FVoidSchemaVersion& Other) const
	{
		return Major == Other.Major && Minor == Other.Minor;
	}
};
