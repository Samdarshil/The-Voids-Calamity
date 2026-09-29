// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VOIDWorldBuilderGeneratorsModule.h"
#include "VoidWorldBuilderLog.h"

void FVOIDWorldBuilderGeneratorsModule::StartupModule()
{
	UE_LOG(LogVoidWorldBuilder, Log, TEXT("VOIDWorldBuilderGenerators module started (no generators registered yet - Phase 1)."));
}

void FVOIDWorldBuilderGeneratorsModule::ShutdownModule()
{
	UE_LOG(LogVoidWorldBuilder, Log, TEXT("VOIDWorldBuilderGenerators module shut down."));
}

IMPLEMENT_MODULE(FVOIDWorldBuilderGeneratorsModule, VOIDWorldBuilderGenerators)
