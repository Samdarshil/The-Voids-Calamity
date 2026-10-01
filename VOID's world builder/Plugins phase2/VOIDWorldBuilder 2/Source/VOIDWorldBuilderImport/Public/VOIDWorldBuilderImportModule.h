// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * FVOIDWorldBuilderImportModule
 *
 * Owns the import pipeline (FVoidJsonReader, FVoidDesignPackageImporter,
 * FVoidPackageValidator). These are plain classes with static entry
 * points, not services the module needs to spin up -- StartupModule
 * exists to log that the module is present and to give future work
 * (e.g. a project-settings object for schema paths) somewhere to live.
 */
class FVOIDWorldBuilderImportModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface
};
