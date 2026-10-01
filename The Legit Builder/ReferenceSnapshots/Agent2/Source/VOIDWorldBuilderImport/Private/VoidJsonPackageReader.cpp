// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VoidJsonPackageReader.h"
#include "VoidJsonReader.h"
#include "VoidWorldBuilderImportLog.h"
#include "VoidMetroNetworkMapper.h" // Agent 2: metro block mapping
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace VoidJsonPackageReaderPrivate
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

	/** Case-insensitive match against EVoidRoadType's display names. Returns false (leaving OutType untouched) for anything unrecognized -- callers fall back to EVoidRoadType::Local with a Warning, never a hard failure. */
	static bool TryParseRoadType(const FString& InString, EVoidRoadType& OutType)
	{
		static const TMap<FString, EVoidRoadType> Lookup = {
			{ TEXT("Highway"),    EVoidRoadType::Highway },
			{ TEXT("Primary"),    EVoidRoadType::Primary },
			{ TEXT("Secondary"),  EVoidRoadType::Secondary },
			{ TEXT("Local"),      EVoidRoadType::Local },
			{ TEXT("Service"),    EVoidRoadType::Service },
			{ TEXT("Alley"),      EVoidRoadType::Alley },
			{ TEXT("Roundabout"), EVoidRoadType::Roundabout },
		};

		for (const TPair<FString, EVoidRoadType>& Pair : Lookup)
		{
			if (Pair.Key.Equals(InString, ESearchCase::IgnoreCase))
			{
				OutType = Pair.Value;
				return true;
			}
		}
		return false;
	}

	/**
	 * Reports any field on Object not present in KnownFields as an
	 * unknown-field issue, at Error severity if bFailOnUnknownFields is
	 * set (Info otherwise -- the tolerant default, since an unknown field
	 * is often just a newer-schema-minor field this build doesn't know
	 * about yet).
	 *
	 * Any key starting with '_' is treated as an intentional annotation
	 * (VOID World Builder's documented convention for JSON "comments",
	 * since standard JSON has no native comment syntax and this plugin
	 * will not attempt to strip // or /* style comments with a regex --
	 * doing so risks corrupting any string field that legitimately
	 * contains those characters) and is silently skipped, never reported.
	 */
	static void ReportUnknownFields(
		const TSharedPtr<FJsonObject>& Object,
		const TArray<FString>& KnownFields,
		const FString& ObjectContext,
		const FVoidImportContext& Context,
		FVoidValidationReport& OutReport)
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
		{
			if (Pair.Key.StartsWith(TEXT("_")))
			{
				continue; // Documented "comment" convention -- always ignored, never reported.
			}

			if (KnownFields.Contains(Pair.Key))
			{
				continue;
			}

			const FString Message = FString::Printf(TEXT("Unknown field '%s' in %s (ignored)."), *Pair.Key, *ObjectContext);
			const FString FieldPath = FString::Printf(TEXT("%s.%s"), *ObjectContext, *Pair.Key);

			if (Context.bFailOnUnknownFields)
			{
				OutReport.AddError(Message, FieldPath, TEXT("VOID.Import.UnknownField"),
					TEXT("Remove the field, or update the importer's known-field list if it's a new, intentional field."));
			}
			else
			{
				OutReport.AddInfo(Message, FieldPath, TEXT("VOID.Import.UnknownField"),
					TEXT("No action needed unless this was meant to be recognized -- check for a typo, or enable 'Fail On Unknown Fields' in Project Settings to treat this as blocking."));
			}
		}
	}
}

FName FVoidJsonPackageReader::GetSupportedExtension() const
{
	return FName(TEXT("json"));
}

bool FVoidJsonPackageReader::TryRead(const FString& FilePath, FVoidImportContext& Context, FVoidDesignPackage& OutPackage, FVoidValidationReport& OutReport) const
{
	TSharedPtr<FJsonObject> JsonObject;
	FString ReadError;
	if (!FVoidJsonReader::ReadFromFile(FilePath, JsonObject, ReadError))
	{
		OutReport.AddFatal(ReadError, TEXT("file"), TEXT("VOID.Import.UnreadableFile"),
			TEXT("Check the file path is correct and the file contains valid JSON."));
		UE_LOG(LogVoidImport, Error, TEXT("Failed to read '%s': %s"), *FilePath, *ReadError);
		return false;
	}

	MapJsonObjectToPackage(JsonObject, Context, OutPackage, OutReport);
	return true;
}

bool FVoidJsonPackageReader::TryReadFromString(const FString& RawJson, FVoidImportContext& Context, FVoidDesignPackage& OutPackage, FVoidValidationReport& OutReport) const
{
	TSharedPtr<FJsonObject> JsonObject;
	FString ReadError;
	if (!FVoidJsonReader::ParseString(RawJson, JsonObject, ReadError))
	{
		OutReport.AddFatal(ReadError, TEXT("root"), TEXT("VOID.Import.MalformedJson"),
			TEXT("Check the JSON text is well-formed (matching braces/brackets, quoted keys, no trailing commas)."));
		return false;
	}

	MapJsonObjectToPackage(JsonObject, Context, OutPackage, OutReport);
	return true;
}

void FVoidJsonPackageReader::MapJsonObjectToPackage(const TSharedPtr<FJsonObject>& JsonObject, FVoidImportContext& Context, FVoidDesignPackage& OutPackage, FVoidValidationReport& OutReport) const
{
	using namespace VoidJsonPackageReaderPrivate;

	if (!JsonObject.IsValid())
	{
		OutReport.AddFatal(TEXT("Root JSON object was invalid."), TEXT("root"), TEXT("VOID.Import.MalformedJson"),
			TEXT("Check the JSON text is well-formed."));
		return;
	}

	static const TArray<FString> RootKnownFields = { TEXT("schemaVersion"), TEXT("metadata"), TEXT("district"), TEXT("metro") };
	ReportUnknownFields(JsonObject, RootKnownFields, TEXT("root"), Context, OutReport);

	// --- Schema version -----------------------------------------------
	FString SchemaVersionString;
	if (JsonObject->TryGetStringField(TEXT("schemaVersion"), SchemaVersionString))
	{
		FVoidSchemaVersion Parsed;
		if (FVoidSchemaVersion::TryParse(SchemaVersionString, Parsed))
		{
			OutPackage.SchemaVersion = Parsed;
		}
		else
		{
			OutReport.AddWarning(
				FString::Printf(TEXT("Could not parse schemaVersion '%s'; assuming %s."), *SchemaVersionString, *FVoidSchemaVersion::CurrentToolVersion().ToString()),
				TEXT("schemaVersion"), TEXT("VOID.Import.UnparseableSchemaVersion"),
				TEXT("Use the \"Major.Minor\" format, e.g. \"1.0\"."));
			OutPackage.SchemaVersion = FVoidSchemaVersion::CurrentToolVersion();
		}
	}
	else
	{
		OutReport.AddWarning(
			FString::Printf(TEXT("Package is missing 'schemaVersion'; assuming %s."), *FVoidSchemaVersion::CurrentToolVersion().ToString()),
			TEXT("schemaVersion"), TEXT("VOID.Import.MissingSchemaVersion"),
			TEXT("Add a \"schemaVersion\" field, e.g. \"1.0\", to make forward compatibility explicit."));
		OutPackage.SchemaVersion = FVoidSchemaVersion::CurrentToolVersion();
	}

	// --- Metadata -------------------------------------------------------
	const TSharedPtr<FJsonObject>* MetadataObject = nullptr;
	if (JsonObject->TryGetObjectField(TEXT("metadata"), MetadataObject))
	{
		static const TArray<FString> MetadataKnownFields = { TEXT("sourceDocumentName"), TEXT("sourceDocumentVersion"), TEXT("isApproved") };
		ReportUnknownFields(*MetadataObject, MetadataKnownFields, TEXT("metadata"), Context, OutReport);

		(*MetadataObject)->TryGetStringField(TEXT("sourceDocumentName"), OutPackage.Metadata.SourceDocumentName);
		(*MetadataObject)->TryGetStringField(TEXT("sourceDocumentVersion"), OutPackage.Metadata.SourceDocumentVersion);

		bool bIsApproved = false;
		if ((*MetadataObject)->TryGetBoolField(TEXT("isApproved"), bIsApproved))
		{
			OutPackage.Metadata.bIsApproved = bIsApproved;
		}
		else
		{
			OutReport.AddInfo(TEXT("Package metadata is missing 'isApproved'; defaulting to not approved."),
				TEXT("metadata.isApproved"), TEXT("VOID.Import.MissingField"));
		}
	}
	else
	{
		OutReport.AddError(TEXT("Package is missing a 'metadata' object."), TEXT("metadata"),
			TEXT("VOID.Import.MissingField"), TEXT("Add a 'metadata' object with sourceDocumentName, sourceDocumentVersion, and isApproved."));
	}

	// --- Metro (Agent 2, optional, additive) ---------------------------
	// Reuses this reader's traversal and report; the block is mapped by
	// FVoidMetroNetworkMapper into OutPackage.Metro. Absent = no effect.
	const TSharedPtr<FJsonObject>* MetroObject = nullptr;
	if (JsonObject->TryGetObjectField(TEXT("metro"), MetroObject))
	{
		FVoidMetroNetworkMapper::MapPackageMetroBlock(*MetroObject, OutPackage.Metro, OutReport);
	}

	// --- District ---------------------------------------------------
	const TSharedPtr<FJsonObject>* DistrictObject = nullptr;
	if (!JsonObject->TryGetObjectField(TEXT("district"), DistrictObject))
	{
		OutReport.AddError(TEXT("Package is missing a 'district' object."), TEXT("district"),
			TEXT("VOID.Import.MissingField"), TEXT("Add a 'district' object with districtId, buildings, and roads."));
		return;
	}

	static const TArray<FString> DistrictKnownFields = { TEXT("districtId"), TEXT("buildings"), TEXT("roads") };
	ReportUnknownFields(*DistrictObject, DistrictKnownFields, TEXT("district"), Context, OutReport);

	FString DistrictIdString;
	if ((*DistrictObject)->TryGetStringField(TEXT("districtId"), DistrictIdString) && !DistrictIdString.IsEmpty())
	{
		OutPackage.District.DistrictId = FVoidElementId(FName(*DistrictIdString));
	}
	else
	{
		OutReport.AddError(TEXT("district is missing a non-empty 'districtId'."), TEXT("district.districtId"),
			TEXT("VOID.Import.MissingField"), TEXT("Add a non-empty 'districtId' string to the district object."));
	}

	static const TArray<FString> BuildingKnownFields = { TEXT("id"), TEXT("heightUnits"), TEXT("buildingType"), TEXT("footprintCorners") };
	static const TArray<FString> RoadKnownFields = {
		TEXT("id"), TEXT("widthUnits"), TEXT("centerlinePoints"),
		TEXT("roadType"), TEXT("laneCount"), TEXT("speedLimitUnits"), TEXT("elevationUnits"),
		TEXT("hasSidewalk"), TEXT("hasMedian"), TEXT("isBridge"), TEXT("isTunnel"),
		TEXT("culDeSacAtEnd"), TEXT("roundaboutRadiusUnits"), TEXT("connectionIds")
	};

	const TArray<TSharedPtr<FJsonValue>>* BuildingsArray = nullptr;
	if ((*DistrictObject)->TryGetArrayField(TEXT("buildings"), BuildingsArray))
	{
		for (int32 Index = 0; Index < BuildingsArray->Num(); ++Index)
		{
			const TSharedPtr<FJsonValue>& BuildingValue = (*BuildingsArray)[Index];
			const FString BuildingContext = FString::Printf(TEXT("district.buildings[%d]"), Index);

			const TSharedPtr<FJsonObject>* BuildingObject = nullptr;
			if (!BuildingValue->TryGetObject(BuildingObject))
			{
				OutReport.AddError(FString::Printf(TEXT("Non-object entry at %s."), *BuildingContext), BuildingContext,
					TEXT("VOID.Import.MalformedElement"), TEXT("Each entry in 'buildings' must be a JSON object."));
				continue;
			}

			ReportUnknownFields(*BuildingObject, BuildingKnownFields, BuildingContext, Context, OutReport);

			FVoidBuildingSpec Building;

			FString BuildingIdString;
			if ((*BuildingObject)->TryGetStringField(TEXT("id"), BuildingIdString) && !BuildingIdString.IsEmpty())
			{
				Building.Id = FVoidElementId(FName(*BuildingIdString));
			}
			else
			{
				OutReport.AddError(FString::Printf(TEXT("%s is missing a non-empty 'id'."), *BuildingContext), BuildingContext + TEXT(".id"),
					TEXT("VOID.Import.MissingField"), TEXT("Add a non-empty 'id' string."));
			}

			double HeightUnitsValue = 0.0;
			if ((*BuildingObject)->TryGetNumberField(TEXT("heightUnits"), HeightUnitsValue))
			{
				Building.HeightUnits = static_cast<float>(HeightUnitsValue);
			}
			else
			{
				OutReport.AddError(FString::Printf(TEXT("%s is missing 'heightUnits'."), *BuildingContext), BuildingContext + TEXT(".heightUnits"),
					TEXT("VOID.Import.MissingField"), TEXT("Add a numeric 'heightUnits' greater than zero."));
			}

			if (!(*BuildingObject)->TryGetStringField(TEXT("buildingType"), Building.BuildingType))
			{
				OutReport.AddInfo(FString::Printf(TEXT("%s has no 'buildingType'; will generate as an untyped placeholder."), *BuildingContext),
					BuildingContext + TEXT(".buildingType"), TEXT("VOID.Import.MissingField"));
			}

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
				OutReport.AddError(FString::Printf(TEXT("%s is missing 'footprintCorners'."), *BuildingContext), BuildingContext + TEXT(".footprintCorners"),
					TEXT("VOID.Import.MissingField"), TEXT("Add a 'footprintCorners' array of at least 3 [x, y] pairs."));
			}

			UE_LOG(LogVoidImport, Verbose, TEXT("Mapped building '%s' (%d corners, height %.1f)."),
				*Building.Id.Value.ToString(), Building.FootprintCorners.Num(), Building.HeightUnits);

			OutPackage.District.Buildings.Add(MoveTemp(Building));
		}
	}
	else
	{
		OutReport.AddInfo(TEXT("district has no 'buildings' array; district will have zero buildings."),
			TEXT("district.buildings"), TEXT("VOID.Import.MissingField"));
	}

	const TArray<TSharedPtr<FJsonValue>>* RoadsArray = nullptr;
	if ((*DistrictObject)->TryGetArrayField(TEXT("roads"), RoadsArray))
	{
		for (int32 Index = 0; Index < RoadsArray->Num(); ++Index)
		{
			const TSharedPtr<FJsonValue>& RoadValue = (*RoadsArray)[Index];
			const FString RoadContext = FString::Printf(TEXT("district.roads[%d]"), Index);

			const TSharedPtr<FJsonObject>* RoadObject = nullptr;
			if (!RoadValue->TryGetObject(RoadObject))
			{
				OutReport.AddError(FString::Printf(TEXT("Non-object entry at %s."), *RoadContext), RoadContext,
					TEXT("VOID.Import.MalformedElement"), TEXT("Each entry in 'roads' must be a JSON object."));
				continue;
			}

			ReportUnknownFields(*RoadObject, RoadKnownFields, RoadContext, Context, OutReport);

			FVoidRoadSpec Road;

			FString RoadIdString;
			if ((*RoadObject)->TryGetStringField(TEXT("id"), RoadIdString) && !RoadIdString.IsEmpty())
			{
				Road.Id = FVoidElementId(FName(*RoadIdString));
			}
			else
			{
				OutReport.AddError(FString::Printf(TEXT("%s is missing a non-empty 'id'."), *RoadContext), RoadContext + TEXT(".id"),
					TEXT("VOID.Import.MissingField"), TEXT("Add a non-empty 'id' string."));
			}

			double WidthUnitsValue = 0.0;
			if ((*RoadObject)->TryGetNumberField(TEXT("widthUnits"), WidthUnitsValue))
			{
				Road.WidthUnits = static_cast<float>(WidthUnitsValue);
			}
			else
			{
				OutReport.AddError(FString::Printf(TEXT("%s is missing 'widthUnits'."), *RoadContext), RoadContext + TEXT(".widthUnits"),
					TEXT("VOID.Import.MissingField"), TEXT("Add a numeric 'widthUnits' greater than zero."));
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
				OutReport.AddError(FString::Printf(TEXT("%s is missing 'centerlinePoints'."), *RoadContext), RoadContext + TEXT(".centerlinePoints"),
					TEXT("VOID.Import.MissingField"), TEXT("Add a 'centerlinePoints' array of at least 2 [x, y] pairs."));
			}

			// --- Phase 3 fields: all optional, all with safe defaults ---
			FString RoadTypeString;
			if ((*RoadObject)->TryGetStringField(TEXT("roadType"), RoadTypeString))
			{
				EVoidRoadType ParsedType;
				if (TryParseRoadType(RoadTypeString, ParsedType))
				{
					Road.RoadType = ParsedType;
				}
				else
				{
					OutReport.AddWarning(
						FString::Printf(TEXT("%s has unrecognized roadType '%s'; defaulting to Local."), *RoadContext, *RoadTypeString),
						RoadContext + TEXT(".roadType"), TEXT("VOID.Import.UnrecognizedRoadType"),
						TEXT("Use one of: Highway, Primary, Secondary, Local, Service, Alley, Roundabout."));
				}
			}
			else
			{
				OutReport.AddInfo(FString::Printf(TEXT("%s has no 'roadType'; defaulting to Local."), *RoadContext),
					RoadContext + TEXT(".roadType"), TEXT("VOID.Import.MissingField"));
			}

			double LaneCountValue = 0.0;
			if ((*RoadObject)->TryGetNumberField(TEXT("laneCount"), LaneCountValue))
			{
				Road.LaneCount = FMath::RoundToInt(LaneCountValue);
			}

			double SpeedLimitValue = 0.0;
			if ((*RoadObject)->TryGetNumberField(TEXT("speedLimitUnits"), SpeedLimitValue))
			{
				Road.SpeedLimitUnits = FMath::RoundToInt(SpeedLimitValue);
			}

			double ElevationValue = 0.0;
			if ((*RoadObject)->TryGetNumberField(TEXT("elevationUnits"), ElevationValue))
			{
				Road.ElevationUnits = static_cast<float>(ElevationValue);
			}

			(*RoadObject)->TryGetBoolField(TEXT("hasSidewalk"), Road.bHasSidewalk);
			(*RoadObject)->TryGetBoolField(TEXT("hasMedian"), Road.bHasMedian);
			(*RoadObject)->TryGetBoolField(TEXT("isBridge"), Road.bIsBridge);
			(*RoadObject)->TryGetBoolField(TEXT("isTunnel"), Road.bIsTunnel);
			(*RoadObject)->TryGetBoolField(TEXT("culDeSacAtEnd"), Road.bCulDeSacAtEnd);

			double RoundaboutRadiusValue = 0.0;
			if ((*RoadObject)->TryGetNumberField(TEXT("roundaboutRadiusUnits"), RoundaboutRadiusValue))
			{
				Road.RoundaboutRadiusUnits = static_cast<float>(RoundaboutRadiusValue);
			}
			else if (Road.RoadType == EVoidRoadType::Roundabout)
			{
				OutReport.AddWarning(FString::Printf(TEXT("%s is a Roundabout but has no 'roundaboutRadiusUnits'; it will not generate."), *RoadContext),
					RoadContext + TEXT(".roundaboutRadiusUnits"), TEXT("VOID.Import.MissingField"),
					TEXT("Add a positive 'roundaboutRadiusUnits' for a Roundabout-type road."));
			}

			const TArray<TSharedPtr<FJsonValue>>* ConnectionIdsArray = nullptr;
			if ((*RoadObject)->TryGetArrayField(TEXT("connectionIds"), ConnectionIdsArray))
			{
				for (const TSharedPtr<FJsonValue>& ConnectionValue : *ConnectionIdsArray)
				{
					FString ConnectionIdString;
					if (ConnectionValue->TryGetString(ConnectionIdString) && !ConnectionIdString.IsEmpty())
					{
						Road.ConnectionIds.Add(FVoidElementId(FName(*ConnectionIdString)));
					}
				}
			}

			UE_LOG(LogVoidImport, Verbose, TEXT("Mapped road '%s' (%d centerline points, width %.1f, type %d)."),
				*Road.Id.Value.ToString(), Road.CenterlinePoints.Num(), Road.WidthUnits, static_cast<int32>(Road.RoadType));

			OutPackage.District.Roads.Add(MoveTemp(Road));
		}
	}
	else
	{
		OutReport.AddInfo(TEXT("district has no 'roads' array; district will have zero roads."),
			TEXT("district.roads"), TEXT("VOID.Import.MissingField"));
	}
}
