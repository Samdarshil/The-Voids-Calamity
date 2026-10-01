// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IVoidGenerator.h"

/**
 * FVoidGeneratorRegistry
 *
 * Central lookup for IVoidGenerator implementations, keyed by generator
 * id. Introduced in Phase 1 (empty, no registrants) specifically so the
 * Phase 10 orchestrator can be written against "ask the registry for all
 * generators" instead of a hardcoded list -- per the architecture's
 * Future Expansion Strategy, adding a new generator module should never
 * require editing this class or the orchestrator.
 *
 * Registration is manual (RegisterGenerator) rather than static-init
 * auto-registration in Phase 1, to keep module load order simple to
 * reason about while there are zero registrants. Auto-registration can
 * be introduced when Phase 3 adds the first real generator, without
 * changing this class's public contract.
 */
class VOIDWORLDBUILDERGENERATORS_API FVoidGeneratorRegistry
{
public:
	static FVoidGeneratorRegistry& Get();

	void RegisterGenerator(TSharedRef<IVoidGenerator> Generator);
	void UnregisterGenerator(FName GeneratorId);

	TSharedPtr<IVoidGenerator> FindGenerator(FName GeneratorId) const;
	TArray<TSharedRef<IVoidGenerator>> GetAllGenerators() const;

private:
	TMap<FName, TSharedRef<IVoidGenerator>> Generators;
};
