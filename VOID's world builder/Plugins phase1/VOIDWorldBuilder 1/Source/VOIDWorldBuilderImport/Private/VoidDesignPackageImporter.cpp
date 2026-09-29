// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VoidDesignPackageImporter.h"
#include "VoidJsonReader.h"
#include "VoidPackageValidator.h"
#include "VoidWorldBuilderLog.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace VoidImportPrivate
{
	static FVector2D JsonValueToVector2D(const TSharedPtr<FJsonValue>& Value)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Value.IsValid() && Value->TryGetArray(Arr) && Arr->Num() >= 2)
		{
			return FVector2D((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber());
		}
		return FVector2D::ZeroVector;
	}
}

FVoidImportResult FVoidDesignPackageImporter::LoadFromFile(const FString& FilePath)
{
	FVoidImportResult Result;

	TSharedPtr<FJsonObject> JsonObject;
	FString ReadError;
	if (!FVoidJsonReader::ReadFromFile(FilePath, JsonObject, ReadError))
	{
		Result.ValidationReport.AddError(ReadError);
		UE_LOG(LogVoidWorldBuilder, Error, TEXT("Import failed: %s"), *ReadError);
		return Result;
	}

	TArray<FString> MappingWarnings;
	Result.Package = MapJsonToPackage(JsonObject, MappingWarnings);

	for (const FString& Warning : MappingWarnings)
	{
		Result.ValidationReport.AddWarning(Warning);
	}

	Result.ValidationReport = FVoidPackageValidator::Validate(Result.Package, Result.ValidationReport);

	UE_LOG(LogVoidWorldBuilder, Log, TEXT("Imported package '%s': valid=%s, errors=%d, warnings=%d"),
		*Result.Package.DistrictId.Value.ToString(),
		Result.ValidationReport.bIsValid ? TEXT("true") : TEXT("false"),
		Result.ValidationReport.NumErrors(),
		Result.ValidationReport.NumWarnings());

	return Result;
}

FVoidImportResult FVoidDesignPackageImporter::LoadFromJsonString(const FString& RawJson)
{
	FVoidImportResult Result;

	TSharedPtr<FJsonObject> JsonObject;
	FString ReadError;
	if (!FVoidJsonReader::ParseString(RawJson, JsonObject, ReadError))
	{
		Result.ValidationReport.AddError(ReadError);
		return Result;
	}

	TArray<FString> MappingWarnings;
	Result.Package = MapJsonToPackage(JsonObject, MappingWarnings);

	for (const FString& Warning : MappingWarnings)
	{
		Result.ValidationReport.AddWarning(Warning);
	}

	Result.ValidationReport = FVoidPackageValidator::Validate(Result.Package, Result.ValidationReport);
	return Result;
}

FVoidDesignPackage FVoidDesignPackageImporter::MapJsonToPackage(const TSharedPtr<FJsonObject>& JsonObject, TArray<FString>& OutMappingWarnings)
{
	using namespace VoidImportPrivate;

	FVoidDesignPackage Package;

	if (!JsonObject.IsValid())
	{
		OutMappingWarnings.Add(TEXT("Root JSON object was invalid; returning empty package."));
		return Package;
	}

	FString DistrictIdString;
	if (JsonObject->TryGetStringField(TEXT("districtId"), DistrictIdString) && !DistrictIdString.IsEmpty())
	{
		Package.DistrictId = FVoidElementId(FName(*DistrictIdString));
	}
	else
	{
		OutMappingWarnings.Add(TEXT("Package is missing a non-empty 'districtId'."));
	}

	const TSharedPtr<FJsonObject>* MetadataObject = nullptr;
	if (JsonObject->TryGetObjectField(TEXT("metadata"), MetadataObject))
	{
		(*MetadataObject)->TryGetStringField(TEXT("sourceDocumentName"), Package.Metadata.SourceDocumentName);
		(*MetadataObject)->TryGetStringField(TEXT("sourceDocumentVersion"), Package.Metadata.SourceDocumentVersion);

		bool bIsApproved = false;
		if ((*MetadataObject)->TryGetBoolField(TEXT("isApproved"), bIsApproved))
		{
			Package.Metadata.bIsApproved = bIsApproved;
		}
		else
		{
			OutMappingWarnings.Add(TEXT("Package metadata is missing 'isApproved'; defaulting to not approved."));
		}
	}
	else
	{
		OutMappingWarnings.Add(TEXT("Package is missing a 'metadata' object."));
	}

	const TArray<TSharedPtr<FJsonValue>>* BuildingsArray = nullptr;
	if (JsonObject->TryGetArrayField(TEXT("buildings"), BuildingsArray))
	{
		for (int32 Index = 0; Index < BuildingsArray->Num(); ++Index)
		{
			const TSharedPtr<FJsonValue>& BuildingValue = (*BuildingsArray)[Index];

			const TSharedPtr<FJsonObject>* BuildingObject = nullptr;
			if (!BuildingValue->TryGetObject(BuildingObject))
			{
				OutMappingWarnings.Add(FString::Printf(TEXT("Skipped a non-object entry at buildings[%d]."), Index));
				continue;
			}

			FVoidBuildingSpec Building;

			FString BuildingIdString;
			if ((*BuildingObject)->TryGetStringField(TEXT("id"), BuildingIdString) && !BuildingIdString.IsEmpty())
			{
				Building.Id = FVoidElementId(FName(*BuildingIdString));
			}
			else
			{
				OutMappingWarnings.Add(FString::Printf(TEXT("buildings[%d] is missing a non-empty 'id'."), Index));
			}

			double HeightUnitsValue = 0.0;
			if ((*BuildingObject)->TryGetNumberField(TEXT("heightUnits"), HeightUnitsValue))
			{
				Building.HeightUnits = static_cast<float>(HeightUnitsValue);
			}
			else
			{
				OutMappingWarnings.Add(FString::Printf(TEXT("buildings[%d] is missing 'heightUnits'."), Index));
			}

			(*BuildingObject)->TryGetStringField(TEXT("buildingType"), Building.BuildingType);

			const TArray<TSharedPtr<FJsonValue>>* FootprintArray = nullptr;
			if ((*BuildingObject)->TryGetArrayField(TEXT("footprintCorners"), FootprintArray))
			{
				for (const TSharedPtr<FJsonValue>& CornerValue : *FootprintArray)
				{
					Building.FootprintCorners.Add(JsonValueToVector2D(CornerValue));
				}
			}
			else
			{
				OutMappingWarnings.Add(FString::Printf(TEXT("buildings[%d] is missing 'footprintCorners'."), Index));
			}

			Package.Buildings.Add(MoveTemp(Building));
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* RoadsArray = nullptr;
	if (JsonObject->TryGetArrayField(TEXT("roads"), RoadsArray))
	{
		for (int32 Index = 0; Index < RoadsArray->Num(); ++Index)
		{
			const TSharedPtr<FJsonValue>& RoadValue = (*RoadsArray)[Index];

			const TSharedPtr<FJsonObject>* RoadObject = nullptr;
			if (!RoadValue->TryGetObject(RoadObject))
			{
				OutMappingWarnings.Add(FString::Printf(TEXT("Skipped a non-object entry at roads[%d]."), Index));
				continue;
			}

			FVoidRoadSpec Road;

			FString RoadIdString;
			if ((*RoadObject)->TryGetStringField(TEXT("id"), RoadIdString) && !RoadIdString.IsEmpty())
			{
				Road.Id = FVoidElementId(FName(*RoadIdString));
			}
			else
			{
				OutMappingWarnings.Add(FString::Printf(TEXT("roads[%d] is missing a non-empty 'id'."), Index));
			}

			double WidthUnitsValue = 0.0;
			if ((*RoadObject)->TryGetNumberField(TEXT("widthUnits"), WidthUnitsValue))
			{
				Road.WidthUnits = static_cast<float>(WidthUnitsValue);
			}
			else
			{
				OutMappingWarnings.Add(FString::Printf(TEXT("roads[%d] is missing 'widthUnits'."), Index));
			}

			const TArray<TSharedPtr<FJsonValue>>* CenterlineArray = nullptr;
			if ((*RoadObject)->TryGetArrayField(TEXT("centerlinePoints"), CenterlineArray))
			{
				for (const TSharedPtr<FJsonValue>& PointValue : *CenterlineArray)
				{
					Road.CenterlinePoints.Add(JsonValueToVector2D(PointValue));
				}
			}
			else
			{
				OutMappingWarnings.Add(FString::Printf(TEXT("roads[%d] is missing 'centerlinePoints'."), Index));
			}

			Package.Roads.Add(MoveTemp(Road));
		}
	}

	return Package;
}
