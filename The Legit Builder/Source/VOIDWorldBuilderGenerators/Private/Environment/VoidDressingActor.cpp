// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Environment/VoidDressingActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"

AVoidDressingActor::AVoidDressingActor()
{
	PrimaryActorTick.bCanEverTick = false; // Static dressing. Never ticks.
	PrimaryActorTick.bStartWithTickEnabled = false;

	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	RootScene->SetMobility(EComponentMobility::Static);
	SetRootComponent(RootScene);
}

UInstancedStaticMeshComponent* AVoidDressingActor::FindBucketComponent(FName Category, FName SlotId, int32 PartIndex) const
{
	for (const FVoidDressingBucketInfo& Bucket : Buckets)
	{
		if (Bucket.Category == Category && Bucket.SlotId == SlotId && Bucket.PartIndex == PartIndex)
		{
			return Bucket.Component;
		}
	}
	return nullptr;
}

bool AVoidDressingActor::SetBucketMesh(FName Category, FName SlotId, UStaticMesh* NewMesh, int32 PartIndex)
{
	UInstancedStaticMeshComponent* Comp = FindBucketComponent(Category, SlotId, PartIndex);
	if (!Comp || !NewMesh)
	{
		return false;
	}
	Comp->SetStaticMesh(NewMesh);
	Comp->ComponentTags.Remove(FName(TEXT("VOID.Placeholder")));
	return true;
}

int32 AVoidDressingActor::GetTotalInstanceCount() const
{
	int32 Total = 0;
	for (const FVoidDressingBucketInfo& Bucket : Buckets)
	{
		// PartIndex > 0 buckets duplicate the instance count of part 0 of the same slot; count each instance once.
		if (Bucket.PartIndex == 0 && Bucket.Component)
		{
			Total += Bucket.Component->GetInstanceCount();
		}
	}
	return Total;
}
