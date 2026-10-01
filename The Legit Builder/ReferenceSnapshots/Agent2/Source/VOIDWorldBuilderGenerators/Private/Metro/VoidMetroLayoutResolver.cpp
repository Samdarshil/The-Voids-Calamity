// Copyright VOID Engineering Studio. Internal tool - not for shipping content.
// Added by Agent 2 (Phase 6 Metro Generator).

#include "Metro/VoidMetroLayoutResolver.h"
#include "Metro/VoidMetroGenerationSettings.h"

namespace VoidMetroResolverPrivate
{
	static constexpr float PortalZThreshold = -1.0f;

	static FVector2D PolarToXY(float Radius, float AzimuthDegrees)
	{
		const float Rad = FMath::DegreesToRadians(AzimuthDegrees);
		return FVector2D(Radius * FMath::Cos(Rad), Radius * FMath::Sin(Rad));
	}

	static EVoidMetroGrade StationGrade(const FVoidMetroStationSpec& S, const FVoidMetroLayoutParams& P)
	{
		if (S.bHasGradeOverride) { return S.Grade; }
		return S.Network == EVoidMetroNetworkKind::Dead ? EVoidMetroGrade::Underground : P.LiveDefaultGrade;
	}

	static EVoidMetroGrade LineGrade(const FVoidMetroLineSpec& L, const FVoidMetroLayoutParams& P)
	{
		if (L.bHasGradeOverride) { return L.Grade; }
		return L.Network == EVoidMetroNetworkKind::Dead ? EVoidMetroGrade::Underground : P.LiveDefaultGrade;
	}

	/** Derives a placeholder XY for a station from its district's band radius + azimuth. */
	static bool TryPlaceholderPosition(const FVoidMetroStationSpec& S, const FVoidMetroLayoutParams& P, TMap<FName, int32>& DistrictOrdinal, FVector2D& OutXY)
	{
		const FName* Band = P.DistrictBand.Find(S.DistrictId);
		if (!Band) { return false; }
		const float* Radius = P.BandRadiusUnits.Find(*Band);
		const float* Azimuth = P.DistrictAzimuthDegrees.Find(S.DistrictId);
		if (!Radius || !Azimuth) { return false; }

		int32& Ordinal = DistrictOrdinal.FindOrAdd(S.DistrictId);
		const float Angle = *Azimuth + static_cast<float>(Ordinal) * P.NodeAngularSpacingDegrees;
		++Ordinal;
		OutXY = PolarToXY(*Radius, Angle);
		return true;
	}

	static float Radius2D(const FVector& V) { return FMath::Sqrt(V.X * V.X + V.Y * V.Y); }
	static float AzimuthDegrees2D(const FVector& V) { return FMath::RadiansToDegrees(FMath::Atan2(V.Y, V.X)); }

	static FVector Lift(const FVector2D& XY, float Z) { return FVector(XY.X, XY.Y, Z); }
}

// -----------------------------------------------------------------------------
// Params
// -----------------------------------------------------------------------------

FVoidMetroLayoutParams FVoidMetroLayoutParams::MakeBuiltInDefaults()
{
	FVoidMetroLayoutParams P;
	P.BandRadiusUnits = {
		{ TEXT("core"), 0.0f },
		{ TEXT("inner_rings_unbuilt"), 200000.0f },
		{ TEXT("mid_tier_rings"), 400000.0f },
		{ TEXT("seam_zone"), 550000.0f },
		{ TEXT("outer_rings"), 700000.0f },
		{ TEXT("off_gradient"), 800000.0f },
		{ TEXT("inner_mid_outer_continuous"), 400000.0f },
	};
	P.DistrictBand = {
		{ TEXT("olympus_spire"), TEXT("core") },
		{ TEXT("white_zones"), TEXT("mid_tier_rings") },
		{ TEXT("metro_archives"), TEXT("seam_zone") },
		{ TEXT("undercroft"), TEXT("inner_mid_outer_continuous") },
		{ TEXT("sector_0"), TEXT("off_gradient") },
	};
	P.DistrictAzimuthDegrees = {
		{ TEXT("olympus_spire"), 0.0f },
		{ TEXT("white_zones"), 0.0f },
		{ TEXT("undercroft"), 200.0f },
		{ TEXT("metro_archives"), 130.0f },
		{ TEXT("sector_0"), 260.0f },
	};
	return P;
}

FVoidMetroLayoutParams FVoidMetroLayoutParams::FromSettings(const UVoidMetroGenerationSettings* S)
{
	FVoidMetroLayoutParams P = MakeBuiltInDefaults();
	if (!S) { return P; }
	P.bAllowPlaceholderLayout = S->bAllowPlaceholderLayout;
	P.BandRadiusUnits = S->BandRadiusUnits;
	P.DistrictBand = S->DistrictBand;
	P.DistrictAzimuthDegrees = S->DistrictAzimuthDegrees;
	P.NodeAngularSpacingDegrees = S->NodeAngularSpacingDegrees;
	P.RingSampleCount = S->RingSampleCount;
	P.LiveDefaultGrade = S->LiveNetworkDefaultGrade;
	P.ElevatedHeightUnits = S->ElevatedHeightUnits;
	P.UndergroundDepthUnits = S->UndergroundDepthUnits;
	P.GradeRampLengthUnits = S->GradeRampLengthUnits;
	P.MaxSegmentLengthUnits = S->MaxSegmentLengthUnits;
	P.DefaultEntrancesPerStation = S->DefaultEntrancesPerStation;
	P.EntranceOffsetUnits = S->EntranceOffsetUnits;
	return P;
}

float FVoidMetroLayoutParams::GradeZ(EVoidMetroGrade Grade) const
{
	switch (Grade)
	{
	case EVoidMetroGrade::Elevated:    return ElevatedHeightUnits;
	case EVoidMetroGrade::Underground: return -UndergroundDepthUnits;
	default:                           return 0.0f;
	}
}

const FVoidMetroResolvedStation* FVoidMetroResolvedLayout::FindStation(FName Id) const
{
	return Stations.FindByPredicate([Id](const FVoidMetroResolvedStation& S) { return S.Id == Id; });
}

// -----------------------------------------------------------------------------
// Geometry helpers
// -----------------------------------------------------------------------------

TArray<FVector> FVoidMetroLayoutResolver::Densify(const TArray<FVector>& Points, float MaxLength, bool bClosed)
{
	TArray<FVector> Out;
	const int32 N = Points.Num();
	if (N < 2 || MaxLength < 1.0f)
	{
		return Points;
	}
	Out.Reserve(N);

	const int32 NumSpans = bClosed ? N : N - 1;
	for (int32 i = 0; i < NumSpans; ++i)
	{
		const FVector& A = Points[i];
		const FVector& B = Points[(i + 1) % N];
		Out.Add(A);
		const float Len = FVector::Dist(A, B);
		const int32 Pieces = FMath::Max(1, FMath::CeilToInt(Len / MaxLength));
		for (int32 k = 1; k < Pieces; ++k)
		{
			Out.Add(FMath::Lerp(A, B, static_cast<float>(k) / static_cast<float>(Pieces)));
		}
	}
	if (!bClosed)
	{
		Out.Add(Points.Last());
	}
	return Out;
}

void FVoidMetroLayoutResolver::ApplyGradeRamps(TArray<FVector>& Points, float BodyZ, float StartZ, float EndZ, float RampLength)
{
	const int32 N = Points.Num();
	if (N < 2)
	{
		return;
	}

	TArray<float> Dist;
	Dist.SetNumUninitialized(N);
	Dist[0] = 0.0f;
	for (int32 i = 1; i < N; ++i)
	{
		Dist[i] = Dist[i - 1] + FVector::Dist2D(Points[i - 1], Points[i]);
	}
	const float Total = Dist.Last();
	const float Ramp = FMath::Max(RampLength, 1.0f);

	for (int32 i = 0; i < N; ++i)
	{
		float Z = BodyZ;
		const float FromStart = FMath::Clamp(Dist[i] / Ramp, 0.0f, 1.0f);
		const float FromEnd = FMath::Clamp((Total - Dist[i]) / Ramp, 0.0f, 1.0f);
		// Smoothstep so the grade eases in/out instead of kinking.
		const float WS = 1.0f - FMath::SmoothStep(0.0f, 1.0f, FromStart);
		const float WE = 1.0f - FMath::SmoothStep(0.0f, 1.0f, FromEnd);
		Z += (StartZ - BodyZ) * WS;
		Z += (EndZ - BodyZ) * WE;
		Points[i].Z = Z;
	}
}

void FVoidMetroLayoutResolver::FindPortalCrossings(const TArray<FVector>& Points, TArray<FVector>& OutLocations, TArray<float>& OutYawDegrees)
{
	using namespace VoidMetroResolverPrivate;
	for (int32 i = 0; i + 1 < Points.Num(); ++i)
	{
		const FVector& A = Points[i];
		const FVector& B = Points[i + 1];
		const bool bADown = A.Z < PortalZThreshold;
		const bool bBDown = B.Z < PortalZThreshold;
		if (bADown == bBDown)
		{
			continue;
		}
		const float T = FMath::Clamp((PortalZThreshold - A.Z) / (B.Z - A.Z), 0.0f, 1.0f);
		FVector At = FMath::Lerp(A, B, T);
		At.Z = 0.0f;
		OutLocations.Add(At);
		OutYawDegrees.Add(FMath::RadiansToDegrees(FMath::Atan2(B.Y - A.Y, B.X - A.X)));
	}
}

void FVoidMetroLayoutResolver::FindElevatedRuns(const TArray<FVector>& Points, float Threshold, TArray<TPair<FVector, FVector>>& OutRuns)
{
	int32 RunStart = INDEX_NONE;
	for (int32 i = 0; i < Points.Num(); ++i)
	{
		const bool bUp = Points[i].Z > Threshold;
		if (bUp && RunStart == INDEX_NONE)
		{
			RunStart = i;
		}
		if ((!bUp || i == Points.Num() - 1) && RunStart != INDEX_NONE)
		{
			const int32 RunEnd = bUp ? i : i - 1;
			OutRuns.Emplace(Points[RunStart], Points[RunEnd]);
			RunStart = INDEX_NONE;
		}
	}
}

void FVoidMetroLayoutResolver::FindPolylineContacts2D(const TArray<FVector>& A, bool bClosedA, const TArray<FVector>& B, bool bClosedB, float Tolerance, TArray<FVector>& OutPoints)
{
	if (A.Num() < 2 || B.Num() < 2)
	{
		return;
	}

	// Bounding box of B, inflated: cheap reject so spoke x ring stays near-linear in practice.
	FBox2D BoxB(ForceInit);
	for (const FVector& P : B) { BoxB += FVector2D(P.X, P.Y); }
	BoxB = BoxB.ExpandBy(Tolerance);

	const int32 NA = A.Num();
	const int32 NB = B.Num();
	const int32 SpansA = bClosedA ? NA : NA - 1;
	const int32 SpansB = bClosedB ? NB : NB - 1;

	for (int32 i = 0; i < SpansA; ++i)
	{
		const FVector A0(A[i].X, A[i].Y, 0.0f);
		const FVector A1(A[(i + 1) % NA].X, A[(i + 1) % NA].Y, 0.0f);

		FBox2D BoxSeg(ForceInit);
		BoxSeg += FVector2D(A0.X, A0.Y);
		BoxSeg += FVector2D(A1.X, A1.Y);
		if (!BoxSeg.Intersect(BoxB))
		{
			continue;
		}

		for (int32 j = 0; j < SpansB; ++j)
		{
			const FVector B0(B[j].X, B[j].Y, 0.0f);
			const FVector B1(B[(j + 1) % NB].X, B[(j + 1) % NB].Y, 0.0f);

			// Per-span reject: skip the exact closest-point test unless the two spans' inflated boxes overlap.
			if (FMath::Max(B0.X, B1.X) + Tolerance < BoxSeg.Min.X || FMath::Min(B0.X, B1.X) - Tolerance > BoxSeg.Max.X ||
				FMath::Max(B0.Y, B1.Y) + Tolerance < BoxSeg.Min.Y || FMath::Min(B0.Y, B1.Y) - Tolerance > BoxSeg.Max.Y)
			{
				continue;
			}

			FVector CA, CB;
			FMath::SegmentDistToSegmentSafe(A0, A1, B0, B1, CA, CB);
			if (FVector::DistSquared(CA, CB) <= Tolerance * Tolerance)
			{
				const FVector Mid = (CA + CB) * 0.5f;
				bool bDuplicate = false;
				for (const FVector& Existing : OutPoints)
				{
					if (FVector::DistSquared2D(Existing, Mid) <= Tolerance * Tolerance * 4.0f) { bDuplicate = true; break; }
				}
				if (!bDuplicate) { OutPoints.Add(Mid); }
			}
		}
	}
}

bool FVoidMetroLayoutResolver::NearestPathYawDegrees(const TArray<FVector>& Points, bool bClosed, const FVector& Location, float& OutYawDegrees, float& OutDistance)
{
	const int32 N = Points.Num();
	if (N < 2) { return false; }
	const int32 Spans = bClosed ? N : N - 1;
	float Best = TNumericLimits<float>::Max();
	FVector BestDir = FVector::ForwardVector;
	const FVector L(Location.X, Location.Y, 0.0f);

	for (int32 i = 0; i < Spans; ++i)
	{
		const FVector A(Points[i].X, Points[i].Y, 0.0f);
		const FVector B(Points[(i + 1) % N].X, Points[(i + 1) % N].Y, 0.0f);
		const FVector C = FMath::ClosestPointOnSegment(L, A, B);
		const float D = FVector::DistSquared(C, L);
		if (D < Best)
		{
			const FVector Dir = (B - A);
			if (!Dir.IsNearlyZero())
			{
				Best = D;
				BestDir = Dir;
			}
		}
	}
	if (Best == TNumericLimits<float>::Max()) { return false; }
	OutYawDegrees = FMath::RadiansToDegrees(FMath::Atan2(BestDir.Y, BestDir.X));
	OutDistance = FMath::Sqrt(Best);
	return true;
}

// -----------------------------------------------------------------------------
// Resolve
// -----------------------------------------------------------------------------

FVoidMetroResolvedLayout FVoidMetroLayoutResolver::Resolve(const FVoidMetroData& Data, const FVoidMetroLayoutParams& P, FVoidValidationReport& Report)
{
	using namespace VoidMetroResolverPrivate;

	FVoidMetroResolvedLayout Layout;
	int32 NumPlaceholderStations = 0;
	int32 NumPlaceholderSegments = 0;

	// ---- Stations -------------------------------------------------------------
	TMap<FName, int32> DistrictOrdinal;
	TMap<FName, int32> StationIndex;
	Layout.Stations.Reserve(Data.Stations.Num());

	for (const FVoidMetroStationSpec& S : Data.Stations)
	{
		FVector2D XY = FVector2D::ZeroVector;
		bool bPlaceholder = false;

		if (S.bHasPosition)
		{
			XY = S.Position;
		}
		else if (P.bAllowPlaceholderLayout && TryPlaceholderPosition(S, P, DistrictOrdinal, XY))
		{
			bPlaceholder = true;
			++NumPlaceholderStations;
		}
		else
		{
			Report.AddWarning(
				FString::Printf(TEXT("Station '%s' has no authored position and no placeholder could be derived (district '%s'); skipped."), *S.Id.Value.ToString(), *S.DistrictId.ToString()),
				TEXT("metro.stations"), TEXT("VOID.Metro.StationUnplaced"),
				TEXT("Author a 'position' for the station, or add its district to the placeholder band/azimuth tables in Project Settings."));
			continue;
		}

		FVoidMetroResolvedStation R;
		R.Id = S.Id.Value;
		R.Network = S.Network;
		R.DistrictId = S.DistrictId;
		R.Grade = StationGrade(S, P);
		R.Location = Lift(XY, P.GradeZ(R.Grade));
		R.Condition = S.PlatformCondition;
		R.bPlaceholderPosition = bPlaceholder;
		R.bIsTerminus = S.bIsTerminus;
		R.bHasServiceFacility = S.bHasMaintenanceFacility;
		StationIndex.Add(R.Id, Layout.Stations.Num());
		Layout.Stations.Add(MoveTemp(R));
	}

	// Membership helpers -------------------------------------------------------
	auto NetworkStationIndices = [&](EVoidMetroNetworkKind Kind)
	{
		TArray<int32> Out;
		for (int32 i = 0; i < Layout.Stations.Num(); ++i)
		{
			if (Layout.Stations[i].Network == Kind) { Out.Add(i); }
		}
		return Out;
	};

	auto FindStationAtXY = [&](const FVector& Location, EVoidMetroNetworkKind Kind) -> int32
	{
		for (int32 i = 0; i < Layout.Stations.Num(); ++i)
		{
			if (Layout.Stations[i].Network == Kind && FVector::Dist2D(Layout.Stations[i].Location, Location) <= 50.0f) { return i; }
		}
		return INDEX_NONE;
	};

	// ---- Lines -> segments --------------------------------------------------------
	auto FinalizeSegment = [&](FVoidMetroResolvedSegment&& Seg, float BodyZ)
	{
		Seg.Points = Densify(Seg.Points, P.MaxSegmentLengthUnits, Seg.bClosed);
		for (FVector& Pt : Seg.Points) { Pt.Z = BodyZ; }

		if (!Seg.bClosed && Seg.Points.Num() >= 2)
		{
			const int32 SI = FindStationAtXY(Seg.Points[0], Seg.Network);
			const int32 EI = FindStationAtXY(Seg.Points.Last(), Seg.Network);
			const float StartZ = (SI != INDEX_NONE) ? Layout.Stations[SI].Location.Z : BodyZ;
			const float EndZ = (EI != INDEX_NONE) ? Layout.Stations[EI].Location.Z : BodyZ;
			if (!FMath::IsNearlyEqual(StartZ, BodyZ) || !FMath::IsNearlyEqual(EndZ, BodyZ))
			{
				ApplyGradeRamps(Seg.Points, BodyZ, StartZ, EndZ, P.GradeRampLengthUnits);
			}
		}

		TArray<FVector> PortalLocs;
		TArray<float> PortalYaws;
		FindPortalCrossings(Seg.Points, PortalLocs, PortalYaws);
		for (int32 k = 0; k < PortalLocs.Num(); ++k)
		{
			FVoidMetroPortal Portal;
			Portal.Id = FName(*FString::Printf(TEXT("%s_portal_%d"), *Seg.Id.ToString(), k));
			Portal.SegmentId = Seg.Id;
			Portal.Network = Seg.Network;
			Portal.Location = PortalLocs[k];
			Portal.YawDegrees = PortalYaws[k];
			Layout.Portals.Add(MoveTemp(Portal));
		}

		TArray<TPair<FVector, FVector>> Runs;
		FindElevatedRuns(Seg.Points, P.ElevatedSpanThresholdUnits, Runs);
		for (const TPair<FVector, FVector>& Run : Runs)
		{
			FVoidMetroElevatedSpan Span;
			Span.SegmentId = Seg.Id;
			Span.Start = Run.Key;
			Span.End = Run.Value;
			Layout.ElevatedSpans.Add(Span);
		}

		if (Seg.bPlaceholder) { ++NumPlaceholderSegments; }
		Layout.Segments.Add(MoveTemp(Seg));
	};

	bool bLegacyHandled[2] = { false, false }; // indexed by EVoidMetroNetworkKind

	for (const FVoidMetroLineSpec& L : Data.Lines)
	{
		const EVoidMetroGrade Grade = LineGrade(L, P);
		const float BodyZ = P.GradeZ(Grade);
		const EVoidMetroPlatformCondition Condition = (L.Network == EVoidMetroNetworkKind::Dead) ? EVoidMetroPlatformCondition::Abandoned : EVoidMetroPlatformCondition::Maintained;

		// 1. Authored centerline always wins.
		if (L.CenterlinePoints.Num() >= 2)
		{
			FVoidMetroResolvedSegment Seg;
			Seg.Id = L.Id.Value;
			Seg.LineId = L.Id.Value;
			Seg.Network = L.Network;
			Seg.Grade = Grade;
			Seg.Condition = Condition;
			Seg.bClosed = L.bClosedLoop;
			Seg.bPlaceholder = false;
			for (const FVector2D& XY : L.CenterlinePoints) { Seg.Points.Add(Lift(XY, BodyZ)); }
			FinalizeSegment(MoveTemp(Seg), BodyZ);
			continue;
		}

		if (!P.bAllowPlaceholderLayout)
		{
			Report.AddWarning(FString::Printf(TEXT("Line '%s' has no authored centerline and placeholder layout is disabled; skipped."), *L.Id.Value.ToString()),
				TEXT("metro.lines"), TEXT("VOID.Metro.LineUnplaced"), TEXT("Author 'centerlinePoints', or enable 'Allow Placeholder Layout' in Project Settings."));
			continue;
		}

		// Members: explicit StationIds, else all stations of this network.
		TArray<int32> Members;
		if (L.StationIds.Num() > 0)
		{
			for (const FVoidElementId& Sid : L.StationIds)
			{
				if (const int32* Idx = StationIndex.Find(Sid.Value)) { Members.Add(*Idx); }
			}
		}
		else
		{
			Members = NetworkStationIndices(L.Network);
		}

		switch (L.Topology)
		{
		case EVoidMetroLineTopology::Radial:
		{
			if (Members.Num() < 2)
			{
				Report.AddWarning(FString::Printf(TEXT("Radial line '%s' needs at least 2 stations; skipped."), *L.Id.Value.ToString()), TEXT("metro.lines"), TEXT("VOID.Metro.LineUnplaced"),
					TEXT("Add stations to the network or author a centerline."));
				break;
			}
			// Origin = member closest to the world origin (the Spire's base plaza station in Meridian's data).
			int32 Origin = Members[0];
			for (int32 M : Members)
			{
				if (Radius2D(Layout.Stations[M].Location) < Radius2D(Layout.Stations[Origin].Location)) { Origin = M; }
			}
			TArray<float> BandRadii;
			for (const FName& Band : L.ConnectsBands)
			{
				if (const float* R = P.BandRadiusUnits.Find(Band)) { BandRadii.Add(*R); }
			}
			BandRadii.Sort();

			for (int32 M : Members)
			{
				if (M == Origin) { continue; }
				const FVoidMetroResolvedStation& Dest = Layout.Stations[M];
				const FVoidMetroResolvedStation& Org = Layout.Stations[Origin];
				const float RO = Radius2D(Org.Location);
				const float RD = Radius2D(Dest.Location);
				const float Az = AzimuthDegrees2D(Dest.Location);

				FVoidMetroResolvedSegment Seg;
				Seg.Id = FName(*FString::Printf(TEXT("%s__to__%s"), *L.Id.Value.ToString(), *Dest.Id.ToString()));
				Seg.LineId = L.Id.Value;
				Seg.Network = L.Network;
				Seg.Grade = Grade;
				Seg.Condition = Condition;
				Seg.bClosed = false;
				Seg.bPlaceholder = Org.bPlaceholderPosition || Dest.bPlaceholderPosition;
				Seg.Points.Add(Lift(FVector2D(Org.Location.X, Org.Location.Y), BodyZ));
				for (float R : BandRadii)
				{
					if (R > RO + 1.0f && R < RD - 1.0f) { Seg.Points.Add(Lift(PolarToXY(R, Az), BodyZ)); }
				}
				Seg.Points.Add(Lift(FVector2D(Dest.Location.X, Dest.Location.Y), BodyZ));
				FinalizeSegment(MoveTemp(Seg), BodyZ);
			}
			break;
		}
		case EVoidMetroLineTopology::Ring:
		{
			// Exclude the origin-anchor station (radius ~0) from a ring's membership.
			TArray<int32> RingMembers;
			for (int32 M : Members)
			{
				if (Radius2D(Layout.Stations[M].Location) > 1.0f) { RingMembers.Add(M); }
			}

			float RingRadius = -1.0f;
			for (const FName& Band : L.ConnectsBands)
			{
				if (const float* R = P.BandRadiusUnits.Find(Band)) { RingRadius = *R; break; }
			}
			if (RingRadius <= 0.0f && RingMembers.Num() > 0)
			{
				double Sum = 0.0;
				for (int32 M : RingMembers) { Sum += Radius2D(Layout.Stations[M].Location); }
				RingRadius = static_cast<float>(Sum / RingMembers.Num());
			}
			if (RingRadius <= 0.0f)
			{
				Report.AddWarning(FString::Printf(TEXT("Ring line '%s' has no resolvable radius; skipped."), *L.Id.Value.ToString()), TEXT("metro.lines"), TEXT("VOID.Metro.LineUnplaced"),
					TEXT("List a known radial band in connects_bands, or author a centerline."));
				break;
			}

			// Uniform samples + exact member azimuths, so member stations lie ON the ring polyline.
			TArray<float> Angles;
			const int32 Samples = FMath::Max(8, P.RingSampleCount);
			for (int32 k = 0; k < Samples; ++k) { Angles.Add(FMath::Fmod(360.0f * k / Samples, 360.0f)); }
			for (int32 M : RingMembers)
			{
				float A = AzimuthDegrees2D(Layout.Stations[M].Location);
				if (A < 0.0f) { A += 360.0f; }
				Angles.Add(A);
				if (FMath::Abs(Radius2D(Layout.Stations[M].Location) - RingRadius) > 100.0f)
				{
					Report.AddWarning(FString::Printf(TEXT("Station '%s' is %.0f cm off ring '%s'; the ring will not pass through it."), *Layout.Stations[M].Id.ToString(),
						FMath::Abs(Radius2D(Layout.Stations[M].Location) - RingRadius), *L.Id.Value.ToString()), TEXT("metro.lines"), TEXT("VOID.Metro.StationOffLine"),
						TEXT("Move the station onto the ring radius or author the ring centerline."));
				}
			}
			Angles.Sort();
			TArray<float> Unique;
			for (float A : Angles)
			{
				if (Unique.Num() == 0 || A - Unique.Last() > 0.005f) { Unique.Add(A); }
			}

			FVoidMetroResolvedSegment Seg;
			Seg.Id = FName(*FString::Printf(TEXT("%s__ring"), *L.Id.Value.ToString()));
			Seg.LineId = L.Id.Value;
			Seg.Network = L.Network;
			Seg.Grade = Grade;
			Seg.Condition = Condition;
			Seg.bClosed = true;
			Seg.bPlaceholder = true; // The ring radius itself is a placeholder unless authored.
			for (float A : Unique) { Seg.Points.Add(Lift(PolarToXY(RingRadius, A), BodyZ)); }
			FinalizeSegment(MoveTemp(Seg), BodyZ);
			break;
		}
		case EVoidMetroLineTopology::LegacySegments:
		{
			if (bLegacyHandled[static_cast<int32>(L.Network)])
			{
				Report.AddWarning(FString::Printf(TEXT("Second legacy-segment line '%s' in the same network ignored; tunnel segments are generated once per network."), *L.Id.Value.ToString()),
					TEXT("metro.lines"), TEXT("VOID.Metro.LineDuplicateSegments"), TEXT("Merge legacy lines or author centerlines."));
				break;
			}
			bLegacyHandled[static_cast<int32>(L.Network)] = true;

			if (Data.TunnelLinks.Num() == 0)
			{
				Report.AddWarning(FString::Printf(TEXT("Legacy line '%s' has no tunnel links (RoadNetwork.json tunnel_relationships); no dead-network segments generated."), *L.Id.Value.ToString()),
					TEXT("metro.tunnelLinks"), TEXT("VOID.Metro.NoTunnelLinks"), TEXT("Load RoadNetwork.json tunnel relationships through FVoidMetroNetworkMapper."));
				break;
			}

			for (const FVoidMetroTunnelLink& Link : Data.TunnelLinks)
			{
				auto FindEnd = [&](FName District) -> int32
				{
					int32 Fallback = INDEX_NONE;
					for (int32 i = 0; i < Layout.Stations.Num(); ++i)
					{
						const FVoidMetroResolvedStation& RS = Layout.Stations[i];
						if (RS.Network != L.Network || RS.DistrictId != District) { continue; }
						// Prefer the station that declares this tunnel (shares_tunnel_id).
						const FVoidMetroStationSpec* Spec = Data.Stations.FindByPredicate([&RS](const FVoidMetroStationSpec& SS) { return SS.Id.Value == RS.Id; });
						if (Spec && Spec->SharesTunnelId == Link.Id.Value) { return i; }
						if (Fallback == INDEX_NONE) { Fallback = i; }
					}
					return Fallback;
				};

				const int32 IA = FindEnd(Link.DistrictA);
				const int32 IB = FindEnd(Link.DistrictB);
				if (IA == INDEX_NONE || IB == INDEX_NONE)
				{
					Report.AddWarning(FString::Printf(TEXT("Tunnel '%s' endpoint stations could not be found; segment skipped."), *Link.Id.Value.ToString()),
						TEXT("metro.tunnelLinks"), TEXT("VOID.Metro.TunnelUnplaced"), TEXT("Ensure each tunnel district has a dead-network station."));
					continue;
				}
				const FVoidMetroResolvedStation& SA = Layout.Stations[IA];
				const FVoidMetroResolvedStation& SB = Layout.Stations[IB];

				FVoidMetroResolvedSegment Seg;
				Seg.Id = Link.Id.Value;
				Seg.LineId = L.Id.Value;
				Seg.TunnelId = Link.Id.Value;
				Seg.Network = L.Network;
				Seg.Grade = Grade;
				Seg.Condition = Condition;
				Seg.bClosed = false;
				Seg.bPlaceholder = SA.bPlaceholderPosition || SB.bPlaceholderPosition;
				Seg.Points.Add(Lift(FVector2D(SA.Location.X, SA.Location.Y), BodyZ));
				Seg.Points.Add(Lift(FVector2D(SB.Location.X, SB.Location.Y), BodyZ));
				FinalizeSegment(MoveTemp(Seg), BodyZ);
			}
			break;
		}
		default:
			Report.AddWarning(FString::Printf(TEXT("Line '%s' has no recognizable topology and no authored centerline; skipped."), *L.Id.Value.ToString()),
				TEXT("metro.lines"), TEXT("VOID.Metro.LineUnplaced"), TEXT("Author 'centerlinePoints' for this line."));
			break;
		}
	}

	// ---- Station orientation + entrances -------------------------------------------
	for (FVoidMetroResolvedStation& RS : Layout.Stations)
	{
		float BestDist = TNumericLimits<float>::Max();
		float BestYaw = 0.0f;
		for (const FVoidMetroResolvedSegment& Seg : Layout.Segments)
		{
			if (Seg.Network != RS.Network) { continue; }
			float Yaw = 0.0f, Dist = 0.0f;
			if (NearestPathYawDegrees(Seg.Points, Seg.bClosed, RS.Location, Yaw, Dist) && Dist < BestDist)
			{
				BestDist = Dist;
				BestYaw = Yaw;
			}
		}
		RS.YawDegrees = BestYaw;

		const FVoidMetroStationSpec* Spec = Data.Stations.FindByPredicate([&RS](const FVoidMetroStationSpec& SS) { return SS.Id.Value == RS.Id; });
		const int32 Limit = (Spec && Spec->MaxEntrances > 0) ? Spec->MaxEntrances : MAX_int32;

		if (Spec && Spec->Entrances.Num() > 0)
		{
			for (const FVoidMetroEntranceSpec& E : Spec->Entrances)
			{
				if (RS.Entrances.Num() >= Limit)
				{
					Report.AddWarning(FString::Printf(TEXT("Station '%s' entrance '%s' dropped: limit is %d."), *RS.Id.ToString(), *E.Id.Value.ToString(), Limit),
						TEXT("metro.stations"), TEXT("VOID.Metro.EntranceLimit"), TEXT("Remove the extra entrance."));
					continue;
				}
				FVoidMetroResolvedEntrance RE;
				RE.Id = E.Id.Value;
				RE.StationId = RS.Id;
				RE.YawDegrees = E.YawDegrees;
				RE.bPlaceholder = !E.bHasPosition;
				if (E.bHasPosition)
				{
					RE.Location = FVector(E.Position.X, E.Position.Y, 0.0f);
				}
				else
				{
					const float Rad = FMath::DegreesToRadians(RS.YawDegrees);
					const FVector Right(-FMath::Sin(Rad), FMath::Cos(Rad), 0.0f);
					RE.Location = FVector(RS.Location.X, RS.Location.Y, 0.0f) + Right * P.EntranceOffsetUnits;
				}
				RS.Entrances.Add(MoveTemp(RE));
			}
		}
		else if (P.bAllowPlaceholderLayout)
		{
			const int32 Count = FMath::Min(P.DefaultEntrancesPerStation, Limit);
			const float Rad = FMath::DegreesToRadians(RS.YawDegrees);
			const FVector Right(-FMath::Sin(Rad), FMath::Cos(Rad), 0.0f);
			for (int32 k = 0; k < Count; ++k)
			{
				const float Side = (k % 2 == 0) ? 1.0f : -1.0f;
				const float Push = P.EntranceOffsetUnits * (1.0f + 0.5f * static_cast<float>(k / 2));
				FVoidMetroResolvedEntrance RE;
				RE.Id = FName(*FString::Printf(TEXT("%s_entrance_%d"), *RS.Id.ToString(), k));
				RE.StationId = RS.Id;
				RE.Location = FVector(RS.Location.X, RS.Location.Y, 0.0f) + Right * Side * Push;
				RE.YawDegrees = RS.YawDegrees + (Side > 0.0f ? 90.0f : -90.0f);
				RE.bPlaceholder = true;
				RS.Entrances.Add(MoveTemp(RE));
			}
		}
	}

	// ---- Interchanges (Live network only; the Dead network has none by design) -----------
	for (const FVoidMetroInterchangeSpec& Spec : Data.Interchanges)
	{
		TArray<TArray<int32>> SegmentsPerLine;
		EVoidMetroNetworkKind Kind = EVoidMetroNetworkKind::Live;
		bool bKindSet = false;
		for (const FVoidElementId& LineId : Spec.ConnectedLineIds)
		{
			TArray<int32> Segs;
			for (int32 i = 0; i < Layout.Segments.Num(); ++i)
			{
				if (Layout.Segments[i].LineId == LineId.Value)
				{
					Segs.Add(i);
					if (!bKindSet) { Kind = Layout.Segments[i].Network; bKindSet = true; }
				}
			}
			SegmentsPerLine.Add(MoveTemp(Segs));
		}
		if (!bKindSet || Kind != EVoidMetroNetworkKind::Live)
		{
			continue; // Validator reports dead interchanges; never generate them.
		}

		TArray<FVector> Contacts;
		for (int32 a = 0; a < SegmentsPerLine.Num(); ++a)
		{
			for (int32 b = a + 1; b < SegmentsPerLine.Num(); ++b)
			{
				for (int32 SA : SegmentsPerLine[a])
				{
					for (int32 SB : SegmentsPerLine[b])
					{
						if (Layout.Segments[SA].Network != Layout.Segments[SB].Network) { continue; }
						FindPolylineContacts2D(Layout.Segments[SA].Points, Layout.Segments[SA].bClosed, Layout.Segments[SB].Points, Layout.Segments[SB].bClosed, FMath::Max(P.InterchangeToleranceUnits, 1.0f), Contacts);
					}
				}
			}
		}

		int32 HubCounter = 0;
		for (const FVector& C : Contacts)
		{
			// Attach to an existing Live station at the crossing rather than stacking a second structure on it.
			int32 Near = INDEX_NONE;
			for (int32 i = 0; i < Layout.Stations.Num(); ++i)
			{
				if (Layout.Stations[i].Network == EVoidMetroNetworkKind::Live && FVector::Dist2D(Layout.Stations[i].Location, C) <= P.InterchangeToleranceUnits * 4.0f) { Near = i; break; }
			}
			if (Near != INDEX_NONE)
			{
				Layout.Stations[Near].bIsInterchange = true;
				for (const FVoidElementId& LineId : Spec.ConnectedLineIds) { Layout.Stations[Near].InterchangeLineIds.AddUnique(LineId.Value); }
			}
			else
			{
				FVoidMetroInterchangeHub Hub;
				Hub.SpecId = Spec.Id.Value;
				Hub.Id = FName(*FString::Printf(TEXT("%s_%d"), *Spec.Id.Value.ToString(), HubCounter++));
				Hub.Location = FVector(C.X, C.Y, P.GradeZ(P.LiveDefaultGrade));
				for (const FVoidElementId& LineId : Spec.ConnectedLineIds) { Hub.LineIds.Add(LineId.Value); }
				float Yaw = 0.0f, Dist = 0.0f;
				for (const FVoidMetroResolvedSegment& Seg : Layout.Segments)
				{
					if (Seg.Network == EVoidMetroNetworkKind::Live && Seg.LineId == Hub.LineIds[0] && NearestPathYawDegrees(Seg.Points, Seg.bClosed, Hub.Location, Yaw, Dist)) { break; }
				}
				Hub.YawDegrees = Yaw;
				Layout.InterchangeHubs.Add(MoveTemp(Hub));
			}
		}
	}

	Layout.bUsedPlaceholder = (NumPlaceholderStations > 0) || (NumPlaceholderSegments > 0);
	if (Layout.bUsedPlaceholder)
	{
		Report.AddWarning(
			FString::Printf(TEXT("Metro layout used generator-derived PLACEHOLDER geometry for %d station(s) and %d track segment(s). Meridian's data carries no coordinates; these positions are not canon."), NumPlaceholderStations, NumPlaceholderSegments),
			TEXT("metro"), TEXT("VOID.Metro.PlaceholderLayout"),
			TEXT("Supply authored 'position' / 'centerlinePoints' via the package 'metro' block to replace them."));
	}

	return Layout;
}
