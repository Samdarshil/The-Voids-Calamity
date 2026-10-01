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
		"schemaVersion": "1.0",
		"metadata": {
			"sourceDocumentName": "Meridian District 04",
			"sourceDocumentVersion": "1.0",
			"isApproved": true
		},
		"district": {
			"districtId": "district_04_white_zone",
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
	}
	)JSON");

	const FVoidImportResult Result = FVoidDesignPackageImporter::LoadFromJsonString(ValidJson);

	TestTrue(TEXT("Valid package should pass validation"), Result.WasSuccessful());
	TestEqual(TEXT("District id should be mapped"), Result.Package.District.DistrictId.Value, FName(TEXT("district_04_white_zone")));
	TestEqual(TEXT("One building should be mapped"), Result.Package.District.Buildings.Num(), 1);
	TestEqual(TEXT("One road should be mapped"), Result.Package.District.Roads.Num(), 1);
	TestEqual(TEXT("Valid package should have zero errors"), Result.ValidationReport.NumErrors(), 0);
	TestEqual(TEXT("Valid package should have zero fatal issues"), Result.ValidationReport.NumFatal(), 0);
	TestEqual(TEXT("Explicit schemaVersion 1.0 should be parsed, not defaulted"), Result.Package.SchemaVersion.ToString(), FString(TEXT("1.0")));

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
		"schemaVersion": "1.0",
		"metadata": {
			"sourceDocumentName": "Meridian District 05 (Draft)",
			"sourceDocumentVersion": "0.1",
			"isApproved": false
		},
		"district": {
			"districtId": "district_05_draft",
			"buildings": [],
			"roads": []
		}
	}
	)JSON");

	const FVoidImportResult Result = FVoidDesignPackageImporter::LoadFromJsonString(UnapprovedJson);

	TestFalse(TEXT("Unapproved package should fail validation"), Result.WasSuccessful());
	TestTrue(TEXT("Unapproved package should report at least one error"), Result.ValidationReport.NumErrors() > 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidImporterMalformedJsonTest,
	"VOID.WorldBuilder.Import.MalformedJsonIsFatalNotJustError",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidImporterMalformedJsonTest::RunTest(const FString& Parameters)
{
	const FString MalformedJson = TEXT("{ this is not valid json ");

	const FVoidImportResult Result = FVoidDesignPackageImporter::LoadFromJsonString(MalformedJson);

	TestFalse(TEXT("Malformed JSON should fail validation, not crash"), Result.WasSuccessful());
	TestTrue(TEXT("Malformed JSON should report at least one Fatal issue"), Result.ValidationReport.NumFatal() > 0);

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
		"schemaVersion": "1.0",
		"metadata": {
			"sourceDocumentName": "Meridian District 06",
			"sourceDocumentVersion": "1.0",
			"isApproved": true
		},
		"district": {
			"districtId": "district_06",
			"buildings": [
				{ "id": "shared_id", "heightUnits": 100.0, "buildingType": "Residential", "footprintCorners": [[0,0],[0,10],[10,10]] },
				{ "id": "shared_id", "heightUnits": 100.0, "buildingType": "Residential", "footprintCorners": [[20,0],[20,10],[30,10]] }
			],
			"roads": []
		}
	}
	)JSON");

	const FVoidImportResult Result = FVoidDesignPackageImporter::LoadFromJsonString(DuplicateIdJson);

	TestFalse(TEXT("Package with duplicate ids should fail validation"), Result.WasSuccessful());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidValidatorReservedDistrictIdTest,
	"VOID.WorldBuilder.Import.ElementIdCollidingWithDistrictIdIsRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidValidatorReservedDistrictIdTest::RunTest(const FString& Parameters)
{
	const FString CollidingIdJson = TEXT(R"JSON(
	{
		"schemaVersion": "1.0",
		"metadata": {
			"sourceDocumentName": "Meridian District 07",
			"sourceDocumentVersion": "1.0",
			"isApproved": true
		},
		"district": {
			"districtId": "district_07",
			"buildings": [
				{ "id": "district_07", "heightUnits": 100.0, "buildingType": "Residential", "footprintCorners": [[0,0],[0,10],[10,10]] }
			],
			"roads": []
		}
	}
	)JSON");

	const FVoidImportResult Result = FVoidDesignPackageImporter::LoadFromJsonString(CollidingIdJson);

	TestFalse(TEXT("A building id equal to the district id should be rejected as a collision"), Result.WasSuccessful());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidImporterSchemaVersionCompatibilityTest,
	"VOID.WorldBuilder.Import.SchemaVersionCompatibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidImporterSchemaVersionCompatibilityTest::RunTest(const FString& Parameters)
{
	// Newer minor version -> Warning, still succeeds.
	{
		const FString NewerMinorJson = TEXT(R"JSON(
		{
			"schemaVersion": "1.7",
			"metadata": { "sourceDocumentName": "Doc", "sourceDocumentVersion": "1.0", "isApproved": true },
			"district": { "districtId": "d1", "buildings": [], "roads": [] }
		}
		)JSON");

		const FVoidImportResult Result = FVoidDesignPackageImporter::LoadFromJsonString(NewerMinorJson);
		TestTrue(TEXT("A newer minor schema version should still validate successfully"), Result.WasSuccessful());
		TestTrue(TEXT("A newer minor schema version should produce at least one warning"), Result.ValidationReport.NumWarnings() > 0);
	}

	// Newer major version -> Fatal, fails.
	{
		const FString NewerMajorJson = TEXT(R"JSON(
		{
			"schemaVersion": "99.0",
			"metadata": { "sourceDocumentName": "Doc", "sourceDocumentVersion": "1.0", "isApproved": true },
			"district": { "districtId": "d1", "buildings": [], "roads": [] }
		}
		)JSON");

		const FVoidImportResult Result = FVoidDesignPackageImporter::LoadFromJsonString(NewerMajorJson);
		TestFalse(TEXT("A newer major schema version should fail"), Result.WasSuccessful());
		TestTrue(TEXT("A newer major schema version should be Fatal, not just an Error"), Result.ValidationReport.NumFatal() > 0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidImporterUnknownFieldTest,
	"VOID.WorldBuilder.Import.UnknownFieldsAreInfoByDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidImporterUnknownFieldTest::RunTest(const FString& Parameters)
{
	const FString UnknownFieldJson = TEXT(R"JSON(
	{
		"schemaVersion": "1.0",
		"_comment": "This annotation field must be silently ignored, not reported as unknown.",
		"metadata": { "sourceDocumentName": "Doc", "sourceDocumentVersion": "1.0", "isApproved": true },
		"district": { "districtId": "d1", "buildings": [], "roads": [], "totallyMadeUpField": 42 }
	}
	)JSON");

	const FVoidImportResult Result = FVoidDesignPackageImporter::LoadFromJsonString(UnknownFieldJson);

	TestTrue(TEXT("An unknown field should not block validation by default"), Result.WasSuccessful());
	TestTrue(TEXT("An unknown field should produce at least one Info issue by default"), Result.ValidationReport.NumInfo() > 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
