// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Data/VoidRoadOutput.h"

namespace VoidRoadOutputPrivate
{
	/** Segment i runs Points[i] -> Points[(i+1)%N]; closed loops have N segments, open N-1. */
	static int32 NumSegments(const TArray<FVector>& Points, bool bClosed)
	{
		return Points.Num() < 2 ? 0 : (bClosed ? Points.Num() : Points.Num() - 1);
	}

	static double SegmentLength(const TArray<FVector>& Points, int32 Index)
	{
		return FVector::Dist(Points[Index], Points[(Index + 1) % Points.Num()]);
	}
}

double FVoidRoadRecord::GetLength() const
{
	using namespace VoidRoadOutputPrivate;
	double Total = 0.0;
	for (int32 i = 0; i < NumSegments(Centerline, bClosedLoop); ++i)
	{
		Total += SegmentLength(Centerline, i);
	}
	return Total;
}

void FVoidRoadNetworkOutput::Reset()
{
	Roads.Reset();
	Intersections.Reset();
	RoadIndexById.Reset();
}

void FVoidRoadNetworkOutput::AddRoad(FVoidRoadRecord Record)
{
	if (const int32* Existing = RoadIndexById.Find(Record.RoadId))
	{
		Roads[*Existing] = MoveTemp(Record); // Same id re-registered (regeneration): replace, never duplicate.
		return;
	}
	RoadIndexById.Add(Record.RoadId, Roads.Num());
	Roads.Add(MoveTemp(Record));
}

void FVoidRoadNetworkOutput::AddIntersection(FVoidIntersectionRecord Record)
{
	Intersections.Add(MoveTemp(Record));
}

void FVoidRoadNetworkOutput::RebuildConnectivity()
{
	for (FVoidRoadRecord& Road : Roads)
	{
		Road.ConnectedRoadIds.Reset();
	}
	for (const FVoidIntersectionRecord& Intersection : Intersections)
	{
		for (const FName RoadId : Intersection.RoadIds)
		{
			if (FVoidRoadRecord* Road = FindRoadMutable(RoadId))
			{
				for (const FName Other : Intersection.RoadIds)
				{
					if (Other != RoadId)
					{
						Road->ConnectedRoadIds.AddUnique(Other);
					}
				}
			}
		}
	}
}

FVoidRoadRecord* FVoidRoadNetworkOutput::FindRoadMutable(FName RoadId)
{
	const int32* Index = RoadIndexById.Find(RoadId);
	return Index ? &Roads[*Index] : nullptr;
}

const FVoidRoadRecord* FVoidRoadNetworkOutput::FindRoad(FName RoadId) const
{
	const int32* Index = RoadIndexById.Find(RoadId);
	return Index ? &Roads[*Index] : nullptr;
}

TArray<const FVoidRoadRecord*> FVoidRoadNetworkOutput::GetRoadsByType(EVoidRoadType Type) const
{
	TArray<const FVoidRoadRecord*> Out;
	for (const FVoidRoadRecord& Road : Roads) { if (Road.RoadType == Type) { Out.Add(&Road); } }
	return Out;
}

TArray<const FVoidRoadRecord*> FVoidRoadNetworkOutput::GetRoadsByTier(int32 Tier) const
{
	TArray<const FVoidRoadRecord*> Out;
	for (const FVoidRoadRecord& Road : Roads) { if (Road.HierarchyTier == Tier) { Out.Add(&Road); } }
	return Out;
}

TArray<const FVoidRoadRecord*> FVoidRoadNetworkOutput::GetRoadsServingDistrict(FName DistrictId) const
{
	TArray<const FVoidRoadRecord*> Out;
	for (const FVoidRoadRecord& Road : Roads) { if (Road.ServedDistrictIds.Contains(DistrictId)) { Out.Add(&Road); } }
	return Out;
}

bool FVoidRoadNetworkOutput::SampleRoad(FName RoadId, double DistanceAlong, FVoidRoadSample& OutSample) const
{
	using namespace VoidRoadOutputPrivate;

	const FVoidRoadRecord* Road = FindRoad(RoadId);
	if (!Road || !Road->bHasGeometry || !FMath::IsFinite(DistanceAlong))
	{
		return false;
	}

	const TArray<FVector>& P = Road->Centerline;
	const int32 NumSeg = NumSegments(P, Road->bClosedLoop);
	if (NumSeg == 0)
	{
		return false;
	}

	const double Total = Road->GetLength();
	if (Total <= UE_DOUBLE_SMALL_NUMBER)
	{
		return false;
	}

	double D = Road->bClosedLoop ? FMath::Fmod(DistanceAlong, Total) : FMath::Clamp(DistanceAlong, 0.0, Total);
	if (D < 0.0) { D += Total; }

	double Accum = 0.0;
	for (int32 i = 0; i < NumSeg; ++i)
	{
		const double Len = SegmentLength(P, i);
		if (D <= Accum + Len || i == NumSeg - 1)
		{
			const FVector& A = P[i];
			const FVector& B = P[(i + 1) % P.Num()];
			const double T = Len > UE_DOUBLE_SMALL_NUMBER ? FMath::Clamp((D - Accum) / Len, 0.0, 1.0) : 0.0;
			OutSample.RoadId = RoadId;
			OutSample.Position = FMath::Lerp(A, B, T);
			OutSample.Direction = (B - A).GetSafeNormal();
			if (OutSample.Direction.IsNearlyZero()) { OutSample.Direction = FVector::ForwardVector; }
			OutSample.DistanceAlong = D;
			return true;
		}
		Accum += Len;
	}
	return false;
}

bool FVoidRoadNetworkOutput::FindNearestRoadPoint(const FVector& Query, FVoidRoadSample& OutSample, double& OutDistance2D) const
{
	using namespace VoidRoadOutputPrivate;

	bool bFound = false;
	double BestDistSq = TNumericLimits<double>::Max();

	for (const FVoidRoadRecord& Road : Roads)
	{
		if (!Road.bHasGeometry) { continue; }
		const TArray<FVector>& P = Road.Centerline;
		const int32 NumSeg = NumSegments(P, Road.bClosedLoop);
		double Accum = 0.0;

		for (int32 i = 0; i < NumSeg; ++i)
		{
			const FVector& A = P[i];
			const FVector& B = P[(i + 1) % P.Num()];
			const FVector2D A2(A.X, A.Y), B2(B.X, B.Y), Q2(Query.X, Query.Y);
			const FVector2D AB = B2 - A2;
			const double AbSq = AB.SizeSquared();
			const double T = AbSq > UE_DOUBLE_SMALL_NUMBER ? FMath::Clamp(FVector2D::DotProduct(Q2 - A2, AB) / AbSq, 0.0, 1.0) : 0.0;
			const FVector2D C2 = A2 + AB * T;
			const double DistSq = FVector2D::DistSquared(Q2, C2);

			const double Len = FVector::Dist(A, B);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				bFound = true;
				OutSample.RoadId = Road.RoadId;
				OutSample.Position = FMath::Lerp(A, B, T);
				OutSample.Direction = (B - A).GetSafeNormal();
				if (OutSample.Direction.IsNearlyZero()) { OutSample.Direction = FVector::ForwardVector; }
				OutSample.DistanceAlong = Accum + Len * T;
			}
			Accum += Len;
		}
	}

	if (bFound) { OutDistance2D = FMath::Sqrt(BestDistSq); }
	return bFound;
}

TArray<FVector> FVoidRoadNetworkOutput::OffsetPolyline(const TArray<FVector>& Points, bool bClosed, double SignedOffsetLeft)
{
	TArray<FVector> Out;
	const int32 N = Points.Num();
	if (N < 2) { return Out; }
	Out.SetNum(N);

	for (int32 i = 0; i < N; ++i)
	{
		FVector2D Dir;
		if (bClosed)
		{
			const FVector& Prev = Points[(i - 1 + N) % N];
			const FVector& Next = Points[(i + 1) % N];
			Dir = FVector2D(Next.X - Prev.X, Next.Y - Prev.Y);
		}
		else if (i == 0)      { Dir = FVector2D(Points[1].X - Points[0].X, Points[1].Y - Points[0].Y); }
		else if (i == N - 1)  { Dir = FVector2D(Points[i].X - Points[i - 1].X, Points[i].Y - Points[i - 1].Y); }
		else                  { Dir = FVector2D(Points[i + 1].X - Points[i - 1].X, Points[i + 1].Y - Points[i - 1].Y); }

		Dir = Dir.IsNearlyZero() ? FVector2D(1.0, 0.0) : Dir.GetSafeNormal();

		// Unreal is left-handed: heading +X, Right is +Y and Left is -Y. With Dir=(Dx,Dy), Left=(Dy,-Dx)
		// (heading +X -> (0,-1) = -Y). This matches the mesh builder's perpendicular.
		const FVector2D Left(Dir.Y, -Dir.X);
		Out[i] = FVector(Points[i].X + Left.X * SignedOffsetLeft, Points[i].Y + Left.Y * SignedOffsetLeft, Points[i].Z);
	}
	return Out;
}

bool FVoidRoadNetworkOutput::GetSidewalkBoundary(FName RoadId, EVoidRoadSide Side, TArray<FVector>& OutInnerEdge, TArray<FVector>& OutOuterEdge) const
{
	const FVoidRoadRecord* Road = FindRoad(RoadId);
	if (!Road || !Road->bHasGeometry || !Road->bHasSidewalk || Road->SidewalkOuterOffsetUU <= Road->SidewalkInnerOffsetUU)
	{
		return false;
	}

	const double Sign = (Side == EVoidRoadSide::Left) ? 1.0 : -1.0;
	OutInnerEdge = OffsetPolyline(Road->Centerline, Road->bClosedLoop, Sign * Road->SidewalkInnerOffsetUU);
	OutOuterEdge = OffsetPolyline(Road->Centerline, Road->bClosedLoop, Sign * Road->SidewalkOuterOffsetUU);
	return OutInnerEdge.Num() >= 2;
}

TArray<const FVoidIntersectionRecord*> FVoidRoadNetworkOutput::GetIntersectionsForRoad(FName RoadId) const
{
	TArray<const FVoidIntersectionRecord*> Out;
	for (const FVoidIntersectionRecord& I : Intersections) { if (I.RoadIds.Contains(RoadId)) { Out.Add(&I); } }
	return Out;
}

const FVoidIntersectionRecord* FVoidRoadNetworkOutput::FindNearestIntersection(const FVector& Query, double MaxDistance2D) const
{
	const FVoidIntersectionRecord* Best = nullptr;
	double BestSq = FMath::Square(MaxDistance2D);
	for (const FVoidIntersectionRecord& I : Intersections)
	{
		const double DSq = FVector2D::DistSquared(FVector2D(I.Location.X, I.Location.Y), FVector2D(Query.X, Query.Y));
		if (DSq <= BestSq) { BestSq = DSq; Best = &I; }
	}
	return Best;
}
