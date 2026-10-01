// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Data/VoidSchemaVersion.h"
#include "VoidImportSettings.generated.h"

/**
 * UVoidImportSettings
 *
 * Project-wide configuration for the import pipeline, exposed under
 * Edit -> Project Settings -> Plugins -> VOID World Builder Import.
 * Using UDeveloperSettings rather than a hand-built Slate settings panel
 * means this shows up in the standard Project Settings UI, is
 * per-project saved to Config/DefaultVOIDWorldBuilder.ini, and is
 * searchable/diffable like every other engine setting -- for free.
 *
 * FVoidImportContext snapshots these values at the start of each import
 * call (see FVoidDesignPackageImporter); readers and the validator never
 * read this class directly, so Core stays decoupled from
 * UDeveloperSettings entirely.
 */
UCLASS(Config = VOIDWorldBuilder, DefaultConfig, meta = (DisplayName = "VOID World Builder Import"))
class VOIDWORLDBUILDERIMPORT_API UVoidImportSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UVoidImportSettings();

	/**
	 * If true, an unrecognized field in a design package JSON object is
	 * reported as an Error (blocks generation). If false (default), it's
	 * reported as Info and otherwise ignored -- the tolerant default,
	 * since unknown fields are often a newer-schema-minor-version field
	 * this build simply doesn't know about yet.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Validation", meta = (DisplayName = "Fail On Unknown Fields"))
	bool bFailOnUnknownFields = false;

	/**
	 * The oldest package schema version this project intends to support
	 * importing. Currently informational / reserved for a future
	 * migration system; the hard compatibility gate enforced today is
	 * FVoidSchemaVersion::CurrentToolVersion()'s major-version match,
	 * applied in FVoidPackageValidator.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Validation", meta = (DisplayName = "Minimum Supported Schema Version"))
	FVoidSchemaVersion MinimumSupportedSchemaVersion;
};
