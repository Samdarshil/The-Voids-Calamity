// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VOIDWorldBuilderImportModule.h"
#include "VoidWorldBuilderLog.h"
#include "VoidWorldBuilderImportLog.h"
#include "VoidPackageReaderRegistry.h"
#include "VoidJsonPackageReader.h"

void FVOIDWorldBuilderImportModule::StartupModule()
{
	UE_LOG(LogVoidWorldBuilder, Log, TEXT("VOIDWorldBuilderImport module started."));

	// Register every built-in format reader here. Adding a new format
	// later means adding one more RegisterReader call (and the matching
	// UnregisterReader below) -- nothing else in the plugin changes.
	FVoidPackageReaderRegistry::Get().RegisterReader(MakeShared<FVoidJsonPackageReader>());
}

void FVOIDWorldBuilderImportModule::ShutdownModule()
{
	FVoidPackageReaderRegistry::Get().UnregisterReader(TEXT("json"));

	UE_LOG(LogVoidWorldBuilder, Log, TEXT("VOIDWorldBuilderImport module shut down."));
}

IMPLEMENT_MODULE(FVOIDWorldBuilderImportModule, VOIDWorldBuilderImport)
