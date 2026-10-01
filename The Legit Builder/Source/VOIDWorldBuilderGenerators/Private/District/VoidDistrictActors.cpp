// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "District/VoidDistrictActors.h"
#include "Components/SceneComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ProceduralMeshComponent.h"

AVoidMeridianRootActor::AVoidMeridianRootActor()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

AVoidDistrictActor::AVoidDistrictActor()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	BoundaryMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("BoundaryMesh"));
	BoundaryMesh->SetupAttachment(Root);
	BoundaryMesh->SetCastShadow(false);

	PublicSpaceMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("PublicSpaceMesh"));
	PublicSpaceMesh->SetupAttachment(Root);
	PublicSpaceMesh->SetCastShadow(false);

	TreeMarkers = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TreeMarkers"));
	TreeMarkers->SetupAttachment(Root);
	TreeMarkers->SetCastShadow(false);
}

UInstancedStaticMeshComponent* AVoidDistrictActor::FindBuildingComponent(FName CategoryId) const
{
	const FString Wanted = FString::Printf(TEXT("Buildings_%s"), *CategoryId.ToString());
	for (UInstancedStaticMeshComponent* Component : BuildingComponents)
	{
		if (Component && Component->GetName() == Wanted)
		{
			return Component;
		}
	}
	return nullptr;
}

AVoidDistrictLandmarkActor::AVoidDistrictLandmarkActor()
{
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	ProxyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProxyMesh"));
	ProxyMesh->SetupAttachment(Root);
}
