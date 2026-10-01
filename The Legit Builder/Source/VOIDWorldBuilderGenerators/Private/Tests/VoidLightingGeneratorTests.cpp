// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Misc/AutomationTest.h"
#include "Lighting/VoidLightingGenerator.h"
#include "VoidLightingDirector.h"
#include "VoidDistrictLightingActor.h"
#include "VoidLandmarkLightingActor.h"
#include "VoidLightingSettings.h"
#include "VoidLightingConfig.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/DirectionalLight.h"
#include "Components/LightComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace VoidLightingTestPrivate
{
	static FVoidDesignPackage MakePackage()
	{
		FVoidDesignPackage P;
		P.District.DistrictId = FVoidElementId(FName(TEXT("white_zones")));

		FVoidRoadSpec Main;
		Main.Id = FVoidElementId(FName(TEXT("main")));
		Main.RoadType = EVoidRoadType::Primary;
		Main.CenterlinePoints = { FVector2D(-6000, 0), FVector2D(6000, 0) };
		Main.bHasSidewalk = true;
		P.District.Roads.Add(Main);

		FVoidRoadSpec Cross;
		Cross.Id = FVoidElementId(FName(TEXT("cross")));
		Cross.RoadType = EVoidRoadType::Secondary;
		Cross.CenterlinePoints = { FVector2D(0, -6000), FVector2D(0, 0) };
		Cross.bHasSidewalk = true;
		P.District.Roads.Add(Cross);

		FVoidRoadSpec Tunnel;
		Tunnel.Id = FVoidElementId(FName(TEXT("tunnel")));
		Tunnel.RoadType = EVoidRoadType::Secondary;
		Tunnel.CenterlinePoints = { FVector2D(0, 0), FVector2D(0, 6000) };
		Tunnel.bIsTunnel = true;
		P.District.Roads.Add(Tunnel);

		auto AddBuilding = [&P](const TCHAR* Id, const TCHAR* Type, float X, float Y, float W, float H)
		{
			FVoidBuildingSpec B;
			B.Id = FVoidElementId(FName(Id));
			B.BuildingType = Type;
			B.HeightUnits = H;
			B.FootprintCorners = { FVector2D(X, Y), FVector2D(X + W, Y), FVector2D(X + W, Y + W), FVector2D(X, Y + W) };
			P.District.Buildings.Add(B);
		};
		AddBuilding(TEXT("b_res"), TEXT("Residential_MidTier"), 800, 800, 1500, 3000);
		AddBuilding(TEXT("b_shop"), TEXT("Retail_Strip"), -2400, 600, 1200, 1200);
		AddBuilding(TEXT("b_landmark"), TEXT("CivicCenter"), 3000, -3000, 2000, 9000);
		return P;
	}

	static UWorld* MakeWorld()
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld=*/false);
		return World;
	}

	static void Destroy(UWorld* World)
	{
		if (World)
		{
			World->DestroyWorld(/*bInformEngineOfWorld=*/false);
		}
	}

	static int32 CountActors(UWorld* World)
	{
		int32 N = 0;
		for (TActorIterator<AVoidDistrictLightingActor> It(World); It; ++It) { ++N; }
		for (TActorIterator<AVoidLandmarkLightingActor> It(World); It; ++It) { ++N; }
		return N;
	}
}

// NOTE: these tests need the editor (material asset creation, plugin Config folder). They were written but NOT executed
// by the author -- see HANDOFF_AGENT7_LIGHTING.md, "Verification status".

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoidLightingGenerateAndSwitchTest,
	"VOID.WorldBuilder.Lighting.Generator.GeneratesAndSwitchesPresets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FVoidLightingGenerateAndSwitchTest::RunTest(const FString& Parameters)
{
	using namespace VoidLightingTestPrivate;

	UVoidLightingSettings* Settings = GetMutableDefault<UVoidLightingSettings>();
	const FString OldMaterialPath = Settings->GeneratedMaterialPath;
	const TArray<FVoidLandmarkBinding> OldBindings = Settings->LandmarkBindings;
	Settings->GeneratedMaterialPath = TEXT("/Engine/Transient/VOIDLightingTest_M");
	FVoidLandmarkBinding Bind;
	Bind.LandmarkId = TEXT("test_landmark");
	Bind.BuildingId = TEXT("b_landmark");
	Settings->LandmarkBindings = { Bind };

	UWorld* World = MakeWorld();
	if (!TestNotNull(TEXT("Test world created"), World))
	{
		return false;
	}

	FVoidLightingGenerator Generator;
	FVoidGenerationContext Ctx;
	Ctx.TargetWorld = World;
	const FVoidDesignPackage Package = MakePackage();

	const bool bOk = Generator.Generate(Package, Ctx);
	TestTrue(TEXT("Generate succeeds"), bOk);
	TestEqual(TEXT("No fatal issues"), Ctx.GenerationValidationReport.NumFatal(), 0);

	AVoidLightingDirector* Director = AVoidLightingDirector::FindDirector(World);
	if (TestNotNull(TEXT("Director spawned"), Director))
	{
		TestEqual(TEXT("Four presets in the library"), Director->PresetLibrary.Num(), 4);
		TestEqual(TEXT("One district actor"), Director->DistrictActors.Num(), 1);
		TestEqual(TEXT("One landmark actor (bound explicitly)"), Director->LandmarkActors.Num(), 1);
		TestTrue(TEXT("World bounds valid"), Director->WorldBounds.IsValid != 0);
		TestTrue(TEXT("Has a landmark anchor"), Director->GetAnchorsOfType(EVoidAnchorType::Landmark).Num() == 1);
		TestTrue(TEXT("Has a district-centre anchor"), Director->GetAnchorsOfType(EVoidAnchorType::DistrictCenter).Num() == 1);
		TestTrue(TEXT("Has a major-road anchor"), Director->GetAnchorsOfType(EVoidAnchorType::MajorRoad).Num() >= 1);
		TestTrue(TEXT("Has skyline points"), Director->GetAnchorsOfType(EVoidAnchorType::SkylinePoint).Num() >= 1);

		if (Director->DistrictActors.Num() == 1)
		{
			AVoidDistrictLightingActor* D = Director->DistrictActors[0];
			TestTrue(TEXT("Has poles"), D->GetElementInstanceCount(EVoidLightingElement::StreetPole) > 0);
			TestTrue(TEXT("Has windows"), D->GetElementInstanceCount(EVoidLightingElement::WindowResidential) + D->GetElementInstanceCount(EVoidLightingElement::WindowCommercial) > 0);
			TestTrue(TEXT("Tunnel portals got emergency beacons"), D->GetElementInstanceCount(EVoidLightingElement::EmergencyBeacon) == 2);
			TestTrue(TEXT("Real lights stay within budget"), D->GetNumRealLights() <= Settings->MaxRealStreetLightsPerDistrict);
		}

		// Preset switching changes the sun.
		TestTrue(TEXT("Apply Day"), FVoidLightingGenerator::ApplyPreset(World, TEXT("Day")));
		ADirectionalLight* Sun = Director->Sun;
		if (TestNotNull(TEXT("Sun exists"), Sun))
		{
			const float DayLux = Sun->GetLightComponent()->Intensity;
			TestTrue(TEXT("Apply Night"), FVoidLightingGenerator::ApplyPreset(World, TEXT("Night")));
			TestTrue(TEXT("Night sun is dimmer than day sun"), Sun->GetLightComponent()->Intensity < DayLux);
		}
		TestFalse(TEXT("Unknown preset id is rejected"), FVoidLightingGenerator::ApplyPreset(World, TEXT("Nope")));
	}

	// Regeneration replaces, never duplicates.
	const int32 Before = CountActors(World);
	FVoidGenerationContext Ctx2;
	Ctx2.TargetWorld = World;
	Generator.Generate(Package, Ctx2);
	TestEqual(TEXT("Regeneration keeps actor count stable"), CountActors(World), Before);

	// Clear removes them.
	FVoidLightingGenerator::ClearLighting(World, FName(TEXT("white_zones")));
	TestEqual(TEXT("ClearLighting removes district + landmark actors"), CountActors(World), 0);

	Destroy(World);
	Settings->GeneratedMaterialPath = OldMaterialPath;
	Settings->LandmarkBindings = OldBindings;
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
