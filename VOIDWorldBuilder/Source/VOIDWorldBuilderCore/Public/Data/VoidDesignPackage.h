// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Types/VoidWorldBuilderTypes.h"
#include "Data/VoidSchemaVersion.h"
#include "VoidDesignPackage.generated.h"

/**
 * Provenance metadata carried with every design package. Exists so
 * generated content can always be traced back to the approved document
 * and version it came from -- required given the studio's rule that this
 * tool implements approved design, never invents it.
 */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERCORE_API FVoidPackageMetadata
{
	GENERATED_BODY()

	/** Name of the source design document, e.g. "Meridian District 04 - White Zone". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	FString SourceDocumentName;

	/** Version/revision string of the approved document this package was exported from. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	FString SourceDocumentVersion;

	/** Whether this package has been marked approved by design leads. Unapproved packages must not be generated into a shared level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	bool bIsApproved = false;
};

/**
 * A single building placement within a district, as specified by design.
 * Deliberately minimal in Phase 1/2 -- footprint + height + a type tag is
 * enough to validate and store. The Building Generator (Phase 4) is what
 * turns this into an actual greybox mesh; this struct does not know how
 * to generate anything.
 */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERCORE_API FVoidBuildingSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	FVoidElementId Id;

	/** Footprint corners in district-local space, in Unreal units, wound consistently (CCW). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	TArray<FVector2D> FootprintCorners;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	float HeightUnits = 0.0f;

	/**
	 * Free-form design tag, e.g. "Residential_MidTier", "CivicCenter".
	 * Not an enum in Phase 1/2 because the design vocabulary is still
	 * growing; validated as non-empty only.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	FString BuildingType;
};

/**
 * A single road segment as specified by design. Spline points define the
 * centerline; width is uniform per segment. Everything below WidthUnits
 * was added in Phase 3 to support road generation -- all additive, all
 * with safe defaults, so every Phase 2 package remains valid unchanged.
 * See Docs/RoadGeneratorArchitecture.md for the full rationale.
 */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERCORE_API FVoidRoadSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	FVoidElementId Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	TArray<FVector2D> CenterlinePoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	float WidthUnits = 0.0f;

	/** Functional classification. Drives the Road Generator's per-type profile (default width/lanes/sidewalk) and, for Roundabout, an entirely different geometry path. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	EVoidRoadType RoadType = EVoidRoadType::Local;

	/** 0 = not specified; the Road Generator falls back to RoadType's default lane count. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	int32 LaneCount = 0;

	/** 0 = not specified; the Road Generator falls back to RoadType's default speed limit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	int32 SpeedLimitUnits = 0;

	/** Base elevation offset for the whole road (e.g. a hill), independent of bridge/tunnel ramps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	float ElevationUnits = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	bool bHasSidewalk = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	bool bHasMedian = false;

	/** Mutually exclusive with bIsTunnel -- FVoidRoadValidator rejects a road with both set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	bool bIsBridge = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	bool bIsTunnel = false;

	/** Only meaningful if this road's end point has no other road connecting to it. If true, generates a turnaround bulb instead of a flat dead-end cap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	bool bCulDeSacAtEnd = false;

	/** Only meaningful when RoadType == Roundabout. CenterlinePoints[0] is treated as the roundabout's center point; this is its radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	float RoundaboutRadiusUnits = 0.0f;

	/**
	 * Optional explicit adjacency (e.g. this road connects to a specific
	 * roundabout's id). When empty, the Road Generator falls back to
	 * detecting connections by coincident endpoint geometry -- explicit
	 * ConnectionIds exist as an override for cases where two centerlines
	 * are meant to connect but don't line up to the tolerance the
	 * coincident-point heuristic uses.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	TArray<FVoidElementId> ConnectionIds;

	/**
	 * Agent 1 addition (additive, default false = every existing package is unchanged). True for a
	 * closed circumferential road such as a ring road: CenterlinePoints is a loop whose last point does
	 * NOT repeat the first. Distinct from RoadType == Roundabout, which is generated from a center + radius.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	bool bClosedLoop = false;
};

/**
 * FVoidDistrictData
 *
 * The actual district payload: its id and everything placed within it.
 * Split out from FVoidDesignPackage as its own named type so that a
 * future multi-district package format (e.g. a batch export covering
 * several districts at once) can hold `TArray<FVoidDistrictData>`
 * without changing this struct or anything that reads it -- only the
 * envelope around it would need to change.
 */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERCORE_API FVoidDistrictData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	FVoidElementId DistrictId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	TArray<FVoidBuildingSpec> Buildings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	TArray<FVoidRoadSpec> Roads;
};

/**
 * FVoidDesignPackage
 *
 * The full imported unit: a schema version for the envelope format
 * itself, provenance metadata, and the district payload. Intentionally
 * data-only -- no methods beyond trivial accessors -- so Core stays a
 * plain-data module with no generation or editor logic in it.
 */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERCORE_API FVoidDesignPackage
{
	GENERATED_BODY()

	/** Version of the JSON schema this package itself is written in. See FVoidSchemaVersion for the compatibility policy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	FVoidSchemaVersion SchemaVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	FVoidPackageMetadata Metadata;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	FVoidDistrictData District;
};
