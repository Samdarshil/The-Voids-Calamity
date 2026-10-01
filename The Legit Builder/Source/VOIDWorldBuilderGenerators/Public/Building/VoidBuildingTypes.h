// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Types/VoidWorldBuilderTypes.h"
#include "VoidBuildingTypes.generated.h"

/**
 * Behavioural category of a building. Derived from FVoidBuildingSpec::BuildingType
 * (a free-form design tag) by FVoidBuildingNormalizer::Classify -- the package
 * schema itself has no category field, and this generator does not add one.
 * Unknown is a first-class value: anything the classifier does not recognise
 * generates as a plain extruded block instead of guessing an architecture.
 */
UENUM(BlueprintType)
enum class EVoidBuildingCategory : uint8
{
	Unknown,
	Residential,
	Commercial,
	Office,
	Industrial,
	Civic,
	Institutional,
	Medical,
	Government,
	MixedUse,
	Landmark
};

/** Massing recipe picked deterministically per building. */
UENUM(BlueprintType)
enum class EVoidBuildingArchetype : uint8
{
	SimpleExtrude,
	PodiumTower,
	SetbackTiers,
	Courtyard,
	PlinthBody,
	LandmarkTower
};

/** What to do when a footprint cannot be cleared of the road corridor + setback. */
UENUM(BlueprintType)
enum class EVoidBuildingConflictPolicy : uint8
{
	/** Slide the building away from the road (bounded by MaxAdjustmentUnits); skip it if that fails. Default. */
	AdjustThenSkip,
	/** Never move authored footprints; skip any building that violates the corridor. */
	SkipOnly,
	/** Generate regardless and report the conflict (debugging aid). */
	GenerateAnyway
};

/** How much window geometry to emit. */
UENUM(BlueprintType)
enum class EVoidBuildingWindowDetail : uint8
{
	None,
	/** One dark strip per wall per floor. Cheapest. */
	Ribbon,
	/** Individual bays where the per-building quad budget allows, ribbons otherwise. Default. */
	Auto,
	/** Individual bays everywhere (can be very heavy on large districts). */
	Bays
};

/** Mesh surface classes. One ProceduralMesh section each, so one draw call per class per batch. */
enum class EVoidBuildingSurface : uint8
{
	Wall = 0,
	Roof,
	Glass,
	Trim,
	Foundation,
	Technical,
	Count
};

enum class EVoidWindowStyle : uint8
{
	None,
	Punched,
	Ribbon,
	Curtain
};

enum class EVoidRoofKind : uint8
{
	Flat,
	Parapet,
	Pyramid
};

/** A door on the frontage wall, in world XY. */
struct FVoidBuildingEntrance
{
	int32 EdgeIndex = INDEX_NONE;
	FVector2D Location = FVector2D::ZeroVector;      // Centre of the door on the wall line.
	FVector2D OutwardNormal = FVector2D::ZeroVector; // Unit, points away from the building.
	float Width = 200.0f;
	float Height = 250.0f;
	bool bCanopy = false;
};

/** Result of relating a building to the road network. */
struct FVoidBuildingFrontage
{
	bool bHasRoadFrontage = false;
	FName RoadId = NAME_None;
	int32 FrontEdgeIndex = INDEX_NONE;
	FVector2D FrontNormal = FVector2D::ZeroVector;   // Outward normal of the front edge.
	FVector2D AccessPoint = FVector2D::ZeroVector;   // Point on the road corridor edge (kerb/sidewalk outer edge) nearest the main entrance.
	float ClearanceToCorridor = 0.0f;                // Footprint-to-corridor-edge distance at the front.
	float BaseElevation = 0.0f;                      // Elevation of the frontage road (at-grade roads only), else 0.
};

/**
 * Internal, normalised representation the generator works on. Built once from
 * FVoidBuildingSpec by FVoidBuildingNormalizer. Plain struct, not reflected:
 * nothing outside this module should hold on to it (see AGENT4_BUILDING_SCHEMA.md).
 */
struct FVoidNormalizedBuilding
{
	FName Id = NAME_None;
	FName DistrictId = NAME_None;
	FString TypeTag;
	EVoidBuildingCategory Category = EVoidBuildingCategory::Unknown;
	bool bIsLandmark = false;

	/** Footprint as authored, cleaned (deduped, CCW). */
	TArray<FVector2D> SourceFootprint;
	/** Footprint actually generated (== SourceFootprint unless slid off a road corridor). */
	TArray<FVector2D> Footprint;

	FVector2D Centroid = FVector2D::ZeroVector;
	FVector2D BoundsMin = FVector2D::ZeroVector;
	FVector2D BoundsMax = FVector2D::ZeroVector;
	float Area = 0.0f;

	float HeightUnits = 0.0f; // Authored, never altered.
	int32 FloorCount = 1;
	float FloorHeight = 300.0f; // HeightUnits / FloorCount.
	float BaseZ = 0.0f;

	int32 Seed = 0; // Stable from (Id, GlobalSeed).

	FVoidBuildingFrontage Frontage;
	TArray<FVoidBuildingEntrance> Entrances;

	FVector2D AdjustmentOffset = FVector2D::ZeroVector;
	bool bAdjustedForRoad = false;
	bool bRoadConflictUnresolved = false;
};

/**
 * Per-building record stored on the batch actor. This is the hook for the
 * later Asset Replacement system: it says what a greybox building was, where,
 * and how big, without that system having to re-derive anything.
 */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERGENERATORS_API FVoidBuildingMetadata
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Building")
	FName BuildingId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Building")
	FName DistrictId = NAME_None;

	/** The authored buildingType string, untouched. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Building")
	FString TypeTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Building")
	EVoidBuildingCategory Category = EVoidBuildingCategory::Unknown;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Building")
	EVoidBuildingArchetype Archetype = EVoidBuildingArchetype::SimpleExtrude;

	/** Dotted category for asset lookup, e.g. "Building.Office.HighRise" or "Building.Landmark". */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Building")
	FName AssetCategory = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Building")
	bool bIsLandmark = false;

	/** Footprint as generated (world XY, CCW). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Building")
	TArray<FVector2D> FootprintCorners;

	/** Footprint as authored, if different from the generated one (bAdjustedForRoad). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Building")
	TArray<FVector2D> SourceFootprintCorners;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Building")
	FVector2D Centroid = FVector2D::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Building")
	FVector2D BoundsMin = FVector2D::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Building")
	FVector2D BoundsMax = FVector2D::ZeroVector;

	/** Yaw (degrees) of the footprint's longest edge; a sensible orientation for a replacement asset. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Building")
	float YawDegrees = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Building")
	float HeightUnits = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Building")
	int32 FloorCount = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Building")
	float BaseZ = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Building")
	bool bHasRoadFrontage = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Building")
	FName FrontageRoadId = NAME_None;

	/** Main entrance (world XY) and the matching point on the road corridor edge. Zero if no entrance was generated. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Building")
	FVector2D MainEntranceLocation = FVector2D::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Building")
	FVector2D AccessPoint = FVector2D::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Building")
	int32 NumEntrances = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Building")
	bool bAdjustedForRoad = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Building")
	FVector2D AdjustmentOffset = FVector2D::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Building")
	int32 Seed = 0;

	/** True when a replacement actor from the settings' AssetOverrides was spawned and no greybox mesh exists for this building. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Building")
	bool bGreyboxSuppressed = false;
};
