// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VoidMeridianImporter.h"
#include "VoidJsonReader.h"
#include "VoidJsonSchemaLite.h"
#include "VoidWorldBuilderImportLog.h"
#include "Settings/VoidWorldBuilderSettings.h"
#include "Utilities/VoidSha256.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTime.h"

namespace VoidMeridianPrivate
{
	using FJsonObj = TSharedPtr<FJsonObject>;

	static const FName CodeMissingFile(TEXT("VOID.Meridian.MissingFile"));
	static const FName CodeMalformed(TEXT("VOID.Meridian.MalformedJson"));
	static const FName CodeSchemaId(TEXT("VOID.Meridian.SchemaIdMismatch"));
	static const FName CodeSchemaViolation(TEXT("VOID.Meridian.SchemaViolation"));
	static const FName CodeBroken(TEXT("VOID.Meridian.BrokenReference"));
	static const FName CodeDuplicate(TEXT("VOID.Meridian.DuplicateId"));
	static const FName CodeOrder(TEXT("VOID.Meridian.ImportOrderViolation"));
	static const FName CodeSpatial(TEXT("VOID.Meridian.FabricatedSpatialData"));
	static const FName CodeChecksum(TEXT("VOID.Meridian.ChecksumMismatch"));
	static const FName CodeLockedMissing(TEXT("VOID.Meridian.LockedInputMissing"));
	static const FName CodeContradiction(TEXT("VOID.Meridian.TraversalContradiction"));

	static FString S(const FJsonObj& O, const TCHAR* Field)
	{
		FString V;
		if (O.IsValid()) { O->TryGetStringField(Field, V); }
		return V;
	}

	/** Absent/null/empty string fields map to NAME_None explicitly (an FName built from "" is not guaranteed to be NAME_None). */
	static FName N(const FJsonObj& O, const TCHAR* Field)
	{
		const FString V = S(O, Field);
		return V.IsEmpty() ? NAME_None : FName(*V);
	}

	static TArray<FName> NameArray(const FJsonObj& O, const TCHAR* Field)
	{
		TArray<FName> Out;
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (O.IsValid() && O->TryGetArrayField(Field, Arr))
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				FString Str;
				if (V.IsValid() && V->TryGetString(Str) && !Str.IsEmpty()) { Out.Add(FName(*Str)); }
			}
		}
		return Out;
	}

	static const TArray<TSharedPtr<FJsonValue>>* ArrayField(const FJsonObj& O, const TCHAR* Field)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		return (O.IsValid() && O->TryGetArrayField(Field, Arr)) ? Arr : nullptr;
	}

	static EVoidNetworkKind ParseNetwork(const FString& Str)
	{
		if (Str == TEXT("live_network")) { return EVoidNetworkKind::Live; }
		if (Str == TEXT("dead_network")) { return EVoidNetworkKind::Dead; }
		return EVoidNetworkKind::Unknown;
	}

	/** Recursively flags numeric spatial/unit fields. Meridian's own rule (VR-012 / coordinate_policy) forbids them. */
	static bool IsSpatialKey(const FString& Key)
	{
		static const TArray<FString> Tokens = {
			TEXT("x"), TEXT("y"), TEXT("z"), TEXT("coord"), TEXT("coords"), TEXT("coordinate"), TEXT("coordinates"),
			TEXT("position"), TEXT("transform"), TEXT("location"), TEXT("elevation"), TEXT("altitude"),
			TEXT("latitude"), TEXT("longitude"), TEXT("units"), TEXT("unit"), TEXT("cm"), TEXT("meters"), TEXT("metres"),
			TEXT("km"), TEXT("feet"), TEXT("ft"), TEXT("width"), TEXT("radius")
		};
		TArray<FString> Parts;
		Key.ToLower().ParseIntoArray(Parts, TEXT("_"), true);
		for (const FString& P : Parts) { if (Tokens.Contains(P)) { return true; } }
		return false;
	}

	static void ScanSpatial(const TSharedPtr<FJsonValue>& V, const FString& Path, TArray<FString>& OutHits)
	{
		if (!V.IsValid()) { return; }
		if (V->Type == EJson::Object)
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& P : V->AsObject()->Values)
			{
				const bool bNumericLike = P.Value.IsValid() && (P.Value->Type == EJson::Number || P.Value->Type == EJson::Array);
				if (bNumericLike && IsSpatialKey(P.Key))
				{
					// An array under a spatial key only counts if it actually holds numbers.
					bool bHasNumber = P.Value->Type == EJson::Number;
					if (P.Value->Type == EJson::Array) { for (const TSharedPtr<FJsonValue>& E : P.Value->AsArray()) { if (E.IsValid() && E->Type == EJson::Number) { bHasNumber = true; break; } } }
					if (bHasNumber) { OutHits.Add(Path + TEXT(".") + P.Key); }
				}
				ScanSpatial(P.Value, Path + TEXT(".") + P.Key, OutHits);
			}
		}
		else if (V->Type == EJson::Array)
		{
			const TArray<TSharedPtr<FJsonValue>>& Arr = V->AsArray();
			// A bare 2-3 element all-numeric array is a coordinate tuple.
			if ((Arr.Num() == 2 || Arr.Num() == 3) && Arr.ContainsByPredicate([](const TSharedPtr<FJsonValue>& E) { return E.IsValid() && E->Type == EJson::Number; })
				&& !Arr.ContainsByPredicate([](const TSharedPtr<FJsonValue>& E) { return !E.IsValid() || E->Type != EJson::Number; }))
			{
				OutHits.Add(Path + TEXT(" (numeric tuple)"));
			}
			for (int32 i = 0; i < Arr.Num(); ++i) { ScanSpatial(Arr[i], FString::Printf(TEXT("%s[%d]"), *Path, i), OutHits); }
		}
	}

	static FString RegistryExpectedSchemaId(const FString& Key)
	{
		return FString::Printf(TEXT("void_%s_schema_v1"), *Key);
	}

	struct FLoadedRegistry
	{
		FString Key;
		FString File;
		FJsonObj Json;
	};
}

FVoidMeridianImportOptions FVoidMeridianImportOptions::FromSettings()
{
	FVoidMeridianImportOptions O;
	if (const UVoidWorldBuilderSettings* Settings = UVoidWorldBuilderSettings::Get())
	{
		O.bStrictCanonLock = Settings->bStrictCanonLock;
	}
	return O;
}

int32 FVoidMeridianImporter::SupportedSchemaMajor() { return 1; }

bool FVoidMeridianImporter::TryParseSchemaId(const FString& SchemaId, FString& OutFamily, int32& OutMajor)
{
	int32 Idx = INDEX_NONE;
	if (!SchemaId.FindLastChar(TEXT('_'), Idx) || Idx <= 0 || Idx + 2 >= SchemaId.Len() + 0) { return false; }
	const FString Tail = SchemaId.Mid(Idx + 1);
	if (Tail.Len() < 2 || Tail[0] != TEXT('v')) { return false; }
	const FString Digits = Tail.Mid(1);
	if (!Digits.IsNumeric() || Digits.Contains(TEXT("."))) { return false; }
	OutFamily = SchemaId.Left(Idx);
	OutMajor = FCString::Atoi(*Digits);
	return true;
}

FVoidMeridianImportResult FVoidMeridianImporter::LoadFromPath(const FString& FileOrFolder)
{
	if (FPaths::DirectoryExists(FileOrFolder))
	{
		return LoadFromMasterFile(FPaths::Combine(FileOrFolder, TEXT("Meridian_Master.json")));
	}
	return LoadFromMasterFile(FileOrFolder);
}

FVoidMeridianImportResult FVoidMeridianImporter::LoadFromMasterFile(const FString& MasterJsonPath)
{
	return LoadFromMasterFile(MasterJsonPath, FVoidMeridianImportOptions::FromSettings());
}

FVoidMeridianImportResult FVoidMeridianImporter::LoadFromMasterFile(const FString& MasterJsonPath, const FVoidMeridianImportOptions& Options)
{
	using namespace VoidMeridianPrivate;

	const double Start = FPlatformTime::Seconds();
	FVoidMeridianImportResult Result;
	FVoidValidationReport& Report = Result.ValidationReport;
	Report.bIsValid = true; // Flipped false by any AddError/AddFatal.
	Result.Context.SourceDescription = MasterJsonPath;
	const FString Dir = FPaths::GetPath(MasterJsonPath);
	Result.World.SourceDirectory = Dir;

	auto Finish = [&]() -> FVoidMeridianImportResult
	{
		Result.Context.ElapsedMilliseconds = (FPlatformTime::Seconds() - Start) * 1000.0;
		UE_LOG(LogVoidImport, Log, TEXT("--- Meridian Import Summary: %s ---"), *MasterJsonPath);
		UE_LOG(LogVoidImport, Log, TEXT("Valid: %s   Registries: %d   Info: %d  Warnings: %d  Errors: %d  Fatal: %d   (%.2f ms)"),
			Result.WasSuccessful() ? TEXT("true") : TEXT("false"), Result.LoadedRegistries.Num(),
			Report.NumInfo(), Report.NumWarnings(), Report.NumErrors(), Report.NumFatal(), Result.Context.ElapsedMilliseconds);
		for (const FVoidValidationIssue& I : Report.Issues)
		{
			const TCHAR* Sev = I.Severity == EVoidValidationSeverity::Fatal ? TEXT("FATAL") : I.Severity == EVoidValidationSeverity::Error ? TEXT("ERROR") : I.Severity == EVoidValidationSeverity::Warning ? TEXT("WARN") : TEXT("INFO");
			UE_LOG(LogVoidImport, Log, TEXT("  [%s] (%s) %s -- %s"), Sev, *I.ErrorCode.ToString(), *I.Message, *I.FieldPath);
		}
		return Result;
	};

	// ---- 1. Meridian_Master.json (the single import file) -----------------------------------
	FJsonObj Master;
	{
		FString Err;
		if (!FPaths::FileExists(MasterJsonPath))
		{
			Report.AddFatal(FString::Printf(TEXT("Meridian_Master.json not found at '%s'."), *MasterJsonPath), TEXT("file"), CodeMissingFile,
				TEXT("Pass the path to Meridian_Master.json (or its folder). It is the only file the Builder imports directly."));
			return Finish();
		}
		if (!FVoidJsonReader::ReadFromFile(MasterJsonPath, Master, Err))
		{
			Report.AddFatal(Err, TEXT("Meridian_Master.json"), CodeMalformed, TEXT("Fix the JSON syntax (VR-001)."));
			return Finish();
		}
	}

	FVoidMeridianManifest& Manifest = Result.World.Manifest;
	Manifest.SchemaId = S(Master, TEXT("$schema"));
	{
		FString Family; int32 Major = 0;
		if (!TryParseSchemaId(Manifest.SchemaId, Family, Major) || Family != TEXT("void_meridian_master_schema") || Major != SupportedSchemaMajor())
		{
			Report.AddFatal(FString::Printf(TEXT("Meridian_Master.json declares schema '%s'; this importer supports 'void_meridian_master_schema_v%d'."), *Manifest.SchemaId, SupportedSchemaMajor()),
				TEXT("Meridian_Master.json.$schema"), CodeSchemaId,
				TEXT("Update the plugin to a version supporting this schema, or re-export the package at schema v1."));
			return Finish();
		}
	}

	const FJsonObj* ProjectMeta = nullptr;
	if (Master->TryGetObjectField(TEXT("project_metadata"), ProjectMeta))
	{
		Manifest.PackageVersion = S(*ProjectMeta, TEXT("package_version"));
		Manifest.CanonVersion = S(*ProjectMeta, TEXT("canon_version"));
		Manifest.Status = S(*ProjectMeta, TEXT("status"));
	}

	const FJsonObj* Pipeline = nullptr;
	if (Master->TryGetObjectField(TEXT("generation_pipeline"), Pipeline))
	{
		Manifest.ImportOrder = NameArray(*Pipeline, TEXT("import_order"));
		if (const TArray<TSharedPtr<FJsonValue>>* Steps = ArrayField(*Pipeline, TEXT("generation_order")))
		{
			for (const TSharedPtr<FJsonValue>& V : *Steps)
			{
				const FJsonObj* Step = nullptr;
				if (V.IsValid() && V->TryGetObject(Step)) { Manifest.GenerationOrder.Add(N(*Step, TEXT("target"))); }
			}
		}
	}
	else
	{
		Report.AddFatal(TEXT("Meridian_Master.json has no 'generation_pipeline' object; import order is unknown."), TEXT("generation_pipeline"), CodeMalformed,
			TEXT("Restore generation_pipeline.import_order in Meridian_Master.json."));
		return Finish();
	}

	const FJsonObj* Scale = nullptr;
	if (Master->TryGetObjectField(TEXT("world_scale_metadata"), Scale))
	{
		Manifest.RadialBands = NameArray(*Scale, TEXT("radial_bands"));
		Manifest.VerticalTiers = NameArray(*Scale, TEXT("vertical_tiers"));
	}

	if (const TArray<TSharedPtr<FJsonValue>>* Flags = ArrayField(Master, TEXT("flagged_open_items")))
	{
		for (const TSharedPtr<FJsonValue>& V : *Flags)
		{
			const FJsonObj* Item = nullptr;
			if (V.IsValid() && V->TryGetObject(Item) && S(*Item, TEXT("status")).StartsWith(TEXT("unresolved")))
			{
				Manifest.FlaggedOpenItemIds.Add(N(*Item, TEXT("id")));
			}
		}
		Report.AddInfo(FString::Printf(TEXT("%d unresolved flagged open item(s) carried by the package; content tied to them must not be generated unless explicitly allowed."), Manifest.FlaggedOpenItemIds.Num()),
			TEXT("flagged_open_items"), TEXT("VOID.Meridian.FlaggedOpenItems"));
	}

	// Coordinate policy self-declaration must still say "no fabricated coordinates".
	{
		const FJsonObj* Cfg = nullptr;
		if (Master->TryGetObjectField(TEXT("builder_configuration"), Cfg) && !S(*Cfg, TEXT("coordinate_policy")).Contains(TEXT("no_fabricated_coordinates")))
		{
			Report.AddWarning(TEXT("builder_configuration.coordinate_policy no longer states 'no_fabricated_coordinates'; this importer assumes Meridian carries no spatial data."),
				TEXT("builder_configuration.coordinate_policy"), TEXT("VOID.Meridian.PolicyChanged"),
				TEXT("Review FVoidWorldSpace and FVoidMeridianRoadPlanner: if Meridian now carries coordinates they must be consumed through FVoidWorldSpace."));
		}
	}

	{
		TArray<FString> MasterHits;
		ScanSpatial(MakeShared<FJsonValueObject>(Master), TEXT("Meridian_Master"), MasterHits);
		for (const FString& H : MasterHits)
		{
			Report.AddError(FString::Printf(TEXT("Spatial or unit data found at %s. Meridian forbids coordinates, transforms and unit values (VR-012)."), *H), H, CodeSpatial,
				TEXT("Remove it. Introducing coordinates requires a schema version bump and an update to FVoidWorldSpace first."));
		}
	}

	// ---- 2. Master schema (VR-002) ------------------------------------------------------------
	auto SchemaCheck = [&](const FJsonObj& Doc, const FString& JsonFile, const FString& Label)
	{
		const FString SchemaPath = FPaths::Combine(Dir, FPaths::GetBaseFilename(JsonFile) + TEXT(".schema.json"));
		if (!FPaths::FileExists(SchemaPath))
		{
			Report.AddWarning(FString::Printf(TEXT("No schema file '%s.schema.json' next to the package; structural validation of %s skipped."), *FPaths::GetBaseFilename(JsonFile), *Label),
				Label, TEXT("VOID.Meridian.NoSchemaFile"), TEXT("Ship the *.schema.json files alongside the data."));
			return;
		}
		FJsonObj Schema; FString Err;
		if (!FVoidJsonReader::ReadFromFile(SchemaPath, Schema, Err))
		{
			Report.AddError(FString::Printf(TEXT("Schema '%s' unreadable: %s"), *SchemaPath, *Err), Label, CodeMalformed, TEXT("Fix the schema file's JSON syntax."));
			return;
		}
		TArray<FString> Violations;
		FVoidJsonSchemaLite::Validate(MakeShared<FJsonValueObject>(Doc), Schema, Label, Violations);
		for (const FString& V : Violations)
		{
			Report.AddError(V, Label, CodeSchemaViolation, TEXT("Make the file conform to its schema, or version-bump both together (see CompatibilityMatrix.md Sec. 6)."));
		}
	};
	SchemaCheck(Master, TEXT("Meridian_Master.json"), TEXT("Meridian_Master"));

	// ---- 3. Registries, resolved THROUGH the master, in import_order --------------------------
	const FJsonObj* Refs = nullptr;
	if (!Master->TryGetObjectField(TEXT("registry_references"), Refs))
	{
		Report.AddFatal(TEXT("Meridian_Master.json has no 'registry_references'."), TEXT("registry_references"), CodeMalformed, TEXT("Restore registry_references."));
		return Finish();
	}

	TMap<FString, FJsonObj> Registries;
	for (const FName KeyName : Manifest.ImportOrder)
	{
		const FString Key = KeyName.ToString();
		const FJsonObj* Ref = nullptr;
		if (!(*Refs)->TryGetObjectField(Key, Ref))
		{
			Report.AddFatal(FString::Printf(TEXT("import_order names '%s' but registry_references has no such entry."), *Key), FString::Printf(TEXT("registry_references.%s"), *Key), CodeBroken,
				TEXT("Add the registry to registry_references or remove it from import_order."));
			return Finish();
		}

		const FString File = S(*Ref, TEXT("file"));
		bool bRequired = false;
		(*Ref)->TryGetBoolField(TEXT("required"), bRequired);
		const FString Full = FPaths::Combine(Dir, File);

		FJsonObj Doc; FString Err;
		if (!FPaths::FileExists(Full))
		{
			if (bRequired)
			{
				// BuilderRules.json: a missing required reference HALTS at step 1 - never proceed with a partial manifest.
				Report.AddFatal(FString::Printf(TEXT("Required registry '%s' (%s) is missing."), *Key, *File), FString::Printf(TEXT("registry_references.%s"), *Key), CodeMissingFile,
					TEXT("Restore the file next to Meridian_Master.json. The pipeline halts on a missing required reference."));
				return Finish();
			}
			Report.AddWarning(FString::Printf(TEXT("Optional registry '%s' (%s) is missing."), *Key, *File), FString::Printf(TEXT("registry_references.%s"), *Key), CodeMissingFile);
			continue;
		}
		if (!FVoidJsonReader::ReadFromFile(Full, Doc, Err))
		{
			Report.AddFatal(FString::Printf(TEXT("%s: %s"), *File, *Err), File, CodeMalformed, TEXT("Fix the JSON syntax (VR-001)."));
			return Finish();
		}

		// $schema id check (family + supported major).
		const FString DeclaredSchema = S(Doc, TEXT("$schema"));
		FString Family; int32 Major = 0;
		if (DeclaredSchema != RegistryExpectedSchemaId(Key))
		{
			if (TryParseSchemaId(DeclaredSchema, Family, Major) && Family == FString::Printf(TEXT("void_%s_schema"), *Key) && Major != SupportedSchemaMajor())
			{
				Report.AddFatal(FString::Printf(TEXT("%s declares schema major v%d; only v%d is supported."), *File, Major, SupportedSchemaMajor()), File + TEXT(".$schema"), CodeSchemaId,
					TEXT("Update the plugin, or re-export at schema v1."));
				return Finish();
			}
			Report.AddError(FString::Printf(TEXT("%s declares schema '%s', expected '%s'."), *File, *DeclaredSchema, *RegistryExpectedSchemaId(Key)), File + TEXT(".$schema"), CodeSchemaId,
				TEXT("Correct the $schema field."));
		}

		SchemaCheck(Doc, File, FPaths::GetBaseFilename(File));

		// VR-012: spatial data / units must not exist.
		TArray<FString> Hits;
		ScanSpatial(MakeShared<FJsonValueObject>(Doc), FPaths::GetBaseFilename(File), Hits);
		for (const FString& H : Hits)
		{
			Report.AddError(FString::Printf(TEXT("Spatial or unit data found at %s. Meridian forbids coordinates, transforms and unit values (VR-012)."), *H), H, CodeSpatial,
				TEXT("Remove it. If the design team intends to introduce coordinates, that requires a schema version bump and an update to FVoidWorldSpace first."));
		}

		// Keep raw text for registries not yet typed.
		FString Raw;
		FFileHelper::LoadFileToString(Raw, *Full);
		Result.World.RawRegistries.Add(KeyName, MoveTemp(Raw));

		Registries.Add(Key, Doc);
		Result.LoadedRegistries.Add(KeyName);
	}

	// Every OTHER required json registry that isn't in import_order should still exist.
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Refs)->Values)
	{
		const FJsonObj* Ref = nullptr;
		if (Pair.Value.IsValid() && Pair.Value->TryGetObject(Ref) && !Manifest.ImportOrder.Contains(FName(*Pair.Key)))
		{
			const FString File = S(*Ref, TEXT("file"));
			bool bRequired = false; (*Ref)->TryGetBoolField(TEXT("required"), bRequired);
			if (bRequired && File.EndsWith(TEXT(".json")) && !FPaths::FileExists(FPaths::Combine(Dir, File)))
			{
				Report.AddFatal(FString::Printf(TEXT("Required registry '%s' (%s) is missing."), *Pair.Key, *File), FString::Printf(TEXT("registry_references.%s"), *Pair.Key), CodeMissingFile, TEXT("Restore the file."));
				return Finish();
			}
			else if (bRequired && File.EndsWith(TEXT(".md")) && !FPaths::FileExists(FPaths::Combine(Dir, File)))
			{
				Report.AddWarning(FString::Printf(TEXT("Referenced document '%s' is missing (not needed by the Builder)."), *File), FString::Printf(TEXT("registry_references.%s"), *Pair.Key), CodeMissingFile);
			}
		}
	}

	// ---- 4. import_order must satisfy module_dependencies -------------------------------------
	if (const FJsonObj* ModDeps = nullptr; Pipeline && (*Pipeline)->TryGetObjectField(TEXT("module_dependencies"), ModDeps))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*ModDeps)->Values)
		{
			const int32 Pos = Manifest.ImportOrder.IndexOfByKey(FName(*Pair.Key));
			if (Pos == INDEX_NONE || !Pair.Value.IsValid() || Pair.Value->Type != EJson::Array) { continue; }
			for (const TSharedPtr<FJsonValue>& D : Pair.Value->AsArray())
			{
				FString Dep;
				if (D.IsValid() && D->TryGetString(Dep) && Manifest.ImportOrder.IndexOfByKey(FName(*Dep)) > Pos)
				{
					Report.AddError(FString::Printf(TEXT("import_order loads '%s' before its dependency '%s'."), *Pair.Key, *Dep), TEXT("generation_pipeline.import_order"), CodeOrder,
						TEXT("Reorder import_order so every module follows its dependencies."));
				}
			}
		}
	}

	// ---- 5. District registry ------------------------------------------------------------------
	TSet<FName> DistrictIds;
	if (const FJsonObj* Doc = Registries.Find(TEXT("district_registry")))
	{
		if (const TArray<TSharedPtr<FJsonValue>>* Arr = ArrayField(*Doc, TEXT("districts")))
		{
			for (int32 i = 0; i < Arr->Num(); ++i)
			{
				const FJsonObj* D = nullptr;
				if (!(*Arr)[i].IsValid() || !(*Arr)[i]->TryGetObject(D)) { continue; }
				FVoidMeridianDistrict District;
				District.Id = N(*D, TEXT("id"));
				District.DisplayName = S(*D, TEXT("display_name"));
				District.RadialBand = N(*D, TEXT("radial_band"));
				District.VerticalTier = N(*D, TEXT("vertical_tier"));
				District.DataReference = S(*D, TEXT("data_reference"));
				double Tmp = 0;
				if ((*D)->TryGetNumberField(TEXT("generation_priority"), Tmp)) { District.GenerationPriority = FMath::RoundToInt(Tmp); }
				if ((*D)->TryGetNumberField(TEXT("build_order_index"), Tmp)) { District.BuildOrderIndex = FMath::RoundToInt(Tmp); }
				District.DependsOn = NameArray(*D, TEXT("depends_on"));

				if (DistrictIds.Contains(District.Id))
				{
					Report.AddError(FString::Printf(TEXT("Duplicate district id '%s'."), *District.Id.ToString()), FString::Printf(TEXT("DistrictRegistry.districts[%d].id"), i), CodeDuplicate, TEXT("Give each district a unique id."));
				}
				DistrictIds.Add(District.Id);
				Result.World.Districts.Add(MoveTemp(District));
			}
		}
		// Referential checks within the registry.
		for (const FVoidMeridianDistrict& D : Result.World.Districts)
		{
			for (const FName Dep : D.DependsOn)
			{
				if (!DistrictIds.Contains(Dep)) { Report.AddError(FString::Printf(TEXT("District '%s' depends_on unknown district '%s'."), *D.Id.ToString(), *Dep.ToString()), TEXT("DistrictRegistry.districts"), CodeBroken, TEXT("Fix the id.")); }
			}
			if (!D.RadialBand.IsNone() && !Manifest.RadialBands.Contains(D.RadialBand) && D.RadialBand != FName(TEXT("inner_mid_outer_continuous")))
			{
				// Undercroft's band ("inner_mid_outer_continuous") is a documented composite, not a member of world_scale_metadata.radial_bands.
				Report.AddInfo(FString::Printf(TEXT("District '%s' radial_band '%s' is not in Meridian_Master world_scale_metadata.radial_bands."), *D.Id.ToString(), *D.RadialBand.ToString()), TEXT("DistrictRegistry.districts"), TEXT("VOID.Meridian.BandNotInMaster"));
			}
		}
	}

	// ---- 6. Road network ------------------------------------------------------------------------
	if (const FJsonObj* Doc = Registries.Find(TEXT("road_network")))
	{
		FVoidMeridianRoadNetwork& RN = Result.World.RoadNetwork;
		RN.RoadHierarchy = NameArray(*Doc, TEXT("road_hierarchy"));

		if (const TArray<TSharedPtr<FJsonValue>>* Cats = ArrayField(*Doc, TEXT("road_categories")))
		{
			for (const TSharedPtr<FJsonValue>& V : *Cats)
			{
				const FJsonObj* C = nullptr;
				if (!V.IsValid() || !V->TryGetObject(C)) { continue; }
				FVoidMeridianRoadCategory Cat;
				Cat.Id = N(*C, TEXT("id"));
				double Tier = 0; (*C)->TryGetNumberField(TEXT("hierarchy_tier"), Tier);
				Cat.HierarchyTier = FMath::RoundToInt(Tier);
				Cat.Network = ParseNetwork(S(*C, TEXT("network")));
				RN.Categories.Add(Cat);
			}
		}

		auto ReadRoutes = [&](const TCHAR* Field, EVoidMeridianRouteClass Class)
		{
			const TArray<TSharedPtr<FJsonValue>>* Arr = ArrayField(*Doc, Field);
			if (!Arr) { return; }
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				const FJsonObj* R = nullptr;
				if (!V.IsValid() || !V->TryGetObject(R)) { continue; }
				FVoidMeridianRoute Route;
				Route.Id = N(*R, TEXT("id"));
				Route.RouteClass = Class;
				Route.CategoryId = N(*R, TEXT("category"));
				Route.OriginDistrict = N(*R, TEXT("origin_district"));
				Route.OriginSubLocation = N(*R, TEXT("origin_sub_location"));
				Route.GenerationDependency = N(*R, TEXT("generation_dependency"));
				Route.HardConstraint = S(*R, TEXT("hard_constraint"));
				if (Class == EVoidMeridianRouteClass::Primary)
				{
					Route.Bands = NameArray(*R, TEXT("traverses_bands"));
					Route.ServedDistricts = NameArray(*R, TEXT("serves_districts"));
				}
				else if (Class == EVoidMeridianRouteClass::Secondary)
				{
					const FName Band = N(*R, TEXT("radial_band"));
					if (!Band.IsNone()) { Route.Bands.Add(Band); }
					Route.ServedDistricts = NameArray(*R, TEXT("serves_districts"));
				}
				else
				{
					Route.ServedDistricts = NameArray(*R, TEXT("shared_by_districts"));
					const FName Owner = N(*R, TEXT("owning_district")); // JSON null -> TryGetStringField fails -> empty -> NAME_None-equivalent
					if (!S(*R, TEXT("owning_district")).IsEmpty()) { Route.OwningDistrict = Owner; }
					const TArray<FName> Belts = NameArray(*R, TEXT("sub_belts_served"));
					(void)Belts; // sub-belt ids are qualitative Undercroft data; retained in RawRegistries.
					(*R)->TryGetBoolField(TEXT("vehicle_synchronization"), Route.bVehicleSynchronization);
					if (!Route.OwningDistrict.IsNone()) { Route.ServedDistricts.AddUnique(Route.OwningDistrict); }
				}
				if (const FVoidMeridianRoadCategory* Cat = RN.FindCategory(Route.CategoryId))
				{
					Route.HierarchyTier = Cat->HierarchyTier;
					Route.Network = Cat->Network;
				}
				RN.Routes.Add(MoveTemp(Route));
			}
		};
		ReadRoutes(TEXT("primary_routes"), EVoidMeridianRouteClass::Primary);
		ReadRoutes(TEXT("secondary_routes"), EVoidMeridianRouteClass::Secondary);
		ReadRoutes(TEXT("service_routes"), EVoidMeridianRouteClass::Service);

		if (const TArray<TSharedPtr<FJsonValue>>* Arr = ArrayField(*Doc, TEXT("bridge_relationships")))
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				const FJsonObj* B = nullptr;
				if (!V.IsValid() || !V->TryGetObject(B)) { continue; }
				FVoidMeridianBridge Bridge;
				Bridge.Id = N(*B, TEXT("id")); Bridge.Type = N(*B, TEXT("type")); Bridge.AccessTier = N(*B, TEXT("access_tier"));
				Bridge.Connects = NameArray(*B, TEXT("connects"));
				(*B)->TryGetBoolField(TEXT("flagged_for_signoff"), Bridge.bFlaggedForSignoff);
				RN.Bridges.Add(MoveTemp(Bridge));
			}
		}
		if (const TArray<TSharedPtr<FJsonValue>>* Arr = ArrayField(*Doc, TEXT("tunnel_relationships")))
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				const FJsonObj* T = nullptr;
				if (!V.IsValid() || !V->TryGetObject(T)) { continue; }
				FVoidMeridianTunnel Tunnel;
				Tunnel.Id = N(*T, TEXT("id")); Tunnel.Type = N(*T, TEXT("type")); Tunnel.Connects = NameArray(*T, TEXT("connects"));
				Tunnel.Directionality = S(*T, TEXT("directionality"));
				RN.Tunnels.Add(MoveTemp(Tunnel));
			}
		}

		const FJsonObj* TG = nullptr;
		TArray<TArray<FName>> UnreachablePairs;
		if ((*Doc)->TryGetObjectField(TEXT("traversal_graph"), TG))
		{
			RN.TraversalNodes = NameArray(*TG, TEXT("nodes"));
			if (const TArray<TSharedPtr<FJsonValue>>* Edges = ArrayField(*TG, TEXT("edges")))
			{
				for (const TSharedPtr<FJsonValue>& V : *Edges)
				{
					const FJsonObj* E = nullptr;
					if (!V.IsValid() || !V->TryGetObject(E)) { continue; }
					FVoidMeridianTraversalEdge Edge;
					Edge.From = N(*E, TEXT("from")); Edge.To = N(*E, TEXT("to")); Edge.Via = N(*E, TEXT("via"));
					Edge.Network = ParseNetwork(S(*E, TEXT("network")));
					(*E)->TryGetBoolField(TEXT("directional"), Edge.bDirectional);
					RN.TraversalEdges.Add(Edge);
				}
			}
			if (const TArray<TSharedPtr<FJsonValue>>* Pairs = ArrayField(*TG, TEXT("unreachable_pairs_by_design")))
			{
				for (const TSharedPtr<FJsonValue>& V : *Pairs)
				{
					TArray<FName> P;
					if (V.IsValid() && V->Type == EJson::Array) { for (const TSharedPtr<FJsonValue>& E : V->AsArray()) { FString Str; if (E.IsValid() && E->TryGetString(Str)) { P.Add(FName(*Str)); } } }
					if (P.Num() == 2) { UnreachablePairs.Add(P); }
				}
			}
		}

		// ---- Road network semantic validation (VR-003) ----
		TSet<FName> AllIds;
		auto CheckUnique = [&](FName Id, const FString& Where)
		{
			if (Id.IsNone()) { Report.AddError(FString::Printf(TEXT("Empty id at %s."), *Where), Where, TEXT("VOID.Meridian.MissingId"), TEXT("Every entry needs a non-empty id.")); return; }
			if (AllIds.Contains(Id)) { Report.AddError(FString::Printf(TEXT("Duplicate id '%s' (%s)."), *Id.ToString(), *Where), Where, CodeDuplicate, TEXT("Ids must be unique across categories, routes, tunnels and bridges.")); }
			AllIds.Add(Id);
		};
		for (const FVoidMeridianRoadCategory& C : RN.Categories) { CheckUnique(C.Id, TEXT("RoadNetwork.road_categories")); }
		for (const FVoidMeridianRoute& R : RN.Routes)             { CheckUnique(R.Id, TEXT("RoadNetwork.routes")); }
		for (const FVoidMeridianTunnel& T : RN.Tunnels)           { CheckUnique(T.Id, TEXT("RoadNetwork.tunnel_relationships")); }
		for (const FVoidMeridianBridge& B : RN.Bridges)           { CheckUnique(B.Id, TEXT("RoadNetwork.bridge_relationships")); }

		int32 LastTier = 0;
		for (const FName H : RN.RoadHierarchy)
		{
			const FVoidMeridianRoadCategory* Cat = RN.FindCategory(H);
			if (!Cat) { Report.AddError(FString::Printf(TEXT("road_hierarchy entry '%s' is not a road_category."), *H.ToString()), TEXT("RoadNetwork.road_hierarchy"), CodeBroken, TEXT("Add the category or remove the entry.")); continue; }
			if (Cat->HierarchyTier < LastTier) { Report.AddError(TEXT("road_hierarchy is not in ascending hierarchy_tier order."), TEXT("RoadNetwork.road_hierarchy"), TEXT("VOID.Meridian.HierarchyOrder"), TEXT("Order road_hierarchy from tier 1 downward.")); }
			LastTier = Cat->HierarchyTier;
		}

		TSet<FName> RouteIds, TunnelIds;
		for (const FVoidMeridianRoute& R : RN.Routes) { RouteIds.Add(R.Id); }
		for (const FVoidMeridianTunnel& T : RN.Tunnels) { TunnelIds.Add(T.Id); }

		auto NeedDistrict = [&](FName D, const FString& Where)
		{
			if (!D.IsNone() && !DistrictIds.Contains(D)) { Report.AddError(FString::Printf(TEXT("%s references unknown district '%s'."), *Where, *D.ToString()), Where, CodeBroken, TEXT("Fix the district id.")); }
		};

		for (const FVoidMeridianRoute& R : RN.Routes)
		{
			const FString W = FString::Printf(TEXT("RoadNetwork.routes[%s]"), *R.Id.ToString());
			if (!RN.FindCategory(R.CategoryId)) { Report.AddError(FString::Printf(TEXT("%s has unknown category '%s'."), *W, *R.CategoryId.ToString()), W, CodeBroken, TEXT("Fix the category id.")); }
			NeedDistrict(R.OriginDistrict, W + TEXT(".origin_district"));
			NeedDistrict(R.OwningDistrict, W + TEXT(".owning_district"));
			for (const FName D : R.ServedDistricts) { NeedDistrict(D, W + TEXT(".districts")); }
			for (const FName B : R.Bands) { if (!Manifest.RadialBands.Contains(B)) { Report.AddError(FString::Printf(TEXT("%s references unknown radial band '%s'."), *W, *B.ToString()), W, CodeBroken, TEXT("Use a band from world_scale_metadata.radial_bands.")); } }
			if (!R.GenerationDependency.IsNone() && !DistrictIds.Contains(R.GenerationDependency) && !RouteIds.Contains(R.GenerationDependency))
			{
				Report.AddError(FString::Printf(TEXT("%s generation_dependency '%s' is neither a district nor a route."), *W, *R.GenerationDependency.ToString()), W, CodeBroken, TEXT("Fix the id."));
			}
			if (R.GenerationDependency == R.Id) { Report.AddError(FString::Printf(TEXT("%s depends on itself."), *W), W, CodeBroken, TEXT("Break the cycle.")); }
		}
		for (const FVoidMeridianTunnel& T : RN.Tunnels)
		{
			if (T.Connects.Num() != 2) { Report.AddError(FString::Printf(TEXT("Tunnel '%s' must connect exactly 2 districts."), *T.Id.ToString()), TEXT("RoadNetwork.tunnel_relationships"), TEXT("VOID.Meridian.InvalidTunnel"), TEXT("List exactly two district ids.")); }
			for (const FName D : T.Connects) { NeedDistrict(D, FString::Printf(TEXT("RoadNetwork.tunnel_relationships[%s]"), *T.Id.ToString())); }
		}
		for (const FVoidMeridianTraversalEdge& E : RN.TraversalEdges)
		{
			if (!RouteIds.Contains(E.Via) && !TunnelIds.Contains(E.Via)) { Report.AddError(FString::Printf(TEXT("Traversal edge %s->%s uses unknown route/tunnel '%s'."), *E.From.ToString(), *E.To.ToString(), *E.Via.ToString()), TEXT("RoadNetwork.traversal_graph.edges"), CodeBroken, TEXT("Fix the 'via' id.")); }
			if (!RN.TraversalNodes.Contains(E.From) || !RN.TraversalNodes.Contains(E.To)) { Report.AddError(FString::Printf(TEXT("Traversal edge %s->%s uses a node not in traversal_graph.nodes."), *E.From.ToString(), *E.To.ToString()), TEXT("RoadNetwork.traversal_graph.edges"), CodeBroken, TEXT("Add the node or fix the edge.")); }
			for (const TArray<FName>& P : UnreachablePairs)
			{
				if ((P[0] == E.From && P[1] == E.To) || (P[0] == E.To && P[1] == E.From))
				{
					Report.AddError(FString::Printf(TEXT("%s<->%s is declared unreachable by design but edge via '%s' connects them."), *E.From.ToString(), *E.To.ToString(), *E.Via.ToString()), TEXT("RoadNetwork.traversal_graph"), CodeContradiction, TEXT("Remove the edge or the unreachable pair."));
				}
			}
		}
		if (const TArray<TSharedPtr<FJsonValue>>* Conn = ArrayField(*Doc, TEXT("district_connectivity")))
		{
			for (const TSharedPtr<FJsonValue>& V : *Conn)
			{
				const FJsonObj* C = nullptr;
				if (!V.IsValid() || !V->TryGetObject(C)) { continue; }
				NeedDistrict(N(*C, TEXT("district")), TEXT("RoadNetwork.district_connectivity"));
				for (const FName Via : NameArray(*C, TEXT("connected_via"))) { if (!RN.FindCategory(Via)) { Report.AddError(FString::Printf(TEXT("district_connectivity references unknown category '%s'."), *Via.ToString()), TEXT("RoadNetwork.district_connectivity"), CodeBroken, TEXT("Fix the category id.")); } }
			}
		}
		for (const FVoidMeridianBridge& B : RN.Bridges)
		{
			if (B.bFlaggedForSignoff)
			{
				Report.AddInfo(FString::Printf(TEXT("Bridge '%s' is flagged for sign-off; it is a pedestrian skybridge between buildings and is not generated by the Road Generator."), *B.Id.ToString()),
					FString::Printf(TEXT("RoadNetwork.bridge_relationships[%s]"), *B.Id.ToString()), TEXT("VOID.Meridian.FlaggedBridge"));
			}
			Report.AddWarning(FString::Printf(TEXT("Bridge '%s' connects sub-location ids (%s) defined in district data files that are not part of this package; they cannot be resolved here."), *B.Id.ToString(), *FString::JoinBy(B.Connects, TEXT(", "), [](const FName& Nm) { return Nm.ToString(); })),
				FString::Printf(TEXT("RoadNetwork.bridge_relationships[%s].connects"), *B.Id.ToString()), TEXT("VOID.Meridian.UnresolvableExternalRef"),
				TEXT("Load the owning district's *_data.json to resolve these ids (owned by the Building/District generators)."));
		}
	}

	// ---- 7. generation_order dependencies must reference earlier steps -------------------------
	if (const TArray<TSharedPtr<FJsonValue>>* Steps = Pipeline ? ArrayField(*Pipeline, TEXT("generation_order")) : nullptr)
	{
		TSet<FName> Done;
		for (const TSharedPtr<FJsonValue>& V : *Steps)
		{
			const FJsonObj* Step = nullptr;
			if (!V.IsValid() || !V->TryGetObject(Step)) { continue; }
			for (const FName Dep : NameArray(*Step, TEXT("depends_on")))
			{
				if (!Done.Contains(Dep)) { Report.AddError(FString::Printf(TEXT("generation_order step '%s' depends on '%s', which is not an earlier step."), *S(*Step, TEXT("target")), *Dep.ToString()), TEXT("generation_pipeline.generation_order"), CodeOrder, TEXT("Reorder generation_order.")); }
			}
			Done.Add(N(*Step, TEXT("target")));
		}
	}

	// ---- 8. Canon lock: PackageManifest.json SHA-256 (VR-011) ---------------------------------
	if (Options.bVerifyChecksums)
	{
		const FString ManifestPath = FPaths::Combine(Dir, TEXT("PackageManifest.json"));
		FJsonObj PM; FString Err;
		if (!FPaths::FileExists(ManifestPath))
		{
			Report.AddWarning(TEXT("PackageManifest.json not found; canon-lock checksums were not verified."), TEXT("PackageManifest.json"), CodeLockedMissing, TEXT("Ship PackageManifest.json with the package."));
		}
		else if (!FVoidJsonReader::ReadFromFile(ManifestPath, PM, Err))
		{
			Report.AddError(FString::Printf(TEXT("PackageManifest.json unreadable: %s"), *Err), TEXT("PackageManifest.json"), CodeMalformed, TEXT("Fix the JSON."));
		}
		else
		{
			int32 NumVerified = 0, NumMissingLocked = 0;
			const TArray<FString> Groups = { TEXT("package_files"), TEXT("locked_inputs") };
			for (const FString& Group : Groups)
			{
				const bool bLocked = (Group == TEXT("locked_inputs"));
				const TArray<TSharedPtr<FJsonValue>>* Arr = ArrayField(PM, *Group);
				if (!Arr) { continue; }
				for (const TSharedPtr<FJsonValue>& V : *Arr)
				{
					const FJsonObj* E = nullptr;
					if (!V.IsValid() || !V->TryGetObject(E)) { continue; }
					const FString File = S(*E, TEXT("file"));
					const FString Expected = S(*E, TEXT("sha256")).ToLower();
					const FString Full = FPaths::Combine(Dir, File);
					if (!FPaths::FileExists(Full))
					{
						if (bLocked) { ++NumMissingLocked; }
						else { Report.AddError(FString::Printf(TEXT("Package file '%s' listed in PackageManifest.json is missing."), *File), File, CodeMissingFile, TEXT("Restore the file.")); }
						continue;
					}
					FString Actual;
					if (!FVoidSha256::HashFile(Full, Actual)) { Report.AddError(FString::Printf(TEXT("Could not read '%s' to verify its checksum."), *File), File, CodeMissingFile, TEXT("Check file permissions.")); continue; }
					++NumVerified;
					if (Actual != Expected)
					{
						const FString Msg = FString::Printf(TEXT("Checksum mismatch for %s '%s' (expected %s..., found %s...)."), bLocked ? TEXT("LOCKED input") : TEXT("package file"), *File, *Expected.Left(12), *Actual.Left(12));
						if (Options.bStrictCanonLock) { Report.AddError(Msg, File, CodeChecksum, TEXT("Restore the original file; locked canon files are immutable. Disable strict canon lock only for deliberate experiments.")); }
						else { Report.AddWarning(Msg, File, CodeChecksum, TEXT("Strict canon lock is off; this mismatch is not blocking.")); }
					}
				}
			}
			Report.AddInfo(FString::Printf(TEXT("Verified %d checksum(s) against PackageManifest.json."), NumVerified), TEXT("PackageManifest.json"), TEXT("VOID.Meridian.ChecksumsVerified"));
			if (NumMissingLocked > 0)
			{
				Report.AddWarning(FString::Printf(TEXT("%d locked input file(s) named by PackageManifest.json are not present, so their integrity cannot be verified (absent is not the same as modified)."), NumMissingLocked),
					TEXT("PackageManifest.json.locked_inputs"), CodeLockedMissing, TEXT("Place the five district packages next to the package if a full canon-lock check is required."));
			}
		}
	}

	return Finish();
}
