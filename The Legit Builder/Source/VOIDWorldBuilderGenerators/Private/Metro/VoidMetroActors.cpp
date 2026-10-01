// Copyright VOID Engineering Studio. Internal tool - not for shipping content.
// Added by Agent 2 (Phase 6 Metro Generator).

#include "Metro/VoidMetroActors.h"
#include "ProceduralMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AVoidMetroActorBase::AVoidMetroActorBase()
{
	// Static generated content: never ticks (Step 7, performance).
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	RootScene->SetMobility(EComponentMobility::Static);
	SetRootComponent(RootScene);

	MetroMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("MetroMesh"));
	MetroMesh->SetupAttachment(RootScene);
	MetroMesh->bUseComplexAsSimpleCollision = true;
	MetroMesh->SetMobility(EComponentMobility::Static);
}

AVoidMetroTrackActor::AVoidMetroTrackActor()
{
	TrackSpline = CreateDefaultSubobject<USplineComponent>(TEXT("TrackSpline"));
	TrackSpline->SetupAttachment(RootScene);
	TrackSpline->SetMobility(EComponentMobility::Static);

	// Same shared engine cube the Road Generator instances for its piers.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));

	SleeperInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("SleeperInstances"));
	SleeperInstances->SetupAttachment(RootScene);
	SleeperInstances->SetMobility(EComponentMobility::Static);
	SleeperInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PierInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PierInstances"));
	PierInstances->SetupAttachment(RootScene);
	PierInstances->SetMobility(EComponentMobility::Static);

	if (CubeFinder.Succeeded())
	{
		SleeperInstances->SetStaticMesh(CubeFinder.Object);
		PierInstances->SetStaticMesh(CubeFinder.Object);
	}
}

AVoidMetroStationActor::AVoidMetroStationActor()
{
}
