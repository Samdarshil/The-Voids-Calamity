// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IVoidGenerator.h"
#include "District/VoidDistrictLayoutBuilder.h"

/** Non-layout options for turning a plan into actors. */
struct VOIDWORLDBUILDERGENERATORS_API FVoidDistrictSpawnOptions
{
	bool bPreferRegisteredBuildingGenerator = true;
	bool bContinueOnValidationErrors = false;
	bool bUseRegionFolders = true;
};

struct VOIDWORLDBUILDERGENERATORS_API FVoidDistrictGenerationStats
{
	int32 NumDistricts = 0;
	int32 NumRoadActors = 0;
	int32 NumJunctionActors = 0;
	int32 NumBuildings = 0;
	int32 NumPublicSpaces = 0;
	int32 NumLandmarks = 0;
	int32 NumRemovedActors = 0;
	bool bUsedRegisteredBuildingGenerator = false;
};

/**
 * FVoidDistrictGenerator ("District")
 *
 * Phase 5 generator. Pipeline:
 *   Meridian data -> FVoidDistrictLayoutBuilder (plan) -> FVoidDistrictValidator
 *   -> [remove previous generation] -> Root + District actors
 *   -> Road generator (existing, unchanged; two calls: live surface network, dead network)
 *   -> Building placement (registered "Building" generator if present, else greybox instancing)
 *   -> Public spaces, landmarks, ports, tree markers.
 *
 * It is registered under IVoidGenerator like Road, so the editor panel and any
 * later orchestration call it through the same contract. Meridian data is read
 * from UVoidDistrictGenerationSettings (the FVoidDesignPackage argument is only
 * consulted for an explicit single-district override, see ApplyExplicitPackage).
 */
class VOIDWORLDBUILDERGENERATORS_API FVoidDistrictGenerator : public IVoidGenerator
{
public:
	virtual FName GetGeneratorId() const override;
	virtual bool Generate(const FVoidDesignPackage& Package, FVoidGenerationContext& Context) override;

	/** Programmatic entry (tests, commandlets, Agent 6/7 orchestration). Package may be null. */
	static bool GenerateFromInput(const FVoidDistrictLayoutInput& Input, const FVoidDistrictSpawnOptions& Options, const FVoidDesignPackage* ExplicitPackage, FVoidGenerationContext& Context, FVoidDistrictGenerationStats* OutStats = nullptr);

	/** Destroys every actor previously created by this generator (tag VoidMeridianGenerated). Returns the number destroyed. */
	static int32 RemoveExistingGeneration(UWorld* World);
};
