// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VOIDWorldBuilderValidationModule.h"
#include "VoidValidationLog.h"
#include "VoidPackageValidators.h"
#include "VoidGeneratedWorldValidator.h"
#include "Interfaces/IVoidValidator.h"

DEFINE_LOG_CATEGORY(LogVoidValidation);

void FVOIDWorldBuilderValidationModule::StartupModule()
{
	UE_LOG(LogVoidValidation, Log, TEXT("VOIDWorldBuilderValidation module started."));

	FVoidValidatorRegistry& Registry = FVoidValidatorRegistry::Get();
	Registry.Register(MakeShared<FVoidPackageDataValidator>());
	Registry.Register(MakeShared<FVoidRoadNetworkValidator>());
	Registry.Register(MakeShared<FVoidBuildingHookValidator>());
	Registry.Register(MakeShared<FVoidGeneratedWorldValidator>());
}

void FVOIDWorldBuilderValidationModule::ShutdownModule()
{
	FVoidValidatorRegistry& Registry = FVoidValidatorRegistry::Get();
	Registry.Unregister(TEXT("VOID.Package.Data"));
	Registry.Unregister(TEXT("VOID.Package.Roads"));
	Registry.Unregister(TEXT("VOID.Package.Buildings"));
	Registry.Unregister(TEXT("VOID.World.Generated"));

	UE_LOG(LogVoidValidation, Log, TEXT("VOIDWorldBuilderValidation module shut down."));
}

IMPLEMENT_MODULE(FVOIDWorldBuilderValidationModule, VOIDWorldBuilderValidation)
