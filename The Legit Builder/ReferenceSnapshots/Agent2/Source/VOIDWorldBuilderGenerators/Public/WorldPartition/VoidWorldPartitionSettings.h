// Copyright VOID Engineering Studio. Internal tool - not for shipping content.
// Added by Agent 2 (selected Phase 9: World Partition support).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VoidWorldPartitionSettings.generated.h"

/**
 * UVoidWorldPartitionSettings
 *
 * Practical World Partition support for generated actors -- placement
 * metadata only. This does NOT create, convert, or configure a World
 * Partition world, does NOT define runtime grids (those live in the map's
 * World Settings), and does NOT implement any streaming logic. It stamps
 * runtime-grid / spatially-loaded / HLOD-relevance / outliner-folder values
 * on generated actors, and sizes generated actor chunks to the project's
 * grid so a long track does not become one huge always-overlapping actor.
 *
 * Any generator (Metro today) can use FVoidWorldPartitionHelper with these
 * settings; nothing here is metro-specific.
 */
UCLASS(Config = VOIDWorldBuilder, DefaultConfig, meta = (DisplayName = "VOID World Builder World Partition"))
class VOIDWORLDBUILDERGENERATORS_API UVoidWorldPartitionSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UVoidWorldPartitionSettings();

	/** Master switch. When false, no World Partition properties are touched (outliner folders are still set). */
	UPROPERTY(Config, EditAnywhere, Category = "Placement")
	bool bApplyWorldPartitionPlacement = true;

	/**
	 * Runtime grid name for Live-network metro actors. NAME_None = let World Partition choose (the map's default grid).
	 * A named grid must exist in the map's World Settings runtime hash; otherwise actors fall back to the default grid.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Placement")
	FName LiveNetworkRuntimeGrid;

	/** Runtime grid name for Dead-network metro actors. See LiveNetworkRuntimeGrid. */
	UPROPERTY(Config, EditAnywhere, Category = "Placement")
	FName DeadNetworkRuntimeGrid;

	/** Generated actors are made spatially loaded (streamed by distance). Turn off to keep them always loaded. */
	UPROPERTY(Config, EditAnywhere, Category = "Placement")
	bool bSpatiallyLoaded = true;

	/**
	 * Cell size used ONLY to split long generated paths into per-cell actor chunks. Match your map's runtime grid cell size
	 * (World Settings). Units: cm. Does not configure the grid.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Chunking", meta = (ClampMin = "1000.0"))
	float ChunkCellSizeUnits = 25600.0f;

	/**
	 * Dead-network track is chunked with this multiple of ChunkCellSizeUnits. StreamingStrategy.json asks for the
	 * dead network to stream as 'one continuous underground volume ... related region cluster', so it gets coarser chunks.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Chunking", meta = (ClampMin = "1.0", ClampMax = "16.0"))
	float DeadNetworkChunkCellMultiplier = 4.0f;

	/** Sets AActor::bEnableAutoLODGeneration ('include in HLOD'). NOTE: procedural-mesh components do not contribute to HLOD proxies; this is a hook for when the geometry is converted to static meshes. */
	UPROPERTY(Config, EditAnywhere, Category = "HLOD")
	bool bIncludeInHLOD = false;

	/** Optional HLOD layer asset assigned to generated actors (set via reflection on AActor::HLODLayer so the plugin has no compile-time dependency on the HLOD API). */
	UPROPERTY(Config, EditAnywhere, Category = "HLOD", meta = (AllowedClasses = "/Script/Engine.HLODLayer"))
	FSoftObjectPath HLODLayer;

	/** Root outliner folder for generated actors. Sub-folders are Network/Category (e.g. VOID/Meridian/Metro/Live/Track). */
	UPROPERTY(Config, EditAnywhere, Category = "Organization")
	FString OutlinerFolderRoot = TEXT("VOID/Meridian/Metro");
};
