// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Misc/AutomationTest.h"
#include "Data/VoidSchemaVersion.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidSchemaVersionParseTest,
	"VOID.WorldBuilder.Core.SchemaVersionParsesMajorMinor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidSchemaVersionParseTest::RunTest(const FString& Parameters)
{
	FVoidSchemaVersion Parsed;

	TestTrue(TEXT("\"1.0\" should parse"), FVoidSchemaVersion::TryParse(TEXT("1.0"), Parsed));
	TestEqual(TEXT("Major should be 1"), Parsed.Major, 1);
	TestEqual(TEXT("Minor should be 0"), Parsed.Minor, 0);
	TestEqual(TEXT("ToString should round-trip"), Parsed.ToString(), FString(TEXT("1.0")));

	TestTrue(TEXT("\"2.7\" should parse"), FVoidSchemaVersion::TryParse(TEXT("2.7"), Parsed));
	TestEqual(TEXT("Major should be 2"), Parsed.Major, 2);
	TestEqual(TEXT("Minor should be 7"), Parsed.Minor, 7);

	FVoidSchemaVersion Unchanged(9, 9);
	TestFalse(TEXT("Malformed version string should fail to parse"), FVoidSchemaVersion::TryParse(TEXT("not-a-version"), Unchanged));
	TestEqual(TEXT("Failed parse should not modify OutVersion"), Unchanged.Major, 9);

	TestFalse(TEXT("Version with only a major component should fail to parse"), FVoidSchemaVersion::TryParse(TEXT("1"), Unchanged));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidSchemaVersionCurrentToolVersionTest,
	"VOID.WorldBuilder.Core.SchemaVersionCurrentToolVersionIsStable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidSchemaVersionCurrentToolVersionTest::RunTest(const FString& Parameters)
{
	const FVoidSchemaVersion& FirstCall = FVoidSchemaVersion::CurrentToolVersion();
	const FVoidSchemaVersion& SecondCall = FVoidSchemaVersion::CurrentToolVersion();

	TestTrue(TEXT("CurrentToolVersion should be stable across calls"), FirstCall == SecondCall);
	TestEqual(TEXT("Current tool major version should be 1"), FirstCall.Major, 1);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
