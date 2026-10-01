// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Road/VoidRoadMeshBuilder.h"

FVoidRoadMeshSection FVoidRoadMeshBuilder::BuildRibbon(
	const TArray<FVector>& CenterPoints,
	float LeftOffset,
	float RightOffset,
	float HeightOffset,
	const FLinearColor& VertexColor,
	bool bClosedLoop,
	float UvLengthScale)
{
	FVoidRoadMeshSection Section;

	const int32 NumPoints = CenterPoints.Num();
	if (NumPoints < 2)
	{
		return Section;
	}

	// "Right" vector (perpendicular to direction of travel, in the XY
	// plane) at each point, averaged from the adjacent segments so the
	// ribbon doesn't kink sharply at way points.
	TArray<FVector2D> RightVectors;
	RightVectors.SetNum(NumPoints);

	for (int32 Index = 0; Index < NumPoints; ++Index)
	{
		FVector2D Direction = FVector2D::ZeroVector;

		if (bClosedLoop)
		{
			const FVector& Prev = CenterPoints[(Index - 1 + NumPoints) % NumPoints];
			const FVector& Next = CenterPoints[(Index + 1) % NumPoints];
			Direction = FVector2D(Next.X - Prev.X, Next.Y - Prev.Y);
		}
		else if (Index == 0)
		{
			Direction = FVector2D(CenterPoints[1].X - CenterPoints[0].X, CenterPoints[1].Y - CenterPoints[0].Y);
		}
		else if (Index == NumPoints - 1)
		{
			Direction = FVector2D(CenterPoints[Index].X - CenterPoints[Index - 1].X, CenterPoints[Index].Y - CenterPoints[Index - 1].Y);
		}
		else
		{
			const FVector& Prev = CenterPoints[Index - 1];
			const FVector& Next = CenterPoints[Index + 1];
			Direction = FVector2D(Next.X - Prev.X, Next.Y - Prev.Y);
		}

		if (!Direction.IsNearlyZero())
		{
			Direction.Normalize();
		}
		else
		{
			Direction = FVector2D(1.0f, 0.0f); // Degenerate (coincident points) -- arbitrary fallback, never divide by zero downstream.
		}

		RightVectors[Index] = FVector2D(Direction.Y, -Direction.X);
	}

	TArray<FVector> LeftRow;
	TArray<FVector> RightRow;
	LeftRow.SetNum(NumPoints);
	RightRow.SetNum(NumPoints);

	for (int32 Index = 0; Index < NumPoints; ++Index)
	{
		const FVector2D& Right = RightVectors[Index];
		const FVector& Center = CenterPoints[Index];

		LeftRow[Index]  = FVector(Center.X + Right.X * LeftOffset,  Center.Y + Right.Y * LeftOffset,  Center.Z + HeightOffset);
		RightRow[Index] = FVector(Center.X + Right.X * RightOffset, Center.Y + Right.Y * RightOffset, Center.Z + HeightOffset);
	}

	TArray<float> CumulativeLength;
	CumulativeLength.SetNum(NumPoints);
	CumulativeLength[0] = 0.0f;
	for (int32 Index = 1; Index < NumPoints; ++Index)
	{
		CumulativeLength[Index] = CumulativeLength[Index - 1] + FVector::Dist(CenterPoints[Index - 1], CenterPoints[Index]);
	}

	const int32 NumSegments = bClosedLoop ? NumPoints : (NumPoints - 1);
	const float SafeUvScale = FMath::Max(UvLengthScale, 1.0f);

	// See this class's header comment: flip if strips render back-face
	// culled (invisible from above) on first in-editor verification.
	constexpr bool bFlipWindingForVerification = false;

	auto AddTriangle = [&Section](const FVector& A, const FVector& B, const FVector& C, const FVector2D& UvA, const FVector2D& UvB, const FVector2D& UvC, const FLinearColor& Color)
	{
		FVector Normal = FVector::CrossProduct(B - A, C - A);
		Normal = Normal.IsNearlyZero() ? FVector::UpVector : Normal.GetSafeNormal();

		const FProcMeshTangent ProcTangent((B - A).GetSafeNormal(), false);

		const int32 BaseIndex = Section.Vertices.Num();

		Section.Vertices.Add(A);
		Section.Vertices.Add(B);
		Section.Vertices.Add(C);

		Section.Normals.Add(Normal);
		Section.Normals.Add(Normal);
		Section.Normals.Add(Normal);

		Section.UVs.Add(UvA);
		Section.UVs.Add(UvB);
		Section.UVs.Add(UvC);

		Section.VertexColors.Add(Color);
		Section.VertexColors.Add(Color);
		Section.VertexColors.Add(Color);

		Section.Tangents.Add(ProcTangent);
		Section.Tangents.Add(ProcTangent);
		Section.Tangents.Add(ProcTangent);

		if (bFlipWindingForVerification)
		{
			Section.Triangles.Add(BaseIndex);
			Section.Triangles.Add(BaseIndex + 2);
			Section.Triangles.Add(BaseIndex + 1);
		}
		else
		{
			Section.Triangles.Add(BaseIndex);
			Section.Triangles.Add(BaseIndex + 1);
			Section.Triangles.Add(BaseIndex + 2);
		}
	};

	for (int32 SegmentIndex = 0; SegmentIndex < NumSegments; ++SegmentIndex)
	{
		const int32 IndexA = SegmentIndex;
		const int32 IndexB = (SegmentIndex + 1) % NumPoints;

		const FVector& L0 = LeftRow[IndexA];
		const FVector& R0 = RightRow[IndexA];
		const FVector& L1 = LeftRow[IndexB];
		const FVector& R1 = RightRow[IndexB];

		const float U0 = CumulativeLength[IndexA] / SafeUvScale;
		const float SegmentLength = FVector::Dist(CenterPoints[IndexA], CenterPoints[IndexB]);
		const float U1 = (U0 * SafeUvScale + SegmentLength) / SafeUvScale;

		// Two triangles per quad: (L0, R0, R1) and (L0, R1, L1).
		AddTriangle(L0, R0, R1, FVector2D(U0, 0.0f), FVector2D(U0, 1.0f), FVector2D(U1, 1.0f), VertexColor);
		AddTriangle(L0, R1, L1, FVector2D(U0, 0.0f), FVector2D(U1, 1.0f), FVector2D(U1, 0.0f), VertexColor);
	}

	return Section;
}

void FVoidRoadMeshBuilder::CreateSection(UProceduralMeshComponent* MeshComponent, int32 SectionIndex, const FVoidRoadMeshSection& Section, bool bEnableCollision)
{
	if (!MeshComponent || Section.IsEmpty())
	{
		return;
	}

	MeshComponent->CreateMeshSection_LinearColor(
		SectionIndex,
		Section.Vertices,
		Section.Triangles,
		Section.Normals,
		Section.UVs,
		Section.VertexColors,
		Section.Tangents,
		bEnableCollision);
}
