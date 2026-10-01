// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Misc/AutomationTest.h"
#include "VoidLightingConfig.h"
#include "VoidLightingMetadata.h"
#include "VoidLightingTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidLightingShippedConfigLoadsTest,
	"VOID.WorldBuilder.Lighting.Config.ShippedJsonLoadsAndHasAllPresets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidLightingShippedConfigLoadsTest::RunTest(const FString& Parameters)
{
	FVoidLightingConfig& Config = FVoidLightingConfig::Get();
	TestTrue(TEXT("Both JSON files load from the plugin Config folder"), Config.Reload());

	const TArray<FName> Required = { FName(TEXT("Day")), FName(TEXT("GoldenHour")), FName(TEXT("Night")), FName(TEXT("Overcast")) };
	for (const FName& Name : Required)
	{
		FVoidLightingPreset P;
		TestTrue(FString::Printf(TEXT("Preset %s exists"), *Name.ToString()), Config.FindPreset(Name, P));
	}

	FVoidLightingPreset Night, Day;
	Config.FindPreset(TEXT("Night"), Night);
	Config.FindPreset(TEXT("Day"), Day);
	TestTrue(TEXT("Night enables volumetric fog"), Night.Fog.bVolumetric);
	TestFalse(TEXT("Day keeps volumetric fog off (cost)"), Day.Fog.bVolumetric);
	TestTrue(TEXT("Night has emissive street lamps, Day does not"), Night.Local.StreetLampEmissive > 0.0f && Day.Local.StreetLampEmissive <= 0.0f);
	TestTrue(TEXT("Night sun is off, moon is on"), Night.Sun.IntensityLux <= 0.0f && Night.Moon.bEnabled);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidLightingRoadHierarchyTest,
	"VOID.WorldBuilder.Lighting.Config.RoadRulesRespectHierarchy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidLightingRoadHierarchyTest::RunTest(const FString& Parameters)
{
	FVoidLightingConfig& Config = FVoidLightingConfig::Get();
	Config.Reload();

	const FVoidRoadLightingRule Highway = Config.GetRoadRule(EVoidRoadType::Highway);
	const FVoidRoadLightingRule Primary = Config.GetRoadRule(EVoidRoadType::Primary);
	const FVoidRoadLightingRule Local = Config.GetRoadRule(EVoidRoadType::Local);
	const FVoidRoadLightingRule Alley = Config.GetRoadRule(EVoidRoadType::Alley);

	TestTrue(TEXT("Highway is denser than Primary"), Highway.SpacingUnits < Primary.SpacingUnits);
	TestTrue(TEXT("Primary is denser than Local"), Primary.SpacingUnits < Local.SpacingUnits);
	TestTrue(TEXT("Local is denser than Alley"), Local.SpacingUnits < Alley.SpacingUnits);
	TestTrue(TEXT("Higher classes have higher real-light priority"), Highway.Priority > Local.Priority && Local.Priority > Alley.Priority);
	TestEqual(TEXT("Alleys get no real lights"), Alley.RealLightEveryN, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidLightingClassifyTest,
	"VOID.WorldBuilder.Lighting.Config.BuildingClassification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidLightingClassifyTest::RunTest(const FString& Parameters)
{
	FVoidLightingConfig& Config = FVoidLightingConfig::Get();
	Config.Reload();

	bool bMatched = false;
	TestTrue(TEXT("Residential_MidTier"), Config.ClassifyBuilding(TEXT("Residential_MidTier"), &bMatched).Usage == EVoidLightingUsage::Residential);
	TestTrue(TEXT("matched"), bMatched);
	TestTrue(TEXT("CivicCenter"), Config.ClassifyBuilding(TEXT("CivicCenter"), &bMatched).Usage == EVoidLightingUsage::Civic);
	TestTrue(TEXT("elite_residential_towers is residential, not commercial (order matters)"), Config.ClassifyBuilding(TEXT("elite_residential_towers"), &bMatched).Usage == EVoidLightingUsage::Residential);
	TestTrue(TEXT("Retail_Strip"), Config.ClassifyBuilding(TEXT("Retail_Strip"), &bMatched).Usage == EVoidLightingUsage::Commercial);

	Config.ClassifyBuilding(TEXT("zzz_unknown_type"), &bMatched);
	TestFalse(TEXT("unknown type reports unmatched"), bMatched);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidLightingDistrictProfileTest,
	"VOID.WorldBuilder.Lighting.Config.DistrictProfilesFollowMeridianSpectrum",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidLightingDistrictProfileTest::RunTest(const FString& Parameters)
{
	FVoidLightingConfig& Config = FVoidLightingConfig::Get();
	Config.Reload();

	bool bMatched = false;
	const FVoidDistrictLightingProfile Spire = Config.FindDistrictProfile(TEXT("olympus_spire"), &bMatched);
	TestTrue(TEXT("Spire profile found"), bMatched);
	const FVoidDistrictLightingProfile White = Config.FindDistrictProfile(TEXT("white_zones"), &bMatched);
	const FVoidDistrictLightingProfile Archives = Config.FindDistrictProfile(TEXT("metro_archives"), &bMatched);
	const FVoidDistrictLightingProfile Under = Config.FindDistrictProfile(TEXT("undercroft"), &bMatched);
	const FVoidDistrictLightingProfile S0 = Config.FindDistrictProfile(TEXT("sector_0"), &bMatched);

	TestTrue(TEXT("Spire is the warmest street light"), Spire.StreetLampTempB < White.StreetLampTempA);
	TestTrue(TEXT("White Zones are uniform (narrow colour pair)"), FMath::Abs(White.StreetLampTempB - White.StreetLampTempA) < 1000.0f);
	TestTrue(TEXT("Metro Archives mix warm and cold (wide pair)"), Archives.StreetLampTempB - Archives.StreetLampTempA > 2000.0f);
	TestTrue(TEXT("Undercroft is below grade with neon"), Under.bBelowGrade && Under.NeonDensity > 0.0f);
	TestTrue(TEXT("Sector 0 is below grade with four provisional states"), S0.bBelowGrade && S0.States.Num() == 4 && S0.States[0].bProvisional);
	TestEqual(TEXT("Spire has no neon"), Spire.NeonDensity, 0.0f);

	const FVoidDistrictLightingProfile Unknown = Config.FindDistrictProfile(TEXT("nope"), &bMatched);
	TestFalse(TEXT("Unknown district falls back without matching"), bMatched);
	TestTrue(TEXT("Fallback is the neutral default profile"), Unknown.Id == FName(TEXT("default")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidLightingMetadataParseTest,
	"VOID.WorldBuilder.Lighting.Metadata.RegistryParsing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidLightingMetadataParseTest::RunTest(const FString& Parameters)
{
	FVoidMeridianLightingMetadata Meta;

	const FString Districts = TEXT("{\"districts\":[{\"id\":\"undercroft\",\"display_name\":\"The Undercroft\",\"vertical_tier\":\"below_grade_living\",\"generation_priority\":4},{\"id\":\"olympus_spire\",\"vertical_tier\":\"absolute_top\"}]}");
	TestTrue(TEXT("district registry parses"), Meta.ParseDistrictRegistry(Districts));
	TestEqual(TEXT("two districts"), Meta.GetDistricts().Num(), 2);
	TestTrue(TEXT("undercroft below grade"), Meta.FindDistrict(TEXT("undercroft"))->IsBelowGrade());
	TestFalse(TEXT("spire above grade"), Meta.FindDistrict(TEXT("olympus_spire"))->IsBelowGrade());

	const FString Landmarks = TEXT("{\"landmarks\":[{\"id\":\"olympus_spire_silhouette\",\"district\":\"olympus_spire\",\"visibility_tier\":1},{\"id\":\"atrium_of_perfect_memory\",\"district\":\"olympus_spire\",\"visibility_tier\":\"interior_not_applicable_to_skyline\"}]}");
	TestTrue(TEXT("landmark registry parses"), Meta.ParseLandmarkRegistry(Landmarks));
	TestEqual(TEXT("Spire tier is 1"), Meta.FindLandmark(TEXT("olympus_spire_silhouette"))->VisibilityTier, 1);
	TestFalse(TEXT("Interior landmark has no skyline presence"), Meta.FindLandmark(TEXT("atrium_of_perfect_memory"))->HasSkylinePresence());
	TestNull(TEXT("Unregistered landmark is never invented"), Meta.FindLandmark(TEXT("made_up")));

	TestFalse(TEXT("Malformed JSON is rejected"), Meta.ParseLandmarkRegistry(TEXT("{not json")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
