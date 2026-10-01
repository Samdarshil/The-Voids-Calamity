// Copyright VOID Engineering Studio. Internal tool - not for shipping content.
// Added by Agent 2 (Phase 6 Metro Generator).

#include "Metro/VoidMetroMeshBuilder.h"

namespace VoidMetroMeshPrivate
{
	static bool GFlipWinding = false;
}

void FVoidMetroMeshBuilder::SetFlipWinding(bool bFlip)
{
	VoidMetroMeshPrivate::GFlipWinding = bFlip;
}

FQuat FVoidMetroMeshBuilder::YawQuat(float YawDegrees)
{
	return FQuat(FVector::UpVector, FMath::DegreesToRadians(YawDegrees));
}

void FVoidMetroMeshBuilder::AppendQuad(FVoidRoadMeshSection& S, const FVector& V0, const FVector& V1In, const FVector& V2, const FVector& V3In, const FVector& DesiredNormal, const FLinearColor& Color)
{
	FVector V1 = V1In;
	FVector V3 = V3In;

	// Geometric normal of (V0,V1,V2). If it opposes the desired normal, reverse the winding.
	FVector Geo = FVector::CrossProduct(V1 - V0, V2 - V0);
	if (FVector::DotProduct(Geo, DesiredNormal) < 0.0f)
	{
		Swap(V1, V3);
		Geo = -Geo;
	}
	const FVector N = Geo.IsNearlyZero() ? DesiredNormal.GetSafeNormal() : Geo.GetSafeNormal();
	const FProcMeshTangent Tangent((V1 - V0).GetSafeNormal(), false);

	const int32 Base = S.Vertices.Num();
	const FVector Verts[4] = { V0, V1, V2, V3 };
	const FVector2D Uvs[4] = { FVector2D(0, 0), FVector2D(1, 0), FVector2D(1, 1), FVector2D(0, 1) };
	for (int32 i = 0; i < 4; ++i)
	{
		S.Vertices.Add(Verts[i]);
		S.Normals.Add(N);
		S.UVs.Add(Uvs[i]);
		S.VertexColors.Add(Color);
		S.Tangents.Add(Tangent);
	}

	if (VoidMetroMeshPrivate::GFlipWinding)
	{
		S.Triangles.Append({ Base, Base + 2, Base + 1, Base, Base + 3, Base + 2 });
	}
	else
	{
		S.Triangles.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
	}
}

void FVoidMetroMeshBuilder::AppendOrientedBox(FVoidRoadMeshSection& S, const FVector& Center, const FQuat& Rot, const FVector& H, const FLinearColor& Color)
{
	if (H.X <= 0.0f || H.Y <= 0.0f || H.Z <= 0.0f)
	{
		return;
	}
	const FVector AX = Rot.GetAxisX();
	const FVector AY = Rot.GetAxisY();
	const FVector AZ = Rot.GetAxisZ();

	// Face = (normal axis, sign, tangent axis u, tangent axis v) with half extents. Winding is corrected in AppendQuad.
	struct FFace { const FVector& N; float HN; const FVector& U; float HU; const FVector& V; float HV; };
	const FFace Faces[3] = {
		{ AX, H.X, AY, H.Y, AZ, H.Z },
		{ AY, H.Y, AZ, H.Z, AX, H.X },
		{ AZ, H.Z, AX, H.X, AY, H.Y },
	};
	for (const FFace& F : Faces)
	{
		for (int32 Sign = -1; Sign <= 1; Sign += 2)
		{
			const FVector N = F.N * static_cast<float>(Sign);
			const FVector C = Center + N * F.HN;
			const FVector U = F.U * F.HU;
			const FVector V = F.V * F.HV;
			AppendQuad(S, C - U - V, C + U - V, C + U + V, C - U + V, N, Color);
		}
	}
}

void FVoidMetroMeshBuilder::AppendBoxAlong(FVoidRoadMeshSection& S, const FVector& A, const FVector& B, float Width, float Thickness, float Lateral, float Vertical, const FLinearColor& Color)
{
	const FVector D = B - A;
	const float Len = D.Size();
	if (Len < 1.0f)
	{
		return;
	}
	const FVector Dir = D / Len;
	// Near-vertical spans have no meaningful "up-facing" frame; skip rather than emit a degenerate basis.
	if (FMath::Abs(Dir.Z) > 0.995f)
	{
		return;
	}
	const FQuat Rot = FRotationMatrix::MakeFromXZ(Dir, FVector::UpVector).ToQuat();
	FVector Center = (A + B) * 0.5f + Rot.GetAxisY() * Lateral;
	Center.Z += Vertical;
	// Small overlap hides hairline gaps between consecutive spans on gentle bends.
	AppendOrientedBox(S, Center, Rot, FVector(Len * 0.5f + 2.0f, Width * 0.5f, Thickness * 0.5f), Color);
}

void FVoidMetroMeshBuilder::AppendPathBoxes(FVoidRoadMeshSection& S, const TArray<FVector>& P, bool bClosed, float Width, float Thickness, float Lateral, float Vertical, const FLinearColor& Color)
{
	const int32 N = P.Num();
	const int32 Spans = bClosed ? N : N - 1;
	for (int32 i = 0; i < Spans; ++i)
	{
		AppendBoxAlong(S, P[i], P[(i + 1) % N], Width, Thickness, Lateral, Vertical, Color);
	}
}

void FVoidMetroMeshBuilder::AppendTunnelTube(FVoidRoadMeshSection& S, const TArray<FVector>& P, bool bClosed, float Radius, int32 Sides, float CenterZOffset, const FLinearColor& Color)
{
	const int32 N = P.Num();
	if (N < 2 || Sides < 3 || Radius <= 0.0f)
	{
		return;
	}

	// Ring frames.
	TArray<FVector> Right, Up;
	Right.SetNum(N);
	Up.SetNum(N);
	for (int32 i = 0; i < N; ++i)
	{
		FVector T;
		if (bClosed) { T = P[(i + 1) % N] - P[(i - 1 + N) % N]; }
		else if (i == 0) { T = P[1] - P[0]; }
		else if (i == N - 1) { T = P[i] - P[i - 1]; }
		else { T = P[i + 1] - P[i - 1]; }
		T = T.GetSafeNormal();
		if (T.IsNearlyZero()) { T = FVector::ForwardVector; }
		FVector R = FVector::CrossProduct(T, FVector::UpVector).GetSafeNormal();
		if (R.IsNearlyZero()) { R = FVector::RightVector; }
		Right[i] = R;
		Up[i] = FVector::CrossProduct(R, T).GetSafeNormal();
	}

	auto RingPoint = [&](int32 i, int32 k)
	{
		const float A = 2.0f * PI * static_cast<float>(k % Sides) / static_cast<float>(Sides);
		FVector C = P[i];
		C.Z += CenterZOffset;
		return C + (Right[i] * FMath::Cos(A) + Up[i] * FMath::Sin(A)) * Radius;
	};

	const int32 Spans = bClosed ? N : N - 1;
	for (int32 i = 0; i < Spans; ++i)
	{
		const int32 j = (i + 1) % N;
		for (int32 k = 0; k < Sides; ++k)
		{
			const FVector A = RingPoint(i, k);
			const FVector B = RingPoint(i, k + 1);
			const FVector C = RingPoint(j, k + 1);
			const FVector D = RingPoint(j, k);
			FVector Centre = (P[i] + P[j]) * 0.5f;
			Centre.Z += CenterZOffset;
			const FVector Inward = (Centre - (A + B + C + D) * 0.25f).GetSafeNormal();
			AppendQuad(S, A, B, C, D, Inward, Color);
		}
	}
}

void FVoidMetroMeshBuilder::AppendPortalFrame(FVoidRoadMeshSection& S, const FVector& Location, float YawDegrees, float ClearWidth, float ClearHeight, float Depth, const FLinearColor& Color)
{
	const FQuat Rot = YawQuat(YawDegrees);
	const float Jamb = 80.0f;
	const FVector Y = Rot.GetAxisY();
	const float Off = ClearWidth * 0.5f + Jamb * 0.5f;
	AppendOrientedBox(S, Location - Y * Off + FVector(0, 0, ClearHeight * 0.5f), Rot, FVector(Depth * 0.5f, Jamb * 0.5f, ClearHeight * 0.5f), Color);
	AppendOrientedBox(S, Location + Y * Off + FVector(0, 0, ClearHeight * 0.5f), Rot, FVector(Depth * 0.5f, Jamb * 0.5f, ClearHeight * 0.5f), Color);
	AppendOrientedBox(S, Location + FVector(0, 0, ClearHeight + Jamb * 0.5f), Rot, FVector(Depth * 0.5f, ClearWidth * 0.5f + Jamb, Jamb * 0.5f), Color);
}
