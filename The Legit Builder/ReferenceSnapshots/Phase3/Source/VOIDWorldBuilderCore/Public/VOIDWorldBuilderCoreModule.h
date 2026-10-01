// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * FVOIDWorldBuilderCoreModule
 *
 * Runtime module for VOID World Builder. Holds only plain data types
 * (design package structs, validation report structs) and interfaces
 * that the editor-only modules depend on. Contains no editor-only code,
 * so these types could in principle be consumed at runtime in a packaged
 * build without pulling in editor dependencies.
 *
 * This module has no runtime initialization work to do yet; the
 * interface implementation exists so it loads/unloads cleanly and so
 * future runtime-facing features have somewhere to hook in.
 */
class FVOIDWorldBuilderCoreModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface
};
