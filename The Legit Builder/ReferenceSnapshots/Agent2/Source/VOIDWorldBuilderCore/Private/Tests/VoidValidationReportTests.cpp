// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Misc/AutomationTest.h"
#include "Data/VoidValidationReport.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidValidationReportTest,
	"VOID.WorldBuilder.Core.ValidationReportTracksErrorsAndWarnings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidValidationReportTest::RunTest(const FString& Parameters)
{
	FVoidValidationReport Report;
	Report.bIsValid = true;

	TestTrue(TEXT("Freshly constructed report with no issues should be valid"), Report.bIsValid);

	Report.AddWarning(TEXT("This is a warning."));
	TestTrue(TEXT("A warning alone should not invalidate the report"), Report.bIsValid);
	TestEqual(TEXT("Warning count should be 1"), Report.NumWarnings(), 1);

	Report.AddError(TEXT("This is an error."));
	TestFalse(TEXT("An error should invalidate the report"), Report.bIsValid);
	TestEqual(TEXT("Error count should be 1"), Report.NumErrors(), 1);
	TestEqual(TEXT("Total issue count should be 2"), Report.Issues.Num(), 2);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
