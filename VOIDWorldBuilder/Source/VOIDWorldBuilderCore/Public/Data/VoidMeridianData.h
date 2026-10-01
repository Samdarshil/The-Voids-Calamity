// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Types/VoidWorldBuilderTypes.h"
#include "VoidMeridianData.generated.h"

/**
 * Normalized, importer-produced representation of the Meridian_Master package.
 *
 * Field names map 1:1 onto the ACTUAL JSON files in "Meridian Master/" (Meridian_Master.json,
 * DistrictRegistry.json, RoadNetwork.json). Nothing here is invented: Meridian deliberately
 * contains NO coordinates, units, widths, elevations or transforms (Meridian_Master.json
 * builder_configuration.coordinate_policy; validation rule VR-012), so none of these structs have
 * such fields. Spatial placement is produced later by the Builder (see FVoidWorldSpace and
 * FVoidMeridianRoadPlanner) and is always marked as Builder-derived.
 *
 * Only data needed by Phase 1-3 (foundation, import, roads) is materialised. Other registries
 * (Metro, Navigation, Gameplay, Landmark, Infrastructure, WorldPartition, DataLayers, Streaming,
 * BuilderRules) are loaded, schema-checked and kept as raw JSON text in FVoidMeridianWorld::RawRegistries
 * so Agents 2/3+ can add typed mapping without changing the importer's loading pipeline.
 */

USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERCORE_API FVoidMeridianDistrict
{
	GENERATED_BODY()

	/** e.g. "olympus_spire". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	FString DisplayName;

	/** Named radial band, e.g. "core", "mid_tier_rings", "seam_zone", "off_gradient". Qualitative only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	FName RadialBand;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	FName VerticalTier;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	int32 GenerationPriority = 0;

	/** Order the district was DESIGNED in (not generation order). Kept distinct on purpose - see HANDOFF discrepancy notes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	int32 BuildOrderIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	TArray<FName> DependsOn;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	FString DataReference;
};

USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERCORE_API FVoidMeridianRoadCategory
{
	GENERATED_BODY()

	/** "radial_arterial", "ring_road", "service_maintenance_route". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	FName Id;

	/** 1 = highest. From RoadNetwork.json road_categories[].hierarchy_tier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	int32 HierarchyTier = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	EVoidNetworkKind Network = EVoidNetworkKind::Unknown;
};

/** Which of RoadNetwork.json's three route arrays a route came from. */
UENUM(BlueprintType)
enum class EVoidMeridianRouteClass : uint8
{
	Primary,
	Secondary,
	Service
};

/**
 * One entry of primary_routes / secondary_routes / service_routes. The three arrays have different
 * required fields in the real schema; this struct is their union and the unused fields stay empty.
 */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERCORE_API FVoidMeridianRoute
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	EVoidMeridianRouteClass RouteClass = EVoidMeridianRouteClass::Primary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	FName CategoryId;

	/** Resolved from CategoryId; mirrored here so consumers need not re-lookup. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	int32 HierarchyTier = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	EVoidNetworkKind Network = EVoidNetworkKind::Unknown;

	/** primary_routes only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	FName OriginDistrict;

	/** primary_routes only. e.g. "base_plaza_monument". Sub-location ids live in district data files. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	FName OriginSubLocation;

	/** primary_routes: traverses_bands (ordered outward). secondary_routes: single radial_band. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	TArray<FName> Bands;

	/** serves_districts (primary/secondary) or shared_by_districts (service). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	TArray<FName> ServedDistricts;

	/** service_routes only; may be NAME_None (JSON null). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	FName OwningDistrict;

	/** A district id or another route id. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	FName GenerationDependency;

	/** secondary_routes.hard_constraint, verbatim (opaque token, e.g. "must_never_route_through_metro_archives_or_undercroft"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	FString HardConstraint;

	/** service_routes.vehicle_synchronization. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	bool bVehicleSynchronization = false;

	/** True when the Meridian data describes something with a surface footprint the Road Generator may build. Service routes (Dead Network) do not. */
	bool HasSurfaceGeometry() const { return Network == EVoidNetworkKind::Live; }
};

USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERCORE_API FVoidMeridianBridge
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	FName Id;

	/** e.g. "elevated_pedestrian_bridge". A pedestrian skybridge between BUILDINGS, not a road bridge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	FName Type;

	/** Sub-location ids (defined in district data files, e.g. Olympus_Spire_data.json). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	TArray<FName> Connects;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	FName AccessTier;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	bool bFlaggedForSignoff = false;
};

USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERCORE_API FVoidMeridianTunnel
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	FName Type;

	/** Exactly two district ids. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	TArray<FName> Connects;

	/** Non-empty for one-directional narrative gates (Connects[0] -> Connects[1]). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	FString Directionality;
};

USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERCORE_API FVoidMeridianTraversalEdge
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	FName From;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	FName To;

	/** Route id or tunnel id. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	FName Via;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	EVoidNetworkKind Network = EVoidNetworkKind::Unknown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	bool bDirectional = false;
};

USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERCORE_API FVoidMeridianRoadNetwork
{
	GENERATED_BODY()

	/** RoadNetwork.json road_hierarchy, highest tier first. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	TArray<FName> RoadHierarchy;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	TArray<FVoidMeridianRoadCategory> Categories;

	/** Primary, then Secondary, then Service routes, in file order. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	TArray<FVoidMeridianRoute> Routes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	TArray<FVoidMeridianBridge> Bridges;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	TArray<FVoidMeridianTunnel> Tunnels;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	TArray<FName> TraversalNodes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	TArray<FVoidMeridianTraversalEdge> TraversalEdges;

	const FVoidMeridianRoute* FindRoute(FName RouteId) const
	{
		return Routes.FindByPredicate([RouteId](const FVoidMeridianRoute& R) { return R.Id == RouteId; });
	}

	const FVoidMeridianRoadCategory* FindCategory(FName CategoryId) const
	{
		return Categories.FindByPredicate([CategoryId](const FVoidMeridianRoadCategory& C) { return C.Id == CategoryId; });
	}
};

/** Provenance + version info from Meridian_Master.json. */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERCORE_API FVoidMeridianManifest
{
	GENERATED_BODY()

	/** "void_meridian_master_schema_v1" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	FString SchemaId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	FString PackageVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	FString CanonVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	FString Status;

	/** generation_pipeline.import_order (registry keys). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	TArray<FName> ImportOrder;

	/** generation_pipeline.generation_order[].target, in step order. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	TArray<FName> GenerationOrder;

	/** world_scale_metadata.radial_bands, inner to outer. Qualitative names only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	TArray<FName> RadialBands;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	TArray<FName> VerticalTiers;

	/** Ids of flagged_open_items whose status is unresolved. Generators for blocked content must consult this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	TArray<FName> FlaggedOpenItemIds;
};

/**
 * The normalized Meridian world handed to generators (FVoidGenerationContext::Meridian).
 * Produced only by FVoidMeridianImporter; treat as read-only.
 */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERCORE_API FVoidMeridianWorld
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	FVoidMeridianManifest Manifest;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	TArray<FVoidMeridianDistrict> Districts;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	FVoidMeridianRoadNetwork RoadNetwork;

	/** Registry key (e.g. "metro_network") -> raw JSON text. Loaded and schema-checked but not yet typed. */
	UPROPERTY(VisibleAnywhere, Category = "VOID Meridian")
	TMap<FName, FString> RawRegistries;

	/** Folder the package was loaded from. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Meridian")
	FString SourceDirectory;

	const FVoidMeridianDistrict* FindDistrict(FName DistrictId) const
	{
		return Districts.FindByPredicate([DistrictId](const FVoidMeridianDistrict& D) { return D.Id == DistrictId; });
	}

	bool HasFlaggedOpenItem(FName ItemId) const
	{
		return Manifest.FlaggedOpenItemIds.Contains(ItemId);
	}
};
