// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "VoidValidateCommandlet.generated.h"

/**
 * UVoidValidateCommandlet
 *
 * CI / headless entry point for first-look validation (no world needed):
 *
 *   UnrealEditor-Cmd <Project>.uproject -run=VoidValidate
 *       -Meridian="<dir containing Meridian_Master.json>"
 *       [-Package="<design package .json>"]     also run import + data/road/building validators
 *       [-Out="<file>.json|.md|.txt"]           default: <Project>/Saved/VOID/Validation/FirstLook.json (+ .md)
 *       [-AllowMissingDistricts]                missing locked district files become WARNING
 *       [-Strict]                               WARNINGs also fail the run
 *
 * Exit codes: 0 = pass, 1 = validation failed, 2 = bad usage.
 * Distinct from UVoidWorldBuilderCommandlet (-run=VoidWorldBuilder, import only).
 */
UCLASS()
class VOIDWORLDBUILDERVALIDATION_API UVoidValidateCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UVoidValidateCommandlet();
	virtual int32 Main(const FString& Params) override;
};
