// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Building/VoidBuildingTypes.h"
#include "Building/VoidBuildingParams.h"
#include "Building/VoidBuildingMassing.h"

/** Raw mesh data for one surface class. Mirrors FVoidRoadMeshSection (minus tangents; flat-shaded greybox). */
struct FVoidBuildingMeshSection
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> VertexColors;

	bool IsEmpty() const { return Vertices.Num() == 0; }
};

/**
 * Collects geometry for many buildings into one section per surface class,
 * in coordinates relative to Origin (the batch actor's location), so a batch
 * of hundreds of buildings is one component and at most six draw calls.
 *
 * Winding: UE front faces are clockwise as seen from the front, which for
 * raw coordinates means (B-A)x(C-A) points along the face normal. Every
 * emitter below relies on that.
 */
class VOIDWORLDBUILDERGENERATORS_API FVoidBuildingMeshAccumulator
{
public:
	FVector Origin = FVector::ZeroVector;
	FVoidBuildingMeshSection Sections[static_cast<int32>(EVoidBuildingSurface::Count)];

	FVoidBuildingMeshSection& Get(EVoidBuildingSurface Surface) { return Sections[static_cast<int32>(Surface)]; }
	const FVoidBuildingMeshSection& Get(EVoidBuildingSurface Surface) const { return Sections[static_cast<int32>(Surface)]; }

	void AddTriangle(EVoidBuildingSurface Surface, const FVector& A, const FVector& B, const FVector& C, const FLinearColor& Color,
		const FVector2D& UVA, const FVector2D& UVB, const FVector2D& UVC);

	/** Quad A(bottom-left) B(bottom-right) C(top-right) D(top-left); triangles (A,B,C) and (A,C,D). */
	void AddQuad(EVoidBuildingSurface Surface, const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FLinearColor& Color,
		float U0, float U1, float V0, float V1);

	int32 NumTriangles() const;
	int32 NumVertices() const;
};

class VOIDWORLDBUILDERGENERATORS_API FVoidBuildingMeshBuilder
{
public:
	/** Emits every surface of one building into the accumulator. World-space input; the accumulator applies its Origin. */
	static void Emit(
		const FVoidNormalizedBuilding& Building,
		const FVoidBuildingMassPlan& Plan,
		const FVoidBuildingGenerationParams& Params,
		FVoidBuildingMeshAccumulator& Accumulator);

	/** Base wall colour per category (vertex colour; a later Environment pass may re-tint per district). */
	static FLinearColor CategoryColor(EVoidBuildingCategory Category);

	/** Number of window quads the plan would emit if punched windows were emitted individually. */
	static int32 EstimatePunchedWindowQuads(const FVoidBuildingMassPlan& Plan);
};
