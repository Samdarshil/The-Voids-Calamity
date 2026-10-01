// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Building/VoidBuildingGenerationSettings.h"
#include "Road/VoidRoadGenerationSettings.h"
#include "Engine/DataTable.h"

UVoidBuildingGenerationSettings::UVoidBuildingGenerationSettings()
{
	CategoryName = TEXT("Plugins");
}

FVoidBuildingGenerationParams UVoidBuildingGenerationSettings::ToParams() const
{
	FVoidBuildingGenerationParams P;
	P.GlobalSeed = GlobalSeed;
	P.BatchCellSizeUnits = BatchCellSizeUnits;
	P.MinFootprintAreaUnits2 = MinFootprintAreaUnits2;
	P.MinSetbackUnits = MinSetbackUnits;
	P.LandmarkSetbackMultiplier = LandmarkSetbackMultiplier;
	P.ConflictPolicy = ConflictPolicy;
	P.MaxAdjustmentUnits = MaxAdjustmentUnits;
	P.MaxFrontageSearchUnits = MaxFrontageSearchUnits;
	P.bAlignBaseToFrontageRoad = bAlignBaseToFrontageRoad;
	P.bGenerateEntrances = bGenerateEntrances;
	P.bGenerateWindows = bGenerateWindows;
	P.bGenerateRooftopStructures = bGenerateRooftopStructures;
	P.bGenerateFloorBands = bGenerateFloorBands;
	P.WindowDetail = WindowDetail;
	P.MaxWindowQuadsPerBuilding = MaxWindowQuadsPerBuilding;
	P.FoundationDepthUnits = FoundationDepthUnits;
	P.bGenerateCollision = bGenerateCollision;
	P.KeywordOverrides = KeywordOverrides;

	for (const TPair<FName, TSoftClassPtr<AActor>>& Pair : AssetOverrides)
	{
		if (!Pair.Value.IsNull())
		{
			P.AssetOverrideClassPaths.Add(Pair.Key, Pair.Value.ToString());
		}
	}

	const UVoidRoadGenerationSettings* RoadSettings = GetDefault<UVoidRoadGenerationSettings>();
	if (RoadSettings && !RoadSettings->RoadTypeProfileTable.IsNull())
	{
		P.RoadProfileTable = RoadSettings->RoadTypeProfileTable.LoadSynchronous();
	}
	return P;
}
