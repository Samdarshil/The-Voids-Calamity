// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VoidDistrictActors.generated.h"

class UProceduralMeshComponent;
class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class AVoidDistrictActor;
class AVoidDistrictLandmarkActor;

/** Tag every generated actor carries. Regeneration destroys exactly the actors with this tag. */
namespace VoidDistrictTags
{
	inline const FName Generated(TEXT("VoidMeridianGenerated"));
	inline const FName Root(TEXT("VoidMeridianRoot"));
	inline const FName Landmark(TEXT("VoidMeridianLandmark"));
	inline FName District(FName Id) { return FName(*FString::Printf(TEXT("VoidDistrict:%s"), *Id.ToString())); }
	inline FName Layer(const TCHAR* Layer) { return FName(*FString::Printf(TEXT("VoidLayer:%s"), Layer)); }
}

/** Character of a district as generated. Read by Environment / Lighting / Cinematic agents. Every value came from Meridian data or a labelled default. */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERGENERATORS_API FVoidDistrictProfileData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "District")
	FString StructuralModel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "District")
	FString RadialBand;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "District")
	FString VerticalTier;

	/** NearZero | Sparse | Moderate | High */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "District")
	FString Density;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "District")
	double BuildingCoverage = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "District")
	int32 MinStories = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "District")
	int32 MaxStories = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "District")
	double OpenSpaceRatio = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "District")
	double BlockSizeUnits = 0.0;

	/** -1 = Meridian data silent. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "District")
	double CommercialShare = -1.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "District")
	double VegetationDensity = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "District")
	int32 PlazaCellsPerNode = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "District")
	int32 LandmarkCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "District")
	TArray<FString> Provenance;
};

/** Attachment anchor for connective tissue, Metro, or navigation. Never geometry. */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERGENERATORS_API FVoidDistrictPortInfo
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Port")
	FName Id;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Port")
	FString Kind;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Port")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Port")
	TArray<FName> Connects;
};

/** Distributed-node district node (White Zones). */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERGENERATORS_API FVoidDistrictNodeInfo
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Node")
	int32 Index = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Node")
	bool bFlagship = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Node")
	FVector Center = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Node")
	FString WorldPartitionRegion;
};

/**
 * AVoidMeridianRootActor -- top of the ownership tree:
 *   Meridian (this) -> District -> { Roads, Buildings, Public Spaces, Landmarks }
 * Exactly one per generated level.
 */
UCLASS(NotPlaceable)
class VOIDWORLDBUILDERGENERATORS_API AVoidMeridianRootActor : public AActor
{
	GENERATED_BODY()

public:
	AVoidMeridianRootActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Meridian")
	FName MeridianId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Meridian")
	int32 Seed = 1;

	/** Bump when the layout algorithm changes in a way that moves geometry. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Meridian")
	int32 GeneratorVersion = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Meridian")
	TArray<TObjectPtr<AVoidDistrictActor>> Districts;
};

UCLASS(NotPlaceable)
class VOIDWORLDBUILDERGENERATORS_API AVoidDistrictActor : public AActor
{
	GENERATED_BODY()

public:
	AVoidDistrictActor();

	/** Registry id, verbatim (stable across regenerations). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID District")
	FName DistrictId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID District")
	FString DisplayName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID District")
	FVoidDistrictProfileData Profile;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID District")
	TArray<FVoidDistrictPortInfo> Ports;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID District")
	TArray<FVoidDistrictNodeInfo> Nodes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID District")
	TArray<TObjectPtr<AVoidDistrictLandmarkActor>> Landmarks;

	/** Road + junction actors produced by the Road generator for this district, attached under this actor. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID District")
	TArray<TObjectPtr<AActor>> RoadActors;

	/** Actors produced by a registered Building generator, when one was used. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID District")
	TArray<TObjectPtr<AActor>> BuildingActors;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID District")
	int32 NumRoads = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID District")
	int32 NumBuildings = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID District")
	int32 NumPublicSpaces = 0;

	/** Boundary outline(s) (greybox). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID District")
	TObjectPtr<UProceduralMeshComponent> BoundaryMesh;

	/** Parks, plazas, civic spaces (greybox, vertex-coloured by type). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID District")
	TObjectPtr<UProceduralMeshComponent> PublicSpaceMesh;

	/** One instanced-cube component per building category, named "Buildings_<category>". Swap the mesh on a component to replace a category's asset. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID District")
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> BuildingComponents;

	/** Tree markers in parks, scaled by profile vegetation density. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID District")
	TObjectPtr<UInstancedStaticMeshComponent> TreeMarkers;

	UInstancedStaticMeshComponent* FindBuildingComponent(FName CategoryId) const;
};

UCLASS(NotPlaceable)
class VOIDWORLDBUILDERGENERATORS_API AVoidDistrictLandmarkActor : public AActor
{
	GENERATED_BODY()

public:
	AVoidDistrictLandmarkActor();

	/** LandmarkRegistry.json id, verbatim. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Landmark")
	FName LandmarkId;

	/** Unique per placed instance ("<id>.node_2" for per-node fixtures). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Landmark")
	FName InstanceId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Landmark")
	FName DistrictId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Landmark")
	FString LandmarkType;

	/** 1 = most dominant skyline element. -1 = not a skyline landmark (interior / below grade). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Landmark")
	int32 VisibilityTier = -1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Landmark")
	bool bInterior = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Landmark")
	bool bBelowGrade = false;

	/** True until a VoidDistrictLayoutOverrides.json position replaces it. Meridian data has no coordinates by policy. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Landmark")
	bool bPositionIsPlaceholder = true;

	/** Non-empty = BuilderRules.json blocks encounter content here (architecture only). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Landmark")
	FName OpenFlagId;

	/** Non-empty = this landmark IS a district building (same geometry; no separate proxy). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Landmark")
	FName BackingBuildingId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Landmark")
	TArray<FName> SightlineVisibleFrom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Landmark")
	TArray<FName> SightlineExcluded;

	/** Bounds for cameras / lighting / asset replacement (world space, XY half extent + height). Zero for interior anchors. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Landmark")
	FVector2D HalfExtent = FVector2D::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Landmark")
	double Height = 0.0;

	/** Greybox volume for stand-alone landmarks only (null when BackingBuildingId is set or the landmark has no volume). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Landmark")
	TObjectPtr<UStaticMeshComponent> ProxyMesh;
};
