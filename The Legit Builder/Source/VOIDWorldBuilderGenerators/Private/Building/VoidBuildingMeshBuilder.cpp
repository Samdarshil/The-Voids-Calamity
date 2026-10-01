// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Building/VoidBuildingMeshBuilder.h"
#include "Building/VoidBuildingGeometry.h"

// ---------------------------------------------------------------------------------------------
// Accumulator
// ---------------------------------------------------------------------------------------------

void FVoidBuildingMeshAccumulator::AddTriangle(EVoidBuildingSurface Surface, const FVector& A, const FVector& B, const FVector& C, const FLinearColor& Color,
	const FVector2D& UVA, const FVector2D& UVB, const FVector2D& UVC)
{
	FVoidBuildingMeshSection& S = Get(Surface);
	FVector N = FVector::CrossProduct(B - A, C - A);
	N = N.GetSafeNormal();
	if (N.IsNearlyZero()) { N = FVector(0.0, 0.0, 1.0); }

	const int32 Base = S.Vertices.Num();
	S.Vertices.Add(A - Origin);
	S.Vertices.Add(B - Origin);
	S.Vertices.Add(C - Origin);
	S.Normals.Add(N); S.Normals.Add(N); S.Normals.Add(N);
	S.UVs.Add(UVA); S.UVs.Add(UVB); S.UVs.Add(UVC);
	S.VertexColors.Add(Color); S.VertexColors.Add(Color); S.VertexColors.Add(Color);
	S.Triangles.Add(Base); S.Triangles.Add(Base + 1); S.Triangles.Add(Base + 2);
}

void FVoidBuildingMeshAccumulator::AddQuad(EVoidBuildingSurface Surface, const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FLinearColor& Color,
	float U0, float U1, float V0, float V1)
{
	AddTriangle(Surface, A, B, C, Color, FVector2D(U0, V0), FVector2D(U1, V0), FVector2D(U1, V1));
	AddTriangle(Surface, A, C, D, Color, FVector2D(U0, V0), FVector2D(U1, V1), FVector2D(U0, V1));
}

int32 FVoidBuildingMeshAccumulator::NumTriangles() const
{
	int32 Total = 0;
	for (const FVoidBuildingMeshSection& S : Sections) { Total += S.Triangles.Num() / 3; }
	return Total;
}

int32 FVoidBuildingMeshAccumulator::NumVertices() const
{
	int32 Total = 0;
	for (const FVoidBuildingMeshSection& S : Sections) { Total += S.Vertices.Num(); }
	return Total;
}

// ---------------------------------------------------------------------------------------------
// Emitters
// ---------------------------------------------------------------------------------------------

namespace
{
	using G = FVoidBuildingGeometry;
	using Surf = EVoidBuildingSurface;

	struct FCtx
	{
		FVoidBuildingMeshAccumulator& Acc;
		const FVoidNormalizedBuilding& B;
		const FVoidBuildingMassPlan& Plan;
		const FVoidBuildingGenerationParams& Params;
		FLinearColor Wall, Roof, Glass, Door, Trim, Foundation, Technical;
	};

	FLinearColor Scale(const FLinearColor& C, float S)
	{
		return FLinearColor(FMath::Clamp(C.R * S, 0.0f, 1.0f), FMath::Clamp(C.G * S, 0.0f, 1.0f), FMath::Clamp(C.B * S, 0.0f, 1.0f), 1.0f);
	}

	FVector V3(const FVector2D& P, double Z) { return FVector(P.X, P.Y, Z); }

	/** Vertical quad along the edge P->Q, facing the right-hand side of P->Q (outward for a CCW outline). Offset shifts the quad along that normal. */
	void WallQuad(FCtx& C, Surf Surface, const FVector2D& P, const FVector2D& Q, double Z0, double Z1, const FLinearColor& Color, double Offset = 0.0)
	{
		FVector2D A = P, Bp = Q;
		if (Offset != 0.0)
		{
			const FVector2D D = G::SafeNormal(Q - P);
			const FVector2D N(D.Y, -D.X);
			A = FVector2D(P.X + N.X * Offset, P.Y + N.Y * Offset);
			Bp = FVector2D(Q.X + N.X * Offset, Q.Y + N.Y * Offset);
		}
		const float Len = static_cast<float>(G::Length(Q - P)) / 100.0f;
		C.Acc.AddQuad(Surface, V3(A, Z0), V3(Bp, Z0), V3(Bp, Z1), V3(A, Z1), Color, 0.0f, Len, static_cast<float>(Z0) / 100.0f, static_cast<float>(Z1) / 100.0f);
	}

	void WallRing(FCtx& C, Surf Surface, const TArray<FVector2D>& Outline, double Z0, double Z1, const FLinearColor& Color, bool bInward)
	{
		const int32 N = Outline.Num();
		for (int32 I = 0; I < N; ++I)
		{
			const FVector2D& P = Outline[I];
			const FVector2D& Q = Outline[(I + 1) % N];
			if (bInward) { WallQuad(C, Surface, Q, P, Z0, Z1, Color); }
			else         { WallQuad(C, Surface, P, Q, Z0, Z1, Color); }
		}
	}

	void CapTri(FCtx& C, Surf Surface, const FVector& A, const FVector& B, const FVector& Cc, const FLinearColor& Color)
	{
		C.Acc.AddTriangle(Surface, A, B, Cc, Color, FVector2D(A.X / 100.0, A.Y / 100.0), FVector2D(B.X / 100.0, B.Y / 100.0), FVector2D(Cc.X / 100.0, Cc.Y / 100.0));
	}

	/** Upward-facing polygon cap. */
	void Cap(FCtx& C, Surf Surface, const TArray<FVector2D>& Outline, double Z, const FLinearColor& Color)
	{
		TArray<int32> Idx;
		if (G::Triangulate(Outline, Idx))
		{
			for (int32 I = 0; I + 2 < Idx.Num(); I += 3)
			{
				CapTri(C, Surface, V3(Outline[Idx[I]], Z), V3(Outline[Idx[I + 1]], Z), V3(Outline[Idx[I + 2]], Z), Color);
			}
		}
		else
		{
			for (int32 I = 1; I + 1 < Outline.Num(); ++I) // fan fallback, valid for star-shaped outlines
			{
				CapTri(C, Surface, V3(Outline[0], Z), V3(Outline[I], Z), V3(Outline[I + 1], Z), Color);
			}
		}
	}

	/** Upward-facing strip between an outline and its same-vertex-count inset. */
	void RingCap(FCtx& C, Surf Surface, const TArray<FVector2D>& Outer, const TArray<FVector2D>& Inner, double Z, const FLinearColor& Color)
	{
		const int32 N = Outer.Num();
		if (Inner.Num() != N) { Cap(C, Surface, Outer, Z, Color); return; }
		for (int32 I = 0; I < N; ++I)
		{
			const int32 J = (I + 1) % N;
			CapTri(C, Surface, V3(Outer[I], Z), V3(Outer[J], Z), V3(Inner[J], Z), Color);
			CapTri(C, Surface, V3(Outer[I], Z), V3(Inner[J], Z), V3(Inner[I], Z), Color);
		}
	}

	void Box(FCtx& C, Surf Surface, const FVector2D& Center, const FVector2D& Half, double Yaw, double Z0, double Z1, const FLinearColor& Color)
	{
		const double Cs = FMath::Cos(Yaw), Sn = FMath::Sin(Yaw);
		const double SX[4] = { -1, 1, 1, -1 };
		const double SY[4] = { -1, -1, 1, 1 };
		TArray<FVector2D> Corners;
		for (int32 I = 0; I < 4; ++I)
		{
			const double LX = SX[I] * Half.X, LY = SY[I] * Half.Y;
			Corners.Add(FVector2D(Center.X + LX * Cs - LY * Sn, Center.Y + LX * Sn + LY * Cs));
		}
		WallRing(C, Surface, Corners, Z0, Z1, Color, false);
		Cap(C, Surface, Corners, Z1, Color);
	}

	// ---- windows -------------------------------------------------------------------------------

	struct FSpan { double A; double B; };

	/** Door footprints on edge EdgeIndex, as [u0,u1] along the wall. */
	void DoorSpans(const FCtx& C, int32 EdgeIndex, const FVector2D& P, const FVector2D& Dir, TArray<FSpan>& Out)
	{
		Out.Reset();
		for (const FVoidBuildingEntrance& D : C.B.Entrances)
		{
			if (D.EdgeIndex != EdgeIndex) { continue; }
			const double U = G::Dot(D.Location - P, Dir);
			Out.Add({ U - D.Width * 0.5 - 60.0, U + D.Width * 0.5 + 60.0 });
		}
	}

	/** Emits (or, with bCountOnly, only counts) the windows of one wall loop of one block. */
	void WindowsForLoop(FCtx& C, const FVoidMassBlock& Block, const TArray<FVector2D>& Loop, bool bInward, EVoidWindowStyle Style, bool bCountOnly, int32& Quads)
	{
		if (Style == EVoidWindowStyle::None) { return; }
		const FVoidWindowParams& W = C.Plan.Windows;
		const int32 N = Loop.Num();
		const double FH = C.Plan.FloorHeight;
		const bool bHasDoorsHere = !bInward && Block.FirstFloor == 0 && Loop.Num() == C.B.Footprint.Num();

		for (int32 I = 0; I < N; ++I)
		{
			const FVector2D P = bInward ? Loop[(I + 1) % N] : Loop[I];
			const FVector2D Q = bInward ? Loop[I] : Loop[(I + 1) % N];
			const double Len = G::Length(Q - P);
			if (Len < 500.0) { continue; }
			const FVector2D Dir = G::SafeNormal(Q - P);

			const double Margin = 120.0;
			const double Span = Len - 2.0 * Margin;
			if (Span < 200.0) { continue; }

			TArray<FSpan> Doors;
			if (bHasDoorsHere) { DoorSpans(C, I, P, Dir, Doors); }

			auto OverlapsDoor = [&Doors](double U0, double U1)
			{
				for (const FSpan& D : Doors) { if (U1 > D.A && U0 < D.B) { return true; } }
				return false;
			};

			const bool bPunched = (Style == EVoidWindowStyle::Punched);
			const int32 Bays = bPunched ? FMath::Max(1, FMath::FloorToInt(static_cast<float>(Span / W.BayUnits))) : 1;
			const double Spacing = Span / Bays;
			const double WinW = Spacing * W.WidthFrac;

			// Ribbon / curtain runs: the full wall, and (ground floor only) the wall minus door gaps.
			TArray<FSpan> RunsGround, RunsUpper;
			if (!bPunched)
			{
				RunsUpper.Add({ Margin, Len - Margin });
				double Cursor = Margin;
				TArray<FSpan> Sorted = Doors;
				Sorted.Sort([](const FSpan& L, const FSpan& R) { return L.A < R.A; });
				for (const FSpan& D : Sorted)
				{
					if (D.A > Cursor + 150.0) { RunsGround.Add({ Cursor, D.A }); }
					Cursor = FMath::Max(Cursor, D.B);
				}
				if (Len - Margin > Cursor + 150.0) { RunsGround.Add({ Cursor, Len - Margin }); }
			}

			for (int32 F = Block.FirstFloor; F < Block.FirstFloor + Block.NumFloors; ++F)
			{
				const bool bGround = (F == 0);
				const float SillF = (bGround && W.GroundSillFrac >= 0.0f) ? W.GroundSillFrac : W.SillFrac;
				const float HeightF = (bGround && W.GroundSillFrac >= 0.0f) ? W.GroundHeightFrac : W.HeightFrac;
				const double Z0 = C.Plan.BaseZ + F * FH + SillF * FH;
				const double Z1 = FMath::Min(Z0 + HeightF * FH, C.Plan.BaseZ + (F + 1) * FH - 20.0);
				if (Z1 <= Z0 + 10.0) { continue; }

				if (bPunched)
				{
					for (int32 K = 0; K < Bays; ++K)
					{
						const double Uc = Margin + Spacing * (K + 0.5);
						const double U0 = Uc - WinW * 0.5, U1 = Uc + WinW * 0.5;
						if (bGround && OverlapsDoor(U0, U1)) { continue; }
						++Quads;
						if (!bCountOnly)
						{
							const FVector2D A(P.X + Dir.X * U0, P.Y + Dir.Y * U0);
							const FVector2D B(P.X + Dir.X * U1, P.Y + Dir.Y * U1);
							WallQuad(C, Surf::Glass, A, B, Z0, Z1, C.Glass, W.Depth);
						}
					}
				}
				else
				{
					for (const FSpan& R : (bGround ? RunsGround : RunsUpper))
					{
						if (bGround && R.B - R.A < 150.0) { continue; }
						++Quads;
						if (!bCountOnly)
						{
							const FVector2D A(P.X + Dir.X * R.A, P.Y + Dir.Y * R.A);
							const FVector2D B(P.X + Dir.X * R.B, P.Y + Dir.Y * R.B);
							WallQuad(C, Surf::Glass, A, B, Z0, Z1, C.Glass, W.Depth);
						}
					}
				}
			}
		}
	}

	EVoidWindowStyle EffectiveStyle(EVoidWindowStyle BlockStyle, EVoidBuildingWindowDetail Detail, bool bBudgetExceeded)
	{
		if (Detail == EVoidBuildingWindowDetail::None) { return EVoidWindowStyle::None; }
		if (BlockStyle == EVoidWindowStyle::Punched)
		{
			if (Detail == EVoidBuildingWindowDetail::Ribbon) { return EVoidWindowStyle::Ribbon; }
			if (Detail == EVoidBuildingWindowDetail::Auto && bBudgetExceeded) { return EVoidWindowStyle::Ribbon; }
		}
		return BlockStyle;
	}

	// ---- string courses ------------------------------------------------------------------------

	void StringCourses(FCtx& C, const FVoidMassBlock& Block)
	{
		const int32 Every = C.Plan.Windows.StringCourseEvery;
		if (!Block.bStringCourses || Every <= 0) { return; }
		const int32 N = Block.Outline.Num();
		for (int32 F = Block.FirstFloor + 1; F < Block.FirstFloor + Block.NumFloors; ++F)
		{
			if (F % Every != 0) { continue; }
			const double Z = C.Plan.BaseZ + F * C.Plan.FloorHeight;
			for (int32 I = 0; I < N; ++I)
			{
				WallQuad(C, Surf::Trim, Block.Outline[I], Block.Outline[(I + 1) % N], Z - 14.0, Z + 14.0, C.Trim, C.Plan.Windows.Depth + 1.0);
			}
		}
	}

	// ---- roof ----------------------------------------------------------------------------------

	void Roof(FCtx& C, const FVoidMassBlock& Block, const FVoidMassBlock* Next)
	{
		const double Z = Block.ZTop;

		if (Block.bCapToNext && Next)
		{
			RingCap(C, Surf::Roof, Block.Outline, Next->Outline, Z, C.Roof);
			return;
		}
		if (Block.CourtyardOutline.Num() > 0)
		{
			RingCap(C, Surf::Roof, Block.Outline, Block.CourtyardOutline, Z, C.Roof);
			return;
		}

		switch (Block.Roof)
		{
		case EVoidRoofKind::Pyramid:
		{
			const double Rise = FMath::Clamp(G::MinDimension(Block.Outline) * 0.25, 150.0, 600.0);
			const FVector Apex = V3(G::Centroid(Block.Outline), Z + Rise);
			const int32 N = Block.Outline.Num();
			for (int32 I = 0; I < N; ++I)
			{
				CapTri(C, Surf::Roof, V3(Block.Outline[I], Z), V3(Block.Outline[(I + 1) % N], Z), Apex, C.Roof);
			}
			break;
		}
		case EVoidRoofKind::Parapet:
		{
			Cap(C, Surf::Roof, Block.Outline, Z, C.Roof);
			TArray<FVector2D> Inner;
			if (G::InsetPolygon(Block.Outline, 22.0, Inner))
			{
				const double H = 60.0;
				WallRing(C, Surf::Wall, Block.Outline, Z, Z + H, C.Wall, false);
				WallRing(C, Surf::Wall, Inner, Z, Z + H, C.Wall, true);
				RingCap(C, Surf::Trim, Block.Outline, Inner, Z + H, C.Trim);
			}
			break;
		}
		default:
			Cap(C, Surf::Roof, Block.Outline, Z, C.Roof);
			break;
		}
	}

	// ---- entrances -----------------------------------------------------------------------------

	void Entrances(FCtx& C)
	{
		const int32 N = C.B.Footprint.Num();
		for (const FVoidBuildingEntrance& D : C.B.Entrances)
		{
			if (D.EdgeIndex < 0 || D.EdgeIndex >= N) { continue; }
			const FVector2D P = C.B.Footprint[D.EdgeIndex];
			const FVector2D Q = C.B.Footprint[(D.EdgeIndex + 1) % N];
			const FVector2D Dir = G::SafeNormal(Q - P);
			const double Depth = C.Plan.Windows.Depth;
			const double Z0 = C.Plan.BaseZ;

			auto Span = [&](double HalfW, double Z1, Surf S, const FLinearColor& Col, double Off)
			{
				const FVector2D A(D.Location.X - Dir.X * HalfW, D.Location.Y - Dir.Y * HalfW);
				const FVector2D B(D.Location.X + Dir.X * HalfW, D.Location.Y + Dir.Y * HalfW);
				WallQuad(C, S, A, B, Z0, Z1, Col, Off);
			};

			Span(D.Width * 0.5 + 20.0, D.Height + 20.0, Surf::Trim, C.Trim, Depth * 0.5);          // frame
			Span(D.Width * 0.5,        D.Height,        Surf::Glass, C.Door, Depth * 0.5 + 1.5);   // leaf / glazing

			if (D.bCanopy)
			{
				const double Yaw = FMath::Atan2(Dir.Y, Dir.X);
				const double Reach = 150.0;
				const FVector2D Centre(D.Location.X + D.OutwardNormal.X * Reach * 0.5, D.Location.Y + D.OutwardNormal.Y * Reach * 0.5);
				Box(C, Surf::Trim, Centre, FVector2D(D.Width * 0.5 + 100.0, Reach * 0.5), Yaw, Z0 + D.Height + 30.0, Z0 + D.Height + 55.0, C.Trim);
			}
		}
	}
}

FLinearColor FVoidBuildingMeshBuilder::CategoryColor(EVoidBuildingCategory Category)
{
	switch (Category)
	{
	case EVoidBuildingCategory::Residential:   return FLinearColor(0.62f, 0.55f, 0.48f);
	case EVoidBuildingCategory::Commercial:    return FLinearColor(0.55f, 0.58f, 0.62f);
	case EVoidBuildingCategory::Office:        return FLinearColor(0.42f, 0.50f, 0.60f);
	case EVoidBuildingCategory::Industrial:    return FLinearColor(0.45f, 0.44f, 0.40f);
	case EVoidBuildingCategory::Civic:         return FLinearColor(0.72f, 0.70f, 0.64f);
	case EVoidBuildingCategory::Institutional: return FLinearColor(0.60f, 0.52f, 0.46f);
	case EVoidBuildingCategory::Medical:       return FLinearColor(0.78f, 0.80f, 0.82f);
	case EVoidBuildingCategory::Government:    return FLinearColor(0.66f, 0.66f, 0.68f);
	case EVoidBuildingCategory::MixedUse:      return FLinearColor(0.58f, 0.54f, 0.52f);
	case EVoidBuildingCategory::Landmark:      return FLinearColor(0.80f, 0.74f, 0.60f);
	default:                                   return FLinearColor(0.60f, 0.60f, 0.60f);
	}
}

int32 FVoidBuildingMeshBuilder::EstimatePunchedWindowQuads(const FVoidBuildingMassPlan& Plan)
{
	int32 Total = 0;
	for (const FVoidMassBlock& Block : Plan.Blocks)
	{
		if (Block.WindowStyle == EVoidWindowStyle::None) { continue; }
		auto CountLoop = [&](const TArray<FVector2D>& Loop)
		{
			const int32 N = Loop.Num();
			for (int32 I = 0; I < N; ++I)
			{
				const double Span = G::Length(Loop[(I + 1) % N] - Loop[I]) - 240.0;
				if (Span < 200.0) { continue; }
				const int32 Bays = (Block.WindowStyle == EVoidWindowStyle::Punched) ? FMath::Max(1, FMath::FloorToInt(static_cast<float>(Span / Plan.Windows.BayUnits))) : 1;
				Total += Bays * Block.NumFloors;
			}
		};
		CountLoop(Block.Outline);
		if (Block.CourtyardOutline.Num() > 0) { CountLoop(Block.CourtyardOutline); }
	}
	return Total;
}

void FVoidBuildingMeshBuilder::Emit(
	const FVoidNormalizedBuilding& B,
	const FVoidBuildingMassPlan& Plan,
	const FVoidBuildingGenerationParams& Params,
	FVoidBuildingMeshAccumulator& Acc)
{
	if (Plan.Blocks.Num() == 0) { return; }

	FVoidBuildingRng ColorRng(static_cast<uint32>(B.Seed) ^ 0x51ED270Bu);
	const float Tint = static_cast<float>(ColorRng.Range(0.93, 1.07));
	const FLinearColor Base = Scale(CategoryColor(B.Category), Tint);

	FCtx C
	{
		Acc, B, Plan, Params,
		Base,
		Scale(Base, 0.55f),
		FLinearColor(0.10f, 0.16f, 0.22f),
		FLinearColor(0.06f, 0.09f, 0.12f),
		Scale(Base, 1.15f),
		FLinearColor(0.32f, 0.31f, 0.30f),
		FLinearColor(0.36f, 0.37f, 0.40f)
	};

	// Foundation skirt under the first block: buildings never float on sloped ground.
	if (Plan.FoundationDepth > 0.0)
	{
		WallRing(C, Surf::Foundation, Plan.Blocks[0].Outline, Plan.BaseZ - Plan.FoundationDepth, Plan.BaseZ, C.Foundation, false);
	}

	// Window budget: only relevant to Auto, which falls back to ribbons for heavy buildings.
	const bool bBudgetExceeded = Params.WindowDetail == EVoidBuildingWindowDetail::Auto
		&& FVoidBuildingMeshBuilder::EstimatePunchedWindowQuads(Plan) > Params.MaxWindowQuadsPerBuilding;

	for (int32 I = 0; I < Plan.Blocks.Num(); ++I)
	{
		const FVoidMassBlock& Block = Plan.Blocks[I];
		const FVoidMassBlock* Next = Plan.Blocks.IsValidIndex(I + 1) ? &Plan.Blocks[I + 1] : nullptr;

		WallRing(C, Surf::Wall, Block.Outline, Block.ZBottom, Block.ZTop, C.Wall, false);
		if (Block.CourtyardOutline.Num() > 0)
		{
			WallRing(C, Surf::Wall, Block.CourtyardOutline, Block.ZBottom, Block.ZTop, C.Wall, true);
			Cap(C, Surf::Roof, Block.CourtyardOutline, Block.ZBottom, Scale(C.Foundation, 1.1f)); // courtyard floor
		}

		Roof(C, Block, Next);
		StringCourses(C, Block);

		if (Params.bGenerateWindows)
		{
			const EVoidWindowStyle Style = EffectiveStyle(Block.WindowStyle, Params.WindowDetail, bBudgetExceeded);
			int32 Unused = 0;
			WindowsForLoop(C, Block, Block.Outline, false, Style, false, Unused);
			if (Block.CourtyardOutline.Num() > 0)
			{
				WindowsForLoop(C, Block, Block.CourtyardOutline, true, Style, false, Unused);
			}
		}
	}

	if (Params.bGenerateEntrances)
	{
		Entrances(C);
	}

	for (const FVoidRooftopBox& R : Plan.RooftopBoxes)
	{
		Box(C, Surf::Technical, R.Center, R.HalfExtent, R.YawRadians, R.ZBottom, R.ZBottom + R.Height, C.Technical);
	}
}
