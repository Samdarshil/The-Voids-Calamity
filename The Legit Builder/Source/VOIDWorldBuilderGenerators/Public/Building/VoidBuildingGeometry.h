// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"

/**
 * Small deterministic PRNG (PCG-style, 32-bit integer arithmetic only), used
 * instead of FRandomStream/FMath::Rand so the sequence is identical on every
 * platform and engine version. Seeded per building from a stable hash of its id.
 */
struct FVoidBuildingRng
{
	uint32 State;

	explicit FVoidBuildingRng(uint32 Seed)
		: State(Seed * 747796405u + 2891336453u)
	{
		NextU32();
	}

	uint32 NextU32()
	{
		State = State * 747796405u + 2891336453u;
		const uint32 Word = ((State >> ((State >> 28u) + 4u)) ^ State) * 277803737u;
		return (Word >> 22u) ^ Word;
	}

	/** [0, 1) */
	double NextDouble() { return static_cast<double>(NextU32() >> 8) * (1.0 / 16777216.0); }
	double Range(double Min, double Max) { return Min + (Max - Min) * NextDouble(); }
	bool Chance(double Probability) { return NextDouble() < Probability; }

	/** Inclusive on both ends. */
	int32 RangeInt(int32 Min, int32 Max)
	{
		if (Max <= Min) { return Min; }
		const int32 Span = Max - Min + 1;
		int32 Value = Min + static_cast<int32>(NextDouble() * Span);
		return Value > Max ? Max : Value;
	}
};

/**
 * FVoidBuildingGeometry
 *
 * Engine-independent 2D polygon math used by the Building Generator. Every
 * function is pure (no world, no UObject) so it can be unit-tested in
 * isolation. Polygons are TArray<FVector2D> with implicit closure; "CCW"
 * means counter-clockwise in raw X/Y (positive signed area).
 */
class VOIDWORLDBUILDERGENERATORS_API FVoidBuildingGeometry
{
public:
	static double Dot(const FVector2D& A, const FVector2D& B) { return A.X * B.X + A.Y * B.Y; }
	static double Cross(const FVector2D& A, const FVector2D& B) { return A.X * B.Y - A.Y * B.X; }
	static double Length(const FVector2D& A) { return FMath::Sqrt(Dot(A, A)); }
	static FVector2D SafeNormal(const FVector2D& A, const FVector2D& Fallback = FVector2D(1.0, 0.0));

	/** > 0 for CCW. */
	static double SignedArea(const TArray<FVector2D>& P);
	static void EnsureCCW(TArray<FVector2D>& P);

	/** Drops repeated points (incl. a duplicated closing point) and near-collinear vertices. Leaves >= 3 points or empties the polygon. */
	static void CleanPolygon(TArray<FVector2D>& P, double DuplicateTolerance = 1.0, double CollinearSine = 0.002);

	/** True if no two non-adjacent edges touch or cross. */
	static bool IsSimple(const TArray<FVector2D>& P);
	static bool IsConvex(const TArray<FVector2D>& P);

	static FVector2D Centroid(const TArray<FVector2D>& P);
	static void GetBounds(const TArray<FVector2D>& P, FVector2D& OutMin, FVector2D& OutMax);
	static bool PointInPolygon(const FVector2D& Pt, const TArray<FVector2D>& P);

	/** Smallest extent measured perpendicular to any edge; a cheap "how narrow is this building" figure. */
	static double MinDimension(const TArray<FVector2D>& P);
	/** Yaw (radians) of the longest edge. */
	static double LongestEdgeYaw(const TArray<FVector2D>& P);
	static int32 LongestEdgeIndex(const TArray<FVector2D>& P);

	/**
	 * Mitre inset of a CCW simple polygon by Dist. The output has exactly one
	 * vertex per input vertex (index correspondence is relied upon to build
	 * ring caps between a tier and the tier above it). Returns false, leaving
	 * Out empty, if the result would collapse, flip an edge or self-intersect.
	 */
	static bool InsetPolygon(const TArray<FVector2D>& P, double Dist, TArray<FVector2D>& Out);

	/** Tries Dist, then progressively smaller distances. */
	static bool InsetPolygonWithFallback(const TArray<FVector2D>& P, double Dist, TArray<FVector2D>& Out);

	/** Ear-clip triangulation of a CCW simple polygon. Indices are CCW triples. */
	static bool Triangulate(const TArray<FVector2D>& P, TArray<int32>& OutIndices);

	static double PointSegmentDistSq(const FVector2D& P, const FVector2D& A, const FVector2D& B, FVector2D* OutClosest = nullptr);
	static bool SegmentsIntersect(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D);

	/**
	 * Distance from a polygon (as a filled region) to a segment; 0 if they overlap.
	 * OutPolyPoint / OutSegPoint are the closest pair when the distance is > 0.
	 */
	static double PolygonSegmentDistance(const TArray<FVector2D>& Poly, const FVector2D& A, const FVector2D& B, FVector2D* OutPolyPoint = nullptr, FVector2D* OutSegPoint = nullptr);

	/** True if two simple polygons share any area or boundary point (edge crossing or containment). */
	static bool PolygonsOverlap(const TArray<FVector2D>& A, const TArray<FVector2D>& B);

	/** FNV-1a over the lower-cased characters: platform-independent, unlike GetTypeHash(FName). */
	static uint32 StableHash(const FString& Text);
};
