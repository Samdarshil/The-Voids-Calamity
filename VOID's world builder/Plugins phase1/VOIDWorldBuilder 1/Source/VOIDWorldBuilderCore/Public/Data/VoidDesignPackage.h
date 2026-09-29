// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Types/VoidWorldBuilderTypes.h"
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
 * Deliberately minimal in Phase 1 -- footprint + height + a type tag is
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
	 * Not an enum in Phase 1 because the design vocabulary is still
	 * growing; validated as non-empty only.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	FString BuildingType;
};

/**
 * A single road segment as specified by design. Spline points define the
 * centerline; width is uniform per segment in Phase 1. Intersections are
 * inferred later by the Road Generator (Phase 3), not stored here, since
 * intersection resolution is generation logic, not design data.
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
};

/**
 * The full design package for a single district: everything the Import
 * pipeline reads from JSON and everything later generators (Phase 3+)
 * will consume. Intentionally data-only -- no methods beyond trivial
 * accessors -- so Core stays a plain-data module with no generation or
 * editor logic in it.
 */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERCORE_API FVoidDesignPackage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	FVoidElementId DistrictId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	FVoidPackageMetadata Metadata;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	TArray<FVoidBuildingSpec> Buildings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
	TArray<FVoidRoadSpec> Roads;
};
