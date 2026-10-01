// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Types/VoidWorldBuilderTypes.h"

/**
 * ROAD OUTPUT CONTRACT
 *
 * What the Road Generator publishes for Building / District / Environment / Metro / Navigation
 * generators. Consumers query IVoidRoadQuery (via FVoidGenerationContext::RoadOutput) and never parse
 * Meridian JSON or inspect road actors.
 *
 * Conventions: all positions are world-space Unreal units (cm), produced by FVoidWorldSpace.
 * "Left"/"Right" are in the direction of increasing distance along the centerline, in Unreal's
 * left-handed frame (heading +X, Left = -Y, Right = +Y).
 * Sidewalk data exists ONLY where the road spec actually had a sidewalk - Meridian-derived roads do
 * not (Meridian specifies none), and that is reported honestly via bHasSidewalk == false.
 */

enum class EVoidIntersectionKind : uint8
{
	DeadEnd,
	CulDeSac,
	TwoWayJoin,
	TJunction,
	FourWayJunction,
	Complex,
	RoundaboutSpur,
	/** Two roads crossing mid-span (e.g. a radial spine crossing a ring road). Never produced by endpoint clustering. */
	Crossing
};

enum class EVoidRoadSide : uint8
{
	Left,
	Right
};

/** A point sampled on a road: position, unit tangent direction, and distance along the centerline. */
struct VOIDWORLDBUILDERCORE_API FVoidRoadSample
{
	FName RoadId;
	FVector Position = FVector::ZeroVector;
	/** Unit tangent in the direction of travel (3D, includes any bridge/tunnel slope). */
	FVector Direction = FVector::ForwardVector;
	double DistanceAlong = 0.0;
};

struct VOIDWORLDBUILDERCORE_API FVoidRoadRecord
{
	/** Legacy road id, or Meridian route id for Meridian-derived roads. */
	FName RoadId;
	EVoidRoadSource Source = EVoidRoadSource::LegacyDesignPackage;
	EVoidRoadType RoadType = EVoidRoadType::Local;

	/** Meridian hierarchy tier (1 = highest). For legacy roads: 1 Highway/Primary/Roundabout, 2 Secondary, 3 Local, 4 Service/Alley. */
	int32 HierarchyTier = 0;
	EVoidNetworkKind Network = EVoidNetworkKind::Unknown;
	/** Meridian category id (e.g. "radial_arterial"); NAME_None for legacy roads. */
	FName CategoryId;

	/** Total carriageway width, Unreal units. */
	double WidthUU = 0.0;
	/** True when WidthUU came from a Builder default profile rather than authored data (always true for Meridian-derived roads). */
	bool bWidthIsBuilderDefault = false;
	int32 LaneCount = 0;

	bool bClosedLoop = false;
	bool bIsBridge = false;
	bool bIsTunnel = false;
	bool bHasSidewalk = false;
	bool bHasMedian = false;

	/** Sidewalk band, measured from the centerline outward (curb included in the inner offset). Both 0 when bHasSidewalk is false. */
	double SidewalkInnerOffsetUU = 0.0;
	double SidewalkOuterOffsetUU = 0.0;

	/** False for topology-only records (e.g. Dead Network service routes: Meridian gives them no surface footprint). Centerline is empty then. */
	bool bHasGeometry = true;

	/** World-space centerline. For closed loops the last point does NOT repeat the first. */
	TArray<FVector> Centerline;

	/** District ids this road serves (Meridian) - lets District/Building generators find "their" roads. */
	TArray<FName> ServedDistrictIds;

	/** Filled by FVoidRoadNetworkOutput::RebuildConnectivity() from shared intersections. */
	TArray<FName> ConnectedRoadIds;

	/** Spawned actor, if any (weak: may be null after undo/reset). */
	TWeakObjectPtr<AActor> Actor;

	/** Total 3D centerline length (including the closing segment for loops). */
	double GetLength() const;
};

struct VOIDWORLDBUILDERCORE_API FVoidIntersectionRecord
{
	FName IntersectionId;
	FVector Location = FVector::ZeroVector;
	EVoidIntersectionKind Kind = EVoidIntersectionKind::DeadEnd;
	TArray<FName> RoadIds;
	/** Pad radius in Unreal units (0 if no pad was generated). */
	double PadRadiusUU = 0.0;
};

/** Read-only query interface. Agents 2/3 code against this; tests can mock it. */
class VOIDWORLDBUILDERCORE_API IVoidRoadQuery
{
public:
	virtual ~IVoidRoadQuery() = default;

	virtual const TArray<FVoidRoadRecord>& GetRoads() const = 0;
	virtual const TArray<FVoidIntersectionRecord>& GetIntersections() const = 0;

	virtual const FVoidRoadRecord* FindRoad(FName RoadId) const = 0;
	virtual TArray<const FVoidRoadRecord*> GetRoadsByType(EVoidRoadType Type) const = 0;
	/** Roads with HierarchyTier == Tier. */
	virtual TArray<const FVoidRoadRecord*> GetRoadsByTier(int32 Tier) const = 0;
	virtual TArray<const FVoidRoadRecord*> GetRoadsServingDistrict(FName DistrictId) const = 0;

	/** Position + direction at DistanceAlong (clamped for open roads, wrapped for closed loops). False if the road is unknown or has no geometry. */
	virtual bool SampleRoad(FName RoadId, double DistanceAlong, FVoidRoadSample& OutSample) const = 0;

	/** Nearest point on any road with geometry, by XY distance. False if there is no geometry. */
	virtual bool FindNearestRoadPoint(const FVector& Query, FVoidRoadSample& OutSample, double& OutDistance2D) const = 0;

	/** Offset polyline(s) bounding the sidewalk on one side. False if the road has no sidewalk / no geometry. */
	virtual bool GetSidewalkBoundary(FName RoadId, EVoidRoadSide Side, TArray<FVector>& OutInnerEdge, TArray<FVector>& OutOuterEdge) const = 0;

	virtual TArray<const FVoidIntersectionRecord*> GetIntersectionsForRoad(FName RoadId) const = 0;
	virtual const FVoidIntersectionRecord* FindNearestIntersection(const FVector& Query, double MaxDistance2D) const = 0;
};

/** Concrete road output, written by the Road Generator. */
class VOIDWORLDBUILDERCORE_API FVoidRoadNetworkOutput : public IVoidRoadQuery
{
public:
	void Reset();
	void AddRoad(FVoidRoadRecord Record);
	void AddIntersection(FVoidIntersectionRecord Record);
	/** Populates ConnectedRoadIds on every road from the intersection list. Call once after all roads/intersections are added. */
	void RebuildConnectivity();

	FVoidRoadRecord* FindRoadMutable(FName RoadId);

	//~ Begin IVoidRoadQuery
	virtual const TArray<FVoidRoadRecord>& GetRoads() const override { return Roads; }
	virtual const TArray<FVoidIntersectionRecord>& GetIntersections() const override { return Intersections; }
	virtual const FVoidRoadRecord* FindRoad(FName RoadId) const override;
	virtual TArray<const FVoidRoadRecord*> GetRoadsByType(EVoidRoadType Type) const override;
	virtual TArray<const FVoidRoadRecord*> GetRoadsByTier(int32 Tier) const override;
	virtual TArray<const FVoidRoadRecord*> GetRoadsServingDistrict(FName DistrictId) const override;
	virtual bool SampleRoad(FName RoadId, double DistanceAlong, FVoidRoadSample& OutSample) const override;
	virtual bool FindNearestRoadPoint(const FVector& Query, FVoidRoadSample& OutSample, double& OutDistance2D) const override;
	virtual bool GetSidewalkBoundary(FName RoadId, EVoidRoadSide Side, TArray<FVector>& OutInnerEdge, TArray<FVector>& OutOuterEdge) const override;
	virtual TArray<const FVoidIntersectionRecord*> GetIntersectionsForRoad(FName RoadId) const override;
	virtual const FVoidIntersectionRecord* FindNearestIntersection(const FVector& Query, double MaxDistance2D) const override;
	//~ End IVoidRoadQuery

	/** Lateral offset of a polyline by SignedOffset (positive = Left). Shared with tests. Uses the same central-difference tangents as the mesh builder. */
	static TArray<FVector> OffsetPolyline(const TArray<FVector>& Points, bool bClosed, double SignedOffsetLeft);

private:
	TArray<FVoidRoadRecord> Roads;
	TArray<FVoidIntersectionRecord> Intersections;
	TMap<FName, int32> RoadIndexById;
};
