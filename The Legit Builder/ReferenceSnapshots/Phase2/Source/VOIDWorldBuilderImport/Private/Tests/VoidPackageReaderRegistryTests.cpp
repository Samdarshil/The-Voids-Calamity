// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Misc/AutomationTest.h"
#include "VoidDesignPackageImporter.h"

#if WITH_DEV_AUTOMATION_TESTS

// Note: FVoidPackageReaderRegistry itself is exercised indirectly here,
// through FVoidDesignPackageImporter::LoadFromFile, rather than directly
// -- this matches how every real caller (Editor panel, commandlet, future
// generators) actually uses it, and confirms the registry is correctly
// populated by VOIDWorldBuilderImport's module startup in a running
// Editor/commandlet process rather than only in isolation.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidPackageReaderRegistryUnsupportedExtensionTest,
	"VOID.WorldBuilder.Import.UnsupportedFileExtensionFailsGracefully",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidPackageReaderRegistryUnsupportedExtensionTest::RunTest(const FString& Parameters)
{
	// No .yaml reader is registered in Phase 2. This must fail gracefully
	// with a Fatal validation issue, not crash or silently return an
	// empty "successful" result.
	const FVoidImportResult Result = FVoidDesignPackageImporter::LoadFromFile(TEXT("C:/nonexistent/district.yaml"));

	TestFalse(TEXT("An unsupported extension should not report success"), Result.WasSuccessful());
	TestTrue(TEXT("An unsupported extension should report at least one Fatal issue"), Result.ValidationReport.NumFatal() > 0);

	bool bFoundUnsupportedFormatCode = false;
	for (const FVoidValidationIssue& Issue : Result.ValidationReport.Issues)
	{
		if (Issue.ErrorCode == FName(TEXT("VOID.Import.UnsupportedFormat")))
		{
			bFoundUnsupportedFormatCode = true;
			break;
		}
	}
	TestTrue(TEXT("The Fatal issue should carry the VOID.Import.UnsupportedFormat error code"), bFoundUnsupportedFormatCode);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidPackageReaderRegistryUnreadableFileTest,
	"VOID.WorldBuilder.Import.UnreadableFileFailsGracefully",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidPackageReaderRegistryUnreadableFileTest::RunTest(const FString& Parameters)
{
	// A .json path that simply doesn't exist on disk -- the registry
	// correctly resolves a reader for the extension, but the reader
	// itself must fail gracefully when the file can't be opened.
	const FVoidImportResult Result = FVoidDesignPackageImporter::LoadFromFile(TEXT("C:/definitely/does/not/exist/district.json"));

	TestFalse(TEXT("A missing file should not report success"), Result.WasSuccessful());
	TestTrue(TEXT("A missing file should report at least one Fatal issue"), Result.ValidationReport.NumFatal() > 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
