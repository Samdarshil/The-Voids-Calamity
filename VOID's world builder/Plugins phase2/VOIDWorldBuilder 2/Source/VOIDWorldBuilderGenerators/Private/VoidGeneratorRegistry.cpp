// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VoidGeneratorRegistry.h"
#include "VoidWorldBuilderLog.h"

FVoidGeneratorRegistry& FVoidGeneratorRegistry::Get()
{
	static FVoidGeneratorRegistry Instance;
	return Instance;
}

void FVoidGeneratorRegistry::RegisterGenerator(TSharedRef<IVoidGenerator> Generator)
{
	const FName Id = Generator->GetGeneratorId();

	if (Generators.Contains(Id))
	{
		UE_LOG(LogVoidWorldBuilder, Warning, TEXT("Generator '%s' is already registered; overwriting previous registration."), *Id.ToString());
	}

	Generators.Add(Id, Generator);
	UE_LOG(LogVoidWorldBuilder, Log, TEXT("Registered generator '%s'."), *Id.ToString());
}

void FVoidGeneratorRegistry::UnregisterGenerator(FName GeneratorId)
{
	Generators.Remove(GeneratorId);
}

TSharedPtr<IVoidGenerator> FVoidGeneratorRegistry::FindGenerator(FName GeneratorId) const
{
	if (const TSharedRef<IVoidGenerator>* Found = Generators.Find(GeneratorId))
	{
		return *Found;
	}
	return nullptr;
}

TArray<TSharedRef<IVoidGenerator>> FVoidGeneratorRegistry::GetAllGenerators() const
{
	TArray<TSharedRef<IVoidGenerator>> Result;
	Generators.GenerateValueArray(Result);
	return Result;
}
