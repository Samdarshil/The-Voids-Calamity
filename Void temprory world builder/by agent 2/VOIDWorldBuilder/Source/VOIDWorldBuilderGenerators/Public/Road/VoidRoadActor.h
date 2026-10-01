// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/VoidDesignPackage.h"
#include "VoidRoadActor.generated.h"

class USplineComponent;
class UProceduralMeshComponent;
class UInstancedStaticMeshComponent;

/**
 * AVoidRoadActor
 *
 * Spawned once per road (and once per detected junction, via a separate,
 * simpler actor -- see AVoidRoadJunctionActor) by FVoidRoadGenerator.
 * Holds the components; FVoidRoadGenerator and its builder helpers
 * (FVoidRoadSplineBuilder, FVoidRoadMeshBuilder, FVoidRoadBridgeTunnelBuilder)
 * populate them. This class deliberately contains no generation logic
 * itself -- it's a component container, kept modular and replaceable per
 * the Phase 3 brief ("generated roads must be modular and replaceable"):
 * swapping in a different actor class later only means changing what
 * FVoidRoadGenerator spawns, not rewriting the geometry math.
 *
 * RoadMesh section indices (populated by FVoidRoadGenerator):
 *   0 = road surface
 *   1 = median (only if bHasMedian)
 *   2 = left curb, 3 = right curb (only if bHasSidewalk)
 *   4 = left sidewalk, 5 = right sidewalk (only if bHasSidewalk)
 *
 * RoadMesh collision is enabled only on the road surface section (index 0)
 * -- future Navigation compatibility (explicitly out of scope for Phase 3)
 * will have real, walkable geometry to build a NavMesh against without
 * any change needed here.
 */
UCLASS()
class VOIDWORLDBUILDERGENERATORS_API AVoidRoadActor : public AActor
{
	GENERATED_BODY()

public:
	AVoidRoadActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Road")
	TObjectPtr<USceneComponent> RootScene;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Road")
	TObjectPtr<USplineComponent> RoadSpline;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Road")
	TObjectPtr<UProceduralMeshComponent> RoadMesh;

	/** Bridge pier instances (many, evenly spaced) or tunnel portal instances (exactly 2, start+end) depending on which flag was set -- a road is never both, see FVoidRoadValidator. Unused (zero instances) for a plain road. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Road")
	TObjectPtr<UInstancedStaticMeshComponent> StructureMarkers;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Road")
	FVoidElementId RoadId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Road")
	EVoidRoadType RoadType = EVoidRoadType::Local;
};

/**
 * AVoidRoadJunctionActor
 *
 * Spawned once per detected junction (dead end, cul-de-sac, T/four-way,
 * roundabout spur). Separate from AVoidRoadActor since a junction isn't
 * "a road" -- it has no spline, no lane data, just a pad mesh (and
 * crosswalk stripes for T/four-way junctions).
 */
UCLASS()
class VOIDWORLDBUILDERGENERATORS_API AVoidRoadJunctionActor : public AActor
{
	GENERATED_BODY()

public:
	AVoidRoadJunctionActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Road")
	TObjectPtr<UProceduralMeshComponent> JunctionMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Road")
	TArray<FVoidElementId> ConnectedRoadIds;
};
