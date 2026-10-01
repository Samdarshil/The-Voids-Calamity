// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VoidPackageValidators.h"
#include "VoidValidationMath.h"
#include "Road/VoidRoadValidator.h"
#include "Road/VoidRoadTypeProfile.h"

using namespace VoidValidationMath;

namespace
{
	// ------------------------------------------------------------------
	// Uniform spatial hash over 2D AABBs. Items whose box would span more
	// than MaxCellsPerItem cells (a 5 km road with a 50 m cell size) go to an
	// "oversized" list that every query also returns, so worst-case cost is
	// bounded instead of exploding.
	// ------------------------------------------------------------------
	class FSpatialHash2D
	{
	public:
		explicit FSpatialHash2D(double InCellSize) : CellSize(FMath::Max(InCellSize, 1.0)) {}

		void Insert(int32 Item, double MinX, double MinY, double MaxX, double MaxY)
		{
			const int64 X0 = Cell(MinX), X1 = Cell(MaxX), Y0 = Cell(MinY), Y1 = Cell(MaxY);
			if ((X1 - X0 + 1) * (Y1 - Y0 + 1) > MaxCellsPerItem)
			{
				Oversized.Add(Item);
				return;
			}
			for (int64 X = X0; X <= X1; ++X)
			{
				for (int64 Y = Y0; Y <= Y1; ++Y)
				{
					Cells.FindOrAdd(Key(X, Y)).Add(Item);
				}
			}
		}

		/** Appends candidate items whose cells overlap the box; result is sorted and unique. Candidates only: callers do the exact test. */
		void Query(double MinX, double MinY, double MaxX, double MaxY, TArray<int32>& Out) const
		{
			Out.Reset();
			const int64 X0 = Cell(MinX), X1 = Cell(MaxX), Y0 = Cell(MinY), Y1 = Cell(MaxY);
			if ((X1 - X0 + 1) * (Y1 - Y0 + 1) > MaxCellsPerItem * 4)
			{
				// A huge query box: fall back to "everything in any cell" would be O(N); callers only issue small queries. Guard anyway.
				for (const TPair<int64, TArray<int32>>& Pair : Cells) { Out.Append(Pair.Value); }
			}
			else
			{
				for (int64 X = X0; X <= X1; ++X)
				{
					for (int64 Y = Y0; Y <= Y1; ++Y)
					{
						if (const TArray<int32>* Items = Cells.Find(Key(X, Y))) { Out.Append(*Items); }
					}
				}
			}
			Out.Append(Oversized);
			Out.Sort();
			int32 Write = 0;
			for (int32 Read = 0; Read < Out.Num(); ++Read)
			{
				if (Write == 0 || Out[Read] != Out[Write - 1]) { Out[Write++] = Out[Read]; }
			}
			Out.SetNum(Write, EAllowShrinking::No);
		}

	private:
		static constexpr int64 MaxCellsPerItem = 1024;
		double CellSize;
		TMap<int64, TArray<int32>> Cells;
		TArray<int32> Oversized;

		int64 Cell(double V) const { return static_cast<int64>(FMath::FloorToDouble(V / CellSize)); }
		static int64 Key(int64 X, int64 Y) { return (X << 32) ^ (Y & 0xFFFFFFFFll); }
	};

	struct FUnionFind
	{
		TArray<int32> Parent;
		explicit FUnionFind(int32 N) { Parent.SetNumUninitialized(N); for (int32 i = 0; i < N; ++i) { Parent[i] = i; } }
		int32 Find(int32 X) { while (Parent[X] != X) { Parent[X] = Parent[Parent[X]]; X = Parent[X]; } return X; }
		void Union(int32 A, int32 B) { A = Find(A); B = Find(B); if (A != B) { Parent[B] = A; } }
	};

	FString IdOf(const FVoidElementId& Id) { return Id.Value.ToString(); }
	FV2 ToV2(const FVector2D& P) { return FV2{ P.X, P.Y }; }
	bool IsIdSyntaxOk(const FString& Id)
	{
		for (TCHAR C : Id)
		{
			if (!(FChar::IsAlnum(C) || C == TEXT('_') || C == TEXT('-') || C == TEXT('.') || C == TEXT(':'))) { return false; }
		}
		return true;
	}

	bool AllFinite(const TArray<FVector2D>& Points, double MaxAbs, bool& bOutOfRange)
	{
		bool bFinite = true;
		for (const FVector2D& P : Points)
		{
			if (!IsFinite(P.X) || !IsFinite(P.Y)) { bFinite = false; }
			else if (FMath::Abs(P.X) > MaxAbs || FMath::Abs(P.Y) > MaxAbs) { bOutOfRange = true; }
		}
		return bFinite;
	}

	// ---------------------------------------------------------- road model
	struct FRoad
	{
		int32 Index = 0;
		const FVoidRoadSpec* Spec = nullptr;
		FString Id;
		bool bRoundabout = false;
		bool bUsable = false;      // finite, >= 2 distinct points (or a well-formed roundabout)
		TArray<FV2> Pts;
		double HalfWidth = 0.0;
		bool bGradeSeparated = false;

		FVector2D Endpoint(bool bStart) const { const FV2& P = bStart ? Pts[0] : Pts.Last(); return FVector2D(P.X, P.Y); }
	};

	double EffectiveWidth(const FVoidRoadSpec& Spec)
	{
		return Spec.WidthUnits > 0.0f ? Spec.WidthUnits : FVoidRoadTypeProfileLibrary::GetBuiltInDefault(Spec.RoadType).DefaultWidthUnits;
	}

	void BuildRoads(const FVoidDesignPackage& Package, TArray<FRoad>& OutRoads)
	{
		OutRoads.Reset(Package.District.Roads.Num());
		for (int32 i = 0; i < Package.District.Roads.Num(); ++i)
		{
			const FVoidRoadSpec& Spec = Package.District.Roads[i];
			FRoad R;
			R.Index = i; R.Spec = &Spec; R.Id = IdOf(Spec.Id);
			R.bRoundabout = Spec.RoadType == EVoidRoadType::Roundabout;
			R.HalfWidth = 0.5 * EffectiveWidth(Spec);
			R.bGradeSeparated = Spec.bIsBridge || Spec.bIsTunnel;
			bool bRange = false;
			if (AllFinite(Spec.CenterlinePoints, 1.0e30, bRange))
			{
				for (const FVector2D& P : Spec.CenterlinePoints) { R.Pts.Add(ToV2(P)); }
			}
			R.bUsable = R.bRoundabout ? (R.Pts.Num() >= 1 && Spec.RoundaboutRadiusUnits > 0.0f) : (R.Pts.Num() >= 2);
			OutRoads.Add(MoveTemp(R));
		}
	}
}

// ============================================================== DATA ====

void FVoidPackageDataValidator::Validate(EVoidValidationStage, const FVoidValidationInput& Input, FVoidValidationContext& Context) const
{
	if (!Input.Package)
	{
		Context.Info(TEXT("VOID.Data.NoPackage"), TEXT("Data validation skipped: no design package was provided."));
		return;
	}
	Run(*Input.Package, Input, Context);
}

void FVoidPackageDataValidator::Run(const FVoidDesignPackage& Package, const FVoidValidationInput& Input, FVoidValidationContext& Ctx)
{
	const double MaxAbs = Ctx.Options.MaxCoordinateAbs;
	const FVoidDistrictData& District = Package.District;

	const FString DistrictId = IdOf(District.DistrictId);
	if (District.DistrictId.IsValid())
	{
		if (!IsIdSyntaxOk(DistrictId))
		{
			Ctx.Warn(TEXT("VOID.Data.IdSyntax"), FString::Printf(TEXT("District id '%s' contains characters outside [A-Za-z0-9_.:-]; it is used in actor names and tags."), *DistrictId), DistrictId, TEXT("district.districtId"), TEXT("Use snake_case ids."));
		}
		if (Input.KnownDistrictIds.Num() > 0 && !Input.KnownDistrictIds.Contains(District.DistrictId.Value))
		{
			Ctx.Error(TEXT("VOID.Data.UnknownDistrict"), FString::Printf(TEXT("Package district '%s' is not in the Meridian DistrictRegistry (known: %d districts)."), *DistrictId, Input.KnownDistrictIds.Num()), DistrictId, TEXT("district.districtId"), TEXT("Use an id from DistrictRegistry.json; new districts require the registry extension review gate."));
		}
	}

	auto CheckScalar = [&Ctx, MaxAbs](double V, const FString& Field, const FString& Id, const FVector* Where)
	{
		if (!IsFinite(V))
		{
			Ctx.Error(TEXT("VOID.Data.NonFiniteValue"), FString::Printf(TEXT("%s is NaN or infinite."), *Field), Id, Field, TEXT("Replace with a finite number; this usually means an export or unit-conversion bug."), Where);
		}
		else if (FMath::Abs(V) > MaxAbs)
		{
			Ctx.Error(TEXT("VOID.Data.CoordinateOutOfRange"), FString::Printf(TEXT("%s = %s exceeds the plausible world extent of %s units."), *Field, *FString::SanitizeFloat(V), *FString::SanitizeFloat(MaxAbs)), Id, Field, TEXT("Check units (Unreal units are centimetres) and the source export."), Where);
		}
	};

	for (int32 i = 0; i < District.Roads.Num(); ++i)
	{
		const FVoidRoadSpec& Road = District.Roads[i];
		const FString Id = IdOf(Road.Id);
		const FString Prefix = FString::Printf(TEXT("district.roads[%d]"), i);
		if (Road.Id.IsValid() && !IsIdSyntaxOk(Id))
		{
			Ctx.Warn(TEXT("VOID.Data.IdSyntax"), FString::Printf(TEXT("Road id '%s' contains characters outside [A-Za-z0-9_.:-]."), *Id), Id, Prefix + TEXT(".id"), TEXT("Use snake_case ids."));
		}
		for (int32 p = 0; p < Road.CenterlinePoints.Num(); ++p)
		{
			const FVector2D& P = Road.CenterlinePoints[p];
			const FVector Where(IsFinite(P.X) ? P.X : 0.0, IsFinite(P.Y) ? P.Y : 0.0, 0.0);
			CheckScalar(P.X, FString::Printf(TEXT("%s.centerlinePoints[%d].x"), *Prefix, p), Id, &Where);
			CheckScalar(P.Y, FString::Printf(TEXT("%s.centerlinePoints[%d].y"), *Prefix, p), Id, &Where);
		}
		CheckScalar(Road.WidthUnits, Prefix + TEXT(".widthUnits"), Id, nullptr);
		CheckScalar(Road.ElevationUnits, Prefix + TEXT(".elevationUnits"), Id, nullptr);
		CheckScalar(Road.RoundaboutRadiusUnits, Prefix + TEXT(".roundaboutRadiusUnits"), Id, nullptr);
	}

	for (int32 i = 0; i < District.Buildings.Num(); ++i)
	{
		const FVoidBuildingSpec& B = District.Buildings[i];
		const FString Id = IdOf(B.Id);
		const FString Prefix = FString::Printf(TEXT("district.buildings[%d]"), i);
		if (B.Id.IsValid() && !IsIdSyntaxOk(Id))
		{
			Ctx.Warn(TEXT("VOID.Data.IdSyntax"), FString::Printf(TEXT("Building id '%s' contains characters outside [A-Za-z0-9_.:-]."), *Id), Id, Prefix + TEXT(".id"), TEXT("Use snake_case ids."));
		}
		for (int32 p = 0; p < B.FootprintCorners.Num(); ++p)
		{
			const FVector2D& P = B.FootprintCorners[p];
			const FVector Where(IsFinite(P.X) ? P.X : 0.0, IsFinite(P.Y) ? P.Y : 0.0, 0.0);
			CheckScalar(P.X, FString::Printf(TEXT("%s.footprintCorners[%d].x"), *Prefix, p), Id, &Where);
			CheckScalar(P.Y, FString::Printf(TEXT("%s.footprintCorners[%d].y"), *Prefix, p), Id, &Where);
		}
		CheckScalar(B.HeightUnits, Prefix + TEXT(".heightUnits"), Id, nullptr);
	}
}

// ============================================================== ROADS ===

void FVoidRoadNetworkValidator::Validate(EVoidValidationStage, const FVoidValidationInput& Input, FVoidValidationContext& Context) const
{
	if (!Input.Package)
	{
		Context.Info(TEXT("VOID.Road.NoPackage"), TEXT("Road validation skipped: no design package was provided."));
		return;
	}
	Run(*Input.Package, Input, Context);
}

void FVoidRoadNetworkValidator::Run(const FVoidDesignPackage& Package, const FVoidValidationInput& Input, FVoidValidationContext& Ctx)
{
	const FVoidValidationOptions& Opt = Ctx.Options;
	const FVoidDistrictData& District = Package.District;

	// Reuse the Phase 3 generation-time validator now, so bridge/tunnel conflicts,
	// unresolvable connectionIds and bad roundabouts surface before generation.
	{
		FVoidValidationReport RoadReport = FVoidRoadValidator::Validate(District);
		for (const FVoidValidationIssue& Issue : RoadReport.Issues)
		{
			Ctx.Emit(Issue.Severity, Issue.ErrorCode, Issue.Message, FString(), Issue.FieldPath, Issue.SuggestedFix);
		}
	}

	if (District.Roads.Num() == 0)
	{
		Ctx.Warn(TEXT("VOID.Road.NoRoads"), TEXT("The package contains no roads; the Road Generator will report failure (it returns false when it builds nothing)."), IdOf(District.DistrictId), TEXT("district.roads"),
			TEXT("If this package came from Meridian_Master, note that Meridian carries no geometry: a layout stage must produce road centerlines first."));
		return;
	}

	TArray<FRoad> Roads;
	BuildRoads(Package, Roads);

	TMap<FName, int32> RoadByName;
	for (const FRoad& R : Roads) { if (R.Spec->Id.IsValid()) { RoadByName.Add(R.Spec->Id.Value, R.Index); } }
	TSet<FName> BuildingNames;
	for (const FVoidBuildingSpec& B : District.Buildings) { if (B.Id.IsValid()) { BuildingNames.Add(B.Id.Value); } }

	const double Tol = FMath::Max(Opt.JunctionToleranceUnits, 1.0);

	// ---- per-road geometry + explicit connections -----------------------
	double TotalSegLength = 0.0;
	int32 NumSegs = 0;
	for (const FRoad& R : Roads)
	{
		const FVoidRoadSpec& S = *R.Spec;
		const FString Prefix = FString::Printf(TEXT("district.roads[%d]"), R.Index);
		if (!R.bUsable)
		{
			continue; // structural problems already reported by Phase 2/3 validators and the data validator
		}
		const FVector Where(R.Pts[0].X, R.Pts[0].Y, S.ElevationUnits);

		if (S.WidthUnits > Opt.MaxRoadWidthUnits)
		{
			Ctx.Warn(TEXT("VOID.Road.WidthImplausible"), FString::Printf(TEXT("Road '%s' is %.0f units wide (> %.0f). Likely a unit error (centimetres vs metres)."), *R.Id, S.WidthUnits, Opt.MaxRoadWidthUnits), R.Id, Prefix + TEXT(".widthUnits"), TEXT("Check the unit; 600 units = 6 m."), &Where);
		}
		if (S.LaneCount < 0 || S.SpeedLimitUnits < 0)
		{
			Ctx.Error(TEXT("VOID.Road.InvalidLaneOrSpeed"), FString::Printf(TEXT("Road '%s' has negative laneCount (%d) or speedLimitUnits (%d)."), *R.Id, S.LaneCount, S.SpeedLimitUnits), R.Id, Prefix, TEXT("Use 0 for 'unspecified' or a positive value."), &Where);
		}
		if (FMath::Abs(S.ElevationUnits) > 100.0f && !S.bIsBridge && !S.bIsTunnel)
		{
			Ctx.Warn(TEXT("VOID.Road.ElevatedWithoutStructure"), FString::Printf(TEXT("Road '%s' sits %.0f units off ground but is neither bridge nor tunnel; it will float without supports."), *R.Id, S.ElevationUnits), R.Id, Prefix + TEXT(".elevationUnits"), TEXT("Set isBridge/isTunnel, or use elevationUnits ~ 0 for at-grade roads."), &Where);
		}

		if (!R.bRoundabout)
		{
			double Length = 0.0;
			int32 Degenerate = 0;
			int32 Hairpins = 0;
			for (int32 p = 0; p + 1 < R.Pts.Num(); ++p)
			{
				const double SegLen = FMath::Sqrt(DistSq(R.Pts[p], R.Pts[p + 1]));
				Length += SegLen;
				if (SegLen < Opt.MinSegmentLengthUnits) { ++Degenerate; }
				if (p + 2 < R.Pts.Num())
				{
					const FV2 U{ R.Pts[p + 1].X - R.Pts[p].X, R.Pts[p + 1].Y - R.Pts[p].Y };
					const FV2 V{ R.Pts[p + 2].X - R.Pts[p + 1].X, R.Pts[p + 2].Y - R.Pts[p + 1].Y };
					if (SegLen >= Opt.MinSegmentLengthUnits && AngleBetweenDegrees(U, V) > Opt.HairpinAngleDegrees) { ++Hairpins; }
				}
			}
			TotalSegLength += Length; NumSegs += R.Pts.Num() - 1;
			if (Length < Opt.MinSegmentLengthUnits)
			{
				Ctx.Error(TEXT("VOID.Road.ZeroLength"), FString::Printf(TEXT("Road '%s' has total centerline length %.3f: all points coincide."), *R.Id, Length), R.Id, Prefix + TEXT(".centerlinePoints"), TEXT("Provide at least two distinct points."), &Where);
			}
			else if (Degenerate > 0)
			{
				Ctx.Warn(TEXT("VOID.Road.DegenerateSegment"), FString::Printf(TEXT("Road '%s' has %d consecutive point pair(s) closer than %.1f units; zero-length spline segments give NaN tangents."), *R.Id, Degenerate, Opt.MinSegmentLengthUnits), R.Id, Prefix + TEXT(".centerlinePoints"), TEXT("Remove the duplicate points."), &Where);
			}
			if (Hairpins > 0)
			{
				Ctx.Warn(TEXT("VOID.Road.SharpTurn"), FString::Printf(TEXT("Road '%s' has %d turn(s) sharper than %.0f degrees; ribbon geometry will self-overlap."), *R.Id, Hairpins, Opt.HairpinAngleDegrees), R.Id, Prefix + TEXT(".centerlinePoints"), TEXT("Add intermediate points to round the turn, or split into two roads with a junction."), &Where);
			}
		}

		// explicit connectionIds
		TSet<FName> SeenConnections;
		for (const FVoidElementId& C : S.ConnectionIds)
		{
			if (!C.IsValid()) { continue; }
			if (C.Value == S.Id.Value)
			{
				Ctx.Error(TEXT("VOID.Road.SelfConnection"), FString::Printf(TEXT("Road '%s' lists itself in connectionIds."), *R.Id), R.Id, Prefix + TEXT(".connectionIds"), TEXT("Remove the self reference."), &Where);
				continue;
			}
			if (SeenConnections.Contains(C.Value))
			{
				Ctx.Warn(TEXT("VOID.Road.DuplicateConnection"), FString::Printf(TEXT("Road '%s' lists connection '%s' more than once."), *R.Id, *IdOf(C)), R.Id, Prefix + TEXT(".connectionIds"), TEXT("Remove the duplicate."), &Where);
			}
			SeenConnections.Add(C.Value);

			const int32* Other = RoadByName.Find(C.Value);
			if (!Other)
			{
				if (BuildingNames.Contains(C.Value))
				{
					Ctx.Warn(TEXT("VOID.Road.ConnectionToNonRoad"), FString::Printf(TEXT("Road '%s' lists building '%s' in connectionIds; the junction builder only understands road ids."), *R.Id, *IdOf(C)), R.Id, Prefix + TEXT(".connectionIds"), TEXT("Remove it, or model the entrance as a service road."), &Where);
				}
				continue; // unknown ids are reported by FVoidRoadValidator above
			}
			const FRoad& O = Roads[*Other];
			if (!O.bUsable) { continue; }
			double Best = TNumericLimits<double>::Max();
			for (int32 e = 0; e < 2; ++e)
			{
				const FV2 EP = e == 0 ? R.Pts[0] : R.Pts.Last();
				if (O.bRoundabout)
				{
					Best = FMath::Min(Best, FMath::Abs(FMath::Sqrt(DistSq(EP, O.Pts[0])) - O.Spec->RoundaboutRadiusUnits));
				}
				else
				{
					for (int32 q = 0; q + 1 < O.Pts.Num(); ++q) { Best = FMath::Min(Best, FMath::Sqrt(PointSegmentDistSq(EP, O.Pts[q], O.Pts[q + 1]))); }
				}
			}
			const double Limit = 5.0 * Tol + O.HalfWidth + R.HalfWidth;
			if (Best > Limit)
			{
				Ctx.Warn(TEXT("VOID.Road.ConnectionTooFar"), FString::Printf(TEXT("Road '%s' declares a connection to '%s' but its nearest endpoint is %.0f units away (limit %.0f); no junction will form there."), *R.Id, *IdOf(C), Best, Limit), R.Id, Prefix + TEXT(".connectionIds"), TEXT("Move the endpoint onto the other road, or remove the connection."), &Where);
			}
		}
	}

	// ---- topology (endpoint clusters, unsplit T's, crossings) ------------
	const double AvgSeg = NumSegs > 0 ? TotalSegLength / NumSegs : 1000.0;
	const double CellSize = FMath::Clamp(AvgSeg * 2.0, 500.0, 50000.0);

	// endpoints
	struct FEndpoint { int32 Road; bool bStart; FV2 P; };
	TArray<FEndpoint> Endpoints;
	for (const FRoad& R : Roads)
	{
		if (!R.bUsable || R.bRoundabout) { continue; }
		Endpoints.Add({ R.Index, true, R.Pts[0] });
		Endpoints.Add({ R.Index, false, R.Pts.Last() });
	}

	FUnionFind RoadUF(Roads.Num());
	FUnionFind EndpointUF(Endpoints.Num());
	{
		FSpatialHash2D EndpointHash(FMath::Max(Tol * 2.0, 50.0));
		for (int32 e = 0; e < Endpoints.Num(); ++e)
		{
			EndpointHash.Insert(e, Endpoints[e].P.X, Endpoints[e].P.Y, Endpoints[e].P.X, Endpoints[e].P.Y);
		}
		TArray<int32> Candidates;
		for (int32 e = 0; e < Endpoints.Num(); ++e)
		{
			EndpointHash.Query(Endpoints[e].P.X - Tol, Endpoints[e].P.Y - Tol, Endpoints[e].P.X + Tol, Endpoints[e].P.Y + Tol, Candidates);
			for (int32 c : Candidates)
			{
				if (c > e && DistSq(Endpoints[e].P, Endpoints[c].P) <= Tol * Tol)
				{
					EndpointUF.Union(e, c);
					if (Endpoints[e].Road != Endpoints[c].Road) { RoadUF.Union(Endpoints[e].Road, Endpoints[c].Road); }
				}
			}
		}
	}

	// explicit connections also join roads
	for (const FRoad& R : Roads)
	{
		for (const FVoidElementId& C : R.Spec->ConnectionIds)
		{
			if (const int32* O = RoadByName.Find(C.Value)) { RoadUF.Union(R.Index, *O); }
		}
	}

	// spurs into roundabouts: an endpoint near a ring joins the roundabout node
	TArray<int32> RoundaboutRoads;
	for (const FRoad& R : Roads) { if (R.bUsable && R.bRoundabout) { RoundaboutRoads.Add(R.Index); } }
	TSet<int32> EndpointsOnRoundabout;
	for (int32 e = 0; e < Endpoints.Num(); ++e)
	{
		for (int32 RIdx : RoundaboutRoads)
		{
			const FRoad& Ring = Roads[RIdx];
			const double Dist = FMath::Sqrt(DistSq(Endpoints[e].P, Ring.Pts[0]));
			if (FMath::Abs(Dist - Ring.Spec->RoundaboutRadiusUnits) <= Ring.HalfWidth + Tol)
			{
				RoadUF.Union(Endpoints[e].Road, RIdx);
				EndpointsOnRoundabout.Add(e);
			}
		}
	}

	// segment hash (non-roundabout roads)
	struct FSeg { int32 Road; int32 Index; };
	TArray<FSeg> Segs;
	FSpatialHash2D SegHash(CellSize);
	for (const FRoad& R : Roads)
	{
		if (!R.bUsable || R.bRoundabout) { continue; }
		for (int32 p = 0; p + 1 < R.Pts.Num(); ++p)
		{
			const int32 SegId = Segs.Add({ R.Index, p });
			SegHash.Insert(SegId, FMath::Min(R.Pts[p].X, R.Pts[p + 1].X), FMath::Min(R.Pts[p].Y, R.Pts[p + 1].Y), FMath::Max(R.Pts[p].X, R.Pts[p + 1].X), FMath::Max(R.Pts[p].Y, R.Pts[p + 1].Y));
		}
	}

	// unsplit T-junctions: an endpoint that touches another road's interior
	TSet<int32> EndpointsOnInterior;
	{
		TSet<uint64> Reported;
		TArray<int32> Candidates;
		for (int32 e = 0; e < Endpoints.Num(); ++e)
		{
			const FEndpoint& EP = Endpoints[e];
			SegHash.Query(EP.P.X - Tol, EP.P.Y - Tol, EP.P.X + Tol, EP.P.Y + Tol, Candidates);
			for (int32 SegId : Candidates)
			{
				const FSeg& Seg = Segs[SegId];
				if (Seg.Road == EP.Road) { continue; }
				const FRoad& O = Roads[Seg.Road];
				if (PointSegmentDistSq(EP.P, O.Pts[Seg.Index], O.Pts[Seg.Index + 1]) > Tol * Tol) { continue; }
				if (DistSq(EP.P, O.Pts[0]) <= Tol * Tol || DistSq(EP.P, O.Pts.Last()) <= Tol * Tol) { continue; } // ordinary endpoint junction
				EndpointsOnInterior.Add(e);
				RoadUF.Union(EP.Road, Seg.Road);
				const uint64 Key = (static_cast<uint64>(e) << 32) | static_cast<uint32>(Seg.Road);
				if (Reported.Contains(Key)) { continue; }
				Reported.Add(Key);
				const FRoad& R = Roads[EP.Road];
				const FVector Where(EP.P.X, EP.P.Y, R.Spec->ElevationUnits);
				Ctx.Warn(TEXT("VOID.Road.UnsplitTJunction"),
					FString::Printf(TEXT("The %s of road '%s' lands on the middle of road '%s'. The junction builder only clusters endpoints, so no junction pad or crosswalk will be generated here."), EP.bStart ? TEXT("start") : TEXT("end"), *R.Id, *O.Id),
					R.Id, FString::Printf(TEXT("district.roads[%d].centerlinePoints"), R.Index), FString::Printf(TEXT("Split '%s' at this point so both halves end here, or add a shared vertex."), *O.Id), &Where);
			}
		}
	}

	// crossings without a junction
	if (Opt.bCheckRoadCrossings)
	{
		int32 GradeSeparated = 0;
		TArray<int32> Candidates;
		for (int32 s = 0; s < Segs.Num(); ++s)
		{
			const FSeg& A = Segs[s];
			const FRoad& RA = Roads[A.Road];
			const FV2& A0 = RA.Pts[A.Index];
			const FV2& A1 = RA.Pts[A.Index + 1];
			SegHash.Query(FMath::Min(A0.X, A1.X), FMath::Min(A0.Y, A1.Y), FMath::Max(A0.X, A1.X), FMath::Max(A0.Y, A1.Y), Candidates);
			for (int32 t : Candidates)
			{
				if (t <= s) { continue; }
				const FSeg& B = Segs[t];
				if (B.Road == A.Road) { continue; }
				const FRoad& RB = Roads[B.Road];
				FV2 X;
				if (!SegmentsProperlyCross(A0, A1, RB.Pts[B.Index], RB.Pts[B.Index + 1], &X)) { continue; }
				if (RA.bGradeSeparated || RB.bGradeSeparated || FMath::Abs(RA.Spec->ElevationUnits - RB.Spec->ElevationUnits) > 200.0f) { ++GradeSeparated; continue; }
				const FVector Where(X.X, X.Y, RA.Spec->ElevationUnits);
				Ctx.Warn(TEXT("VOID.Road.CrossingWithoutJunction"),
					FString::Printf(TEXT("Roads '%s' and '%s' cross at grade with no shared endpoint; the surfaces will z-fight and no junction is generated."), *RA.Id, *RB.Id),
					RA.Id, FString::Printf(TEXT("district.roads[%d].centerlinePoints"), RA.Index), FString::Printf(TEXT("Split both roads at the crossing so they meet at a shared endpoint, or mark one as bridge/tunnel (other road: '%s')."), *RB.Id), &Where);
			}
		}
		if (GradeSeparated > 0)
		{
			Ctx.Info(TEXT("VOID.Road.GradeSeparatedCrossings"), FString::Printf(TEXT("%d crossing(s) are grade-separated (bridge/tunnel or >200 units elevation difference) and were not flagged."), GradeSeparated));
		}
	}

	// junction clusters: acute angles and overload
	{
		TMap<int32, TArray<int32>> Clusters;
		for (int32 e = 0; e < Endpoints.Num(); ++e) { Clusters.FindOrAdd(EndpointUF.Find(e)).Add(e); }
		for (const TPair<int32, TArray<int32>>& Cluster : Clusters)
		{
			const TArray<int32>& Members = Cluster.Value;
			if (Members.Num() < 2) { continue; }
			FV2 Center{ 0, 0 };
			TArray<FV2> Dirs; TArray<int32> DirRoad;
			for (int32 m : Members)
			{
				const FEndpoint& EP = Endpoints[m];
				Center.X += EP.P.X; Center.Y += EP.P.Y;
				const FRoad& R = Roads[EP.Road];
				const int32 Next = EP.bStart ? 1 : R.Pts.Num() - 2;
				Dirs.Add(FV2{ R.Pts[Next].X - EP.P.X, R.Pts[Next].Y - EP.P.Y });
				DirRoad.Add(EP.Road);
			}
			Center.X /= Members.Num(); Center.Y /= Members.Num();
			const FVector Where(Center.X, Center.Y, 0.0);

			if (Members.Num() >= Opt.OverloadedJunctionRoadCount)
			{
				Ctx.Warn(TEXT("VOID.Road.OverloadedJunction"), FString::Printf(TEXT("%d road endpoints meet within %.0f units of each other; the pad cannot represent that many approaches."), Members.Num(), Tol), FString(), FString(), TEXT("Split the junction into two, or reduce the number of roads meeting here."), &Where);
			}
			if (Members.Num() <= 32) // pair test is O(k^2) on tiny k; skip pathological clusters (already reported as overloaded)
			{
				bool bReported = false;
				for (int32 a = 0; a < Dirs.Num() && !bReported; ++a)
				{
					for (int32 b = a + 1; b < Dirs.Num() && !bReported; ++b)
					{
						if (DirRoad[a] == DirRoad[b]) { continue; }
						if (AngleBetweenDegrees(Dirs[a], Dirs[b]) < Opt.AcuteJunctionAngleDegrees)
						{
							bReported = true;
							Ctx.Warn(TEXT("VOID.Road.AcuteJunction"), FString::Printf(TEXT("Roads '%s' and '%s' leave the same junction at under %.0f degrees to each other; their surfaces will overlap."), *Roads[DirRoad[a]].Id, *Roads[DirRoad[b]].Id, Opt.AcuteJunctionAngleDegrees), Roads[DirRoad[a]].Id, FString(), TEXT("Spread the approach angles or merge the roads."), &Where);
						}
					}
				}
			}
		}

		// dangling endpoints (Info summary) and isolated roads
		int32 Dangling = 0;
		TMap<int32, int32> ClusterSize;
		for (const TPair<int32, TArray<int32>>& Cluster : Clusters) { ClusterSize.Add(Cluster.Key, Cluster.Value.Num()); }
		for (int32 e = 0; e < Endpoints.Num(); ++e)
		{
			const FEndpoint& EP = Endpoints[e];
			const bool bConnected = ClusterSize[EndpointUF.Find(e)] > 1 || EndpointsOnInterior.Contains(e) || EndpointsOnRoundabout.Contains(e);
			if (!bConnected && !(!EP.bStart && Roads[EP.Road].Spec->bCulDeSacAtEnd)) { ++Dangling; }
		}
		if (Dangling > 0)
		{
			Ctx.Info(TEXT("VOID.Road.DanglingEndpoints"), FString::Printf(TEXT("%d road endpoint(s) connect to nothing and are not flagged cul-de-sac (they generate plain dead-end caps)."), Dangling));
		}

		// connectivity: components over usable roads
		TMap<int32, TArray<int32>> Components;
		for (const FRoad& R : Roads) { if (R.bUsable) { Components.FindOrAdd(RoadUF.Find(R.Index)).Add(R.Index); } }
		if (Components.Num() > 1)
		{
			TArray<TArray<int32>> Sorted;
			for (const TPair<int32, TArray<int32>>& C : Components) { Sorted.Add(C.Value); }
			Sorted.Sort([](const TArray<int32>& A, const TArray<int32>& B) { return A.Num() != B.Num() ? A.Num() > B.Num() : A[0] < B[0]; });
			for (int32 c = 1; c < Sorted.Num(); ++c)
			{
				const FRoad& First = Roads[Sorted[c][0]];
				const FVector Where(First.Pts[0].X, First.Pts[0].Y, First.Spec->ElevationUnits);
				if (Sorted[c].Num() == 1)
				{
					Ctx.Warn(TEXT("VOID.Road.IsolatedRoad"), FString::Printf(TEXT("Road '%s' is not connected to any other road (no shared endpoint, spur, or connectionId). The largest network has %d road(s)."), *First.Id, Sorted[0].Num()), First.Id, FString::Printf(TEXT("district.roads[%d]"), First.Index), TEXT("Connect it to the network, or confirm it is intentionally standalone (e.g. Dead-Network service road)."), &Where);
				}
				else
				{
					Ctx.Warn(TEXT("VOID.Road.DisconnectedNetwork"), FString::Printf(TEXT("A group of %d road(s) starting at '%s' is disconnected from the main network of %d road(s)."), Sorted[c].Num(), *First.Id, Sorted[0].Num()), First.Id, FString::Printf(TEXT("district.roads[%d]"), First.Index), TEXT("Connect the groups, or split them into separate districts/packages (Meridian's Live and Dead networks must stay separate)."), &Where);
				}
			}
		}
	}
}

// ============================================================ BUILDINGS =

void FVoidBuildingHookValidator::Validate(EVoidValidationStage, const FVoidValidationInput& Input, FVoidValidationContext& Context) const
{
	if (!Input.Package)
	{
		Context.Info(TEXT("VOID.Building.NoPackage"), TEXT("Building validation skipped: no design package was provided."));
		return;
	}
	Run(*Input.Package, Input, Context);
}

void FVoidBuildingHookValidator::Run(const FVoidDesignPackage& Package, const FVoidValidationInput& Input, FVoidValidationContext& Ctx)
{
	const FVoidValidationOptions& Opt = Ctx.Options;
	const TArray<FVoidBuildingSpec>& Buildings = Package.District.Buildings;
	if (Buildings.Num() == 0) { return; }

	struct FBuilding { int32 Index = 0; FString Id; TArray<FV2> Poly; FV2 Centroid{ 0, 0 }; double MinX = 0, MinY = 0, MaxX = 0, MaxY = 0; bool bSimple = false; };
	TArray<FBuilding> Usable;
	Usable.Reserve(Buildings.Num());

	for (int32 i = 0; i < Buildings.Num(); ++i)
	{
		const FVoidBuildingSpec& Spec = Buildings[i];
		const FString Id = IdOf(Spec.Id);
		const FString Prefix = FString::Printf(TEXT("district.buildings[%d]"), i);

		if (Spec.HeightUnits > Opt.MaxBuildingHeightUnits)
		{
			Ctx.Warn(TEXT("VOID.Building.HeightImplausible"), FString::Printf(TEXT("Building '%s' is %.0f units tall (> %.0f). Likely a unit error."), *Id, Spec.HeightUnits, Opt.MaxBuildingHeightUnits), Id, Prefix + TEXT(".heightUnits"), TEXT("Check the unit; 1000 units = 10 m."));
		}

		bool bRange = false;
		if (Spec.FootprintCorners.Num() < 3 || !AllFinite(Spec.FootprintCorners, 1.0e30, bRange)) { continue; } // Phase 2 / data validator already reported this

		FBuilding B;
		B.Index = i; B.Id = Id;
		int32 Duplicates = 0;
		for (int32 p = 0; p < Spec.FootprintCorners.Num(); ++p)
		{
			const FV2 P = ToV2(Spec.FootprintCorners[p]);
			if (p > 0 && DistSq(P, B.Poly.Last()) < 1.0) { ++Duplicates; continue; }
			B.Poly.Add(P);
		}
		if (B.Poly.Num() > 1 && DistSq(B.Poly[0], B.Poly.Last()) < 1.0) { B.Poly.Pop(); ++Duplicates; }

		B.MinX = B.MaxX = B.Poly[0].X; B.MinY = B.MaxY = B.Poly[0].Y;
		for (const FV2& P : B.Poly)
		{
			B.MinX = FMath::Min(B.MinX, P.X); B.MaxX = FMath::Max(B.MaxX, P.X); B.MinY = FMath::Min(B.MinY, P.Y); B.MaxY = FMath::Max(B.MaxY, P.Y);
			B.Centroid.X += P.X; B.Centroid.Y += P.Y;
		}
		B.Centroid.X /= B.Poly.Num(); B.Centroid.Y /= B.Poly.Num();
		const FVector Where(B.Centroid.X, B.Centroid.Y, 0.0);

		if (Duplicates > 0)
		{
			Ctx.Warn(TEXT("VOID.Building.DuplicateCorner"), FString::Printf(TEXT("Building '%s' has %d duplicate consecutive footprint corner(s)."), *Id, Duplicates), Id, Prefix + TEXT(".footprintCorners"), TEXT("Remove the repeated corners."), &Where);
		}
		if (B.Poly.Num() < 3)
		{
			Ctx.Error(TEXT("VOID.Building.FootprintDegenerate"), FString::Printf(TEXT("Building '%s' footprint collapses to %d distinct point(s)."), *Id, B.Poly.Num()), Id, Prefix + TEXT(".footprintCorners"), TEXT("Provide at least 3 distinct corners."), &Where);
			continue;
		}
		const double Area = PolygonSignedArea(B.Poly.GetData(), B.Poly.Num());
		if (FMath::Abs(Area) < 1.0)
		{
			Ctx.Error(TEXT("VOID.Building.FootprintDegenerate"), FString::Printf(TEXT("Building '%s' footprint has near-zero area (%.4f); the corners are collinear."), *Id, Area), Id, Prefix + TEXT(".footprintCorners"), TEXT("Fix the corner coordinates."), &Where);
			continue;
		}
		if (Area < 0.0)
		{
			Ctx.Warn(TEXT("VOID.Building.FootprintWinding"), FString::Printf(TEXT("Building '%s' footprint is wound clockwise; the spec requires counter-clockwise. Walls/normals may face inward."), *Id), Id, Prefix + TEXT(".footprintCorners"), TEXT("Reverse the corner order."), &Where);
		}
		if (FMath::Max(B.MaxX - B.MinX, B.MaxY - B.MinY) > Opt.MaxBuildingExtentUnits)
		{
			Ctx.Warn(TEXT("VOID.Building.FootprintTooLarge"), FString::Printf(TEXT("Building '%s' footprint spans %.0f units (> %.0f)."), *Id, FMath::Max(B.MaxX - B.MinX, B.MaxY - B.MinY), Opt.MaxBuildingExtentUnits), Id, Prefix + TEXT(".footprintCorners"), TEXT("Check the unit, or split into several buildings."), &Where);
		}
		if (B.Poly.Num() > Opt.MaxFootprintCorners)
		{
			Ctx.Warn(TEXT("VOID.Building.FootprintTooComplex"), FString::Printf(TEXT("Building '%s' has %d corners (> %d); the self-intersection check was skipped for it."), *Id, B.Poly.Num(), Opt.MaxFootprintCorners), Id, Prefix + TEXT(".footprintCorners"), TEXT("Simplify the footprint."), &Where);
			B.bSimple = true; // unknown; treat as usable for the coarse checks below
		}
		else if (PolygonSelfIntersects(B.Poly.GetData(), B.Poly.Num()))
		{
			Ctx.Error(TEXT("VOID.Building.FootprintSelfIntersects"), FString::Printf(TEXT("Building '%s' footprint crosses itself (bow-tie); it cannot be triangulated into a valid wall loop."), *Id), Id, Prefix + TEXT(".footprintCorners"), TEXT("Reorder the corners so the outline does not cross itself."), &Where);
			continue;
		}
		else
		{
			B.bSimple = true;
		}
		Usable.Add(MoveTemp(B));
	}

	if (Usable.Num() == 0) { return; }

	double AvgExtent = 0.0;
	for (const FBuilding& B : Usable) { AvgExtent += FMath::Max(B.MaxX - B.MinX, B.MaxY - B.MinY); }
	AvgExtent /= Usable.Num();
	const double CellSize = FMath::Clamp(AvgExtent * 2.0, 500.0, 50000.0);

	// ---- building vs building ------------------------------------------
	if (Opt.bCheckBuildingOverlaps)
	{
		FSpatialHash2D Hash(CellSize);
		for (int32 i = 0; i < Usable.Num(); ++i) { Hash.Insert(i, Usable[i].MinX, Usable[i].MinY, Usable[i].MaxX, Usable[i].MaxY); }
		TArray<int32> Candidates;
		for (int32 i = 0; i < Usable.Num(); ++i)
		{
			const FBuilding& A = Usable[i];
			Hash.Query(A.MinX, A.MinY, A.MaxX, A.MaxY, Candidates);
			for (int32 j : Candidates)
			{
				if (j <= i) { continue; }
				const FBuilding& B = Usable[j];
				if (A.MaxX < B.MinX || B.MaxX < A.MinX || A.MaxY < B.MinY || B.MaxY < A.MinY) { continue; }
				if (PolygonsOverlap(A.Poly.GetData(), A.Poly.Num(), B.Poly.GetData(), B.Poly.Num()))
				{
					const FVector Where(A.Centroid.X, A.Centroid.Y, 0.0);
					Ctx.Warn(TEXT("VOID.Building.OverlapsBuilding"), FString::Printf(TEXT("Building footprints '%s' and '%s' overlap."), *A.Id, *B.Id), A.Id, FString::Printf(TEXT("district.buildings[%d].footprintCorners"), A.Index), FString::Printf(TEXT("Move or resize one of them (other: '%s')."), *B.Id), &Where);
				}
			}
		}
	}

	// ---- building vs road ----------------------------------------------
	if (Opt.bCheckBuildingRoadConflicts && Package.District.Roads.Num() > 0)
	{
		TArray<FRoad> Roads;
		BuildRoads(Package, Roads);
		struct FSeg { int32 Road; int32 Index; };
		TArray<FSeg> Segs;
		FSpatialHash2D SegHash(CellSize);
		for (const FRoad& R : Roads)
		{
			if (!R.bUsable || R.bRoundabout || R.bGradeSeparated) { continue; } // bridges/tunnels do not occupy the ground plane
			for (int32 p = 0; p + 1 < R.Pts.Num(); ++p)
			{
				const int32 SegId = Segs.Add({ R.Index, p });
				const double H = R.HalfWidth;
				SegHash.Insert(SegId, FMath::Min(R.Pts[p].X, R.Pts[p + 1].X) - H, FMath::Min(R.Pts[p].Y, R.Pts[p + 1].Y) - H, FMath::Max(R.Pts[p].X, R.Pts[p + 1].X) + H, FMath::Max(R.Pts[p].Y, R.Pts[p + 1].Y) + H);
			}
		}
		TSet<uint64> Reported;
		TArray<int32> Candidates;
		for (const FBuilding& B : Usable)
		{
			SegHash.Query(B.MinX, B.MinY, B.MaxX, B.MaxY, Candidates);
			for (int32 SegId : Candidates)
			{
				const FSeg& Seg = Segs[SegId];
				const FRoad& R = Roads[Seg.Road];
				const uint64 Key = (static_cast<uint64>(B.Index) << 32) | static_cast<uint32>(Seg.Road);
				if (Reported.Contains(Key)) { continue; }
				const FV2& S0 = R.Pts[Seg.Index];
				const FV2& S1 = R.Pts[Seg.Index + 1];

				bool bOnRoad = PointSegmentDistSq(B.Centroid, S0, S1) <= R.HalfWidth * R.HalfWidth;
				bool bTouches = bOnRoad;
				if (!bTouches)
				{
					for (int32 p = 0; p < B.Poly.Num() && !bTouches; ++p)
					{
						if (PointSegmentDistSq(B.Poly[p], S0, S1) <= R.HalfWidth * R.HalfWidth) { bTouches = true; }
						else if (SegmentsProperlyCross(B.Poly[p], B.Poly[(p + 1) % B.Poly.Num()], S0, S1)) { bTouches = true; }
					}
					bTouches = bTouches || PointInPolygon(S0, B.Poly.GetData(), B.Poly.Num());
				}
				if (!bTouches) { continue; }
				Reported.Add(Key);
				const FVector Where(B.Centroid.X, B.Centroid.Y, 0.0);
				if (bOnRoad)
				{
					Ctx.Error(TEXT("VOID.Building.OnRoad"), FString::Printf(TEXT("Building '%s' has its centre on the surface of road '%s' (half-width %.0f units); it would be built in the middle of the road."), *B.Id, *R.Id, R.HalfWidth), B.Id, FString::Printf(TEXT("district.buildings[%d].footprintCorners"), B.Index), FString::Printf(TEXT("Move the building off road '%s', or reroute the road."), *R.Id), &Where);
				}
				else
				{
					Ctx.Warn(TEXT("VOID.Building.EncroachesRoad"), FString::Printf(TEXT("Building '%s' footprint overlaps the surface of road '%s'."), *B.Id, *R.Id), B.Id, FString::Printf(TEXT("district.buildings[%d].footprintCorners"), B.Index), FString::Printf(TEXT("Pull the footprint back from road '%s' by at least its half-width plus sidewalk."), *R.Id), &Where);
				}
			}
		}
	}
}
