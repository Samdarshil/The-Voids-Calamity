// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/VoidDesignPackage.h"
#include "Building/VoidBuildingTypes.h"
#include "VoidBuildingActor.generated.h"

class UProceduralMeshComponent;

/**
 * AVoidBuildingBatchActor
 *
 * One actor per (district, grid cell). Holds a single ProceduralMesh with one
 * section per surface class (see EVoidBuildingSurface) for every building whose
 * centroid falls in the cell, plus a FVoidBuildingMetadata record per building.
 * No logic, no Tick. The actor sits at the cell centre and mesh vertices are
 * relative to it, so actor bounds stay small for streaming/culling.
 *
 * Section indices: 0 Wall, 1 Roof, 2 Glass, 3 Trim, 4 Foundation, 5 Technical.
 */
UCLASS()
class VOIDWORLDBUILDERGENERATORS_API AVoidBuildingBatchActor : public AActor
{
	GENERATED_BODY()

public:
	AVoidBuildingBatchActor();

	/** Tag on everything a Building Generator run creates (batch actors and replacement actors). */
	static const FName GeneratedTag;
	/** "VoidDistrict:<DistrictId>" -- used to find and clean up a district's previous run. */
	static FName MakeDistrictTag(FName DistrictId);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Building")
	TObjectPtr<USceneComponent> RootScene;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Building")
	TObjectPtr<UProceduralMeshComponent> BuildingMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Building")
	FName DistrictId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Building")
	FIntPoint BatchCell = FIntPoint::ZeroValue;

	/** Hash of the normalized input + settings that produced this batch. Equal signature => identical geometry. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Building")
	FString GenerationSignature;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Building")
	TArray<FVoidBuildingMetadata> Buildings;

	const FVoidBuildingMetadata* FindBuilding(FName BuildingId) const;
};
