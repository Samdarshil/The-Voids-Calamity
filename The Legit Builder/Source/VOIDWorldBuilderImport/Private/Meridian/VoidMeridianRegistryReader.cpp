// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Meridian/VoidMeridianRegistryReader.h"
#include "VoidWorldBuilderImportLog.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

namespace VoidMeridianReaderPrivate
{
	static bool ParseRoot(const FString& Json, TSharedPtr<FJsonObject>& OutRoot, const TCHAR* What, FVoidValidationReport& Report)
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, OutRoot) || !OutRoot.IsValid())
		{
			Report.AddFatal(
				FString::Printf(TEXT("%s is not valid JSON."), What),
				What,
				TEXT("VOID.Meridian.MalformedJson"),
				TEXT("Fix the JSON syntax error; Meridian files are locked and should be re-exported, not hand-edited."));
			return false;
		}
		return true;
	}

	static TArray<FString> ReadStringArray(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
	{
		TArray<FString> Result;
		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		if (Object.IsValid() && Object->TryGetArrayField(Field, Array) && Array)
		{
			for (const TSharedPtr<FJsonValue>& Value : *Array)
			{
				FString Text;
				if (Value.IsValid() && Value->TryGetString(Text) && !Text.IsEmpty())
				{
					Result.Add(Text);
				}
			}
		}
		return Result;
	}

	static TArray<FName> ReadNameArray(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
	{
		TArray<FName> Result;
		for (const FString& Text : ReadStringArray(Object, Field))
		{
			Result.Add(FName(*Text));
		}
		return Result;
	}

	static FString ReadString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
	{
		FString Value;
		if (Object.IsValid())
		{
			Object->TryGetStringField(Field, Value);
		}
		return Value;
	}

	static FName ReadName(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
	{
		const FString Value = ReadString(Object, Field);
		return Value.IsEmpty() ? FName(NAME_None) : FName(*Value);
	}

	static const TSharedPtr<FJsonObject> ReadObject(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
	{
		const TSharedPtr<FJsonObject>* Child = nullptr;
		if (Object.IsValid() && Object->TryGetObjectField(Field, Child) && Child)
		{
			return *Child;
		}
		return nullptr;
	}

	/** Reads [[a,b],[c,d]] string pair lists. */
	static void ReadPairList(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, TArray<TPair<FName, FName>>& OutPairs)
	{
		const TArray<TSharedPtr<FJsonValue>>* Outer = nullptr;
		if (!Object.IsValid() || !Object->TryGetArrayField(Field, Outer) || !Outer)
		{
			return;
		}

		for (const TSharedPtr<FJsonValue>& PairValue : *Outer)
		{
			const TArray<TSharedPtr<FJsonValue>>* Inner = nullptr;
			if (!PairValue.IsValid() || !PairValue->TryGetArray(Inner) || !Inner || Inner->Num() < 2)
			{
				continue;
			}

			FString A;
			FString B;
			if ((*Inner)[0]->TryGetString(A) && (*Inner)[1]->TryGetString(B))
			{
				OutPairs.Add(TPair<FName, FName>(FName(*A), FName(*B)));
			}
		}
	}

	static bool ReadJsonFile(const FString& Path, FString& OutText)
	{
		return FFileHelper::LoadFileToString(OutText, *Path);
	}

	static void AppendRoutes(const TSharedPtr<FJsonObject>& Root, const TCHAR* ArrayField, const TMap<FName, FString>& CategoryNetwork, const TMap<FName, int32>& CategoryTier, FVoidMeridianDataSet& Data)
	{
		const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
		if (!Root->TryGetArrayField(ArrayField, Array) || !Array)
		{
			return;
		}

		for (const TSharedPtr<FJsonValue>& Value : *Array)
		{
			const TSharedPtr<FJsonObject>* RouteObject = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(RouteObject) || !RouteObject)
			{
				continue;
			}

			FVoidMeridianRoute Route;
			Route.Id = ReadName(*RouteObject, TEXT("id"));
			if (Route.Id.IsNone())
			{
				continue;
			}

			Route.Category = ReadString(*RouteObject, TEXT("category"));
			Route.OriginDistrict = ReadName(*RouteObject, TEXT("origin_district"));
			Route.OriginSubLocation = ReadName(*RouteObject, TEXT("origin_sub_location"));
			Route.OwningDistrict = ReadName(*RouteObject, TEXT("owning_district"));
			Route.RadialBand = ReadString(*RouteObject, TEXT("radial_band"));
			Route.HardConstraint = ReadString(*RouteObject, TEXT("hard_constraint"));
			Route.TraversesBands = ReadStringArray(*RouteObject, TEXT("traverses_bands"));
			Route.ServesDistricts = ReadNameArray(*RouteObject, TEXT("serves_districts"));
			Route.SharedByDistricts = ReadNameArray(*RouteObject, TEXT("shared_by_districts"));
			Route.SubBeltsServed = ReadNameArray(*RouteObject, TEXT("sub_belts_served"));
			Route.GenerationDependency = ReadName(*RouteObject, TEXT("generation_dependency"));

			if (const FString* Network = CategoryNetwork.Find(FName(*Route.Category)))
			{
				Route.Network = *Network;
			}
			if (const int32* Tier = CategoryTier.Find(FName(*Route.Category)))
			{
				Route.HierarchyTier = *Tier;
			}

			Data.Routes.Add(MoveTemp(Route));
		}
	}
}

bool FVoidMeridianRegistryReader::LoadFromDirectory(const FString& Directory, FVoidMeridianDataSet& OutData, FVoidValidationReport& OutReport)
{
	using namespace VoidMeridianReaderPrivate;

	OutData = FVoidMeridianDataSet();
	OutData.RootDirectory = Directory;
	OutReport.bIsValid = true;

	const FString ManifestPath = FPaths::Combine(Directory, TEXT("Meridian_Master.json"));
	FString ManifestText;
	if (!ReadJsonFile(ManifestPath, ManifestText))
	{
		OutReport.AddFatal(
			FString::Printf(TEXT("Could not read Meridian_Master.json at '%s'."), *ManifestPath),
			TEXT("Meridian_Master.json"),
			TEXT("VOID.Meridian.ManifestMissing"),
			TEXT("Point the District Generator at the folder that contains Meridian_Master.json."));
		return false;
	}

	TArray<FString> RequiredFiles;
	if (!ParseMasterManifest(ManifestText, OutData, RequiredFiles, OutReport))
	{
		return false;
	}

	// BuilderRules missing_reference_error: any required registry_reference that
	// does not resolve HALTs the pipeline at import; no partial manifests.
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	for (const FString& RequiredFile : RequiredFiles)
	{
		if (!PlatformFile.FileExists(*FPaths::Combine(Directory, RequiredFile)))
		{
			OutReport.AddFatal(
				FString::Printf(TEXT("Required registry '%s' referenced by Meridian_Master.json was not found in '%s'."), *RequiredFile, *Directory),
				RequiredFile,
				TEXT("VOID.Meridian.RegistryMissing"),
				TEXT("BuilderRules.json missing_reference_error: generation halts. Restore the file; do not generate from a partial manifest."));
		}
	}
	if (OutReport.HasFatalIssue())
	{
		return false;
	}

	struct FRegistryLoad
	{
		const TCHAR* File;
		bool (*Parse)(const FString&, FVoidMeridianDataSet&, FVoidValidationReport&);
	};

	// Import order follows Meridian_Master.json generation_pipeline.import_order
	// (district_registry first, then road_network, then landmark_registry).
	const FRegistryLoad Loads[] =
	{
		{ TEXT("DistrictRegistry.json"), &FVoidMeridianRegistryReader::ParseDistrictRegistry },
		{ TEXT("RoadNetwork.json"),      &FVoidMeridianRegistryReader::ParseRoadNetwork },
		{ TEXT("LandmarkRegistry.json"), &FVoidMeridianRegistryReader::ParseLandmarkRegistry },
	};

	for (const FRegistryLoad& Load : Loads)
	{
		FString Text;
		if (!ReadJsonFile(FPaths::Combine(Directory, Load.File), Text))
		{
			OutReport.AddFatal(
				FString::Printf(TEXT("Could not read '%s'."), Load.File), Load.File,
				TEXT("VOID.Meridian.RegistryUnreadable"));
			return false;
		}
		if (!Load.Parse(Text, OutData, OutReport))
		{
			return false;
		}
	}

	// Optional per-district data files (authoritative when present).
	for (FVoidMeridianDistrictEntry& District : OutData.Districts)
	{
		if (District.DistrictDataFile.IsEmpty())
		{
			continue;
		}

		FString Text;
		if (!ReadJsonFile(FPaths::Combine(Directory, District.DistrictDataFile), Text))
		{
			OutReport.AddWarning(
				FString::Printf(TEXT("District '%s' data file '%s' was not found next to the registries; generating from registry-level data only."), *District.Id.ToString(), *District.DistrictDataFile),
				District.DistrictDataFile,
				TEXT("VOID.Meridian.DistrictDataMissing"),
				TEXT("Add the approved per-district export next to the registries to enable data-driven character for this district."));
			continue;
		}

		FVoidValidationReport DistrictReport;
		DistrictReport.bIsValid = true;
		ParseDistrictData(Text, OutData, DistrictReport);
		OutReport.Issues.Append(DistrictReport.Issues);
		if (!DistrictReport.bIsValid)
		{
			OutReport.bIsValid = false;
		}
	}

	UE_LOG(LogVoidImport, Log, TEXT("Meridian registries loaded from '%s': %d districts, %d landmarks, %d routes, %d tunnels."),
		*Directory, OutData.Districts.Num(), OutData.Landmarks.Num(), OutData.Routes.Num(), OutData.Tunnels.Num());

	return OutReport.bIsValid;
}

bool FVoidMeridianRegistryReader::ParseMasterManifest(const FString& Json, FVoidMeridianDataSet& InOutData, TArray<FString>& OutRequiredFiles, FVoidValidationReport& OutReport)
{
	using namespace VoidMeridianReaderPrivate;

	TSharedPtr<FJsonObject> Root;
	if (!ParseRoot(Json, Root, TEXT("Meridian_Master.json"), OutReport))
	{
		return false;
	}

	if (const TSharedPtr<FJsonObject> References = ReadObject(Root, TEXT("registry_references")))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : References->Values)
		{
			const TSharedPtr<FJsonObject>* Entry = nullptr;
			if (!Pair.Value.IsValid() || !Pair.Value->TryGetObject(Entry) || !Entry)
			{
				continue;
			}

			const FString File = ReadString(*Entry, TEXT("file"));
			bool bRequired = false;
			(*Entry)->TryGetBoolField(TEXT("required"), bRequired);
			if (bRequired && !File.IsEmpty())
			{
				OutRequiredFiles.Add(File);
			}
		}
	}
	else
	{
		OutReport.AddFatal(TEXT("Meridian_Master.json has no registry_references object."), TEXT("registry_references"), TEXT("VOID.Meridian.ManifestMalformed"));
		return false;
	}

	if (const TSharedPtr<FJsonObject> Pipeline = ReadObject(Root, TEXT("generation_pipeline")))
	{
		const TArray<TSharedPtr<FJsonValue>>* Order = nullptr;
		if (Pipeline->TryGetArrayField(TEXT("generation_order"), Order) && Order)
		{
			TArray<TPair<int32, FName>> Steps;
			for (const TSharedPtr<FJsonValue>& Value : *Order)
			{
				const TSharedPtr<FJsonObject>* Step = nullptr;
				if (Value.IsValid() && Value->TryGetObject(Step) && Step)
				{
					int32 StepIndex = Steps.Num() + 1;
					(*Step)->TryGetNumberField(TEXT("step"), StepIndex);
					const FName Target = ReadName(*Step, TEXT("target"));
					if (!Target.IsNone())
					{
						Steps.Add(TPair<int32, FName>(StepIndex, Target));
					}
				}
			}
			Steps.Sort([](const TPair<int32, FName>& A, const TPair<int32, FName>& B) { return A.Key < B.Key; });
			for (const TPair<int32, FName>& Step : Steps)
			{
				InOutData.MacroGenerationOrder.Add(Step.Value);
			}
		}
	}

	return true;
}

bool FVoidMeridianRegistryReader::ParseDistrictRegistry(const FString& Json, FVoidMeridianDataSet& InOutData, FVoidValidationReport& OutReport)
{
	using namespace VoidMeridianReaderPrivate;

	TSharedPtr<FJsonObject> Root;
	if (!ParseRoot(Json, Root, TEXT("DistrictRegistry.json"), OutReport))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* DistrictsArray = nullptr;
	if (!Root->TryGetArrayField(TEXT("districts"), DistrictsArray) || !DistrictsArray)
	{
		OutReport.AddFatal(TEXT("DistrictRegistry.json has no 'districts' array."), TEXT("districts"), TEXT("VOID.Meridian.RegistryMalformed"));
		return false;
	}

	const TSharedPtr<FJsonObject> Streaming = ReadObject(Root, TEXT("streaming_relationships"));

	for (const TSharedPtr<FJsonValue>& Value : *DistrictsArray)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(Object) || !Object)
		{
			continue;
		}

		FVoidMeridianDistrictEntry Entry;
		Entry.Id = ReadName(*Object, TEXT("id"));
		if (Entry.Id.IsNone())
		{
			OutReport.AddError(TEXT("A district in DistrictRegistry.json has no id."), TEXT("districts[]"), TEXT("VOID.Meridian.DistrictNoId"));
			continue;
		}

		Entry.DisplayName = ReadString(*Object, TEXT("display_name"));
		Entry.Status = ReadString(*Object, TEXT("status"));
		Entry.RadialBand = ReadString(*Object, TEXT("radial_band"));
		Entry.RadialPositionType = ReadString(*Object, TEXT("radial_position_type"));
		Entry.VerticalTier = ReadString(*Object, TEXT("vertical_tier"));
		Entry.StructuralModel = ReadString(*Object, TEXT("structural_model"));
		Entry.DistrictDataFile = ReadString(*Object, TEXT("data_reference"));
		Entry.DependsOn = ReadNameArray(*Object, TEXT("depends_on"));
		(*Object)->TryGetNumberField(TEXT("generation_priority"), Entry.GenerationPriority);
		(*Object)->TryGetNumberField(TEXT("build_order_index"), Entry.BuildOrderIndex);

		if (const TSharedPtr<FJsonObject> StreamingEntry = ReadObject(Streaming, *Entry.Id.ToString()))
		{
			Entry.WorldPartitionRegion = ReadString(StreamingEntry, TEXT("region"));
			Entry.WorldPartitionTemplate = ReadString(StreamingEntry, TEXT("template"));
			Entry.Instancing = ReadString(StreamingEntry, TEXT("instancing"));
		}

		if (InOutData.FindDistrict(Entry.Id))
		{
			OutReport.AddError(FString::Printf(TEXT("Duplicate district id '%s' in DistrictRegistry.json."), *Entry.Id.ToString()), TEXT("districts[]"), TEXT("VOID.Meridian.DuplicateDistrict"));
			continue;
		}

		InOutData.Districts.Add(MoveTemp(Entry));
	}

	if (const TSharedPtr<FJsonObject> Hierarchy = ReadObject(Root, TEXT("hierarchy")))
	{
		InOutData.RadialBandsOrdered = ReadStringArray(Hierarchy, TEXT("radial_bands_ordered"));
	}

	if (const TSharedPtr<FJsonObject> Adjacency = ReadObject(Root, TEXT("adjacency_graph")))
	{
		const TArray<TSharedPtr<FJsonValue>>* Edges = nullptr;
		if (Adjacency->TryGetArrayField(TEXT("edges"), Edges) && Edges)
		{
			for (const TSharedPtr<FJsonValue>& EdgeValue : *Edges)
			{
				const TSharedPtr<FJsonObject>* Edge = nullptr;
				if (EdgeValue.IsValid() && EdgeValue->TryGetObject(Edge) && Edge)
				{
					FVoidMeridianAdjacencyEdge Parsed;
					Parsed.From = ReadName(*Edge, TEXT("from"));
					Parsed.To = ReadName(*Edge, TEXT("to"));
					Parsed.AdjacencyType = ReadString(*Edge, TEXT("adjacency_type"));
					InOutData.AdjacencyEdges.Add(MoveTemp(Parsed));
				}
			}
		}
	}

	if (const TSharedPtr<FJsonObject> Connectivity = ReadObject(Root, TEXT("connectivity_graph")))
	{
		ReadPairList(Connectivity, TEXT("connective_tissue_required_between"), InOutData.ConnectiveTissuePairs);
	}

	// Cross-reference check (BuilderRules cross_file_inconsistency_error): dependency ids must resolve.
	for (const FVoidMeridianDistrictEntry& District : InOutData.Districts)
	{
		for (const FName& Dependency : District.DependsOn)
		{
			if (!InOutData.FindDistrict(Dependency))
			{
				OutReport.AddError(
					FString::Printf(TEXT("District '%s' depends_on unknown district '%s'."), *District.Id.ToString(), *Dependency.ToString()),
					TEXT("districts[].depends_on"), TEXT("VOID.Meridian.UnresolvedDependency"));
			}
		}
	}

	return OutReport.bIsValid;
}

bool FVoidMeridianRegistryReader::ParseLandmarkRegistry(const FString& Json, FVoidMeridianDataSet& InOutData, FVoidValidationReport& OutReport)
{
	using namespace VoidMeridianReaderPrivate;

	TSharedPtr<FJsonObject> Root;
	if (!ParseRoot(Json, Root, TEXT("LandmarkRegistry.json"), OutReport))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* LandmarksArray = nullptr;
	if (!Root->TryGetArrayField(TEXT("landmarks"), LandmarksArray) || !LandmarksArray)
	{
		OutReport.AddFatal(TEXT("LandmarkRegistry.json has no 'landmarks' array."), TEXT("landmarks"), TEXT("VOID.Meridian.RegistryMalformed"));
		return false;
	}

	for (const TSharedPtr<FJsonValue>& Value : *LandmarksArray)
	{
		const TSharedPtr<FJsonObject>* Object = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(Object) || !Object)
		{
			continue;
		}

		FVoidMeridianLandmarkEntry Entry;
		Entry.Id = ReadName(*Object, TEXT("id"));
		Entry.DistrictId = ReadName(*Object, TEXT("district"));
		if (Entry.Id.IsNone() || Entry.DistrictId.IsNone())
		{
			OutReport.AddError(TEXT("A landmark in LandmarkRegistry.json is missing id or district."), TEXT("landmarks[]"), TEXT("VOID.Meridian.LandmarkMalformed"));
			continue;
		}

		Entry.SubLocationRef = ReadName(*Object, TEXT("sub_location_ref"));
		Entry.Type = ReadString(*Object, TEXT("type"));
		Entry.CanonStatus = ReadString(*Object, TEXT("canon_status"));
		Entry.RecognitionPriority = ReadString(*Object, TEXT("recognition_priority"));
		Entry.GameplayImportance = ReadString(*Object, TEXT("gameplay_importance"));
		Entry.NavigationImportance = ReadString(*Object, TEXT("navigation_importance"));
		Entry.InstancingPolicy = ReadString(*Object, TEXT("instancing_policy"));

		// visibility_tier is a number for skyline landmarks and a text label otherwise.
		const TSharedPtr<FJsonValue> Tier = (*Object)->TryGetField(TEXT("visibility_tier"));
		if (Tier.IsValid())
		{
			double TierNumber = 0.0;
			FString TierText;
			if (Tier->TryGetNumber(TierNumber))
			{
				Entry.VisibilityTier = static_cast<int32>(TierNumber);
			}
			else if (Tier->TryGetString(TierText))
			{
				Entry.VisibilityTierLabel = TierText;
			}
		}

		if (const TSharedPtr<FJsonObject> Sightline = ReadObject(*Object, TEXT("sightline_rule")))
		{
			Entry.SightlineVisibleFrom = ReadNameArray(Sightline, TEXT("visible_from"));
			Entry.SightlineExcluded = ReadNameArray(Sightline, TEXT("excluded"));
		}

		if (InOutData.Landmarks.ContainsByPredicate([&Entry](const FVoidMeridianLandmarkEntry& Existing) { return Existing.Id == Entry.Id; }))
		{
			OutReport.AddError(FString::Printf(TEXT("Duplicate landmark id '%s'."), *Entry.Id.ToString()), TEXT("landmarks[]"), TEXT("VOID.Meridian.DuplicateLandmark"));
			continue;
		}

		if (InOutData.FindDistrict(Entry.DistrictId) == nullptr)
		{
			OutReport.AddError(
				FString::Printf(TEXT("Landmark '%s' belongs to unknown district '%s' (BuilderRules: no sixth district)."), *Entry.Id.ToString(), *Entry.DistrictId.ToString()),
				TEXT("landmarks[].district"), TEXT("VOID.Meridian.LandmarkUnknownDistrict"));
			continue;
		}

		InOutData.Landmarks.Add(MoveTemp(Entry));
	}

	return OutReport.bIsValid;
}

bool FVoidMeridianRegistryReader::ParseRoadNetwork(const FString& Json, FVoidMeridianDataSet& InOutData, FVoidValidationReport& OutReport)
{
	using namespace VoidMeridianReaderPrivate;

	TSharedPtr<FJsonObject> Root;
	if (!ParseRoot(Json, Root, TEXT("RoadNetwork.json"), OutReport))
	{
		return false;
	}

	TMap<FName, FString> CategoryNetwork;
	TMap<FName, int32> CategoryTier;

	const TArray<TSharedPtr<FJsonValue>>* Categories = nullptr;
	if (Root->TryGetArrayField(TEXT("road_categories"), Categories) && Categories)
	{
		for (const TSharedPtr<FJsonValue>& Value : *Categories)
		{
			const TSharedPtr<FJsonObject>* Category = nullptr;
			if (Value.IsValid() && Value->TryGetObject(Category) && Category)
			{
				const FName Id = ReadName(*Category, TEXT("id"));
				if (!Id.IsNone())
				{
					CategoryNetwork.Add(Id, ReadString(*Category, TEXT("network")));
					int32 Tier = INDEX_NONE;
					(*Category)->TryGetNumberField(TEXT("hierarchy_tier"), Tier);
					CategoryTier.Add(Id, Tier);
				}
			}
		}
	}

	AppendRoutes(Root, TEXT("primary_routes"), CategoryNetwork, CategoryTier, InOutData);
	AppendRoutes(Root, TEXT("secondary_routes"), CategoryNetwork, CategoryTier, InOutData);
	AppendRoutes(Root, TEXT("service_routes"), CategoryNetwork, CategoryTier, InOutData);

	const TArray<TSharedPtr<FJsonValue>>* Tunnels = nullptr;
	if (Root->TryGetArrayField(TEXT("tunnel_relationships"), Tunnels) && Tunnels)
	{
		for (const TSharedPtr<FJsonValue>& Value : *Tunnels)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (Value.IsValid() && Value->TryGetObject(Object) && Object)
			{
				FVoidMeridianTunnel Tunnel;
				Tunnel.Id = ReadName(*Object, TEXT("id"));
				Tunnel.Type = ReadString(*Object, TEXT("type"));
				Tunnel.Connects = ReadNameArray(*Object, TEXT("connects"));
				Tunnel.Gate = ReadString(*Object, TEXT("gate"));
				if (!Tunnel.Id.IsNone())
				{
					InOutData.Tunnels.Add(MoveTemp(Tunnel));
				}
			}
		}
	}

	if (const TSharedPtr<FJsonObject> Traversal = ReadObject(Root, TEXT("traversal_graph")))
	{
		ReadPairList(Traversal, TEXT("unreachable_pairs_by_design"), InOutData.UnreachablePairs);
	}

	// Cross-reference: every district id named by a route must exist (no sixth district).
	for (const FVoidMeridianRoute& Route : InOutData.Routes)
	{
		TArray<FName> Referenced;
		Referenced.Append(Route.ServesDistricts);
		Referenced.Append(Route.SharedByDistricts);
		if (!Route.OriginDistrict.IsNone()) { Referenced.Add(Route.OriginDistrict); }
		if (!Route.OwningDistrict.IsNone()) { Referenced.Add(Route.OwningDistrict); }

		for (const FName& DistrictId : Referenced)
		{
			if (InOutData.FindDistrict(DistrictId) == nullptr)
			{
				OutReport.AddError(
					FString::Printf(TEXT("Route '%s' references unknown district '%s'."), *Route.Id.ToString(), *DistrictId.ToString()),
					TEXT("routes[]"), TEXT("VOID.Meridian.RouteUnknownDistrict"));
			}
		}
	}

	return OutReport.bIsValid;
}

bool FVoidMeridianRegistryReader::ParseDistrictData(const FString& Json, FVoidMeridianDataSet& InOutData, FVoidValidationReport& OutReport)
{
	using namespace VoidMeridianReaderPrivate;

	TSharedPtr<FJsonObject> Root;
	if (!ParseRoot(Json, Root, TEXT("<district>_data.json"), OutReport))
	{
		return false;
	}

	const FName LocationId = ReadName(Root, TEXT("location_id"));
	FVoidMeridianDistrictEntry* Entry = InOutData.Districts.FindByPredicate([LocationId](const FVoidMeridianDistrictEntry& E) { return E.Id == LocationId; });
	if (!Entry)
	{
		OutReport.AddError(
			FString::Printf(TEXT("District data file names location_id '%s', which is not in DistrictRegistry.json."), *LocationId.ToString()),
			TEXT("location_id"), TEXT("VOID.Meridian.DistrictDataUnknownLocation"));
		return false;
	}

	Entry->bHasDistrictData = true;

	if (const TSharedPtr<FJsonObject> Position = ReadObject(Root, TEXT("structural_position")))
	{
		Entry->VerticalStratum = ReadString(Position, TEXT("vertical_stratum"));
	}

	if (const TSharedPtr<FJsonObject> Economic = ReadObject(Root, TEXT("economic_band")))
	{
		bool bValue = false;
		if (Economic->TryGetBoolField(TEXT("commercial_presence"), bValue))
		{
			Entry->bCommercialPresence = bValue;
		}
		if (Economic->TryGetBoolField(TEXT("corporate_presence"), bValue))
		{
			Entry->bCorporatePresence = bValue;
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* SubLocations = nullptr;
	if (Root->TryGetArrayField(TEXT("sub_locations"), SubLocations) && SubLocations)
	{
		for (const TSharedPtr<FJsonValue>& Value : *SubLocations)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (Value.IsValid() && Value->TryGetObject(Object) && Object)
			{
				const FName Id = ReadName(*Object, TEXT("id"));
				if (!Id.IsNone())
				{
					Entry->SubLocationIds.Add(Id);
				}
			}
		}
	}

	return OutReport.bIsValid;
}

bool FVoidMeridianRegistryReader::LoadOverridesFromFile(const FString& FilePath, FVoidMeridianLayoutOverrides& OutOverrides, FVoidValidationReport& OutReport, bool bRequired)
{
	OutOverrides = FVoidMeridianLayoutOverrides();

	if (FilePath.IsEmpty())
	{
		return true;
	}

	FString Text;
	if (!VoidMeridianReaderPrivate::ReadJsonFile(FilePath, Text))
	{
		if (bRequired)
		{
			OutReport.AddError(FString::Printf(TEXT("Layout overrides file '%s' could not be read."), *FilePath), TEXT("overrides"), TEXT("VOID.Meridian.OverridesUnreadable"));
			return false;
		}
		return true;
	}

	return ParseLayoutOverrides(Text, OutOverrides, OutReport);
}

bool FVoidMeridianRegistryReader::ParseLayoutOverrides(const FString& Json, FVoidMeridianLayoutOverrides& OutOverrides, FVoidValidationReport& OutReport)
{
	using namespace VoidMeridianReaderPrivate;

	OutOverrides = FVoidMeridianLayoutOverrides();

	TSharedPtr<FJsonObject> Root;
	if (!ParseRoot(Json, Root, TEXT("VoidDistrictLayoutOverrides.json"), OutReport))
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Districts = nullptr;
	if (Root->TryGetArrayField(TEXT("districts"), Districts) && Districts)
	{
		for (int32 DistrictIndex = 0; DistrictIndex < Districts->Num(); ++DistrictIndex)
		{
			const TSharedPtr<FJsonObject>* Object = nullptr;
			if (!(*Districts)[DistrictIndex].IsValid() || !(*Districts)[DistrictIndex]->TryGetObject(Object) || !Object)
			{
				continue;
			}

			FVoidMeridianBoundaryOverride Boundary;
			Boundary.DistrictId = ReadName(*Object, TEXT("id"));

			const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
			if (!Boundary.DistrictId.IsNone() && (*Object)->TryGetArrayField(TEXT("boundary"), Points) && Points)
			{
				for (const TSharedPtr<FJsonValue>& PointValue : *Points)
				{
					const TArray<TSharedPtr<FJsonValue>>* Pair = nullptr;
					if (PointValue.IsValid() && PointValue->TryGetArray(Pair) && Pair && Pair->Num() >= 2)
					{
						Boundary.Polygon.Add(FVector2D((*Pair)[0]->AsNumber(), (*Pair)[1]->AsNumber()));
					}
				}

				if (Boundary.Polygon.Num() < 3)
				{
					OutReport.AddError(
						FString::Printf(TEXT("Boundary override for '%s' needs at least 3 points."), *Boundary.DistrictId.ToString()),
						FString::Printf(TEXT("districts[%d].boundary"), DistrictIndex), TEXT("VOID.Meridian.OverrideBoundaryTooSmall"));
				}
				else
				{
					OutOverrides.Boundaries.Add(MoveTemp(Boundary));
				}
			}

			const TArray<TSharedPtr<FJsonValue>>* Landmarks = nullptr;
			if ((*Object)->TryGetArrayField(TEXT("landmarks"), Landmarks) && Landmarks)
			{
				for (const TSharedPtr<FJsonValue>& LandmarkValue : *Landmarks)
				{
					const TSharedPtr<FJsonObject>* LandmarkObject = nullptr;
					if (!LandmarkValue.IsValid() || !LandmarkValue->TryGetObject(LandmarkObject) || !LandmarkObject)
					{
						continue;
					}

					FVoidMeridianLandmarkOverride Landmark;
					Landmark.LandmarkId = ReadName(*LandmarkObject, TEXT("id"));

					const TArray<TSharedPtr<FJsonValue>>* Position = nullptr;
					if (!Landmark.LandmarkId.IsNone() && (*LandmarkObject)->TryGetArrayField(TEXT("position"), Position) && Position && Position->Num() >= 2)
					{
						const double Z = Position->Num() >= 3 ? (*Position)[2]->AsNumber() : 0.0;
						Landmark.Location = FVector((*Position)[0]->AsNumber(), (*Position)[1]->AsNumber(), Z);
						(*LandmarkObject)->TryGetNumberField(TEXT("yawDegrees"), Landmark.YawDegrees);
						OutOverrides.Landmarks.Add(MoveTemp(Landmark));
					}
				}
			}
		}
	}

	return OutReport.bIsValid;
}
