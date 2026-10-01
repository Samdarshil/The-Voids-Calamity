// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VOIDWorldBuilderEditorModule.h"
#include "VoidWorldBuilderLog.h"
#include "SVoidWorldBuilderPanel.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "FVOIDWorldBuilderEditorModule"

const FName FVOIDWorldBuilderEditorModule::WorldBuilderTabName(TEXT("VOIDWorldBuilder"));

void FVOIDWorldBuilderEditorModule::StartupModule()
{
	UE_LOG(LogVoidWorldBuilder, Log, TEXT("VOIDWorldBuilderEditor module started."));
	RegisterTabSpawner();
}

void FVOIDWorldBuilderEditorModule::ShutdownModule()
{
	UnregisterTabSpawner();
	UE_LOG(LogVoidWorldBuilder, Log, TEXT("VOIDWorldBuilderEditor module shut down."));
}

void FVOIDWorldBuilderEditorModule::RegisterTabSpawner()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		WorldBuilderTabName,
		FOnSpawnTab::CreateRaw(this, &FVOIDWorldBuilderEditorModule::SpawnWorldBuilderTab))
		.SetDisplayName(LOCTEXT("TabTitle", "VOID World Builder"))
		.SetTooltipText(LOCTEXT("TabTooltip", "Import and validate VOID design packages."))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());
}

void FVOIDWorldBuilderEditorModule::UnregisterTabSpawner()
{
	if (FGlobalTabmanager::Get().IsValid())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(WorldBuilderTabName);
	}
}

TSharedRef<SDockTab> FVOIDWorldBuilderEditorModule::SpawnWorldBuilderTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SVoidWorldBuilderPanel)
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FVOIDWorldBuilderEditorModule, VOIDWorldBuilderEditor)
