// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Misc/AutomationTest.h"
#include "VoidDesignPackageImporter.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidImporterValidPackageTest,
	"VOID.WorldBuilder.Import.ValidPackageParsesAndValidates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidImporterValidPackageTest::RunTest(const FString& Parameters)
{
	const FString ValidJson = TEXT(R"JSON(
	{
		"districtId": "district_04_white_zone",
		"metadata": {
			"sourceDocumentName": "Meridian District 04",
			"sourceDocumentVersion": "1.0",
			"isApproved": true
		},
		"buildings": [
			{
				"id": "bldg_01",
				"heightUnits": 500.0,
				"buildingType": "CivicCenter",
				"footprintCorners": [[0,0],[0,100],[100,100],[100,0]]
			}
		],
		"roads": [
			{
				"id": "road_01",
				"widthUnits": 12.0,
				"centerlinePoints": [[0,0],[500,0]]
			}
		]
	}
	)JSON");

	const FVoidImportResult Result = FVoidDesignPackageImporter::LoadFromJsonString(ValidJson);

	TestTrue(TEXT("Valid package should pass validation"), Result.WasSuccessful());
	TestEqual(TEXT("District id should be mapped"), Result.Package.DistrictId.Value, FName(TEXT("district_04_white_zone")));
	TestEqual(TEXT("One building should be mapped"), Result.Package.Buildings.Num(), 1);
	TestEqual(TEXT("One road should be mapped"), Result.Package.Roads.Num(), 1);
	TestEqual(TEXT("Valid package should have zero errors"), Result.ValidationReport.NumErrors(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidImporterUnapprovedPackageTest,
	"VOID.WorldBuilder.Import.UnapprovedPackageFailsValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidImporterUnapprovedPackageTest::RunTest(const FString& Parameters)
{
	const FString UnapprovedJson = TEXT(R"JSON(
	{
		"districtId": "district_05_draft",
		"metadata": {
			"sourceDocumentName": "Meridian District 05 (Draft)",
			"sourceDocumentVersion": "0.1",
			"isApproved": false
		},
		"buildings": [],
		"roads": []
	}
	)JSON");

	const FVoidImportResult Result = FVoidDesignPackageImporter::LoadFromJsonString(UnapprovedJson);

	TestFalse(TEXT("Unapproved package should fail validation"), Result.WasSuccessful());
	TestTrue(TEXT("Unapproved package should report at least one error"), Result.ValidationReport.NumErrors() > 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidImporterMalformedJsonTest,
	"VOID.WorldBuilder.Import.MalformedJsonFailsGracefully",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidImporterMalformedJsonTest::RunTest(const FString& Parameters)
{
	const FString MalformedJson = TEXT("{ this is not valid json ");

	const FVoidImportResult Result = FVoidDesignPackageImporter::LoadFromJsonString(MalformedJson);

	TestFalse(TEXT("Malformed JSON should fail validation, not crash"), Result.WasSuccessful());
	TestTrue(TEXT("Malformed JSON should report at least one error"), Result.ValidationReport.NumErrors() > 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidValidatorDuplicateIdTest,
	"VOID.WorldBuilder.Import.DuplicateIdsAreRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidValidatorDuplicateIdTest::RunTest(const FString& Parameters)
{
	const FString DuplicateIdJson = TEXT(R"JSON(
	{
		"districtId": "district_06",
		"metadata": {
			"sourceDocumentName": "Meridian District 06",
			"sourceDocumentVersion": "1.0",
			"isApproved": true
		},
		"buildings": [
			{ "id": "shared_id", "heightUnits": 100.0, "buildingType": "Residential", "footprintCorners": [[0,0],[0,10],[10,10]] },
			{ "id": "shared_id", "heightUnits": 100.0, "buildingType": "Residential", "footprintCorners": [[20,0],[20,10],[30,10]] }
		],
		"roads": []
	}
	)JSON");

	const FVoidImportResult Result = FVoidDesignPackageImporter::LoadFromJsonString(DuplicateIdJson);

	TestFalse(TEXT("Package with duplicate ids should fail validation"), Result.WasSuccessful());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
