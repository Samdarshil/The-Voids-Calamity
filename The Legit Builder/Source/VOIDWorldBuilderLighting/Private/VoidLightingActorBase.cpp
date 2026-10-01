// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VoidLightingActorBase.h"
#include "VoidLightingLog.h"
#include "VoidLightingSettings.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/LocalLightComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"

namespace VoidLightingActorPrivate
{
	static constexpr int32 NumElements = static_cast<int32>(EVoidLightingElement::Count);

	static UStaticMesh* LoadBasicMesh(bool bSphere)
	{
		// Engine content present in every project. Cube and Sphere are 100 units on a side / in diameter.
		return LoadObject<UStaticMesh>(nullptr, bSphere ? TEXT("/Engine/BasicShapes/Sphere.Sphere") : TEXT("/Engine/BasicShapes/Cube.Cube"));
	}

	static bool WantsSphere(EVoidLightingElement Element)
	{
		return Element == EVoidLightingElement::EmergencyBeacon || Element == EVoidLightingElement::LandmarkBeacon;
	}
}

AVoidLightingActorBase::AVoidLightingActorBase()
{
	// Everything here is driven by events (preset switch / generation). No Tick, ever.
	PrimaryActorTick.bCanEverTick = false;

	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(RootScene);
	RootScene->SetMobility(EComponentMobility::Static);

	Tags.AddUnique(VoidLightingParams::GeneratedActorTag);
}

void AVoidLightingActorBase::EnsureArrays()
{
	using namespace VoidLightingActorPrivate;

	if (ElementISMs.Num() != NumElements)
	{
		ElementISMs.SetNumZeroed(NumElements);
	}
	if (ElementColorA.Num() != NumElements)
	{
		ElementColorA.Init(FLinearColor::White, NumElements);
	}
	if (ElementColorB.Num() != NumElements)
	{
		ElementColorB.Init(FLinearColor::White, NumElements);
	}
	if (ElementIntensity.Num() != NumElements)
	{
		ElementIntensity.Init(0.0f, NumElements);
	}
	if (ElementLitFraction.Num() != NumElements)
	{
		ElementLitFraction.Init(0.0f, NumElements);
	}
	if (ElementLitBias.Num() != NumElements)
	{
		ElementLitBias.Init(1.0f, NumElements);
	}
	if (ElementIntensityBias.Num() != NumElements)
	{
		ElementIntensityBias.Init(1.0f, NumElements);
	}
	if (ElementMIDs.Num() != NumElements)
	{
		ElementMIDs.SetNumZeroed(NumElements);
	}
}

bool AVoidLightingActorBase::IsEmissiveElement(EVoidLightingElement Element)
{
	return Element != EVoidLightingElement::StreetPole && Element != EVoidLightingElement::TrafficHousing;
}

UInstancedStaticMeshComponent* AVoidLightingActorBase::EnsureElement(EVoidLightingElement Element)
{
	using namespace VoidLightingActorPrivate;

	EnsureArrays();
	const int32 Index = static_cast<int32>(Element);
	if (Index < 0 || Index >= NumElements)
	{
		return nullptr;
	}

	if (ElementISMs[Index])
	{
		return ElementISMs[Index];
	}

	UStaticMesh* Mesh = LoadBasicMesh(WantsSphere(Element));
	if (!Mesh)
	{
		UE_LOG(LogVoidLighting, Error, TEXT("%s: could not load /Engine/BasicShapes mesh for element %d."), *GetName(), Index);
		return nullptr;
	}

	const FName CompName = MakeUniqueObjectName(this, UInstancedStaticMeshComponent::StaticClass(),
		FName(*FString::Printf(TEXT("ISM_%s"), *StaticEnum<EVoidLightingElement>()->GetNameStringByIndex(Index))));

	UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(this, CompName, RF_Transactional);
	ISM->SetStaticMesh(Mesh);
	ISM->SetMobility(EComponentMobility::Static);
	ISM->NumCustomDataFloats = VoidLightingParams::NumCustomData;
	ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ISM->SetCanEverAffectNavigation(false);
	// Thin poles / emissive plates: shadow casting is pure cost here.
	ISM->SetCastShadow(false);
	ISM->bAffectDistanceFieldLighting = false;

	if (const UVoidLightingSettings* Settings = GetDefault<UVoidLightingSettings>())
	{
		if (Settings->InstanceEndCullDistance > 0.0f)
		{
			ISM->InstanceEndCullDistance = static_cast<int32>(Settings->InstanceEndCullDistance);
		}
	}

	ISM->SetupAttachment(GetRootComponent());
	ISM->RegisterComponent();
	AddInstanceComponent(ISM);

	ElementISMs[Index] = ISM;
	return ISM;
}

int32 AVoidLightingActorBase::AddElementInstance(EVoidLightingElement Element, const FTransform& Transform, float Threshold, float Blend)
{
	UInstancedStaticMeshComponent* ISM = EnsureElement(Element);
	if (!ISM)
	{
		return INDEX_NONE;
	}

	const int32 InstanceIndex = ISM->AddInstance(Transform, /*bWorldSpace=*/false);
	if (InstanceIndex != INDEX_NONE)
	{
		// bMarkRenderStateDirty = false: FinishElement dirties once for the whole batch.
		ISM->SetCustomDataValue(InstanceIndex, VoidLightingParams::CustomDataThreshold, Threshold, false);
		ISM->SetCustomDataValue(InstanceIndex, VoidLightingParams::CustomDataBlend, Blend, false);
	}
	return InstanceIndex;
}

void AVoidLightingActorBase::FinishElement(EVoidLightingElement Element)
{
	EnsureArrays();
	if (UInstancedStaticMeshComponent* ISM = ElementISMs[static_cast<int32>(Element)])
	{
		ISM->MarkRenderStateDirty();
		// Every element gets the MID (non-emissive ones stay at Intensity 0 = black), so poles and signal housings never show the default grid material.
		PushMaterialState(Element);
	}
}

void AVoidLightingActorBase::SetElementPalette(EVoidLightingElement Element, const FLinearColor& ColorA, const FLinearColor& ColorB)
{
	EnsureArrays();
	const int32 Index = static_cast<int32>(Element);
	ElementColorA[Index] = ColorA;
	ElementColorB[Index] = ColorB;
	if (ElementISMs[Index])
	{
		PushMaterialState(Element);
	}
}

void AVoidLightingActorBase::SetElementBias(EVoidLightingElement Element, float LitBias, float IntensityBias)
{
	EnsureArrays();
	const int32 Index = static_cast<int32>(Element);
	ElementLitBias[Index] = LitBias;
	ElementIntensityBias[Index] = IntensityBias;
}

UMaterialInstanceDynamic* AVoidLightingActorBase::EnsureMID(EVoidLightingElement Element)
{
	EnsureArrays();
	const int32 Index = static_cast<int32>(Element);

	if (ElementMIDs[Index])
	{
		return ElementMIDs[Index];
	}

	UInstancedStaticMeshComponent* ISM = ElementISMs[Index];
	if (!ISM)
	{
		return nullptr;
	}

	UMaterialInterface* Parent = EmissiveParent;
	if (!Parent)
	{
		if (const UVoidLightingSettings* Settings = GetDefault<UVoidLightingSettings>())
		{
			Parent = !Settings->EmissiveMaterialOverride.IsNull()
				? Settings->EmissiveMaterialOverride.LoadSynchronous()
				: LoadObject<UMaterialInterface>(nullptr, *Settings->GeneratedMaterialPath);
		}
		EmissiveParent = Parent;
	}

	if (!Parent)
	{
		if (!bWarnedNoMaterial)
		{
			bWarnedNoMaterial = true;
			UE_LOG(LogVoidLighting, Warning, TEXT("%s: no emissive parent material available; emissive elements will render with the default material and presets will only drive real lights."), *GetName());
		}
		return nullptr;
	}

	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Parent, this);
	ElementMIDs[Index] = MID;
	ISM->SetMaterial(0, MID);
	return MID;
}

void AVoidLightingActorBase::PushMaterialState(EVoidLightingElement Element)
{
	const int32 Index = static_cast<int32>(Element);
	if (UMaterialInstanceDynamic* MID = EnsureMID(Element))
	{
		MID->SetVectorParameterValue(VoidLightingParams::ColorA, ElementColorA[Index]);
		MID->SetVectorParameterValue(VoidLightingParams::ColorB, ElementColorB[Index]);
		MID->SetScalarParameterValue(VoidLightingParams::Intensity, ElementIntensity[Index]);
		MID->SetScalarParameterValue(VoidLightingParams::LitFraction, ElementLitFraction[Index]);
	}
}

void AVoidLightingActorBase::SetElementState(EVoidLightingElement Element, float Intensity, float LitFraction)
{
	EnsureArrays();
	const int32 Index = static_cast<int32>(Element);
	if (!IsEmissiveElement(Element))
	{
		return;
	}

	ElementIntensity[Index] = FMath::Max(0.0f, Intensity * ElementIntensityBias[Index]);
	ElementLitFraction[Index] = FMath::Clamp(LitFraction * ElementLitBias[Index], 0.0f, 1.0f);

	if (ElementISMs[Index])
	{
		PushMaterialState(Element);
	}
}

void AVoidLightingActorBase::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	// MIDs are transient: rebuild them from the persisted state so a level
	// load or PIE start matches what was last applied in the editor.
	EnsureArrays();
	for (int32 Index = 0; Index < VoidLightingActorPrivate::NumElements; ++Index)
	{
		const EVoidLightingElement Element = static_cast<EVoidLightingElement>(Index);
		if (ElementISMs[Index])
		{
			PushMaterialState(Element);
		}
	}
}

ULocalLightComponent* AVoidLightingActorBase::AddSpotLight(EVoidRealLightRole Role, const FVector& Location, const FRotator& Rotation, float Lumens, float AttenuationRadius, float OuterConeDegrees, float TemperatureK, bool bCastShadows, float VolumetricScattering)
{
	USpotLightComponent* Spot = NewObject<USpotLightComponent>(this, MakeUniqueObjectName(this, USpotLightComponent::StaticClass(), TEXT("VoidSpot")), RF_Transactional);
	Spot->SetMobility(EComponentMobility::Movable);
	Spot->SetupAttachment(GetRootComponent());
	Spot->SetRelativeLocationAndRotation(Location, Rotation);
	Spot->SetIntensityUnits(ELightUnits::Lumens);
	Spot->SetAttenuationRadius(AttenuationRadius);
	Spot->SetOuterConeAngle(OuterConeDegrees);
	Spot->SetInnerConeAngle(OuterConeDegrees * 0.5f);
	Spot->SetUseTemperature(true);
	Spot->SetTemperature(TemperatureK);
	Spot->SetCastShadows(bCastShadows);
	Spot->SetVolumetricScatteringIntensity(VolumetricScattering);
	Spot->SetIntensity(Lumens);
	Spot->RegisterComponent();
	AddInstanceComponent(Spot);

	FVoidLightRecord& Record = Lights.AddDefaulted_GetRef();
	Record.Light = Spot;
	Record.Role = Role;
	Record.BaseIntensity = Lumens;
	Record.BaseVolumetric = VolumetricScattering;
	return Spot;
}

ULocalLightComponent* AVoidLightingActorBase::AddPointLight(EVoidRealLightRole Role, const FVector& Location, float Lumens, float AttenuationRadius, float TemperatureK, bool bCastShadows, float VolumetricScattering)
{
	UPointLightComponent* Point = NewObject<UPointLightComponent>(this, MakeUniqueObjectName(this, UPointLightComponent::StaticClass(), TEXT("VoidPoint")), RF_Transactional);
	Point->SetMobility(EComponentMobility::Movable);
	Point->SetupAttachment(GetRootComponent());
	Point->SetRelativeLocation(Location);
	Point->SetIntensityUnits(ELightUnits::Lumens);
	Point->SetAttenuationRadius(AttenuationRadius);
	Point->SetUseTemperature(true);
	Point->SetTemperature(TemperatureK);
	Point->SetCastShadows(bCastShadows);
	Point->SetVolumetricScatteringIntensity(VolumetricScattering);
	Point->SetIntensity(Lumens);
	Point->RegisterComponent();
	AddInstanceComponent(Point);

	FVoidLightRecord& Record = Lights.AddDefaulted_GetRef();
	Record.Light = Point;
	Record.Role = Role;
	Record.BaseIntensity = Lumens;
	Record.BaseVolumetric = VolumetricScattering;
	return Point;
}

void AVoidLightingActorBase::SetRoleScale(EVoidRealLightRole Role, float Scale, float VolumetricScale)
{
	for (FVoidLightRecord& Record : Lights)
	{
		if (Record.Role != Role || !Record.Light)
		{
			continue;
		}

		const float Intensity = Record.BaseIntensity * FMath::Max(0.0f, Scale);
		const bool bVisible = Intensity > 1.0f; // < 1 lumen is invisible; hide instead of paying for it.
		Record.Light->SetVisibility(bVisible);
		if (bVisible)
		{
			Record.Light->SetIntensity(Intensity);
			Record.Light->SetVolumetricScatteringIntensity(Record.BaseVolumetric * FMath::Max(0.0f, VolumetricScale));
		}
	}
}

int32 AVoidLightingActorBase::GetElementInstanceCount(EVoidLightingElement Element) const
{
	const int32 Index = static_cast<int32>(Element);
	if (ElementISMs.IsValidIndex(Index) && ElementISMs[Index])
	{
		return ElementISMs[Index]->GetInstanceCount();
	}
	return 0;
}

int32 AVoidLightingActorBase::GetNumInstances() const
{
	int32 Total = 0;
	for (const TObjectPtr<UInstancedStaticMeshComponent>& ISM : ElementISMs)
	{
		Total += ISM ? ISM->GetInstanceCount() : 0;
	}
	return Total;
}

void AVoidLightingActorBase::ClearGeneratedComponents()
{
	for (TObjectPtr<UInstancedStaticMeshComponent>& ISM : ElementISMs)
	{
		if (ISM)
		{
			RemoveInstanceComponent(ISM);
			ISM->DestroyComponent();
			ISM = nullptr;
		}
	}
	for (FVoidLightRecord& Record : Lights)
	{
		if (Record.Light)
		{
			RemoveInstanceComponent(Record.Light);
			Record.Light->DestroyComponent();
		}
	}
	Lights.Reset();
	ElementMIDs.Reset();
}
