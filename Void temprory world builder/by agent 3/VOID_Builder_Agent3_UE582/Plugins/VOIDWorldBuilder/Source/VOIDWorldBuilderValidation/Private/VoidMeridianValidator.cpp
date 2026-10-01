// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VoidMeridianValidator.h"
#include "VoidJsonSchemaSubset.h"
#include "VoidSha256.h"
#include "VoidValidationLog.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Containers/StringConv.h"

namespace
{
	using FObj = TSharedPtr<FJsonObject>;
	using FArr = TArray<TSharedPtr<FJsonValue>>;

	constexpr int64 MaxMeridianFileBytes = 64ll * 1024 * 1024;

	// ------------------------------------------------------------ JSON helpers
	TSharedPtr<FJsonValue> Get(const FObj& O, const TCHAR* Key)
	{
		if (!O.IsValid()) { return nullptr; }
		const TSharedPtr<FJsonValue>* F = O->Values.Find(FString(Key));
		return F ? *F : nullptr;
	}
	FObj GetObj(const FObj& O, const TCHAR* Key)
	{
		const TSharedPtr<FJsonValue> V = Get(O, Key);
		return (V.IsValid() && V->Type == EJson::Object) ? V->AsObject() : FObj();
	}
	const FArr* GetArr(const FObj& O, const TCHAR* Key)
	{
		const TSharedPtr<FJsonValue> V = Get(O, Key);
		if (V.IsValid() && V->Type == EJson::Array)
		{
			return &V->AsArray();
		}
		return nullptr;
	}
	FString GetStr(const FObj& O, const TCHAR* Key)
	{
		const TSharedPtr<FJsonValue> V = Get(O, Key);
		return (V.IsValid() && V->Type == EJson::String) ? V->AsString() : FString();
	}
	TArray<FString> StrList(const FArr* A)
	{
		TArray<FString> Out;
		if (A)
		{
			for (const TSharedPtr<FJsonValue>& V : *A)
			{
				if (V.IsValid() && V->Type == EJson::String) { Out.Add(V->AsString()); }
			}
		}
		return Out;
	}
	TArray<FObj> ObjList(const FArr* A)
	{
		TArray<FObj> Out;
		if (A)
		{
			for (const TSharedPtr<FJsonValue>& V : *A)
			{
				if (V.IsValid() && V->Type == EJson::Object) { Out.Add(V->AsObject()); }
			}
		}
		return Out;
	}

	FString ToHex(const uint8 Digest[32])
	{
		FString Out;
		for (int32 i = 0; i < 32; ++i) { Out += FString::Printf(TEXT("%02x"), Digest[i]); }
		return Out;
	}
	FString Sha256Hex(const TArray<uint8>& Bytes)
	{
		uint8 Digest[32];
		VoidValidationHash::Sha256(Bytes.GetData(), static_cast<size_t>(Bytes.Num()), Digest);
		return ToHex(Digest);
	}
	TArray<uint8> CrlfToLf(const TArray<uint8>& In)
	{
		TArray<uint8> Out;
		Out.Reserve(In.Num());
		for (int32 i = 0; i < In.Num(); ++i)
		{
			if (In[i] == '\r' && i + 1 < In.Num() && In[i + 1] == '\n') { continue; }
			Out.Add(In[i]);
		}
		return Out;
	}

	/** Trailing "_vN" of a schema id, or -1 if it has none. */
	int32 SchemaIdVersion(const FString& Id)
	{
		int32 Pos = INDEX_NONE;
		if (!Id.FindLastChar(TEXT('_'), Pos) || Pos + 2 >= Id.Len() || Id[Pos + 1] != TEXT('v'))
		{
			return -1;
		}
		const FString Digits = Id.RightChop(Pos + 2);
		return Digits.IsNumeric() ? FCString::Atoi(*Digits) : -1;
	}

	/** Iterative DFS cycle finder. Nodes absent from the graph are leaves. */
	bool FindCycle(const TMap<FString, TArray<FString>>& Graph, TArray<FString>& OutCycle)
	{
		TMap<FString, int32> Color; // 1 = on stack, 2 = done
		for (const TPair<FString, TArray<FString>>& Root : Graph)
		{
			if (Color.Contains(Root.Key)) { continue; }
			TArray<FString> Path;
			TArray<int32> NextChild;
			Path.Add(Root.Key); NextChild.Add(0); Color.Add(Root.Key, 1);
			while (Path.Num() > 0)
			{
				const FString Node = Path.Last();
				const TArray<FString>* Children = Graph.Find(Node);
				int32& Idx = NextChild.Last();
				if (Children && Idx < Children->Num())
				{
					const FString Child = (*Children)[Idx++];
					const int32* State = Color.Find(Child);
					if (State && *State == 1)
					{
						OutCycle.Reset();
						const int32 Start = Path.IndexOfByKey(Child);
						for (int32 i = Start; i < Path.Num(); ++i) { OutCycle.Add(Path[i]); }
						OutCycle.Add(Child);
						return true;
					}
					if (!State)
					{
						Color.Add(Child, 1); Path.Add(Child); NextChild.Add(0);
					}
				}
				else
				{
					Color.Add(Node, 2); Path.Pop(); NextChild.Pop();
				}
			}
		}
		return false;
	}

	// ------------------------------------------------------------------ run
	class FMeridianRun
	{
	public:
		FMeridianRun(const FVoidMeridianFileSet& InFiles, FVoidValidationContext& InCtx, FVoidMeridianSummary& InSummary)
			: Files(InFiles), Ctx(InCtx), Summary(InSummary) {}

		void Execute()
		{
			Summary.NumFiles = Files.Files.Num();
			ParseAll();
			if (!Root(TEXT("Meridian_Master.json")).IsValid())
			{
				Err(TEXT("Meridian_Master.json"), TEXT("VOID.Meridian.ManifestMissing"),
					TEXT("Meridian_Master.json (the single import manifest) is missing or unreadable; nothing else can be located."),
					TEXT("Meridian_Master.json"), FString(), TEXT("Restore Meridian_Master.json from the package."), true);
				return;
			}
			Summary.bManifestLoaded = true;
			CheckRequiredFiles();
			CheckSchemas();
			LoadIndexes();
			CheckDistrictRegistry();
			CheckRoadAndMetroReferences();
			WalkAll();
			CheckWorldPartition();
			CheckDataLayers();
			CheckPipeline();
			ReportFlaggedItems();
			CheckManifestIntegrity();
			ReportInformational();
		}

	private:
		const FVoidMeridianFileSet& Files;
		FVoidValidationContext& Ctx;
		FVoidMeridianSummary& Summary;

		TMap<FString, TSharedPtr<FJsonValue>> Parsed;
		TSet<FString> DistrictIds;
		TSet<FString> RouteIds;
		TSet<FString> StructureIds;
		TSet<FString> StationIds;
		TSet<FString> RegionIds;

		FObj Root(const TCHAR* Name) const
		{
			const TSharedPtr<FJsonValue>* V = Parsed.Find(FString(Name));
			return (V && V->IsValid() && (*V)->Type == EJson::Object) ? (*V)->AsObject() : FObj();
		}

		void Emit(EVoidValidationSeverity Sev, const FString& File, const TCHAR* Code, const FString& Msg, const FString& Obj, const FString& Path, const FString& Fix)
		{
			Ctx.SetSource(File);
			Ctx.Emit(Sev, FName(Code), Msg, Obj, Path, Fix);
		}
		void Err(const FString& File, const TCHAR* Code, const FString& Msg, const FString& Obj, const FString& Path, const FString& Fix, bool bFatal = false)
		{
			Emit(bFatal ? EVoidValidationSeverity::Fatal : EVoidValidationSeverity::Error, File, Code, Msg, Obj, Path, Fix);
		}
		void Warn(const FString& File, const TCHAR* Code, const FString& Msg, const FString& Obj, const FString& Path, const FString& Fix)
		{
			Emit(EVoidValidationSeverity::Warning, File, Code, Msg, Obj, Path, Fix);
		}
		void Info(const FString& File, const TCHAR* Code, const FString& Msg, const FString& Obj, const FString& Path, const FString& Fix)
		{
			Emit(EVoidValidationSeverity::Info, File, Code, Msg, Obj, Path, Fix);
		}

		// -------------------------------------------------- VR-001 syntax
		void ParseAll()
		{
			TArray<FString> Names;
			Files.Files.GetKeys(Names);
			Names.Sort();
			for (const FString& Name : Names)
			{
				if (!Name.EndsWith(TEXT(".json"))) { continue; }
				const TArray<uint8>& Bytes = Files.Files[Name];
				int32 Offset = 0;
				if (Bytes.Num() >= 3 && Bytes[0] == 0xEF && Bytes[1] == 0xBB && Bytes[2] == 0xBF) { Offset = 3; } // UTF-8 BOM
				FString Text;
				if (Bytes.Num() > Offset)
				{
					FUTF8ToTCHAR Conv(reinterpret_cast<const ANSICHAR*>(Bytes.GetData() + Offset), Bytes.Num() - Offset);
					Text = FString(Conv.Length(), Conv.Get());
				}
				const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
				TSharedPtr<FJsonValue> Value;
				if (!FJsonSerializer::Deserialize(Reader, Value) || !Value.IsValid())
				{
					Err(Name, TEXT("VOID.Meridian.InvalidJson"),
						FString::Printf(TEXT("%s is not valid JSON: %s (line %d, column %d)."), *Name, *Reader->GetErrorMessage(), Reader->GetLineNumber(), Reader->GetCharacterNumber()),
						Name, FString(), TEXT("Fix the syntax error at the reported position and re-export the file. (VR-001)"));
					continue;
				}
				Parsed.Add(Name, Value);
				++Summary.NumJsonParsed;
			}
		}

		void CheckRequiredFiles()
		{
			const FObj RegRefs = GetObj(Root(TEXT("Meridian_Master.json")), TEXT("registry_references"));
			if (!RegRefs.IsValid()) { return; }
			TArray<FString> Keys;
			RegRefs->Values.GetKeys(Keys);
			Keys.Sort();
			for (const FString& Key : Keys)
			{
				const FObj Ref = GetObj(RegRefs, *Key);
				const FString File = GetStr(Ref, TEXT("file"));
				const TSharedPtr<FJsonValue> Required = Get(Ref, TEXT("required"));
				if (!File.IsEmpty() && Required.IsValid() && Required->Type == EJson::Boolean && Required->AsBool() && !Files.Contains(File))
				{
					Summary.MissingFiles.Add(File);
					Err(TEXT("Meridian_Master.json"), TEXT("VOID.Meridian.MissingRequiredFile"),
						FString::Printf(TEXT("Required registry '%s' points to '%s', which is not in the package."), *Key, *File),
						Key, FString::Printf(TEXT("$.registry_references.%s.file"), *Key), TEXT("Add the file to the package, or (with sign-off) remove it from registry_references."));
				}
			}
		}

		// ------------------------------------------- VR-002 schema conformance
		void CheckSchemas()
		{
			TArray<FString> Names;
			Parsed.GetKeys(Names);
			Names.Sort();
			for (const FString& Name : Names)
			{
				if (Name.EndsWith(TEXT(".schema.json"))) { continue; }
				const FObj Data = Root(*Name);
				const FString Declared = Data.IsValid() ? GetStr(Data, TEXT("$schema")) : FString();

				if (!Declared.IsEmpty())
				{
					const int32 Version = SchemaIdVersion(Declared);
					if (Version >= 0 && Version != FVoidMeridianValidator::GetSupportedSchemaMajor())
					{
						Err(Name, TEXT("VOID.Meridian.UnsupportedSchemaVersion"),
							FString::Printf(TEXT("%s declares schema '%s' (version %d); this Builder supports version %d."), *Name, *Declared, Version, FVoidMeridianValidator::GetSupportedSchemaMajor()),
							Name, TEXT("$.$schema"), TEXT("Re-export the file at a supported schema version, or upgrade the Builder."));
					}
				}

				const FString SchemaName = Name.LeftChop(5) + TEXT(".schema.json");
				const FObj Schema = Root(*SchemaName);
				if (!Schema.IsValid())
				{
					Summary.FilesWithoutSchema.Add(Name);
					continue;
				}
				const FString SchemaId = GetStr(Schema, TEXT("$id"));
				if (SchemaId != Declared)
				{
					Err(Name, TEXT("VOID.Meridian.SchemaIdMismatch"),
						FString::Printf(TEXT("%s declares $schema '%s' but %s has $id '%s'."), *Name, *Declared, *SchemaName, *SchemaId),
						Name, TEXT("$.$schema"), TEXT("Point the data file at the schema that describes it."));
				}
				Ctx.SetSource(Name);
				FVoidJsonSchemaSubset::Validate(Parsed[Name], Schema, Name, Ctx);
			}
		}

		// ------------------------------------------------------- id indexes
		void LoadIndexes()
		{
			if (const FObj DR = Root(TEXT("DistrictRegistry.json")))
			{
				for (const FObj& D : ObjList(GetArr(DR, TEXT("districts")))) { DistrictIds.Add(GetStr(D, TEXT("id"))); }
			}
			if (const FObj RN = Root(TEXT("RoadNetwork.json")))
			{
				for (const TCHAR* Key : { TEXT("primary_routes"), TEXT("secondary_routes"), TEXT("service_routes") })
				{
					for (const FObj& R : ObjList(GetArr(RN, Key))) { RouteIds.Add(GetStr(R, TEXT("id"))); }
				}
				for (const TCHAR* Key : { TEXT("tunnel_relationships"), TEXT("bridge_relationships") })
				{
					for (const FObj& R : ObjList(GetArr(RN, Key))) { StructureIds.Add(GetStr(R, TEXT("id"))); }
				}
			}
			if (const FObj MN = Root(TEXT("MetroNetwork.json")))
			{
				for (const FObj& Net : ObjList(GetArr(MN, TEXT("networks"))))
				{
					for (const FObj& S : ObjList(GetArr(Net, TEXT("stations")))) { StationIds.Add(GetStr(S, TEXT("id"))); }
				}
			}
			if (const FObj WP = Root(TEXT("WorldPartition.json")))
			{
				for (const FObj& R : ObjList(GetArr(WP, TEXT("regions")))) { RegionIds.Add(GetStr(R, TEXT("id"))); }
			}
			for (const FString& Id : DistrictIds) { Summary.DistrictIds.Add(FName(*Id)); }
			for (const FString& Id : RouteIds) { Summary.RouteIds.Add(FName(*Id)); }
		}

		// ------------------------------- district registry: VR-007/009/010
		void CheckDistrictRegistry()
		{
			const FObj DR = Root(TEXT("DistrictRegistry.json"));
			if (!DR.IsValid()) { return; }
			const FString File = TEXT("DistrictRegistry.json");

			TMap<FString, int32> Seen;
			for (const FObj& D : ObjList(GetArr(DR, TEXT("districts")))) { Seen.FindOrAdd(GetStr(D, TEXT("id")), 0) += 1; }
			for (const TPair<FString, int32>& P : Seen)
			{
				if (P.Value > 1)
				{
					Err(File, TEXT("VOID.Meridian.DuplicateId"), FString::Printf(TEXT("District id '%s' appears %d times in DistrictRegistry.districts."), *P.Key, P.Value), P.Key, TEXT("$.districts"), TEXT("Make district ids unique."));
				}
			}

			TSet<FString> Expected(FVoidMeridianValidator::GetExpectedDistrictIds());
			if (!DistrictIds.Difference(Expected).IsEmpty() || !Expected.Difference(DistrictIds).IsEmpty())
			{
				Err(File, TEXT("VOID.Meridian.DistrictSetMismatch"),
					FString::Printf(TEXT("Registry districts differ from the canon-locked five (VR-007): extra [%s], missing [%s]."),
						*FString::Join(DistrictIds.Difference(Expected).Array(), TEXT(", ")), *FString::Join(Expected.Difference(DistrictIds).Array(), TEXT(", "))),
					FString(), TEXT("$.districts"), TEXT("New districts may only be added through the registry extension review gate."));
			}

			// VR-010: every unordered pair exactly once
			TMap<FString, int32> PairCount;
			for (const FObj& Rel : ObjList(GetArr(DR, TEXT("relationships"))))
			{
				TArray<FString> Pair = StrList(GetArr(Rel, TEXT("pair")));
				if (Pair.Num() != 2) { continue; }
				Pair.Sort();
				PairCount.FindOrAdd(Pair[0] + TEXT("|") + Pair[1], 0) += 1;
			}
			TArray<FString> Sorted = DistrictIds.Array();
			Sorted.Sort();
			for (int32 i = 0; i < Sorted.Num(); ++i)
			{
				for (int32 j = i + 1; j < Sorted.Num(); ++j)
				{
					const FString Key = Sorted[i] + TEXT("|") + Sorted[j];
					const int32* Count = PairCount.Find(Key);
					if (!Count)
					{
						Err(File, TEXT("VOID.Meridian.MissingRelationshipPair"), FString::Printf(TEXT("No relationship is documented for district pair %s (VR-010)."), *Key), Key, TEXT("$.relationships"), TEXT("Document the pair exactly once."));
					}
					else if (*Count > 1)
					{
						Err(File, TEXT("VOID.Meridian.DuplicateRelationshipPair"), FString::Printf(TEXT("District pair %s is documented %d times (VR-010)."), *Key, *Count), Key, TEXT("$.relationships"), TEXT("Keep exactly one entry per pair."));
					}
				}
			}

			// VR-009 (district tier) + consistency of the two dependency declarations
			TMap<FString, TArray<FString>> Deps;
			if (const FObj DepObj = GetObj(DR, TEXT("dependencies")))
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& P : DepObj->Values)
				{
					if (P.Value.IsValid() && P.Value->Type == EJson::Array) { Deps.Add(P.Key, StrList(&P.Value->AsArray())); }
				}
			}
			TArray<FString> Cycle;
			if (FindCycle(Deps, Cycle))
			{
				Err(File, TEXT("VOID.Meridian.DependencyCycle"), FString::Printf(TEXT("District dependency cycle: %s (VR-009)."), *FString::Join(Cycle, TEXT(" -> "))), Cycle[0], TEXT("$.dependencies"), TEXT("Break the cycle."));
			}
			for (const FObj& D : ObjList(GetArr(DR, TEXT("districts"))))
			{
				const FString Id = GetStr(D, TEXT("id"));
				TArray<FString> A = StrList(GetArr(D, TEXT("depends_on")));
				TArray<FString> B = Deps.Contains(Id) ? Deps[Id] : TArray<FString>();
				A.Sort(); B.Sort();
				if (A != B)
				{
					Err(File, TEXT("VOID.Meridian.DependencyMismatch"), FString::Printf(TEXT("District '%s': depends_on disagrees with the 'dependencies' map."), *Id), Id, TEXT("$.districts"), TEXT("Make both dependency declarations agree."));
				}
			}

			Info(File, TEXT("VOID.Meridian.AmbiguousOrderFields"),
				TEXT("DistrictRegistry carries both build_order_index (authoring order) and generation_priority (generation order); they intentionally disagree. Generators must use Meridian_Master.generation_pipeline.generation_order only."),
				FString(), TEXT("$.districts[*]"), TEXT("Never derive generation order from build_order_index."));
		}

		// ------------------------------------ VR-003 road/metro references
		void CheckRoadAndMetroReferences()
		{
			if (const FObj RN = Root(TEXT("RoadNetwork.json")))
			{
				const FString File = TEXT("RoadNetwork.json");
				if (const FObj TG = GetObj(RN, TEXT("traversal_graph")))
				{
					const TSet<FString> Nodes(StrList(GetArr(TG, TEXT("nodes"))));
					for (const FObj& E : ObjList(GetArr(TG, TEXT("edges"))))
					{
						const FString Via = GetStr(E, TEXT("via"));
						if (!Via.IsEmpty() && !RouteIds.Contains(Via) && !StructureIds.Contains(Via))
						{
							Err(File, TEXT("VOID.Meridian.UnresolvedReference"), FString::Printf(TEXT("Traversal edge 'via' = '%s' matches no route, tunnel or bridge id."), *Via), Via, TEXT("$.traversal_graph.edges"), TEXT("Fix the id, or define the route/tunnel/bridge."));
						}
						for (const TCHAR* End : { TEXT("from"), TEXT("to") })
						{
							const FString Node = GetStr(E, End);
							if (!Node.IsEmpty() && !Nodes.Contains(Node))
							{
								Err(File, TEXT("VOID.Meridian.UnresolvedReference"), FString::Printf(TEXT("Traversal edge %s = '%s' is not listed in traversal_graph.nodes."), End, *Node), Node, TEXT("$.traversal_graph.edges"), TEXT("Add the node or fix the id."));
							}
						}
					}
				}
			}
			if (const FObj MN = Root(TEXT("MetroNetwork.json")))
			{
				const FString File = TEXT("MetroNetwork.json");
				if (RouteIds.Num() > 0)
				{
					for (const FObj& Net : ObjList(GetArr(MN, TEXT("networks"))))
					{
						for (const FObj& Line : ObjList(GetArr(Net, TEXT("lines"))))
						{
							const FString Shared = GetStr(Line, TEXT("shares_route_id"));
							if (!Shared.IsEmpty() && !RouteIds.Contains(Shared))
							{
								Err(File, TEXT("VOID.Meridian.UnresolvedReference"), FString::Printf(TEXT("Metro line '%s' shares_route_id '%s', which matches no road route."), *GetStr(Line, TEXT("id")), *Shared), Shared, TEXT("$.networks[*].lines"), TEXT("Fix the route id."));
							}
						}
					}
				}
				for (const FObj& C : ObjList(GetArr(MN, TEXT("district_connectivity"))))
				{
					const FString Ref = GetStr(C, TEXT("station_ref"));
					if (!Ref.IsEmpty() && !StationIds.Contains(Ref))
					{
						Err(File, TEXT("VOID.Meridian.UnresolvedReference"), FString::Printf(TEXT("district_connectivity station_ref '%s' matches no station."), *Ref), Ref, TEXT("$.district_connectivity"), TEXT("Fix the station id."));
					}
				}
			}
		}

		// ------------------ district references (VR-003/007) + geometry (VR-012)
		static bool IsGeometryKey(const FString& Key)
		{
			static const TCHAR* Tokens[] = { TEXT("coordinate"), TEXT("transform"), TEXT("mesh_id"), TEXT("centerline"), TEXT("footprint"), TEXT("vertices"), TEXT("position_xyz"), TEXT("location_xyz") };
			const FString Lower = Key.ToLower();
			for (const TCHAR* T : Tokens) { if (Lower.Contains(T)) { return true; } }
			return false;
		}

		void Walk(const FString& File, const TSharedPtr<FJsonValue>& Node, const FString& Path)
		{
			if (!Node.IsValid()) { return; }
			if (Node->Type == EJson::Object)
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& P : Node->AsObject()->Values)
				{
					const FString Child = Path + TEXT(".") + P.Key;
					const TSharedPtr<FJsonValue>& V = P.Value;
					if (DistrictIds.Num() > 0)
					{
						if ((P.Key == TEXT("origin_district") || P.Key == TEXT("owning_district") || P.Key == TEXT("district")) && V->Type == EJson::String && !DistrictIds.Contains(V->AsString()))
						{
							// GameplayGraph.mission_relationships[].district is polymorphic in the real data: it also carries
							// sub-locations and transitions. Warn rather than error so real errors elsewhere stay visible.
							const bool bPolymorphic = File == TEXT("GameplayGraph.json") && Child.StartsWith(TEXT("$.mission_relationships[")) && Child.EndsWith(TEXT("].district"));
							if (bPolymorphic)
							{
								Warn(File, TEXT("VOID.Meridian.PolymorphicDistrictField"), FString::Printf(TEXT("'district' value '%s' is not a district id; in GameplayGraph.mission_relationships this field also carries sub-locations/transitions."), *V->AsString()), V->AsString(), Child, TEXT("Ignore, or split into district + location fields in the next schema revision."));
							}
							else
							{
								Err(File, TEXT("VOID.Meridian.UnresolvedReference"), FString::Printf(TEXT("District reference '%s' matches no district in DistrictRegistry."), *V->AsString()), V->AsString(), Child, TEXT("Fix the district id."));
							}
						}
						if ((P.Key == TEXT("serves_districts") || P.Key == TEXT("shared_by_districts")) && V->Type == EJson::Array)
						{
							for (const FString& Id : StrList(&V->AsArray()))
							{
								if (!DistrictIds.Contains(Id))
								{
									Err(File, TEXT("VOID.Meridian.UnresolvedReference"), FString::Printf(TEXT("District reference '%s' matches no district in DistrictRegistry."), *Id), Id, Child, TEXT("Fix the district id."));
								}
							}
						}
					}
					if (IsGeometryKey(P.Key) && V->Type != EJson::String)
					{
						Err(File, TEXT("VOID.Meridian.FabricatedGeometry"), FString::Printf(TEXT("Key '%s' carries coordinate/transform-shaped data; the package forbids fabricated engineering precision (VR-012)."), *P.Key), P.Key, Child, TEXT("Remove it; the Builder computes geometry."));
					}
					Walk(File, V, Child);
				}
			}
			else if (Node->Type == EJson::Array)
			{
				const FArr& Items = Node->AsArray();
				bool bNumericTuple = Items.Num() >= 2 && Items.Num() <= 3;
				for (const TSharedPtr<FJsonValue>& I : Items) { bNumericTuple = bNumericTuple && I.IsValid() && I->Type == EJson::Number; }
				if (bNumericTuple)
				{
					Warn(File, TEXT("VOID.Meridian.CoordinateShapedArray"), FString::Printf(TEXT("Numeric array of length %d at this path looks like a coordinate (VR-012)."), Items.Num()), FString(), Path, TEXT("Confirm it is not a world position."));
				}
				for (int32 i = 0; i < Items.Num(); ++i) { Walk(File, Items[i], FString::Printf(TEXT("%s[%d]"), *Path, i)); }
			}
		}

		void WalkAll()
		{
			TArray<FString> Names;
			Parsed.GetKeys(Names);
			Names.Sort();
			for (const FString& Name : Names)
			{
				if (Name.EndsWith(TEXT(".schema.json")) || Name == TEXT("PackageManifest.json")) { continue; }
				Walk(Name, Parsed[Name], TEXT("$"));
			}
		}

		// ---------------------------------------------- WorldPartition, VR-004
		void CheckWorldPartition()
		{
			const FObj WP = Root(TEXT("WorldPartition.json"));
			const FObj DR = Root(TEXT("DistrictRegistry.json"));
			if (!WP.IsValid() || DistrictIds.Num() == 0) { return; }
			const FString File = TEXT("WorldPartition.json");

			TMap<FString, TSet<FString>> ContainedByDistrict; // non-template regions only
			TSet<FString> RegionDistricts;
			for (const FObj& R : ObjList(GetArr(WP, TEXT("regions"))))
			{
				const FString District = GetStr(R, TEXT("district"));
				RegionDistricts.Add(District);
				TSet<FString> CellIds;
				for (const FObj& Cell : ObjList(GetArr(R, TEXT("streaming_cells")))) { CellIds.Add(GetStr(Cell, TEXT("id"))); }
				for (const FObj& CD : ObjList(GetArr(R, TEXT("cell_dependencies"))))
				{
					for (const TCHAR* Key : { TEXT("cell"), TEXT("streams_after") })
					{
						const FString Ref = GetStr(CD, Key);
						if (!Ref.IsEmpty() && !CellIds.Contains(Ref))
						{
							Err(File, TEXT("VOID.Meridian.UnresolvedReference"), FString::Printf(TEXT("Cell dependency %s '%s' is not a cell of region %s."), Key, *Ref, *GetStr(R, TEXT("id"))), Ref, TEXT("$.regions[*].cell_dependencies"), TEXT("Fix the cell id."));
						}
					}
				}
				const TSharedPtr<FJsonValue> Templ = Get(R, TEXT("template_based"));
				const bool bTemplate = Templ.IsValid() && Templ->Type == EJson::Boolean && Templ->AsBool();
				if (!bTemplate)
				{
					for (const FObj& Cell : ObjList(GetArr(R, TEXT("streaming_cells"))))
					{
						for (const FString& C : StrList(GetArr(Cell, TEXT("contains")))) { ContainedByDistrict.FindOrAdd(District).Add(C); }
					}
				}
			}
			if (RegionDistricts.Difference(DistrictIds).Num() > 0 || DistrictIds.Difference(RegionDistricts).Num() > 0)
			{
				Err(File, TEXT("VOID.Meridian.DistrictSetMismatch"), FString::Printf(TEXT("WorldPartition regions cover [%s] but the registry has [%s]."), *FString::Join(RegionDistricts.Array(), TEXT(", ")), *FString::Join(DistrictIds.Array(), TEXT(", "))), FString(), TEXT("$.regions"), TEXT("Every district needs its own region(s)."));
			}
			if (DR.IsValid())
			{
				if (const FObj SR = GetObj(DR, TEXT("streaming_relationships")))
				{
					for (const TPair<FString, TSharedPtr<FJsonValue>>& P : SR->Values)
					{
						const FString Region = P.Value.IsValid() && P.Value->Type == EJson::Object ? GetStr(P.Value->AsObject(), TEXT("region")) : FString();
						if (!Region.IsEmpty() && !RegionIds.Contains(Region))
						{
							Err(TEXT("DistrictRegistry.json"), TEXT("VOID.Meridian.UnresolvedReference"), FString::Printf(TEXT("Registry streaming region '%s' (district '%s') does not exist in WorldPartition."), *Region, *P.Key), Region, TEXT("$.streaming_relationships"), TEXT("Fix the region id."));
						}
					}
				}
			}

			// VR-004: each district's sub_locations must be covered exactly once, when its data file is available.
			if (!DR.IsValid()) { return; }
			for (const FObj& D : ObjList(GetArr(DR, TEXT("districts"))))
			{
				const FString Id = GetStr(D, TEXT("id"));
				const FString DataFile = GetStr(D, TEXT("data_reference"));
				const FObj Data = DataFile.IsEmpty() ? FObj() : Root(*DataFile);
				const FArr* Subs = Data.IsValid() ? GetArr(Data, TEXT("sub_locations")) : nullptr;
				if (!Subs)
				{
					Info(File, TEXT("VOID.Meridian.SubLocationCheckSkipped"), FString::Printf(TEXT("VR-004 for district '%s' skipped: %s is %s."), *Id, *DataFile, Files.Contains(DataFile) ? TEXT("present but has no sub_locations array") : TEXT("not present")), Id, FString(), TEXT("Supply the district data file to enable this check."));
					continue;
				}
				const TSet<FString>* Contained = ContainedByDistrict.Find(Id);
				TSet<FString> SubIds;
				for (const FObj& S : ObjList(Subs))
				{
					const FString SubId = GetStr(S, TEXT("id"));
					SubIds.Add(SubId);
					if (!Contained || !Contained->Contains(SubId))
					{
						Err(File, TEXT("VOID.Meridian.SubLocationUncovered"), FString::Printf(TEXT("Sub-location '%s' of district '%s' is in no streaming cell (VR-004)."), *SubId, *Id), SubId, TEXT("$.regions[*].streaming_cells"), TEXT("Add it to exactly one cell's 'contains'."));
					}
				}
				if (Contained)
				{
					for (const FString& C : *Contained)
					{
						if (!SubIds.Contains(C))
						{
							Warn(File, TEXT("VOID.Meridian.CellContainsUnknownSubLocation"), FString::Printf(TEXT("A '%s' streaming cell contains '%s', which is not a sub_location in %s."), *Id, *C, *DataFile), C, TEXT("$.regions[*].streaming_cells"), TEXT("Fix the id, or record it as an explicit content-state exception."));
						}
					}
				}
			}
		}

		// ---------------------------------------------------------- VR-005
		void CheckDataLayers()
		{
			const FObj DL = Root(TEXT("DataLayers.json"));
			if (!DL.IsValid()) { return; }
			int32 Actual = 0;
			for (const FObj& G : ObjList(GetArr(DL, TEXT("district_gating_layers"))))
			{
				if (const FArr* Layers = GetArr(G, TEXT("layers"))) { Actual += Layers->Num(); }
			}
			const TSharedPtr<FJsonValue> Declared = Get(DL, TEXT("total_district_gating_layers"));
			if (Declared.IsValid() && Declared->Type == EJson::Number && static_cast<int32>(Declared->AsNumber()) != Actual)
			{
				Err(TEXT("DataLayers.json"), TEXT("VOID.Meridian.ArithmeticMismatch"), FString::Printf(TEXT("DataLayers declares total_district_gating_layers=%d but contains %d (VR-005)."), static_cast<int32>(Declared->AsNumber()), Actual), TEXT("total_district_gating_layers"), TEXT("$.total_district_gating_layers"), TEXT("Correct the declared total."));
			}
		}

		// ------------------------------------ generation/import order, VR-009
		void CheckPipeline()
		{
			const FObj MM = Root(TEXT("Meridian_Master.json"));
			const FObj GP = GetObj(MM, TEXT("generation_pipeline"));
			if (!GP.IsValid()) { return; }
			const FString File = TEXT("Meridian_Master.json");

			TMap<FString, TArray<FString>> ModuleDeps;
			if (const FObj MD = GetObj(GP, TEXT("module_dependencies")))
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& P : MD->Values)
				{
					if (P.Value.IsValid() && P.Value->Type == EJson::Array) { ModuleDeps.Add(P.Key, StrList(&P.Value->AsArray())); }
				}
			}
			TSet<FString> Roots;
			if (const FObj RR = GetObj(MM, TEXT("registry_references"))) { for (const TPair<FString, TSharedPtr<FJsonValue>>& P : RR->Values) { Roots.Add(P.Key); } }
			for (const TPair<FString, TArray<FString>>& P : ModuleDeps)
			{
				for (const FString& Dep : P.Value)
				{
					if (!ModuleDeps.Contains(Dep) && !Roots.Contains(Dep))
					{
						Err(File, TEXT("VOID.Meridian.UnresolvedReference"), FString::Printf(TEXT("module_dependencies: '%s' depends on unknown module '%s'."), *P.Key, *Dep), Dep, TEXT("$.generation_pipeline.module_dependencies"), TEXT("Fix the module id."));
					}
				}
			}
			TArray<FString> Cycle;
			if (FindCycle(ModuleDeps, Cycle))
			{
				Err(File, TEXT("VOID.Meridian.DependencyCycle"), FString::Printf(TEXT("Module dependency cycle: %s (VR-009)."), *FString::Join(Cycle, TEXT(" -> "))), Cycle[0], TEXT("$.generation_pipeline.module_dependencies"), TEXT("Break the cycle."));
			}

			TSet<FString> SeenTargets;
			for (const FObj& Step : ObjList(GetArr(GP, TEXT("generation_order"))))
			{
				const FString Target = GetStr(Step, TEXT("target"));
				for (const FString& Dep : StrList(GetArr(Step, TEXT("depends_on"))))
				{
					if (!SeenTargets.Contains(Dep))
					{
						Err(File, TEXT("VOID.Meridian.GenerationOrderViolation"), FString::Printf(TEXT("Generation step '%s' depends on '%s', which is not an earlier step."), *Target, *Dep), Target, TEXT("$.generation_pipeline.generation_order"), TEXT("Reorder the steps or fix the dependency."));
					}
				}
				SeenTargets.Add(Target);
				Summary.GenerationTargets.Add(FName(*Target));
			}

			TMap<FString, int32> ImportIndex;
			const TArray<FString> ImportOrder = StrList(GetArr(GP, TEXT("import_order")));
			for (int32 i = 0; i < ImportOrder.Num(); ++i) { ImportIndex.Add(ImportOrder[i], i); }
			for (const TPair<FString, TArray<FString>>& P : ModuleDeps)
			{
				const int32* NodeIdx = ImportIndex.Find(P.Key);
				for (const FString& Dep : P.Value)
				{
					const int32* DepIdx = ImportIndex.Find(Dep);
					if (NodeIdx && DepIdx && *DepIdx > *NodeIdx)
					{
						Err(File, TEXT("VOID.Meridian.ImportOrderViolation"), FString::Printf(TEXT("import_order places '%s' before its dependency '%s'."), *P.Key, *Dep), P.Key, TEXT("$.generation_pipeline.import_order"), TEXT("Reorder import_order."));
					}
				}
			}
		}

		void ReportFlaggedItems()
		{
			const FObj MM = Root(TEXT("Meridian_Master.json"));
			for (const FObj& Flag : ObjList(GetArr(MM, TEXT("flagged_open_items"))))
			{
				const FString Id = GetStr(Flag, TEXT("id"));
				const TArray<FString> Blocks = StrList(GetArr(Flag, TEXT("blocks")));
				if (Blocks.Num() > 0) { Summary.BlockedContentIds.Add(Id); }
				Info(TEXT("Meridian_Master.json"), TEXT("VOID.Meridian.FlaggedOpenItem"),
					FString::Printf(TEXT("Open item '%s' (%s, %s); blocks: %s."), *Id, *GetStr(Flag, TEXT("priority")), *GetStr(Flag, TEXT("status")), Blocks.Num() ? *FString::Join(Blocks, TEXT(", ")) : TEXT("nothing")),
					Id, TEXT("$.flagged_open_items"), Blocks.Num() ? TEXT("Do not generate the blocked content without recorded writing-team sign-off.") : TEXT("Log the sign-off reminder."));
			}
		}

		// ---------------------------------------------------------- VR-011
		void CheckManifestIntegrity()
		{
			const FObj PM = Root(TEXT("PackageManifest.json"));
			if (!PM.IsValid()) { return; }
			const FString File = TEXT("PackageManifest.json");
			const bool bDowngrade = Ctx.Options.bAllowMissingDistrictPackages;
			TSet<FString> Listed;

			for (const TCHAR* Section : { TEXT("locked_inputs"), TEXT("package_files") })
			{
				for (const FObj& E : ObjList(GetArr(PM, Section)))
				{
					const FString Name = GetStr(E, TEXT("file"));
					Listed.Add(Name);
					const FString Path = FString::Printf(TEXT("$.%s"), Section);
					const TArray<uint8>* Bytes = Files.Files.Find(Name);
					if (!Bytes)
					{
						Summary.MissingFiles.AddUnique(Name);
						Emit(bDowngrade ? EVoidValidationSeverity::Warning : EVoidValidationSeverity::Error, File, TEXT("VOID.Meridian.LockedInputMissing"),
							FString::Printf(TEXT("Manifest %s entry '%s' is not in the package."), Section, *Name), Name, Path,
							TEXT("Supply the file. The district it belongs to cannot be verified or generated without it."));
						continue;
					}
					const FString Expected = GetStr(E, TEXT("sha256")).ToLower();
					if (Expected.IsEmpty()) { continue; }
					if (Sha256Hex(*Bytes) != Expected)
					{
						if (Sha256Hex(CrlfToLf(*Bytes)) == Expected)
						{
							Warn(File, TEXT("VOID.Meridian.LineEndingChecksum"), FString::Printf(TEXT("%s: checksum differs only because of CRLF line endings."), *Name), Name, Path, TEXT("Normalise to LF (git: '* text eol=lf' in .gitattributes)."));
						}
						else
						{
							Err(File, TEXT("VOID.Meridian.ChecksumMismatch"), FString::Printf(TEXT("%s: sha256 differs from the manifest (VR-011); the file was modified after the manifest was generated."), *Name), Name, Path, TEXT("Restore the locked file, or regenerate the manifest with recorded sign-off."));
						}
					}
				}
			}

			TArray<FString> Unlisted;
			for (const TPair<FString, TArray<uint8>>& F : Files.Files) { if (!Listed.Contains(F.Key)) { Unlisted.Add(F.Key); } }
			Unlisted.Sort();
			if (Unlisted.Num() > 0)
			{
				Warn(File, TEXT("VOID.Meridian.UnlistedFile"), FString::Printf(TEXT("%d file(s) in the package are not inventoried by PackageManifest: %s."), Unlisted.Num(), *FString::Join(Unlisted, TEXT(", "))), FString(), FString(), TEXT("Regenerate PackageManifest.json so it inventories every file."));
			}
			if (const FObj Totals = GetObj(PM, TEXT("totals")))
			{
				const TSharedPtr<FJsonValue> Total = Get(Totals, TEXT("total_files"));
				if (Total.IsValid() && Total->Type == EJson::Number && static_cast<int32>(Total->AsNumber()) != Files.Files.Num())
				{
					Warn(File, TEXT("VOID.Meridian.ManifestTotalsStale"), FString::Printf(TEXT("PackageManifest.totals.total_files=%d but the package holds %d file(s)."), static_cast<int32>(Total->AsNumber()), Files.Files.Num()), TEXT("totals"), TEXT("$.totals"), TEXT("Regenerate the manifest."));
				}
			}
		}

		void ReportInformational()
		{
			Info(TEXT("Meridian_Master.json"), TEXT("VOID.Meridian.NoGeometrySource"),
				TEXT("Meridian_Master is coordinate-free by design (builder_configuration.coordinate_policy), but FVoidDesignPackage / FVoidRoadGenerator require centerlinePoints and footprints. A layout/adapter stage must synthesise them; none exists in the Builder yet."),
				FString(), TEXT("$.builder_configuration"), TEXT("Assign ownership of the Meridian -> FVoidDesignPackage layout step (see INTEGRATION_RISK_REGISTER R-01)."));
			if (Summary.FilesWithoutSchema.Num() > 0)
			{
				Summary.FilesWithoutSchema.Sort();
				Info(TEXT(""), TEXT("VOID.Meridian.NoSchemaForFile"), FString::Printf(TEXT("%d JSON file(s) have no companion .schema.json and were only syntax-checked: %s."), Summary.FilesWithoutSchema.Num(), *FString::Join(Summary.FilesWithoutSchema, TEXT(", "))), FString(), FString(), TEXT("Optional: author the missing schemas (void_world_location_schema_v1 is known to be unmaterialised)."));
			}
		}
	};
}

void FVoidMeridianFileSet::AddText(const FString& Name, const FString& Text)
{
	FTCHARToUTF8 Conv(*Text);
	TArray<uint8> Bytes;
	Bytes.Append(reinterpret_cast<const uint8*>(Conv.Get()), Conv.Length());
	Files.Add(Name, MoveTemp(Bytes));
}

bool FVoidMeridianFileSet::AddFromDirectory(const FString& Directory, FString& OutError)
{
	if (!IFileManager::Get().DirectoryExists(*Directory))
	{
		OutError = FString::Printf(TEXT("Meridian directory does not exist: %s"), *Directory);
		return false;
	}
	TArray<FString> Names;
	IFileManager::Get().FindFiles(Names, *(FPaths::Combine(Directory, TEXT("*"))), true, false);
	for (const FString& Name : Names)
	{
		const FString Full = FPaths::Combine(Directory, Name);
		if (IFileManager::Get().FileSize(*Full) > MaxMeridianFileBytes)
		{
			UE_LOG(LogVoidValidation, Warning, TEXT("Skipping oversized file %s"), *Full);
			continue;
		}
		TArray<uint8> Bytes;
		if (FFileHelper::LoadFileToArray(Bytes, *Full)) { Files.Add(Name, MoveTemp(Bytes)); }
		else { UE_LOG(LogVoidValidation, Warning, TEXT("Could not read %s"), *Full); }
	}
	return true;
}

const TArray<FString>& FVoidMeridianValidator::GetExpectedDistrictIds()
{
	static const TArray<FString> Ids = { TEXT("olympus_spire"), TEXT("white_zones"), TEXT("metro_archives"), TEXT("undercroft"), TEXT("sector_0") };
	return Ids;
}

void FVoidMeridianValidator::Validate(const FVoidMeridianFileSet& FileSet, FVoidValidationContext& Context, FVoidMeridianSummary* OutSummary)
{
	FVoidMeridianSummary LocalSummary;
	FVoidMeridianSummary& Summary = OutSummary ? *OutSummary : LocalSummary;
	FMeridianRun Run(FileSet, Context, Summary);
	Run.Execute();
	Context.FlushSuppressionSummary();
}

FVoidValidationReport FVoidMeridianValidator::ValidateDirectory(const FString& Directory, const FVoidValidationOptions& Options, FVoidMeridianSummary* OutSummary)
{
	FVoidValidationReport Report;
	Report.bIsValid = true;
	FVoidValidationContext Context(Report, Options, TEXT("Meridian"), Directory);

	FVoidMeridianFileSet FileSet;
	FString LoadError;
	if (!FileSet.AddFromDirectory(Directory, LoadError))
	{
		Context.Fatal(TEXT("VOID.Meridian.DirectoryMissing"), LoadError, Directory, FString(), TEXT("Pass the folder that contains Meridian_Master.json."));
		return Report;
	}
	Validate(FileSet, Context, OutSummary);
	return Report;
}
