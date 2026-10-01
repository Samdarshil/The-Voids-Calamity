// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Building/VoidBuildingGeometry.h"

namespace
{
	inline int32 Wrap(int32 Index, int32 Count) { return ((Index % Count) + Count) % Count; }

	bool PointInTriangleInclusive(const FVector2D& P, const FVector2D& A, const FVector2D& B, const FVector2D& C)
	{
		const double D1 = FVoidBuildingGeometry::Cross(B - A, P - A);
		const double D2 = FVoidBuildingGeometry::Cross(C - B, P - B);
		const double D3 = FVoidBuildingGeometry::Cross(A - C, P - C);
		const double Eps = -1e-9;
		return D1 >= Eps && D2 >= Eps && D3 >= Eps;
	}

	double Orient(const FVector2D& A, const FVector2D& B, const FVector2D& C)
	{
		return FVoidBuildingGeometry::Cross(B - A, C - A);
	}

	bool OnSegment(const FVector2D& A, const FVector2D& B, const FVector2D& P)
	{
		return P.X >= FMath::Min(A.X, B.X) - 1e-9 && P.X <= FMath::Max(A.X, B.X) + 1e-9
			&& P.Y >= FMath::Min(A.Y, B.Y) - 1e-9 && P.Y <= FMath::Max(A.Y, B.Y) + 1e-9;
	}
}

FVector2D FVoidBuildingGeometry::SafeNormal(const FVector2D& A, const FVector2D& Fallback)
{
	const double L = Length(A);
	return L > 1e-9 ? FVector2D(A.X / L, A.Y / L) : Fallback;
}

double FVoidBuildingGeometry::SignedArea(const TArray<FVector2D>& P)
{
	const int32 N = P.Num();
	double Sum = 0.0;
	for (int32 I = 0; I < N; ++I)
	{
		const FVector2D& A = P[I];
		const FVector2D& B = P[(I + 1) % N];
		Sum += A.X * B.Y - B.X * A.Y;
	}
	return 0.5 * Sum;
}

void FVoidBuildingGeometry::EnsureCCW(TArray<FVector2D>& P)
{
	if (SignedArea(P) < 0.0)
	{
		const int32 N = P.Num();
		for (int32 I = 0; I < N / 2; ++I)
		{
			const FVector2D Tmp = P[I];
			P[I] = P[N - 1 - I];
			P[N - 1 - I] = Tmp;
		}
	}
}

void FVoidBuildingGeometry::CleanPolygon(TArray<FVector2D>& P, double DuplicateTolerance, double CollinearSine)
{
	const double TolSq = DuplicateTolerance * DuplicateTolerance;

	// Repeated points, including first == last.
	for (int32 I = P.Num() - 1; I >= 0 && P.Num() > 1; --I)
	{
		const FVector2D& A = P[I];
		const FVector2D& B = P[(I + 1) % P.Num()];
		const double DX = A.X - B.X;
		const double DY = A.Y - B.Y;
		if (DX * DX + DY * DY <= TolSq)
		{
			P.RemoveAt(I);
		}
	}

	// Near-collinear vertices.
	bool bChanged = true;
	while (bChanged && P.Num() > 3)
	{
		bChanged = false;
		for (int32 I = 0; I < P.Num() && P.Num() > 3; ++I)
		{
			const FVector2D& Prev = P[Wrap(I - 1, P.Num())];
			const FVector2D& Cur = P[I];
			const FVector2D& Next = P[(I + 1) % P.Num()];
			const FVector2D E0 = Cur - Prev;
			const FVector2D E1 = Next - Cur;
			const double L0 = Length(E0);
			const double L1 = Length(E1);
			if (L0 < 1e-9 || L1 < 1e-9 || FMath::Abs(Cross(E0, E1)) / (L0 * L1) < CollinearSine)
			{
				P.RemoveAt(I);
				bChanged = true;
				break;
			}
		}
	}

	if (P.Num() < 3)
	{
		P.Reset();
	}
}

bool FVoidBuildingGeometry::SegmentsIntersect(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D)
{
	const double O1 = Orient(A, B, C);
	const double O2 = Orient(A, B, D);
	const double O3 = Orient(C, D, A);
	const double O4 = Orient(C, D, B);

	if (((O1 > 1e-9 && O2 < -1e-9) || (O1 < -1e-9 && O2 > 1e-9)) && ((O3 > 1e-9 && O4 < -1e-9) || (O3 < -1e-9 && O4 > 1e-9)))
	{
		return true;
	}
	if (FMath::Abs(O1) <= 1e-9 && OnSegment(A, B, C)) { return true; }
	if (FMath::Abs(O2) <= 1e-9 && OnSegment(A, B, D)) { return true; }
	if (FMath::Abs(O3) <= 1e-9 && OnSegment(C, D, A)) { return true; }
	if (FMath::Abs(O4) <= 1e-9 && OnSegment(C, D, B)) { return true; }
	return false;
}

bool FVoidBuildingGeometry::IsSimple(const TArray<FVector2D>& P)
{
	const int32 N = P.Num();
	if (N < 3) { return false; }
	for (int32 I = 0; I < N; ++I)
	{
		const FVector2D& A = P[I];
		const FVector2D& B = P[(I + 1) % N];
		for (int32 J = I + 1; J < N; ++J)
		{
			if (J == I + 1 || (I == 0 && J == N - 1)) { continue; } // adjacent edges share a vertex by definition
			if (SegmentsIntersect(A, B, P[J], P[(J + 1) % N])) { return false; }
		}
	}
	return true;
}

bool FVoidBuildingGeometry::IsConvex(const TArray<FVector2D>& P)
{
	const int32 N = P.Num();
	if (N < 3) { return false; }
	for (int32 I = 0; I < N; ++I)
	{
		if (Cross(P[(I + 1) % N] - P[I], P[(I + 2) % N] - P[(I + 1) % N]) < -1e-6) { return false; }
	}
	return true;
}

FVector2D FVoidBuildingGeometry::Centroid(const TArray<FVector2D>& P)
{
	const int32 N = P.Num();
	double A2 = 0.0, CX = 0.0, CY = 0.0;
	for (int32 I = 0; I < N; ++I)
	{
		const FVector2D& A = P[I];
		const FVector2D& B = P[(I + 1) % N];
		const double C = A.X * B.Y - B.X * A.Y;
		A2 += C;
		CX += (A.X + B.X) * C;
		CY += (A.Y + B.Y) * C;
	}
	if (FMath::Abs(A2) < 1e-6)
	{
		FVector2D Avg(0.0, 0.0);
		for (const FVector2D& V : P) { Avg.X += V.X; Avg.Y += V.Y; }
		return N > 0 ? FVector2D(Avg.X / N, Avg.Y / N) : FVector2D(0.0, 0.0);
	}
	return FVector2D(CX / (3.0 * A2), CY / (3.0 * A2));
}

void FVoidBuildingGeometry::GetBounds(const TArray<FVector2D>& P, FVector2D& OutMin, FVector2D& OutMax)
{
	if (P.Num() == 0) { OutMin = FVector2D(0, 0); OutMax = FVector2D(0, 0); return; }
	OutMin = P[0];
	OutMax = P[0];
	for (const FVector2D& V : P)
	{
		OutMin.X = FMath::Min(OutMin.X, V.X); OutMin.Y = FMath::Min(OutMin.Y, V.Y);
		OutMax.X = FMath::Max(OutMax.X, V.X); OutMax.Y = FMath::Max(OutMax.Y, V.Y);
	}
}

bool FVoidBuildingGeometry::PointInPolygon(const FVector2D& Pt, const TArray<FVector2D>& P)
{
	bool bInside = false;
	const int32 N = P.Num();
	for (int32 I = 0, J = N - 1; I < N; J = I++)
	{
		const FVector2D& A = P[I];
		const FVector2D& B = P[J];
		if (((A.Y > Pt.Y) != (B.Y > Pt.Y)) && (Pt.X < (B.X - A.X) * (Pt.Y - A.Y) / (B.Y - A.Y) + A.X))
		{
			bInside = !bInside;
		}
	}
	return bInside;
}

double FVoidBuildingGeometry::MinDimension(const TArray<FVector2D>& P)
{
	const int32 N = P.Num();
	double Best = TNumericLimits<double>::Max();
	for (int32 I = 0; I < N; ++I)
	{
		const FVector2D Dir = SafeNormal(P[(I + 1) % N] - P[I]);
		const FVector2D Nrm(-Dir.Y, Dir.X);
		double Lo = TNumericLimits<double>::Max(), Hi = -TNumericLimits<double>::Max();
		for (const FVector2D& V : P)
		{
			const double D = Dot(V, Nrm);
			Lo = FMath::Min(Lo, D);
			Hi = FMath::Max(Hi, D);
		}
		Best = FMath::Min(Best, Hi - Lo);
	}
	return N > 0 ? Best : 0.0;
}

int32 FVoidBuildingGeometry::LongestEdgeIndex(const TArray<FVector2D>& P)
{
	const int32 N = P.Num();
	int32 BestIndex = 0;
	double BestLen = -1.0;
	for (int32 I = 0; I < N; ++I)
	{
		const double L = Length(P[(I + 1) % N] - P[I]);
		if (L > BestLen) { BestLen = L; BestIndex = I; }
	}
	return BestIndex;
}

double FVoidBuildingGeometry::LongestEdgeYaw(const TArray<FVector2D>& P)
{
	if (P.Num() < 2) { return 0.0; }
	const int32 I = LongestEdgeIndex(P);
	const FVector2D E = P[(I + 1) % P.Num()] - P[I];
	return FMath::Atan2(E.Y, E.X);
}

bool FVoidBuildingGeometry::InsetPolygon(const TArray<FVector2D>& P, double Dist, TArray<FVector2D>& Out)
{
	Out.Reset();
	const int32 N = P.Num();
	if (N < 3 || Dist <= 0.0) { return false; }
	const double A0 = SignedArea(P);
	if (A0 <= 0.0) { return false; }

	TArray<FVector2D> EdgeDir, EdgeNormal;
	EdgeDir.Reserve(N);
	EdgeNormal.Reserve(N);
	for (int32 I = 0; I < N; ++I)
	{
		const FVector2D D = SafeNormal(P[(I + 1) % N] - P[I]);
		EdgeDir.Add(D);
		EdgeNormal.Add(FVector2D(-D.Y, D.X)); // left of a CCW edge = inward
	}

	TArray<FVector2D> Result;
	Result.Reserve(N);
	for (int32 I = 0; I < N; ++I)
	{
		const FVector2D& N0 = EdgeNormal[Wrap(I - 1, N)];
		const FVector2D& N1 = EdgeNormal[I];
		const double Denom = 1.0 + Dot(N0, N1);
		if (Denom < 0.2) { return false; } // needle-sharp corner: mitre would explode
		const FVector2D M = N0 + N1;
		Result.Add(FVector2D(P[I].X + M.X * (Dist / Denom), P[I].Y + M.Y * (Dist / Denom)));
	}

	const double A1 = SignedArea(Result);
	if (A1 <= 1.0 || A1 >= A0) { return false; }
	for (int32 I = 0; I < N; ++I)
	{
		if (Dot(Result[(I + 1) % N] - Result[I], EdgeDir[I]) <= 0.0) { return false; } // edge collapsed or flipped
	}
	if (!IsSimple(Result)) { return false; }

	Out = MoveTemp(Result);
	return true;
}

bool FVoidBuildingGeometry::InsetPolygonWithFallback(const TArray<FVector2D>& P, double Dist, TArray<FVector2D>& Out)
{
	static const double Factors[] = { 1.0, 0.6, 0.35, 0.2 };
	for (const double F : Factors)
	{
		if (InsetPolygon(P, Dist * F, Out)) { return true; }
	}
	Out.Reset();
	return false;
}

bool FVoidBuildingGeometry::Triangulate(const TArray<FVector2D>& P, TArray<int32>& OutIndices)
{
	OutIndices.Reset();
	const int32 N = P.Num();
	if (N < 3) { return false; }

	TArray<int32> V;
	V.Reserve(N);
	for (int32 I = 0; I < N; ++I) { V.Add(I); }

	while (V.Num() > 3)
	{
		bool bFoundEar = false;
		const int32 M = V.Num();
		for (int32 I = 0; I < M; ++I)
		{
			const int32 IPrev = V[Wrap(I - 1, M)];
			const int32 ICur = V[I];
			const int32 INext = V[(I + 1) % M];
			const FVector2D& A = P[IPrev];
			const FVector2D& B = P[ICur];
			const FVector2D& C = P[INext];
			if (Cross(B - A, C - B) <= 1e-9) { continue; } // reflex or degenerate

			bool bBlocked = false;
			for (int32 K = 0; K < M; ++K)
			{
				const int32 IK = V[K];
				if (IK == IPrev || IK == ICur || IK == INext) { continue; }
				if (PointInTriangleInclusive(P[IK], A, B, C)) { bBlocked = true; break; }
			}
			if (bBlocked) { continue; }

			OutIndices.Add(IPrev);
			OutIndices.Add(ICur);
			OutIndices.Add(INext);
			V.RemoveAt(I);
			bFoundEar = true;
			break;
		}

		if (!bFoundEar)
		{
			// Degenerate leftovers (collinear runs): drop one flat vertex and retry, else give up.
			bool bRemoved = false;
			for (int32 I = 0; I < V.Num(); ++I)
			{
				const FVector2D& A = P[V[Wrap(I - 1, V.Num())]];
				const FVector2D& B = P[V[I]];
				const FVector2D& C = P[V[(I + 1) % V.Num()]];
				if (FMath::Abs(Cross(B - A, C - B)) <= 1e-9) { V.RemoveAt(I); bRemoved = true; break; }
			}
			if (!bRemoved) { OutIndices.Reset(); return false; }
		}
	}

	if (V.Num() == 3)
	{
		OutIndices.Add(V[0]);
		OutIndices.Add(V[1]);
		OutIndices.Add(V[2]);
	}
	return OutIndices.Num() >= 3;
}

double FVoidBuildingGeometry::PointSegmentDistSq(const FVector2D& P, const FVector2D& A, const FVector2D& B, FVector2D* OutClosest)
{
	const FVector2D AB = B - A;
	const double Len2 = Dot(AB, AB);
	double T = 0.0;
	if (Len2 > 1e-12)
	{
		T = FMath::Clamp(Dot(P - A, AB) / Len2, 0.0, 1.0);
	}
	const FVector2D C(A.X + AB.X * T, A.Y + AB.Y * T);
	if (OutClosest) { *OutClosest = C; }
	const FVector2D D = P - C;
	return Dot(D, D);
}

double FVoidBuildingGeometry::PolygonSegmentDistance(const TArray<FVector2D>& Poly, const FVector2D& A, const FVector2D& B, FVector2D* OutPolyPoint, FVector2D* OutSegPoint)
{
	const int32 N = Poly.Num();
	if (N < 3) { return TNumericLimits<double>::Max(); }

	if (PointInPolygon(A, Poly) || PointInPolygon(B, Poly)) { return 0.0; }

	double BestSq = TNumericLimits<double>::Max();
	FVector2D BestPoly(0, 0), BestSeg(0, 0);

	for (int32 I = 0; I < N; ++I)
	{
		const FVector2D& P = Poly[I];
		const FVector2D& Q = Poly[(I + 1) % N];
		if (SegmentsIntersect(P, Q, A, B)) { return 0.0; }

		FVector2D C;
		double D;

		D = PointSegmentDistSq(P, A, B, &C);
		if (D < BestSq) { BestSq = D; BestPoly = P; BestSeg = C; }
		D = PointSegmentDistSq(Q, A, B, &C);
		if (D < BestSq) { BestSq = D; BestPoly = Q; BestSeg = C; }
		D = PointSegmentDistSq(A, P, Q, &C);
		if (D < BestSq) { BestSq = D; BestPoly = C; BestSeg = A; }
		D = PointSegmentDistSq(B, P, Q, &C);
		if (D < BestSq) { BestSq = D; BestPoly = C; BestSeg = B; }
	}

	if (OutPolyPoint) { *OutPolyPoint = BestPoly; }
	if (OutSegPoint) { *OutSegPoint = BestSeg; }
	return FMath::Sqrt(BestSq);
}

bool FVoidBuildingGeometry::PolygonsOverlap(const TArray<FVector2D>& A, const TArray<FVector2D>& B)
{
	const int32 NA = A.Num(), NB = B.Num();
	if (NA < 3 || NB < 3) { return false; }
	for (int32 I = 0; I < NA; ++I)
	{
		for (int32 J = 0; J < NB; ++J)
		{
			if (SegmentsIntersect(A[I], A[(I + 1) % NA], B[J], B[(J + 1) % NB])) { return true; }
		}
	}
	return PointInPolygon(A[0], B) || PointInPolygon(B[0], A);
}

uint32 FVoidBuildingGeometry::StableHash(const FString& Text)
{
	uint32 Hash = 2166136261u;
	const int32 Len = Text.Len();
	for (int32 I = 0; I < Len; ++I)
	{
		uint32 C = static_cast<uint32>(Text[I]);
		if (C >= 'A' && C <= 'Z') { C += 32u; }
		Hash ^= C;
		Hash *= 16777619u;
	}
	return Hash;
}
