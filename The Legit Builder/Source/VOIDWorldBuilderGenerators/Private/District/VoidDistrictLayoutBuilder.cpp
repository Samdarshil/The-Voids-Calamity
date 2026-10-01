// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "District/VoidDistrictLayoutBuilder.h"
#include "District/VoidDistrictGeometry.h"
#include "Algo/Sort.h"

namespace VoidLayoutPrivate
{
	constexpr double TwoPi = 2.0 * PI;

	/** Sidewalk (150) + curb (25) of the Road generator's built-in profiles; RoadClearance in the params is added on top. */
	constexpr double RoadEdgeUnits = 175.0;

	static double Rad(double Degrees) { return Degrees * PI / 180.0; }

	static double NormAngle(double Angle)
	{
		double Result = FMath::Fmod(Angle, TwoPi);
		if (Result < 0.0) { Result += TwoPi; }
		return Result;
	}

	static FString N(FName Name) { return Name.ToString(); }

	/** Density band per district. Source: Meridian_Master_Plan.md Sec. 22 "Population Distribution" table. */
	static bool LookupDensityBand(FName DistrictId, EVoidDistrictDensity& OutDensity, FString& OutSource)
	{
		OutSource = TEXT("Meridian_Master_Plan.md Sec. 22");
		if (DistrictId == VoidDistrictNames::OlympusSpire) { OutDensity = EVoidDistrictDensity::Sparse;   return true; }
		if (DistrictId == VoidDistrictNames::WhiteZones)   { OutDensity = EVoidDistrictDensity::High;     return true; }
		if (DistrictId == VoidDistrictNames::MetroArchives){ OutDensity = EVoidDistrictDensity::Sparse;   return true; }
		if (DistrictId == VoidDistrictNames::Undercroft)   { OutDensity = EVoidDistrictDensity::High;     return true; }
		if (DistrictId == VoidDistrictNames::Sector0)      { OutDensity = EVoidDistrictDensity::NearZero; return true; }
		return false;
	}

	static const TCHAR* DensityName(EVoidDistrictDensity Density)
	{
		switch (Density)
		{
			case EVoidDistrictDensity::NearZero: return TEXT("NearZero");
			case EVoidDistrictDensity::Sparse:   return TEXT("Sparse");
			case EVoidDistrictDensity::Moderate: return TEXT("Moderate");
			default:                             return TEXT("High");
		}
	}

	struct FLayoutCtx
	{
		const FVoidDistrictLayoutInput& In;
		const FVoidDistrictLayoutParams& P;
		const FVoidMeridianDataSet& D;
		FVoidDistrictLayoutPlan& Plan;

		TArray<double> SpokeAngles;
		double SpokeStep = 0.0;
		double MidRing = 0.0;
		double SpireRing = 0.0;
		double ArcStepRad = 0.05;
		FName SpineRouteId;

		TSet<FName> PlacedLandmarkRegistryIds;

		FLayoutCtx(const FVoidDistrictLayoutInput& InInput, FVoidDistrictLayoutPlan& InPlan)
			: In(InInput), P(InInput.Params), D(*InInput.Data), Plan(InPlan)
		{
		}

		double RoadHalfClear(double Width) const { return Width * 0.5 + RoadEdgeUnits + P.RoadClearance; }
		FVector2D Polar(double Radius, double Angle) const { return FVoidDistrictGeometry::PolarPoint(FVector2D::ZeroVector, Radius, Angle); }
	};

	static TArray<FVector2D> ArcPoints(double Radius, double A0, double A1, double StepRad)
	{
		TArray<FVector2D> Points;
		const int32 Steps = FMath::Max(1, static_cast<int32>(FMath::CeilToInt(static_cast<float>((A1 - A0) / StepRad))));
		for (int32 Index = 0; Index <= Steps; ++Index)
		{
			const double A = A0 + (A1 - A0) * static_cast<double>(Index) / static_cast<double>(Steps);
			Points.Add(FVoidDistrictGeometry::PolarPoint(FVector2D::ZeroVector, Radius, A));
		}
		return Points;
	}

	static FVoidPlannedRoad MakeRoad(const FString& Id, FName Owner, EVoidRoadType Type, double Width, const TArray<FVector2D>& Points, double Elevation, bool bSidewalk, bool bMedian, FName RouteId, const TArray<FName>& Serves)
	{
		FVoidPlannedRoad Road;
		Road.Spec.Id = FVoidElementId(FName(*Id));
		Road.Spec.CenterlinePoints = Points;
		Road.Spec.WidthUnits = static_cast<float>(Width);
		Road.Spec.RoadType = Type;
		Road.Spec.ElevationUnits = static_cast<float>(Elevation);
		Road.Spec.bHasSidewalk = bSidewalk;
		Road.Spec.bHasMedian = bMedian;
		Road.OwnerId = Owner;
		Road.RouteId = RouteId;
		Road.ServesDistricts = Serves;
		return Road;
	}

	/** Splits a full circle at the given angles into arcs; every arc endpoint is shared with its neighbour, so no free endpoints. */
	static void AddRingArcs(FLayoutCtx& C, const FString& Prefix, double Radius, TArray<double> Angles, FName Owner, EVoidRoadType Type, double Width, double Elevation, bool bSidewalk, FName RouteId, const TArray<FName>& Serves, TArray<FVoidPlannedRoad>& Out)
	{
		for (double& Angle : Angles) { Angle = NormAngle(Angle); }
		Angles.Sort();

		TArray<double> Unique;
		for (double Angle : Angles)
		{
			if (Unique.Num() == 0 || Angle - Unique.Last() > 1e-6) { Unique.Add(Angle); }
		}
		if (Unique.Num() > 1 && (Unique[0] + TwoPi) - Unique.Last() < 1e-6) { Unique.Pop(); }
		if (Unique.Num() < 2) { return; }

		for (int32 Index = 0; Index < Unique.Num(); ++Index)
		{
			const double A0 = Unique[Index];
			const double A1 = (Index + 1 < Unique.Num()) ? Unique[Index + 1] : Unique[0] + TwoPi;
			Out.Add(MakeRoad(FString::Printf(TEXT("%s.arc_%d"), *Prefix, Index), Owner, Type, Width, ArcPoints(Radius, A0, A1, C.ArcStepRad), Elevation, bSidewalk, false, RouteId, Serves));
		}
	}

	static FVoidPlannedLandmark* AddLandmark(FLayoutCtx& C, FVoidPlannedDistrict& District, FName RegistryId, const FString& InstanceSuffix, const FVector& Location, double YawDegrees, const FVector2D& HalfExtent, double Height, FName BackingBuildingId, int32 NodeIndex)
	{
		const FVoidMeridianLandmarkEntry* Entry = C.D.Landmarks.FindByPredicate([RegistryId](const FVoidMeridianLandmarkEntry& L) { return L.Id == RegistryId; });
		if (!Entry)
		{
			C.Plan.Report.AddWarning(
				FString::Printf(TEXT("Landmark '%s' has a placement rule but is not in LandmarkRegistry.json; not generated."), *N(RegistryId)),
				TEXT("landmarks"), TEXT("VOID.District.LandmarkNotInRegistry"));
			return nullptr;
		}
		if (Entry->DistrictId != District.Id)
		{
			C.Plan.Report.AddError(
				FString::Printf(TEXT("Landmark '%s' is registered to district '%s' but its placement rule ran for '%s'."), *N(RegistryId), *N(Entry->DistrictId), *N(District.Id)),
				TEXT("landmarks"), TEXT("VOID.District.LandmarkWrongDistrict"));
			return nullptr;
		}

		FVoidPlannedLandmark Landmark;
		Landmark.RegistryId = RegistryId;
		Landmark.Id = InstanceSuffix.IsEmpty() ? RegistryId : FName(*FString::Printf(TEXT("%s.%s"), *N(RegistryId), *InstanceSuffix));
		Landmark.OwnerId = District.Id;
		Landmark.Type = Entry->Type;
		Landmark.VisibilityTier = Entry->VisibilityTier;
		Landmark.bInterior = Entry->IsInterior();
		Landmark.bBelowGrade = Entry->IsBelowGrade();
		Landmark.Location = Location;
		Landmark.YawDegrees = YawDegrees;
		Landmark.HalfExtent = HalfExtent;
		Landmark.Height = Height;
		Landmark.BackingBuildingId = BackingBuildingId;
		Landmark.SightlineVisibleFrom = Entry->SightlineVisibleFrom;
		Landmark.SightlineExcluded = Entry->SightlineExcluded;
		Landmark.NodeIndex = NodeIndex;
		if (RegistryId == VoidDistrictNames::SpireServerHub)
		{
			// BuilderRules.json error_handling.highest_priority_flag: architecture yes, encounter content no.
			Landmark.OpenFlagId = VoidDistrictNames::ServerHubFlag;
		}

		if (C.In.Overrides)
		{
			if (const FVoidMeridianLandmarkOverride* Override = C.In.Overrides->FindLandmark(RegistryId))
			{
				if (InstanceSuffix.IsEmpty())
				{
					Landmark.Location = Override->Location;
					Landmark.YawDegrees = Override->YawDegrees;
					Landmark.bPositionIsPlaceholder = false;
				}
			}
		}

		C.PlacedLandmarkRegistryIds.Add(RegistryId);
		District.Landmarks.Add(Landmark);
		return &District.Landmarks.Last();
	}

	static FVoidPlannedBuilding MakeBuilding(const FString& Id, FName Owner, FName Category, const FString& BuildingType, const FVoidBox2D& Box, double Height, double BaseZ, bool bMajor)
	{
		FVoidPlannedBuilding Building;
		Building.Spec.Id = FVoidElementId(FName(*Id));
		Building.Spec.FootprintCorners = Box.GetCorners();
		Building.Spec.HeightUnits = static_cast<float>(Height);
		Building.Spec.BuildingType = BuildingType;
		Building.OwnerId = Owner;
		Building.CategoryId = Category;
		Building.Center = Box.Center;
		Building.HalfExtent = Box.HalfExtent;
		Building.YawRadians = Box.YawRadians;
		Building.BaseZ = BaseZ;
		Building.bMajorStructure = bMajor;
		return Building;
	}

	static FVoidPlannedPublicSpace MakePublicSpace(const FString& Id, FName Owner, EVoidPublicSpaceType Type, const FVoidBox2D& Box, int32 NodeIndex)
	{
		FVoidPlannedPublicSpace Space;
		Space.Id = FName(*Id);
		Space.OwnerId = Owner;
		Space.Type = Type;
		Space.Polygon = Box.GetCorners();
		Space.Center = Box.Center;
		Space.NodeIndex = NodeIndex;
		return Space;
	}

	static FVoidPlannedBoundary MakeCircleBoundary(FName Id, const FVector2D& Center, double Radius, double HoleRadius, double Z)
	{
		FVoidPlannedBoundary Boundary;
		Boundary.Id = Id;
		Boundary.Polygon = FVoidDistrictGeometry::MakeCirclePolygon(Center, Radius, 96);
		Boundary.HoleCenter = Center;
		Boundary.HoleRadius = HoleRadius;
		Boundary.Z = Z;
		return Boundary;
	}

	static FVoidPlannedBoundary MakeBoxBoundary(FName Id, const FVoidBox2D& Box, double Z)
	{
		FVoidPlannedBoundary Boundary;
		Boundary.Id = Id;
		Boundary.Polygon = Box.GetCorners();
		Boundary.Z = Z;
		return Boundary;
	}

	static FVoidPlannedDistrict& BeginDistrict(FLayoutCtx& C, const FVoidMeridianDistrictEntry& Entry)
	{
		FVoidPlannedDistrict& District = C.Plan.Districts.AddDefaulted_GetRef();
		District.Id = Entry.Id;
		District.DisplayName = Entry.DisplayName;

		int32 LandmarkCount = 0;
		for (const FVoidMeridianLandmarkEntry& Landmark : C.D.Landmarks)
		{
			if (Landmark.DistrictId == Entry.Id)
			{
				++LandmarkCount;
			}
		}
		District.Profile = FVoidDistrictLayoutBuilder::ResolveProfile(Entry, C.In, LandmarkCount);
		return District;
	}

	// =====================================================================
	// Live Network: radial spine (outer part) + mid-tier ring road.
	// Topology from RoadNetwork.json; counts / radii from layout params.
	// =====================================================================
	static void BuildLiveNetwork(FLayoutCtx& C)
	{
		const FVoidMeridianRoute* Spine = C.D.FindRoute(VoidDistrictNames::SpireRadialSpine);
		const FVoidMeridianRoute* Ring = C.D.FindRoute(VoidDistrictNames::MidTierRingRoad);
		if (!Spine)
		{
			C.Plan.Report.AddWarning(TEXT("RoadNetwork.json has no 'spire_radial_spine' route; the Live Network is not generated."), TEXT("primary_routes"), TEXT("VOID.District.NoSpine"));
			return;
		}

		const TArray<FName> SpineServes = Spine->ServesDistricts;
		const int32 Spokes = C.SpokeAngles.Num();

		for (int32 Index = 0; Index < Spokes; ++Index)
		{
			const double A = C.SpokeAngles[Index];
			C.Plan.LiveNetworkRoads.Add(MakeRoad(
				FString::Printf(TEXT("live_network.spoke_%d.outer"), Index), VoidDistrictNames::LiveNetworkOwner, EVoidRoadType::Primary, C.P.PrimaryRoadWidth,
				{ C.Polar(C.P.CoreRadius, A), C.Polar(C.MidRing, A) }, 0.0, true, true, Spine->Id, SpineServes));
		}

		if (Ring && C.D.FindDistrict(VoidDistrictNames::WhiteZones))
		{
			TArray<double> Angles = C.SpokeAngles;
			for (int32 Index = 0; Index < Spokes; ++Index)
			{
				Angles.Add(C.SpokeAngles[Index] + C.SpokeStep * 0.5); // node connector points
			}
			AddRingArcs(C, TEXT("live_network.mid_ring"), C.MidRing, Angles, VoidDistrictNames::LiveNetworkOwner, EVoidRoadType::Secondary, C.P.SecondaryRoadWidth, 0.0, true, Ring->Id, Ring->ServesDistricts, C.Plan.LiveNetworkRoads);
		}
	}

	// =====================================================================
	// Olympus Spire: singular civic centerpiece at the origin.
	// =====================================================================
	static void BuildSpire(FLayoutCtx& C, const FVoidMeridianDistrictEntry& Entry)
	{
		FVoidPlannedDistrict& District = BeginDistrict(C, Entry);
		const FName Id = Entry.Id;

		District.Boundary = MakeCircleBoundary(Id, FVector2D::ZeroVector, C.P.CoreRadius, 0.0, 0.0);

		// The tower (landmark tier 1) and the base plaza that wraps it.
		const double TowerHalf = C.P.SpireTowerHalfFootprint;
		FVoidBox2D TowerBox;
		TowerBox.HalfExtent = FVector2D(TowerHalf, TowerHalf);
		District.Buildings.Add(MakeBuilding(N(Id) + TEXT(".bld.primary_tower"), Id, TEXT("spire_primary_tower"), TEXT("SpireTower"), TowerBox, C.P.SpireHeight, 0.0, true));

		FVoidPlannedPublicSpace Plaza;
		Plaza.Id = FName(*(N(Id) + TEXT(".ps.base_plaza")));
		Plaza.OwnerId = Id;
		Plaza.Type = EVoidPublicSpaceType::Plaza;
		Plaza.Polygon = FVoidDistrictGeometry::MakeCirclePolygon(FVector2D::ZeroVector, C.P.BasePlazaRadius, 64);
		Plaza.HoleCenter = FVector2D::ZeroVector;
		Plaza.HoleRadius = TowerHalf * 1.4143 + 500.0;
		District.PublicSpaces.Add(Plaza);

		const double MonumentAngle = C.SpokeAngles.Num() > 0 ? C.SpokeAngles[0] + C.SpokeStep * 0.5 : 0.0;
		const double MonumentRadius = (Plaza.HoleRadius + C.P.BasePlazaRadius) * 0.5;

		AddLandmark(C, District, VoidDistrictNames::SpireSilhouette, FString(), FVector(0, 0, 0), 0.0, FVector2D(TowerHalf, TowerHalf), C.P.SpireHeight, FName(*(N(Id) + TEXT(".bld.primary_tower"))), INDEX_NONE);

		{
			const FVector2D P = C.Polar(MonumentRadius, MonumentAngle);
			AddLandmark(C, District, VoidDistrictNames::ContinuityMonument, FString(), FVector(P.X, P.Y, 0), FMath::RadiansToDegrees(MonumentAngle), FVector2D(400, 400), 1500.0, NAME_None, INDEX_NONE);
		}

		// Interiors live inside the tower footprint; position is a placeholder anchor, never a claim about interior layout.
		AddLandmark(C, District, VoidDistrictNames::AtriumOfPerfectMemory, FString(), FVector(0, 0, 0), 0.0, FVector2D::ZeroVector, 0.0, FName(*(N(Id) + TEXT(".bld.primary_tower"))), INDEX_NONE);
		AddLandmark(C, District, VoidDistrictNames::SpireServerHub, FString(), FVector(0, 0, 0), 0.0, FVector2D::ZeroVector, 0.0, FName(*(N(Id) + TEXT(".bld.primary_tower"))), INDEX_NONE);

		// Elite residential towers: a cluster of secondary towers between base plaza ring and district edge.
		const int32 Spokes = FMath::Max(1, C.SpokeAngles.Num());
		const double BaseTowerRadius = (C.SpireRing + C.P.CoreRadius) * 0.5;
		const double TowerSpacing = C.P.EliteTowerHalfFootprint * 3.5;
		FVector2D ClusterMin(1e300, 1e300), ClusterMax(-1e300, -1e300);
		for (int32 Index = 0; Index < C.P.EliteTowerCount; ++Index)
		{
			const int32 Sector = Index % Spokes;
			const int32 Layer = Index / Spokes;
			const double A = C.SpokeAngles[Sector] + C.SpokeStep * 0.5;
			const FVector2D P = C.Polar(BaseTowerRadius + Layer * TowerSpacing, A);

			FVoidBox2D Box;
			Box.Center = P;
			Box.HalfExtent = FVector2D(C.P.EliteTowerHalfFootprint, C.P.EliteTowerHalfFootprint);
			Box.YawRadians = A;
			District.Buildings.Add(MakeBuilding(FString::Printf(TEXT("%s.bld.elite_tower_%d"), *N(Id), Index), Id, TEXT("elite_residential_tower"), TEXT("EliteResidentialTower"), Box, C.P.EliteTowerHeight, 0.0, true));

			ClusterMin = FVector2D(FMath::Min(ClusterMin.X, P.X), FMath::Min(ClusterMin.Y, P.Y));
			ClusterMax = FVector2D(FMath::Max(ClusterMax.X, P.X), FMath::Max(ClusterMax.Y, P.Y));
		}
		if (C.P.EliteTowerCount > 0)
		{
			const FVector2D Mid((ClusterMin.X + ClusterMax.X) * 0.5, (ClusterMin.Y + ClusterMax.Y) * 0.5);
			const FVector2D Half((ClusterMax.X - ClusterMin.X) * 0.5 + C.P.EliteTowerHalfFootprint, (ClusterMax.Y - ClusterMin.Y) * 0.5 + C.P.EliteTowerHalfFootprint);
			AddLandmark(C, District, VoidDistrictNames::EliteResidentialTowers, FString(), FVector(Mid.X, Mid.Y, 0), 0.0, Half, C.P.EliteTowerHeight, NAME_None, INDEX_NONE);
		}
		// RoadNetwork.json bridge_relationships 'spire_tower_skybridges' is flagged_for_signoff: deliberately NOT generated.

		// Roads: inner ring around the plaza, and the inner part of every spoke.
		const TArray<FName> Serves = { Id, VoidDistrictNames::WhiteZones };
		for (int32 Index = 0; Index < C.SpokeAngles.Num(); ++Index)
		{
			const double A = C.SpokeAngles[Index];
			District.Roads.Add(MakeRoad(FString::Printf(TEXT("%s.spoke_%d.inner"), *N(Id), Index), Id, EVoidRoadType::Primary, C.P.PrimaryRoadWidth,
				{ C.Polar(C.SpireRing, A), C.Polar(C.P.CoreRadius, A) }, 0.0, true, true, C.SpineRouteId, Serves));
		}
		AddRingArcs(C, N(Id) + TEXT(".plaza_ring"), C.SpireRing, C.SpokeAngles, Id, EVoidRoadType::Primary, C.P.PrimaryRoadWidth, 0.0, true, NAME_None, { Id }, District.Roads);

		// Ports for later systems (Live Network stop at the Base Plaza per Master Plan Sec. 12).
		{
			FVoidPlannedPort Stop;
			Stop.Id = FName(*(N(Id) + TEXT(".port.base_plaza_live_network_stop")));
			Stop.OwnerId = Id;
			Stop.Kind = TEXT("live_network_stop_anchor");
			const FVector2D P = C.Polar(C.P.BasePlazaRadius, MonumentAngle);
			Stop.Location = FVector(P.X, P.Y, 0);
			District.Ports.Add(Stop);

			FVoidPlannedPort Origin;
			Origin.Id = FName(*(N(Id) + TEXT(".port.spire_radial_spine_origin")));
			Origin.OwnerId = Id;
			Origin.Kind = TEXT("radial_spine_origin");
			Origin.Location = FVector(0, 0, 0);
			Origin.Connects = { C.SpineRouteId };
			District.Ports.Add(Origin);
		}
	}

	// =====================================================================
	// White Zones: one identical typology node per sector between spokes.
	// =====================================================================
	static void BuildWhiteZones(FLayoutCtx& C, const FVoidMeridianDistrictEntry& Entry)
	{
		FVoidPlannedDistrict& District = BeginDistrict(C, Entry);
		const FName Id = Entry.Id;
		const FVoidDistrictProfile& Profile = District.Profile;

		const int32 HA = C.P.NodeHalfCellsAlongRing;
		const int32 HR = C.P.NodeHalfCellsAcrossRing;
		const double Block = Profile.BlockSizeUnits;
		const double UsableHalf = Block * 0.5 - C.RoadHalfClear(C.P.LocalRoadWidth);

		if (UsableHalf < 500.0)
		{
			C.Plan.Report.AddError(
				FString::Printf(TEXT("White Zones block size %.0f is too small for a %.0f-wide local road plus clearance (usable half-lot %.0f)."), Block, C.P.LocalRoadWidth, UsableHalf),
				TEXT("params.BlockSize"), TEXT("VOID.District.BlockTooSmall"), TEXT("Increase BlockSize or reduce LocalRoadWidth / RoadClearance."));
			return;
		}

		const int32 CellsAlong = 2 * HA;
		const int32 CellsAcross = 2 * HR;
		const int32 TotalCells = CellsAlong * CellsAcross;
		const int32 PlazaCells = FMath::Clamp(Profile.PlazaCellsPerNode, 1, TotalCells - 2);
		const int32 OpenCells = FMath::Clamp(static_cast<int32>(FMath::RoundToInt(static_cast<float>(Profile.OpenSpaceRatio * TotalCells))), PlazaCells, TotalCells - 2);
		const int32 ParkCells = OpenCells - PlazaCells;

		for (int32 NodeIndex = 0; NodeIndex < C.SpokeAngles.Num(); ++NodeIndex)
		{
			const double A = C.SpokeAngles[NodeIndex] + C.SpokeStep * 0.5;
			const double HalfRadial = HR * Block;
			const double HalfAlong = HA * Block;
			const double CenterRadius = C.MidRing - Block - HalfRadial;

			const FVector2D Center = C.Polar(CenterRadius, A);
			const FVector2D U(FMath::Cos(A), FMath::Sin(A));
			const FVector2D V(-FMath::Sin(A), FMath::Cos(A));

			auto GridPoint = [&](double AlongI, double AcrossJ)
			{
				return FVector2D(Center.X + U.X * AcrossJ * Block + V.X * AlongI * Block, Center.Y + U.Y * AcrossJ * Block + V.Y * AlongI * Block);
			};

			FVoidPlannedNode Node;
			Node.Index = NodeIndex;
			Node.bFlagship = (NodeIndex == 0);
			Node.Center = Center;
			Node.AngleRadians = A;
			Node.WorldPartitionRegion = Node.bFlagship ? Entry.WorldPartitionRegion : Entry.WorldPartitionTemplate;
			if (Node.WorldPartitionRegion.IsEmpty()) { Node.WorldPartitionRegion = Entry.WorldPartitionRegion; }

			FVoidBox2D NodeBox;
			NodeBox.Center = Center;
			NodeBox.HalfExtent = FVector2D(HalfRadial, HalfAlong);
			NodeBox.YawRadians = A;
			Node.Footprint = NodeBox.GetCorners();
			District.Nodes.Add(Node);
			District.SubRegions.Add(MakeBoxBoundary(FName(*FString::Printf(TEXT("%s.node_%d"), *N(Id), NodeIndex)), NodeBox, 0.0));

			// --- Roads: interior through-lines + perimeter split at every grid vertex, so every endpoint is shared. ---
			const TArray<FName> Serves = { Id };
			auto NodeRoadId = [&](const TCHAR* Name, int32 A0, int32 A1) { return FString::Printf(TEXT("%s.node_%d.%s_%d_%d"), *N(Id), NodeIndex, Name, A0, A1); };

			for (int32 I = -HA + 1; I <= HA - 1; ++I)
			{
				District.Roads.Add(MakeRoad(NodeRoadId(TEXT("line_r"), I + HA, 0), Id, EVoidRoadType::Local, C.P.LocalRoadWidth, { GridPoint(I, -HR), GridPoint(I, HR) }, 0.0, true, false, NAME_None, Serves));
			}
			for (int32 J = -HR + 1; J <= HR - 1; ++J)
			{
				District.Roads.Add(MakeRoad(NodeRoadId(TEXT("line_t"), J + HR, 0), Id, EVoidRoadType::Local, C.P.LocalRoadWidth, { GridPoint(-HA, J), GridPoint(HA, J) }, 0.0, true, false, NAME_None, Serves));
			}
			for (int32 I = -HA; I < HA; ++I)
			{
				District.Roads.Add(MakeRoad(NodeRoadId(TEXT("edge_out"), I + HA, 0), Id, EVoidRoadType::Local, C.P.LocalRoadWidth, { GridPoint(I, HR), GridPoint(I + 1, HR) }, 0.0, true, false, NAME_None, Serves));
				District.Roads.Add(MakeRoad(NodeRoadId(TEXT("edge_in"), I + HA, 0), Id, EVoidRoadType::Local, C.P.LocalRoadWidth, { GridPoint(I, -HR), GridPoint(I + 1, -HR) }, 0.0, true, false, NAME_None, Serves));
			}
			for (int32 J = -HR; J < HR; ++J)
			{
				District.Roads.Add(MakeRoad(NodeRoadId(TEXT("edge_left"), J + HR, 0), Id, EVoidRoadType::Local, C.P.LocalRoadWidth, { GridPoint(-HA, J), GridPoint(-HA, J + 1) }, 0.0, true, false, NAME_None, Serves));
				District.Roads.Add(MakeRoad(NodeRoadId(TEXT("edge_right"), J + HR, 0), Id, EVoidRoadType::Local, C.P.LocalRoadWidth, { GridPoint(HA, J), GridPoint(HA, J + 1) }, 0.0, true, false, NAME_None, Serves));
			}

			// Connector from the middle of the node's outer edge to the ring road (grid line i = 0 always exists because cell counts are even).
			District.Roads.Add(MakeRoad(FString::Printf(TEXT("%s.node_%d.ring_connector"), *N(Id), NodeIndex), Id, EVoidRoadType::Secondary, C.P.SecondaryRoadWidth,
				{ GridPoint(0, HR), C.Polar(C.MidRing, A) }, 0.0, true, false, NAME_None, Serves));

			// --- Cells ---
			struct FCell { int32 I; int32 J; FVector2D Center; double Dist; };
			TArray<FCell> Cells;
			for (int32 I = -HA; I < HA; ++I)
			{
				for (int32 J = -HR; J < HR; ++J)
				{
					FCell Cell;
					Cell.I = I; Cell.J = J;
					Cell.Center = GridPoint(I + 0.5, J + 0.5);
					const double DX = Cell.Center.X - Center.X, DY = Cell.Center.Y - Center.Y;
					Cell.Dist = FMath::Sqrt(DX * DX + DY * DY);
					Cells.Add(Cell);
				}
			}
			Cells.Sort([](const FCell& L, const FCell& R)
			{
				if (FMath::Abs(L.Dist - R.Dist) > 1e-6) { return L.Dist < R.Dist; }
				if (L.I != R.I) { return L.I < R.I; }
				return L.J < R.J;
			});

			TArray<int32> Kind; // 0 lot, 1 plaza, 2 park, 3 trust center
			Kind.Init(0, Cells.Num());
			for (int32 Index = 0; Index < PlazaCells; ++Index) { Kind[Index] = 1; }
			for (int32 Index = 0; Index < ParkCells; ++Index) { Kind[Cells.Num() - 1 - Index] = 2; } // farthest cells (corners) become parks
			int32 TrustIndex = INDEX_NONE;
			{
				double BestDist = 1e300;
				for (int32 Index = 0; Index < Cells.Num(); ++Index)
				{
					if (Kind[Index] != 0) { continue; }
					const double DX = Cells[Index].Center.X - Cells[0].Center.X, DY = Cells[Index].Center.Y - Cells[0].Center.Y;
					const double Dist = FMath::Sqrt(DX * DX + DY * DY);
					if (Dist < BestDist - 1e-6) { BestDist = Dist; TrustIndex = Index; }
				}
				if (TrustIndex != INDEX_NONE) { Kind[TrustIndex] = 3; }
			}

			// Identical typology per node: the RNG is re-seeded per node from the district id, not the node index.
			FVoidDistrictRng Rng(FVoidDistrictGeometry::StableHash(N(Id)) + static_cast<uint32>(C.P.Seed));
			const double CoverageSide = FMath::Sqrt(Profile.BuildingCoverage);

			for (int32 Index = 0; Index < Cells.Num(); ++Index)
			{
				const FCell& Cell = Cells[Index];
				FVoidBox2D CellBox;
				CellBox.Center = Cell.Center;
				CellBox.HalfExtent = FVector2D(UsableHalf, UsableHalf);
				CellBox.YawRadians = A;
				const FString CellTag = FString::Printf(TEXT("n%d_%d_%d"), NodeIndex, Cell.I + HA, Cell.J + HR);

				if (Kind[Index] == 1)
				{
					District.PublicSpaces.Add(MakePublicSpace(N(Id) + TEXT(".ps.plaza_") + CellTag, Id, EVoidPublicSpaceType::Plaza, CellBox, NodeIndex));
				}
				else if (Kind[Index] == 2)
				{
					District.PublicSpaces.Add(MakePublicSpace(N(Id) + TEXT(".ps.park_") + CellTag, Id, EVoidPublicSpaceType::Park, CellBox, NodeIndex));
				}
				else
				{
					const bool bTrust = (Kind[Index] == 3);
					const int32 Stories = bTrust ? Profile.MaxStories : Rng.NextInt(Profile.MinStories, Profile.MaxStories);
					FVoidBox2D Footprint;
					Footprint.Center = Cell.Center;
					Footprint.YawRadians = A;
					Footprint.HalfExtent = bTrust
						? FVector2D(UsableHalf * 0.9, UsableHalf * 0.9)
						: FVector2D(FMath::Min(UsableHalf, UsableHalf * CoverageSide * (0.9 + 0.2 * Rng.NextUnit())), FMath::Min(UsableHalf, UsableHalf * CoverageSide * (0.9 + 0.2 * Rng.NextUnit())));
					District.Buildings.Add(MakeBuilding(N(Id) + TEXT(".bld.") + CellTag, Id,
						bTrust ? FName(TEXT("community_trust_center")) : FName(TEXT("residential_mid_tier")),
						bTrust ? TEXT("CommunityTrustCenter") : TEXT("Residential_MidTier"),
						Footprint, Stories * Profile.StoryHeightUnits, 0.0, bTrust));

					if (bTrust)
					{
						FVoidPlannedPort Stop;
						Stop.Id = FName(*FString::Printf(TEXT("%s.port.node_%d_live_network_stop"), *N(Id), NodeIndex));
						Stop.OwnerId = Id;
						Stop.Kind = TEXT("live_network_stop_anchor");
						Stop.Location = FVector(Cell.Center.X, Cell.Center.Y, 0);
						District.Ports.Add(Stop);
					}
				}
			}

			// Per-node instanced landmark (registry instancing_policy).
			{
				const FVoidMeridianLandmarkEntry* Column = C.D.Landmarks.FindByPredicate([](const FVoidMeridianLandmarkEntry& L) { return L.Id == VoidDistrictNames::WeaveDisplayColumn; });
				if (Column)
				{
					const bool bPerNode = Column->InstancingPolicy.Contains(TEXT("per_network_node"));
					if (bPerNode || NodeIndex == 0)
					{
						FVoidPlannedLandmark* Landmark = AddLandmark(C, District, VoidDistrictNames::WeaveDisplayColumn, bPerNode ? FString::Printf(TEXT("node_%d"), NodeIndex) : FString(),
							FVector(Cells[0].Center.X, Cells[0].Center.Y, 0), FMath::RadiansToDegrees(A), FVector2D(150, 150), 1200.0, NAME_None, NodeIndex);
						(void)Landmark;
					}
				}
			}

			FVoidPlannedPort Connector;
			Connector.Id = FName(*FString::Printf(TEXT("%s.port.node_%d_ring_connector"), *N(Id), NodeIndex));
			Connector.OwnerId = Id;
			Connector.Kind = TEXT("ring_connector");
			const FVector2D ConnectorPoint = GridPoint(0, HR);
			Connector.Location = FVector(ConnectorPoint.X, ConnectorPoint.Y, 0);
			Connector.Connects = { VoidDistrictNames::MidTierRingRoad };
			District.Ports.Add(Connector);
		}

		// The district "boundary" of a distributed-node typology is the union of its node footprints; expose a bounding outline too.
		if (District.SubRegions.Num() > 0)
		{
			District.Boundary = District.SubRegions[0];
			District.Boundary.Id = Id;
		}
	}

	// =====================================================================
	// Metro Archives: singular bespoke facility at the seam.
	// =====================================================================
	struct FArchivesGeometry
	{
		double Angle = 0.0;
		FVoidBox2D Boundary;
		FVoidBox2D Building;
		FVoidBox2D Forecourt;
	};

	/** Shared by the Archives builder and the Undercroft seam road so both agree on where the sub-basement port is. */
	static FArchivesGeometry ComputeArchivesGeometry(const FLayoutCtx& C)
	{
		FArchivesGeometry G;
		const double SeamMid = (C.P.MidTierOuterRadius + C.P.SeamOuterRadius) * 0.5;
		G.Angle = Rad(C.P.SeamAngleDegrees);
		const FVector2D Center = C.Polar(SeamMid, G.Angle);
		const FVector2D U(FMath::Cos(G.Angle), FMath::Sin(G.Angle));
		const FVector2D V(-FMath::Sin(G.Angle), FMath::Cos(G.Angle));

		const double Half = C.P.ArchivesBoundaryHalfSize;
		const double BX = C.P.ArchivesBuildingHalfExtent.X;
		const double BY = C.P.ArchivesBuildingHalfExtent.Y;
		const double ForecourtStart = -(Half - 1000.0);
		const double ForecourtEnd = ForecourtStart + C.P.ArchivesForecourtDepth;
		const double BuildingLocalX = ForecourtEnd + 1000.0 + BX;

		auto Local = [&](double X, double Y) { return FVector2D(Center.X + U.X * X + V.X * Y, Center.Y + U.Y * X + V.Y * Y); };

		G.Boundary.Center = Center;
		G.Boundary.HalfExtent = FVector2D(Half, Half);
		G.Boundary.YawRadians = G.Angle;

		G.Building.Center = Local(BuildingLocalX, 0.0);
		G.Building.HalfExtent = FVector2D(BX, BY);
		G.Building.YawRadians = G.Angle;

		G.Forecourt.Center = Local((ForecourtStart + ForecourtEnd) * 0.5, 0.0);
		G.Forecourt.HalfExtent = FVector2D(C.P.ArchivesForecourtDepth * 0.5, BY);
		G.Forecourt.YawRadians = G.Angle;
		return G;
	}

	static void BuildMetroArchives(FLayoutCtx& C, const FVoidMeridianDistrictEntry& Entry)
	{
		FVoidPlannedDistrict& District = BeginDistrict(C, Entry);
		const FName Id = Entry.Id;

		const FArchivesGeometry G = ComputeArchivesGeometry(C);
		const double A = G.Angle;
		const double BX = C.P.ArchivesBuildingHalfExtent.X;
		const double BY = C.P.ArchivesBuildingHalfExtent.Y;

		District.Boundary = MakeBoxBoundary(Id, G.Boundary, 0.0);

		const FVoidBox2D& BuildingBox = G.Building;
		const FName BuildingId(*(N(Id) + TEXT(".bld.archives_building")));
		District.Buildings.Add(MakeBuilding(BuildingId.ToString(), Id, TEXT("archives_bespoke_building"), TEXT("MetroArchivesBuilding"), BuildingBox, C.P.ArchivesHeight, 0.0, true));

		District.PublicSpaces.Add(MakePublicSpace(N(Id) + TEXT(".ps.forecourt"), Id, EVoidPublicSpaceType::CivicSpace, G.Forecourt, INDEX_NONE));

		const double YawDeg = FMath::RadiansToDegrees(A);
		const FVector BuildingCenter(BuildingBox.Center.X, BuildingBox.Center.Y, 0);
		AddLandmark(C, District, VoidDistrictNames::MetroArchivesBuilding, FString(), BuildingCenter, YawDeg, FVector2D(BX, BY), C.P.ArchivesHeight, BuildingId, INDEX_NONE);
		AddLandmark(C, District, VoidDistrictNames::MetroArchivesReadingRoom, FString(), BuildingCenter, YawDeg, FVector2D::ZeroVector, 0.0, BuildingId, INDEX_NONE);
		{
			// East-wing exhibit case: registry navigation_importance says "east_wing_orientation"; world +X is east.
			AddLandmark(C, District, VoidDistrictNames::MetroArchivesExhibitCase, FString(), BuildingCenter + FVector(BX * 0.6, 0, 0), YawDeg, FVector2D::ZeroVector, 0.0, BuildingId, INDEX_NONE);
		}

		FVoidPlannedPort Seam;
		Seam.Id = FName(*(N(Id) + TEXT(".port.sub_basement_seam")));
		Seam.OwnerId = Id;
		Seam.Kind = TEXT("dead_network_seam");
		Seam.Location = FVector(BuildingBox.Center.X, BuildingBox.Center.Y, -C.P.BelowGradeAgingDepth);
		Seam.Connects = { VoidDistrictNames::UndercroftArchivesSeam };
		District.Ports.Add(Seam);
	}

	// =====================================================================
	// Undercroft: below-grade multi-ring substrate, three belts, dead network.
	// =====================================================================
	static void BuildUndercroft(FLayoutCtx& C, const FVoidMeridianDistrictEntry& Entry, bool bHasArchives, bool bHasSector0)
	{
		FVoidPlannedDistrict& District = BeginDistrict(C, Entry);
		const FName Id = Entry.Id;
		const double Z = -C.P.BelowGradeLivingDepth;

		const double Edges[4] = { C.P.CoreRadius, C.P.InnerRingsOuterRadius, C.P.MidTierOuterRadius, C.P.OuterRingsOuterRadius };

		TArray<FName> BeltNames;
		if (const FVoidMeridianRoute* Layer = C.D.FindRoute(FName(TEXT("undercroft_maintenance_layer"))))
		{
			BeltNames = Layer->SubBeltsServed;
		}
		if (BeltNames.Num() != 3)
		{
			C.Plan.Report.AddWarning(TEXT("RoadNetwork.json 'undercroft_maintenance_layer' does not list exactly three sub_belts_served; using registry-independent belt names."), TEXT("service_routes"), TEXT("VOID.District.UndercroftBeltNames"));
			BeltNames = { FName(TEXT("belt_inner")), FName(TEXT("belt_mid")), FName(TEXT("belt_outer")) };
		}

		District.Boundary = MakeCircleBoundary(Id, FVector2D::ZeroVector, Edges[3], Edges[0], Z);
		for (int32 Belt = 0; Belt < 3; ++Belt)
		{
			District.SubRegions.Add(MakeCircleBoundary(FName(*FString::Printf(TEXT("%s.%s"), *N(Id), *BeltNames[Belt].ToString())), FVector2D::ZeroVector, Edges[Belt + 1], Edges[Belt], Z));
		}

		// Dead-network maintenance layer. Service roads, below grade; never a variant of the surface hierarchy.
		TArray<double> SplitAngles = C.SpokeAngles;
		const double SeamAngle = Rad(C.P.SeamAngleDegrees);
		const double S0Angle = Rad(C.P.Sector0AngleDegrees);
		if (bHasArchives) { SplitAngles.Add(SeamAngle); }
		if (bHasSector0)  { SplitAngles.Add(S0Angle); }

		const double RingMid[3] = { (Edges[0] + Edges[1]) * 0.5, (Edges[1] + Edges[2]) * 0.5, (Edges[2] + Edges[3]) * 0.5 };
		const FName LayerRoute(TEXT("undercroft_maintenance_layer"));
		const TArray<FName> Serves = { Id };
		for (int32 Belt = 0; Belt < 3; ++Belt)
		{
			AddRingArcs(C, FString::Printf(TEXT("%s.maint_ring_%d"), *N(Id), Belt), RingMid[Belt], SplitAngles, Id, EVoidRoadType::Service, 400.0, Z, false, LayerRoute, Serves, District.Roads);
		}
		for (int32 Index = 0; Index < C.SpokeAngles.Num(); ++Index)
		{
			const double A = C.SpokeAngles[Index];
			for (int32 Belt = 0; Belt < 2; ++Belt)
			{
				District.Roads.Add(MakeRoad(FString::Printf(TEXT("%s.maint_tie_%d_%d"), *N(Id), Index, Belt), Id, EVoidRoadType::Service, 400.0,
					{ C.Polar(RingMid[Belt], A), C.Polar(RingMid[Belt + 1], A) }, Z, false, false, LayerRoute, Serves));
			}
		}

		const FVector2D ArchivesCenter = ComputeArchivesGeometry(C).Building.Center; // seam port XY: the Archives sub-basement
		if (bHasArchives)
		{
			// Shared pre-Council transit substrate seam: Undercroft outer ring <-> Metro Archives sub-basement.
			District.Roads.Add(MakeRoad(N(Id) + TEXT(".seam_to_metro_archives"), Id, EVoidRoadType::Service, 400.0,
				{ C.Polar(RingMid[2], SeamAngle), ArchivesCenter }, Z, false, false, VoidDistrictNames::UndercroftArchivesSeam, { Id, VoidDistrictNames::MetroArchives }));
			FVoidPlannedPort Port;
			Port.Id = FName(*(N(Id) + TEXT(".port.archives_seam")));
			Port.OwnerId = Id;
			Port.Kind = TEXT("dead_network_seam");
			Port.Location = FVector(ArchivesCenter.X, ArchivesCenter.Y, Z);
			Port.Connects = { VoidDistrictNames::UndercroftArchivesSeam };
			District.Ports.Add(Port);
		}

		if (bHasSector0)
		{
			// The single confirmed access point into Sector 0 (hard one-directional narrative gate).
			const FVector2D Threshold = C.Polar(Edges[3], S0Angle);
			District.Roads.Add(MakeRoad(N(Id) + TEXT(".threshold_to_sector_0"), Id, EVoidRoadType::Service, 400.0,
				{ C.Polar(RingMid[2], S0Angle), Threshold }, Z, false, false, VoidDistrictNames::UndercroftSector0Threshold, { Id, VoidDistrictNames::Sector0 }));
			FVoidPlannedPort Port;
			Port.Id = FName(*(N(Id) + TEXT(".port.sector_0_threshold")));
			Port.OwnerId = Id;
			Port.Kind = TEXT("dead_network_terminus_one_way_gate");
			Port.Location = FVector(Threshold.X, Threshold.Y, Z);
			Port.Connects = { VoidDistrictNames::UndercroftSector0Threshold };
			District.Ports.Add(Port);
		}

		// Landmarks: registry gives ids and navigation roles, not coordinates. Placeholder anchors, one per belt, flagged as such.
		const FName Riggs(TEXT("undercroft_riggs_safehouse"));
		const FName Yara(TEXT("undercroft_yaras_stall"));
		const FName Coen(TEXT("undercroft_coens_atrium"));
		const double Anchor = C.SpokeAngles.Num() > 0 ? C.SpokeAngles[0] + C.SpokeStep * 0.5 : 0.0;
		const FName Ids[3] = { Coen, Yara, Riggs }; // Riggs = "far stacks" (registry navigation_importance) -> outermost belt.
		for (int32 Belt = 0; Belt < 3; ++Belt)
		{
			const FVector2D P = C.Polar(RingMid[Belt] + 2000.0, Anchor);
			AddLandmark(C, District, Ids[Belt], FString(), FVector(P.X, P.Y, Z), 0.0, FVector2D::ZeroVector, 0.0, NAME_None, INDEX_NONE);
		}
		// Undercroft building fabric: not generated. The registry states density only qualitatively (High, informal) and
		// VOID_Location_01_Undercroft_data.json is not part of the delivered Meridian set; inventing structures is out of scope.
	}

	// =====================================================================
	// Sector 0: deepest terminus of the dead network.
	// =====================================================================
	static void BuildSector0(FLayoutCtx& C, const FVoidMeridianDistrictEntry& Entry, FVector2D& OutCenter)
	{
		FVoidPlannedDistrict& District = BeginDistrict(C, Entry);
		const FName Id = Entry.Id;
		const double Z = -C.P.AbsoluteBottomDepth;

		const double A = Rad(C.P.Sector0AngleDegrees);
		const double H = C.P.Sector0HalfSize;
		const FVector2D Center = C.Polar(C.P.OuterRingsOuterRadius + H * 0.9, A);
		OutCenter = Center;

		FVoidBox2D Box;
		Box.Center = Center;
		Box.HalfExtent = FVector2D(H, H);
		Box.YawRadians = A;
		District.Boundary = MakeBoxBoundary(Id, Box, Z);

		const FVector2D Threshold = C.Polar(C.P.OuterRingsOuterRadius, A);
		AddLandmark(C, District, VoidDistrictNames::Sector0Threshold, FString(), FVector(Threshold.X, Threshold.Y, Z), FMath::RadiansToDegrees(A), FVector2D::ZeroVector, 0.0, NAME_None, INDEX_NONE);
		AddLandmark(C, District, VoidDistrictNames::Sector0EchoRelay, FString(), FVector(Center.X, Center.Y, Z), FMath::RadiansToDegrees(A), FVector2D::ZeroVector, 0.0, NAME_None, INDEX_NONE);
		// Near-zero density by design: no buildings, no public spaces, no roads.
	}

	static void ApplyBoundaryOverrides(FLayoutCtx& C)
	{
		if (!C.In.Overrides)
		{
			return;
		}
		for (const FVoidMeridianBoundaryOverride& Override : C.In.Overrides->Boundaries)
		{
			FVoidPlannedDistrict* District = C.Plan.FindDistrict(Override.DistrictId);
			if (!District)
			{
				C.Plan.Report.AddWarning(
					FString::Printf(TEXT("Boundary override names district '%s', which is not in the plan; ignored."), *N(Override.DistrictId)),
					TEXT("overrides"), TEXT("VOID.District.OverrideUnknownDistrict"));
				continue;
			}
			District->Boundary.Polygon = Override.Polygon;
			FVoidDistrictGeometry::EnsureCounterClockwise(District->Boundary.Polygon);
			District->Boundary.HoleRadius = 0.0;
			District->Boundary.bFromOverride = true;
			District->SubRegions.Reset();
			C.Plan.Report.AddInfo(FString::Printf(TEXT("District '%s' boundary replaced by layout override; sub-regions cleared."), *N(Override.DistrictId)), TEXT("overrides"), TEXT("VOID.District.BoundaryOverridden"));
		}
	}
}

// ---------------------------------------------------------------------------

FVoidDistrictProfile FVoidDistrictLayoutBuilder::ResolveProfile(const FVoidMeridianDistrictEntry& Entry, const FVoidDistrictLayoutInput& Input, int32 LandmarkCount)
{
	using namespace VoidLayoutPrivate;

	const FVoidDistrictLayoutParams& P = Input.Params;
	FVoidDistrictProfile Profile;
	Profile.DistrictId = Entry.Id;
	Profile.StructuralModel = Entry.StructuralModel;
	Profile.RadialBand = Entry.RadialBand;
	Profile.VerticalTier = Entry.VerticalTier;
	Profile.LandmarkCount = LandmarkCount;
	Profile.StoryHeightUnits = P.StoryHeight;

	auto Note = [&Profile](const FString& Line) { Profile.Provenance.Add(Line); };

	FString DensitySource;
	if (LookupDensityBand(Entry.Id, Profile.Density, DensitySource))
	{
		Note(FString::Printf(TEXT("density = %s (%s)"), DensityName(Profile.Density), *DensitySource));
	}
	else
	{
		Profile.Density = EVoidDistrictDensity::Moderate;
		Note(TEXT("density = Moderate (Default: district not in Master Plan Sec. 22 table)"));
	}

	switch (Profile.Density)
	{
		case EVoidDistrictDensity::High:     Profile.BuildingCoverage = P.CoverageHigh;     Profile.OpenSpaceRatio = 0.15; Profile.VegetationDensity = 0.25; break;
		case EVoidDistrictDensity::Moderate: Profile.BuildingCoverage = P.CoverageModerate; Profile.OpenSpaceRatio = 0.25; Profile.VegetationDensity = 0.35; break;
		case EVoidDistrictDensity::Sparse:   Profile.BuildingCoverage = P.CoverageSparse;   Profile.OpenSpaceRatio = 0.55; Profile.VegetationDensity = 0.50; break;
		default:                             Profile.BuildingCoverage = P.CoverageNearZero; Profile.OpenSpaceRatio = 0.95; Profile.VegetationDensity = 0.0;  break;
	}
	Note(FString::Printf(TEXT("buildingCoverage = %.2f, openSpaceRatio = %.2f, vegetationDensity = %.2f (Default: density-band table)"), Profile.BuildingCoverage, Profile.OpenSpaceRatio, Profile.VegetationDensity));

	Profile.BlockSizeUnits = P.BlockSize;
	Profile.PlazaCellsPerNode = 1;
	Profile.MinStories = 1;
	Profile.MaxStories = 4;

	if (Entry.Id == VoidDistrictNames::WhiteZones)
	{
		Profile.MinStories = 2;
		Profile.MaxStories = 4;
		Note(TEXT("stories = 2..4 (Meridian_Master_Plan.md vertical tiers: White Zones 2-4 stories, hard ceiling)"));
	}
	else if (Entry.Id == VoidDistrictNames::MetroArchives)
	{
		const int32 Stories = FMath::Max(1, static_cast<int32>(FMath::RoundToInt(static_cast<float>(P.ArchivesHeight / P.StoryHeight))));
		Profile.MinStories = Stories;
		Profile.MaxStories = Stories;
		Note(TEXT("stories = mid-rise (LandmarkRegistry.json visibility tier 3 'bespoke_individually_designed_mid_rise'); height from params.ArchivesHeight"));
	}
	else
	{
		Note(TEXT("stories = 1..4 (Default; no story data for this district)"));
	}

	// Commercial / residential balance: only where the district's own data says something.
	if (Entry.bCommercialPresence.IsSet())
	{
		Profile.CommercialShare = Entry.bCommercialPresence.GetValue() ? 0.5 : 0.0;
		Note(FString::Printf(TEXT("commercialShare = %s (%s economic_band.commercial_presence = %s)"),
			Entry.bCommercialPresence.GetValue() ? TEXT("0.5, magnitude unspecified by data") : TEXT("0.0"), *Entry.DistrictDataFile, Entry.bCommercialPresence.GetValue() ? TEXT("true") : TEXT("false")));
	}
	else
	{
		Profile.CommercialShare = -1.0;
		Note(TEXT("commercialShare = unknown (data silent: district data file not present or has no economic_band)"));
	}

	if (const FVoidDistrictCharacterOverride* Override = Input.CharacterOverrides.Find(Entry.Id))
	{
		if (Override->OpenSpaceRatio >= 0.0) { Profile.OpenSpaceRatio = Override->OpenSpaceRatio; Note(FString::Printf(TEXT("openSpaceRatio = %.2f (Settings character override)"), Profile.OpenSpaceRatio)); }
		if (Override->VegetationDensity >= 0.0) { Profile.VegetationDensity = Override->VegetationDensity; Note(FString::Printf(TEXT("vegetationDensity = %.2f (Settings character override)"), Profile.VegetationDensity)); }
		if (Override->BlockSize > 0.0) { Profile.BlockSizeUnits = Override->BlockSize; Note(FString::Printf(TEXT("blockSize = %.0f (Settings character override)"), Profile.BlockSizeUnits)); }
		if (Override->CommercialShare >= 0.0) { Profile.CommercialShare = Override->CommercialShare; Note(FString::Printf(TEXT("commercialShare = %.2f (Settings character override)"), Profile.CommercialShare)); }
		if (Override->MinStories > 0) { Profile.MinStories = Override->MinStories; Note(FString::Printf(TEXT("minStories = %d (Settings character override)"), Profile.MinStories)); }
		if (Override->MaxStories > 0) { Profile.MaxStories = Override->MaxStories; Note(FString::Printf(TEXT("maxStories = %d (Settings character override)"), Profile.MaxStories)); }
		if (Override->PlazaCellsPerNode > 0) { Profile.PlazaCellsPerNode = Override->PlazaCellsPerNode; Note(FString::Printf(TEXT("plazaCellsPerNode = %d (Settings character override)"), Profile.PlazaCellsPerNode)); }
	}
	if (Profile.MaxStories < Profile.MinStories) { Profile.MaxStories = Profile.MinStories; }

	Note(FString::Printf(TEXT("landmarkCount = %d (LandmarkRegistry.json)"), LandmarkCount));
	return Profile;
}

FVoidDistrictLayoutPlan FVoidDistrictLayoutBuilder::Build(const FVoidDistrictLayoutInput& Input)
{
	using namespace VoidLayoutPrivate;

	FVoidDistrictLayoutPlan Plan;
	Plan.Report.bIsValid = true;

	if (!Input.Data || Input.Data->Districts.Num() == 0)
	{
		Plan.Report.AddFatal(TEXT("No Meridian district data supplied."), TEXT("data"), TEXT("VOID.District.NoData"), TEXT("Load Meridian_Master.json and its registries with FVoidMeridianRegistryReader first."));
		return Plan;
	}

	const FVoidDistrictLayoutParams& P = Input.Params;
	if (!(P.CoreRadius < P.InnerRingsOuterRadius && P.InnerRingsOuterRadius < P.MidTierOuterRadius && P.MidTierOuterRadius < P.SeamOuterRadius && P.SeamOuterRadius < P.OuterRingsOuterRadius))
	{
		Plan.Report.AddFatal(TEXT("Radial band radii must be strictly increasing (core < inner < mid < seam < outer)."), TEXT("params"), TEXT("VOID.District.BadRadii"));
		return Plan;
	}
	if (P.SpokeCount < 1)
	{
		Plan.Report.AddFatal(TEXT("SpokeCount must be at least 1."), TEXT("params.SpokeCount"), TEXT("VOID.District.BadSpokeCount"));
		return Plan;
	}

	FLayoutCtx C(Input, Plan);
	C.SpokeStep = TwoPi / static_cast<double>(P.SpokeCount);
	for (int32 Index = 0; Index < P.SpokeCount; ++Index)
	{
		C.SpokeAngles.Add(Rad(P.SpokeBaseAngleDegrees) + C.SpokeStep * Index);
	}
	C.MidRing = (P.InnerRingsOuterRadius + P.MidTierOuterRadius) * 0.5;
	C.SpireRing = P.BasePlazaRadius + 1500.0;
	C.ArcStepRad = Rad(P.ArcStepDegrees);
	if (const FVoidMeridianRoute* Spine = C.D.FindRoute(VoidDistrictNames::SpireRadialSpine)) { C.SpineRouteId = Spine->Id; }

	if (P.BasePlazaRadius >= P.CoreRadius * 0.6)
	{
		Plan.Report.AddFatal(TEXT("BasePlazaRadius leaves no room for elite towers inside the core band."), TEXT("params.BasePlazaRadius"), TEXT("VOID.District.PlazaTooLarge"));
		return Plan;
	}

	// Macro order: Meridian_Master.json generation_order, mapping the two non-district step ids to their districts.
	static const TCHAR* StepAliases[][2] =
	{
		{ TEXT("white_zones_network_nodes"), TEXT("white_zones") },
		{ TEXT("undercroft_substrate"), TEXT("undercroft") },
	};
	TArray<FName> Order;
	auto AddOrdered = [&](FName Id) { if (Id != NAME_None && C.D.FindDistrict(Id) && !Order.Contains(Id)) { Order.Add(Id); } };
	for (const FName& Step : C.D.MacroGenerationOrder)
	{
		FName Target = Step;
		for (const TCHAR* const* Alias : StepAliases)
		{
			if (Step == FName(Alias[0])) { Target = FName(Alias[1]); }
		}
		AddOrdered(Target);
	}
	{
		TArray<const FVoidMeridianDistrictEntry*> Rest;
		for (const FVoidMeridianDistrictEntry& Entry : C.D.Districts) { if (!Order.Contains(Entry.Id)) { Rest.Add(&Entry); } }
		Algo::Sort(Rest, [](const FVoidMeridianDistrictEntry* L, const FVoidMeridianDistrictEntry* R) { return L->GenerationPriority < R->GenerationPriority; }); // Algo::Sort passes the pointers themselves; TArray::Sort would dereference pointer elements
		for (const FVoidMeridianDistrictEntry* Entry : Rest) { Order.Add(Entry->Id); }
	}

	const bool bHasArchives = C.D.FindDistrict(VoidDistrictNames::MetroArchives) != nullptr;
	const bool bHasSector0 = C.D.FindDistrict(VoidDistrictNames::Sector0) != nullptr;

	BuildLiveNetwork(C);

	for (const FName& Id : Order)
	{
		const FVoidMeridianDistrictEntry* Entry = C.D.FindDistrict(Id);
		if (!Entry) { continue; }

		if (Id == VoidDistrictNames::OlympusSpire)        { BuildSpire(C, *Entry); }
		else if (Id == VoidDistrictNames::WhiteZones)     { BuildWhiteZones(C, *Entry); }
		else if (Id == VoidDistrictNames::MetroArchives)  { BuildMetroArchives(C, *Entry); }
		else if (Id == VoidDistrictNames::Sector0)        { FVector2D Unused; BuildSector0(C, *Entry, Unused); }
		else if (Id == VoidDistrictNames::Undercroft)
		{
			// Undercroft's roads reference the Archives port and Sector 0's angle from shared params, so build order between them does not matter.
			BuildUndercroft(C, *Entry, bHasArchives, bHasSector0);
		}
		else
		{
			Plan.Report.AddWarning(
				FString::Printf(TEXT("District '%s' is in DistrictRegistry.json but has no layout rule in the District Generator; skipped. (No sixth district is invented.)"), *N(Id)),
				TEXT("districts"), TEXT("VOID.District.NoLayoutRule"));
		}
	}

	ApplyBoundaryOverrides(C);

	// Every registry landmark should have been placed.
	for (const FVoidMeridianLandmarkEntry& Landmark : C.D.Landmarks)
	{
		if (!C.PlacedLandmarkRegistryIds.Contains(Landmark.Id))
		{
			Plan.Report.AddWarning(
				FString::Printf(TEXT("Registry landmark '%s' (district '%s') was not placed: no placement rule."), *N(Landmark.Id), *N(Landmark.DistrictId)),
				TEXT("landmarks"), TEXT("VOID.District.LandmarkUnplaced"));
		}
	}

	return Plan;
}

bool FVoidDistrictLayoutBuilder::ApplyExplicitPackage(FVoidDistrictLayoutPlan& InOutPlan, const FVoidDesignPackage& Package)
{
	FVoidPlannedDistrict* District = InOutPlan.FindDistrict(Package.District.DistrictId.Value);
	if (!District)
	{
		return false;
	}
	if (Package.District.Roads.Num() == 0 && Package.District.Buildings.Num() == 0)
	{
		return false;
	}

	if (Package.District.Roads.Num() > 0)
	{
		District->Roads.Reset();
		for (const FVoidRoadSpec& Spec : Package.District.Roads)
		{
			FVoidPlannedRoad Road;
			Road.Spec = Spec;
			Road.Spec.Id = FVoidElementId(FName(*FString::Printf(TEXT("%s.%s"), *District->Id.ToString(), *Spec.Id.Value.ToString())));
			Road.OwnerId = District->Id;
			Road.bFreeEndpointsAllowed = true; // Explicit data is authoritative; free ends are the author's intent.
			District->Roads.Add(Road);
		}
	}

	if (Package.District.Buildings.Num() > 0)
	{
		District->Buildings.Reset();
		for (const FVoidBuildingSpec& Spec : Package.District.Buildings)
		{
			if (Spec.FootprintCorners.Num() < 3) { continue; }
			FVector2D Min(1e300, 1e300), Max(-1e300, -1e300);
			for (const FVector2D& Corner : Spec.FootprintCorners)
			{
				Min = FVector2D(FMath::Min(Min.X, Corner.X), FMath::Min(Min.Y, Corner.Y));
				Max = FVector2D(FMath::Max(Max.X, Corner.X), FMath::Max(Max.Y, Corner.Y));
			}
			FVoidPlannedBuilding Building;
			Building.Spec = Spec;
			Building.Spec.Id = FVoidElementId(FName(*FString::Printf(TEXT("%s.%s"), *District->Id.ToString(), *Spec.Id.Value.ToString())));
			Building.OwnerId = District->Id;
			Building.CategoryId = FName(*Spec.BuildingType);
			Building.Center = FVector2D((Min.X + Max.X) * 0.5, (Min.Y + Max.Y) * 0.5);
			Building.HalfExtent = FVector2D((Max.X - Min.X) * 0.5, (Max.Y - Min.Y) * 0.5);
			District->Buildings.Add(Building);
		}
	}

	District->bFromExplicitPackage = true;
	return true;
}
