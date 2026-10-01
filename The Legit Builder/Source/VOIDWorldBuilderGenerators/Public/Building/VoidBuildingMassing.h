// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Building/VoidBuildingTypes.h"
#include "Building/VoidBuildingParams.h"

/** One extruded volume of a building (podium, tower, tier, crown, ...). */
struct FVoidMassBlock
{
	/** CCW outline in world XY. */
	TArray<FVector2D> Outline;
	/** If non-empty, this block is a ring: Outline is the outer wall, this is the inner (courtyard) wall. */
	TArray<FVector2D> CourtyardOutline;

	int32 FirstFloor = 0;
	int32 NumFloors = 1;
	double ZBottom = 0.0;
	double ZTop = 0.0;

	EVoidWindowStyle WindowStyle = EVoidWindowStyle::None;
	EVoidRoofKind Roof = EVoidRoofKind::Flat;
	bool bStringCourses = false;

	/** The next block is an inset of this outline (same vertex count): cap only the ring between them. */
	bool bCapToNext = false;
};

struct FVoidRooftopBox
{
	FVector2D Center = FVector2D::ZeroVector;
	FVector2D HalfExtent = FVector2D::ZeroVector;
	double YawRadians = 0.0;
	double ZBottom = 0.0;
	double Height = 100.0;
};

struct FVoidWindowParams
{
	float BayUnits = 350.0f;
	float WidthFrac = 0.55f;   // Fraction of a bay a punched window occupies.
	float SillFrac = 0.28f;    // Sill height as a fraction of floor height.
	float HeightFrac = 0.5f;
	float GroundSillFrac = -1.0f;   // < 0: ground floor uses the normal window.
	float GroundHeightFrac = 0.7f;
	float Depth = 3.0f;        // Outward offset of window/trim quads: the only "facade depth" cue at greybox level.
	int32 StringCourseEvery = 0;
};

/** Everything the mesh builder needs; contains no engine objects. */
struct FVoidBuildingMassPlan
{
	EVoidBuildingArchetype Archetype = EVoidBuildingArchetype::SimpleExtrude;
	TArray<FVoidMassBlock> Blocks;
	TArray<FVoidRooftopBox> RooftopBoxes;
	FVoidWindowParams Windows;
	double BaseZ = 0.0;
	double FoundationDepth = 150.0;
	double FloorHeight = 300.0;
	double RoofZ = 0.0;
	double YawRadians = 0.0;
};

/**
 * FVoidBuildingMassPlanner
 *
 * Chooses a massing recipe from (category, floors, footprint size/shape) and
 * a per-building deterministic RNG, then lays out blocks, roof forms,
 * rooftop equipment and window/bay parameters. Authored footprint and height
 * are never changed: podiums, tiers, crowns and spires all subdivide the
 * authored height. Rooftop technical elements are the only things that may
 * stand above it.
 */
class VOIDWORLDBUILDERGENERATORS_API FVoidBuildingMassPlanner
{
public:
	static FVoidBuildingMassPlan Plan(const FVoidNormalizedBuilding& Building, const FVoidBuildingGenerationParams& Params);

	static const TCHAR* ArchetypeToString(EVoidBuildingArchetype Archetype);
};
