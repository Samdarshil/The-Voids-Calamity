// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VOIDWorldBuilderLightingModule.h"
#include "VoidLightingLog.h"
#include "Modules/ModuleManager.h"

void FVOIDWorldBuilderLightingModule::StartupModule()
{
	UE_LOG(LogVoidLighting, Log, TEXT("VOIDWorldBuilderLighting module started."));
}

void FVOIDWorldBuilderLightingModule::ShutdownModule()
{
	UE_LOG(LogVoidLighting, Log, TEXT("VOIDWorldBuilderLighting module shut down."));
}

IMPLEMENT_MODULE(FVOIDWorldBuilderLightingModule, VOIDWorldBuilderLighting)
