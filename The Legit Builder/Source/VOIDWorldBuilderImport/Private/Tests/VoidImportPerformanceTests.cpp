// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Misc/AutomationTest.h"
#include "VoidDesignPackageImporter.h"
#include "HAL/PlatformTime.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace VoidImportPerformanceTestPrivate
{
	static FString BuildSyntheticPackageJson(int32 NumBuildings, int32 NumRoads)
	{
		FString Json = TEXT("{ \"schemaVersion\": \"1.0\", \"metadata\": { \"sourceDocumentName\": \"Perf Test\", \"sourceDocumentVersion\": \"1.0\", \"isApproved\": true }, \"district\": { \"districtId\": \"perf_district\", \"buildings\": [");

		for (int32 Index = 0; Index < NumBuildings; ++Index)
		{
			if (Index > 0)
			{
				Json += TEXT(",");
			}
			Json += FString::Printf(
				TEXT("{ \"id\": \"bldg_%d\", \"heightUnits\": 300.0, \"buildingType\": \"Residential\", \"footprintCorners\": [[0,0],[0,10],[10,10],[10,0]] }"),
				Index);
		}

		Json += TEXT("], \"roads\": [");

		for (int32 Index = 0; Index < NumRoads; ++Index)
		{
			if (Index > 0)
			{
				Json += TEXT(",");
			}
			Json += FString::Printf(
				TEXT("{ \"id\": \"road_%d\", \"widthUnits\": 8.0, \"centerlinePoints\": [[0,0],[100,0]] }"),
				Index);
		}

		Json += TEXT("] } }");
		return Json;
	}
}

/**
 * This is a smoke-level regression guard, not a benchmark suite: it
 * asserts import of a moderately-sized synthetic package (200 buildings,
 * 50 roads) completes within a generous time budget. It exists to catch
 * an accidental O(n^2) regression (e.g. a future change that re-scans
 * the whole issue list per element), not to characterize real-world
 * performance -- a proper profiling pass belongs to whichever phase
 * first imports packages at genuine city scale.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidImportPerformanceSmokeTest,
	"VOID.WorldBuilder.Import.ModeratelySizedPackageImportsWithinBudget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::PerfFilter)

bool FVoidImportPerformanceSmokeTest::RunTest(const FString& Parameters)
{
	using namespace VoidImportPerformanceTestPrivate;

	const FString SyntheticJson = BuildSyntheticPackageJson(200, 50);

	const double StartSeconds = FPlatformTime::Seconds();
	const FVoidImportResult Result = FVoidDesignPackageImporter::LoadFromJsonString(SyntheticJson);
	const double ElapsedMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;

	TestTrue(TEXT("A well-formed synthetic package of this size should validate successfully"), Result.WasSuccessful());
	TestEqual(TEXT("All 200 buildings should be mapped"), Result.Package.District.Buildings.Num(), 200);
	TestEqual(TEXT("All 50 roads should be mapped"), Result.Package.District.Roads.Num(), 50);

	// Generous budget: this should take low milliseconds on any dev
	// machine; 500ms is a wide margin specifically to keep this test
	// stable in CI, not a claim about expected real-world performance.
	TestTrue(FString::Printf(TEXT("Import should complete within 500ms (took %.2fms)"), ElapsedMs), ElapsedMs < 500.0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
