// Copyright VOID Engineering Studio. Internal tool - not for shipping content.
// Added by Agent 2 (Phase 6 Metro Generator).

#pragma once

#include "CoreMinimal.h"
#include "Road/VoidRoadMeshBuilder.h" // reuses FVoidRoadMeshSection + CreateSection (no duplicate mesh container)

/**
 * FVoidMetroMeshBuilder
 *
 * Greybox primitives for metro geometry, appended into the SAME
 * FVoidRoadMeshSection container the Road Generator uses, so both share
 * FVoidRoadMeshBuilder::CreateSection for upload.
 *
 * Winding: every quad is built through AppendQuad, which computes the
 * geometric normal of the vertices it is given and reverses the vertex order if
 * that normal disagrees with the DESIRED outward normal. So faces point the
 * right way by construction (front face = (B-A)x(C-A) along the normal, the
 * same relationship the Road Generator's ribbon relies on). The one thing that
 * cannot be verified without running an editor is whether that relationship is
 * the engine's front-face convention; SetFlipWinding() is the single switch
 * (project setting bFlipTriangleWinding) if metro geometry looks inside-out.
 */
class VOIDWORLDBUILDERGENERATORS_API FVoidMetroMeshBuilder
{
public:
	static void SetFlipWinding(bool bFlip);

	/** One flat-shaded quad (4 verts, 2 triangles) facing DesiredNormal. */
	static void AppendQuad(FVoidRoadMeshSection& Section, const FVector& V0, const FVector& V1, const FVector& V2, const FVector& V3, const FVector& DesiredNormal, const FLinearColor& Color);

	/** Closed box, 6 outward faces. */
	static void AppendOrientedBox(FVoidRoadMeshSection& Section, const FVector& Center, const FQuat& Rotation, const FVector& HalfExtents, const FLinearColor& Color);

	/** Box spanning A->B. Width = local Y, Thickness = local Z. Lateral is along local Y, Vertical is world Z. */
	static void AppendBoxAlong(FVoidRoadMeshSection& Section, const FVector& A, const FVector& B, float Width, float Thickness, float Lateral, float Vertical, const FLinearColor& Color);

	/** AppendBoxAlong for every span of a path. */
	static void AppendPathBoxes(FVoidRoadMeshSection& Section, const TArray<FVector>& Points, bool bClosed, float Width, float Thickness, float Lateral, float Vertical, const FLinearColor& Color);

	/** Inward-facing tube (a tunnel bore seen from inside) swept along a path. */
	static void AppendTunnelTube(FVoidRoadMeshSection& Section, const TArray<FVector>& Points, bool bClosed, float Radius, int32 Sides, float CenterZOffset, const FLinearColor& Color);

	/** Two jambs + lintel around a tunnel mouth. Location = centre of the opening at ground level. */
	static void AppendPortalFrame(FVoidRoadMeshSection& Section, const FVector& Location, float YawDegrees, float ClearWidth, float ClearHeight, float Depth, const FLinearColor& Color);

	static FQuat YawQuat(float YawDegrees);
};
