// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VOIDWorldBuilderCoreModule.h"
#include "VoidWorldBuilderLog.h"

void FVOIDWorldBuilderCoreModule::StartupModule()
{
	UE_LOG(LogVoidWorldBuilder, Log, TEXT("VOIDWorldBuilderCore module started."));
}

void FVOIDWorldBuilderCoreModule::ShutdownModule()
{
	UE_LOG(LogVoidWorldBuilder, Log, TEXT("VOIDWorldBuilderCore module shut down."));
}

IMPLEMENT_MODULE(FVOIDWorldBuilderCoreModule, VOIDWorldBuilderCore)
