// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Misc/AutomationTest.h"
#include "Road/VoidRoadValidator.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidRoadValidatorBridgeTunnelConflictTest,
	"VOID.WorldBuilder.RoadGenerator.Validator.BridgeAndTunnelTogetherIsRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidRoadValidatorBridgeTunnelConflictTest::RunTest(const FString& Parameters)
{
	FVoidDistrictData District;
	District.DistrictId = FVoidElementId(FName(TEXT("district_1")));

	FVoidRoadSpec Road;
	Road.Id = FVoidElementId(FName(TEXT("road_1")));
	Road.CenterlinePoints = { FVector2D(0, 0), FVector2D(1000, 0) };
	Road.bIsBridge = true;
	Road.bIsTunnel = true;
	District.Roads.Add(Road);

	const FVoidValidationReport Report = FVoidRoadValidator::Validate(District);

	TestFalse(TEXT("A road flagged as both bridge and tunnel should fail validation"), Report.bIsValid);
	TestTrue(TEXT("Should report at least one error"), Report.NumErrors() > 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidRoadValidatorUnresolvableConnectionTest,
	"VOID.WorldBuilder.RoadGenerator.Validator.UnresolvableConnectionIdIsRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidRoadValidatorUnresolvableConnectionTest::RunTest(const FString& Parameters)
{
	FVoidDistrictData District;
	District.DistrictId = FVoidElementId(FName(TEXT("district_1")));

	FVoidRoadSpec Road;
	Road.Id = FVoidElementId(FName(TEXT("road_1")));
	Road.CenterlinePoints = { FVector2D(0, 0), FVector2D(1000, 0) };
	Road.ConnectionIds.Add(FVoidElementId(FName(TEXT("road_that_does_not_exist"))));
	District.Roads.Add(Road);

	const FVoidValidationReport Report = FVoidRoadValidator::Validate(District);

	TestFalse(TEXT("An unresolvable connectionId should fail validation"), Report.bIsValid);
	TestTrue(TEXT("Should report at least one error"), Report.NumErrors() > 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidRoadValidatorValidConnectionTest,
	"VOID.WorldBuilder.RoadGenerator.Validator.ConnectionToRealRoadPasses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidRoadValidatorValidConnectionTest::RunTest(const FString& Parameters)
{
	FVoidDistrictData District;
	District.DistrictId = FVoidElementId(FName(TEXT("district_1")));

	FVoidRoadSpec RoadA;
	RoadA.Id = FVoidElementId(FName(TEXT("road_a")));
	RoadA.CenterlinePoints = { FVector2D(0, 0), FVector2D(1000, 0) };
	RoadA.ConnectionIds.Add(FVoidElementId(FName(TEXT("road_b"))));
	District.Roads.Add(RoadA);

	FVoidRoadSpec RoadB;
	RoadB.Id = FVoidElementId(FName(TEXT("road_b")));
	RoadB.CenterlinePoints = { FVector2D(1000, 0), FVector2D(2000, 0) };
	District.Roads.Add(RoadB);

	const FVoidValidationReport Report = FVoidRoadValidator::Validate(District);

	TestTrue(TEXT("A connectionId that resolves to a real road should pass validation"), Report.bIsValid);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidRoadValidatorInvalidRoundaboutTest,
	"VOID.WorldBuilder.RoadGenerator.Validator.RoundaboutWithoutRadiusIsRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidRoadValidatorInvalidRoundaboutTest::RunTest(const FString& Parameters)
{
	FVoidDistrictData District;
	District.DistrictId = FVoidElementId(FName(TEXT("district_1")));

	FVoidRoadSpec Roundabout;
	Roundabout.Id = FVoidElementId(FName(TEXT("roundabout_1")));
	Roundabout.RoadType = EVoidRoadType::Roundabout;
	Roundabout.CenterlinePoints = { FVector2D(0, 0) };
	// RoundaboutRadiusUnits left at 0 -- invalid.
	District.Roads.Add(Roundabout);

	const FVoidValidationReport Report = FVoidRoadValidator::Validate(District);

	TestFalse(TEXT("A roundabout without a positive radius should fail validation"), Report.bIsValid);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
