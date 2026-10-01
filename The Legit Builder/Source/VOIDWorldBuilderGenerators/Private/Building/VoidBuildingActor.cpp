// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Building/VoidBuildingActor.h"
#include "ProceduralMeshComponent.h"

const FName AVoidBuildingBatchActor::GeneratedTag(TEXT("VoidBuildingsGenerated"));

FName AVoidBuildingBatchActor::MakeDistrictTag(FName InDistrictId)
{
	return FName(*FString::Printf(TEXT("VoidDistrict:%s"), *InDistrictId.ToString()));
}

AVoidBuildingBatchActor::AVoidBuildingBatchActor()
{
	PrimaryActorTick.bCanEverTick = false;

	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(RootScene);

	BuildingMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("BuildingMesh"));
	BuildingMesh->SetupAttachment(RootScene);
	BuildingMesh->SetMobility(EComponentMobility::Static);
	BuildingMesh->bUseComplexAsSimpleCollision = true;
	BuildingMesh->SetCastShadow(true);
}

const FVoidBuildingMetadata* AVoidBuildingBatchActor::FindBuilding(FName BuildingId) const
{
	return Buildings.FindByPredicate([BuildingId](const FVoidBuildingMetadata& M) { return M.BuildingId == BuildingId; });
}
