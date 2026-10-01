// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/** Registers the built-in validators with FVoidValidatorRegistry (Core). Leaf module: nothing depends on it. */
class FVOIDWorldBuilderValidationModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
