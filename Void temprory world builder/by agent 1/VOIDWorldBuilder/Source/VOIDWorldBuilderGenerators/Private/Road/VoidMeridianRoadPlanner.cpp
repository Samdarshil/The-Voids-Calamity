// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Road/VoidMeridianRoadPlanner.h"

FVoidMeridianLayout FVoidMeridianLayout::FromSettings(const UVoidWorldBuilderSettings* Settings)
{
	FVoidMeridianLayout L;
	if (Settings)
	{
		L.BandRadii = Settings->BandRadii;
		L.DefaultRadialAzimuthDegrees = Settings->DefaultRadialAzimuthDegrees;
		L.RouteAzimuthDegrees = Settings->RouteAzimuthDegrees;
		L.RingRoadSegments = Settings->RingRoadSegments;
		L.RadialSampleSpacingUU = Settings->RadialSampleSpacingUU;
		L.bAllowFlaggedContent = Settings->bAllowFlaggedContent;
	}
	return L;
}

bool FVoidMeridianLayout::FindBand(FName BandId, double& OutInner, double& OutOuter) const
{
	for (const FVoidBandRadius& B : BandRadii)
	{
		if (B.BandId == BandId) { OutInner = B.InnerRadiusUU; OutOuter = B.OuterRadiusUU; return true; }
	}
	return false;
}

double FVoidMeridianLayout::GetAzimuthFor(FName RouteId) const
{
	const double* Override = RouteAzimuthDegrees.Find(RouteId);
	return Override ? *Override : DefaultRadialAzimuthDegrees;
}

EVoidRoadType FVoidMeridianRoadPlanner::MapTierToRoadType(int32 Tier)
{
	switch (Tier)
	{
		case 1:  return EVoidRoadType::Primary;
		case 2:  return EVoidRoadType::Secondary;
		default: return EVoidRoadType::Service;
	}
}

namespace VoidMeridianPlannerPrivate
{
	static const FName CodeNoRadius(TEXT("VOID.RoadPlan.MissingBandRadius"));
	static const FName CodeCoord(TEXT("VOID.RoadPlan.CoordinateRejected"));
	static const FName CodeConstraint(TEXT("VOID.RoadPlan.HardConstraintViolated"));
	static const FName CodeTopology(TEXT("VOID.RoadPlan.TopologyOnly"));

	static void AddTopologyOnly(FVoidMeridianRoadPlan& Plan, const FVoidMeridianRoute& Route, const FString& Reason)
	{
		FVoidTopologyOnlyRoute T;
		T.Id = Route.Id; T.CategoryId = Route.CategoryId; T.HierarchyTier = Route.HierarchyTier;
		T.Network = Route.Network; T.ServedDistricts = Route.ServedDistricts; T.Reason = Reason;
		Plan.TopologyOnly.Add(MoveTemp(T));
	}
}

bool FVoidMeridianRoadPlanner::BuildPlan(const FVoidMeridianWorld& World, const FVoidWorldSpace& WorldSpace, const FVoidMeridianLayout& Layout, FVoidMeridianRoadPlan& OutPlan, FVoidValidationReport& OutReport)
{
	using namespace VoidMeridianPlannerPrivate;

	OutPlan = FVoidMeridianRoadPlan();
	const FVoidMeridianRoadNetwork& RN = World.RoadNetwork;

	OutReport.AddWarning(TEXT("Road geometry is derived from the Builder's placeholder band layout (UVoidWorldBuilderSettings), not from Meridian data - Meridian specifies no coordinates or distances."),
		TEXT("layout"), TEXT("VOID.RoadPlan.PlaceholderLayout"), TEXT("Replace UVoidWorldBuilderSettings::BandRadii with design-approved values when available."));
	OutReport.AddInfo(TEXT("Meridian specifies no widths, sidewalks, curbs, crosswalks, lane counts or intersections; roads use the Builder's default per-type profile width and none of the others are generated."),
		TEXT("RoadNetwork"), TEXT("VOID.RoadPlan.UnspecifiedByMeridian"));

	// Pass 1: build each Live route that has a geometry rule.
	for (const FVoidMeridianRoute& Route : RN.Routes)
	{
		if (!Route.HasSurfaceGeometry())
		{
			AddTopologyOnly(OutPlan, Route, TEXT("Dead Network route: Meridian gives it no surface footprint or position."));
			continue;
		}

		const bool bRadial = (Route.CategoryId == FName(TEXT("radial_arterial")));
		const bool bRing = (Route.CategoryId == FName(TEXT("ring_road")));
		if (!bRadial && !bRing)
		{
			AddTopologyOnly(OutPlan, Route, FString::Printf(TEXT("No geometry rule exists for category '%s'."), *Route.CategoryId.ToString()));
			OutReport.AddWarning(FString::Printf(TEXT("Route '%s' has category '%s', which the Road Generator has no geometry rule for; recorded as topology only."), *Route.Id.ToString(), *Route.CategoryId.ToString()),
				Route.Id.ToString(), CodeTopology, TEXT("Add a rule to FVoidMeridianRoadPlanner only if Meridian describes what that category looks like."));
			continue;
		}

		if (Route.Bands.Num() == 0)
		{
			OutReport.AddError(FString::Printf(TEXT("Route '%s' names no radial band; cannot place it."), *Route.Id.ToString()), Route.Id.ToString(), CodeNoRadius, TEXT("Fix RoadNetwork.json."));
			continue;
		}

		// Resolve band extents.
		double MinR = TNumericLimits<double>::Max(), MaxR = 0.0;
		bool bBandsOk = true;
		for (const FName Band : Route.Bands)
		{
			double In = 0, Out = 0;
			if (!Layout.FindBand(Band, In, Out) || Out <= In)
			{
				OutReport.AddError(FString::Printf(TEXT("Route '%s' crosses band '%s' but no valid radius is configured for it."), *Route.Id.ToString(), *Band.ToString()),
					Route.Id.ToString(), CodeNoRadius, TEXT("Add the band to UVoidWorldBuilderSettings::BandRadii with OuterRadius > InnerRadius."));
				bBandsOk = false;
				break;
			}
			MinR = FMath::Min(MinR, In); MaxR = FMath::Max(MaxR, Out);
		}
		if (!bBandsOk) { continue; }

		FVoidPlannedRoad Planned;
		Planned.CategoryId = Route.CategoryId;
		Planned.HierarchyTier = Route.HierarchyTier;
		Planned.Network = Route.Network;
		Planned.ServedDistricts = Route.ServedDistricts;
		Planned.bIsRing = bRing;
		Planned.Spec.Id = FVoidElementId(Route.Id);
		Planned.Spec.RoadType = MapTierToRoadType(Route.HierarchyTier);
		// WidthUnits / LaneCount deliberately left 0: "not specified" -> Builder default profile (flagged downstream).
		Planned.Spec.bHasSidewalk = false;
		Planned.Spec.bHasMedian = false;

		TArray<FVector> Points;
		FString Err;
		bool bOk = true;

		if (bRadial)
		{
			const double Az = Layout.GetAzimuthFor(Route.Id);
			Planned.AzimuthDegrees = Az; Planned.MinRadiusUU = MinR; Planned.MaxRadiusUU = MaxR;
			const double Span = MaxR - MinR;
			const int32 NumSeg = FMath::Max(1, FMath::CeilToInt(Span / FMath::Max(Layout.RadialSampleSpacingUU, 1.0)));
			for (int32 i = 0; i <= NumSeg && bOk; ++i)
			{
				FVector P;
				bOk = WorldSpace.TryFromPolar(MinR + Span * (double(i) / NumSeg), Az, 0.0, P, &Err);
				if (bOk) { Points.Add(P); }
			}
		}
		else
		{
			const double Radius = 0.5 * (MinR + MaxR);
			// Hard constraint (opaque token in Meridian): the ring must never route through a district it names.
			for (const FVoidMeridianDistrict& D : World.Districts)
			{
				if (Route.HardConstraint.Contains(D.Id.ToString()) && Route.Bands.Contains(D.RadialBand))
				{
					OutReport.AddError(FString::Printf(TEXT("Ring road '%s' is placed in band '%s', which is district '%s' - violating its hard constraint '%s'."), *Route.Id.ToString(), *D.RadialBand.ToString(), *D.Id.ToString(), *Route.HardConstraint),
						Route.Id.ToString(), CodeConstraint, TEXT("Fix the route's band in RoadNetwork.json."));
					bOk = false;
				}
			}

			// Vertex 0 sits on the azimuth of the route this ring depends on, so their crossing lands exactly on a ring vertex.
			double StartAz = 0.0;
			if (const FVoidMeridianRoute* Dep = RN.FindRoute(Route.GenerationDependency))
			{
				if (Dep->CategoryId == FName(TEXT("radial_arterial"))) { StartAz = Layout.GetAzimuthFor(Dep->Id); }
			}
			Planned.AzimuthDegrees = StartAz; Planned.MinRadiusUU = Planned.MaxRadiusUU = Radius;
			Planned.Spec.bClosedLoop = true;
			const int32 NumSeg = FMath::Max(8, Layout.RingRoadSegments);
			for (int32 i = 0; i < NumSeg && bOk; ++i)
			{
				FVector P;
				bOk = WorldSpace.TryFromPolar(Radius, StartAz + 360.0 * double(i) / NumSeg, 0.0, P, &Err);
				if (bOk) { Points.Add(P); }
			}
		}

		if (!bOk)
		{
			if (!Err.IsEmpty())
			{
				OutReport.AddError(FString::Printf(TEXT("Route '%s' rejected: %s"), *Route.Id.ToString(), *Err), Route.Id.ToString(), CodeCoord,
					TEXT("Reduce the band radii or raise UVoidWorldBuilderSettings::MaxAbsCoordinateUU deliberately."));
			}
			continue;
		}

		for (const FVector& P : Points) { Planned.Spec.CenterlinePoints.Add(FVector2D(P.X, P.Y)); }
		Planned.Spec.ElevationUnits = static_cast<float>(Points.Num() > 0 ? Points[0].Z : 0.0);
		OutPlan.Roads.Add(MoveTemp(Planned));
	}

	// Pass 2: analytic crossings between a radial road and a ring road it geometrically spans.
	for (const FVoidPlannedRoad& Radial : OutPlan.Roads)
	{
		if (Radial.bIsRing) { continue; }
		for (const FVoidPlannedRoad& Ring : OutPlan.Roads)
		{
			if (!Ring.bIsRing) { continue; }
			if (Ring.MinRadiusUU >= Radial.MinRadiusUU && Ring.MinRadiusUU <= Radial.MaxRadiusUU)
			{
				FVector Loc;
				if (WorldSpace.TryFromPolar(Ring.MinRadiusUU, Radial.AzimuthDegrees, 0.0, Loc))
				{
					FVoidPlannedCrossing C;
					C.RoadA = Radial.Spec.Id.Value; C.RoadB = Ring.Spec.Id.Value; C.Location = Loc;
					OutPlan.Crossings.Add(C);
				}
			}
		}
	}

	// Tunnels are topology only; bridges are pedestrian skybridges between buildings, not roads.
	for (const FVoidMeridianTunnel& T : RN.Tunnels)
	{
		FVoidTopologyOnlyRoute R;
		R.Id = T.Id; R.Network = EVoidNetworkKind::Dead; R.ServedDistricts = T.Connects;
		R.Reason = TEXT("Tunnel relationship: Meridian gives no route, depth or position - topology only.");
		OutPlan.TopologyOnly.Add(MoveTemp(R));
	}
	for (const FVoidMeridianBridge& B : RN.Bridges)
	{
		if (B.bFlaggedForSignoff && !Layout.bAllowFlaggedContent)
		{
			OutPlan.SkippedFlagged.Add(B.Id);
		}
		OutReport.AddInfo(FString::Printf(TEXT("Bridge '%s' (%s) connects buildings, not roads; not generated by the Road Generator."), *B.Id.ToString(), *B.Type.ToString()),
			B.Id.ToString(), TEXT("VOID.RoadPlan.BridgeNotARoad"));
	}

	return OutPlan.Roads.Num() > 0;
}
