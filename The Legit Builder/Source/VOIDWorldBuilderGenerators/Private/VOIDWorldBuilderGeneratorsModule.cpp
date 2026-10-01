// Copyright VOID Engineering Studio. Merged Agent 1-7 registration set.
#include "VOIDWorldBuilderGeneratorsModule.h"
#include "VoidWorldBuilderLog.h"
#include "VoidGeneratorRegistry.h"
#include "Road/VoidRoadGenerator.h"
#include "Metro/VoidMetroGenerator.h"
#include "Building/VoidBuildingGenerator.h"
#include "District/VoidDistrictGenerator.h"
#include "Environment/VoidDressingGenerator.h"
#include "Lighting/VoidLightingGenerator.h"

void FVOIDWorldBuilderGeneratorsModule::StartupModule()
{
    UE_LOG(LogVoidWorldBuilder, Log, TEXT("VOIDWorldBuilderGenerators module started."));
    FVoidGeneratorRegistry::Get().RegisterGenerator(MakeShared<FVoidRoadGenerator>());
    FVoidGeneratorRegistry::Get().RegisterGenerator(MakeShared<FVoidMetroGenerator>());
    FVoidGeneratorRegistry::Get().RegisterGenerator(MakeShared<FVoidBuildingGenerator>());
    FVoidGeneratorRegistry::Get().RegisterGenerator(MakeShared<FVoidDistrictGenerator>());
    FVoidGeneratorRegistry::Get().RegisterGenerator(MakeShared<FVoidEnvironmentGenerator>());
    FVoidGeneratorRegistry::Get().RegisterGenerator(MakeShared<FVoidPropGenerator>());
    FVoidGeneratorRegistry::Get().RegisterGenerator(MakeShared<FVoidLightingGenerator>());
}

void FVOIDWorldBuilderGeneratorsModule::ShutdownModule()
{
    FVoidGeneratorRegistry::Get().UnregisterGenerator(TEXT("Lighting"));
    FVoidGeneratorRegistry::Get().UnregisterGenerator(TEXT("Props"));
    FVoidGeneratorRegistry::Get().UnregisterGenerator(TEXT("Environment"));
    FVoidGeneratorRegistry::Get().UnregisterGenerator(TEXT("District"));
    FVoidGeneratorRegistry::Get().UnregisterGenerator(TEXT("Building"));
    FVoidGeneratorRegistry::Get().UnregisterGenerator(TEXT("Metro"));
    FVoidGeneratorRegistry::Get().UnregisterGenerator(TEXT("Road"));
    UE_LOG(LogVoidWorldBuilder, Log, TEXT("VOIDWorldBuilderGenerators module shut down."));
}
IMPLEMENT_MODULE(FVOIDWorldBuilderGeneratorsModule, VOIDWorldBuilderGenerators)
