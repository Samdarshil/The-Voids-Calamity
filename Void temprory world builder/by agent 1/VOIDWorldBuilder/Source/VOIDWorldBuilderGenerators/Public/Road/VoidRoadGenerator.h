// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IVoidGenerator.h"
#include "Road/VoidMeridianRoadPlanner.h"

class AActor;

/**
 * FVoidRoadGenerator
 *
 * Registered with FVoidGeneratorRegistry under the id "Road" by
 * FVOIDWorldBuilderGeneratorsModule::StartupModule(). Two input paths share ONE build core:
 *
 *   Generate(FVoidDesignPackage)        legacy design package: explicit 2D centerlines, converted through
 *                                       Context.WorldSpace (the single coordinate conversion).
 *   GenerateFromMeridian(FVoidMeridianWorld)  Meridian: FVoidMeridianRoadPlanner derives roads from the real
 *                                       RoadNetwork (radial arterial, ring road); Dead-Network routes, tunnels
 *                                       and the skybridge are recorded as topology only - never invented.
 *
 * Spawns one AVoidRoadActor per road and one AVoidRoadJunctionActor per junction / analytic crossing, then
 * publishes FVoidRoadNetworkOutput into Context.RoadOutput for Agents 2/3+ to query.
 *
 * Large-world safety: every generated actor is placed at its own local origin (bounding-box centre) with
 * geometry stored relative to it, so float32 vertex buffers never hold large coordinates.
 *
 * Lifecycle: Reset() destroys everything this generator spawned and clears the published output.
 */
class VOIDWORLDBUILDERGENERATORS_API FVoidRoadGenerator : public IVoidGenerator
{
public:
	//~ Begin IVoidGenerator
	virtual FName GetGeneratorId() const override;
	virtual FText GetDisplayName() const override;
	virtual bool SupportsPackageInput() const override { return true; }
	virtual bool SupportsMeridianInput() const override { return true; }
	virtual bool Generate(const FVoidDesignPackage& Package, FVoidGenerationContext& Context) override;
	virtual bool GenerateFromMeridian(const FVoidMeridianWorld& World, FVoidGenerationContext& Context) override;
	virtual void Reset(FVoidGenerationContext& Context) override;
	//~ End IVoidGenerator

private:
	/** Per-road facts the planner/package knows and the raw spec does not. */
	struct FRoadMeta
	{
		EVoidRoadSource Source = EVoidRoadSource::LegacyDesignPackage;
		int32 HierarchyTier = 0;
		EVoidNetworkKind Network = EVoidNetworkKind::Unknown;
		FName CategoryId;
		TArray<FName> ServedDistricts;
		bool bWidthIsBuilderDefault = false;
	};

	struct FBuildRequest
	{
		FName DistrictLabel;
		/** True when spec points are already world space (Meridian planner output). False: convert via Context.WorldSpace. */
		bool bSpecsAreWorldSpace = false;
		TMap<FName, FRoadMeta> MetaByRoadId;
		TArray<FVoidPlannedCrossing> Crossings;
		TArray<FVoidTopologyOnlyRoute> TopologyOnly;
	};

	bool BuildRoads(const TArray<FVoidRoadSpec>& Roads, const FBuildRequest& Request, FVoidGenerationContext& Context);

	/** Actors spawned by this generator, for Reset(). Weak: safe if the user deleted them. */
	TArray<TWeakObjectPtr<AActor>> SpawnedActors;
};
