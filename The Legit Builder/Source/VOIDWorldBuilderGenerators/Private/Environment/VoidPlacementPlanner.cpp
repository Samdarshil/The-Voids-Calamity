// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Environment/VoidPlacementPlanner.h"
#include "Misc/Crc.h"

namespace VoidPlan
{

	namespace
	{
		/** Cached FNames: comparing against a freshly constructed FName per candidate would hash a string per comparison. */
		struct FNames
		{
			FName Roadside{Context::Roadside}, Sidewalk{Context::Sidewalk}, Curbside{Context::Curbside}, Alley{Context::Alley},
				Commercial{Context::Commercial}, Junction{Context::Junction}, Park{Context::Park}, Plaza{Context::Plaza},
				Site{Context::Site}, Rooftop{Context::Rooftop}, Facade{Context::Facade}, Building{TEXT("Building")};
		};
		static const FNames& N() { static const FNames Instance; return Instance; }
	}

	// ======================================================================
	// Hashing (stable across sessions/machines; no FName indices, no FRandomStream)
	// ======================================================================

	uint64 Mix64(uint64 X)
	{
		X += 0x9E3779B97F4A7C15ull;
		X = (X ^ (X >> 30)) * 0xBF58476D1CE4E5B9ull;
		X = (X ^ (X >> 27)) * 0x94D049BB133111EBull;
		return X ^ (X >> 31);
	}

	uint64 HashName(FName Name)
	{
		const FString Str = Name.ToString();
		return static_cast<uint64>(FCrc::StrCrc32(*Str)) | (static_cast<uint64>(FCrc::StrCrc32(*Str.ToLower())) << 32);
	}

	uint64 MakeStableId(uint64 Seed, FName District, FName RuleId, FName Source, uint64 Index)
	{
		uint64 H = Mix64(Seed ^ HashName(District));
		H = Mix64(H + HashName(RuleId));
		H = Mix64(H + HashName(Source));
		H = Mix64(H + Index);
		return H;
	}

	float Hash01(uint64 Id, uint32 Salt)
	{
		const uint64 H = Mix64(Id + 0x1234567ull * (static_cast<uint64>(Salt) + 1ull));
		return static_cast<float>(static_cast<double>(H >> 11) * (1.0 / 9007199254740992.0));
	}

	// ======================================================================
	// Geometry helpers
	// ======================================================================

	bool PointInPolygon(const FVector2D& P, const TArray<FVector2D>& Poly)
	{
		bool bInside = false;
		const int32 N = Poly.Num();
		for (int32 I = 0, J = N - 1; I < N; J = I++)
		{
			const FVector2D& A = Poly[I];
			const FVector2D& B = Poly[J];
			if (((A.Y > P.Y) != (B.Y > P.Y)) && (P.X < (B.X - A.X) * (P.Y - A.Y) / (B.Y - A.Y) + A.X))
			{
				bInside = !bInside;
			}
		}
		return bInside;
	}

	static double DistToSegment(const FVector2D& P, const FVector2D& A, const FVector2D& B)
	{
		const FVector2D AB = B - A;
		const double LenSq = AB.X * AB.X + AB.Y * AB.Y;
		double T = 0.0;
		if (LenSq > 1e-9)
		{
			T = ((P.X - A.X) * AB.X + (P.Y - A.Y) * AB.Y) / LenSq;
			T = FMath::Clamp(T, 0.0, 1.0);
		}
		const double DX = P.X - (A.X + AB.X * T);
		const double DY = P.Y - (A.Y + AB.Y * T);
		return FMath::Sqrt(DX * DX + DY * DY);
	}

	float DistanceToPolygonEdges(const FVector2D& P, const TArray<FVector2D>& Poly)
	{
		double Best = 1e30;
		const int32 N = Poly.Num();
		for (int32 I = 0; I < N; ++I)
		{
			Best = FMath::Min(Best, DistToSegment(P, Poly[I], Poly[(I + 1) % N]));
		}
		return static_cast<float>(Best);
	}

	static double SignedArea(const TArray<FVector2D>& Poly)
	{
		double A = 0.0;
		const int32 N = Poly.Num();
		for (int32 I = 0; I < N; ++I)
		{
			const FVector2D& P = Poly[I];
			const FVector2D& Q = Poly[(I + 1) % N];
			A += P.X * Q.Y - Q.X * P.Y;
		}
		return A * 0.5;
	}

	static float DistanceToRoads(const FVector2D& P, const TArray<FRoad>& Roads)
	{
		double Best = 1e30;
		for (const FRoad& Road : Roads)
		{
			for (int32 I = 0; I + 1 < Road.Points.Num(); ++I)
			{
				Best = FMath::Min(Best, DistToSegment(P, FVector2D(Road.Points[I].X, Road.Points[I].Y), FVector2D(Road.Points[I + 1].X, Road.Points[I + 1].Y)));
			}
		}
		return static_cast<float>(Best);
	}

	static float YawOf(const FVector2D& Dir)
	{
		return static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)));
	}

	/** UE is X-forward / Y-right; the right-hand side of a forward direction T is (-Ty, Tx). */
	static FVector2D RightOf(const FVector2D& T)
	{
		return FVector2D(-T.Y, T.X);
	}

	// ======================================================================
	// Planner internals
	// ======================================================================

	namespace
	{
		struct FAreaBounds
		{
			FVector2D Min = FVector2D::ZeroVector;
			FVector2D Max = FVector2D::ZeroVector;
		};

		struct FPolyline
		{
			const TArray<FVector>* Points = nullptr;
			TArray<double> Cum;   // cumulative length; Cum.Num() == Points.Num() (+1 if closed)
			bool bClosed = false;
			double Length = 0.0;

			void Build(const TArray<FVector>& InPoints, bool bInClosed)
			{
				Points = &InPoints;
				bClosed = bInClosed;
				Cum.Reset();
				double Acc = 0.0;
				Cum.Add(0.0);
				for (int32 I = 1; I < InPoints.Num(); ++I)
				{
					Acc += FVector::Dist(InPoints[I - 1], InPoints[I]);
					Cum.Add(Acc);
				}
				if (bClosed && InPoints.Num() > 2)
				{
					Acc += FVector::Dist(InPoints.Last(), InPoints[0]);
					Cum.Add(Acc);
				}
				Length = Acc;
			}

			/** Position and unit 2D tangent at distance S along the polyline. */
			void Sample(double S, FVector& OutPos, FVector2D& OutTangent) const
			{
				const TArray<FVector>& P = *Points;
				const int32 NumSeg = Cum.Num() - 1;
				int32 Seg = 0;
				while (Seg < NumSeg - 1 && Cum[Seg + 1] < S)
				{
					++Seg;
				}
				const FVector& A = P[Seg];
				const FVector& B = (Seg + 1 < P.Num()) ? P[Seg + 1] : P[0];
				const double SegLen = FMath::Max(Cum[Seg + 1] - Cum[Seg], 1e-6);
				const double T = FMath::Clamp((S - Cum[Seg]) / SegLen, 0.0, 1.0);
				OutPos = A + (B - A) * T;
				FVector2D Dir(B.X - A.X, B.Y - A.Y);
				const double L = FMath::Sqrt(Dir.X * Dir.X + Dir.Y * Dir.Y);
				OutTangent = (L > 1e-6) ? FVector2D(Dir.X / L, Dir.Y / L) : FVector2D(1, 0);
			}
		};

		class FPlanner
		{
		public:
			FPlanner(const FInputs& InInputs, FStats& InStats)
				: In(InInputs), Stats(InStats)
			{
				for (const FArea& A : In.Areas)
				{
					FAreaBounds B;
					B.Min = FVector2D(1e30, 1e30);
					B.Max = FVector2D(-1e30, -1e30);
					for (const FVector2D& P : A.Polygon)
					{
						B.Min.X = FMath::Min(B.Min.X, P.X); B.Min.Y = FMath::Min(B.Min.Y, P.Y);
						B.Max.X = FMath::Max(B.Max.X, P.X); B.Max.Y = FMath::Max(B.Max.Y, P.Y);
					}
					AreaBounds.Add(B);
				}
				BuildFocusPoints();
			}

			void RunRule(const FRule& Rule, TArray<FInstance>& Out)
			{
				if (In.CategoryAllowList.Num() > 0 && !In.CategoryAllowList.Contains(Rule.Category))
				{
					return;
				}
				RuleKept = 0;

				const FName Ctx = Rule.Context;
				if (Ctx == N().Roadside || Ctx == N().Sidewalk || Ctx == N().Curbside || Ctx == N().Alley || Ctx == N().Commercial)
				{
					for (const FRoad& Road : In.Roads)
					{
						if (Cancelled()) { return; }
						LinearRule(Rule, Road, Out);
					}
				}
				else if (Ctx == N().Junction)
				{
					for (const FJunction& J : In.Junctions)
					{
						if (Cancelled()) { return; }
						JunctionRule(Rule, J, Out);
					}
				}
				else if (Ctx == N().Park || Ctx == N().Plaza || Ctx == N().Site)
				{
					for (int32 I = 0; I < In.Areas.Num(); ++I)
					{
						if (Cancelled()) { return; }
						AreaScatterRule(Rule, I, Out);
					}
				}
				else if (Ctx == N().Rooftop || Ctx == N().Facade)
				{
					for (int32 I = 0; I < In.Areas.Num(); ++I)
					{
						if (Cancelled()) { return; }
						BuildingRule(Rule, I, Out);
					}
				}
			}

		private:
			const FInputs& In;
			FStats& Stats;
			TArray<FAreaBounds> AreaBounds;
			TArray<FVector2D> FocusPoints;
			int32 RuleKept = 0;

			bool Cancelled()
			{
				if (In.IsCancelled && In.IsCancelled())
				{
					Stats.bCancelled = true;
					return true;
				}
				return false;
			}

			void BuildFocusPoints()
			{
				// Cinematic priority: plazas, real junctions (3+ roads = hubs), and design/settings-provided camera areas.
				for (int32 I = 0; I < In.Areas.Num(); ++I)
				{
					if (In.Areas[I].Kind == N().Plaza && In.Areas[I].Polygon.Num() > 0)
					{
						FVector2D C(0, 0);
						for (const FVector2D& P : In.Areas[I].Polygon) { C += P; }
						FocusPoints.Add(C / static_cast<double>(In.Areas[I].Polygon.Num()));
					}
				}
				for (const FJunction& J : In.Junctions)
				{
					if (J.NumRoads >= 3) { FocusPoints.Add(FVector2D(J.Location.X, J.Location.Y)); }
				}
				for (const FVector& F : In.ExtraFocusPoints) { FocusPoints.Add(FVector2D(F.X, F.Y)); }
			}

			static float TierWeight(int32 Tier)
			{
				static const float W[] = { 1.0f, 0.9f, 0.7f, 0.5f, 0.35f, 0.25f };
				return W[FMath::Clamp(Tier, 0, 5)];
			}

			float Importance(const FVector& Pos, int32 Tier) const
			{
				float Focus = 0.0f;
				for (const FVector2D& F : FocusPoints)
				{
					const double D = FMath::Sqrt((Pos.X - F.X) * (Pos.X - F.X) + (Pos.Y - F.Y) * (Pos.Y - F.Y));
					Focus = FMath::Max(Focus, static_cast<float>(FMath::Clamp(1.0 - D / FMath::Max(In.FocusRadiusUnits, 1.0f), 0.0, 1.0)));
				}
				return FMath::Clamp(TierWeight(Tier) * 0.6f + Focus * 0.4f, 0.0f, 1.0f);
			}

			bool InBounds(const FVector& P) const
			{
				return !In.bUseBounds || (P.X >= In.BoundsMin.X && P.X <= In.BoundsMax.X && P.Y >= In.BoundsMin.Y && P.Y <= In.BoundsMax.Y);
			}

			bool InsideBuilding(const FVector2D& P) const
			{
				for (int32 I = 0; I < In.Areas.Num(); ++I)
				{
					if (In.Areas[I].Kind != N().Building) { continue; }
					const FAreaBounds& B = AreaBounds[I];
					if (P.X < B.Min.X || P.X > B.Max.X || P.Y < B.Min.Y || P.Y > B.Max.Y) { continue; }
					if (PointInPolygon(P, In.Areas[I].Polygon)) { return true; }
				}
				return false;
			}

			bool NearJunction(const FVector& P, float Extra) const
			{
				for (const FJunction& J : In.Junctions)
				{
					const double D = FMath::Sqrt((P.X - J.Location.X) * (P.X - J.Location.X) + (P.Y - J.Location.Y) * (P.Y - J.Location.Y));
					if (D < J.PadRadius + Extra) { return true; }
				}
				return false;
			}

			bool NearMatchingBuilding(const FVector2D& P, const FRule& Rule) const
			{
				for (int32 I = 0; I < In.Areas.Num(); ++I)
				{
					const FArea& A = In.Areas[I];
					if (A.Kind != N().Building) { continue; }
					if (!Rule.RequiredAreaUse.IsNone() && A.Use != Rule.RequiredAreaUse) { continue; }
					const FAreaBounds& B = AreaBounds[I];
					const double M = Rule.AreaProximityUnits;
					if (P.X < B.Min.X - M || P.X > B.Max.X + M || P.Y < B.Min.Y - M || P.Y > B.Max.Y + M) { continue; }
					if (DistanceToPolygonEdges(P, A.Polygon) <= Rule.AreaProximityUnits) { return true; }
				}
				return false;
			}

			/** Applies district/polish/importance/density/probability. Returns final keep-probability. */
			float KeepProbability(const FRule& Rule, float Importance) const
			{
				float P = Rule.Probability * In.DensityScale * In.District.DensityMultiplier;
				if (const float* CatMul = In.District.CategoryMultiplier.Find(Rule.Category))
				{
					P *= *CatMul;
				}
				P *= FMath::Max(0.0f, 1.0f + Rule.PolishResponse * (In.District.Polish - 0.5f) * 2.0f);
				P *= FMath::Lerp(In.MinImportanceDensity, 1.0f, Importance);
				return P;
			}

			/** Final accept/emit. Returns false if culled. */
			bool Emit(const FRule& Rule, FName Source, uint64 Index, const FVector& Pos, float Yaw, float Importance, int32 /*Tier*/, TArray<FInstance>& Out)
			{
				++Stats.Candidates;
				if (!InBounds(Pos)) { ++Stats.RejectedBounds; return false; }

				const uint64 Id = MakeStableId(In.GlobalSeed, In.DistrictId, Rule.RuleId, Source, Index);
				if (Hash01(Id, 1) >= KeepProbability(Rule, Importance)) { ++Stats.RejectedDensity; return false; }
				if (Rule.MaxPerDistrict > 0 && RuleKept >= Rule.MaxPerDistrict) { ++Stats.RejectedRuleCap; return false; }

				FInstance Inst;
				Inst.StableId = Id;
				Inst.RuleId = Rule.RuleId;
				Inst.Domain = Rule.Domain;
				Inst.Category = Rule.Category;
				Inst.SlotFilter = Rule.SlotFilter;
				Inst.Context = Rule.Context;
				Inst.SourceId = Source;
				Inst.Location = Pos;
				Inst.YawDegrees = Yaw + (Hash01(Id, 2) - 0.5f) * 2.0f * Rule.YawJitterDegrees;
				Inst.UniformScale = FMath::Lerp(Rule.ScaleMin, Rule.ScaleMax, Hash01(Id, 3));
				Inst.Importance = Importance;
				Inst.Priority = Rule.PriorityBase + Importance;
				Out.Add(Inst);
				++RuleKept;
				return true;
			}

			/** Emits Pos plus (optionally) a deterministic cluster around it. */
			void EmitWithCluster(const FRule& Rule, FName Source, uint64 Index, const FVector& Pos, float Yaw, float Importance, int32 Tier, TArray<FInstance>& Out)
			{
				const int32 ClusterMax = FMath::Max(Rule.ClusterMax, 1);
				const int32 ClusterMin = FMath::Clamp(Rule.ClusterMin, 1, ClusterMax);
				int32 Count = 1;
				if (ClusterMax > 1)
				{
					const uint64 Id = MakeStableId(In.GlobalSeed, In.DistrictId, Rule.RuleId, Source, Index);
					Count = ClusterMin + static_cast<int32>(Hash01(Id, 9) * static_cast<float>(ClusterMax - ClusterMin + 1));
					Count = FMath::Min(Count, ClusterMax);
				}
				for (int32 M = 0; M < Count; ++M)
				{
					FVector P = Pos;
					const uint64 SubIndex = Index * 64ull + static_cast<uint64>(M);
					if (M > 0)
					{
						const uint64 Id = MakeStableId(In.GlobalSeed, In.DistrictId, Rule.RuleId, Source, SubIndex);
						const float Ang = Hash01(Id, 11) * 2.0f * static_cast<float>(PI);
						const float Rad = Rule.ClusterRadius * FMath::Sqrt(Hash01(Id, 12));
						P += FVector(FMath::Cos(Ang) * Rad, FMath::Sin(Ang) * Rad, 0.0);
					}
					Emit(Rule, Source, SubIndex, P, Yaw, Importance, Tier, Out);
				}
			}

			// ---- Linear (road-following) rules ------------------------------

			void LinearRule(const FRule& Rule, const FRoad& Road, TArray<FInstance>& Out)
			{
				const FName Ctx = Rule.Context;

				if (Road.Points.Num() < 2 || Rule.SpacingUnits < 1.0f) { ++Stats.RejectedContext; return; }
				if (Road.Tier < Rule.MinTier || Road.Tier > Rule.MaxTier) { ++Stats.RejectedContext; return; }
				if (Road.bTunnel) { ++Stats.RejectedContext; return; }
				if (Road.bBridge && Rule.bSkipOnBridge) { ++Stats.RejectedContext; return; }
				if ((Rule.bRequireSidewalk || Ctx == N().Sidewalk) && !Road.bHasSidewalk) { ++Stats.RejectedContext; return; }
				if (Rule.bRequireMedian && !Road.bHasMedian) { ++Stats.RejectedContext; return; }
				if (Ctx == N().Alley && Road.Tier < 4) { ++Stats.RejectedContext; return; } // Service (4) / Alley (5) only

				FPolyline Line;
				Line.Build(Road.Points, Road.bClosedLoop);
				if (Line.Length < Rule.SpacingUnits * 0.5f) { ++Stats.RejectedContext; return; }

				const uint64 RoadHash = HashName(Road.Id);
				const double Margin = Road.bClosedLoop ? 0.0 : Rule.StartMargin;

				uint64 Index = 0;
				for (int32 K = 0; ; ++K)
				{
					const uint64 BaseId = MakeStableId(In.GlobalSeed, In.DistrictId, Rule.RuleId, Road.Id, static_cast<uint64>(K));
					const double Jit = (Hash01(BaseId, 20) - 0.5) * 2.0 * Rule.AlongJitterFraction * Rule.SpacingUnits;
					const double S = Margin + (K + 0.5) * Rule.SpacingUnits + Jit;
					if (S >= Line.Length - Margin) { break; }
					if (S < Margin) { continue; }

					FVector Pos;
					FVector2D T;
					Line.Sample(S, Pos, T);
					const FVector2D R = RightOf(T);
					const float Imp = Importance(Pos, Road.Tier);

					int32 Sides[2] = { 1, -1 };
					int32 NumSides = 2;
					if (Rule.LateralMode == ELateralMode::Median) { Sides[0] = 0; NumSides = 1; }
					else if (!Rule.bBothSides || Rule.bAlternateSides)
					{
						Sides[0] = (Rule.bAlternateSides ? ((K & 1) ? -1 : 1) : ((Hash01(BaseId, 21) < 0.5f) ? 1 : -1));
						NumSides = 1;
					}

					for (int32 SI = 0; SI < NumSides; ++SI)
					{
						const int32 Side = Sides[SI];
						const uint64 SlotIndex = static_cast<uint64>(K) * 2ull + static_cast<uint64>(Side < 0 ? 1 : 0);
						++Index;

						double Offset = 0.0;
						switch (Rule.LateralMode)
						{
						case ELateralMode::Median:      Offset = 0.0; break;
						case ELateralMode::RoadEdge:    Offset = Road.HalfWidth - Rule.LateralInsetUnits; break;
						default:                        Offset = Road.BandInner + (Road.BandOuter - Road.BandInner) * Rule.LateralFraction; break;
						}
						const uint64 SideId = MakeStableId(In.GlobalSeed ^ RoadHash, In.DistrictId, Rule.RuleId, Road.Id, SlotIndex);
						Offset += (Hash01(SideId, 22) - 0.5) * 2.0 * Rule.LateralJitterUnits;

						FVector P = Pos + FVector(R.X * Side * Offset, R.Y * Side * Offset, Rule.LateralMode == ELateralMode::SidewalkBand ? Road.BandZOffset : 0.0);

						if (NearJunction(P, Rule.JunctionClearance)) { ++Stats.Candidates; ++Stats.RejectedJunctionClearance; continue; }
						if (Rule.LateralMode != ELateralMode::RoadEdge && InsideBuilding(FVector2D(P.X, P.Y))) { ++Stats.Candidates; ++Stats.RejectedInsideBuilding; continue; }
						if (Ctx == N().Commercial && !NearMatchingBuilding(FVector2D(P.X, P.Y), Rule)) { ++Stats.Candidates; ++Stats.RejectedContext; continue; }

						FVector2D Facing = T; // AlongTraffic
						switch (Rule.YawMode)
						{
						case EYawMode::AlongTraffic:
						{
							// Right-hand traffic: the right side of the road travels forward; the left side travels backward.
							const bool bForward = (Side >= 0) != In.bLeftHandTraffic;
							Facing = bForward ? T : FVector2D(-T.X, -T.Y);
							break;
						}
						case EYawMode::FaceRoad:        Facing = FVector2D(-R.X * Side, -R.Y * Side); break;
						case EYawMode::OutwardFromArea: Facing = FVector2D(R.X * Side, R.Y * Side); break;
						case EYawMode::Random:
							Facing = FVector2D(FMath::Cos(Hash01(SideId, 23) * 2.0 * PI), FMath::Sin(Hash01(SideId, 23) * 2.0 * PI));
							break;
						}
						if (Side == 0 && Rule.YawMode == EYawMode::FaceRoad) { Facing = T; }

						EmitWithCluster(Rule, Road.Id, SlotIndex, P, YawOf(Facing), Imp, Road.Tier, Out);
					}
				}
				(void)Index;
			}

			// ---- Junction rules ---------------------------------------------

			void JunctionRule(const FRule& Rule, const FJunction& J, TArray<FInstance>& Out)
			{
				if (J.NumRoads < Rule.MinJunctionRoads || J.NumRoads > Rule.MaxJunctionRoads) { ++Stats.RejectedContext; return; }

				int32 ApproachIdx = 0;
				for (const FApproach& A : J.Approaches)
				{
					const FVector2D R = RightOf(A.Dir);
					for (int32 SideIdx = 0; SideIdx < 2; ++SideIdx)
					{
						const int32 Side = (SideIdx == 0) ? 1 : -1;
						if (!Rule.bBothSides)
						{
							// One corner per approach: the corner on the driver's right (right-hand traffic).
							const int32 Wanted = In.bLeftHandTraffic ? 1 : -1; // approaching traffic travels -Dir; its right is -RightOf(Dir)
							if (Side != Wanted) { continue; }
						}
						const uint64 Index = static_cast<uint64>(ApproachIdx) * 2ull + static_cast<uint64>(SideIdx);
						const double Along = J.PadRadius + 80.0 + Rule.JunctionClearance;
						const double Lat = A.HalfWidth + (A.BandOuter - A.HalfWidth) * FMath::Clamp(Rule.LateralFraction, 0.0f, 1.0f);
						const FVector P = J.Location + FVector(A.Dir.X * Along + R.X * Side * Lat, A.Dir.Y * Along + R.Y * Side * Lat, 0.0);
						const float Imp = Importance(P, FMath::Min(2, 1 + (J.NumRoads >= 3 ? 0 : 1)));
						float Yaw = YawOf(A.Dir); // faces oncoming traffic
						if (Rule.YawMode == EYawMode::FaceRoad) { Yaw = YawOf(FVector2D(-R.X * Side, -R.Y * Side)); }
						if (Rule.YawMode == EYawMode::Random) { Yaw = Hash01(MakeStableId(In.GlobalSeed, In.DistrictId, Rule.RuleId, J.Key, Index), 23) * 360.0f; }
						if (!InsideBuilding(FVector2D(P.X, P.Y)))
						{
							EmitWithCluster(Rule, J.Key, Index, P, Yaw, Imp, 2, Out);
						}
						else { ++Stats.Candidates; ++Stats.RejectedInsideBuilding; }
					}
					++ApproachIdx;
				}

				// Dead ends with no approaches recorded still deserve a marker at the centre.
				if (J.Approaches.Num() == 0 && Rule.MaxJunctionRoads <= 1)
				{
					EmitWithCluster(Rule, J.Key, 0, J.Location, 0.0f, Importance(J.Location, 4), 4, Out);
				}
			}

			// ---- Area scatter (Park / Plaza / Site) ---------------------------

			bool AreaMatches(const FRule& Rule, const FArea& A) const
			{
				if (Rule.Context == N().Park)  { return A.Kind == N().Park; }
				if (Rule.Context == N().Plaza) { return A.Kind == N().Plaza; }
				if (Rule.Context == N().Site)  { return !Rule.RequiredAreaUse.IsNone() && A.Use == Rule.RequiredAreaUse; }
				return false;
			}

			void AreaScatterRule(const FRule& Rule, int32 AreaIdx, TArray<FInstance>& Out)
			{
				const FArea& Area = In.Areas[AreaIdx];
				if (!AreaMatches(Rule, Area) || Area.Polygon.Num() < 3 || Rule.DensityPer100SqM <= 0.0f) { ++Stats.RejectedContext; return; }

				const FAreaBounds& B = AreaBounds[AreaIdx];
				const double Cell = FMath::Sqrt(1000000.0 / static_cast<double>(Rule.DensityPer100SqM));
				const int32 NX = FMath::Max(1, FMath::CeilToInt((B.Max.X - B.Min.X) / Cell));
				const int32 NY = FMath::Max(1, FMath::CeilToInt((B.Max.Y - B.Min.Y) / Cell));
				if (static_cast<int64>(NX) * NY > 4000000) { return; } // safety valve against pathological input

				for (int32 IY = 0; IY < NY; ++IY)
				{
					if (Cancelled()) { return; }
					for (int32 IX = 0; IX < NX; ++IX)
					{
						const uint64 Index = static_cast<uint64>(IY) * 100000ull + static_cast<uint64>(IX);
						const uint64 Id = MakeStableId(In.GlobalSeed, In.DistrictId, Rule.RuleId, Area.Id, Index);
						const FVector2D P2(B.Min.X + (IX + Hash01(Id, 30)) * Cell, B.Min.Y + (IY + Hash01(Id, 31)) * Cell);
						if (!PointInPolygon(P2, Area.Polygon)) { continue; } // outside the area is not a candidate at all
						if (Area.Kind != N().Building && InsideBuilding(P2)) { ++Stats.Candidates; ++Stats.RejectedInsideBuilding; continue; }
						const FVector P(P2.X, P2.Y, Area.Height);
						const float Imp = Importance(P, 2);
						EmitWithCluster(Rule, Area.Id, Index, P, Hash01(Id, 32) * 360.0f, Imp, 2, Out);
					}
				}
			}

			// ---- Building rules (Rooftop / Facade) ---------------------------

			void BuildingRule(const FRule& Rule, int32 AreaIdx, TArray<FInstance>& Out)
			{
				const FArea& A = In.Areas[AreaIdx];
				if (A.Kind != N().Building || A.Polygon.Num() < 3) { return; }
				if (!Rule.RequiredAreaUse.IsNone() && A.Use != Rule.RequiredAreaUse) { ++Stats.RejectedContext; return; }
				if (A.Height < Rule.MinBuildingHeight) { ++Stats.RejectedContext; return; }

				if (Rule.Context == N().Rooftop)
				{
					FVector2D C(0, 0);
					for (const FVector2D& P : A.Polygon) { C += P; }
					C /= static_cast<double>(A.Polygon.Num());
					const FVector2D Edge = A.Polygon[1] - A.Polygon[0];
					const FVector P(C.X, C.Y, A.Height + Rule.ZOffset);
					EmitWithCluster(Rule, A.Id, 0, P, YawOf(Edge), Importance(P, 2), 2, Out);
					return;
				}

				// Facade: only edges that face a road within reach, so we never decorate walls nobody sees.
				const double Orient = (SignedArea(A.Polygon) >= 0.0) ? 1.0 : -1.0; // outward normal for CCW is (Ey, -Ex)
				const int32 N = A.Polygon.Num();
				for (int32 E = 0; E < N; ++E)
				{
					const FVector2D P0 = A.Polygon[E];
					const FVector2D P1 = A.Polygon[(E + 1) % N];
					const FVector2D D = P1 - P0;
					const double L = FMath::Sqrt(D.X * D.X + D.Y * D.Y);
					if (L < 300.0) { continue; }
					const FVector2D Normal(D.Y / L * Orient, -D.X / L * Orient);
					const FVector2D Mid = (P0 + P1) * 0.5;
					const FVector2D Probe = Mid + Normal * 200.0;
					if (DistanceToRoads(Probe, In.Roads) > Rule.AreaProximityUnits * 2.0f) { ++Stats.Candidates; ++Stats.RejectedContext; continue; }
					const FVector P(Mid.X + Normal.X * 20.0, Mid.Y + Normal.Y * 20.0, FMath::Min(Rule.ZOffset, A.Height));
					EmitWithCluster(Rule, A.Id, static_cast<uint64>(E), P, YawOf(Normal), Importance(P, 2), 2, Out);
				}
			}
		};
	}

	// ======================================================================
	// Public entry point
	// ======================================================================

	TArray<FInstance> Plan(const FInputs& Inputs, const TArray<FRule>& Rules, FStats* OutStats)
	{
		FStats LocalStats;
		FStats& Stats = OutStats ? *OutStats : LocalStats;
		Stats = FStats();

		TArray<FInstance> Result;
		FPlanner Planner(Inputs, Stats);
		for (const FRule& Rule : Rules)
		{
			Planner.RunRule(Rule, Result);
			if (Stats.bCancelled) { break; }
		}

		// Budget: keep the highest-priority instances (cinematic-relevant + rule priority); ties broken by id -> deterministic.
		if (Inputs.MaxInstances > 0 && Result.Num() > Inputs.MaxInstances)
		{
			Result.Sort([](const FInstance& A, const FInstance& B)
			{
				return (A.Priority != B.Priority) ? (A.Priority > B.Priority) : (A.StableId < B.StableId);
			});
			Stats.RejectedBudget = Result.Num() - Inputs.MaxInstances;
			Result.SetNum(Inputs.MaxInstances);
		}

		// Canonical order: independent of rule order / iteration details.
		Result.Sort([](const FInstance& A, const FInstance& B) { return A.StableId < B.StableId; });

		Stats.Kept = Result.Num();
		for (const FInstance& I : Result)
		{
			int32& C = Stats.KeptPerCategory.FindOrAdd(I.Category);
			++C;
		}
		return Result;
	}
}
