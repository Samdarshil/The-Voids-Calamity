// Copyright VOID Engineering Studio. Internal tool - not for shipping content.
// Added by Agent 2 (selected Phase 9: World Partition support).

#pragma once

#include "CoreMinimal.h"

class AActor;
class UWorld;

/** Placement metadata stamped onto a generated actor. Plain data; not a streaming system. */
struct VOIDWORLDBUILDERGENERATORS_API FVoidWorldPartitionPlacement
{
	FName RuntimeGrid = NAME_None;
	bool bSpatiallyLoaded = true;
	bool bIncludeInHLOD = false;
	FSoftObjectPath HLODLayer;
	FString FolderPath;
};

/** One contiguous piece of a longer path, assigned to a grid cell. */
struct VOIDWORLDBUILDERGENERATORS_API FVoidPathChunk
{
	FIntPoint Cell = FIntPoint::ZeroValue;
	int32 IndexInCell = 0;
	TArray<FVector> Points;
};

/**
 * FVoidWorldPartitionHelper
 *
 * Reusable, generator-agnostic World Partition support:
 *   - detect whether a world is partitioned,
 *   - split long generated paths into per-cell chunks (spatially appropriate generation),
 *   - stamp runtime grid / spatially-loaded / HLOD / outliner folder on actors.
 * It never creates or converts a World Partition world and never changes
 * the map's runtime hash / grid definitions.
 */
class VOIDWORLDBUILDERGENERATORS_API FVoidWorldPartitionHelper
{
public:
	static bool IsWorldPartitioned(const UWorld* World);

	/** Grid cell index containing Location for a given cell size (cm). Uses floor, so negative coordinates map correctly. */
	static FIntPoint GetCellForLocation(const FVector& Location, float CellSize);

	/**
	 * Splits a path so every chunk lies within one cell, decided by span midpoint. Consecutive chunks share their
	 * boundary vertex so there are no gaps. Points are densified to at most CellSize/4 spans first so a span cannot
	 * jump across a whole cell. Chunk order follows path order (deterministic).
	 */
	static TArray<FVoidPathChunk> SplitPathByCells(const TArray<FVector>& Points, bool bClosed, float CellSize);

	/** e.g. MakeFolderPath("VOID/Meridian/Metro", "Live", "Track") -> "VOID/Meridian/Metro/Live/Track". */
	static FString MakeFolderPath(const FString& Root, const FString& Network, const FString& Category);

	/**
	 * Applies placement to an actor. Editor-only APIs are behind WITH_EDITOR; a no-op at runtime.
	 * bWorldPartitioned gates the World Partition-specific properties; the outliner folder is always set.
	 */
	static void ApplyPlacement(AActor* Actor, const FVoidWorldPartitionPlacement& Placement, bool bWorldPartitioned);
};
