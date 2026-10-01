// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VOIDWorldBuilderImportModule.h"
#include "VoidWorldBuilderLog.h"

void FVOIDWorldBuilderImportModule::StartupModule()
{
	UE_LOG(LogVoidWorldBuilder, Log, TEXT("VOIDWorldBuilderImport module started."));
}

void FVOIDWorldBuilderImportModule::ShutdownModule()
{
	UE_LOG(LogVoidWorldBuilder, Log, TEXT("VOIDWorldBuilderImport module shut down."));
}

IMPLEMENT_MODULE(FVOIDWorldBuilderImportModule, VOIDWorldBuilderImport)
