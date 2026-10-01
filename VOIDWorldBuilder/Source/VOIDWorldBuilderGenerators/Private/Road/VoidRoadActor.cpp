// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Road/VoidRoadActor.h"
#include "Components/SplineComponent.h"
#include "ProceduralMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AVoidRoadActor::AVoidRoadActor()
{
	PrimaryActorTick.bCanEverTick = false;

	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(RootScene);

	RoadSpline = CreateDefaultSubobject<USplineComponent>(TEXT("RoadSpline"));
	RoadSpline->SetupAttachment(RootScene);

	RoadMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("RoadMesh"));
	RoadMesh->SetupAttachment(RootScene);
	RoadMesh->SetMobility(EComponentMobility::Static);

	StructureMarkers = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("StructureMarkers"));
	StructureMarkers->SetupAttachment(RootScene);
	StructureMarkers->SetMobility(EComponentMobility::Static);

	// /Engine/BasicShapes/Cube is Engine content present in every UE
	// project by default -- used as the greybox placeholder for bridge
	// piers / tunnel portals so this doesn't depend on any project-
	// specific mesh asset existing.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		StructureMarkers->SetStaticMesh(CubeMeshFinder.Object);
	}
}

AVoidRoadJunctionActor::AVoidRoadJunctionActor()
{
	PrimaryActorTick.bCanEverTick = false;

	JunctionMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("JunctionMesh"));
	SetRootComponent(JunctionMesh);
	JunctionMesh->SetMobility(EComponentMobility::Static);
}
