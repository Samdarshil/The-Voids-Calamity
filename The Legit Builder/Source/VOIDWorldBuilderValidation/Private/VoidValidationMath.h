// Copyright VOID Engineering Studio. Internal tool - not for shipping content.
//
// Engine-independent 2D geometry helpers used by the package validators.
// Raw arrays + <cmath> only, so they compile standalone (see
// Tools/StandaloneTests) and cannot be affected by engine API churn.
// Double precision throughout; design data is far more likely to be wrong
// than to need more than double.

#pragma once

#include <cmath>
#include <cstddef>

namespace VoidValidationMath
{
	struct FV2
	{
		double X = 0.0;
		double Y = 0.0;
	};

	inline bool IsFinite(double V) { return std::isfinite(V); }
	inline bool IsFinite(const FV2& P) { return std::isfinite(P.X) && std::isfinite(P.Y); }

	inline double DistSq(const FV2& A, const FV2& B) { const double DX = A.X - B.X, DY = A.Y - B.Y; return DX * DX + DY * DY; }

	/** Twice the signed area of triangle ABC (positive when C is left of A->B). */
	inline double Cross(const FV2& A, const FV2& B, const FV2& C) { return (B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X); }

	/** Shoelace signed area; positive = counter-clockwise. */
	inline double PolygonSignedArea(const FV2* P, size_t N)
	{
		double Sum = 0.0;
		for (size_t i = 0; i < N; ++i)
		{
			const FV2& A = P[i];
			const FV2& B = P[(i + 1) % N];
			Sum += A.X * B.Y - B.X * A.Y;
		}
		return 0.5 * Sum;
	}

	inline double PointSegmentDistSq(const FV2& P, const FV2& A, const FV2& B)
	{
		const double DX = B.X - A.X, DY = B.Y - A.Y;
		const double LenSq = DX * DX + DY * DY;
		if (LenSq <= 0.0) { return DistSq(P, A); }
		double T = ((P.X - A.X) * DX + (P.Y - A.Y) * DY) / LenSq;
		T = T < 0.0 ? 0.0 : (T > 1.0 ? 1.0 : T);
		const FV2 Q{ A.X + T * DX, A.Y + T * DY };
		return DistSq(P, Q);
	}

	/** Proper crossing only: segments AB and CD cross at a single interior point of both. Touching at an endpoint or collinear overlap returns false (that is a junction/overlap, reported elsewhere). */
	inline bool SegmentsProperlyCross(const FV2& A, const FV2& B, const FV2& C, const FV2& D, FV2* OutPoint = nullptr)
	{
		const double D1 = Cross(C, D, A), D2 = Cross(C, D, B), D3 = Cross(A, B, C), D4 = Cross(A, B, D);
		if (((D1 > 0.0 && D2 < 0.0) || (D1 < 0.0 && D2 > 0.0)) && ((D3 > 0.0 && D4 < 0.0) || (D3 < 0.0 && D4 > 0.0)))
		{
			if (OutPoint)
			{
				const double T = D3 / (D3 - D4);
				OutPoint->X = C.X + (D.X - C.X) * T;
				OutPoint->Y = C.Y + (D.Y - C.Y) * T;
			}
			return true;
		}
		return false;
	}

	/** Even-odd rule. Points exactly on an edge may go either way; callers use tolerances. */
	inline bool PointInPolygon(const FV2& P, const FV2* Poly, size_t N)
	{
		bool bInside = false;
		for (size_t i = 0, j = N - 1; i < N; j = i++)
		{
			if (((Poly[i].Y > P.Y) != (Poly[j].Y > P.Y)) &&
				(P.X < (Poly[j].X - Poly[i].X) * (P.Y - Poly[i].Y) / (Poly[j].Y - Poly[i].Y) + Poly[i].X))
			{
				bInside = !bInside;
			}
		}
		return bInside;
	}

	/** True if any two non-adjacent edges of the polygon properly cross. O(N^2): callers cap N. */
	inline bool PolygonSelfIntersects(const FV2* P, size_t N)
	{
		if (N < 4) { return false; }
		for (size_t i = 0; i < N; ++i)
		{
			const FV2& A = P[i];
			const FV2& B = P[(i + 1) % N];
			for (size_t j = i + 1; j < N; ++j)
			{
				if (j == i + 1 || (i == 0 && j == N - 1)) { continue; } // adjacent edges share a vertex
				if (SegmentsProperlyCross(A, B, P[j], P[(j + 1) % N])) { return true; }
			}
		}
		return false;
	}

	/** Two simple polygons overlap if an edge pair properly crosses or one contains a vertex of the other. */
	inline bool PolygonsOverlap(const FV2* A, size_t NA, const FV2* B, size_t NB)
	{
		for (size_t i = 0; i < NA; ++i)
		{
			for (size_t j = 0; j < NB; ++j)
			{
				if (SegmentsProperlyCross(A[i], A[(i + 1) % NA], B[j], B[(j + 1) % NB])) { return true; }
			}
		}
		return PointInPolygon(A[0], B, NB) || PointInPolygon(B[0], A, NA);
	}

	/** Angle in degrees, [0,180], between directions U and V. Zero-length input returns 0. */
	inline double AngleBetweenDegrees(const FV2& U, const FV2& V)
	{
		const double LU = std::sqrt(U.X * U.X + U.Y * U.Y), LV = std::sqrt(V.X * V.X + V.Y * V.Y);
		if (LU <= 0.0 || LV <= 0.0) { return 0.0; }
		double C = (U.X * V.X + U.Y * V.Y) / (LU * LV);
		C = C < -1.0 ? -1.0 : (C > 1.0 ? 1.0 : C);
		return std::acos(C) * 180.0 / 3.14159265358979323846;
	}
}
