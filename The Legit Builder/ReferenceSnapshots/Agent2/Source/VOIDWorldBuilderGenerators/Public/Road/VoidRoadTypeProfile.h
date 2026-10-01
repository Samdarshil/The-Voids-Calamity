// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Types/VoidWorldBuilderTypes.h"
#include "VoidRoadTypeProfile.generated.h"

/**
 * FVoidRoadTypeProfile
 *
 * Per-EVoidRoadType default values. FVoidRoadGenerator applies
 * DefaultWidthUnits / DefaultLaneCount / DefaultSpeedLimitUnits whenever
 * a road spec leaves the corresponding field at its "unset" sentinel
 * (WidthUnits <= 0, LaneCount == 0, SpeedLimitUnits == 0) -- an
 * unambiguous signal since none of those are sensible real values.
 *
 * bDefaultHasSidewalk / bDefaultHasMedian are informational only, meant
 * as DataTable-authoring guidance ("Local streets typically have
 * sidewalks") for a designer filling in a profile row -- they are NOT
 * auto-applied to a road spec's bHasSidewalk/bHasMedian. A plain bool
 * has no safe "unset" sentinel the way 0 works for the numeric fields
 * (there's no way to tell "explicitly false" apart from "not specified"
 * without a tri-state), so FVoidRoadGenerator honors those two fields on
 * the spec exactly as authored: false means false, full stop.
 *
 * A FTableRowBase so a design team can author a DataTable of these to
 * tune per-project defaults without touching C++ -- see
 * FVoidRoadTypeProfileLibrary::GetBuiltInDefault for the fallback used
 * when no DataTable is configured.
 *
 * Every road type shares the same generation code path (ribbon geometry
 * in FVoidRoadMeshBuilder); this struct is what makes "every road type
 * must be configurable" true without a switch statement per type.
 */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERGENERATORS_API FVoidRoadTypeProfile : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Road Profile")
	float DefaultWidthUnits = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Road Profile")
	int32 DefaultLaneCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Road Profile")
	int32 DefaultSpeedLimitUnits = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Road Profile")
	bool bDefaultHasSidewalk = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Road Profile")
	bool bDefaultHasMedian = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Road Profile")
	float SidewalkWidthUnits = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Road Profile")
	float CurbWidthUnits = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Road Profile")
	float CurbHeightUnits = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Road Profile")
	float MedianWidthUnits = 100.0f;
};

/**
 * Built-in, hardcoded fallback profiles used when no DataTable is
 * configured in UVoidRoadGenerationSettings (or a given RoadType has no
 * row in it). These exist so the Road Generator works correctly the
 * moment the plugin is enabled, with zero authoring required -- a
 * DataTable is an enhancement, never a prerequisite.
 */
class VOIDWORLDBUILDERGENERATORS_API FVoidRoadTypeProfileLibrary
{
public:
	static const FVoidRoadTypeProfile& GetBuiltInDefault(EVoidRoadType RoadType);

	/**
	 * Looks up a row named after RoadType's UENUM display name (e.g.
	 * "Highway") in OptionalProfileTable. Falls back to
	 * GetBuiltInDefault if OptionalProfileTable is null, hasn't loaded,
	 * or has no matching row.
	 */
	static FVoidRoadTypeProfile ResolveProfile(EVoidRoadType RoadType, const UDataTable* OptionalProfileTable);
};
