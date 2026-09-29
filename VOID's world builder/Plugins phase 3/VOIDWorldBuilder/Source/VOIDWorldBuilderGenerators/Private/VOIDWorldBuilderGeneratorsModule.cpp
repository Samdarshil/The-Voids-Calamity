// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VOIDWorldBuilderGeneratorsModule.h"
#include "VoidWorldBuilderLog.h"
#include "VoidGeneratorRegistry.h"
#include "Road/VoidRoadGenerator.h"

void FVOIDWorldBuilderGeneratorsModule::StartupModule()
{
	UE_LOG(LogVoidWorldBuilder, Log, TEXT("VOIDWorldBuilderGenerators module started."));

	// Register every built-in generator here. Adding a new generator
	// (Building, Navigation, etc.) in a later phase means adding one more
	// RegisterGenerator call (and the matching UnregisterGenerator below)
	// -- nothing else in the plugin changes.
	FVoidGeneratorRegistry::Get().RegisterGenerator(MakeShared<FVoidRoadGenerator>());
}

void FVOIDWorldBuilderGeneratorsModule::ShutdownModule()
{
	FVoidGeneratorRegistry::Get().UnregisterGenerator(TEXT("Road"));

	UE_LOG(LogVoidWorldBuilder, Log, TEXT("VOIDWorldBuilderGenerators module shut down."));
}

IMPLEMENT_MODULE(FVOIDWorldBuilderGeneratorsModule, VOIDWorldBuilderGenerators)
