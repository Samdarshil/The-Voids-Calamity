// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "VoidWorldBuilderCommandlet.generated.h"

/**
 * UVoidWorldBuilderCommandlet
 *
 * Headless entry point for running the import pipeline outside the
 * Editor UI -- intended for CI: "does this design package still import
 * and validate cleanly" as an automated check on every change to a
 * design document export.
 *
 * Usage:
 *   UnrealEditor-Cmd.exe <Project> -run=VoidWorldBuilder -Package="C:/path/to/package.json"
 *
 * Phase 1 scope: import + validate + report exit code only. Does not
 * generate content -- there is nothing to generate yet.
 */
UCLASS()
class VOIDWORLDBUILDEREDITOR_API UVoidWorldBuilderCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UVoidWorldBuilderCommandlet();

	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet Interface
};
