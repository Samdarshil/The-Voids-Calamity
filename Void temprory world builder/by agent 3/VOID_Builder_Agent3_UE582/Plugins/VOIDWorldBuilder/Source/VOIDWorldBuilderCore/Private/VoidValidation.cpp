// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Interfaces/IVoidValidator.h"
#include "Data/VoidGeneratedTags.h"
#include "VoidWorldBuilderLog.h"
#include "GameFramework/Actor.h"

const TCHAR* VoidValidationStageToString(EVoidValidationStage Stage)
{
	switch (Stage)
	{
		case EVoidValidationStage::PostImport:      return TEXT("PostImport");
		case EVoidValidationStage::PostRoad:        return TEXT("PostRoad");
		case EVoidValidationStage::PostDistrict:    return TEXT("PostDistrict");
		case EVoidValidationStage::PostBuilding:    return TEXT("PostBuilding");
		case EVoidValidationStage::PostEnvironment: return TEXT("PostEnvironment");
		case EVoidValidationStage::PostLighting:    return TEXT("PostLighting");
		case EVoidValidationStage::PostProps:       return TEXT("PostProps");
		case EVoidValidationStage::PostMetro:       return TEXT("PostMetro");
		case EVoidValidationStage::Final:           return TEXT("Final");
	}
	return TEXT("Unknown");
}

// ---------------------------------------------------------------- Context

FVoidValidationContext::FVoidValidationContext(FVoidValidationReport& InReport, const FVoidValidationOptions& InOptions, FName InSubsystem, FString InSource)
	: Options(InOptions)
	, Report(InReport)
	, Subsystem(InSubsystem)
	, Source(MoveTemp(InSource))
{
}

void FVoidValidationContext::Emit(EVoidValidationSeverity Severity, FName Code, FString Message, FString ObjectId, FString FieldPath, FString SuggestedFix, const FVector* Location)
{
	int32& Count = CodeCounts.FindOrAdd(Code, 0);
	++Count;

	const bool bBlocking = (Severity == EVoidValidationSeverity::Error || Severity == EVoidValidationSeverity::Fatal);
	if (Options.MaxIssuesPerCode > 0 && Count > Options.MaxIssuesPerCode)
	{
		SuppressedCounts.FindOrAdd(Code, 0) += 1;
		if (bBlocking)
		{
			Report.bIsValid = false; // a suppressed error is still an error
		}
		return;
	}

	FVoidValidationIssue Issue(Severity, MoveTemp(Message), MoveTemp(FieldPath), Code, MoveTemp(SuggestedFix));
	Issue.Subsystem = Subsystem;
	Issue.ObjectId = MoveTemp(ObjectId);
	Issue.Source = Source;
	if (Location)
	{
		Issue.WithLocation(*Location);
	}
	Report.AddIssue(MoveTemp(Issue));
}

void FVoidValidationContext::FlushSuppressionSummary()
{
	for (const TPair<FName, int32>& Pair : SuppressedCounts)
	{
		FVoidValidationIssue Issue(
			EVoidValidationSeverity::Info,
			FString::Printf(TEXT("%d further '%s' issue(s) were suppressed after the first %d (all still count toward validity)."), Pair.Value, *Pair.Key.ToString(), Options.MaxIssuesPerCode),
			FString(), FName(TEXT("VOID.Validation.IssuesSuppressed")),
			TEXT("Fix the reported instances first; re-run to see the rest, or raise FVoidValidationOptions::MaxIssuesPerCode."));
		Issue.Subsystem = Subsystem;
		Issue.Source = Source;
		Report.AddIssue(MoveTemp(Issue));
	}
	SuppressedCounts.Reset();
}

// --------------------------------------------------------------- Registry

FVoidValidatorRegistry& FVoidValidatorRegistry::Get()
{
	static FVoidValidatorRegistry Instance;
	return Instance;
}

bool FVoidValidatorRegistry::Register(TSharedRef<IVoidValidator> Validator)
{
	const FName Id = Validator->GetValidatorId();
	if (Validators.Contains(Id))
	{
		RejectedDuplicateIds.AddUnique(Id);
		UE_LOG(LogVoidWorldBuilder, Error, TEXT("Validator id '%s' is already registered; the new registration was REJECTED (ownership conflict between two modules)."), *Id.ToString());
		return false;
	}
	Validators.Add(Id, Validator);
	UE_LOG(LogVoidWorldBuilder, Log, TEXT("Registered validator '%s' (subsystem '%s')."), *Id.ToString(), *Validator->GetSubsystem().ToString());
	return true;
}

void FVoidValidatorRegistry::Unregister(FName ValidatorId)
{
	Validators.Remove(ValidatorId);
}

TSharedPtr<IVoidValidator> FVoidValidatorRegistry::Find(FName ValidatorId) const
{
	if (const TSharedRef<IVoidValidator>* Found = Validators.Find(ValidatorId))
	{
		return *Found;
	}
	return nullptr;
}

TArray<TSharedRef<IVoidValidator>> FVoidValidatorRegistry::GetAll() const
{
	TArray<TSharedRef<IVoidValidator>> Result;
	Validators.GenerateValueArray(Result);
	Result.Sort([](const TSharedRef<IVoidValidator>& A, const TSharedRef<IVoidValidator>& B)
	{
		if (A->GetOrder() != B->GetOrder())
		{
			return A->GetOrder() < B->GetOrder();
		}
		return A->GetValidatorId().ToString() < B->GetValidatorId().ToString();
	});
	return Result;
}

TArray<TSharedRef<IVoidValidator>> FVoidValidatorRegistry::GetForStage(EVoidValidationStage Stage) const
{
	TArray<TSharedRef<IVoidValidator>> Result;
	for (const TSharedRef<IVoidValidator>& Validator : GetAll())
	{
		if (Validator->AppliesToStage(Stage))
		{
			Result.Add(Validator);
		}
	}
	return Result;
}

void FVoidValidatorRegistry::RunStage(EVoidValidationStage Stage, const FVoidValidationInput& Input, const FVoidValidationOptions& Options, FVoidValidationReport& Report) const
{
	for (const TSharedRef<IVoidValidator>& Validator : GetForStage(Stage))
	{
		FVoidValidationContext Context(Report, Options, Validator->GetSubsystem(), FString::Printf(TEXT("Validator:%s"), *Validator->GetValidatorId().ToString()));
		Validator->Validate(Stage, Input, Context);
		Context.FlushSuppressionSummary();
	}
}

// ------------------------------------------------------------------- Tags

FName FVoidGeneratedTags::MarkerTag()
{
	static const FName Marker(TEXT("VOID.Generated"));
	return Marker;
}

void FVoidGeneratedTags::Apply(AActor* Actor, FName GeneratorId, const FString& ObjectId)
{
	if (!Actor)
	{
		return;
	}
	Actor->Tags.AddUnique(MarkerTag());
	Actor->Tags.AddUnique(FName(*FString::Printf(TEXT("VOID.Generator.%s"), *GeneratorId.ToString())));
	if (!ObjectId.IsEmpty())
	{
		Actor->Tags.AddUnique(FName(*FString::Printf(TEXT("VOID.Id.%s"), *ObjectId)));
	}
}

bool FVoidGeneratedTags::TryRead(const AActor* Actor, FName& OutGeneratorId, FString& OutObjectId)
{
	OutGeneratorId = NAME_None;
	OutObjectId.Reset();
	if (!Actor || !Actor->Tags.Contains(MarkerTag()))
	{
		return false;
	}

	static const FString GeneratorPrefix(TEXT("VOID.Generator."));
	static const FString IdPrefix(TEXT("VOID.Id."));
	for (const FName& Tag : Actor->Tags)
	{
		const FString TagString = Tag.ToString();
		if (TagString.StartsWith(GeneratorPrefix))
		{
			OutGeneratorId = FName(*TagString.RightChop(GeneratorPrefix.Len()));
		}
		else if (TagString.StartsWith(IdPrefix))
		{
			OutObjectId = TagString.RightChop(IdPrefix.Len());
		}
	}
	return true;
}
