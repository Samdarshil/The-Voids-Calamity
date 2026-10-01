// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Building/VoidBuildingTypes.h"

class UDataTable;

/**
 * FVoidBuildingGenerationParams
 *
 * Plain-struct snapshot of UVoidBuildingGenerationSettings taken at the start
 * of a run (same reasoning as FVoidImportContext: a long run must not change
 * behaviour because someone edits Project Settings mid-flight). Also lets
 * automation tests drive the generator without touching config.
 */
struct VOIDWORLDBUILDERGENERATORS_API FVoidBuildingGenerationParams
{
	/** Bump to change every building's variation on purpose. Same seed + same data => same city. */
	int32 GlobalSeed = 0;

	/** Buildings are merged into one mesh actor per square cell of this size (by centroid). */
	float BatchCellSizeUnits = 25000.0f;

	/** Footprints smaller than this (cm^2) are rejected as degenerate. */
	float MinFootprintAreaUnits2 = 40000.0f;

	/** Clear distance between a building and the edge of the road corridor (road + kerb + sidewalk). */
	float MinSetbackUnits = 200.0f;
	float LandmarkSetbackMultiplier = 2.0f;

	EVoidBuildingConflictPolicy ConflictPolicy = EVoidBuildingConflictPolicy::AdjustThenSkip;
	float MaxAdjustmentUnits = 800.0f;

	/** Roads further than this (measured from the corridor edge) are not considered frontage. */
	float MaxFrontageSearchUnits = 3000.0f;

	/** Sit the ground floor at the elevation of the frontage road. */
	bool bAlignBaseToFrontageRoad = true;

	bool bGenerateEntrances = true;
	bool bGenerateWindows = true;
	bool bGenerateRooftopStructures = true;
	bool bGenerateFloorBands = true;
	EVoidBuildingWindowDetail WindowDetail = EVoidBuildingWindowDetail::Auto;
	int32 MaxWindowQuadsPerBuilding = 500;

	float FoundationDepthUnits = 150.0f;

	/** Collision on wall / roof / foundation sections only. Windows, trim and technical bits never collide. */
	bool bGenerateCollision = true;

	/** Lower-case substring -> category, consulted before the built-in vocabulary. */
	TMap<FString, EVoidBuildingCategory> KeywordOverrides;

	/** Building id -> class path of a replacement actor (the asset-replacement hook). */
	TMap<FName, FString> AssetOverrideClassPaths;

	/** Optional road profile table, so corridor widths match what the Road Generator built. */
	const UDataTable* RoadProfileTable = nullptr;
};
