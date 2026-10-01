// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Building/VoidBuildingTestFixture.h"
#include "Building/VoidBuildingGeometry.h"

namespace
{
	struct FTag { const TCHAR* Text; float MinH; float MaxH; };

	// Each entry exists to hit one classifier keyword; heights are in cm.
	const FTag GTags[10] =
	{
		{ TEXT("Residential_Midrise"),   900.0f,  3600.0f },
		{ TEXT("Commercial_Retail"),    1200.0f,  4800.0f },
		{ TEXT("Office_Tower"),         3000.0f, 10000.0f },
		{ TEXT("Industrial_Warehouse"),  800.0f,  1800.0f },
		{ TEXT("Civic_Hall"),           1500.0f,  3000.0f },
		{ TEXT("Institutional_School"), 1200.0f,  2400.0f },
		{ TEXT("Medical_Clinic"),       1600.0f,  3600.0f },
		{ TEXT("Government_Ministry"),  1800.0f,  3600.0f },
		{ TEXT("MixedUse_Block"),       2400.0f,  6000.0f },
		{ TEXT("Landmark_Tower"),       8000.0f, 14000.0f },
	};

	FVoidRoadSpec MakeRoad(const TCHAR* Id, EVoidRoadType Type, std::initializer_list<FVector2D> Pts, bool bSidewalk, bool bBridge = false)
	{
		FVoidRoadSpec R;
		R.Id = FVoidElementId(FName(Id));
		R.RoadType = Type;
		R.CenterlinePoints = TArray<FVector2D>(Pts);
		R.bHasSidewalk = bSidewalk;
		R.bIsBridge = bBridge;
		return R;
	}

	void AddBuilding(FVoidDistrictData& D, const FString& Id, const TCHAR* Tag, float Height, std::initializer_list<FVector2D> Corners)
	{
		FVoidBuildingSpec B;
		B.Id = FVoidElementId(FName(*Id));
		B.BuildingType = Tag;
		B.HeightUnits = Height;
		B.FootprintCorners = TArray<FVector2D>(Corners);
		D.Buildings.Add(B);
	}

	void AddRect(FVoidDistrictData& D, const FString& Id, const TCHAR* Tag, float Height, double X0, double Y0, double X1, double Y1)
	{
		AddBuilding(D, Id, Tag, Height, { FVector2D(X0, Y0), FVector2D(X1, Y0), FVector2D(X1, Y1), FVector2D(X0, Y1) });
	}
}

FVoidDistrictData FVoidBuildingTestFixture::MakeSmallDistrict()
{
	FVoidDistrictData D;
	D.DistrictId = FVoidElementId(DistrictName());

	D.Roads.Add(MakeRoad(TEXT("fx_road_main"),  EVoidRoadType::Primary, { FVector2D(-6000, 0), FVector2D(18000, 0) }, true));
	D.Roads.Add(MakeRoad(TEXT("fx_road_west"),  EVoidRoadType::Local,   { FVector2D(0, -9000), FVector2D(0, 9000) }, true));
	D.Roads.Add(MakeRoad(TEXT("fx_road_east"),  EVoidRoadType::Local,   { FVector2D(12000, -9000), FVector2D(12000, 9000) }, true));
	D.Roads.Add(MakeRoad(TEXT("fx_road_alley"), EVoidRoadType::Alley,   { FVector2D(3000, 1000), FVector2D(3000, 7000) }, false));
	FVoidRoadSpec Ring = MakeRoad(TEXT("fx_road_ring"), EVoidRoadType::Roundabout, { FVector2D(6000, 8600) }, true);
	Ring.RoundaboutRadiusUnits = 1000.0f;
	D.Roads.Add(Ring);
	D.Roads.Add(MakeRoad(TEXT("fx_road_bridge"), EVoidRoadType::Secondary, { FVector2D(-6000, -6500), FVector2D(18000, -6500) }, true, true));

	// Regular blocks: 6 columns x 2 rows on each side of the main road.
	int32 Index = 0;
	for (int32 Side = 0; Side < 2; ++Side)
	{
		for (int32 Row = 0; Row < 2; ++Row)
		{
			for (int32 Col = 0; Col < 6; ++Col)
			{
				FVoidBuildingRng Rng(static_cast<uint32>(Index) * 7919u + 17u);
				const FTag& Tag = GTags[Index % 10];
				const float Height = static_cast<float>(Rng.Range(Tag.MinH, Tag.MaxH));
				const double X0 = 1300.0 + Col * 1750.0;
				const double Width = 1300.0 + Rng.Range(0.0, 300.0);
				const double Depth = 1800.0 + Rng.Range(0.0, 500.0);
				const double Y0 = 1400.0 + Row * 2600.0;
				const double Ya = Side == 0 ? Y0 : -(Y0 + Depth);
				AddRect(D, FString::Printf(TEXT("fx_b%03d"), Index), Tag.Text, Height, X0, Ya, X0 + Width, Ya + Depth);
				++Index;
			}
		}
	}

	AddBuilding(D, TEXT("fx_lshape"), TEXT("Office_Tower"), 6000.0f,
		{ FVector2D(13200, 1400), FVector2D(16200, 1400), FVector2D(16200, 3000), FVector2D(14700, 3000), FVector2D(14700, 5600), FVector2D(13200, 5600) });
	AddRect(D, TEXT("fx_courtyard_block"), TEXT("Residential_Courtyard"), 2400.0f, 13200, -7400, 17800, -3000);
	AddRect(D, TEXT("fx_civic_plaza"), TEXT("Civic_Hall"), 2400.0f, 13200, 6600, 16800, 9000);
	AddRect(D, TEXT("fx_landmark_tower"), TEXT("Landmark_Tower"), 22000.0f, 1400, -9600, 4400, -7000);
	AddRect(D, TEXT("fx_landmark_low"), TEXT("Landmark_Hall"), 2400.0f, 5200, -9600, 9200, -7400);

	// Edge cases (each should be handled, not crash).
	AddBuilding(D, TEXT("fx_bowtie"), TEXT("Office_Tower"), 3000.0f, { FVector2D(17000, 6000), FVector2D(18000, 7000), FVector2D(18000, 6000), FVector2D(17000, 7000) });
	AddBuilding(D, TEXT("fx_two_corners"), TEXT("Residential"), 3000.0f, { FVector2D(0, 12000), FVector2D(500, 12000) });
	AddRect(D, TEXT("fx_tiny"), TEXT("Residential"), 3000.0f, 3000, 12000, 3100, 12100);
	AddRect(D, TEXT("fx_mystery"), TEXT("Mystery_Box"), 1500.0f, 16500, 4200, 17500, 5200);
	AddRect(D, TEXT("fx_notag"), TEXT(""), 1500.0f, 8000, 12000, 9000, 13000);
	AddRect(D, TEXT("fx_on_road"), TEXT("Commercial_Retail"), 1800.0f, 7000, -500, 8500, 900);       // straddles the main road: expected to be skipped
	AddRect(D, TEXT("fx_near_road"), TEXT("Commercial_Retail"), 1800.0f, 13200, -1000, 14400, -100); // just too close: expected to be slid clear
	return D;
}

FVoidDistrictData FVoidBuildingTestFixture::MakeLargeDistrict(int32 Columns, int32 Rows)
{
	FVoidDistrictData D;
	D.DistrictId = FVoidElementId(DistrictName());

	const double Pitch = 3000.0;
	const double Extent = Pitch * Columns;
	const double ExtentY = Pitch * Rows;
	for (int32 Col = 0; Col < Columns; Col += 6)
	{
		const double X = (Col + 0.5) * Pitch;
		D.Roads.Add(MakeRoad(*FString::Printf(TEXT("fx_rv_%d"), Col), EVoidRoadType::Local, { FVector2D(X, 0), FVector2D(X, ExtentY) }, true));
	}
	for (int32 Row = 0; Row < Rows; Row += 6)
	{
		const double Y = (Row + 0.5) * Pitch;
		D.Roads.Add(MakeRoad(*FString::Printf(TEXT("fx_rh_%d"), Row), EVoidRoadType::Local, { FVector2D(0, Y), FVector2D(Extent, Y) }, true));
	}

	int32 Index = 0;
	for (int32 Row = 0; Row < Rows; ++Row)
	{
		for (int32 Col = 0; Col < Columns; ++Col)
		{
			if (Col % 6 == 0 || Row % 6 == 0) { continue; } // road corridors
			FVoidBuildingRng Rng(static_cast<uint32>(Index) * 2654435761u + 5u);
			const FTag& Tag = GTags[Rng.RangeInt(0, 8)]; // no landmarks in bulk
			const double X0 = Col * Pitch + 400.0 + Rng.Range(0.0, 150.0);
			const double Y0 = Row * Pitch + 400.0 + Rng.Range(0.0, 150.0);
			AddRect(D, FString::Printf(TEXT("fx_L%06d"), Index), Tag.Text, static_cast<float>(Rng.Range(Tag.MinH, Tag.MaxH)),
				X0, Y0, X0 + 2000.0 + Rng.Range(0.0, 300.0), Y0 + 2000.0 + Rng.Range(0.0, 300.0));
			++Index;
		}
	}
	return D;
}
