// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"

/** Oriented 2D box: center, half extents along its own X/Y axes, and yaw (radians, CCW from world +X). */
struct VOIDWORLDBUILDERGENERATORS_API FVoidBox2D
{
	FVector2D Center = FVector2D::ZeroVector;
	FVector2D HalfExtent = FVector2D::ZeroVector;
	double YawRadians = 0.0;

	FVector2D AxisX() const;
	FVector2D AxisY() const;

	/** Corners in CCW order: (-x,-y), (+x,-y), (+x,+y), (-x,+y). */
	void GetCorners(TArray<FVector2D>& OutCorners) const;
	TArray<FVector2D> GetCorners() const;

	FVoidBox2D Inflated(double Margin) const;
};

/**
 * Deterministic xorshift RNG. Generation must be reproducible run-to-run
 * (regeneration must not reshuffle a district), and FRandomStream is not
 * needed for that; this keeps planning engine-independent.
 */
struct VOIDWORLDBUILDERGENERATORS_API FVoidDistrictRng
{
	uint32 State = 2463534242u;

	explicit FVoidDistrictRng(uint32 Seed = 1u);

	uint32 NextUInt();

	/** [0, 1) */
	double NextUnit();

	/** Inclusive range. */
	int32 NextInt(int32 MinInclusive, int32 MaxInclusive);
};

/**
 * FVoidDistrictGeometry
 *
 * Stateless 2D helpers used by layout and validation. All XY in Unreal
 * units. Kept free of UObject / world access on purpose.
 */
class VOIDWORLDBUILDERGENERATORS_API FVoidDistrictGeometry
{
public:
	/** Stable across platforms and runs (FNV-1a over the UTF-8-ish characters). Used to seed per-district RNG streams. */
	static uint32 StableHash(const FString& Text);

	static FVector2D PolarPoint(const FVector2D& Center, double Radius, double AngleRadians);

	/** CCW regular polygon approximating a circle. */
	static TArray<FVector2D> MakeCirclePolygon(const FVector2D& Center, double Radius, int32 Segments);

	static double PolygonAreaSigned(const TArray<FVector2D>& Polygon);
	static double PolygonArea(const TArray<FVector2D>& Polygon);
	static FVector2D PolygonCentroid(const TArray<FVector2D>& Polygon);
	static void EnsureCounterClockwise(TArray<FVector2D>& Polygon);

	static bool IsPointInPolygon(const FVector2D& Point, const TArray<FVector2D>& Polygon);

	/** Point inside polygon and (if HoleRadius > 0) outside the circular hole. */
	static bool IsPointInShape(const FVector2D& Point, const TArray<FVector2D>& Polygon, const FVector2D& HoleCenter, double HoleRadius);

	static bool SegmentsIntersect(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D);
	static double DistancePointToSegment(const FVector2D& Point, const FVector2D& A, const FVector2D& B);

	/** SAT overlap of two oriented boxes. Margin > 0 treats near-misses within Margin as overlapping. */
	static bool DoBoxesOverlap(const FVoidBox2D& A, const FVoidBox2D& B, double Margin = 0.0);

	/** Box corridor around segment AB with the given half width (extended by EndCap at both ends). */
	static FVoidBox2D MakeSegmentBox(const FVector2D& A, const FVector2D& B, double HalfWidth, double EndCap = 0.0);

	/** True if every corner of the box is inside the shape (polygon minus optional hole) AND no polygon edge crosses a box edge. */
	static bool IsBoxInsideShape(const FVoidBox2D& Box, const TArray<FVector2D>& Polygon, const FVector2D& HoleCenter, double HoleRadius);

	/** True if the box and the shape (polygon minus optional circular hole) share any area, expanded by Margin. */
	static bool DoesBoxOverlapShape(const FVoidBox2D& Box, const TArray<FVector2D>& Polygon, const FVector2D& HoleCenter, double HoleRadius, double Margin = 0.0);

	/** True if any segment of the polyline crosses / lies inside the polygon. */
	static bool DoesPolylineIntersectPolygon(const TArray<FVector2D>& Polyline, const TArray<FVector2D>& Polygon);

	/** Distance from Point to the closest edge of Polygon. */
	static double DistanceToPolygonEdge(const FVector2D& Point, const TArray<FVector2D>& Polygon);

	static bool DoPolygonsOverlap(const TArray<FVector2D>& A, const TArray<FVector2D>& B);

	/**
	 * 2D sightline test used by the (non-blocking) Spire sightline validation.
	 * Casts from (From, FromZ) toward (To, ToZ); returns false if the ray dips
	 * below the top of any box in Obstacles (box height taken from parallel array).
	 */
	static bool IsSightlineClear(const FVector2D& From, double FromZ, const FVector2D& To, double ToZ, const TArray<FVoidBox2D>& Obstacles, const TArray<double>& ObstacleTopZ);
};
