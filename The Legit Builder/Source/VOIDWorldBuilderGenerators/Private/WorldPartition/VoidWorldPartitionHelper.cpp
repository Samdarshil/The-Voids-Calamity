// Copyright VOID Engineering Studio. Internal tool - not for shipping content.
// Added by Agent 2 (selected Phase 9: World Partition support).

#include "WorldPartition/VoidWorldPartitionHelper.h"
#include "Metro/VoidMetroLayoutResolver.h" // Densify only
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"

bool FVoidWorldPartitionHelper::IsWorldPartitioned(const UWorld* World)
{
	// UWorld::GetWorldPartition() is non-null for a partitioned world; comparing the pointer needs no WorldPartition header.
	return World && World->GetWorldPartition() != nullptr;
}

FIntPoint FVoidWorldPartitionHelper::GetCellForLocation(const FVector& Location, float CellSize)
{
	const float Safe = FMath::Max(CellSize, 1.0f);
	return FIntPoint(FMath::FloorToInt(Location.X / Safe), FMath::FloorToInt(Location.Y / Safe));
}

TArray<FVoidPathChunk> FVoidWorldPartitionHelper::SplitPathByCells(const TArray<FVector>& InPoints, bool bClosed, float CellSize)
{
	TArray<FVoidPathChunk> Chunks;
	if (InPoints.Num() < 2)
	{
		return Chunks;
	}

	const float Safe = FMath::Max(CellSize, 1.0f);
	const TArray<FVector> Points = FVoidMetroLayoutResolver::Densify(InPoints, Safe * 0.25f, bClosed);
	const int32 N = Points.Num();
	const int32 Spans = bClosed ? N : N - 1;

	TMap<FIntPoint, int32> CellCounters; // how many chunks each cell already has (same cell can be re-entered)
	FVoidPathChunk* Current = nullptr;

	for (int32 i = 0; i < Spans; ++i)
	{
		const FVector& A = Points[i];
		const FVector& B = Points[(i + 1) % N];
		const FIntPoint Cell = GetCellForLocation((A + B) * 0.5f, Safe);

		if (!Current || Current->Cell != Cell)
		{
			FVoidPathChunk NewChunk;
			NewChunk.Cell = Cell;
			int32& Count = CellCounters.FindOrAdd(Cell);
			NewChunk.IndexInCell = Count++;
			NewChunk.Points.Add(A);
			Chunks.Add(MoveTemp(NewChunk));
			Current = &Chunks.Last(); // pointer refreshed after every Add, so realloc cannot leave it dangling
		}
		Current->Points.Add(B);
	}
	return Chunks;
}

FString FVoidWorldPartitionHelper::MakeFolderPath(const FString& Root, const FString& Network, const FString& Category)
{
	FString Path = Root;
	Path.RemoveFromEnd(TEXT("/"));
	if (!Network.IsEmpty()) { Path += TEXT("/") + Network; }
	if (!Category.IsEmpty()) { Path += TEXT("/") + Category; }
	return Path;
}

void FVoidWorldPartitionHelper::ApplyPlacement(AActor* Actor, const FVoidWorldPartitionPlacement& Placement, bool bWorldPartitioned)
{
#if WITH_EDITOR
	if (!Actor)
	{
		return;
	}

	if (!Placement.FolderPath.IsEmpty())
	{
		Actor->SetFolderPath(FName(*Placement.FolderPath));
	}

	if (!bWorldPartitioned)
	{
		return;
	}

	// World Partition properties. Editor-only setters on AActor.
	if (Actor->CanChangeIsSpatiallyLoadedFlag())
	{
		Actor->SetIsSpatiallyLoaded(Placement.bSpatiallyLoaded);
	}
	if (Placement.RuntimeGrid != NAME_None)
	{
		Actor->SetRuntimeGrid(Placement.RuntimeGrid);
	}

	Actor->bEnableAutoLODGeneration = Placement.bIncludeInHLOD;

	// HLOD layer by reflection: avoids a compile-time dependency on the HLOD headers, which move between engine versions.
	if (Placement.HLODLayer.IsValid())
	{
		if (UObject* Layer = Placement.HLODLayer.TryLoad())
		{
			if (FObjectProperty* Prop = CastField<FObjectProperty>(AActor::StaticClass()->FindPropertyByName(TEXT("HLODLayer"))))
			{
				if (Layer->IsA(Prop->PropertyClass))
				{
					Prop->SetObjectPropertyValue_InContainer(Actor, Layer);
				}
			}
		}
	}
#endif
}
