// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Data/VoidDesignPackage.h"

/**
 * FVoidBuildingTestFixture
 *
 * *** TEST DATA -- NOT MERIDIAN DATA. ***
 *
 * The Meridian package contains no building footprints, heights or road
 * geometry (its docs defer those to an in-editor level-design pass), so the
 * Building Generator cannot be exercised against Meridian yet. This fixture
 * is a synthetic district written in the Builder's own schema
 * (FVoidBuildingSpec / FVoidRoadSpec) to test the generator: it is never
 * shipped as content, its ids are prefixed "fx_", and its district id is
 * TEST_FIXTURE_NOT_MERIDIAN. Building type tags are only chosen to hit each
 * classifier keyword; they are not claims about Meridian buildings.
 */
class VOIDWORLDBUILDERGENERATORS_API FVoidBuildingTestFixture
{
public:
	static FName DistrictName() { return FName(TEXT("TEST_FIXTURE_NOT_MERIDIAN")); }

	/**
	 * 36 buildings (24 on a road grid, 12 special cases): landmarks (2 in the grid by tag rotation, 2 explicit): L-shape, big
	 * courtyard block, self-intersecting footprint, 2-corner footprint, tiny
	 * footprint, unknown/empty type tag, a building straddling a road, one
	 * slightly too close to a road, and a tall landmark. A bridge road is
	 * included to prove it is ignored for setbacks.
	 */
	static FVoidDistrictData MakeSmallDistrict();

	/** Stress grid: about (Columns * Rows * 5/6) buildings, local roads every 6th column/row. */
	static FVoidDistrictData MakeLargeDistrict(int32 Columns, int32 Rows);
};
