// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Road/VoidRoadTypeProfile.h"

const FVoidRoadTypeProfile& FVoidRoadTypeProfileLibrary::GetBuiltInDefault(EVoidRoadType RoadType)
{
	static const FVoidRoadTypeProfile Highway   = [] { FVoidRoadTypeProfile P; P.DefaultWidthUnits = 1400.0f; P.DefaultLaneCount = 6; P.DefaultSpeedLimitUnits = 100; P.bDefaultHasSidewalk = false; P.bDefaultHasMedian = true;  P.MedianWidthUnits = 200.0f; return P; }();
	static const FVoidRoadTypeProfile Primary   = [] { FVoidRoadTypeProfile P; P.DefaultWidthUnits = 1000.0f; P.DefaultLaneCount = 4; P.DefaultSpeedLimitUnits = 60;  P.bDefaultHasSidewalk = true;  P.bDefaultHasMedian = true;  P.MedianWidthUnits = 120.0f; return P; }();
	static const FVoidRoadTypeProfile Secondary = [] { FVoidRoadTypeProfile P; P.DefaultWidthUnits = 800.0f;  P.DefaultLaneCount = 2; P.DefaultSpeedLimitUnits = 50;  P.bDefaultHasSidewalk = true;  P.bDefaultHasMedian = false; return P; }();
	static const FVoidRoadTypeProfile Local     = [] { FVoidRoadTypeProfile P; P.DefaultWidthUnits = 600.0f;  P.DefaultLaneCount = 2; P.DefaultSpeedLimitUnits = 30;  P.bDefaultHasSidewalk = true;  P.bDefaultHasMedian = false; return P; }();
	static const FVoidRoadTypeProfile Service   = [] { FVoidRoadTypeProfile P; P.DefaultWidthUnits = 400.0f;  P.DefaultLaneCount = 1; P.DefaultSpeedLimitUnits = 15;  P.bDefaultHasSidewalk = false; P.bDefaultHasMedian = false; return P; }();
	static const FVoidRoadTypeProfile Alley     = [] { FVoidRoadTypeProfile P; P.DefaultWidthUnits = 250.0f;  P.DefaultLaneCount = 1; P.DefaultSpeedLimitUnits = 10;  P.bDefaultHasSidewalk = false; P.bDefaultHasMedian = false; P.CurbWidthUnits = 0.0f; return P; }();
	static const FVoidRoadTypeProfile Roundabout = [] { FVoidRoadTypeProfile P; P.DefaultWidthUnits = 600.0f; P.DefaultLaneCount = 2; P.DefaultSpeedLimitUnits = 20;  P.bDefaultHasSidewalk = true;  P.bDefaultHasMedian = false; return P; }();

	switch (RoadType)
	{
		case EVoidRoadType::Highway:    return Highway;
		case EVoidRoadType::Primary:    return Primary;
		case EVoidRoadType::Secondary:  return Secondary;
		case EVoidRoadType::Local:      return Local;
		case EVoidRoadType::Service:    return Service;
		case EVoidRoadType::Alley:      return Alley;
		case EVoidRoadType::Roundabout: return Roundabout;
		default:                        return Local;
	}
}

namespace VoidRoadTypeProfilePrivate
{
	/** Canonical row-name string per type, matching FVoidJsonPackageReader's TryParseRoadType lookup so a DataTable authored against the JSON vocabulary "just works". */
	static FName GetCanonicalRowName(EVoidRoadType RoadType)
	{
		switch (RoadType)
		{
			case EVoidRoadType::Highway:    return TEXT("Highway");
			case EVoidRoadType::Primary:    return TEXT("Primary");
			case EVoidRoadType::Secondary:  return TEXT("Secondary");
			case EVoidRoadType::Local:      return TEXT("Local");
			case EVoidRoadType::Service:    return TEXT("Service");
			case EVoidRoadType::Alley:      return TEXT("Alley");
			case EVoidRoadType::Roundabout: return TEXT("Roundabout");
			default:                        return TEXT("Local");
		}
	}
}

FVoidRoadTypeProfile FVoidRoadTypeProfileLibrary::ResolveProfile(EVoidRoadType RoadType, const UDataTable* OptionalProfileTable)
{
	if (OptionalProfileTable)
	{
		const FName RowName = VoidRoadTypeProfilePrivate::GetCanonicalRowName(RoadType);
		if (const FVoidRoadTypeProfile* Row = OptionalProfileTable->FindRow<FVoidRoadTypeProfile>(RowName, TEXT("FVoidRoadTypeProfileLibrary::ResolveProfile"), /*bWarnIfRowMissing=*/false))
		{
			return *Row;
		}
	}

	return GetBuiltInDefault(RoadType);
}
