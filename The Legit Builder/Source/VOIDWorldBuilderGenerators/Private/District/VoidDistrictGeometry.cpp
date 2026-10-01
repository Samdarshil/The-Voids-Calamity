// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "District/VoidDistrictGeometry.h"

namespace VoidDistrictGeometryPrivate
{
	static double Dot(const FVector2D& A, const FVector2D& B)
	{
		return A.X * B.X + A.Y * B.Y;
	}

	static double Cross(const FVector2D& A, const FVector2D& B)
	{
		return A.X * B.Y - A.Y * B.X;
	}

	static FVector2D Sub(const FVector2D& A, const FVector2D& B)
	{
		return FVector2D(A.X - B.X, A.Y - B.Y);
	}

	static double DistSq(const FVector2D& A, const FVector2D& B)
	{
		const double DX = A.X - B.X;
		const double DY = A.Y - B.Y;
		return DX * DX + DY * DY;
	}

	/** Distance from Point to the (filled) box; 0 if inside. */
	static double DistancePointToBox(const FVector2D& Point, const FVoidBox2D& Box)
	{
		const FVector2D Rel = Sub(Point, Box.Center);
		const double LocalX = Dot(Rel, Box.AxisX());
		const double LocalY = Dot(Rel, Box.AxisY());
		const double DX = FMath::Max(FMath::Abs(LocalX) - Box.HalfExtent.X, 0.0);
		const double DY = FMath::Max(FMath::Abs(LocalY) - Box.HalfExtent.Y, 0.0);
		return FMath::Sqrt(DX * DX + DY * DY);
	}

	static bool IsPointInBox(const FVector2D& Point, const FVoidBox2D& Box)
	{
		const FVector2D Rel = Sub(Point, Box.Center);
		return FMath::Abs(Dot(Rel, Box.AxisX())) <= Box.HalfExtent.X && FMath::Abs(Dot(Rel, Box.AxisY())) <= Box.HalfExtent.Y;
	}

	static bool EdgesCross(const TArray<FVector2D>& CornersA, const TArray<FVector2D>& Polygon)
	{
		const int32 NumA = CornersA.Num();
		const int32 NumB = Polygon.Num();
		for (int32 I = 0; I < NumA; ++I)
		{
			const FVector2D& A0 = CornersA[I];
			const FVector2D& A1 = CornersA[(I + 1) % NumA];
			for (int32 J = 0; J < NumB; ++J)
			{
				if (FVoidDistrictGeometry::SegmentsIntersect(A0, A1, Polygon[J], Polygon[(J + 1) % NumB]))
				{
					return true;
				}
			}
		}
		return false;
	}
}

// --- FVoidBox2D -----------------------------------------------------------

FVector2D FVoidBox2D::AxisX() const
{
	return FVector2D(FMath::Cos(YawRadians), FMath::Sin(YawRadians));
}

FVector2D FVoidBox2D::AxisY() const
{
	return FVector2D(-FMath::Sin(YawRadians), FMath::Cos(YawRadians));
}

void FVoidBox2D::GetCorners(TArray<FVector2D>& OutCorners) const
{
	OutCorners.Reset();
	const FVector2D AX = AxisX();
	const FVector2D AY = AxisY();
	const double SX[4] = { -1.0, 1.0, 1.0, -1.0 };
	const double SY[4] = { -1.0, -1.0, 1.0, 1.0 };
	for (int32 Index = 0; Index < 4; ++Index)
	{
		OutCorners.Add(FVector2D(
			Center.X + AX.X * HalfExtent.X * SX[Index] + AY.X * HalfExtent.Y * SY[Index],
			Center.Y + AX.Y * HalfExtent.X * SX[Index] + AY.Y * HalfExtent.Y * SY[Index]));
	}
}

TArray<FVector2D> FVoidBox2D::GetCorners() const
{
	TArray<FVector2D> Corners;
	GetCorners(Corners);
	return Corners;
}

FVoidBox2D FVoidBox2D::Inflated(double Margin) const
{
	FVoidBox2D Result = *this;
	Result.HalfExtent = FVector2D(HalfExtent.X + Margin, HalfExtent.Y + Margin);
	return Result;
}

// --- RNG ------------------------------------------------------------------

FVoidDistrictRng::FVoidDistrictRng(uint32 Seed)
{
	State = (Seed == 0u) ? 2463534242u : Seed;
	// Warm up so nearby seeds diverge.
	for (int32 Index = 0; Index < 4; ++Index)
	{
		NextUInt();
	}
}

uint32 FVoidDistrictRng::NextUInt()
{
	uint32 X = State;
	X ^= X << 13;
	X ^= X >> 17;
	X ^= X << 5;
	State = X;
	return X;
}

double FVoidDistrictRng::NextUnit()
{
	return static_cast<double>(NextUInt()) / 4294967296.0;
}

int32 FVoidDistrictRng::NextInt(int32 MinInclusive, int32 MaxInclusive)
{
	if (MaxInclusive <= MinInclusive)
	{
		return MinInclusive;
	}
	const uint32 Span = static_cast<uint32>(MaxInclusive - MinInclusive + 1);
	return MinInclusive + static_cast<int32>(NextUInt() % Span);
}

// --- FVoidDistrictGeometry -----------------------------------------------

uint32 FVoidDistrictGeometry::StableHash(const FString& Text)
{
	uint32 Hash = 2166136261u;
	for (int32 Index = 0; Index < Text.Len(); ++Index)
	{
		Hash ^= static_cast<uint32>(Text[Index]);
		Hash *= 16777619u;
	}
	return Hash;
}

FVector2D FVoidDistrictGeometry::PolarPoint(const FVector2D& Center, double Radius, double AngleRadians)
{
	return FVector2D(Center.X + Radius * FMath::Cos(AngleRadians), Center.Y + Radius * FMath::Sin(AngleRadians));
}

TArray<FVector2D> FVoidDistrictGeometry::MakeCirclePolygon(const FVector2D& Center, double Radius, int32 Segments)
{
	TArray<FVector2D> Polygon;
	const int32 Count = FMath::Max(Segments, 8);
	Polygon.Reserve(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const double Angle = (2.0 * PI * static_cast<double>(Index)) / static_cast<double>(Count);
		Polygon.Add(PolarPoint(Center, Radius, Angle));
	}
	return Polygon;
}

double FVoidDistrictGeometry::PolygonAreaSigned(const TArray<FVector2D>& Polygon)
{
	double Sum = 0.0;
	const int32 Num = Polygon.Num();
	for (int32 Index = 0; Index < Num; ++Index)
	{
		const FVector2D& A = Polygon[Index];
		const FVector2D& B = Polygon[(Index + 1) % Num];
		Sum += A.X * B.Y - B.X * A.Y;
	}
	return 0.5 * Sum;
}

double FVoidDistrictGeometry::PolygonArea(const TArray<FVector2D>& Polygon)
{
	return FMath::Abs(PolygonAreaSigned(Polygon));
}

FVector2D FVoidDistrictGeometry::PolygonCentroid(const TArray<FVector2D>& Polygon)
{
	const int32 Num = Polygon.Num();
	if (Num == 0)
	{
		return FVector2D::ZeroVector;
	}

	const double Area2 = 2.0 * PolygonAreaSigned(Polygon);
	if (FMath::Abs(Area2) < 1e-9)
	{
		FVector2D Mean = FVector2D::ZeroVector;
		for (const FVector2D& Point : Polygon)
		{
			Mean.X += Point.X;
			Mean.Y += Point.Y;
		}
		return FVector2D(Mean.X / Num, Mean.Y / Num);
	}

	double CX = 0.0;
	double CY = 0.0;
	for (int32 Index = 0; Index < Num; ++Index)
	{
		const FVector2D& A = Polygon[Index];
		const FVector2D& B = Polygon[(Index + 1) % Num];
		const double Cross = A.X * B.Y - B.X * A.Y;
		CX += (A.X + B.X) * Cross;
		CY += (A.Y + B.Y) * Cross;
	}
	return FVector2D(CX / (3.0 * Area2), CY / (3.0 * Area2));
}

void FVoidDistrictGeometry::EnsureCounterClockwise(TArray<FVector2D>& Polygon)
{
	if (PolygonAreaSigned(Polygon) < 0.0)
	{
		const int32 Num = Polygon.Num();
		for (int32 Index = 0; Index < Num / 2; ++Index)
		{
			const FVector2D Temp = Polygon[Index];
			Polygon[Index] = Polygon[Num - 1 - Index];
			Polygon[Num - 1 - Index] = Temp;
		}
	}
}

bool FVoidDistrictGeometry::IsPointInPolygon(const FVector2D& Point, const TArray<FVector2D>& Polygon)
{
	bool bInside = false;
	const int32 Num = Polygon.Num();
	for (int32 I = 0, J = Num - 1; I < Num; J = I++)
	{
		const FVector2D& Pi = Polygon[I];
		const FVector2D& Pj = Polygon[J];
		const bool bCrosses = ((Pi.Y > Point.Y) != (Pj.Y > Point.Y))
			&& (Point.X < (Pj.X - Pi.X) * (Point.Y - Pi.Y) / (Pj.Y - Pi.Y) + Pi.X);
		if (bCrosses)
		{
			bInside = !bInside;
		}
	}
	return bInside;
}

bool FVoidDistrictGeometry::IsPointInShape(const FVector2D& Point, const TArray<FVector2D>& Polygon, const FVector2D& HoleCenter, double HoleRadius)
{
	if (!IsPointInPolygon(Point, Polygon))
	{
		return false;
	}
	if (HoleRadius > 0.0 && VoidDistrictGeometryPrivate::DistSq(Point, HoleCenter) < HoleRadius * HoleRadius)
	{
		return false;
	}
	return true;
}

bool FVoidDistrictGeometry::SegmentsIntersect(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D)
{
	using namespace VoidDistrictGeometryPrivate;

	const FVector2D R = Sub(B, A);
	const FVector2D S = Sub(D, C);
	const double Denominator = Cross(R, S);
	const FVector2D CA = Sub(C, A);

	if (FMath::Abs(Denominator) < 1e-12)
	{
		// Parallel. Treat collinear overlap as intersecting.
		if (FMath::Abs(Cross(CA, R)) > 1e-9)
		{
			return false;
		}
		const double RR = Dot(R, R);
		if (RR < 1e-12)
		{
			return false;
		}
		const double T0 = Dot(CA, R) / RR;
		const double T1 = T0 + Dot(S, R) / RR;
		const double Lo = FMath::Min(T0, T1);
		const double Hi = FMath::Max(T0, T1);
		return Hi >= 0.0 && Lo <= 1.0;
	}

	const double T = Cross(CA, S) / Denominator;
	const double U = Cross(CA, R) / Denominator;
	return T >= 0.0 && T <= 1.0 && U >= 0.0 && U <= 1.0;
}

double FVoidDistrictGeometry::DistancePointToSegment(const FVector2D& Point, const FVector2D& A, const FVector2D& B)
{
	using namespace VoidDistrictGeometryPrivate;

	const FVector2D AB = Sub(B, A);
	const double LengthSq = Dot(AB, AB);
	if (LengthSq < 1e-12)
	{
		return FMath::Sqrt(DistSq(Point, A));
	}
	const double T = FMath::Clamp(Dot(Sub(Point, A), AB) / LengthSq, 0.0, 1.0);
	const FVector2D Closest(A.X + AB.X * T, A.Y + AB.Y * T);
	return FMath::Sqrt(DistSq(Point, Closest));
}

bool FVoidDistrictGeometry::DoBoxesOverlap(const FVoidBox2D& A, const FVoidBox2D& B, double Margin)
{
	using namespace VoidDistrictGeometryPrivate;

	const TArray<FVector2D> CornersA = A.GetCorners();
	const TArray<FVector2D> CornersB = B.GetCorners();
	const FVector2D Axes[4] = { A.AxisX(), A.AxisY(), B.AxisX(), B.AxisY() };

	for (const FVector2D& Axis : Axes)
	{
		double MinA = 1e300, MaxA = -1e300, MinB = 1e300, MaxB = -1e300;
		for (const FVector2D& Corner : CornersA)
		{
			const double P = Dot(Corner, Axis);
			MinA = FMath::Min(MinA, P);
			MaxA = FMath::Max(MaxA, P);
		}
		for (const FVector2D& Corner : CornersB)
		{
			const double P = Dot(Corner, Axis);
			MinB = FMath::Min(MinB, P);
			MaxB = FMath::Max(MaxB, P);
		}
		if (MaxA + Margin < MinB || MaxB + Margin < MinA)
		{
			return false;
		}
	}
	return true;
}

FVoidBox2D FVoidDistrictGeometry::MakeSegmentBox(const FVector2D& A, const FVector2D& B, double HalfWidth, double EndCap)
{
	FVoidBox2D Box;
	Box.Center = FVector2D((A.X + B.X) * 0.5, (A.Y + B.Y) * 0.5);
	const double DX = B.X - A.X;
	const double DY = B.Y - A.Y;
	const double Length = FMath::Sqrt(DX * DX + DY * DY);
	Box.YawRadians = (Length > 1e-9) ? FMath::Atan2(DY, DX) : 0.0;
	Box.HalfExtent = FVector2D(Length * 0.5 + EndCap, HalfWidth);
	return Box;
}

bool FVoidDistrictGeometry::IsBoxInsideShape(const FVoidBox2D& Box, const TArray<FVector2D>& Polygon, const FVector2D& HoleCenter, double HoleRadius)
{
	using namespace VoidDistrictGeometryPrivate;

	const TArray<FVector2D> Corners = Box.GetCorners();
	for (const FVector2D& Corner : Corners)
	{
		if (!IsPointInPolygon(Corner, Polygon))
		{
			return false;
		}
	}

	if (EdgesCross(Corners, Polygon))
	{
		return false;
	}

	if (HoleRadius > 0.0 && DistancePointToBox(HoleCenter, Box) < HoleRadius)
	{
		return false;
	}

	return true;
}

bool FVoidDistrictGeometry::DoesBoxOverlapShape(const FVoidBox2D& Box, const TArray<FVector2D>& Polygon, const FVector2D& HoleCenter, double HoleRadius, double Margin)
{
	using namespace VoidDistrictGeometryPrivate;

	if (Polygon.Num() < 3)
	{
		return false;
	}

	const FVoidBox2D Inflated = Margin > 0.0 ? Box.Inflated(Margin) : Box;
	const TArray<FVector2D> Corners = Inflated.GetCorners();

	bool bOverlap = false;

	for (const FVector2D& Corner : Corners)
	{
		if (IsPointInPolygon(Corner, Polygon))
		{
			bOverlap = true;
			break;
		}
	}

	if (!bOverlap)
	{
		for (const FVector2D& Vertex : Polygon)
		{
			if (IsPointInBox(Vertex, Inflated))
			{
				bOverlap = true;
				break;
			}
		}
	}

	if (!bOverlap && EdgesCross(Corners, Polygon))
	{
		bOverlap = true;
	}

	if (bOverlap && HoleRadius > 0.0)
	{
		// The shape excludes its hole: if the (inflated) box lies entirely inside the hole there is no shared area.
		bool bAllCornersInHole = true;
		for (const FVector2D& Corner : Corners)
		{
			if (DistSq(Corner, HoleCenter) >= HoleRadius * HoleRadius)
			{
				bAllCornersInHole = false;
				break;
			}
		}
		if (bAllCornersInHole)
		{
			return false;
		}
	}

	return bOverlap;
}

bool FVoidDistrictGeometry::DoesPolylineIntersectPolygon(const TArray<FVector2D>& Polyline, const TArray<FVector2D>& Polygon)
{
	if (Polygon.Num() < 3)
	{
		return false;
	}

	for (const FVector2D& Point : Polyline)
	{
		if (IsPointInPolygon(Point, Polygon))
		{
			return true;
		}
	}

	for (int32 Index = 0; Index + 1 < Polyline.Num(); ++Index)
	{
		for (int32 EdgeIndex = 0; EdgeIndex < Polygon.Num(); ++EdgeIndex)
		{
			if (SegmentsIntersect(Polyline[Index], Polyline[Index + 1], Polygon[EdgeIndex], Polygon[(EdgeIndex + 1) % Polygon.Num()]))
			{
				return true;
			}
		}
	}
	return false;
}

double FVoidDistrictGeometry::DistanceToPolygonEdge(const FVector2D& Point, const TArray<FVector2D>& Polygon)
{
	double Best = 1e300;
	const int32 Num = Polygon.Num();
	for (int32 Index = 0; Index < Num; ++Index)
	{
		Best = FMath::Min(Best, DistancePointToSegment(Point, Polygon[Index], Polygon[(Index + 1) % Num]));
	}
	return Best;
}

bool FVoidDistrictGeometry::DoPolygonsOverlap(const TArray<FVector2D>& A, const TArray<FVector2D>& B)
{
	if (A.Num() < 3 || B.Num() < 3)
	{
		return false;
	}
	for (const FVector2D& Point : A)
	{
		if (IsPointInPolygon(Point, B))
		{
			return true;
		}
	}
	for (const FVector2D& Point : B)
	{
		if (IsPointInPolygon(Point, A))
		{
			return true;
		}
	}
	for (int32 I = 0; I < A.Num(); ++I)
	{
		for (int32 J = 0; J < B.Num(); ++J)
		{
			if (SegmentsIntersect(A[I], A[(I + 1) % A.Num()], B[J], B[(J + 1) % B.Num()]))
			{
				return true;
			}
		}
	}
	return false;
}

bool FVoidDistrictGeometry::IsSightlineClear(const FVector2D& From, double FromZ, const FVector2D& To, double ToZ, const TArray<FVoidBox2D>& Obstacles, const TArray<double>& ObstacleTopZ)
{
	using namespace VoidDistrictGeometryPrivate;

	const FVector2D Direction = Sub(To, From);

	for (int32 Index = 0; Index < Obstacles.Num() && Index < ObstacleTopZ.Num(); ++Index)
	{
		const FVoidBox2D& Box = Obstacles[Index];
		const FVector2D AX = Box.AxisX();
		const FVector2D AY = Box.AxisY();

		// Ray in box-local space.
		const FVector2D RelStart = Sub(From, Box.Center);
		const double StartX = Dot(RelStart, AX);
		const double StartY = Dot(RelStart, AY);
		const double DirX = Dot(Direction, AX);
		const double DirY = Dot(Direction, AY);

		double T0 = 0.0;
		double T1 = 1.0;
		bool bHit = true;

		const double Starts[2] = { StartX, StartY };
		const double Dirs[2] = { DirX, DirY };
		const double Halves[2] = { Box.HalfExtent.X, Box.HalfExtent.Y };

		for (int32 Axis = 0; Axis < 2 && bHit; ++Axis)
		{
			if (FMath::Abs(Dirs[Axis]) < 1e-12)
			{
				if (FMath::Abs(Starts[Axis]) > Halves[Axis])
				{
					bHit = false;
				}
			}
			else
			{
				double TA = (-Halves[Axis] - Starts[Axis]) / Dirs[Axis];
				double TB = (Halves[Axis] - Starts[Axis]) / Dirs[Axis];
				if (TA > TB)
				{
					const double Swap = TA; TA = TB; TB = Swap;
				}
				T0 = FMath::Max(T0, TA);
				T1 = FMath::Min(T1, TB);
				if (T0 > T1)
				{
					bHit = false;
				}
			}
		}

		if (!bHit)
		{
			continue;
		}

		// Ray height is linear in T, so its minimum over [T0, T1] is at an endpoint.
		const double ZAtT0 = FromZ + (ToZ - FromZ) * T0;
		const double ZAtT1 = FromZ + (ToZ - FromZ) * T1;
		if (FMath::Min(ZAtT0, ZAtT1) < ObstacleTopZ[Index])
		{
			return false;
		}
	}

	return true;
}
