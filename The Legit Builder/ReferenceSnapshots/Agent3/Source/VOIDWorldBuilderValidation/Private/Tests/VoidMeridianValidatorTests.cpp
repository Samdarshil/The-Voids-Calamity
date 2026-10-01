// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Misc/AutomationTest.h"
#include "VoidMeridianValidator.h"
#include "VoidJsonSchemaSubset.h"
#include "VoidSha256.h"
#include "VoidTestHelpers.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Containers/StringConv.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FString DistrictRegistryJson()
	{
		const TCHAR* Ids[5] = { TEXT("olympus_spire"), TEXT("white_zones"), TEXT("metro_archives"), TEXT("undercroft"), TEXT("sector_0") };
		const TCHAR* Deps[5] = { TEXT(""), TEXT("\"olympus_spire\""), TEXT("\"white_zones\",\"undercroft\""), TEXT("\"olympus_spire\""), TEXT("\"undercroft\"") };
		FString Districts, DepMap, Rels;
		for (int32 i = 0; i < 5; ++i)
		{
			Districts += FString::Printf(TEXT("%s{\"id\":\"%s\",\"depends_on\":[%s]}"), i ? TEXT(",") : TEXT(""), Ids[i], Deps[i]);
			DepMap += FString::Printf(TEXT("%s\"%s\":[%s]"), i ? TEXT(",") : TEXT(""), Ids[i], Deps[i]);
			for (int32 j = i + 1; j < 5; ++j) { Rels += FString::Printf(TEXT("%s{\"pair\":[\"%s\",\"%s\"]}"), Rels.IsEmpty() ? TEXT("") : TEXT(","), Ids[i], Ids[j]); }
		}
		return FString::Printf(TEXT("{\"$schema\":\"void_district_registry_schema_v1\",\"districts\":[%s],\"relationships\":[%s],\"dependencies\":{%s}}"), *Districts, *Rels, *DepMap);
	}

	const TCHAR* MasterJson = TEXT("{\"$schema\":\"void_meridian_master_schema_v1\",\"registry_references\":{\"district_registry\":{\"file\":\"DistrictRegistry.json\",\"required\":true},\"road_network\":{\"file\":\"RoadNetwork.json\",\"required\":true}},\"generation_pipeline\":{\"import_order\":[\"district_registry\",\"road_network\"],\"generation_order\":[{\"step\":1,\"target\":\"a\",\"depends_on\":[]},{\"step\":2,\"target\":\"b\",\"depends_on\":[\"a\"]}],\"module_dependencies\":{\"road_network\":[\"district_registry\"]}},\"flagged_open_items\":[{\"id\":\"flag_1\",\"priority\":\"high\",\"status\":\"open\",\"blocks\":[\"thing\"]}]}");
	const TCHAR* RoadJson = TEXT("{\"$schema\":\"void_road_network_schema_v1\",\"primary_routes\":[{\"id\":\"r1\",\"serves_districts\":[\"olympus_spire\"]}],\"secondary_routes\":[],\"service_routes\":[],\"tunnel_relationships\":[{\"id\":\"t1\"}],\"bridge_relationships\":[],\"traversal_graph\":{\"nodes\":[\"a\",\"b\"],\"edges\":[{\"from\":\"a\",\"to\":\"b\",\"via\":\"r1\"}]},\"district_connectivity\":[{\"district\":\"olympus_spire\"}]}");

	FVoidMeridianFileSet BaseSet()
	{
		FVoidMeridianFileSet S;
		S.AddText(TEXT("Meridian_Master.json"), MasterJson);
		S.AddText(TEXT("DistrictRegistry.json"), DistrictRegistryJson());
		S.AddText(TEXT("RoadNetwork.json"), RoadJson);
		return S;
	}

	FString TextOf(const FVoidMeridianFileSet& S, const TCHAR* Name)
	{
		const TArray<uint8>* B = S.Files.Find(Name);
		if (!B || B->Num() == 0) { return FString(); }
		FUTF8ToTCHAR Conv(reinterpret_cast<const ANSICHAR*>(B->GetData()), B->Num());
		return FString(Conv.Length(), Conv.Get());
	}

	/** @return false if From was not found (the test's mutation would be a no-op). */
	bool Edit(FVoidMeridianFileSet& S, const TCHAR* Name, const TCHAR* From, const TCHAR* To)
	{
		FString T = TextOf(S, Name);
		if (!T.Contains(From)) { return false; }
		S.AddText(Name, T.Replace(From, To));
		return true;
	}

	FVoidValidationReport RunSet(const FVoidMeridianFileSet& S, FVoidValidationOptions Options = FVoidValidationOptions(), FVoidMeridianSummary* Summary = nullptr)
	{
		FVoidValidationReport R;
		R.bIsValid = true;
		FVoidValidationContext Ctx(R, Options, TEXT("Meridian"));
		FVoidMeridianValidator::Validate(S, Ctx, Summary);
		return R;
	}

	FString Hex(const FString& Text)
	{
		FTCHARToUTF8 Conv(*Text);
		uint8 D[32];
		VoidValidationHash::Sha256(reinterpret_cast<const uint8*>(Conv.Get()), Conv.Length(), D);
		FString Out;
		for (int32 i = 0; i < 32; ++i) { Out += FString::Printf(TEXT("%02x"), D[i]); }
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidMeridianCleanTest, "VOID.WorldBuilder.Validation.Meridian.CleanFixtureHasNoBlockingIssues",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidMeridianCleanTest::RunTest(const FString&)
{
	FVoidMeridianSummary Summary;
	const FVoidValidationReport R = RunSet(BaseSet(), FVoidValidationOptions(), &Summary);
	TestEqual(TEXT("No blocking issues"), R.NumBlocking(), 0);
	TestEqual(TEXT("Five districts extracted"), Summary.DistrictIds.Num(), 5);
	TestTrue(TEXT("Route ids extracted"), Summary.RouteIds.Contains(FName(TEXT("r1"))));
	TestEqual(TEXT("Generation targets extracted in order"), Summary.GenerationTargets.Num(), 2);
	TestTrue(TEXT("Flagged blocking item surfaced for the pipeline"), Summary.BlockedContentIds.Contains(TEXT("flag_1")));
	TestTrue(TEXT("Flag logged by id, never generically"), VoidTest::HasCode(R, TEXT("VOID.Meridian.FlaggedOpenItem")));
	TestTrue(TEXT("No-geometry contract gap is reported"), VoidTest::HasCode(R, TEXT("VOID.Meridian.NoGeometrySource")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidMeridianMutationTest, "VOID.WorldBuilder.Validation.Meridian.MutationsProduceExpectedErrors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidMeridianMutationTest::RunTest(const FString&)
{
	struct FCase { const TCHAR* Name; TFunction<bool(FVoidMeridianFileSet&)> Mutate; const TCHAR* Code; };
	const TArray<FCase> Cases = {
		{ TEXT("malformed JSON"),          [](FVoidMeridianFileSet& S) { S.AddText(TEXT("RoadNetwork.json"), TEXT("{ \"broken\": ")); return true; }, TEXT("VOID.Meridian.InvalidJson") },
		{ TEXT("missing required file"),   [](FVoidMeridianFileSet& S) { S.Files.Remove(TEXT("RoadNetwork.json")); return true; }, TEXT("VOID.Meridian.MissingRequiredFile") },
		{ TEXT("missing manifest"),        [](FVoidMeridianFileSet& S) { S.Files.Remove(TEXT("Meridian_Master.json")); return true; }, TEXT("VOID.Meridian.ManifestMissing") },
		{ TEXT("unresolved district ref"), [](FVoidMeridianFileSet& S) { return Edit(S, TEXT("RoadNetwork.json"), TEXT("\"district\":\"olympus_spire\""), TEXT("\"district\":\"atlantis\"")); }, TEXT("VOID.Meridian.UnresolvedReference") },
		{ TEXT("unresolved list ref"),     [](FVoidMeridianFileSet& S) { return Edit(S, TEXT("RoadNetwork.json"), TEXT("\"serves_districts\":[\"olympus_spire\"]"), TEXT("\"serves_districts\":[\"nowhere\"]")); }, TEXT("VOID.Meridian.UnresolvedReference") },
		{ TEXT("unresolved edge via"),     [](FVoidMeridianFileSet& S) { return Edit(S, TEXT("RoadNetwork.json"), TEXT("\"via\":\"r1\""), TEXT("\"via\":\"ghost\"")); }, TEXT("VOID.Meridian.UnresolvedReference") },
		{ TEXT("duplicate district id"),   [](FVoidMeridianFileSet& S) { return Edit(S, TEXT("DistrictRegistry.json"), TEXT("{\"id\":\"sector_0\""), TEXT("{\"id\":\"undercroft\"")); }, TEXT("VOID.Meridian.DuplicateId") },
		{ TEXT("district dependency cycle"), [](FVoidMeridianFileSet& S) { return Edit(S, TEXT("DistrictRegistry.json"), TEXT("\"dependencies\":{\"olympus_spire\":[]"), TEXT("\"dependencies\":{\"olympus_spire\":[\"sector_0\"]")); }, TEXT("VOID.Meridian.DependencyCycle") },
		{ TEXT("missing relationship pair"), [](FVoidMeridianFileSet& S) { return Edit(S, TEXT("DistrictRegistry.json"), TEXT("{\"pair\":[\"undercroft\",\"sector_0\"]}"), TEXT("{\"pair\":[\"undercroft\",\"undercroft\"]}")); }, TEXT("VOID.Meridian.MissingRelationshipPair") },
		{ TEXT("sixth district"),          [](FVoidMeridianFileSet& S) { return Edit(S, TEXT("DistrictRegistry.json"), TEXT("{\"id\":\"sector_0\",\"depends_on\""), TEXT("{\"id\":\"sixth\",\"depends_on\"")); }, TEXT("VOID.Meridian.DistrictSetMismatch") },
		{ TEXT("fabricated coordinates"),  [](FVoidMeridianFileSet& S) { return Edit(S, TEXT("RoadNetwork.json"), TEXT("{\"id\":\"r1\""), TEXT("{\"id\":\"r1\",\"centerline\":[[0,0],[9,9]]")); }, TEXT("VOID.Meridian.FabricatedGeometry") },
		{ TEXT("unsupported schema version"), [](FVoidMeridianFileSet& S) { return Edit(S, TEXT("RoadNetwork.json"), TEXT("void_road_network_schema_v1"), TEXT("void_road_network_schema_v2")); }, TEXT("VOID.Meridian.UnsupportedSchemaVersion") },
		{ TEXT("generation order violation"), [](FVoidMeridianFileSet& S) { return Edit(S, TEXT("Meridian_Master.json"), TEXT("\"target\":\"a\",\"depends_on\":[]"), TEXT("\"target\":\"a\",\"depends_on\":[\"b\"]")); }, TEXT("VOID.Meridian.GenerationOrderViolation") },
		{ TEXT("module dependency cycle"), [](FVoidMeridianFileSet& S) { return Edit(S, TEXT("Meridian_Master.json"), TEXT("\"module_dependencies\":{\"road_network\":[\"district_registry\"]}"), TEXT("\"module_dependencies\":{\"road_network\":[\"m2\"],\"m2\":[\"road_network\"]}")); }, TEXT("VOID.Meridian.DependencyCycle") },
		{ TEXT("import order violation"),  [](FVoidMeridianFileSet& S) { return Edit(S, TEXT("Meridian_Master.json"), TEXT("\"import_order\":[\"district_registry\",\"road_network\"]"), TEXT("\"import_order\":[\"road_network\",\"district_registry\"]")) && Edit(S, TEXT("Meridian_Master.json"), TEXT("\"module_dependencies\":{\"road_network\":[\"district_registry\"]}"), TEXT("\"module_dependencies\":{\"road_network\":[\"district_registry\"],\"district_registry\":[]}")); }, TEXT("VOID.Meridian.ImportOrderViolation") },
	};

	for (const FCase& C : Cases)
	{
		FVoidMeridianFileSet S = BaseSet();
		if (!C.Mutate(S)) { AddError(FString::Printf(TEXT("Test bug: mutation '%s' did not apply"), C.Name)); continue; }
		const FVoidValidationReport R = RunSet(S);
		TestTrue(*FString::Printf(TEXT("'%s' -> %s (ERROR/FATAL)"), C.Name, C.Code),
			VoidTest::HasCodeAt(R, C.Code, EVoidValidationSeverity::Error) || VoidTest::HasCodeAt(R, C.Code, EVoidValidationSeverity::Fatal));
		TestFalse(*FString::Printf(TEXT("'%s' invalidates the report"), C.Name), R.bIsValid);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidMeridianManifestTest, "VOID.WorldBuilder.Validation.Meridian.ManifestChecksumsAndInventory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidMeridianManifestTest::RunTest(const FString&)
{
	auto MakeWithManifest = [](const FString& MasterTextInManifest, const FString& ExtraLocked)
	{
		FVoidMeridianFileSet S = BaseSet();
		const FString Manifest = FString::Printf(TEXT("{\"locked_inputs\":[{\"file\":\"Meridian_Master.json\",\"sha256\":\"%s\"}%s],\"package_files\":[],\"totals\":{\"total_files\":99}}"), *Hex(MasterTextInManifest), *ExtraLocked);
		S.AddText(TEXT("PackageManifest.json"), Manifest);
		return S;
	};

	{
		const FVoidValidationReport R = RunSet(MakeWithManifest(MasterJson, FString()));
		TestFalse(TEXT("Matching checksum: no mismatch"), VoidTest::HasCode(R, TEXT("VOID.Meridian.ChecksumMismatch")));
		TestTrue(TEXT("Stale totals warned"), VoidTest::HasCodeAt(R, TEXT("VOID.Meridian.ManifestTotalsStale"), EVoidValidationSeverity::Warning));
		TestTrue(TEXT("Unlisted files warned"), VoidTest::HasCodeAt(R, TEXT("VOID.Meridian.UnlistedFile"), EVoidValidationSeverity::Warning));
	}
	{
		const FVoidValidationReport R = RunSet(MakeWithManifest(TEXT("different content"), FString()));
		TestTrue(TEXT("Wrong checksum is an ERROR"), VoidTest::HasCodeAt(R, TEXT("VOID.Meridian.ChecksumMismatch"), EVoidValidationSeverity::Error));
	}
	{
		// File on disk has CRLF, manifest was computed over LF: warning, not error.
		FVoidMeridianFileSet S = MakeWithManifest(FString(MasterJson) + TEXT("\n"), FString());
		S.AddText(TEXT("Meridian_Master.json"), FString(MasterJson) + TEXT("\r\n"));
		const FVoidValidationReport R = RunSet(S);
		TestTrue(TEXT("CRLF-only difference is a WARNING"), VoidTest::HasCodeAt(R, TEXT("VOID.Meridian.LineEndingChecksum"), EVoidValidationSeverity::Warning));
		TestFalse(TEXT("...and not an ERROR"), VoidTest::HasCode(R, TEXT("VOID.Meridian.ChecksumMismatch")));
	}
	{
		const FString Extra = TEXT(",{\"file\":\"White_Zones_data.json\",\"sha256\":\"00\"}");
		const FVoidValidationReport Strict = RunSet(MakeWithManifest(MasterJson, Extra));
		TestTrue(TEXT("Missing locked district file is an ERROR by default"), VoidTest::HasCodeAt(Strict, TEXT("VOID.Meridian.LockedInputMissing"), EVoidValidationSeverity::Error));

		FVoidValidationOptions Lenient;
		Lenient.bAllowMissingDistrictPackages = true;
		const FVoidValidationReport Soft = RunSet(MakeWithManifest(MasterJson, Extra), Lenient);
		TestTrue(TEXT("...downgraded to WARNING when allowed"), VoidTest::HasCodeAt(Soft, TEXT("VOID.Meridian.LockedInputMissing"), EVoidValidationSeverity::Warning));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidSha256Test, "VOID.WorldBuilder.Validation.Meridian.Sha256KnownVectors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidSha256Test::RunTest(const FString&)
{
	TestEqual(TEXT("SHA-256(empty)"), Hex(TEXT("")), FString(TEXT("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")));
	TestEqual(TEXT("SHA-256(abc)"), Hex(TEXT("abc")), FString(TEXT("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")));
	TestEqual(TEXT("SHA-256(448-bit message)"), Hex(TEXT("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")), FString(TEXT("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidSchemaSubsetTest, "VOID.WorldBuilder.Validation.Schema.SubsetKeywords",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidSchemaSubsetTest::RunTest(const FString&)
{
	auto Parse = [](const TCHAR* Json)
	{
		TSharedPtr<FJsonValue> V;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FString(Json)), V);
		return V;
	};
	const TSharedPtr<FJsonValue> SchemaValue = Parse(TEXT("{\"type\":\"object\",\"required\":[\"id\",\"n\"],\"additionalProperties\":false,\"properties\":{\"id\":{\"type\":\"string\",\"minLength\":1},\"n\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":10},\"kind\":{\"enum\":[\"a\",\"b\"]},\"v\":{\"const\":\"x\"},\"tags\":{\"type\":\"array\",\"minItems\":1,\"items\":{\"type\":\"string\"}}}}"));
	const TSharedPtr<FJsonObject> Schema = SchemaValue->AsObject();

	auto Codes = [&](const TCHAR* Instance)
	{
		FVoidValidationReport R; R.bIsValid = true;
		FVoidValidationOptions O;
		FVoidValidationContext Ctx(R, O, TEXT("Schema"));
		FVoidJsonSchemaSubset::Validate(Parse(Instance), Schema, TEXT("t.json"), Ctx);
		return R;
	};

	TestEqual(TEXT("Conforming instance: no violations"), Codes(TEXT("{\"id\":\"x\",\"n\":5,\"kind\":\"a\",\"v\":\"x\",\"tags\":[\"t\"]}")).Issues.Num(), 0);
	TestTrue(TEXT("Missing field"), VoidTest::HasCode(Codes(TEXT("{\"id\":\"x\"}")), TEXT("VOID.Schema.MissingField")));
	TestTrue(TEXT("Wrong type"), VoidTest::HasCode(Codes(TEXT("{\"id\":5,\"n\":5}")), TEXT("VOID.Schema.WrongType")));
	TestTrue(TEXT("Non-integer where integer required"), VoidTest::HasCode(Codes(TEXT("{\"id\":\"x\",\"n\":1.5}")), TEXT("VOID.Schema.WrongType")));
	TestTrue(TEXT("Below minimum"), VoidTest::HasCode(Codes(TEXT("{\"id\":\"x\",\"n\":0}")), TEXT("VOID.Schema.Range")));
	TestTrue(TEXT("Above maximum"), VoidTest::HasCode(Codes(TEXT("{\"id\":\"x\",\"n\":11}")), TEXT("VOID.Schema.Range")));
	TestTrue(TEXT("Unknown field"), VoidTest::HasCode(Codes(TEXT("{\"id\":\"x\",\"n\":5,\"zzz\":1}")), TEXT("VOID.Schema.UnknownField")));
	TestTrue(TEXT("Enum mismatch"), VoidTest::HasCode(Codes(TEXT("{\"id\":\"x\",\"n\":5,\"kind\":\"c\"}")), TEXT("VOID.Schema.EnumMismatch")));
	TestTrue(TEXT("Const mismatch"), VoidTest::HasCode(Codes(TEXT("{\"id\":\"x\",\"n\":5,\"v\":\"y\"}")), TEXT("VOID.Schema.ConstMismatch")));
	TestTrue(TEXT("Empty string"), VoidTest::HasCode(Codes(TEXT("{\"id\":\"\",\"n\":5}")), TEXT("VOID.Schema.MinLength")));
	TestTrue(TEXT("Empty required array"), VoidTest::HasCode(Codes(TEXT("{\"id\":\"x\",\"n\":5,\"tags\":[]}")), TEXT("VOID.Schema.ItemCount")));
	TestTrue(TEXT("Array item wrong type reports the index"), Codes(TEXT("{\"id\":\"x\",\"n\":5,\"tags\":[\"a\",3]}")).Issues.Last().FieldPath.Contains(TEXT("tags[1]")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
