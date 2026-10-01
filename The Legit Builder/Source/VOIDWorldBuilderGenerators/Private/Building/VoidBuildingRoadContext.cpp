// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Building/VoidBuildingRoadContext.h"
#include "Building/VoidBuildingGeometry.h"

namespace
{
	using G = FVoidBuildingGeometry;

	/** Frontage preference: a building should face a street, not a motorway or a back alley. */
	float FrontagePenalty(EVoidRoadType Type)
	{
		switch (Type)
		{
		case EVoidRoadType::Highway: return 5000.0f;
		case EVoidRoadType::Alley:   return 600.0f;
		case EVoidRoadType::Service: return 400.0f;
		default:                     return 0.0f;
		}
	}

	float DoorWidthFor(EVoidBuildingCategory Category)
	{
		switch (Category)
		{
		case EVoidBuildingCategory::Civic:
		case EVoidBuildingCategory::Government:
		case EVoidBuildingCategory::Institutional:
		case EVoidBuildingCategory::Landmark:   return 400.0f;
		case EVoidBuildingCategory::Industrial: return 500.0f;
		case EVoidBuildingCategory::Residential:return 160.0f;
		default:                                return 240.0f;
		}
	}

	bool WantsCanopy(EVoidBuildingCategory Category)
	{
		switch (Category)
		{
		case EVoidBuildingCategory::Commercial:
		case EVoidBuildingCategory::Office:
		case EVoidBuildingCategory::Civic:
		case EVoidBuildingCategory::Institutional:
		case EVoidBuildingCategory::Medical:
		case EVoidBuildingCategory::Government:
		case EVoidBuildingCategory::MixedUse:
		case EVoidBuildingCategory::Landmark: return true;
		default:                              return false;
		}
	}

	void Translate(TArray<FVector2D>& Poly, const FVector2D& Delta)
	{
		for (FVector2D& V : Poly) { V.X += Delta.X; V.Y += Delta.Y; }
	}
}

float FVoidBuildingRoadContext::RequiredSetback(const FVoidNormalizedBuilding& Building, const FVoidBuildingGenerationParams& Params)
{
	return Params.MinSetbackUnits * (Building.bIsLandmark ? Params.LandmarkSetbackMultiplier : 1.0f);
}

void FVoidBuildingRoadContext::AddSegment(const FVector2D& A, const FVector2D& B, float Radius, const FVoidRoadSpec& Road, bool bIsland)
{
	FVoidRoadCorridorSegment Seg;
	Seg.A = A;
	Seg.B = B;
	Seg.CorridorRadius = Radius;
	Seg.RoadId = Road.Id.Value;
	Seg.RoadType = Road.RoadType;
	Seg.Elevation = Road.ElevationUnits;
	Seg.bIsIsland = bIsland;

	const int32 Index = Segments.Add(Seg);
	MaxCorridorRadius = FMath::Max(MaxCorridorRadius, Radius);

	// Register in every grid cell the centreline's bounding box touches.
	const int32 X0 = FMath::FloorToInt(FMath::Min(A.X, B.X) / GridCellSize);
	const int32 X1 = FMath::FloorToInt(FMath::Max(A.X, B.X) / GridCellSize);
	const int32 Y0 = FMath::FloorToInt(FMath::Min(A.Y, B.Y) / GridCellSize);
	const int32 Y1 = FMath::FloorToInt(FMath::Max(A.Y, B.Y) / GridCellSize);
	for (int32 X = X0; X <= X1; ++X)
	{
		for (int32 Y = Y0; Y <= Y1; ++Y)
		{
			Grid.FindOrAdd(FIntPoint(X, Y)).Add(Index);
		}
	}
}

void FVoidBuildingRoadContext::Build(const FVoidDistrictData& District, const FVoidBuildingGenerationParams& Params)
{
	Segments.Reset();
	Grid.Reset();
	MaxCorridorRadius = 0.0f;
	SkippedGradeSeparated = 0;

	for (const FVoidRoadSpec& Road : District.Roads)
	{
		if (Road.bIsBridge || Road.bIsTunnel)
		{
			++SkippedGradeSeparated;
			continue;
		}

		const FVoidRoadCorridorDims Dims = ResolveDims(Road, Params);
		const float Radius = Dims.Width * 0.5f + Dims.CurbWidth + Dims.SidewalkWidth;

		if (Road.RoadType == EVoidRoadType::Roundabout)
		{
			if (Road.CenterlinePoints.Num() == 0 || Road.RoundaboutRadiusUnits <= 0.0f) { continue; }
			const FVector2D C = Road.CenterlinePoints[0];
			const double R = Road.RoundaboutRadiusUnits;
			for (int32 I = 0; I < RoundaboutSegments; ++I)
			{
				const double A0 = (2.0 * PI * I) / RoundaboutSegments;
				const double A1 = (2.0 * PI * (I + 1)) / RoundaboutSegments;
				AddSegment(FVector2D(C.X + R * FMath::Cos(A0), C.Y + R * FMath::Sin(A0)),
				           FVector2D(C.X + R * FMath::Cos(A1), C.Y + R * FMath::Sin(A1)), Radius, Road, false);
			}
			// The island inside the ring is not buildable either.
			AddSegment(C, C, static_cast<float>(R) + Radius, Road, true);
			continue;
		}

		for (int32 I = 0; I + 1 < Road.CenterlinePoints.Num(); ++I)
		{
			AddSegment(Road.CenterlinePoints[I], Road.CenterlinePoints[I + 1], Radius, Road, false);
		}
	}
}

void FVoidBuildingRoadContext::QueryNear(const FVector2D& Min, const FVector2D& Max, float Expand, TArray<int32>& OutIndices) const
{
	OutIndices.Reset();
	const int32 X0 = FMath::FloorToInt((Min.X - Expand) / GridCellSize);
	const int32 X1 = FMath::FloorToInt((Max.X + Expand) / GridCellSize);
	const int32 Y0 = FMath::FloorToInt((Min.Y - Expand) / GridCellSize);
	const int32 Y1 = FMath::FloorToInt((Max.Y + Expand) / GridCellSize);
	for (int32 X = X0; X <= X1; ++X)
	{
		for (int32 Y = Y0; Y <= Y1; ++Y)
		{
			if (const TArray<int32>* Cell = Grid.Find(FIntPoint(X, Y)))
			{
				for (const int32 Index : *Cell) { OutIndices.AddUnique(Index); }
			}
		}
	}
	OutIndices.Sort(); // deterministic evaluation order
}

void FVoidBuildingRoadContext::ResolveBuilding(FVoidNormalizedBuilding& B, const FVoidBuildingGenerationParams& Params) const
{
	B.Frontage = FVoidBuildingFrontage();
	B.Entrances.Reset();
	B.bAdjustedForRoad = false;
	B.bRoadConflictUnresolved = false;
	B.AdjustmentOffset = FVector2D(0.0, 0.0);
	B.Footprint = B.SourceFootprint;

	const float Setback = RequiredSetback(B, Params);
	const float QueryExpand = FMath::Max(Params.MaxFrontageSearchUnits, 0.0f) + MaxCorridorRadius + Setback + Params.MaxAdjustmentUnits;

	TArray<int32> Nearby;

	// ---- 1. Clear the corridor + setback --------------------------------------------------
	if (Segments.Num() > 0 && Params.ConflictPolicy != EVoidBuildingConflictPolicy::GenerateAnyway)
	{
		const int32 MaxIterations = (Params.ConflictPolicy == EVoidBuildingConflictPolicy::AdjustThenSkip) ? 8 : 1;
		bool bViolation = false;

		for (int32 Iteration = 0; Iteration < MaxIterations; ++Iteration)
		{
			FVoidBuildingGeometry::GetBounds(B.Footprint, B.BoundsMin, B.BoundsMax);
			B.Centroid = FVoidBuildingGeometry::Centroid(B.Footprint);
			QueryNear(B.BoundsMin, B.BoundsMax, QueryExpand, Nearby);

			FVector2D Push(0.0, 0.0);
			bViolation = false;

			for (const int32 SegIndex : Nearby)
			{
				const FVoidRoadCorridorSegment& Seg = Segments[SegIndex];
				FVector2D PolyPt, SegPt;
				const double D = G::PolygonSegmentDistance(B.Footprint, Seg.A, Seg.B, &PolyPt, &SegPt);
				const double Need = Seg.CorridorRadius + Setback;
				if (D >= Need - 0.5) { continue; }

				bViolation = true;
				FVector2D Dir;
				if (D > 1e-3)
				{
					Dir = G::SafeNormal(PolyPt - SegPt);
				}
				else
				{
					// Overlapping: push the building's centre away from the road's nearest point.
					FVector2D Closest;
					G::PointSegmentDistSq(B.Centroid, Seg.A, Seg.B, &Closest);
					Dir = G::SafeNormal(B.Centroid - Closest, G::SafeNormal(B.Centroid - Seg.A));
				}
				const double Amount = Need - D + 1.0;
				Push.X += Dir.X * Amount;
				Push.Y += Dir.Y * Amount;
			}

			if (!bViolation) { break; }
			if (Params.ConflictPolicy != EVoidBuildingConflictPolicy::AdjustThenSkip) { break; }

			Translate(B.Footprint, Push);
			B.AdjustmentOffset += Push;
			B.bAdjustedForRoad = true;

			if (G::Length(B.AdjustmentOffset) > Params.MaxAdjustmentUnits)
			{
				bViolation = true;
				break;
			}
			bViolation = true; // re-verified on the next pass (or after the loop)
		}

		// Final verification against the (possibly moved) footprint.
		FVoidBuildingGeometry::GetBounds(B.Footprint, B.BoundsMin, B.BoundsMax);
		B.Centroid = FVoidBuildingGeometry::Centroid(B.Footprint);
		QueryNear(B.BoundsMin, B.BoundsMax, QueryExpand, Nearby);
		bool bStillViolating = false;
		for (const int32 SegIndex : Nearby)
		{
			const FVoidRoadCorridorSegment& Seg = Segments[SegIndex];
			if (G::PolygonSegmentDistance(B.Footprint, Seg.A, Seg.B) < Seg.CorridorRadius + Setback - 1.0)
			{
				bStillViolating = true;
				break;
			}
		}

		if (bStillViolating || G::Length(B.AdjustmentOffset) > Params.MaxAdjustmentUnits)
		{
			B.bRoadConflictUnresolved = true;
			// Restore authored data so reports show the original placement.
			B.Footprint = B.SourceFootprint;
			B.AdjustmentOffset = FVector2D(0.0, 0.0);
			B.bAdjustedForRoad = false;
		}
	}
	else if (Segments.Num() > 0)
	{
		// GenerateAnyway: still report a conflict so it is visible in the log.
		QueryNear(B.BoundsMin, B.BoundsMax, QueryExpand, Nearby);
		for (const int32 SegIndex : Nearby)
		{
			const FVoidRoadCorridorSegment& Seg = Segments[SegIndex];
			if (G::PolygonSegmentDistance(B.Footprint, Seg.A, Seg.B) < Seg.CorridorRadius + Setback - 1.0)
			{
				B.bRoadConflictUnresolved = true;
				break;
			}
		}
	}

	FVoidBuildingGeometry::GetBounds(B.Footprint, B.BoundsMin, B.BoundsMax);
	B.Centroid = FVoidBuildingGeometry::Centroid(B.Footprint);

	// ---- 2. Frontage ----------------------------------------------------------------------
	QueryNear(B.BoundsMin, B.BoundsMax, QueryExpand, Nearby);

	int32 BestSeg = INDEX_NONE;
	double BestScore = TNumericLimits<double>::Max();
	double BestClearance = 0.0;
	FVector2D BestPolyPt(0, 0), BestSegPt(0, 0);

	for (const int32 SegIndex : Nearby)
	{
		const FVoidRoadCorridorSegment& Seg = Segments[SegIndex];
		if (Seg.bIsIsland) { continue; }
		FVector2D PolyPt, SegPt;
		const double D = G::PolygonSegmentDistance(B.Footprint, Seg.A, Seg.B, &PolyPt, &SegPt);
		const double Clearance = FMath::Max(0.0, D - Seg.CorridorRadius);
		if (Clearance > Params.MaxFrontageSearchUnits) { continue; }
		const double Score = Clearance + FrontagePenalty(Seg.RoadType);
		if (Score < BestScore) // strict '<' + ascending segment order => deterministic ties
		{
			BestScore = Score;
			BestClearance = Clearance;
			BestSeg = SegIndex;
			BestPolyPt = PolyPt;
			BestSegPt = SegPt;
		}
	}

	const int32 N = B.Footprint.Num();

	if (BestSeg != INDEX_NONE)
	{
		const FVoidRoadCorridorSegment& Seg = Segments[BestSeg];
		B.Frontage.bHasRoadFrontage = true;
		B.Frontage.RoadId = Seg.RoadId;
		B.Frontage.ClearanceToCorridor = static_cast<float>(BestClearance);
		B.Frontage.BaseElevation = Seg.Elevation;

		// Front edge: closest to the road contact point among edges that face it.
		int32 FrontEdge = INDEX_NONE;
		double BestEdgeD = TNumericLimits<double>::Max();
		double BestEdgeLen = 0.0;
		for (int32 Pass = 0; Pass < 2 && FrontEdge == INDEX_NONE; ++Pass)
		{
			for (int32 I = 0; I < N; ++I)
			{
				const FVector2D& P = B.Footprint[I];
				const FVector2D& Q = B.Footprint[(I + 1) % N];
				const FVector2D E = Q - P;
				const double Len = G::Length(E);
				if (Len < 1e-6) { continue; }
				const FVector2D Outward(E.Y / Len, -E.X / Len);
				const FVector2D Mid((P.X + Q.X) * 0.5, (P.Y + Q.Y) * 0.5);
				if (Pass == 0 && G::Dot(Outward, BestSegPt - Mid) <= 0.0) { continue; } // must face the road (pass 0)
				const double D2 = G::PointSegmentDistSq(BestSegPt, P, Q);
				if (D2 < BestEdgeD - 1e-6 || (FMath::Abs(D2 - BestEdgeD) <= 1e-6 && Len > BestEdgeLen))
				{
					BestEdgeD = D2;
					BestEdgeLen = Len;
					FrontEdge = I;
				}
			}
		}
		B.Frontage.FrontEdgeIndex = FrontEdge;
	}
	else
	{
		// No road in range: address the longest wall so a door still exists, but flag it.
		B.Frontage.FrontEdgeIndex = G::LongestEdgeIndex(B.Footprint);
	}

	if (B.Frontage.FrontEdgeIndex != INDEX_NONE)
	{
		const int32 I = B.Frontage.FrontEdgeIndex;
		const FVector2D E = G::SafeNormal(B.Footprint[(I + 1) % N] - B.Footprint[I]);
		B.Frontage.FrontNormal = FVector2D(E.Y, -E.X);
	}

	if (BestSeg != INDEX_NONE && Params.bAlignBaseToFrontageRoad)
	{
		B.BaseZ = B.Frontage.BaseElevation;
	}
	else
	{
		B.BaseZ = 0.0f;
	}

	if (Params.bGenerateEntrances)
	{
		PlaceEntrances(B, Params);
	}

	if (B.Frontage.bHasRoadFrontage && B.Entrances.Num() > 0)
	{
		const FVoidRoadCorridorSegment& Seg = Segments[BestSeg];
		FVector2D RoadPt;
		G::PointSegmentDistSq(B.Entrances[0].Location, Seg.A, Seg.B, &RoadPt);
		const FVector2D Dir = G::SafeNormal(B.Entrances[0].Location - RoadPt, B.Frontage.FrontNormal);
		B.Frontage.AccessPoint = FVector2D(RoadPt.X + Dir.X * Seg.CorridorRadius, RoadPt.Y + Dir.Y * Seg.CorridorRadius);
	}
}

void FVoidBuildingRoadContext::PlaceEntrances(FVoidNormalizedBuilding& B, const FVoidBuildingGenerationParams& Params) const
{
	B.Entrances.Reset();
	const int32 N = B.Footprint.Num();
	const int32 EdgeIndex = B.Frontage.FrontEdgeIndex;
	if (EdgeIndex == INDEX_NONE || N < 3) { return; }

	const FVector2D P = B.Footprint[EdgeIndex];
	const FVector2D Q = B.Footprint[(EdgeIndex + 1) % N];
	const double Len = G::Length(Q - P);
	const FVector2D Dir = G::SafeNormal(Q - P);
	const FVector2D Outward(Dir.Y, -Dir.X);

	const float Width = DoorWidthFor(B.Category);
	const double Margin = 100.0;
	if (Len < Width + 2.0 * Margin) { return; } // wall too short for a door

	int32 Count = 1;
	if (B.Category != EVoidBuildingCategory::Residential)
	{
		Count = FMath::Clamp(FMath::FloorToInt(static_cast<float>(Len) / 2500.0f), 1, 3);
	}

	FVoidBuildingRng Rng(static_cast<uint32>(B.Seed) ^ 0x9E3779B9u);
	const double Lo = Width * 0.5 + Margin;
	const double Hi = Len - Width * 0.5 - Margin;

	for (int32 K = 0; K < Count; ++K)
	{
		double U = Len * (K + 1) / (Count + 1);
		U += Rng.Range(-0.06, 0.06) * Len;
		U = FMath::Clamp(U, Lo, Hi);

		FVoidBuildingEntrance Door;
		Door.EdgeIndex = EdgeIndex;
		Door.Location = FVector2D(P.X + Dir.X * U, P.Y + Dir.Y * U);
		Door.OutwardNormal = Outward;
		Door.Width = Width;
		Door.Height = FMath::Min(B.FloorHeight * 0.85f, B.Category == EVoidBuildingCategory::Industrial ? 450.0f : 300.0f);
		Door.bCanopy = WantsCanopy(B.Category) && K == 0;
		B.Entrances.Add(Door);
	}
}
