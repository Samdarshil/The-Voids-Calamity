// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "District/VoidDistrictValidator.h"
#include "District/VoidDistrictGeometry.h"

namespace VoidValidatorPrivate
{
	constexpr double LayerTolerance = 500.0;
	constexpr double RoadEdgeUnits = 175.0;

	static FString N(FName Name) { return Name.ToString(); }

	static FVoidBox2D BoxOf(const FVoidPlannedBuilding& Building)
	{
		FVoidBox2D Box;
		Box.Center = Building.Center;
		Box.HalfExtent = Building.HalfExtent;
		Box.YawRadians = Building.YawRadians;
		return Box;
	}

	static bool IsSurface(double Z) { return Z > -LayerTolerance; }
	static bool SameLayer(double A, double B) { return FMath::Abs(A - B) < LayerTolerance; }

	/** Region list used for containment / overlap: sub-regions when present (distributed districts), else the boundary. */
	static TArray<const FVoidPlannedBoundary*> Regions(const FVoidPlannedDistrict& District)
	{
		TArray<const FVoidPlannedBoundary*> Result;
		if (District.SubRegions.Num() > 0)
		{
			for (const FVoidPlannedBoundary& Region : District.SubRegions) { Result.Add(&Region); }
		}
		else if (District.Boundary.Polygon.Num() >= 3)
		{
			Result.Add(&District.Boundary);
		}
		return Result;
	}

	static bool BoxInsideAnyRegion(const FVoidPlannedDistrict& District, const FVoidBox2D& Box)
	{
		for (const FVoidPlannedBoundary* Region : Regions(District))
		{
			if (FVoidDistrictGeometry::IsBoxInsideShape(Box, Region->Polygon, Region->HoleCenter, Region->HoleRadius))
			{
				return true;
			}
		}
		return false;
	}

	static bool PointInsideAnyRegion(const FVoidPlannedDistrict& District, const FVector2D& Point)
	{
		for (const FVoidPlannedBoundary* Region : Regions(District))
		{
			if (FVoidDistrictGeometry::IsPointInShape(Point, Region->Polygon, Region->HoleCenter, Region->HoleRadius))
			{
				return true;
			}
		}
		return false;
	}

	struct FRoadSegBox
	{
		const FVoidPlannedRoad* Road = nullptr;
		FVoidBox2D Box;
		double Z = 0.0;
		double Radius = 0.0;
	};

	static void BuildRoadBoxes(const FVoidDistrictLayoutPlan& Plan, const FVoidDistrictLayoutParams& Params, TArray<FRoadSegBox>& Out)
	{
		for (const FVoidPlannedRoad* Road : Plan.GatherAllRoads())
		{
			const double Width = Road->Spec.WidthUnits > 0.0f ? Road->Spec.WidthUnits : 600.0;
			// 1 unit smaller than the true clearance so shapes that exactly touch the corridor edge are not reported.
			const double Half = Width * 0.5 + RoadEdgeUnits + Params.RoadClearance - 1.0;
			const TArray<FVector2D>& Points = Road->Spec.CenterlinePoints;
			for (int32 Index = 0; Index + 1 < Points.Num(); ++Index)
			{
				const double DX = Points[Index + 1].X - Points[Index].X;
				const double DY = Points[Index + 1].Y - Points[Index].Y;
				if (DX * DX + DY * DY < 1.0) { continue; }
				FRoadSegBox Seg;
				Seg.Road = Road;
				Seg.Box = FVoidDistrictGeometry::MakeSegmentBox(Points[Index], Points[Index + 1], Half, 0.0);
				Seg.Z = Road->Spec.ElevationUnits;
				Seg.Radius = FMath::Sqrt(Seg.Box.HalfExtent.X * Seg.Box.HalfExtent.X + Seg.Box.HalfExtent.Y * Seg.Box.HalfExtent.Y);
				Out.Add(Seg);
			}
		}
	}

	static bool NearBounds(const FVoidBox2D& A, double RadiusA, const FRoadSegBox& Seg)
	{
		const double DX = A.Center.X - Seg.Box.Center.X;
		const double DY = A.Center.Y - Seg.Box.Center.Y;
		const double R = RadiusA + Seg.Radius;
		return DX * DX + DY * DY <= R * R;
	}

	static double BoxRadius(const FVoidBox2D& Box)
	{
		return FMath::Sqrt(Box.HalfExtent.X * Box.HalfExtent.X + Box.HalfExtent.Y * Box.HalfExtent.Y);
	}

	static bool PointTouchesRoad(const FVector2D& Point, const FVoidPlannedRoad& Other)
	{
		const TArray<FVector2D>& Points = Other.Spec.CenterlinePoints;
		for (int32 Index = 0; Index + 1 < Points.Num(); ++Index)
		{
			if (FVoidDistrictGeometry::DistancePointToSegment(Point, Points[Index], Points[Index + 1]) <= FVoidDistrictValidator::EndpointTolerance)
			{
				return true;
			}
		}
		return false;
	}

	static void AddUniqueIdCheck(FVoidDistrictLayoutPlan& Plan, TSet<FName>& Seen, FName Id, const TCHAR* Kind)
	{
		if (Id.IsNone())
		{
			Plan.Report.AddError(FString::Printf(TEXT("A %s has no id."), Kind), Kind, TEXT("VOID.District.MissingId"));
			return;
		}
		if (Seen.Contains(Id))
		{
			Plan.Report.AddError(FString::Printf(TEXT("Duplicate %s id '%s'. Generated actors need stable, unique ids."), Kind, *N(Id)), Kind, TEXT("VOID.District.DuplicateId"));
		}
		Seen.Add(Id);
	}
}

void FVoidDistrictValidator::Validate(FVoidDistrictLayoutPlan& Plan, const FVoidMeridianDataSet& Data, const FVoidDistrictLayoutParams& Params)
{
	using namespace VoidValidatorPrivate;

	FVoidValidationReport& Report = Plan.Report;

	// --- Stable ids -------------------------------------------------------
	{
		TSet<FName> Roads, Buildings, Spaces, Landmarks, Ports;
		for (const FVoidPlannedRoad* Road : Plan.GatherAllRoads()) { AddUniqueIdCheck(Plan, Roads, Road->Spec.Id.Value, TEXT("road")); }
		for (const FVoidPlannedDistrict& District : Plan.Districts)
		{
			for (const FVoidPlannedBuilding& B : District.Buildings) { AddUniqueIdCheck(Plan, Buildings, B.Spec.Id.Value, TEXT("building")); }
			for (const FVoidPlannedPublicSpace& S : District.PublicSpaces) { AddUniqueIdCheck(Plan, Spaces, S.Id, TEXT("publicSpace")); }
			for (const FVoidPlannedLandmark& L : District.Landmarks) { AddUniqueIdCheck(Plan, Landmarks, L.Id, TEXT("landmark")); }
			for (const FVoidPlannedPort& Pt : District.Ports) { AddUniqueIdCheck(Plan, Ports, Pt.Id, TEXT("port")); }
		}
	}

	// --- Profiles ---------------------------------------------------------
	for (const FVoidPlannedDistrict& District : Plan.Districts)
	{
		const FVoidDistrictProfile& P = District.Profile;
		if (P.BuildingCoverage <= 0.0 || P.BuildingCoverage >= 1.0 || P.OpenSpaceRatio < 0.0 || P.OpenSpaceRatio > 1.0 || P.MinStories < 1 || P.MaxStories < P.MinStories)
		{
			Report.AddError(FString::Printf(TEXT("District '%s' has an invalid character profile (coverage %.2f, open space %.2f, stories %d..%d)."), *N(District.Id), P.BuildingCoverage, P.OpenSpaceRatio, P.MinStories, P.MaxStories),
				TEXT("profile"), TEXT("VOID.District.BadProfile"));
		}
	}

	TArray<FRoadSegBox> RoadBoxes;
	BuildRoadBoxes(Plan, Params, RoadBoxes);

	// --- Buildings ----------------------------------------------------------
	for (const FVoidPlannedDistrict& District : Plan.Districts)
	{
		for (const FVoidPlannedBuilding& Building : District.Buildings)
		{
			const FVoidBox2D Box = BoxOf(Building);

			if (!District.bFromExplicitPackage && !BoxInsideAnyRegion(District, Box))
			{
				Report.AddError(FString::Printf(TEXT("Building '%s' is not fully inside district '%s' boundary."), *N(Building.Spec.Id.Value), *N(District.Id)),
					TEXT("buildings"), TEXT("VOID.District.BuildingOutsideBoundary"));
			}

			const double Radius = BoxRadius(Box);
			for (const FRoadSegBox& Seg : RoadBoxes)
			{
				if (!SameLayer(Seg.Z, Building.BaseZ) || !NearBounds(Box, Radius, Seg)) { continue; }
				if (FVoidDistrictGeometry::DoBoxesOverlap(Box, Seg.Box, 0.0))
				{
					Report.AddError(FString::Printf(TEXT("Building '%s' overlaps the corridor of road '%s'."), *N(Building.Spec.Id.Value), *N(Seg.Road->Spec.Id.Value)),
						TEXT("buildings"), TEXT("VOID.District.BuildingOnRoad"));
					break;
				}
			}
		}
	}

	// Building vs building across the whole plan (same layer).
	{
		TArray<const FVoidPlannedBuilding*> All;
		for (const FVoidPlannedDistrict& District : Plan.Districts)
		{
			for (const FVoidPlannedBuilding& B : District.Buildings) { All.Add(&B); }
		}
		for (int32 I = 0; I < All.Num(); ++I)
		{
			for (int32 J = I + 1; J < All.Num(); ++J)
			{
				if (!SameLayer(All[I]->BaseZ, All[J]->BaseZ)) { continue; }
				const double DX = All[I]->Center.X - All[J]->Center.X;
				const double DY = All[I]->Center.Y - All[J]->Center.Y;
				if (DX * DX + DY * DY < 1.0)
				{
					Report.AddError(FString::Printf(TEXT("Buildings '%s' and '%s' occupy the same position (duplicate geometry)."), *N(All[I]->Spec.Id.Value), *N(All[J]->Spec.Id.Value)),
						TEXT("buildings"), TEXT("VOID.District.DuplicateGeometry"));
				}
				else if (FVoidDistrictGeometry::DoBoxesOverlap(BoxOf(*All[I]), BoxOf(*All[J]), -1.0))
				{
					Report.AddError(FString::Printf(TEXT("Buildings '%s' and '%s' overlap."), *N(All[I]->Spec.Id.Value), *N(All[J]->Spec.Id.Value)),
						TEXT("buildings"), TEXT("VOID.District.BuildingsOverlap"));
				}
			}
		}
	}

	// --- Public spaces --------------------------------------------------------
	for (const FVoidPlannedDistrict& District : Plan.Districts)
	{
		for (const FVoidPlannedPublicSpace& Space : District.PublicSpaces)
		{
			bool bInside = !District.bFromExplicitPackage;
			if (bInside)
			{
				for (const FVector2D& Vertex : Space.Polygon)
				{
					if (!PointInsideAnyRegion(District, Vertex)) { bInside = false; break; }
				}
			}
			if (!bInside && !District.bFromExplicitPackage)
			{
				Report.AddError(FString::Printf(TEXT("Public space '%s' extends outside district '%s'."), *N(Space.Id), *N(District.Id)), TEXT("publicSpaces"), TEXT("VOID.District.PublicSpaceOutsideBoundary"));
			}

			for (const FVoidPlannedDistrict& Other : Plan.Districts)
			{
				for (const FVoidPlannedBuilding& Building : Other.Buildings)
				{
					if (!SameLayer(Building.BaseZ, Space.Z)) { continue; }
					if (FVoidDistrictGeometry::DoesBoxOverlapShape(BoxOf(Building), Space.Polygon, Space.HoleCenter, Space.HoleRadius, 0.0))
					{
						Report.AddError(
							FString::Printf(TEXT("Public space '%s' overlaps %s '%s'."), *N(Space.Id), Building.bMajorStructure ? TEXT("major structure") : TEXT("building"), *N(Building.Spec.Id.Value)),
							TEXT("publicSpaces"), Building.bMajorStructure ? TEXT("VOID.District.PublicSpaceOverlapsMajorStructure") : TEXT("VOID.District.PublicSpaceOverlapsBuilding"));
					}
				}
			}

			for (const FRoadSegBox& Seg : RoadBoxes)
			{
				if (!IsSurface(Seg.Z)) { continue; }
				if (FVoidDistrictGeometry::DoesBoxOverlapShape(Seg.Box, Space.Polygon, Space.HoleCenter, Space.HoleRadius, 0.0))
				{
					Report.AddError(FString::Printf(TEXT("Public space '%s' overlaps the corridor of road '%s'."), *N(Space.Id), *N(Seg.Road->Spec.Id.Value)), TEXT("publicSpaces"), TEXT("VOID.District.PublicSpaceOnRoad"));
					break;
				}
			}
		}
	}

	// --- Roads ------------------------------------------------------------------
	{
		TArray<const FVoidPlannedRoad*> Roads = Plan.GatherAllRoads();
		TArray<FVector> PortLocations;
		for (const FVoidPlannedDistrict& District : Plan.Districts)
		{
			for (const FVoidPlannedPort& Port : District.Ports) { PortLocations.Add(Port.Location); }
		}

		for (const FVoidPlannedRoad* Road : Roads)
		{
			if (Road->bFreeEndpointsAllowed || Road->Spec.CenterlinePoints.Num() < 2) { continue; }

			const FVector2D Endpoints[2] = { Road->Spec.CenterlinePoints[0], Road->Spec.CenterlinePoints.Last() };
			for (const FVector2D& Endpoint : Endpoints)
			{
				bool bConnected = false;

				for (const FVoidPlannedRoad* Other : Roads)
				{
					if (Other == Road) { continue; }
					// Live and dead networks never connect: only compare within the same layer.
					if (IsSurface(Other->Spec.ElevationUnits) != IsSurface(Road->Spec.ElevationUnits)) { continue; }
					if (PointTouchesRoad(Endpoint, *Other)) { bConnected = true; break; }
				}

				if (!bConnected)
				{
					for (const FVector& Port : PortLocations)
					{
						const double DX = Endpoint.X - Port.X, DY = Endpoint.Y - Port.Y;
						if (DX * DX + DY * DY <= FVoidDistrictValidator::EndpointTolerance * FVoidDistrictValidator::EndpointTolerance) { bConnected = true; break; }
					}
				}

				if (!bConnected)
				{
					Report.AddError(FString::Printf(TEXT("Road '%s' ends at (%.0f, %.0f) without connecting to another road or a port."), *N(Road->Spec.Id.Value), Endpoint.X, Endpoint.Y),
						TEXT("roads"), TEXT("VOID.District.RoadDeadEnd"));
				}
			}
		}

		// Roads must avoid districts they are not allowed to run through.
		const FVoidPlannedDistrict* Archives = Plan.FindDistrict(VoidDistrictNames::MetroArchives);
		const FVoidPlannedDistrict* WhiteZones = Plan.FindDistrict(VoidDistrictNames::WhiteZones);
		for (const FVoidPlannedRoad* Road : Roads)
		{
			TArray<FVector2D> Polyline = Road->Spec.CenterlinePoints;
			if (IsSurface(Road->Spec.ElevationUnits))
			{
				if (Archives && Archives->Boundary.Polygon.Num() >= 3 && FVoidDistrictGeometry::DoesPolylineIntersectPolygon(Polyline, Archives->Boundary.Polygon))
				{
					Report.AddError(FString::Printf(TEXT("Surface road '%s' runs through Metro Archives. Metro Archives is reached by substrate/tunnel routes only."), *N(Road->Spec.Id.Value)),
						TEXT("roads"), TEXT("VOID.District.RoadCrossesDistrict"));
				}
				if (WhiteZones && Road->OwnerId != WhiteZones->Id)
				{
					for (const FVoidPlannedBoundary& Node : WhiteZones->SubRegions)
					{
						if (FVoidDistrictGeometry::DoesPolylineIntersectPolygon(Polyline, Node.Polygon))
						{
							Report.AddError(FString::Printf(TEXT("Road '%s' runs through White Zones node '%s'; arterials and the ring must pass near, not through, nodes."), *N(Road->Spec.Id.Value), *N(Node.Id)),
								TEXT("roads"), TEXT("VOID.District.RoadCrossesDistrict"));
						}
					}
				}
			}
			else if (Road->RouteId == VoidDistrictNames::MidTierRingRoad)
			{
				Report.AddError(FString::Printf(TEXT("Ring road '%s' is below grade; the mid-tier ring may never route through the Undercroft."), *N(Road->Spec.Id.Value)), TEXT("roads"), TEXT("VOID.District.RoadCrossesDistrict"));
			}
		}

		// Unreachable-by-design pairs: no single road may serve both districts of such a pair.
		for (const TPair<FName, FName>& Pair : Data.UnreachablePairs)
		{
			for (const FVoidPlannedRoad* Road : Roads)
			{
				if (Road->ServesDistricts.Contains(Pair.Key) && Road->ServesDistricts.Contains(Pair.Value))
				{
					Report.AddError(FString::Printf(TEXT("Road '%s' connects '%s' and '%s', which RoadNetwork.json marks unreachable by design."), *N(Road->Spec.Id.Value), *N(Pair.Key), *N(Pair.Value)),
						TEXT("roads"), TEXT("VOID.District.UnreachablePairConnected"));
				}
			}
		}
	}

	// --- District boundaries --------------------------------------------------------
	for (int32 I = 0; I < Plan.Districts.Num(); ++I)
	{
		for (int32 J = I + 1; J < Plan.Districts.Num(); ++J)
		{
			const FVoidPlannedDistrict& A = Plan.Districts[I];
			const FVoidPlannedDistrict& B = Plan.Districts[J];
			if (!SameLayer(A.Boundary.Z, B.Boundary.Z)) { continue; }

			bool bOverlap = false;
			for (const FVoidPlannedBoundary* RA : Regions(A))
			{
				for (const FVoidPlannedBoundary* RB : Regions(B))
				{
					if (FVoidDistrictGeometry::DoPolygonsOverlap(RA->Polygon, RB->Polygon)) { bOverlap = true; }
				}
			}
			if (bOverlap)
			{
				Report.AddError(FString::Printf(TEXT("Districts '%s' and '%s' overlap on the same layer."), *N(A.Id), *N(B.Id)), TEXT("boundaries"), TEXT("VOID.District.BoundariesOverlap"));
			}
		}
	}

	for (const FVoidMeridianAdjacencyEdge& Edge : Data.AdjacencyEdges)
	{
		const FVoidPlannedDistrict* A = Plan.FindDistrict(Edge.From);
		const FVoidPlannedDistrict* B = Plan.FindDistrict(Edge.To);
		if (!A || !B) { continue; }

		bool bTouch = false;
		for (const FVoidPlannedBoundary* RA : Regions(*A))
		{
			for (const FVoidPlannedBoundary* RB : Regions(*B))
			{
				if (FVoidDistrictGeometry::DoPolygonsOverlap(RA->Polygon, RB->Polygon)) { bTouch = true; }
			}
		}
		if (!bTouch)
		{
			Report.AddWarning(FString::Printf(TEXT("Registry adjacency '%s' <-> '%s' (%s) is not reflected in the generated boundaries."), *N(Edge.From), *N(Edge.To), *Edge.AdjacencyType), TEXT("boundaries"), TEXT("VOID.District.AdjacencyNotHonored"));
		}
	}

	// --- Landmarks & sightlines ---------------------------------------------------------
	{
		const FVoidPlannedLandmark* Spire = nullptr;
		for (const FVoidPlannedDistrict& District : Plan.Districts)
		{
			for (const FVoidPlannedLandmark& L : District.Landmarks)
			{
				if (L.RegistryId == VoidDistrictNames::SpireSilhouette) { Spire = &L; }
				if (L.OpenFlagId != NAME_None)
				{
					Report.AddInfo(FString::Printf(TEXT("Landmark '%s' carries open flag '%s': architecture is generated, encounter content is not (BuilderRules.json)."), *N(L.Id), *N(L.OpenFlagId)), TEXT("landmarks"), TEXT("VOID.District.OpenFlagCarried"));
				}
			}
		}

		if (Spire)
		{
			const double SpireTop = Spire->Height;
			const FVector2D SpireXY(Spire->Location.X, Spire->Location.Y);

			TArray<FVoidBox2D> Obstacles;
			TArray<double> Tops;
			TArray<FName> ObstacleIds;
			for (const FVoidPlannedDistrict& District : Plan.Districts)
			{
				for (const FVoidPlannedBuilding& B : District.Buildings)
				{
					if (!IsSurface(B.BaseZ)) { continue; }
					Obstacles.Add(BoxOf(B));
					Tops.Add(B.BaseZ + B.Spec.HeightUnits);
					ObstacleIds.Add(B.Spec.Id.Value);
				}
			}

			auto CheckFrom = [&](const FVector2D& From, double FromZ, FName ExcludeBuilding, const FString& Label)
			{
				TArray<FVoidBox2D> Filtered;
				TArray<double> FilteredTops;
				for (int32 Index = 0; Index < Obstacles.Num(); ++Index)
				{
					if (ObstacleIds[Index] == ExcludeBuilding || ObstacleIds[Index] == Spire->BackingBuildingId) { continue; }
					Filtered.Add(Obstacles[Index]);
					FilteredTops.Add(Tops[Index]);
				}
				if (!FVoidDistrictGeometry::IsSightlineClear(From, FromZ, SpireXY, SpireTop, Filtered, FilteredTops))
				{
					Report.AddWarning(FString::Printf(TEXT("Spire sightline from %s is blocked by generated buildings (non-blocking per Master Plan Sec. 17)."), *Label), TEXT("landmarks"), TEXT("VOID.District.SpireSightlineBlocked"));
				}
			};

			const FVoidPlannedDistrict* WhiteZones = Plan.FindDistrict(VoidDistrictNames::WhiteZones);
			bool bWantsWhiteZonePlazas = false, bWantsArchives = false, bWantsUndercroftShafts = false;
			for (const FName& From : Spire->SightlineVisibleFrom)
			{
				const FString S = From.ToString();
				bWantsWhiteZonePlazas |= S.Contains(TEXT("white_zones"));
				bWantsArchives |= S.Contains(TEXT("metro_archives"));
				bWantsUndercroftShafts |= S.Contains(TEXT("undercroft"));
			}

			if (bWantsWhiteZonePlazas && WhiteZones)
			{
				for (const FVoidPlannedPublicSpace& Space : WhiteZones->PublicSpaces)
				{
					if (Space.Type == EVoidPublicSpaceType::Plaza)
					{
						CheckFrom(Space.Center, SightlineEyeHeight, NAME_None, FString::Printf(TEXT("plaza '%s'"), *N(Space.Id)));
					}
				}
			}
			if (bWantsArchives)
			{
				if (const FVoidPlannedDistrict* Archives = Plan.FindDistrict(VoidDistrictNames::MetroArchives))
				{
					if (Archives->Buildings.Num() > 0)
					{
						const FVoidPlannedBuilding& B = Archives->Buildings[0];
						const FVector2D Toward(SpireXY.X - B.Center.X, SpireXY.Y - B.Center.Y);
						const double Len = FMath::Sqrt(Toward.X * Toward.X + Toward.Y * Toward.Y);
						if (Len > 1.0)
						{
							const FVector2D Window(B.Center.X + Toward.X / Len * B.HalfExtent.X, B.Center.Y + Toward.Y / Len * B.HalfExtent.X);
							CheckFrom(Window, 300.0, B.Spec.Id.Value, TEXT("the Metro Archives public floor"));
						}
					}
				}
			}
			if (bWantsUndercroftShafts)
			{
				Report.AddInfo(TEXT("Spire sightline from Undercroft 'rare upward shafts' not checked: Meridian data gives no shaft locations."), TEXT("landmarks"), TEXT("VOID.District.SightlineShaftsUnplaced"));
			}
		}

		// Sector 0 must be enclosed: entirely below grade and excluded from the Spire sightline rule.
		if (const FVoidPlannedDistrict* Sector0 = Plan.FindDistrict(VoidDistrictNames::Sector0))
		{
			if (IsSurface(Sector0->Boundary.Z))
			{
				Report.AddError(TEXT("Sector 0 boundary is at grade; it must be enclosed below grade (no Spire sightline)."), TEXT("boundaries"), TEXT("VOID.District.Sector0Sightline"));
			}
		}
	}

	Report.AddInfo(FString::Printf(TEXT("District plan: %d districts, %d roads, %d buildings, %d public spaces, %d landmarks."),
		Plan.Districts.Num(), Plan.GatherAllRoads().Num(),
		[&Plan]() { int32 Count = 0; for (const FVoidPlannedDistrict& D : Plan.Districts) { Count += D.Buildings.Num(); } return Count; }(),
		[&Plan]() { int32 Count = 0; for (const FVoidPlannedDistrict& D : Plan.Districts) { Count += D.PublicSpaces.Num(); } return Count; }(),
		[&Plan]() { int32 Count = 0; for (const FVoidPlannedDistrict& D : Plan.Districts) { Count += D.Landmarks.Num(); } return Count; }()),
		TEXT("plan"), TEXT("VOID.District.PlanSummary"));
}
