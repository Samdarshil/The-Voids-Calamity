// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VoidPackageValidator.h"
#include "Containers/Set.h"

FVoidValidationReport FVoidPackageValidator::Validate(const FVoidDesignPackage& Package, FVoidValidationReport InReport)
{
	FVoidValidationReport Report = MoveTemp(InReport);
	Report.bIsValid = true; // Flipped false by any AddError call below.

	ValidateMetadata(Package, Report);
	ValidateBuildings(Package, Report);
	ValidateRoads(Package, Report);
	ValidateIdUniqueness(Package, Report);

	return Report;
}

void FVoidPackageValidator::ValidateMetadata(const FVoidDesignPackage& Package, FVoidValidationReport& Report)
{
	if (!Package.DistrictId.IsValid())
	{
		Report.AddError(TEXT("Package is missing a districtId."), TEXT("districtId"));
	}

	if (Package.Metadata.SourceDocumentName.IsEmpty())
	{
		Report.AddError(TEXT("Package metadata is missing sourceDocumentName."), TEXT("metadata.sourceDocumentName"));
	}

	if (!Package.Metadata.bIsApproved)
	{
		Report.AddError(TEXT("Package is not marked approved. Unapproved design packages cannot be generated."), TEXT("metadata.isApproved"));
	}
}

void FVoidPackageValidator::ValidateBuildings(const FVoidDesignPackage& Package, FVoidValidationReport& Report)
{
	for (int32 Index = 0; Index < Package.Buildings.Num(); ++Index)
	{
		const FVoidBuildingSpec& Building = Package.Buildings[Index];
		const FString FieldPrefix = FString::Printf(TEXT("buildings[%d]"), Index);

		if (!Building.Id.IsValid())
		{
			Report.AddError(TEXT("Building is missing an id."), FieldPrefix + TEXT(".id"));
		}

		if (Building.FootprintCorners.Num() < 3)
		{
			Report.AddError(TEXT("Building footprint must have at least 3 corners."), FieldPrefix + TEXT(".footprintCorners"));
		}

		if (Building.HeightUnits <= 0.0f)
		{
			Report.AddError(TEXT("Building height must be greater than zero."), FieldPrefix + TEXT(".heightUnits"));
		}

		if (Building.BuildingType.IsEmpty())
		{
			Report.AddWarning(TEXT("Building has no buildingType tag; will generate as an untyped placeholder."), FieldPrefix + TEXT(".buildingType"));
		}
	}
}

void FVoidPackageValidator::ValidateRoads(const FVoidDesignPackage& Package, FVoidValidationReport& Report)
{
	for (int32 Index = 0; Index < Package.Roads.Num(); ++Index)
	{
		const FVoidRoadSpec& Road = Package.Roads[Index];
		const FString FieldPrefix = FString::Printf(TEXT("roads[%d]"), Index);

		if (!Road.Id.IsValid())
		{
			Report.AddError(TEXT("Road is missing an id."), FieldPrefix + TEXT(".id"));
		}

		if (Road.CenterlinePoints.Num() < 2)
		{
			Report.AddError(TEXT("Road centerline must have at least 2 points."), FieldPrefix + TEXT(".centerlinePoints"));
		}

		if (Road.WidthUnits <= 0.0f)
		{
			Report.AddError(TEXT("Road width must be greater than zero."), FieldPrefix + TEXT(".widthUnits"));
		}
	}
}

void FVoidPackageValidator::ValidateIdUniqueness(const FVoidDesignPackage& Package, FVoidValidationReport& Report)
{
	TSet<FName> SeenIds;

	auto CheckId = [&Report, &SeenIds](const FVoidElementId& Id, const FString& Context)
	{
		if (!Id.IsValid())
		{
			return; // Already reported as missing by the per-type validator above.
		}

		if (SeenIds.Contains(Id.Value))
		{
			Report.AddError(FString::Printf(TEXT("Duplicate element id '%s'."), *Id.Value.ToString()), Context);
		}
		else
		{
			SeenIds.Add(Id.Value);
		}
	};

	for (int32 Index = 0; Index < Package.Buildings.Num(); ++Index)
	{
		CheckId(Package.Buildings[Index].Id, FString::Printf(TEXT("buildings[%d].id"), Index));
	}

	for (int32 Index = 0; Index < Package.Roads.Num(); ++Index)
	{
		CheckId(Package.Roads[Index].Id, FString::Printf(TEXT("roads[%d].id"), Index));
	}
}
