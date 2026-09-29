// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class SDockTab;
class FSpawnTabArgs;

/**
 * FVOIDWorldBuilderEditorModule
 *
 * Thin editor-facing shell: registers the World Builder's tab spawner on
 * startup, unregisters it on shutdown. Contains no generation or import
 * logic itself -- it only wires up UI that calls into
 * VOIDWorldBuilderImport / VOIDWorldBuilderGenerators.
 */
class FVOIDWorldBuilderEditorModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

private:
	void RegisterTabSpawner();
	void UnregisterTabSpawner();

	TSharedRef<SDockTab> SpawnWorldBuilderTab(const FSpawnTabArgs& SpawnTabArgs);

	static const FName WorldBuilderTabName;
};
