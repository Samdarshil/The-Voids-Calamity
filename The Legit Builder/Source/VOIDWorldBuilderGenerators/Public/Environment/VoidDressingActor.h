// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Environment/VoidEnvironmentTypes.h"
#include "VoidDressingActor.generated.h"

class UInstancedStaticMeshComponent;

/**
 * AVoidDressingActor
 *
 * Output container for the Environment and Prop generators: ONE actor per
 * (generator, district, streaming cell), holding instanced-mesh components
 * (one per Category/Slot/Part bucket) -- never one actor per prop.
 *
 * Ownership / regeneration: GeneratorId + DistrictId identify who owns the
 * actor. A regeneration destroys every actor with the same pair before
 * spawning, so re-running never duplicates content. No Tick.
 *
 * Replacement hooks: Buckets[] records Category, SlotId, ReplacementTag and
 * whether a bucket is still a placeholder; components carry matching tags
 * (VOID.Category.<x>, VOID.Slot.<x>, VOID.Placeholder). The full replacement
 * workflow is out of scope -- see FindBucketComponent / SetBucketMesh.
 */
UCLASS(NotBlueprintable)
class VOIDWORLDBUILDERGENERATORS_API AVoidDressingActor : public AActor
{
	GENERATED_BODY()

public:
	AVoidDressingActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Dressing")
	TObjectPtr<USceneComponent> RootScene;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Dressing")
	FName GeneratorId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Dressing")
	FName DistrictId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Dressing")
	FIntPoint Cell = FIntPoint::ZeroValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Dressing")
	int32 Seed = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Dressing")
	TArray<FVoidDressingBucketInfo> Buckets;

	/** Hook: find the instanced component for a bucket (PartIndex 0 for real assets). Null if none. */
	UFUNCTION(BlueprintCallable, Category = "VOID Dressing")
	UInstancedStaticMeshComponent* FindBucketComponent(FName Category, FName SlotId, int32 PartIndex = 0) const;

	/** Hook: swap the mesh of one bucket in place (e.g. placeholder -> AAA). Instances keep their transforms. Returns false if the bucket does not exist. */
	UFUNCTION(BlueprintCallable, Category = "VOID Dressing")
	bool SetBucketMesh(FName Category, FName SlotId, UStaticMesh* NewMesh, int32 PartIndex = 0);

	int32 GetTotalInstanceCount() const;
};
