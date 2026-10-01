// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * FVOIDWorldBuilderGeneratorsModule
 *
 * Owns the generator registry. Phase 1 contains no generator
 * implementations -- Road, Building, District, Navigation, World
 * Partition, Data Layer, and Gameplay Volume generators are added in
 * Phases 3 through 9, each registering itself with
 * FVoidGeneratorRegistry on module startup once it exists.
 */
class FVOIDWorldBuilderGeneratorsModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface
};
