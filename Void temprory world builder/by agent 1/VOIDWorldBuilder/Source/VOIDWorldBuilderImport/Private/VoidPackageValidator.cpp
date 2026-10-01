// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VoidPackageValidator.h"
#include "Containers/Set.h"
#include "Coordinates/VoidWorldSpace.h"

FVoidValidationReport FVoidPackageValidator::Validate(const FVoidDesignPackage& Package, FVoidValidationReport InReport)
{
	FVoidValidationReport Report = MoveTemp(InReport);

	if (Report.HasFatalIssue())
	{
		// A prior stage (reading, mapping) already hit something
		// unrecoverable. Piling on more validation noise about data we
		// couldn't properly read in the first place would only obscure
		// the actual problem.
		return Report;
	}

	Report.bIsValid = true; // Flipped false by any AddError/AddFatal call below.

	ValidateSchemaVersion(Package, Report);
	if (Report.HasFatalIssue())
	{
		return Report; // Incompatible schema major version -- nothing below can be trusted to mean what it says.
	}

	ValidateMetadata(Package, Report);
	ValidateDistrict(Package, Report);

	return Report;
}

void FVoidPackageValidator::ValidateSchemaVersion(const FVoidDesignPackage& Package, FVoidValidationReport& Report)
{
	const FVoidSchemaVersion& Current = FVoidSchemaVersion::CurrentToolVersion();
	const FVoidSchemaVersion& PackageVersion = Package.SchemaVersion;

	if (PackageVersion.Major > Current.Major)
	{
		Report.AddFatal(
			FString::Printf(TEXT("Package schema version %s is newer than this importer supports (%s)."), *PackageVersion.ToString(), *Current.ToString()),
			TEXT("schemaVersion"),
			TEXT("VOID.Import.IncompatibleSchemaMajor"),
			TEXT("Update the VOID World Builder plugin to a version that supports this schema, or re-export the package at an older schema version."));
		return;
	}

	if (PackageVersion.Major < Current.Major)
	{
		Report.AddFatal(
			FString::Printf(TEXT("Package schema version %s is older than this importer supports (%s) and no migration path exists."), *PackageVersion.ToString(), *Current.ToString()),
			TEXT("schemaVersion"),
			TEXT("VOID.Import.IncompatibleSchemaMajor"),
			TEXT("Re-export the package from its original source at the current schema version."));
		return;
	}

	if (PackageVersion.Minor > Current.Minor)
	{
		Report.AddWarning(
			FString::Printf(TEXT("Package schema version %s is newer (minor) than this importer was built against (%s); newer optional fields may be ignored."), *PackageVersion.ToString(), *Current.ToString()),
			TEXT("schemaVersion"),
			TEXT("VOID.Import.NewerSchemaMinor"),
			TEXT("No action required unless generated output is missing data you expect from newer fields."));
	}
}

void FVoidPackageValidator::ValidateMetadata(const FVoidDesignPackage& Package, FVoidValidationReport& Report)
{
	if (Package.Metadata.SourceDocumentName.IsEmpty())
	{
		Report.AddError(TEXT("Package metadata is missing sourceDocumentName."), TEXT("metadata.sourceDocumentName"),
			TEXT("VOID.Import.MissingField"), TEXT("Add a non-empty 'sourceDocumentName' to the package's metadata object."));
	}

	if (!Package.Metadata.bIsApproved)
	{
		Report.AddError(TEXT("Package is not marked approved. Unapproved design packages cannot be generated."), TEXT("metadata.isApproved"),
			TEXT("VOID.Import.NotApproved"), TEXT("Have design leads approve the source document and re-export with isApproved: true."));
	}
}

void FVoidPackageValidator::ValidateDistrict(const FVoidDesignPackage& Package, FVoidValidationReport& Report)
{
	const FVoidDistrictData& District = Package.District;

	if (!District.DistrictId.IsValid())
	{
		Report.AddError(TEXT("Package is missing a districtId."), TEXT("district.districtId"),
			TEXT("VOID.Import.MissingField"), TEXT("Add a non-empty 'districtId' to the package's district object."));
	}

	ValidateBuildings(District, Report);
	ValidateRoads(District, Report);
	ValidateIdUniqueness(District, Report);
}

void FVoidPackageValidator::ValidateBuildings(const FVoidDistrictData& District, FVoidValidationReport& Report)
{
	for (int32 Index = 0; Index < District.Buildings.Num(); ++Index)
	{
		const FVoidBuildingSpec& Building = District.Buildings[Index];
		const FString FieldPrefix = FString::Printf(TEXT("district.buildings[%d]"), Index);

		if (!Building.Id.IsValid())
		{
			Report.AddError(TEXT("Building is missing an id."), FieldPrefix + TEXT(".id"),
				TEXT("VOID.Import.MissingField"), TEXT("Add a non-empty 'id' string."));
		}

		if (Building.FootprintCorners.Num() < 3)
		{
			Report.AddError(TEXT("Building footprint must have at least 3 corners."), FieldPrefix + TEXT(".footprintCorners"),
				TEXT("VOID.Import.InvalidGeometry"), TEXT("Add at least 3 [x, y] corner pairs, wound consistently (CCW)."));
		}

		if (Building.HeightUnits <= 0.0f)
		{
			Report.AddError(TEXT("Building height must be greater than zero."), FieldPrefix + TEXT(".heightUnits"),
				TEXT("VOID.Import.InvalidGeometry"), TEXT("Set 'heightUnits' to a positive number."));
		}

		if (Building.BuildingType.IsEmpty())
		{
			Report.AddWarning(TEXT("Building has no buildingType tag; will generate as an untyped placeholder."), FieldPrefix + TEXT(".buildingType"),
				TEXT("VOID.Import.MissingField"), TEXT("Add a 'buildingType' tag matching the current design vocabulary."));
		}
	}
}

void FVoidPackageValidator::ValidateRoads(const FVoidDistrictData& District, FVoidValidationReport& Report)
{
	for (int32 Index = 0; Index < District.Roads.Num(); ++Index)
	{
		const FVoidRoadSpec& Road = District.Roads[Index];
		const FString FieldPrefix = FString::Printf(TEXT("district.roads[%d]"), Index);

		if (!Road.Id.IsValid())
		{
			Report.AddError(TEXT("Road is missing an id."), FieldPrefix + TEXT(".id"),
				TEXT("VOID.Import.MissingField"), TEXT("Add a non-empty 'id' string."));
		}

		if (Road.CenterlinePoints.Num() < 2)
		{
			Report.AddError(TEXT("Road centerline must have at least 2 points."), FieldPrefix + TEXT(".centerlinePoints"),
				TEXT("VOID.Import.InvalidGeometry"), TEXT("Add at least 2 [x, y] centerline points."));
		}

		// Coordinate validation (Agent 1): every point must convert through the single FVoidWorldSpace
		// without being non-finite or beyond the configured coordinate limit. Data is reported, never altered.
		{
			const FVoidWorldSpace WorldSpace = FVoidWorldSpace::FromSettings();
			for (int32 PointIndex = 0; PointIndex < Road.CenterlinePoints.Num(); ++PointIndex)
			{
				FVector Converted; FString Err;
				if (!WorldSpace.TryFromSource2D(Road.CenterlinePoints[PointIndex], Road.ElevationUnits, Converted, &Err))
				{
					Report.AddError(FString::Printf(TEXT("Road centerline point %d is not usable: %s"), PointIndex, *Err),
						FString::Printf(TEXT("%s.centerlinePoints[%d]"), *FieldPrefix, PointIndex),
						TEXT("VOID.Import.InvalidCoordinate"), TEXT("Use finite coordinates within the World Space limit (Project Settings > VOID World Builder)."));
					break; // One report per road is enough.
				}
			}
			if (!FMath::IsFinite(Road.WidthUnits) || !FMath::IsFinite(Road.ElevationUnits))
			{
				Report.AddError(TEXT("Road width/elevation is not a finite number."), FieldPrefix + TEXT(".widthUnits"),
					TEXT("VOID.Import.InvalidCoordinate"), TEXT("Use finite numbers."));
			}
		}

		if (Road.WidthUnits <= 0.0f)
		{
			Report.AddError(TEXT("Road width must be greater than zero."), FieldPrefix + TEXT(".widthUnits"),
				TEXT("VOID.Import.InvalidGeometry"), TEXT("Set 'widthUnits' to a positive number."));
		}
	}
}

void FVoidPackageValidator::ValidateIdUniqueness(const FVoidDistrictData& District, FVoidValidationReport& Report)
{
	TSet<FName> SeenIds;
	if (District.DistrictId.IsValid())
	{
		SeenIds.Add(District.DistrictId.Value);
	}

	auto CheckId = [&Report, &SeenIds](const FVoidElementId& Id, const FString& Context)
	{
		if (!Id.IsValid())
		{
			return; // Already reported as missing by the per-type validator above.
		}

		if (SeenIds.Contains(Id.Value))
		{
			Report.AddError(
				FString::Printf(TEXT("Duplicate or reserved element id '%s'."), *Id.Value.ToString()),
				Context, TEXT("VOID.Import.DuplicateId"),
				TEXT("Give this element a unique id that doesn't collide with the district id or any other building/road id."));
		}
		else
		{
			SeenIds.Add(Id.Value);
		}
	};

	for (int32 Index = 0; Index < District.Buildings.Num(); ++Index)
	{
		CheckId(District.Buildings[Index].Id, FString::Printf(TEXT("district.buildings[%d].id"), Index));
	}

	for (int32 Index = 0; Index < District.Roads.Num(); ++Index)
	{
		CheckId(District.Roads[Index].Id, FString::Printf(TEXT("district.roads[%d].id"), Index));
	}
}
