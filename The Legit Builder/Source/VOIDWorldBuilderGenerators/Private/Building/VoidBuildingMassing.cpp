// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Building/VoidBuildingMassing.h"
#include "Building/VoidBuildingGeometry.h"

namespace
{
	using G = FVoidBuildingGeometry;
	using Cat = EVoidBuildingCategory;

	bool UsesStringCourses(Cat C)
	{
		switch (C)
		{
		case Cat::Residential: case Cat::Commercial: case Cat::Institutional: case Cat::Civic:
		case Cat::Government: case Cat::Medical: case Cat::MixedUse: return true;
		default: return false;
		}
	}

	EVoidWindowStyle DefaultWindowStyle(Cat C)
	{
		switch (C)
		{
		case Cat::Office:      return EVoidWindowStyle::Curtain;
		case Cat::Industrial:  return EVoidWindowStyle::Ribbon;
		default:               return EVoidWindowStyle::Punched;
		}
	}

	float BayFor(Cat C, FVoidBuildingRng& Rng)
	{
		float Base = 380.0f;
		switch (C)
		{
		case Cat::Residential:   Base = 310.0f; break;
		case Cat::Commercial:    Base = 450.0f; break;
		case Cat::Office:        Base = 300.0f; break;
		case Cat::Industrial:    Base = 600.0f; break;
		case Cat::Civic:         Base = 550.0f; break;
		case Cat::Institutional: Base = 400.0f; break;
		case Cat::Medical:       Base = 350.0f; break;
		case Cat::Government:    Base = 500.0f; break;
		case Cat::MixedUse:      Base = 380.0f; break;
		case Cat::Landmark:      Base = 500.0f; break;
		default: break;
		}
		return Base * static_cast<float>(Rng.Range(0.87, 1.15));
	}

	FVoidMassBlock MakeBlock(const TArray<FVector2D>& Outline, int32 F0, int32 F1, const FVoidBuildingMassPlan& Plan, EVoidWindowStyle Style, EVoidRoofKind Roof, bool bCourses)
	{
		FVoidMassBlock B;
		B.Outline = Outline;
		B.FirstFloor = F0;
		B.NumFloors = F1 - F0;
		B.ZBottom = Plan.BaseZ + F0 * Plan.FloorHeight;
		B.ZTop = Plan.BaseZ + F1 * Plan.FloorHeight;
		B.WindowStyle = Style;
		B.Roof = Roof;
		B.bStringCourses = bCourses;
		return B;
	}

	void BuildSimple(FVoidBuildingMassPlan& Plan, const FVoidNormalizedBuilding& B, EVoidWindowStyle Style, EVoidRoofKind Roof, bool bCourses)
	{
		Plan.Archetype = EVoidBuildingArchetype::SimpleExtrude;
		Plan.Blocks.Reset();
		Plan.Blocks.Add(MakeBlock(B.Footprint, 0, B.FloorCount, Plan, Style, Roof, bCourses));
	}

	bool BuildPodiumTower(FVoidBuildingMassPlan& Plan, const FVoidNormalizedBuilding& B, FVoidBuildingRng& Rng, double MinDim, EVoidWindowStyle PodiumStyle, EVoidWindowStyle TowerStyle, EVoidRoofKind Roof, bool bCourses)
	{
		const int32 F = B.FloorCount;
		if (F < 4) { return false; }

		const int32 PodiumFloors = FMath::Clamp(FMath::RoundToInt(static_cast<float>(F * Rng.Range(0.15, 0.30))), 1, F - 2);
		const double Inset = FMath::Clamp(MinDim * Rng.Range(0.07, 0.14), 150.0, 800.0);

		TArray<FVector2D> Tower;
		if (!G::InsetPolygonWithFallback(B.Footprint, Inset, Tower)) { return false; }

		Plan.Archetype = EVoidBuildingArchetype::PodiumTower;
		Plan.Blocks.Reset();
		Plan.Blocks.Add(MakeBlock(B.Footprint, 0, PodiumFloors, Plan, PodiumStyle, EVoidRoofKind::Flat, bCourses));
		Plan.Blocks.Last().bCapToNext = true;
		Plan.Blocks.Add(MakeBlock(Tower, PodiumFloors, F, Plan, TowerStyle, Roof, false));
		return true;
	}

	bool BuildSetbackTiers(FVoidBuildingMassPlan& Plan, const FVoidNormalizedBuilding& B, FVoidBuildingRng& Rng, double MinDim, EVoidWindowStyle Style, EVoidRoofKind Roof, bool bCourses)
	{
		const int32 F = B.FloorCount;
		if (F < 8) { return false; }

		TArray<int32> Splits;
		if (F >= 16)
		{
			Splits.Add(FMath::RoundToInt(static_cast<float>(F * Rng.Range(0.35, 0.45))));
			Splits.Add(FMath::RoundToInt(static_cast<float>(F * Rng.Range(0.65, 0.80))));
		}
		else
		{
			Splits.Add(FMath::RoundToInt(static_cast<float>(F * Rng.Range(0.50, 0.65))));
		}
		for (int32 I = 0; I < Splits.Num(); ++I)
		{
			const int32 Prev = (I == 0) ? 0 : Splits[I - 1];
			Splits[I] = FMath::Clamp(Splits[I], Prev + 1, F - 1);
		}
		if (Splits.Num() == 2 && Splits[1] <= Splits[0]) { Splits.RemoveAt(1); }

		Plan.Archetype = EVoidBuildingArchetype::SetbackTiers;
		Plan.Blocks.Reset();

		TArray<FVector2D> Outline = B.Footprint;
		int32 FloorStart = 0;
		for (int32 I = 0; I <= Splits.Num(); ++I)
		{
			const int32 FloorEnd = (I < Splits.Num()) ? Splits[I] : F;
			const bool bLast = (I == Splits.Num());
			Plan.Blocks.Add(MakeBlock(Outline, FloorStart, FloorEnd, Plan, Style, bLast ? Roof : EVoidRoofKind::Flat, bCourses && I == 0));
			FloorStart = FloorEnd;

			if (!bLast)
			{
				TArray<FVector2D> Next;
				const double Step = FMath::Clamp(MinDim * Rng.Range(0.06, 0.10), 150.0, 600.0);
				if (!G::InsetPolygonWithFallback(Outline, Step, Next))
				{
					// Cannot step in any further: the current tier simply runs to the roof.
					Plan.Blocks.Last().NumFloors = F - Plan.Blocks.Last().FirstFloor;
					Plan.Blocks.Last().ZTop = Plan.BaseZ + F * Plan.FloorHeight;
					Plan.Blocks.Last().Roof = Roof;
					return true;
				}
				Plan.Blocks.Last().bCapToNext = true;
				Outline = Next;
			}
		}
		return true;
	}

	bool BuildCourtyard(FVoidBuildingMassPlan& Plan, const FVoidNormalizedBuilding& B, FVoidBuildingRng& Rng, double MinDim, EVoidWindowStyle Style, EVoidRoofKind Roof, bool bCourses)
	{
		if (MinDim < 3000.0 || B.Area < 1200000.0f) { return false; }

		const double Depth = FMath::Clamp(MinDim * Rng.Range(0.22, 0.30), 500.0, MinDim * 0.38);
		TArray<FVector2D> Inner;
		if (!G::InsetPolygonWithFallback(B.Footprint, Depth, Inner)) { return false; }
		if (G::MinDimension(Inner) < 800.0) { return false; }

		Plan.Archetype = EVoidBuildingArchetype::Courtyard;
		Plan.Blocks.Reset();
		FVoidMassBlock Ring = MakeBlock(B.Footprint, 0, B.FloorCount, Plan, Style, Roof, bCourses);
		Ring.CourtyardOutline = Inner;
		Plan.Blocks.Add(MoveTemp(Ring));
		return true;
	}

	bool BuildPlinthBody(FVoidBuildingMassPlan& Plan, const FVoidNormalizedBuilding& B, FVoidBuildingRng& Rng, double MinDim, EVoidWindowStyle Style, EVoidRoofKind Roof, bool bCourses)
	{
		if (B.FloorCount < 3) { return false; }
		TArray<FVector2D> Body;
		if (!G::InsetPolygonWithFallback(B.Footprint, FMath::Clamp(MinDim * Rng.Range(0.03, 0.05), 80.0, 300.0), Body)) { return false; }

		Plan.Archetype = EVoidBuildingArchetype::PlinthBody;
		Plan.Blocks.Reset();
		Plan.Blocks.Add(MakeBlock(B.Footprint, 0, 1, Plan, EVoidWindowStyle::None, EVoidRoofKind::Flat, false));
		Plan.Blocks.Last().bCapToNext = true;
		Plan.Blocks.Add(MakeBlock(Body, 1, B.FloorCount, Plan, Style, Roof, bCourses));
		return true;
	}

	bool BuildLandmarkTower(FVoidBuildingMassPlan& Plan, const FVoidNormalizedBuilding& B, FVoidBuildingRng& Rng, double MinDim, EVoidRoofKind Roof)
	{
		const int32 F = B.FloorCount;
		if (F < 6) { return false; }

		const int32 PodiumFloors = FMath::Clamp(FMath::RoundToInt(F * 0.20f), 1, F - 3);
		TArray<FVector2D> Tower;
		if (!G::InsetPolygonWithFallback(B.Footprint, FMath::Clamp(MinDim * Rng.Range(0.10, 0.16), 200.0, 1000.0), Tower)) { return false; }

		const int32 TowerTop = FMath::Max(PodiumFloors + 1, FMath::RoundToInt(F * 0.80f));

		Plan.Archetype = EVoidBuildingArchetype::LandmarkTower;
		Plan.Blocks.Reset();
		Plan.Blocks.Add(MakeBlock(B.Footprint, 0, PodiumFloors, Plan, EVoidWindowStyle::Punched, EVoidRoofKind::Flat, true));
		Plan.Blocks.Last().bCapToNext = true;

		const bool bHasCrown = TowerTop <= F - 2;
		Plan.Blocks.Add(MakeBlock(Tower, PodiumFloors, bHasCrown ? TowerTop : F, Plan, EVoidWindowStyle::Curtain, bHasCrown ? EVoidRoofKind::Flat : Roof, false));
		if (!bHasCrown) { return true; }

		TArray<FVector2D> Crown;
		if (!G::InsetPolygonWithFallback(Tower, FMath::Clamp(MinDim * 0.06, 120.0, 500.0), Crown))
		{
			Plan.Blocks.Last().NumFloors = F - PodiumFloors;
			Plan.Blocks.Last().ZTop = Plan.BaseZ + F * Plan.FloorHeight;
			Plan.Blocks.Last().Roof = Roof;
			return true;
		}
		Plan.Blocks.Last().bCapToNext = true;

		const int32 CrownTop = FMath::Max(TowerTop + 1, FMath::RoundToInt(F * 0.92f));
		const bool bHasSpire = CrownTop <= F - 1;
		Plan.Blocks.Add(MakeBlock(Crown, TowerTop, bHasSpire ? CrownTop : F, Plan, EVoidWindowStyle::None, bHasSpire ? EVoidRoofKind::Flat : Roof, false));
		if (!bHasSpire) { return true; }

		TArray<FVector2D> Spire;
		if (G::InsetPolygonWithFallback(Crown, FMath::Clamp(MinDim * 0.10, 100.0, 600.0), Spire))
		{
			Plan.Blocks.Last().bCapToNext = true;
			Plan.Blocks.Add(MakeBlock(Spire, CrownTop, F, Plan, EVoidWindowStyle::None, EVoidRoofKind::Flat, false));
		}
		else
		{
			Plan.Blocks.Last().NumFloors = F - TowerTop;
			Plan.Blocks.Last().ZTop = Plan.BaseZ + F * Plan.FloorHeight;
			Plan.Blocks.Last().Roof = Roof;
		}
		return true;
	}

	bool BoxFitsInside(const FVoidRooftopBox& Box, const TArray<FVector2D>& Region)
	{
		const double C = FMath::Cos(Box.YawRadians), S = FMath::Sin(Box.YawRadians);
		const double SX[4] = { -1, 1, 1, -1 }, SY[4] = { -1, -1, 1, 1 };
		for (int32 I = 0; I < 4; ++I)
		{
			const double LX = SX[I] * Box.HalfExtent.X, LY = SY[I] * Box.HalfExtent.Y;
			const FVector2D P(Box.Center.X + LX * C - LY * S, Box.Center.Y + LX * S + LY * C);
			if (!G::PointInPolygon(P, Region)) { return false; }
		}
		return true;
	}

	void PlanRooftop(FVoidBuildingMassPlan& Plan, const FVoidNormalizedBuilding& B, FVoidBuildingRng& Rng)
	{
		if (Plan.Blocks.Num() == 0) { return; }
		const FVoidMassBlock& Top = Plan.Blocks.Last();
		if (Top.Roof == EVoidRoofKind::Pyramid || Top.NumFloors <= 0 || Top.CourtyardOutline.Num() > 0) { return; }

		TArray<FVector2D> Region;
		if (!G::InsetPolygon(Top.Outline, 250.0, Region)) { return; }

		int32 Count = 0;
		bool bStack = false;
		switch (B.Category)
		{
		case Cat::Landmark:    Count = Rng.RangeInt(2, 3); break;
		case Cat::Industrial:  Count = Rng.RangeInt(1, 3); bStack = Rng.Chance(0.6); break;
		case Cat::Residential: Count = B.FloorCount >= 4 ? Rng.RangeInt(0, 2) : Rng.RangeInt(0, 1); break;
		case Cat::Unknown:     Count = Rng.RangeInt(0, 1); break;
		default:               Count = FMath::Clamp(FMath::FloorToInt(Top.Outline.Num() > 0 ? static_cast<float>(G::SignedArea(Top.Outline)) / 700000.0f : 0.0f) + Rng.RangeInt(0, 1), 1, 5); break;
		}
		if (Count <= 0 && !bStack) { return; }

		Plan.RooftopBoxes.Reset();
		const double Yaw = Plan.YawRadians;
		FVector2D Min, Max;
		G::GetBounds(Region, Min, Max);

		auto TryPlace = [&](const FVector2D& Half, double Height) -> bool
		{
			for (int32 Attempt = 0; Attempt < 14; ++Attempt)
			{
				FVoidRooftopBox Box;
				Box.Center = FVector2D(Rng.Range(Min.X, Max.X), Rng.Range(Min.Y, Max.Y));
				Box.HalfExtent = Half;
				Box.YawRadians = Yaw;
				Box.ZBottom = Top.ZTop;
				Box.Height = Height;
				if (!BoxFitsInside(Box, Region)) { continue; }

				const double R = G::Length(Half);
				bool bOverlap = false;
				for (const FVoidRooftopBox& Other : Plan.RooftopBoxes)
				{
					const double OR = G::Length(Other.HalfExtent);
					if (G::Length(Other.Center - Box.Center) < R + OR + 60.0) { bOverlap = true; break; }
				}
				if (bOverlap) { continue; }

				Plan.RooftopBoxes.Add(Box);
				return true;
			}
			return false;
		};

		for (int32 I = 0; I < Count; ++I)
		{
			if (I == 0 && B.FloorCount >= 4 && B.Category != Cat::Industrial)
			{
				// Stair / lift overrun.
				TryPlace(FVector2D(Rng.Range(150.0, 220.0), Rng.Range(150.0, 250.0)), Rng.Range(260.0, 320.0));
			}
			else
			{
				TryPlace(FVector2D(Rng.Range(80.0, 150.0), Rng.Range(60.0, 120.0)), Rng.Range(100.0, 180.0));
			}
		}
		if (bStack)
		{
			TryPlace(FVector2D(60.0, 60.0), Rng.Range(500.0, 1200.0));
		}
		if (B.Category == Cat::Landmark)
		{
			// Antenna: kept modest so a landmark's authored height stays the dominant silhouette.
			TryPlace(FVector2D(25.0, 25.0), FMath::Max(300.0, B.HeightUnits * 0.05));
		}
	}
}

const TCHAR* FVoidBuildingMassPlanner::ArchetypeToString(EVoidBuildingArchetype A)
{
	switch (A)
	{
	case EVoidBuildingArchetype::PodiumTower:   return TEXT("PodiumTower");
	case EVoidBuildingArchetype::SetbackTiers:  return TEXT("SetbackTiers");
	case EVoidBuildingArchetype::Courtyard:     return TEXT("Courtyard");
	case EVoidBuildingArchetype::PlinthBody:    return TEXT("PlinthBody");
	case EVoidBuildingArchetype::LandmarkTower: return TEXT("LandmarkTower");
	default:                                    return TEXT("SimpleExtrude");
	}
}

FVoidBuildingMassPlan FVoidBuildingMassPlanner::Plan(const FVoidNormalizedBuilding& B, const FVoidBuildingGenerationParams& Params)
{
	FVoidBuildingMassPlan Plan;
	Plan.BaseZ = B.BaseZ;
	Plan.FloorHeight = B.FloorHeight;
	Plan.FoundationDepth = Params.FoundationDepthUnits;
	Plan.YawRadians = G::LongestEdgeYaw(B.Footprint);

	FVoidBuildingRng Rng(static_cast<uint32>(B.Seed) ^ 0xC0FFEE11u);
	const int32 F = B.FloorCount;
	const double MinDim = G::MinDimension(B.Footprint);
	const Cat C = B.Category;

	// ---- window / facade parameters ---------------------------------------------------------
	FVoidWindowParams& W = Plan.Windows;
	W.BayUnits = BayFor(C, Rng);
	W.Depth = static_cast<float>(Rng.Range(2.0, 6.0));
	W.StringCourseEvery = (Params.bGenerateFloorBands && UsesStringCourses(C)) ? Rng.RangeInt(3, 5) : 0;
	if (C == Cat::Industrial) { W.SillFrac = 0.62f; W.HeightFrac = 0.22f; }
	if (C == Cat::Office)     { W.SillFrac = 0.12f; W.HeightFrac = 0.70f; }
	if (C == Cat::Commercial || C == Cat::MixedUse || C == Cat::Office) { W.GroundSillFrac = 0.06f; W.GroundHeightFrac = 0.72f; }

	const EVoidWindowStyle Style = Params.bGenerateWindows ? DefaultWindowStyle(C) : EVoidWindowStyle::None;
	const EVoidWindowStyle PodiumStyle = Params.bGenerateWindows ? EVoidWindowStyle::Punched : EVoidWindowStyle::None;
	const bool bCourses = W.StringCourseEvery > 0;

	// ---- roof form --------------------------------------------------------------------------
	EVoidRoofKind Roof = (C == Cat::Industrial && !Rng.Chance(0.2)) ? EVoidRoofKind::Flat : EVoidRoofKind::Parapet;

	// ---- archetype --------------------------------------------------------------------------
	bool bBuilt = false;
	switch (C)
	{
	case Cat::Landmark:
		bBuilt = BuildLandmarkTower(Plan, B, Rng, MinDim, Roof)
			|| BuildPodiumTower(Plan, B, Rng, MinDim, PodiumStyle, Style, Roof, bCourses);
		break;

	case Cat::Office:
		if (F >= 8) { bBuilt = Rng.Chance(0.5) ? BuildPodiumTower(Plan, B, Rng, MinDim, PodiumStyle, Style, Roof, false) : BuildSetbackTiers(Plan, B, Rng, MinDim, Style, Roof, false); }
		break;

	case Cat::Commercial:
		if (F >= 5 && Rng.Chance(0.6)) { bBuilt = BuildPodiumTower(Plan, B, Rng, MinDim, PodiumStyle, Style, Roof, bCourses); }
		break;

	case Cat::MixedUse:
		if (F >= 6) { bBuilt = BuildPodiumTower(Plan, B, Rng, MinDim, PodiumStyle, Style, Roof, bCourses); }
		break;

	case Cat::Residential:
		if (F <= 10 && Rng.Chance(0.5)) { bBuilt = BuildCourtyard(Plan, B, Rng, MinDim, Style, Roof, bCourses); }
		else if (F >= 10 && Rng.Chance(0.5)) { bBuilt = BuildSetbackTiers(Plan, B, Rng, MinDim, Style, Roof, bCourses); }
		break;

	case Cat::Civic:
	case Cat::Government:
	case Cat::Institutional:
		if (Rng.Chance(0.4)) { bBuilt = BuildCourtyard(Plan, B, Rng, MinDim, Style, Roof, bCourses); }
		if (!bBuilt) { bBuilt = BuildPlinthBody(Plan, B, Rng, MinDim, Style, Roof, bCourses); }
		break;

	case Cat::Medical:
		if (F >= 6) { bBuilt = BuildPodiumTower(Plan, B, Rng, MinDim, PodiumStyle, Style, Roof, bCourses); }
		else if (Rng.Chance(0.5)) { bBuilt = BuildCourtyard(Plan, B, Rng, MinDim, Style, Roof, bCourses); }
		break;

	default:
		break; // Industrial, Unknown: plain extrusion.
	}

	if (!bBuilt)
	{
		// Low, convex, simple residential footprints get a pyramid roof for skyline variety.
		if (C == Cat::Residential && F <= 3 && B.Footprint.Num() <= 6 && FVoidBuildingGeometry::IsConvex(B.Footprint) && Rng.Chance(0.5))
		{
			Roof = EVoidRoofKind::Pyramid;
		}
		BuildSimple(Plan, B, Style, Roof, bCourses);
	}

	Plan.RoofZ = Plan.Blocks.Last().ZTop;

	if (Params.bGenerateRooftopStructures)
	{
		PlanRooftop(Plan, B, Rng);
	}
	return Plan;
}
