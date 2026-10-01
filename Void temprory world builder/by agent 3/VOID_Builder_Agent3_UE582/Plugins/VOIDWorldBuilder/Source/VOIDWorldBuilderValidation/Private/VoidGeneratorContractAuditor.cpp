// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VoidGeneratorContractAuditor.h"
#include "VoidGeneratorRegistry.h"

const TArray<FName>& FVoidGeneratorContractAuditor::GetExpectedGeneratorIds()
{
	static const TArray<FName> Ids = {
		FName(TEXT("Road")), FName(TEXT("District")), FName(TEXT("Building")), FName(TEXT("Environment")),
		FName(TEXT("Lighting")), FName(TEXT("Props")), FName(TEXT("Metro")) };
	return Ids;
}

EVoidValidationStage FVoidGeneratorContractAuditor::GetStageForGenerator(FName GeneratorId)
{
	if (GeneratorId == TEXT("Road"))        { return EVoidValidationStage::PostRoad; }
	if (GeneratorId == TEXT("District"))    { return EVoidValidationStage::PostDistrict; }
	if (GeneratorId == TEXT("Building"))    { return EVoidValidationStage::PostBuilding; }
	if (GeneratorId == TEXT("Environment")) { return EVoidValidationStage::PostEnvironment; }
	if (GeneratorId == TEXT("Lighting"))    { return EVoidValidationStage::PostLighting; }
	if (GeneratorId == TEXT("Props"))       { return EVoidValidationStage::PostProps; }
	if (GeneratorId == TEXT("Metro"))       { return EVoidValidationStage::PostMetro; }
	return EVoidValidationStage::Final;
}

void FVoidGeneratorContractAuditor::Audit(const FVoidGeneratorRegistry& Generators, const FVoidValidatorRegistry& Validators, FVoidValidationContext& Ctx)
{
	Ctx.SetSubsystem(TEXT("Contract"));
	Ctx.SetSource(TEXT("Registry audit"));
	const TArray<FName>& Expected = GetExpectedGeneratorIds();

	for (const FName& Id : Generators.GetCollidedIds())
	{
		Ctx.Error(TEXT("VOID.Contract.DuplicateGeneratorId"), FString::Printf(TEXT("Generator id '%s' was registered more than once; the later registration silently replaced the earlier one."), *Id.ToString()), Id.ToString(), FString(), TEXT("Two modules claim the same generator id (ids compare case-insensitively). Give each generator a unique id, or remove the duplicate registration."));
	}
	for (const FName& Id : Validators.GetRejectedDuplicateIds())
	{
		Ctx.Error(TEXT("VOID.Contract.DuplicateValidatorId"), FString::Printf(TEXT("Validator id '%s' was registered more than once; the later registration was rejected."), *Id.ToString()), Id.ToString(), FString(), TEXT("Give each validator a unique id (convention: VOID.<Area>.<Name>)."));
	}

	for (const FName& Id : Expected)
	{
		if (!Generators.FindGenerator(Id).IsValid())
		{
			Ctx.Warn(TEXT("VOID.Contract.GeneratorNotRegistered"), FString::Printf(TEXT("Expected generator '%s' is not registered."), *Id.ToString()), Id.ToString(), FString(), TEXT("Expected until the module that implements it is integrated: its StartupModule must call FVoidGeneratorRegistry::Get().RegisterGenerator with GetGeneratorId() == this exact id."));
		}
	}

	for (const FName& Id : Generators.GetRegisteredIdsSorted())
	{
		const FString IdString = Id.ToString();
		const TSharedPtr<IVoidGenerator> Gen = Generators.FindGenerator(Id);
		if (IdString.IsEmpty() || IdString.TrimStartAndEnd().Len() != IdString.Len() || IdString.Contains(TEXT(" ")))
		{
			Ctx.Error(TEXT("VOID.Contract.InvalidGeneratorId"), FString::Printf(TEXT("Generator id '%s' is blank or contains whitespace."), *IdString), IdString, FString(), TEXT("Use a short PascalCase id such as 'Building'."));
		}
		if (Gen.IsValid())
		{
			const FName First = Gen->GetGeneratorId();
			const FName Second = Gen->GetGeneratorId();
			if (First != Id || Second != First)
			{
				Ctx.Error(TEXT("VOID.Contract.UnstableGeneratorId"), FString::Printf(TEXT("Generator registered as '%s' reports ids '%s' then '%s'."), *IdString, *First.ToString(), *Second.ToString()), IdString, FString(), TEXT("GetGeneratorId() must return a constant."));
			}
		}
		if (!Expected.Contains(Id))
		{
			FString Suggestion;
			for (const FName& E : Expected) { if (IdString.Contains(E.ToString())) { Suggestion = E.ToString(); break; } }
			if (!Suggestion.IsEmpty())
			{
				Ctx.Warn(TEXT("VOID.Contract.NonCanonicalGeneratorId"), FString::Printf(TEXT("Generator id '%s' looks like canonical id '%s'; the pipeline will not find it under the canonical name."), *IdString, *Suggestion), IdString, FString(), FString::Printf(TEXT("Rename the id to '%s'."), *Suggestion));
			}
			else
			{
				Ctx.Info(TEXT("VOID.Contract.UnexpectedGenerator"), FString::Printf(TEXT("Generator '%s' is not one of the canonical ids; the default pipeline will not run it."), *IdString), IdString, FString(), TEXT("Add a custom FVoidPipelineDefinition step for it, or use a canonical id."));
			}
		}
		else
		{
			const EVoidValidationStage Stage = GetStageForGenerator(Id);
			if (Validators.GetForStage(Stage).Num() == 0)
			{
				Ctx.Info(TEXT("VOID.Contract.StageWithoutValidator"), FString::Printf(TEXT("Generator '%s' is registered but no validator applies to stage %s; its output is unchecked until Final."), *IdString, VoidValidationStageToString(Stage)), IdString, FString(), TEXT("Register an IVoidValidator for this stage."));
			}
		}
	}
	Ctx.FlushSuppressionSummary();
}
