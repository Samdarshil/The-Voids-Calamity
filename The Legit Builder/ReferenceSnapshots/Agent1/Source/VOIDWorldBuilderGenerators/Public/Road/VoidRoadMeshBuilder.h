// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"

/**
 * Plain vertex/triangle data for one ProceduralMeshComponent section,
 * ready to hand to UProceduralMeshComponent::CreateMeshSection_LinearColor.
 * A plain struct (not a USTRUCT) -- this is intermediate geometry data
 * produced and consumed entirely within the Generators module, never
 * serialized or exposed to Blueprint.
 */
struct VOIDWORLDBUILDERGENERATORS_API FVoidRoadMeshSection
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;

	bool IsEmpty() const { return Vertices.Num() == 0; }
};

/**
 * FVoidRoadMeshBuilder
 *
 * Generates flat-shaded ribbon-strip geometry following a polyline of
 * center points, offset laterally by a given range on each side. One
 * function covers every flat "strip along a path" need in Phase 3: the
 * road surface itself, sidewalks, curbs, and medians are all the same
 * ribbon math at different offsets and colors -- this is a deliberate
 * consolidation (see Docs/RoadGeneratorArchitecture.md) rather than
 * separate classes per surface type, since they share 100% of their
 * geometry logic and differ only in parameters.
 *
 * Greybox scope: flat-shaded (unique vertices per triangle, no vertex
 * welding/smoothing), face normals computed per triangle. This is the
 * correct level of fidelity for blockout geometry -- see Volume IV's
 * "generate placeholder blockout, not final art" note -- not a
 * shortfall relative to some higher standard this tool isn't meant to
 * meet yet.
 *
 * Winding note: triangles are wound assuming Unreal's convention of
 * clockwise-when-viewed-from-the-direction-the-normal-points. If a strip
 * renders back-face-culled (invisible from above) in-editor, flip
 * `bFlipWindingForVerification` at the top of the .cpp -- this could not
 * be verified by compiling in this environment, so it's called out
 * explicitly rather than asserted with false confidence.
 */
class VOIDWORLDBUILDERGENERATORS_API FVoidRoadMeshBuilder
{
public:
	/**
	 * Builds a ribbon strip along CenterPoints, offset laterally between
	 * LeftOffset and RightOffset (signed, relative to direction of
	 * travel; e.g. a road surface of WidthUnits centered on the path is
	 * LeftOffset = -WidthUnits/2, RightOffset = +WidthUnits/2), raised by
	 * HeightOffset on the Z axis (e.g. a curb sitting slightly above the
	 * road surface).
	 * @param UvLengthScale Divides the running arc length for the U texture coordinate, so ribbons of different widths still get a sensible UV scale; pass the ribbon's own width for a roughly 1:1 texel-per-unit result.
	 */
	static FVoidRoadMeshSection BuildRibbon(
		const TArray<FVector>& CenterPoints,
		float LeftOffset,
		float RightOffset,
		float HeightOffset,
		const FLinearColor& VertexColor,
		bool bClosedLoop,
		float UvLengthScale = 100.0f);

	/** Appends Section's data into UProceduralMeshComponent as a new section (index chosen by the caller); no-ops if Section is empty. */
	static void CreateSection(UProceduralMeshComponent* MeshComponent, int32 SectionIndex, const FVoidRoadMeshSection& Section, bool bEnableCollision);
};
