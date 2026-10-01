// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformMisc.h"
#include "VoidMeridianImporter.h"
#include "Utilities/VoidSha256.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "VoidJsonSchemaLite.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace VoidMeridianTestHelpers
{
	/** Miniature package that follows the REAL Meridian structure (master -> registry_references -> registries). No schema files, no manifest. */
	struct FTempPackage
	{
		FString Dir;
		FTempPackage() : Dir(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("VoidTests"), FGuid::NewGuid().ToString())) { IFileManager::Get().MakeDirectory(*Dir, true); }
		~FTempPackage() { IFileManager::Get().DeleteDirectory(*Dir, false, true); }
		void Write(const FString& Name, const FString& Text) const { FFileHelper::SaveStringToFile(Text, *FPaths::Combine(Dir, Name)); }
		FString MasterPath() const { return FPaths::Combine(Dir, TEXT("Meridian_Master.json")); }
	};

	static FString Master(const FString& SchemaId = TEXT("void_meridian_master_schema_v1"))
	{
		return FString::Printf(TEXT(R"({
"$schema":"%s",
"project_metadata":{"package_version":"1.0.0","canon_version":"Final","status":"test"},
"world_scale_metadata":{"radial_bands":["core","inner_rings_unbuilt","mid_tier_rings","seam_zone","outer_rings","off_gradient"],"vertical_tiers":["high","mid","low"]},
"registry_references":{
 "district_registry":{"file":"DistrictRegistry.json","required":true},
 "road_network":{"file":"RoadNetwork.json","required":true}},
"generation_pipeline":{"import_order":["district_registry","road_network"],
 "module_dependencies":{"road_network":["district_registry"]},
 "generation_order":[{"step":1,"target":"olympus_spire","depends_on":[]}]},
"flagged_open_items":[{"id":"skybridge","status":"unresolved_pending_signoff"}],
"builder_configuration":{"coordinate_policy":"no_fabricated_coordinates_no_fabricated_transforms"}
})"), *SchemaId);
	}

	static const TCHAR* Districts = TEXT(R"({"$schema":"void_district_registry_schema_v1","districts":[
{"id":"olympus_spire","display_name":"Olympus Spire","radial_band":"core","vertical_tier":"high","generation_priority":1,"build_order_index":5,"depends_on":[]},
{"id":"white_zones","display_name":"White Zones","radial_band":"mid_tier_rings","vertical_tier":"mid","generation_priority":3,"build_order_index":2,"depends_on":["olympus_spire"]}]})");

	static FString Roads(const FString& SpineCategory = TEXT("radial_arterial"), const FString& ExtraRouteFields = FString(), const FString& ExtraTopLevel = FString())
	{
		return FString::Printf(TEXT(R"({"$schema":"void_road_network_schema_v1","road_hierarchy":["radial_arterial","ring_road"],
"road_categories":[{"id":"radial_arterial","hierarchy_tier":1,"network":"live_network"},{"id":"ring_road","hierarchy_tier":2,"network":"live_network"}],
"primary_routes":[{"id":"spine","category":"%s","origin_district":"olympus_spire","traverses_bands":["core","mid_tier_rings"],"serves_districts":["olympus_spire","white_zones"],"generation_dependency":"olympus_spire"%s}],
"secondary_routes":[{"id":"ring","category":"ring_road","radial_band":"mid_tier_rings","serves_districts":["white_zones"],"generation_dependency":"spine"}],
"service_routes":[],"bridge_relationships":[],
"tunnel_relationships":[{"id":"tun","type":"x","connects":["olympus_spire","white_zones"]}],
"traversal_graph":{"nodes":["olympus_spire","white_zones"],"edges":[{"from":"olympus_spire","to":"white_zones","via":"spine","network":"live_network","directional":false}],"unreachable_pairs_by_design":[]}%s})"),
			*SpineCategory, *ExtraRouteFields, *ExtraTopLevel);
	}

	static FVoidMeridianImportOptions NoChecksums() { FVoidMeridianImportOptions O; O.bVerifyChecksums = false; return O; }

	static bool HasCode(const FVoidMeridianImportResult& R, const TCHAR* Code)
	{
		return R.ValidationReport.Issues.ContainsByPredicate([Code](const FVoidValidationIssue& I) { return I.ErrorCode == FName(Code); });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidMeridianImportValidTest, "VOID.WorldBuilder.Import.Meridian.ValidPackage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidMeridianImportValidTest::RunTest(const FString& Parameters)
{
	using namespace VoidMeridianTestHelpers;
	FTempPackage P; P.Write(TEXT("Meridian_Master.json"), Master()); P.Write(TEXT("DistrictRegistry.json"), Districts); P.Write(TEXT("RoadNetwork.json"), Roads());

	const FVoidMeridianImportResult R = FVoidMeridianImporter::LoadFromMasterFile(P.MasterPath(), NoChecksums());
	TestTrue(TEXT("imports successfully"), R.WasSuccessful());
	TestEqual(TEXT("2 districts"), R.World.Districts.Num(), 2);
	TestEqual(TEXT("2 routes"), R.World.RoadNetwork.Routes.Num(), 2);
	TestEqual(TEXT("1 tunnel"), R.World.RoadNetwork.Tunnels.Num(), 1);
	TestEqual(TEXT("loaded in import order"), R.LoadedRegistries.Num(), 2);
	TestTrue(TEXT("flagged item carried"), R.World.HasFlaggedOpenItem(TEXT("skybridge")));
	if (const FVoidMeridianRoute* Spine = R.World.RoadNetwork.FindRoute(TEXT("spine")))
	{
		TestEqual(TEXT("tier resolved from category"), Spine->HierarchyTier, 1);
		TestTrue(TEXT("network resolved from category"), Spine->Network == EVoidNetworkKind::Live);
		TestEqual(TEXT("bands preserved in order"), Spine->Bands.Num(), 2);
	}
	else { AddError(TEXT("spine route missing")); }
	TestTrue(TEXT("road network raw JSON retained for typed extension"), R.World.RawRegistries.Contains(TEXT("road_network")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidMeridianImportRejectTest, "VOID.WorldBuilder.Import.Meridian.RejectsBadData",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidMeridianImportRejectTest::RunTest(const FString& Parameters)
{
	using namespace VoidMeridianTestHelpers;

	{ // Broken category reference.
		FTempPackage P; P.Write(TEXT("Meridian_Master.json"), Master()); P.Write(TEXT("DistrictRegistry.json"), Districts); P.Write(TEXT("RoadNetwork.json"), Roads(TEXT("no_such_category")));
		const FVoidMeridianImportResult R = FVoidMeridianImporter::LoadFromMasterFile(P.MasterPath(), NoChecksums());
		TestFalse(TEXT("broken category invalid"), R.WasSuccessful());
		TestTrue(TEXT("BrokenReference reported"), HasCode(R, TEXT("VOID.Meridian.BrokenReference")));
	}
	{ // Duplicate id (route id equals a category id).
		FTempPackage P; P.Write(TEXT("Meridian_Master.json"), Master()); P.Write(TEXT("DistrictRegistry.json"), Districts);
		FString Dup = Roads(); Dup.ReplaceInline(TEXT("\"id\":\"ring\""), TEXT("\"id\":\"spine\"")); P.Write(TEXT("RoadNetwork.json"), Dup);
		const FVoidMeridianImportResult R = FVoidMeridianImporter::LoadFromMasterFile(P.MasterPath(), NoChecksums());
		TestTrue(TEXT("DuplicateId reported"), HasCode(R, TEXT("VOID.Meridian.DuplicateId")));
		TestFalse(TEXT("duplicate invalid"), R.WasSuccessful());
	}
	{ // Fabricated coordinates / units are forbidden by Meridian's own policy.
		FTempPackage P; P.Write(TEXT("Meridian_Master.json"), Master()); P.Write(TEXT("DistrictRegistry.json"), Districts);
		P.Write(TEXT("RoadNetwork.json"), Roads(TEXT("radial_arterial"), TEXT(",\"width_meters\":12,\"start_position\":[0,0,0]")));
		const FVoidMeridianImportResult R = FVoidMeridianImporter::LoadFromMasterFile(P.MasterPath(), NoChecksums());
		TestTrue(TEXT("FabricatedSpatialData reported"), HasCode(R, TEXT("VOID.Meridian.FabricatedSpatialData")));
		TestFalse(TEXT("spatial data invalid"), R.WasSuccessful());
	}
	{ // Unknown district reference.
		FTempPackage P; P.Write(TEXT("Meridian_Master.json"), Master()); P.Write(TEXT("DistrictRegistry.json"), Districts);
		FString Bad = Roads(); Bad.ReplaceInline(TEXT("\"origin_district\":\"olympus_spire\""), TEXT("\"origin_district\":\"atlantis\"")); P.Write(TEXT("RoadNetwork.json"), Bad);
		TestFalse(TEXT("unknown district invalid"), FVoidMeridianImporter::LoadFromMasterFile(P.MasterPath(), NoChecksums()).WasSuccessful());
	}
	{ // Missing required registry halts.
		FTempPackage P; P.Write(TEXT("Meridian_Master.json"), Master()); P.Write(TEXT("DistrictRegistry.json"), Districts);
		const FVoidMeridianImportResult R = FVoidMeridianImporter::LoadFromMasterFile(P.MasterPath(), NoChecksums());
		TestFalse(TEXT("missing registry invalid"), R.WasSuccessful());
		TestTrue(TEXT("MissingFile reported"), HasCode(R, TEXT("VOID.Meridian.MissingFile")));
		TestTrue(TEXT("halts before typed data is produced"), R.World.RoadNetwork.Routes.Num() == 0);
	}
	{ // Incompatible schema major.
		FTempPackage P; P.Write(TEXT("Meridian_Master.json"), Master(TEXT("void_meridian_master_schema_v2")));
		const FVoidMeridianImportResult R = FVoidMeridianImporter::LoadFromMasterFile(P.MasterPath(), NoChecksums());
		TestFalse(TEXT("v2 master rejected"), R.WasSuccessful());
		TestTrue(TEXT("SchemaIdMismatch reported"), HasCode(R, TEXT("VOID.Meridian.SchemaIdMismatch")));
	}
	{ // Malformed JSON.
		FTempPackage P; P.Write(TEXT("Meridian_Master.json"), TEXT("{ not json"));
		TestFalse(TEXT("malformed rejected"), FVoidMeridianImporter::LoadFromMasterFile(P.MasterPath(), NoChecksums()).WasSuccessful());
	}
	{ // File not found.
		TestFalse(TEXT("nonexistent rejected"), FVoidMeridianImporter::LoadFromMasterFile(TEXT("Z:/definitely/not/here/Meridian_Master.json"), NoChecksums()).WasSuccessful());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidMeridianCanonLockTest, "VOID.WorldBuilder.Import.Meridian.CanonLockChecksums",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidMeridianCanonLockTest::RunTest(const FString& Parameters)
{
	using namespace VoidMeridianTestHelpers;
	FTempPackage P; P.Write(TEXT("Meridian_Master.json"), Master()); P.Write(TEXT("DistrictRegistry.json"), Districts); P.Write(TEXT("RoadNetwork.json"), Roads());
	P.Write(TEXT("Locked.md"), TEXT("canon text"));
	FString Good; FVoidSha256::HashFile(FPaths::Combine(P.Dir, TEXT("Locked.md")), Good);

	auto WriteManifest = [&](const FString& Sha)
	{
		P.Write(TEXT("PackageManifest.json"), FString::Printf(TEXT(R"({"package_files":[],"locked_inputs":[{"file":"Locked.md","sha256":"%s"},{"file":"Absent.md","sha256":"00"}]})"), *Sha));
	};

	FVoidMeridianImportOptions Strict; Strict.bStrictCanonLock = true;
	WriteManifest(Good);
	FVoidMeridianImportResult R = FVoidMeridianImporter::LoadFromMasterFile(P.MasterPath(), Strict);
	TestTrue(TEXT("matching checksum passes; an ABSENT locked file is only a warning"), R.WasSuccessful());
	TestTrue(TEXT("absent locked input warned"), HasCode(R, TEXT("VOID.Meridian.LockedInputMissing")));

	WriteManifest(TEXT("deadbeef"));
	R = FVoidMeridianImporter::LoadFromMasterFile(P.MasterPath(), Strict);
	TestFalse(TEXT("modified locked file fails under strict lock"), R.WasSuccessful());
	TestTrue(TEXT("ChecksumMismatch reported"), HasCode(R, TEXT("VOID.Meridian.ChecksumMismatch")));

	FVoidMeridianImportOptions Lax; Lax.bStrictCanonLock = false;
	R = FVoidMeridianImporter::LoadFromMasterFile(P.MasterPath(), Lax);
	TestTrue(TEXT("same mismatch is only a warning when strict lock is off"), R.WasSuccessful());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidSchemaLiteTest, "VOID.WorldBuilder.Import.SchemaLite.Keywords",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidSchemaLiteTest::RunTest(const FString& Parameters)
{
	auto Parse = [](const FString& Text) { TSharedPtr<FJsonValue> V; TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text); FJsonSerializer::Deserialize(Reader, V); return V; };
	const TSharedPtr<FJsonValue> SchemaV = Parse(TEXT(R"({"type":"object","required":["a","b"],"additionalProperties":false,"properties":{"a":{"type":"integer","minimum":1},"b":{"type":"array","minItems":1,"maxItems":2,"items":{"enum":["x","y"]}},"c":{"const":"k"}}})"));
	const TSharedPtr<FJsonObject> Schema = SchemaV->AsObject();

	TArray<FString> V;
	TestTrue(TEXT("valid"), FVoidJsonSchemaLite::Validate(Parse(TEXT(R"({"a":2,"b":["x"],"c":"k"})")), Schema, TEXT("r"), V));
	V.Reset(); TestFalse(TEXT("missing required"), FVoidJsonSchemaLite::Validate(Parse(TEXT(R"({"a":2})")), Schema, TEXT("r"), V));
	V.Reset(); TestFalse(TEXT("wrong type"), FVoidJsonSchemaLite::Validate(Parse(TEXT(R"({"a":"s","b":["x"]})")), Schema, TEXT("r"), V));
	V.Reset(); TestFalse(TEXT("non-integer"), FVoidJsonSchemaLite::Validate(Parse(TEXT(R"({"a":1.5,"b":["x"]})")), Schema, TEXT("r"), V));
	V.Reset(); TestFalse(TEXT("below minimum"), FVoidJsonSchemaLite::Validate(Parse(TEXT(R"({"a":0,"b":["x"]})")), Schema, TEXT("r"), V));
	V.Reset(); TestFalse(TEXT("extra property"), FVoidJsonSchemaLite::Validate(Parse(TEXT(R"({"a":2,"b":["x"],"zzz":1})")), Schema, TEXT("r"), V));
	V.Reset(); TestFalse(TEXT("enum violation"), FVoidJsonSchemaLite::Validate(Parse(TEXT(R"({"a":2,"b":["q"]})")), Schema, TEXT("r"), V));
	V.Reset(); TestFalse(TEXT("maxItems"), FVoidJsonSchemaLite::Validate(Parse(TEXT(R"({"a":2,"b":["x","y","x"]})")), Schema, TEXT("r"), V));
	V.Reset(); TestFalse(TEXT("const"), FVoidJsonSchemaLite::Validate(Parse(TEXT(R"({"a":2,"b":["x"],"c":"z"})")), Schema, TEXT("r"), V));
	return true;
}

/**
 * Runs against the REAL Meridian package when -VoidMeridianPath="<folder>" is given to the automation run
 * (e.g. -ExecCmds="Automation RunTests VOID.WorldBuilder.Import.Meridian.RealPackage" -VoidMeridianPath=...).
 * Without the parameter it reports a skip, never a pass-by-default.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidMeridianRealPackageTest, "VOID.WorldBuilder.Import.Meridian.RealPackage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidMeridianRealPackageTest::RunTest(const FString& Parameters)
{
	FString Path;
	if (!FParse::Value(FCommandLine::Get(), TEXT("VoidMeridianPath="), Path) || Path.IsEmpty())
	{
		AddInfo(TEXT("Skipped: pass -VoidMeridianPath=\"<Meridian Master folder>\" to run against the real package."));
		return true;
	}

	const FVoidMeridianImportResult R = FVoidMeridianImporter::LoadFromPath(Path);
	TestTrue(TEXT("real package imports with strict canon lock"), R.WasSuccessful());
	TestEqual(TEXT("5 districts"), R.World.Districts.Num(), 5);
	TestEqual(TEXT("11 registries loaded"), R.LoadedRegistries.Num(), 11);
	TestEqual(TEXT("4 routes (1 primary, 1 secondary, 2 service)"), R.World.RoadNetwork.Routes.Num(), 4);
	TestEqual(TEXT("2 tunnels"), R.World.RoadNetwork.Tunnels.Num(), 2);
	TestEqual(TEXT("1 bridge"), R.World.RoadNetwork.Bridges.Num(), 1);
	TestEqual(TEXT("3 categories"), R.World.RoadNetwork.Categories.Num(), 3);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
