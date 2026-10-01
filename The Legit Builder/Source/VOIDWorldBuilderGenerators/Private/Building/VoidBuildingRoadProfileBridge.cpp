// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

// The only translation unit of the Building Generator that touches the Road
// Generator's profile types. Kept separate so the corridor/placement logic in
// VoidBuildingRoadContext.cpp has no engine-asset dependency and can be
// exercised by the standalone verification harness.

#include "Building/VoidBuildingRoadContext.h"
#include "Road/VoidRoadTypeProfile.h"

FVoidRoadCorridorDims FVoidBuildingRoadContext::ResolveDims(const FVoidRoadSpec& Road, const FVoidBuildingGenerationParams& Params)
{
	const FVoidRoadTypeProfile Profile = FVoidRoadTypeProfileLibrary::ResolveProfile(Road.RoadType, Params.RoadProfileTable);

	FVoidRoadCorridorDims Dims;
	Dims.Width = Road.WidthUnits > 0.0f ? Road.WidthUnits : Profile.DefaultWidthUnits;
	// Mirrors FVoidRoadGenerator: kerb + sidewalk ribbons are built only when the spec says bHasSidewalk.
	if (Road.bHasSidewalk)
	{
		Dims.CurbWidth = Profile.CurbWidthUnits;
		Dims.SidewalkWidth = Profile.SidewalkWidthUnits;
	}
	return Dims;
}
