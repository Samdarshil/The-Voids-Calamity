// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "District/VoidDistrictTypes.h"
#include "District/VoidDistrictLayoutParams.h"
#include "Meridian/VoidMeridianRegistry.h"

/**
 * Everything the layout planner reads. All pointers are borrowed for the
 * duration of Build(); the plan it returns owns copies of what it needs.
 */
struct VOIDWORLDBUILDERGENERATORS_API FVoidDistrictLayoutInput
{
	/** Required. Meridian registries (see FVoidMeridianRegistryReader). */
	const FVoidMeridianDataSet* Data = nullptr;

	/** Optional designer-authored boundary / landmark position overrides. */
	const FVoidMeridianLayoutOverrides* Overrides = nullptr;

	FVoidDistrictLayoutParams Params;

	/** Optional per-district character overrides, keyed by district id. */
	TMap<FName, FVoidDistrictCharacterOverride> CharacterOverrides;
};

/**
 * FVoidDistrictLayoutBuilder
 *
 * Pure planning stage of the District Generator:
 *
 *   Meridian data  --+
 *   Layout params  --+--> FVoidDistrictLayoutPlan (boundaries, roads,
 *   Overrides      --+                           buildings, public spaces,
 *                                                landmarks, ports)
 *
 * No UWorld, no actors, no meshes. That keeps the part of the generator that
 * decides WHAT goes WHERE deterministic and testable in isolation, and lets
 * the commandlet validate a Meridian package in CI without loading a level.
 * FVoidDistrictGenerator turns the plan into actors.
 *
 * Layout rules implemented, and where each comes from:
 *   - Which districts exist, their order, dependencies, adjacency, radial
 *     band / vertical tier / structural model: DistrictRegistry.json.
 *   - Which roads exist, their network (live vs dead), the ring-road hard
 *     constraint, the two tunnels: RoadNetwork.json.
 *   - Which landmarks exist, their skyline tiers, sightline rules, per-node
 *     instancing: LandmarkRegistry.json.
 *   - Density bands, White Zones 2-4 story ceiling, "mid-rise" Archives:
 *     Meridian_Master_Plan.md Sec. 5/22 (cited in each profile's Provenance).
 *   - Everything the data leaves unspecified (coordinates, sizes, counts):
 *     FVoidDistrictLayoutParams / VoidDistrictLayoutOverrides.json.
 */
class VOIDWORLDBUILDERGENERATORS_API FVoidDistrictLayoutBuilder
{
public:
	static FVoidDistrictLayoutPlan Build(const FVoidDistrictLayoutInput& Input);

	/** Resolves one district's character. Exposed for tests and for the actors' exposed profile data. */
	static FVoidDistrictProfile ResolveProfile(const FVoidMeridianDistrictEntry& Entry, const FVoidDistrictLayoutInput& Input, int32 LandmarkCount);

	/**
	 * Adapter for callers that already have an explicit FVoidDesignPackage for
	 * one Meridian district (the pre-Phase-5 contract): its roads and
	 * buildings replace the synthesized ones for that district. Coordinates
	 * are taken as world-space. Returns false if the package's district id is
	 * not in the plan.
	 */
	static bool ApplyExplicitPackage(FVoidDistrictLayoutPlan& InOutPlan, const FVoidDesignPackage& Package);
};
