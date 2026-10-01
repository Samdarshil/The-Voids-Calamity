// Copyright VOID Engineering Studio. Internal tool - not for shipping content.
// Added by Agent 2 (Phase 6 Metro Generator).

#pragma once

#include "CoreMinimal.h"
#include "Types/VoidWorldBuilderTypes.h"
#include "Types/VoidMetroTypes.h"
#include "VoidMetroData.generated.h"

/**
 * Normalized metro data. This is the ONLY metro representation generators
 * consume. It is produced by the existing Import layer (FVoidMetroNetworkMapper,
 * called from FVoidJsonPackageReader for an embedded "metro" block, or directly
 * for Meridian's MetroNetwork.json) -- generators never touch raw JSON.
 *
 * Every spatial field is OPTIONAL (bHas* flags). Meridian's package policy is
 * "no_fabricated_coordinates_no_fabricated_transforms_no_fabricated_mesh_ids",
 * and MetroNetwork.json accordingly carries topology only. Absent coordinates
 * are resolved at generation time by FVoidMetroLayoutResolver and labelled
 * as placeholders; they are never written back into this struct.
 */

/** A physical entrance to a station. Position optional (see file comment). */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERCORE_API FVoidMetroEntranceSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	FVoidElementId Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	bool bHasPosition = false;

	/** Ground-level XY in Unreal units (cm), world space. Only meaningful if bHasPosition. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	FVector2D Position = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	float YawDegrees = 0.0f;
};

USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERCORE_API FVoidMetroStationSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	FVoidElementId Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	EVoidMetroNetworkKind Network = EVoidMetroNetworkKind::Live;

	/** Meridian district id, e.g. "white_zones". Matches DistrictRegistry.json ids. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	FName DistrictId;

	/** e.g. "base_plaza_monument". Informational. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	FName SubLocationId;

	/** Free-form source tags (tier / type / instancing policy), kept for traceability and debugging. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	FString SourceTier;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	FString SourceType;

	/** Dead-network stations that terminate one of RoadNetwork.json's tunnel_relationships. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	FName SharesTunnelId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	bool bIsTerminus = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	EVoidMetroPlatformCondition PlatformCondition = EVoidMetroPlatformCondition::Maintained;

	/** True if the owning network has a maintenance model (Live). Dead network is "none_abandoned_infrastructure", so no service placeholder is generated for it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	bool bHasMaintenanceFacility = false;

	/** 0 = unspecified. Sector 0's threshold sets 1 (access_point_count / "no_additional_entrances_may_be_generated"). Hard limit for generation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	int32 MaxEntrances = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	bool bHasPosition = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	FVector2D Position = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	bool bHasGradeOverride = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	EVoidMetroGrade Grade = EVoidMetroGrade::AtGrade;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	TArray<FVoidMetroEntranceSpec> Entrances;
};

USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERCORE_API FVoidMetroLineSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	FVoidElementId Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	EVoidMetroNetworkKind Network = EVoidMetroNetworkKind::Live;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	EVoidMetroLineTopology Topology = EVoidMetroLineTopology::Unspecified;

	/** RoadNetwork.json category this line shadows, e.g. "radial_arterial". Informational cross-reference for other systems. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	FName MapsToRoadCategory;

	/** RoadNetwork.json route id this line shares, e.g. "spire_radial_spine". Exposed so the road side can find the metro side. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	FName SharesRouteId;

	/** Radial bands (DistrictRegistry.json radial_bands_ordered) this line spans. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	TArray<FName> ConnectsBands;

	/** Optional explicit membership. Empty = the resolver uses every eligible station in the network. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	TArray<FVoidElementId> StationIds;

	/** Authored centerline (>= 2 points). Empty = resolver infers from Topology. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	TArray<FVector2D> CenterlinePoints;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	bool bClosedLoop = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	bool bHasGradeOverride = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	EVoidMetroGrade Grade = EVoidMetroGrade::AtGrade;
};

/** A dead-network tunnel joining two districts (RoadNetwork.json tunnel_relationships). */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERCORE_API FVoidMetroTunnelLink
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	FVoidElementId Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	FName DistrictA;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	FName DistrictB;

	/** e.g. the Sector 0 threshold is a hard one-directional narrative gate. Informational; exposed to other systems. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	bool bDirectional = false;
};

USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERCORE_API FVoidMetroInterchangeSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	FVoidElementId Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	TArray<FVoidElementId> ConnectedLineIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	FString LocationRule;
};

/** MetroNetwork.json district_connectivity entry. */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERCORE_API FVoidMetroDistrictLink
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	FName DistrictId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	EVoidMetroNetworkKind Network = EVoidMetroNetworkKind::Live;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	FVoidElementId StationRef;
};

USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERCORE_API FVoidMetroNetworkSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	FVoidElementId Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	EVoidMetroNetworkKind Kind = EVoidMetroNetworkKind::Live;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	EVoidMetroPlatformCondition PlatformCondition = EVoidMetroPlatformCondition::Maintained;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	bool bHasMaintenanceAccess = false;
};

/** The complete normalized metro payload carried by FVoidDesignPackage::Metro. */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERCORE_API FVoidMetroData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	TArray<FVoidMetroNetworkSpec> Networks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	TArray<FVoidMetroStationSpec> Stations;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	TArray<FVoidMetroLineSpec> Lines;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	TArray<FVoidMetroInterchangeSpec> Interchanges;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	TArray<FVoidMetroTunnelLink> TunnelLinks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder|Metro")
	TArray<FVoidMetroDistrictLink> DistrictLinks;

	bool HasAnyContent() const
	{
		return Stations.Num() > 0 || Lines.Num() > 0;
	}
};
