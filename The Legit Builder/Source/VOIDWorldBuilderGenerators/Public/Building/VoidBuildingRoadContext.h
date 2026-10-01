// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Data/VoidDesignPackage.h"
#include "Building/VoidBuildingTypes.h"
#include "Building/VoidBuildingParams.h"

/** Cross-section dimensions of a road, as the Road Generator will build it. */
struct FVoidRoadCorridorDims
{
	float Width = 600.0f;
	float CurbWidth = 0.0f;
	float SidewalkWidth = 0.0f;
};

/**
 * One straight piece of "keep-out" corridor: a centreline segment plus the
 * distance from that centreline to the outer edge of what the Road Generator
 * builds (half road width, plus kerb + sidewalk when the road has one).
 */
struct FVoidRoadCorridorSegment
{
	FVector2D A = FVector2D::ZeroVector;
	FVector2D B = FVector2D::ZeroVector;
	float CorridorRadius = 0.0f;
	FName RoadId = NAME_None;
	EVoidRoadType RoadType = EVoidRoadType::Local;
	float Elevation = 0.0f;
	/** The point at the centre of a roundabout: blocks the island but is never chosen as frontage. */
	bool bIsIsland = false;
};

/**
 * FVoidBuildingRoadContext
 *
 * The Building Generator's view of the road network. Built from the same
 * FVoidRoadSpec list (and the same profile table) the Road Generator consumes,
 * so it does not depend on the Road Generator having run, on actor names, or
 * on any road mesh output. Bridges and tunnels are not at grade and are left
 * out (a bridge deck passes over buildings; a tunnel is underground).
 */
class VOIDWORLDBUILDERGENERATORS_API FVoidBuildingRoadContext
{
public:
	/** Number of chords used to approximate a roundabout ring. */
	static constexpr int32 RoundaboutSegments = 32;

	/** Resolves road dimensions from the Road Generator's profile table (or built-in defaults). Implemented in the road-profile bridge translation unit. */
	static FVoidRoadCorridorDims ResolveDims(const FVoidRoadSpec& Road, const FVoidBuildingGenerationParams& Params);

	void Build(const FVoidDistrictData& District, const FVoidBuildingGenerationParams& Params);

	/**
	 * 1. Clears the footprint of every corridor + setback (slide / skip / ignore per ConflictPolicy).
	 * 2. Finds the frontage road, front edge, entrances, access point and base elevation.
	 * Sets bRoadConflictUnresolved when a building could not be made compliant.
	 */
	void ResolveBuilding(FVoidNormalizedBuilding& Building, const FVoidBuildingGenerationParams& Params) const;

	/** Required clear distance from a corridor edge for this building. */
	static float RequiredSetback(const FVoidNormalizedBuilding& Building, const FVoidBuildingGenerationParams& Params);

	int32 NumSegments() const { return Segments.Num(); }
	int32 NumSkippedGradeSeparated() const { return SkippedGradeSeparated; }
	const TArray<FVoidRoadCorridorSegment>& GetSegments() const { return Segments; }

private:
	void AddSegment(const FVector2D& A, const FVector2D& B, float Radius, const FVoidRoadSpec& Road, bool bIsland);
	void QueryNear(const FVector2D& Min, const FVector2D& Max, float Expand, TArray<int32>& OutIndices) const;
	void PlaceEntrances(FVoidNormalizedBuilding& Building, const FVoidBuildingGenerationParams& Params) const;

	static constexpr float GridCellSize = 4000.0f;

	TArray<FVoidRoadCorridorSegment> Segments;
	TMap<FIntPoint, TArray<int32>> Grid;
	float MaxCorridorRadius = 0.0f;
	int32 SkippedGradeSeparated = 0;
};
