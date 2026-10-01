// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VoidLightingMaterialBuilder.h"
#include "VoidLightingTypes.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionCeil.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionPerInstanceCustomData.h"
#include "MaterialEditingLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

UMaterialInterface* FVoidLightingMaterialBuilder::FindOrCreate(const FString& AssetPath, FString& OutError)
{
	if (UMaterialInterface* Existing = LoadObject<UMaterialInterface>(nullptr, *AssetPath))
	{
		return Existing;
	}

	const FString PackageName = FPackageName::ObjectPathToPackageName(AssetPath);
	const FString AssetName = FPackageName::GetShortName(PackageName);

	if (!FPackageName::IsValidLongPackageName(PackageName))
	{
		OutError = FString::Printf(TEXT("'%s' is not a valid content path."), *AssetPath);
		return nullptr;
	}

	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		OutError = TEXT("CreatePackage failed.");
		return nullptr;
	}

	UMaterial* Material = NewObject<UMaterial>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
	Material->bUsedWithInstancedStaticMeshes = true;
	// Default lit shading with a black base colour (the default). Kept lit rather than Unlit so the emissive surface is treated
	// like any other emissive by Lumen's surface cache; the plates then contribute bounce light without any real light.

	auto* ColorA = Cast<UMaterialExpressionVectorParameter>(UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionVectorParameter::StaticClass(), -900, -200));
	auto* ColorB = Cast<UMaterialExpressionVectorParameter>(UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionVectorParameter::StaticClass(), -900, 0));
	auto* Intensity = Cast<UMaterialExpressionScalarParameter>(UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionScalarParameter::StaticClass(), -900, 200));
	auto* LitFraction = Cast<UMaterialExpressionScalarParameter>(UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionScalarParameter::StaticClass(), -900, 400));
	auto* Threshold = Cast<UMaterialExpressionPerInstanceCustomData>(UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionPerInstanceCustomData::StaticClass(), -900, 600));
	auto* Blend = Cast<UMaterialExpressionPerInstanceCustomData>(UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionPerInstanceCustomData::StaticClass(), -900, 800));
	auto* Lerp = Cast<UMaterialExpressionLinearInterpolate>(UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionLinearInterpolate::StaticClass(), -600, -100));
	auto* Sub = Cast<UMaterialExpressionSubtract>(UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionSubtract::StaticClass(), -600, 500));
	auto* Ceil = Cast<UMaterialExpressionCeil>(UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionCeil::StaticClass(), -400, 500));
	auto* Sat = Cast<UMaterialExpressionSaturate>(UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionSaturate::StaticClass(), -200, 500));
	auto* MulIntensity = Cast<UMaterialExpressionMultiply>(UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionMultiply::StaticClass(), -300, 100));
	auto* MulLit = Cast<UMaterialExpressionMultiply>(UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionMultiply::StaticClass(), 0, 200));

	if (!ColorA || !ColorB || !Intensity || !LitFraction || !Threshold || !Blend || !Lerp || !Sub || !Ceil || !Sat || !MulIntensity || !MulLit)
	{
		OutError = TEXT("Failed to create one or more material expressions.");
		return nullptr;
	}

	ColorA->ParameterName = VoidLightingParams::ColorA;
	ColorA->DefaultValue = FLinearColor(1.0f, 0.8f, 0.5f, 1.0f);
	ColorB->ParameterName = VoidLightingParams::ColorB;
	ColorB->DefaultValue = FLinearColor(0.7f, 0.85f, 1.0f, 1.0f);
	Intensity->ParameterName = VoidLightingParams::Intensity;
	Intensity->DefaultValue = 0.0f;
	LitFraction->ParameterName = VoidLightingParams::LitFraction;
	LitFraction->DefaultValue = 0.0f;
	Threshold->DataIndex = VoidLightingParams::CustomDataThreshold;
	Threshold->ConstDefaultValue = 0.0f;
	Blend->DataIndex = VoidLightingParams::CustomDataBlend;
	Blend->ConstDefaultValue = 0.0f;

	UMaterialEditingLibrary::ConnectMaterialExpressions(ColorA, TEXT(""), Lerp, TEXT("A"));
	UMaterialEditingLibrary::ConnectMaterialExpressions(ColorB, TEXT(""), Lerp, TEXT("B"));
	UMaterialEditingLibrary::ConnectMaterialExpressions(Blend, TEXT(""), Lerp, TEXT("Alpha"));

	UMaterialEditingLibrary::ConnectMaterialExpressions(Lerp, TEXT(""), MulIntensity, TEXT("A"));
	UMaterialEditingLibrary::ConnectMaterialExpressions(Intensity, TEXT(""), MulIntensity, TEXT("B"));

	UMaterialEditingLibrary::ConnectMaterialExpressions(LitFraction, TEXT(""), Sub, TEXT("A"));
	UMaterialEditingLibrary::ConnectMaterialExpressions(Threshold, TEXT(""), Sub, TEXT("B"));
	UMaterialEditingLibrary::ConnectMaterialExpressions(Sub, TEXT(""), Ceil, TEXT("Input"));
	UMaterialEditingLibrary::ConnectMaterialExpressions(Ceil, TEXT(""), Sat, TEXT("Input"));

	UMaterialEditingLibrary::ConnectMaterialExpressions(MulIntensity, TEXT(""), MulLit, TEXT("A"));
	UMaterialEditingLibrary::ConnectMaterialExpressions(Sat, TEXT(""), MulLit, TEXT("B"));

	UMaterialEditingLibrary::ConnectMaterialProperty(MulLit, TEXT(""), MP_EmissiveColor);

	UMaterialEditingLibrary::RecompileMaterial(Material);

	FAssetRegistryModule::AssetCreated(Material);
	Package->MarkPackageDirty();

	// Save so the material survives an editor restart and other machines get it through source control.
	const FString FilePath = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	if (!UPackage::SavePackage(Package, Material, *FilePath, SaveArgs))
	{
		// Not fatal: the asset exists in memory and is dirty; the user can save it manually.
		OutError = FString::Printf(TEXT("Material created in memory but could not be saved to '%s'. Save it manually."), *FilePath);
	}

	return Material;
}
