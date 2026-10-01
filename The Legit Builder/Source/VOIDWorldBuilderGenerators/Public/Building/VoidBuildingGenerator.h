// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IVoidGenerator.h"
#include "Building/VoidBuildingTypes.h"
#include "Building/VoidBuildingParams.h"
#include "Building/VoidBuildingMeshBuilder.h"

class UWorld;

/** Everything one batch actor needs, computed without touching the world. */
struct FVoidBuildingBatchData
{
	FIntPoint Cell = FIntPoint::ZeroValue;
	FVector Origin = FVector::ZeroVector;
	FString Signature;
	TArray<FVoidBuildingMetadata> Metadata;
	/** Buildings in this cell that get a replacement actor instead of a greybox: id -> transform. */
	TArray<TPair<FName, FTransform>> Overrides;
	FVoidBuildingMeshAccumulator Mesh;
};

struct FVoidBuildingRunStats
{
	int32 NumInput = 0;
	int32 NumRejected = 0;      // failed normalisation
	int32 NumSkippedRoad = 0;   // could not clear road corridor
	int32 NumGenerated = 0;
	int32 NumAdjusted = 0;
	int32 NumNoFrontage = 0;
	int32 NumLandmarks = 0;
	int32 NumOverrides = 0;
	int32 NumTriangles = 0;
	int32 NumBatches = 0;
	int32 NumRoadSegments = 0;
	int32 CategoryCounts[11] = {};
};

/**
 * FVoidBuildingGenerator
 *
 * Registered under the id "Building". Consumes the imported FVoidDesignPackage
 * (FVoidBuildingSpec + FVoidRoadSpec) and produces AVoidBuildingBatchActor
 * instances. BuildBatches() is world-free and deterministic; Generate() adds
 * cleanup + spawning around it.
 */
class VOIDWORLDBUILDERGENERATORS_API FVoidBuildingGenerator : public IVoidGenerator
{
public:
	//~ Begin IVoidGenerator
	virtual FName GetGeneratorId() const override;
	virtual bool Generate(const FVoidDesignPackage& Package, FVoidGenerationContext& Context) override;
	//~ End IVoidGenerator

	/** Pure step: normalise, relate to roads, plan, mesh, batch. No world access. */
	static void BuildBatches(
		const FVoidDistrictData& District,
		const FVoidBuildingGenerationParams& Params,
		FVoidValidationReport& InOutReport,
		TArray<FVoidBuildingBatchData>& OutBatches,
		FVoidBuildingRunStats& OutStats,
		TFunction<bool()> IsCancelled = nullptr);

	/** Destroys every batch/replacement actor a previous run created for this district. Returns the number destroyed. */
	static int32 RemoveGenerated(UWorld* World, FName DistrictId);
};
