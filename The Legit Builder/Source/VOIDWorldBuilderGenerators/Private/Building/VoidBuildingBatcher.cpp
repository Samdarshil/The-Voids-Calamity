// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

// World-free half of the Building Generator: normalise -> road relationship ->
// massing -> mesh -> batches. Deliberately has no UWorld / actor / component
// dependency so it is deterministic, unit-testable, and reusable by tools that
// want the data without spawning anything.

#include "Building/VoidBuildingGenerator.h"
#include "Building/VoidBuildingGeometry.h"
#include "Building/VoidBuildingNormalizer.h"
#include "Building/VoidBuildingRoadContext.h"
#include "Building/VoidBuildingMassing.h"

namespace
{
	constexpr int32 GeneratorVersion = 1; // bump when geometry output changes for identical input

	void HashCombine(uint32& H, uint32 V) { H ^= V; H *= 16777619u; }
	void HashCombine(uint32& H, double V) { HashCombine(H, static_cast<uint32>(FMath::RoundToInt(V * 10.0))); }
	void HashCombine(uint32& H, const FString& S) { HashCombine(H, FVoidBuildingGeometry::StableHash(S)); }

	FVoidBuildingMetadata MakeMetadata(const FVoidNormalizedBuilding& B, const FVoidBuildingMassPlan& Plan)
	{
		FVoidBuildingMetadata M;
		M.BuildingId = B.Id;
		M.DistrictId = B.DistrictId;
		M.TypeTag = B.TypeTag;
		M.Category = B.Category;
		M.Archetype = Plan.Archetype;
		M.AssetCategory = FVoidBuildingNormalizer::MakeAssetCategory(B);
		M.bIsLandmark = B.bIsLandmark;
		M.FootprintCorners = B.Footprint;
		if (B.bAdjustedForRoad) { M.SourceFootprintCorners = B.SourceFootprint; }
		M.Centroid = B.Centroid;
		M.BoundsMin = B.BoundsMin;
		M.BoundsMax = B.BoundsMax;
		M.YawDegrees = static_cast<float>(FMath::RadiansToDegrees(Plan.YawRadians));
		M.HeightUnits = B.HeightUnits;
		M.FloorCount = B.FloorCount;
		M.BaseZ = B.BaseZ;
		M.bHasRoadFrontage = B.Frontage.bHasRoadFrontage;
		M.FrontageRoadId = B.Frontage.RoadId;
		M.NumEntrances = B.Entrances.Num();
		if (B.Entrances.Num() > 0) { M.MainEntranceLocation = B.Entrances[0].Location; }
		M.AccessPoint = B.Frontage.AccessPoint;
		M.bAdjustedForRoad = B.bAdjustedForRoad;
		M.AdjustmentOffset = B.AdjustmentOffset;
		M.Seed = B.Seed;
		return M;
	}
}

void FVoidBuildingGenerator::BuildBatches(
	const FVoidDistrictData& District,
	const FVoidBuildingGenerationParams& Params,
	FVoidValidationReport& Report,
	TArray<FVoidBuildingBatchData>& OutBatches,
	FVoidBuildingRunStats& Stats,
	TFunction<bool()> IsCancelled)
{
	OutBatches.Reset();
	Stats = FVoidBuildingRunStats();
	Stats.NumInput = District.Buildings.Num();

	// 1. Normalise (from the already-imported spec; no JSON here).
	TArray<FVoidNormalizedBuilding> Buildings;
	FVoidBuildingNormalizer::Normalize(District, Params, Report, Buildings);
	Stats.NumRejected = Stats.NumInput - Buildings.Num();

	// 2. Road relationship.
	FVoidBuildingRoadContext RoadContext;
	RoadContext.Build(District, Params);
	Stats.NumRoadSegments = RoadContext.NumSegments();
	if (RoadContext.NumSkippedGradeSeparated() > 0)
	{
		Report.AddInfo(FString::Printf(TEXT("%d bridge/tunnel road(s) ignored for building setbacks (not at grade)."), RoadContext.NumSkippedGradeSeparated()),
			TEXT("district.roads"), TEXT("VOID.Building.GradeSeparatedRoadsIgnored"));
	}
	if (RoadContext.NumSegments() == 0 && Buildings.Num() > 0)
	{
		Report.AddWarning(TEXT("District has no at-grade roads; buildings have no frontage, setbacks or access points."), TEXT("district.roads"), TEXT("VOID.Building.NoRoads"));
	}

	// 3. Per building: resolve -> plan -> mesh, grouped by grid cell (ordered map => deterministic batch order).
	const float Cell = FMath::Max(Params.BatchCellSizeUnits, 1000.0f);
	TMap<FIntPoint, int32> CellToBatch;

	// First pass: resolve + assign cells so batch order does not depend on building order.
	for (FVoidNormalizedBuilding& B : Buildings)
	{
		if (IsCancelled && IsCancelled()) { break; }
		RoadContext.ResolveBuilding(B, Params);
	}

	// Sliding a building off a road can push it into a neighbour; the source data does not
	// guarantee gaps. Report it (footprints are never silently edited beyond the slide).
	for (int32 I = 0; I < Buildings.Num(); ++I)
	{
		const FVoidNormalizedBuilding& A = Buildings[I];
		if (!A.bAdjustedForRoad) { continue; }
		for (int32 J = 0; J < Buildings.Num(); ++J)
		{
			const FVoidNormalizedBuilding& O = Buildings[J];
			if (I == J || O.bRoadConflictUnresolved) { continue; }
			if (A.BoundsMax.X < O.BoundsMin.X || O.BoundsMax.X < A.BoundsMin.X || A.BoundsMax.Y < O.BoundsMin.Y || O.BoundsMax.Y < A.BoundsMin.Y) { continue; }
			if (FVoidBuildingGeometry::PolygonsOverlap(A.Footprint, O.Footprint))
			{
				Report.AddWarning(FString::Printf(TEXT("Building '%s' was slid off a road and now overlaps '%s'."), *A.Id.ToString(), *O.Id.ToString()),
					FString::Printf(TEXT("building:%s"), *A.Id.ToString()), TEXT("VOID.Building.OverlapAfterAdjustment"),
					TEXT("Leave more room between the building and the road in the source data, or between neighbouring footprints."));
				break;
			}
		}
	}

	TArray<int32> Order;
	for (int32 I = 0; I < Buildings.Num(); ++I) { Order.Add(I); }
	auto CellOf = [Cell](const FVoidNormalizedBuilding& B)
	{
		return FIntPoint(FMath::FloorToInt(static_cast<float>(B.Centroid.X) / Cell), FMath::FloorToInt(static_cast<float>(B.Centroid.Y) / Cell));
	};
	Order.Sort([&](int32 L, int32 R)
	{
		const FIntPoint CL = CellOf(Buildings[L]), CR = CellOf(Buildings[R]);
		if (CL.X != CR.X) { return CL.X < CR.X; }
		if (CL.Y != CR.Y) { return CL.Y < CR.Y; }
		return L < R; // Buildings are already sorted by id
	});

	for (const int32 Index : Order)
	{
		if (IsCancelled && IsCancelled()) { Report.AddWarning(TEXT("Building generation cancelled; output is partial."), FString(), TEXT("VOID.Building.Cancelled")); break; }

		const FVoidNormalizedBuilding& B = Buildings[Index];

		if (B.bRoadConflictUnresolved && Params.ConflictPolicy != EVoidBuildingConflictPolicy::GenerateAnyway)
		{
			++Stats.NumSkippedRoad;
			Report.AddWarning(FString::Printf(TEXT("Building '%s' overlaps a road corridor and could not be moved clear; skipped."), *B.Id.ToString()),
				FString::Printf(TEXT("building:%s"), *B.Id.ToString()), TEXT("VOID.Building.RoadConflict"),
				TEXT("Move the footprint away from the road, reduce the setback, or use ConflictPolicy=GenerateAnyway to inspect it."));
			continue;
		}
		if (!B.Frontage.bHasRoadFrontage && RoadContext.NumSegments() > 0)
		{
			++Stats.NumNoFrontage;
		}

		const FIntPoint CellKey = CellOf(B);
		int32* BatchIndex = CellToBatch.Find(CellKey);
		if (!BatchIndex)
		{
			FVoidBuildingBatchData NewBatch;
			NewBatch.Cell = CellKey;
			NewBatch.Origin = FVector((CellKey.X + 0.5) * Cell, (CellKey.Y + 0.5) * Cell, 0.0);
			NewBatch.Mesh.Origin = NewBatch.Origin;
			const int32 NewIndex = OutBatches.Add(MoveTemp(NewBatch));
			BatchIndex = &CellToBatch.Add(CellKey, NewIndex);
		}
		FVoidBuildingBatchData& Batch = OutBatches[*BatchIndex];

		const FVoidBuildingMassPlan Plan = FVoidBuildingMassPlanner::Plan(B, Params);
		FVoidBuildingMetadata Meta = MakeMetadata(B, Plan);

		if (const FString* OverridePath = Params.AssetOverrideClassPaths.Find(B.Id))
		{
			// Asset-replacement hook: no greybox for this building; the caller spawns the class at this transform.
			(void)OverridePath;
			Meta.bGreyboxSuppressed = true;
			const FTransform T(FRotator(0.0, FMath::RadiansToDegrees(Plan.YawRadians), 0.0), FVector(B.Centroid.X, B.Centroid.Y, B.BaseZ));
			Batch.Overrides.Add(TPair<FName, FTransform>(B.Id, T));
			++Stats.NumOverrides;
		}
		else
		{
			FVoidBuildingMeshBuilder::Emit(B, Plan, Params, Batch.Mesh);
		}

		uint32 Sig = 2166136261u;
		HashCombine(Sig, static_cast<uint32>(GeneratorVersion));
		HashCombine(Sig, static_cast<uint32>(Params.WindowDetail));
		HashCombine(Sig, static_cast<uint32>(Params.bGenerateWindows) | (static_cast<uint32>(Params.bGenerateEntrances) << 1) | (static_cast<uint32>(Params.bGenerateRooftopStructures) << 2) | (static_cast<uint32>(Params.bGenerateFloorBands) << 3));
		HashCombine(Sig, static_cast<double>(Params.MinSetbackUnits));
		HashCombine(Sig, Batch.Signature.IsEmpty() ? FString(TEXT("")) : Batch.Signature);
		HashCombine(Sig, B.Id.ToString());
		HashCombine(Sig, static_cast<uint32>(B.Seed));
		HashCombine(Sig, static_cast<uint32>(B.Category));
		HashCombine(Sig, static_cast<double>(B.HeightUnits));
		for (const FVector2D& P : B.Footprint) { HashCombine(Sig, P.X); HashCombine(Sig, P.Y); }
		Batch.Signature = FString::Printf(TEXT("%08x"), Sig);

		Batch.Metadata.Add(MoveTemp(Meta));

		++Stats.NumGenerated;
		if (B.bAdjustedForRoad) { ++Stats.NumAdjusted; }
		if (B.bIsLandmark) { ++Stats.NumLandmarks; }
		++Stats.CategoryCounts[static_cast<int32>(B.Category)];
	}

	Stats.NumBatches = OutBatches.Num();
	for (const FVoidBuildingBatchData& Batch : OutBatches) { Stats.NumTriangles += Batch.Mesh.NumTriangles(); }

	if (Stats.NumAdjusted > 0)
	{
		Report.AddInfo(FString::Printf(TEXT("%d building(s) were slid away from road corridors to honour setbacks (authored footprints kept in metadata)."), Stats.NumAdjusted),
			TEXT("district.buildings"), TEXT("VOID.Building.AdjustedForRoad"));
	}
	if (Stats.NumNoFrontage > 0)
	{
		Report.AddInfo(FString::Printf(TEXT("%d building(s) have no road within frontage range; entrance placed on the longest wall."), Stats.NumNoFrontage),
			TEXT("district.buildings"), TEXT("VOID.Building.NoFrontage"));
	}
}

