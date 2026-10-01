// Copyright VOID Engineering Studio. Internal tool - not for shipping content.
// Added by Agent 2 (Phase 6 Metro Generator).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Types/VoidMetroTypes.h"
#include "VoidMetroActors.generated.h"

class UProceduralMeshComponent;
class USplineComponent;
class UInstancedStaticMeshComponent;

/**
 * Common base for every generated metro actor. Carries the identity used for
 * cleanup ("destroy everything this generator owns before regenerating") and
 * for other systems to find/trace generated metro content.
 * No Tick: PrimaryActorTick.bCanEverTick is false.
 */
UCLASS(Abstract, NotBlueprintable)
class VOIDWORLDBUILDERGENERATORS_API AVoidMetroActorBase : public AActor
{
	GENERATED_BODY()

public:
	AVoidMetroActorBase();

	/** Stable id, e.g. "radial_line__to__white_zones_node_station_c3_-1_0". Deterministic for a given input. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Metro")
	FName GeneratedId;

	/** Generator ownership key (UVoidMetroGenerationSettings::OwnerKey). Cleanup matches on this. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Metro")
	FName OwnerKey;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Metro")
	FName DistrictId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Metro")
	EVoidMetroNetworkKind NetworkKind = EVoidMetroNetworkKind::Live;

	/** True if any position used to build this actor was derived by the resolver, not authored. Never canon. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Metro")
	bool bIsPlaceholderLayout = false;

	/** World Partition chunk cell this actor was assigned to (XY cell index at the configured chunk size). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Metro")
	FIntPoint ChunkCell = FIntPoint::ZeroValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Metro")
	TObjectPtr<USceneComponent> RootScene;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Metro")
	TObjectPtr<UProceduralMeshComponent> MetroMesh;
};

/**
 * One chunk of track: guideway/rails/tunnel/viaduct geometry plus a spline
 * (for camera rigs and other systems to follow) and instanced sleepers/piers.
 */
UCLASS(NotBlueprintable)
class VOIDWORLDBUILDERGENERATORS_API AVoidMetroTrackActor : public AVoidMetroActorBase
{
	GENERATED_BODY()

public:
	AVoidMetroTrackActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Metro")
	FName LineId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Metro")
	FName SegmentId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Metro")
	EVoidMetroGrade Grade = EVoidMetroGrade::AtGrade;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Metro")
	TObjectPtr<USplineComponent> TrackSpline;

	/** Dead-network sleepers (instanced cube). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Metro")
	TObjectPtr<UInstancedStaticMeshComponent> SleeperInstances;

	/** Viaduct piers for elevated runs (instanced cube). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Metro")
	TObjectPtr<UInstancedStaticMeshComponent> PierInstances;
};

/** A station: platforms, canopy/podium, entrances, escalator/elevator and service placeholders. */
UCLASS(NotBlueprintable)
class VOIDWORLDBUILDERGENERATORS_API AVoidMetroStationActor : public AVoidMetroActorBase
{
	GENERATED_BODY()

public:
	AVoidMetroStationActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Metro")
	FName StationId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Metro")
	EVoidMetroGrade Grade = EVoidMetroGrade::AtGrade;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Metro")
	bool bIsInterchange = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Metro")
	TArray<FVector> EntranceLocations;
};
