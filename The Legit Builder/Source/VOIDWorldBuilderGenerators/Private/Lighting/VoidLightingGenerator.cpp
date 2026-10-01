// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Lighting/VoidLightingGenerator.h"
#include "VoidLightingMaterialBuilder.h"
#include "VoidLightingConfig.h"
#include "VoidLightingSettings.h"
#include "VoidLightingMetadata.h"
#include "VoidLightingDirector.h"
#include "VoidDistrictLightingActor.h"
#include "VoidLandmarkLightingActor.h"
#include "VoidLightingLog.h"
#include "Road/VoidRoadTypeProfile.h"
#include "Road/VoidRoadGenerationSettings.h"
#include "Road/VoidRoadSplineBuilder.h"
#include "Road/VoidRoadBridgeTunnelBuilder.h"
#include "Road/VoidRoadIntersectionBuilder.h"
#include "Road/VoidRoadActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ScopedTransaction.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Algo/StableSort.h"
#include "Algo/Reverse.h"
#include "Engine/Light.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/SkyAtmosphere.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "GameFramework/WorldSettings.h"
#include "Components/LightComponent.h"
#include "Materials/MaterialInterface.h"

namespace VoidLightingGenPrivate
{
	constexpr float MinSpacing = 300.0f;

	struct FBuiltRoadInfo
	{
		FVoidRoadSpec Spec;
		TArray<FVector> Points;
		float HalfWidth = 300.0f;
		FVoidRoadTypeProfile Profile;
		bool bClosed = false;
		FVoidRoadLightingRule Rule;
	};

	struct FRealLightCandidate
	{
		FVector Location = FVector::ZeroVector;
		float Lumens = 0.0f;
		float Radius = 0.0f;
		int32 Priority = 0;
		bool bPoint = false;
		EVoidRealLightRole Role = EVoidRealLightRole::Street;
	};

	struct FBuildingInfo
	{
		const FVoidBuildingSpec* Spec = nullptr;
		TArray<FVector> Corners; // world XY at ground Z, wound CCW
		float Height = 0.0f;
		FVector Centroid = FVector::ZeroVector;
		float Radius = 0.0f;
		FName LandmarkId = NAME_None;
		int32 LandmarkTier = 0;
		bool bLandmarkTierProvisional = false;
	};

	static float ToWindingArea(const TArray<FVector2D>& P)
	{
		double A = 0.0;
		for (int32 i = 0; i < P.Num(); ++i)
		{
			const FVector2D& a = P[i];
			const FVector2D& b = P[(i + 1) % P.Num()];
			A += a.X * b.Y - b.X * a.Y;
		}
		return static_cast<float>(A * 0.5);
	}

	static FVector SideOf(const FVector& Tangent)
	{
		// Left of travel, XY plane.
		return FVector(-Tangent.Y, Tangent.X, 0.0f).GetSafeNormal();
	}

	static float YawOf(const FVector& Dir)
	{
		return FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
	}

	static float AvgKelvin(float A, float B) { return 0.5f * (A + B); }

	template <typename FnType>
	static void WalkPolyline(const TArray<FVector>& P, bool bClosed, float Spacing, FnType&& Fn)
	{
		const int32 N = P.Num();
		if (N < 2)
		{
			return;
		}
		Spacing = FMath::Max(Spacing, MinSpacing);
		float Next = Spacing * 0.5f;
		float Acc = 0.0f;
		int32 Index = 0;
		const int32 NumSeg = bClosed ? N : N - 1;
		for (int32 i = 0; i < NumSeg; ++i)
		{
			const FVector& A = P[i];
			const FVector& B = P[(i + 1) % N];
			const float L = FVector::Dist(A, B);
			if (L < KINDA_SMALL_NUMBER)
			{
				continue;
			}
			const FVector T = (B - A) / L;
			while (Next <= Acc + L)
			{
				const float t = (Next - Acc) / L;
				Fn(FMath::Lerp(A, B, t), T, Index++);
				Next += Spacing;
			}
			Acc += L;
		}
	}

	static float DistanceToRoads(const FVector& Point, const TArray<FBuiltRoadInfo>& Roads)
	{
		float Best = TNumericLimits<float>::Max();
		for (const FBuiltRoadInfo& R : Roads)
		{
			const int32 N = R.Points.Num();
			const int32 NumSeg = R.bClosed ? N : N - 1;
			for (int32 i = 0; i < NumSeg; ++i)
			{
				const FVector Closest = FMath::ClosestPointOnSegment(Point, R.Points[i], R.Points[(i + 1) % N]);
				Best = FMath::Min(Best, FVector::Dist2D(Closest, Point) - R.HalfWidth);
			}
		}
		return Best;
	}

	static FString LowerId(const FName& N) { return N.ToString().ToLower(); }

	static EVoidLightingElement WindowElementFor(EVoidLightingUsage Usage)
	{
		switch (Usage)
		{
		case EVoidLightingUsage::Residential: return EVoidLightingElement::WindowResidential;
		case EVoidLightingUsage::Commercial:  return EVoidLightingElement::WindowCommercial;
		case EVoidLightingUsage::Civic:       return EVoidLightingElement::WindowCivic;
		case EVoidLightingUsage::Industrial:  return EVoidLightingElement::WindowIndustrial;
		case EVoidLightingUsage::Landmark:    return EVoidLightingElement::WindowLandmark;
		default:                              return EVoidLightingElement::WindowCommercial;
		}
	}

	static FName MakeAnchorId(const FString& S) { return FName(*S); }
}

FName FVoidLightingGenerator::GetGeneratorId() const
{
	return FName(TEXT("Lighting"));
}

// ---------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------

bool FVoidLightingGenerator::ApplyPreset(UWorld* World, FName PresetId)
{
	AVoidLightingDirector* Director = AVoidLightingDirector::FindDirector(World);
	if (!Director)
	{
		UE_LOG(LogVoidLighting, Warning, TEXT("ApplyPreset('%s'): no AVoidLightingDirector in the world. Run the Lighting generator first."), *PresetId.ToString());
		return false;
	}
	Director->PreviewPresetId = PresetId;
	return Director->ApplyPresetById(PresetId);
}

int32 FVoidLightingGenerator::ClearLighting(UWorld* World, FName DistrictId, bool bRemoveDirector)
{
	if (!World)
	{
		return 0;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("VoidLightingGenerator", "ClearLighting", "Clear VOID Lighting"));
	int32 Destroyed = 0;
	AVoidLightingDirector* Director = AVoidLightingDirector::FindDirector(World);

	TArray<AActor*> ToDestroy;
	for (TActorIterator<AVoidDistrictLightingActor> It(World); It; ++It)
	{
		if (DistrictId == NAME_None || It->DistrictId == DistrictId) { ToDestroy.Add(*It); }
	}
	for (TActorIterator<AVoidLandmarkLightingActor> It(World); It; ++It)
	{
		if (DistrictId == NAME_None || It->DistrictId == DistrictId) { ToDestroy.Add(*It); }
	}

	if (Director)
	{
		Director->Modify();
		if (DistrictId == NAME_None) { Director->Anchors.Reset(); }
		else { Director->RemoveAnchorsForDistrict(DistrictId); }
	}

	for (AActor* Actor : ToDestroy)
	{
		if (World->DestroyActor(Actor)) { ++Destroyed; }
	}

	if (Director)
	{
		if (bRemoveDirector)
		{
			Director->RestoreOriginalLighting();
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				AActor* A = *It;
				if (A != Director && A->ActorHasTag(VoidLightingParams::GeneratedActorTag))
				{
					if (World->DestroyActor(A)) { ++Destroyed; }
				}
			}
			if (World->DestroyActor(Director)) { ++Destroyed; }
		}
		else
		{
			Director->RefreshRegistrations();
			Director->WorldBounds = FBox(ForceInit);
			for (AVoidDistrictLightingActor* D : Director->DistrictActors)
			{
				if (D) { Director->WorldBounds += D->DistrictBounds; }
			}
		}
	}

	UE_LOG(LogVoidLighting, Log, TEXT("ClearLighting(district=%s, removeDirector=%d): destroyed %d actors."), *DistrictId.ToString(), bRemoveDirector ? 1 : 0, Destroyed);
	return Destroyed;
}

void FVoidLightingGenerator::AuditWorld(UWorld* World, TArray<FString>& OutLines)
{
	if (!World)
	{
		OutLines.Add(TEXT("Audit: no world."));
		return;
	}

	int32 NumDir = 0, NumSky = 0, NumAtmo = 0, NumFog = 0, NumPost = 0, NumOtherLocal = 0;
	for (TActorIterator<ADirectionalLight> It(World); It; ++It) { ++NumDir; }
	for (TActorIterator<ASkyLight> It(World); It; ++It) { ++NumSky; }
	for (TActorIterator<ASkyAtmosphere> It(World); It; ++It) { ++NumAtmo; }
	for (TActorIterator<AExponentialHeightFog> It(World); It; ++It) { ++NumFog; }
	for (TActorIterator<APostProcessVolume> It(World); It; ++It) { ++NumPost; }
	for (TActorIterator<ALight> It(World); It; ++It)
	{
		if (!Cast<ADirectionalLight>(*It) && !Cast<ASkyLight>(*It)) { ++NumOtherLocal; }
	}
	int32 NumRoads = 0, NumJunctions = 0;
	for (TActorIterator<AVoidRoadActor> It(World); It; ++It) { ++NumRoads; }
	for (TActorIterator<AVoidRoadJunctionActor> It(World); It; ++It) { ++NumJunctions; }

	OutLines.Add(FString::Printf(TEXT("Audit: DirectionalLights=%d SkyLights=%d SkyAtmospheres=%d HeightFogs=%d PostProcessVolumes=%d OtherLightActors=%d"), NumDir, NumSky, NumAtmo, NumFog, NumPost, NumOtherLocal));
	OutLines.Add(FString::Printf(TEXT("Audit: generated road actors=%d, junction actors=%d (buildings/props/environment are not detectable: no generator for them exists in this project)"), NumRoads, NumJunctions));

	if (const AWorldSettings* WS = World->GetWorldSettings())
	{
		OutLines.Add(FString::Printf(TEXT("Audit: WorldSettings bForceNoPrecomputedLighting=%d (not modified by the lighting generator)"), WS->bForceNoPrecomputedLighting ? 1 : 0));
	}

	auto ReportCVarInt = [&OutLines](const TCHAR* Name)
	{
		if (const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			OutLines.Add(FString::Printf(TEXT("Audit: %s = %d"), Name, CVar->GetInt()));
		}
		else
		{
			OutLines.Add(FString::Printf(TEXT("Audit: %s not found in this build"), Name));
		}
	};
	ReportCVarInt(TEXT("r.DynamicGlobalIlluminationMethod")); // 1 = Lumen
	ReportCVarInt(TEXT("r.ReflectionMethod"));                // 1 = Lumen
	ReportCVarInt(TEXT("r.Shadow.Virtual.Enable"));
	ReportCVarInt(TEXT("r.Nanite"));
	ReportCVarInt(TEXT("r.VolumetricFog"));
	ReportCVarInt(TEXT("r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange"));
}

// ---------------------------------------------------------------------
// Generate
// ---------------------------------------------------------------------

bool FVoidLightingGenerator::Generate(const FVoidDesignPackage& Package, FVoidGenerationContext& Context)
{
	using namespace VoidLightingGenPrivate;

	const double StartSeconds = FPlatformTime::Seconds();
	FVoidValidationReport& Report = Context.GenerationValidationReport;
	Report.bIsValid = true;

	const FVoidDistrictData& District = Package.District;
	const FName DistrictId = District.DistrictId.Value;

	if (!Context.TargetWorld)
	{
		Report.AddFatal(TEXT("No target world."), TEXT(""), TEXT("VOID.Lighting.NoWorld"));
		Context.Log(TEXT("Lighting: aborting, no target world."));
		return false;
	}
	UWorld* World = Context.TargetWorld;

	const UVoidLightingSettings* Settings = GetDefault<UVoidLightingSettings>();
	const UVoidRoadGenerationSettings* RoadSettings = GetDefault<UVoidRoadGenerationSettings>();

	// --- config ---------------------------------------------------------------
	FVoidLightingConfig& Config = FVoidLightingConfig::Get();
	Config.Reload(Settings->ConfigDirectoryOverride.Path);
	if (!Config.HasLoadedPresets() || !Config.HasLoadedProfiles())
	{
		Report.AddFatal(FString::Printf(TEXT("Lighting config could not be loaded from '%s' (need LightingPresets.json and LightingProfiles.json)."), *Config.GetLoadedDirectory()),
			TEXT(""), TEXT("VOID.Lighting.ConfigMissing"), TEXT("Check Config/VOIDLighting in the plugin, or set the override directory in Project Settings."));
		Context.Log(TEXT("Lighting: aborting, config not loaded."));
		return false;
	}

	// --- metadata (optional) ------------------------------------------------------
	FVoidMeridianLightingMetadata Meta;
	if (!Settings->MeridianMetadataDirectory.Path.IsEmpty())
	{
		TArray<FString> Messages;
		Meta.LoadFromDirectory(Settings->MeridianMetadataDirectory.Path, &Messages);
		for (const FString& M : Messages) { Context.Log(TEXT("Lighting metadata: ") + M); Report.AddWarning(M, TEXT(""), TEXT("VOID.Lighting.Metadata")); }
	}
	else
	{
		Report.AddInfo(TEXT("No Meridian metadata directory set; landmarks are matched only through explicit LandmarkBindings."), TEXT(""), TEXT("VOID.Lighting.NoMetadataDir"));
	}

	// --- audit -----------------------------------------------------------------
	{
		TArray<FString> AuditLines;
		AuditWorld(World, AuditLines);
		for (const FString& L : AuditLines) { Context.Log(L); }
	}

	// --- material (outside the transaction: asset creation) ------------------------------
	UMaterialInterface* EmissiveParent = nullptr;
	if (!Settings->EmissiveMaterialOverride.IsNull())
	{
		EmissiveParent = Settings->EmissiveMaterialOverride.LoadSynchronous();
	}
	if (!EmissiveParent)
	{
		FString MatError;
		EmissiveParent = FVoidLightingMaterialBuilder::FindOrCreate(Settings->GeneratedMaterialPath, MatError);
		if (!MatError.IsEmpty())
		{
			Report.AddWarning(MatError, TEXT(""), TEXT("VOID.Lighting.Material"));
			Context.Log(TEXT("Lighting material: ") + MatError);
		}
	}
	if (!EmissiveParent)
	{
		Report.AddWarning(TEXT("No emissive material available: windows, lamps and signs will render black/default; only real lights and the environment presets will work."), TEXT(""), TEXT("VOID.Lighting.NoMaterial"));
	}

	const FScopedTransaction Transaction(NSLOCTEXT("VoidLightingGenerator", "GenerateLighting", "Generate VOID Lighting"));

	// --- director + environment ------------------------------------------------------------
	AVoidLightingDirector* Director = AVoidLightingDirector::FindDirector(World);
	if (!Director)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Director = World->SpawnActor<AVoidLightingDirector>(FVector::ZeroVector, FRotator::ZeroRotator, P);
		if (!Director)
		{
			Report.AddFatal(TEXT("Failed to spawn AVoidLightingDirector."), TEXT(""), TEXT("VOID.Lighting.DirectorSpawn"));
			return false;
		}
#if WITH_EDITOR
		Director->SetActorLabel(TEXT("VOID_LightingDirector"));
		Director->SetFolderPath(TEXT("VOID/Lighting"));
#endif
	}
	Director->Modify();
	Director->PresetLibrary = Config.GetPresets();
	Director->ExistingPolicy = Settings->ExistingLightingPolicy;
	Director->ColorGradingLUT = Settings->ColorGradingLUT;
	Director->EnsureEnvironmentActors(Settings->bGeneratePostProcessVolume);

	// --- district profile ---------------------------------------------------------------------
	bool bProfileMatched = false;
	FVoidDistrictLightingProfile Profile = Config.FindDistrictProfile(DistrictId, &bProfileMatched);
	const FVoidMeridianDistrictInfo* DistrictMeta = Meta.FindDistrict(DistrictId);
	if (!bProfileMatched)
	{
		Report.AddWarning(FString::Printf(TEXT("No lighting profile matches district '%s'; using the neutral default."), *DistrictId.ToString()),
			TEXT("district.districtId"), TEXT("VOID.Lighting.NoDistrictProfile"), TEXT("Add a district entry (or matchIds) in LightingProfiles.json."));
	}
	if (DistrictMeta && DistrictMeta->IsBelowGrade() && !Profile.bBelowGrade)
	{
		Profile.bBelowGrade = true;
		Report.AddInfo(TEXT("DistrictRegistry marks this district below grade; treating it as sun/sky-independent."), TEXT(""), TEXT("VOID.Lighting.BelowGradeFromRegistry"));
	}

	// --- regeneration: replace this district's lighting ------------------------------------------
	{
		TArray<AActor*> Old;
		for (TActorIterator<AVoidDistrictLightingActor> It(World); It; ++It) { if (It->DistrictId == DistrictId) { Old.Add(*It); } }
		for (TActorIterator<AVoidLandmarkLightingActor> It(World); It; ++It) { if (It->DistrictId == DistrictId) { Old.Add(*It); } }
		for (AActor* A : Old) { World->DestroyActor(A); }
		if (Old.Num() > 0) { Context.Log(FString::Printf(TEXT("Lighting: replaced %d existing lighting actors for district '%s'."), Old.Num(), *DistrictId.ToString())); }
		Director->RemoveAnchorsForDistrict(DistrictId);
	}

	// --- build road info ---------------------------------------------------------------------------
	const float RampLength = RoadSettings->RampLengthUnits;
	const float BridgeHeight = RoadSettings->DefaultBridgeHeightUnits;
	const float TunnelDepth = RoadSettings->DefaultTunnelDepthUnits;
	const UDataTable* ProfileTable = !RoadSettings->RoadTypeProfileTable.IsNull() ? RoadSettings->RoadTypeProfileTable.LoadSynchronous() : nullptr;

	TArray<FBuiltRoadInfo> Roads;
	TArray<FVoidBuiltRoad> BuiltForJunctions;
	for (const FVoidRoadSpec& Spec : District.Roads)
	{
		const bool bRound = (Spec.RoadType == EVoidRoadType::Roundabout);
		FBuiltRoadInfo Info;
		Info.Spec = Spec;
		Info.Points = bRound ? FVoidRoadSplineBuilder::BuildRoundaboutLoopPoints(Spec) : FVoidRoadSplineBuilder::BuildCenterlinePoints(Spec);
		if (Info.Points.Num() < 2) { continue; }
		if (!bRound) { FVoidRoadBridgeTunnelBuilder::ApplyElevationRamp(Spec, Info.Points, RampLength, BridgeHeight, TunnelDepth); }
		Info.Profile = FVoidRoadTypeProfileLibrary::ResolveProfile(Spec.RoadType, ProfileTable);
		Info.HalfWidth = (Spec.WidthUnits > 0.0f ? Spec.WidthUnits : Info.Profile.DefaultWidthUnits) * 0.5f;
		Info.bClosed = bRound;
		Info.Rule = Config.GetRoadRule(Spec.RoadType);
		FVoidBuiltRoad B; B.Spec = Spec; B.Points = Info.Points;
		BuiltForJunctions.Add(B);
		Roads.Add(MoveTemp(Info));
	}
	TMap<FName, int32> RoadIndexById;
	for (int32 i = 0; i < Roads.Num(); ++i) { RoadIndexById.Add(Roads[i].Spec.Id.Value, i); }

	const TArray<FVoidRoadJunction> Junctions = FVoidRoadIntersectionBuilder::BuildJunctionGraph(BuiltForJunctions, RoadSettings->JunctionToleranceUnits);

	// --- buildings ------------------------------------------------------------------------------------
	TArray<FBuildingInfo> Buildings;
	int32 SkippedBuildings = 0;
	for (const FVoidBuildingSpec& B : District.Buildings)
	{
		if (B.FootprintCorners.Num() < 3 || B.HeightUnits <= 0.0f) { ++SkippedBuildings; continue; }
		TArray<FVector2D> C2 = B.FootprintCorners;
		if (ToWindingArea(C2) < 0.0f) { Algo::Reverse(C2); } // normalise to CCW so "outward" is always right of the edge direction

		FBuildingInfo Info;
		Info.Spec = &B;
		FVector Sum = FVector::ZeroVector;
		for (const FVector2D& P : C2)
		{
			Info.Corners.Add(FVector(P.X, P.Y, Settings->BuildingGroundZ));
			Sum += Info.Corners.Last();
		}
		Info.Centroid = Sum / Info.Corners.Num();
		for (const FVector& P : Info.Corners) { Info.Radius = FMath::Max(Info.Radius, FVector::Dist2D(P, Info.Centroid)); }
		Info.Height = B.HeightUnits;
		Buildings.Add(MoveTemp(Info));
	}
	if (SkippedBuildings > 0)
	{
		Report.AddWarning(FString::Printf(TEXT("%d building(s) skipped (fewer than 3 footprint corners or non-positive height)."), SkippedBuildings), TEXT("district.buildings"), TEXT("VOID.Lighting.SkippedBuildings"));
	}

	// Landmark association: explicit binding first, then id / type / id-prefix equality with a registry landmark. Never invented.
	TMap<FName, int32> LandmarkBuildingCount;
	for (FBuildingInfo& B : Buildings)
	{
		FName Found = NAME_None;
		for (const FVoidLandmarkBinding& Bind : Settings->LandmarkBindings)
		{
			if (Bind.BuildingId == B.Spec->Id.Value) { Found = Bind.LandmarkId; break; }
		}
		if (Found == NAME_None)
		{
			const FString BId = LowerId(B.Spec->Id.Value);
			const FString BType = B.Spec->BuildingType.ToLower();
			for (const FVoidMeridianLandmarkInfo& L : Meta.GetLandmarks())
			{
				const FString LId = LowerId(L.Id);
				if (BId == LId || BType == LId || BId.StartsWith(LId + TEXT("_")))
				{
					Found = L.Id;
					break;
				}
			}
		}
		if (Found == NAME_None) { continue; }

		const FVoidMeridianLandmarkInfo* LM = Meta.FindLandmark(Found);
		if (LM && !LM->HasSkylinePresence())
		{
			Context.Log(FString::Printf(TEXT("Lighting: landmark '%s' is interior/not skyline-applicable per registry; building '%s' gets no landmark presentation."), *Found.ToString(), *B.Spec->Id.Value.ToString()));
			continue;
		}
		int32& Count = LandmarkBuildingCount.FindOrAdd(Found);
		if (Count >= Settings->MaxLandmarkBuildingsPerLandmark) { continue; }
		++Count;
		B.LandmarkId = Found;
		if (LM) { B.LandmarkTier = LM->VisibilityTier; }
		else
		{
			B.LandmarkTier = 4;
			B.bLandmarkTierProvisional = true;
			Report.AddWarning(FString::Printf(TEXT("Landmark '%s' was bound explicitly but is not in the loaded LandmarkRegistry; using tier 4."), *Found.ToString()), TEXT(""), TEXT("VOID.Lighting.LandmarkNotInRegistry"));
		}
	}

	// --- district actor ---------------------------------------------------------------------------------
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AVoidDistrictLightingActor* DA = World->SpawnActor<AVoidDistrictLightingActor>(FVector::ZeroVector, FRotator::ZeroRotator, SP);
	if (!DA)
	{
		Report.AddFatal(TEXT("Failed to spawn AVoidDistrictLightingActor."), TEXT(""), TEXT("VOID.Lighting.DistrictSpawn"));
		return false;
	}
#if WITH_EDITOR
	DA->SetActorLabel(FString::Printf(TEXT("VoidLighting_%s"), *DistrictId.ToString()));
	DA->SetFolderPath(TEXT("VOID/Lighting"));
#endif
	DA->DistrictId = DistrictId;
	DA->Profile = Profile;
	DA->EmissiveParent = EmissiveParent;

	// Fixed (non palette-driven) colours.
	DA->SetElementPalette(EVoidLightingElement::TrafficRed, FLinearColor(1.0f, 0.04f, 0.02f), FLinearColor(1.0f, 0.04f, 0.02f));
	DA->SetElementPalette(EVoidLightingElement::TrafficAmber, FLinearColor(1.0f, 0.55f, 0.03f), FLinearColor(1.0f, 0.55f, 0.03f));
	DA->SetElementPalette(EVoidLightingElement::TrafficGreen, FLinearColor(0.05f, 1.0f, 0.3f), FLinearColor(0.05f, 1.0f, 0.3f));
	DA->SetElementPalette(EVoidLightingElement::PedestrianSignal, FLinearColor(0.85f, 0.95f, 1.0f), FLinearColor(0.85f, 0.95f, 1.0f));
	DA->SetElementPalette(EVoidLightingElement::EmergencyBeacon, FLinearColor(1.0f, 0.08f, 0.03f), FLinearColor(1.0f, 0.4f, 0.05f));
	DA->RefreshPalettes();

	FRandomStream Rng(static_cast<int32>(GetTypeHash(DistrictId)) ^ 0x51ED270B);

	// ============================== STREETS ==============================
	TArray<FRealLightCandidate> Candidates;
	int32 NumPoles = 0, NumLampHeads = 0, NumTunnelSkipped = 0, NumBeacons = 0, NumTrafficSets = 0, NumPedSignals = 0;
	bool bStreetCapHit = false;
	const int32 StreetCap = Settings->MaxStreetInstancesPerElement;

	auto AddPoleWithHead = [&](const FVector& Base, float PoleH, const FVector& TowardRoad, float Arm) -> FVector
	{
		if (DA->GetElementInstanceCount(EVoidLightingElement::StreetPole) >= StreetCap || DA->GetElementInstanceCount(EVoidLightingElement::StreetLamp) >= StreetCap)
		{
			bStreetCapHit = true;
			return FVector::ZeroVector;
		}
		DA->AddElementInstance(EVoidLightingElement::StreetPole, FTransform(FRotator::ZeroRotator, Base + FVector(0, 0, PoleH * 0.5f), FVector(0.18f, 0.18f, PoleH / 100.0f)), 0.0f, 0.0f);
		++NumPoles;
		const FVector HeadPos = Base + TowardRoad * (Arm * 0.5f) + FVector(0, 0, PoleH);
		DA->AddElementInstance(EVoidLightingElement::StreetLamp, FTransform(FRotator(0.0f, YawOf(TowardRoad), 0.0f), HeadPos, FVector(FMath::Max(Arm, 60.0f) / 100.0f * 0.6f, 0.4f, 0.12f)),
			Rng.FRand(), Rng.FRand());
		++NumLampHeads;
		return HeadPos;
	};

	for (const FBuiltRoadInfo& R : Roads)
	{
		if (!R.Rule.bEnabled) { continue; }
		if (R.Spec.bIsTunnel) { ++NumTunnelSkipped; continue; }

		const float Spacing = R.Rule.SpacingUnits * Profile.StreetLampSpacingMul;
		const float LateralOffset = R.HalfWidth + (R.Spec.bHasSidewalk ? (R.Profile.CurbWidthUnits + R.Profile.SidewalkWidthUnits * 0.5f) : 120.0f);
		int32 PoleCounter = 0;

		WalkPolyline(R.Points, R.bClosed, Spacing, [&](const FVector& Pos, const FVector& T, int32 StationIndex)
		{
			const FVector Left = SideOf(T);
			TArray<float, TInlineAllocator<2>> Signs;
			if (R.bClosed)          { Signs.Add(-1.0f); }               // roundabout: lamps on the outside only (loop is wound CCW, so right = outside)
			else if (R.Rule.bBothSides) { Signs.Add(1.0f); Signs.Add(-1.0f); }
			else                    { Signs.Add((StationIndex & 1) ? -1.0f : 1.0f); }

			for (const float Sign : Signs)
			{
				const FVector Base = Pos + Left * (Sign * LateralOffset);
				const FVector Toward = -Left * Sign;
				const FVector Head = AddPoleWithHead(Base, R.Rule.PoleHeightUnits, Toward, R.Rule.ArmLengthUnits);
				if (Head.IsZero()) { continue; }

				++PoleCounter;
				if (R.Rule.RealLightEveryN > 0 && (PoleCounter % R.Rule.RealLightEveryN) == 0)
				{
					FRealLightCandidate C;
					C.Location = Head - FVector(0, 0, 20.0f);
					C.Lumens = R.Rule.RealLightLumens;
					C.Radius = R.Rule.RealLightRadiusUnits;
					C.Priority = R.Rule.Priority;
					C.bPoint = false;
					C.Role = EVoidRealLightRole::Street;
					Candidates.Add(C);
				}
			}
		});
	}

	// Junctions: corner mast light, traffic signals, pedestrian signals.
	int32 NumIntersectionMasts = 0;
	for (const FVoidRoadJunction& J : Junctions)
	{
		const bool bBranching = (J.Type == EVoidRoadJunctionType::TJunction || J.Type == EVoidRoadJunctionType::FourWayJunction || J.Type == EVoidRoadJunctionType::Complex);
		if (!bBranching) { continue; }

		struct FApproach { FVector Dir; float Width; };
		TArray<FApproach, TInlineAllocator<6>> Approaches;
		FVoidRoadLightingRule Best;
		bool bHaveBest = false;
		bool bAnyTraffic = false, bAnyPed = false;
		float MaxHalf = 0.0f;

		for (const FVoidElementId& Id : J.ConnectedRoadIds)
		{
			const int32* Idx = RoadIndexById.Find(Id.Value);
			if (!Idx) { continue; }
			const FBuiltRoadInfo& R = Roads[*Idx];
			const bool bNearStart = FVector::DistSquared(R.Points[0], J.Location) < FVector::DistSquared(R.Points.Last(), J.Location);
			const FVector Near = bNearStart ? R.Points[0] : R.Points.Last();
			const FVector In = bNearStart ? R.Points[1] : R.Points[R.Points.Num() - 2];
			FVector Dir(In.X - Near.X, In.Y - Near.Y, 0.0f);
			if (Dir.IsNearlyZero()) { continue; }
			Approaches.Add({ Dir.GetSafeNormal(), R.HalfWidth * 2.0f });
			MaxHalf = FMath::Max(MaxHalf, R.HalfWidth);
			if (!bHaveBest || R.Rule.Priority > Best.Priority) { Best = R.Rule; bHaveBest = true; }
			bAnyTraffic |= R.Rule.bTrafficLights;
			bAnyPed |= R.Rule.bPedestrianSignals;
		}
		if (Approaches.Num() < 2 || !bHaveBest || !Best.bEnabled) { continue; }

		// Mast at the corner between the first two approaches.
		FVector Bis = (Approaches[0].Dir + Approaches[1].Dir).GetSafeNormal();
		if (Bis.IsNearlyZero()) { Bis = SideOf(Approaches[0].Dir); }
		const FVector MastBase = J.Location + Bis * (J.PadRadius + MaxHalf * 0.6f + 250.0f);
		const FVector Head = AddPoleWithHead(MastBase, Best.PoleHeightUnits * 1.1f, -Bis, Best.ArmLengthUnits * 1.2f);
		if (!Head.IsZero())
		{
			++NumIntersectionMasts;
			FRealLightCandidate C;
			C.Location = Head - FVector(0, 0, 20.0f);
			C.Lumens = FMath::Max(Best.RealLightLumens * 1.5f, 6000.0f);
			C.Radius = FMath::Max(Best.RealLightRadiusUnits, 1600.0f);
			C.Priority = Best.Priority + 1;
			C.bPoint = true;
			C.Role = EVoidRealLightRole::Intersection;
			Candidates.Add(C);
		}

		const int32 NumSignalApproaches = FMath::Min(Approaches.Num(), 4);
		for (int32 a = 0; a < NumSignalApproaches; ++a)
		{
			const FVector A = Approaches[a].Dir;               // from junction outward along the road
			const FVector Right = FVector(-A.Y, A.X, 0.0f);    // right-hand side of a driver travelling toward the junction (-A)
			const float W = Approaches[a].Width;
			const FVector Pole = J.Location + A * (J.PadRadius + 120.0f) + Right * (W * 0.5f + 140.0f);

			if (bAnyTraffic)
			{
				if (DA->GetElementInstanceCount(EVoidLightingElement::TrafficHousing) >= StreetCap - 1) { bStreetCapHit = true; break; }
				const float PoleH = 520.0f;
				const float FaceYaw = YawOf(A);
				DA->AddElementInstance(EVoidLightingElement::StreetPole, FTransform(FRotator::ZeroRotator, Pole + FVector(0, 0, PoleH * 0.5f), FVector(0.14f, 0.14f, PoleH / 100.0f)), 0.0f, 0.0f);
				DA->AddElementInstance(EVoidLightingElement::TrafficHousing, FTransform(FRotator(0, FaceYaw, 0), Pole + FVector(0, 0, PoleH + 60.0f), FVector(0.35f, 0.35f, 1.1f)), 0.0f, 0.0f);

				// One lens lit per approach (static; a cycling signal would need a timer, which this system deliberately avoids).
				const float Roll = Rng.FRand();
				const int32 LitLens = (Roll < 0.40f) ? 0 : (Roll < 0.85f ? 2 : 1); // red / green / amber
				const EVoidLightingElement Lens[3] = { EVoidLightingElement::TrafficRed, EVoidLightingElement::TrafficAmber, EVoidLightingElement::TrafficGreen };
				for (int32 L = 0; L < 3; ++L)
				{
					const float Z = PoleH + 60.0f + (1 - L) * 36.0f;
					DA->AddElementInstance(Lens[L], FTransform(FRotator(0, FaceYaw, 0), Pole + A * 20.0f + FVector(0, 0, Z), FVector(0.1f, 0.22f, 0.22f)), (L == LitLens) ? 0.0f : 1.0f, 0.0f);
				}
				++NumTrafficSets;
			}

			if (bAnyPed)
			{
				const FVector PedFace = -Right;
				DA->AddElementInstance(EVoidLightingElement::PedestrianSignal, FTransform(FRotator(0, YawOf(PedFace), 0), Pole + PedFace * 12.0f + FVector(0, 0, 260.0f), FVector(0.08f, 0.3f, 0.4f)), 0.0f, 0.0f);
				++NumPedSignals;
			}
		}
	}

	// Emergency beacons at tunnel portals.
	for (const FBuiltRoadInfo& R : Roads)
	{
		if (!R.Spec.bIsTunnel) { continue; }
		for (const FVector& Portal : FVoidRoadBridgeTunnelBuilder::ComputeTunnelPortalPositions(R.Spec, R.Points))
		{
			DA->AddElementInstance(EVoidLightingElement::EmergencyBeacon, FTransform(FRotator::ZeroRotator, Portal + FVector(0, 0, 450.0f), FVector(0.5f)), 0.0f, Rng.FRand());
			++NumBeacons;
		}
	}

	// Real-light budget: keep the highest-priority candidates.
	Algo::StableSort(Candidates, [](const FRealLightCandidate& A, const FRealLightCandidate& B) { return A.Priority > B.Priority; });
	const int32 RealBudget = Settings->MaxRealStreetLightsPerDistrict;
	const int32 NumCandidates = Candidates.Num();
	const int32 NumRealPlaced = FMath::Min(NumCandidates, RealBudget);
	const float StreetTemp = AvgKelvin(Profile.StreetLampTempA, Profile.StreetLampTempB);
	for (int32 i = 0; i < NumRealPlaced; ++i)
	{
		const FRealLightCandidate& C = Candidates[i];
		if (C.bPoint)
		{
			DA->AddPointLight(C.Role, C.Location, C.Lumens, C.Radius, StreetTemp, Settings->bRealStreetLightsCastShadows, 1.0f);
		}
		else
		{
			DA->AddSpotLight(C.Role, C.Location, FRotator(-90.0f, 0.0f, 0.0f), C.Lumens, C.Radius, 75.0f, StreetTemp, Settings->bRealStreetLightsCastShadows, 1.0f);
		}
	}
	if (NumCandidates > RealBudget)
	{
		Report.AddWarning(FString::Printf(TEXT("Real street-light budget hit: %d candidates, %d placed (lowest-priority %d dropped; they keep their emissive lamp heads)."), NumCandidates, RealBudget, NumCandidates - RealBudget),
			TEXT(""), TEXT("VOID.Lighting.RealLightBudget"), TEXT("Raise MaxRealStreetLightsPerDistrict or the road rules' realLightEveryN."));
	}
	if (bStreetCapHit)
	{
		Report.AddWarning(TEXT("Street instance cap hit; some poles/signals were not generated."), TEXT(""), TEXT("VOID.Lighting.StreetCap"), TEXT("Raise MaxStreetInstancesPerElement."));
	}

	// ============================== BUILDINGS ==============================
	// Landmark buildings first so they keep their windows if the district cap is reached.
	TArray<int32> Order;
	for (int32 i = 0; i < Buildings.Num(); ++i) { Order.Add(i); }
	Algo::StableSort(Order, [&Buildings](int32 A, int32 B) { return (Buildings[A].LandmarkId != NAME_None) && (Buildings[B].LandmarkId == NAME_None); });

	int32 TotalWindows = 0, NumNeon = 0, NumUnclassified = 0;
	bool bDistrictWindowCap = false;
	const float NeonZ = 450.0f;
	const int32 MaxNeon = 2000;

	for (const int32 BuildingIndex : Order)
	{
		const FBuildingInfo& B = Buildings[BuildingIndex];
		const bool bIsLandmark = (B.LandmarkId != NAME_None);

		bool bMatched = false;
		const FVoidUsageRule Rule = bIsLandmark ? Config.GetUsageRule(EVoidLightingUsage::Landmark) : Config.ClassifyBuilding(B.Spec->BuildingType, &bMatched);
		if (!bIsLandmark && !bMatched) { ++NumUnclassified; }
		const EVoidLightingUsage Usage = bIsLandmark ? EVoidLightingUsage::Landmark : Rule.Usage;
		const EVoidLightingElement Elem = WindowElementFor(Usage);
		const float Boost = bIsLandmark ? FMath::Max(1.0f, Config.GetLandmarkTierRule(B.LandmarkTier).WindowLitBoost) : 1.0f;

		FRandomStream BRng(static_cast<int32>(GetTypeHash(B.Spec->Id.Value)) ^ 0x2545F491);

		float Bay = FMath::Max(150.0f, Rule.BaySpacingUnits);
		float FloorH = FMath::Max(150.0f, Rule.FloorHeightUnits);
		int32 Candidates2 = 0;
		const int32 NumC = B.Corners.Num();
		for (int32 e = 0; e < NumC; ++e)
		{
			const float Len = FVector::Dist2D(B.Corners[e], B.Corners[(e + 1) % NumC]);
			if (Len >= 150.0f) { Candidates2 += FMath::Max(1, FMath::FloorToInt(Len / Bay)); }
		}
		const int32 Floors0 = FMath::Max(1, FMath::FloorToInt(B.Height / FloorH));
		const int64 Total = static_cast<int64>(Candidates2) * Floors0;
		const int32 PerBuildingCap = Settings->MaxWindowInstancesPerBuilding;
		if (PerBuildingCap > 0 && Total > PerBuildingCap)
		{
			const float Factor = FMath::Sqrt(static_cast<float>(Total) / PerBuildingCap); // coarser grid, same coverage
			Bay *= Factor;
			FloorH *= Factor;
		}
		const int32 Floors = FMath::Max(1, FMath::FloorToInt(B.Height / FloorH));
		const float Density = FMath::Clamp(Profile.WindowDensityMul, 0.0f, 1.0f);

		for (int32 e = 0; e < NumC && !bDistrictWindowCap; ++e)
		{
			const FVector A = B.Corners[e];
			const FVector Bp = B.Corners[(e + 1) % NumC];
			const float Len = FVector::Dist2D(A, Bp);
			if (Len < 150.0f) { continue; }
			const FVector Dir = FVector(Bp.X - A.X, Bp.Y - A.Y, 0.0f).GetSafeNormal();
			const FVector Out(Dir.Y, -Dir.X, 0.0f); // outward for CCW
			const int32 NumBays = FMath::Max(1, FMath::FloorToInt(Len / Bay));
			const float ActualBay = Len / NumBays;
			const float Thick = 8.0f;
			const FRotator Rot(0.0f, YawOf(Dir), 0.0f);

			for (int32 b = 0; b < NumBays && !bDistrictWindowCap; ++b)
			{
				const float t = (b + 0.5f) / NumBays;
				const FVector Along = FMath::Lerp(A, Bp, t);
				for (int32 f = 0; f < Floors; ++f)
				{
					if (Density < 1.0f && BRng.FRand() > Density) { continue; }
					if (Settings->MaxWindowInstancesPerDistrict > 0 && TotalWindows >= Settings->MaxWindowInstancesPerDistrict)
					{
						bDistrictWindowCap = true;
						break;
					}
					const float Z = Settings->BuildingGroundZ + (f + 0.5f) * FloorH;
					const FVector Pos = Along + Out * (Thick * 0.5f + 2.0f) + FVector(0, 0, Z);
					DA->AddElementInstance(Elem, FTransform(Rot, Pos, FVector(ActualBay * 0.55f / 100.0f, Thick / 100.0f, FloorH * 0.42f / 100.0f)),
						BRng.FRand() / Boost, BRng.FRand());
					++TotalWindows;
				}
			}
		}

		// Neon hooks on road-facing edges of commercial buildings.
		if (Usage == EVoidLightingUsage::Commercial && Profile.NeonDensity > 0.0f && NumNeon < MaxNeon)
		{
			TArray<int32, TInlineAllocator<8>> NearEdges;
			float NearLen = 0.0f;
			for (int32 e = 0; e < NumC; ++e)
			{
				const FVector A = B.Corners[e];
				const FVector Bp = B.Corners[(e + 1) % NumC];
				const float Len = FVector::Dist2D(A, Bp);
				if (Len < 300.0f) { continue; }
				if (DistanceToRoads((A + Bp) * 0.5f, Roads) < 2500.0f) { NearEdges.Add(e); NearLen += Len; }
			}
			float Want = Profile.NeonDensity * NearLen / 1500.0f;
			int32 Count = FMath::FloorToInt(Want);
			if (BRng.FRand() < (Want - Count)) { ++Count; }
			Count = FMath::Min(Count, 3);
			for (int32 n = 0; n < Count && NearEdges.Num() > 0 && NumNeon < MaxNeon; ++n)
			{
				const int32 e = NearEdges[BRng.RandRange(0, NearEdges.Num() - 1)];
				const FVector A = B.Corners[e];
				const FVector Bp = B.Corners[(e + 1) % NumC];
				const FVector Dir = FVector(Bp.X - A.X, Bp.Y - A.Y, 0.0f).GetSafeNormal();
				const FVector Out(Dir.Y, -Dir.X, 0.0f);
				const FVector Pos = FMath::Lerp(A, Bp, BRng.FRandRange(0.2f, 0.8f)) + Out * 18.0f + FVector(0, 0, Settings->BuildingGroundZ + NeonZ + BRng.FRandRange(0.0f, 400.0f));
				const FTransform T(FRotator(0, YawOf(Dir), 0), Pos, FVector(3.0f, 0.12f, 1.2f));
				DA->AddElementInstance(EVoidLightingElement::NeonSign, T, BRng.FRand(), BRng.FRand());
				FVoidNeonHook Hook;
				Hook.BuildingId = B.Spec->Id.Value;
				Hook.Transform = T;
				DA->NeonHooks.Add(Hook);
				++NumNeon;
			}
		}
	}
	if (bDistrictWindowCap)
	{
		Report.AddWarning(FString::Printf(TEXT("District window cap (%d) reached; remaining buildings have no window plates (landmarks were processed first)."), Settings->MaxWindowInstancesPerDistrict),
			TEXT(""), TEXT("VOID.Lighting.WindowCap"), TEXT("Raise MaxWindowInstancesPerDistrict or lower windowDensityMul."));
	}
	if (NumUnclassified > 0)
	{
		Report.AddInfo(FString::Printf(TEXT("%d building(s) had a BuildingType matching no usage keyword and were treated as 'Unknown' (commercial-style windows, reduced lit fraction)."), NumUnclassified), TEXT("district.buildings"), TEXT("VOID.Lighting.UnclassifiedBuildings"), TEXT("Extend usageRules keywords in LightingProfiles.json."));
	}

	// Per-usage element bias.
	const EVoidLightingUsage UsageKinds[] = { EVoidLightingUsage::Residential, EVoidLightingUsage::Commercial, EVoidLightingUsage::Civic, EVoidLightingUsage::Industrial, EVoidLightingUsage::Landmark };
	for (const EVoidLightingUsage U : UsageKinds)
	{
		const FVoidUsageRule R = Config.GetUsageRule(U);
		DA->SetElementBias(WindowElementFor(U), R.LitMul, R.IntensityMul);
	}

	// Finish all element batches once.
	for (int32 i = 0; i < static_cast<int32>(EVoidLightingElement::Count); ++i)
	{
		if (DA->GetElementInstanceCount(static_cast<EVoidLightingElement>(i)) > 0)
		{
			DA->FinishElement(static_cast<EVoidLightingElement>(i));
		}
	}

	// ============================== BOUNDS ==============================
	FBox Bounds(ForceInit);
	for (const FBuildingInfo& B : Buildings)
	{
		for (const FVector& P : B.Corners) { Bounds += P; Bounds += P + FVector(0, 0, B.Height); }
	}
	for (const FBuiltRoadInfo& R : Roads)
	{
		for (const FVector& P : R.Points) { Bounds += P + FVector(R.HalfWidth, R.HalfWidth, 0); Bounds += P - FVector(R.HalfWidth, R.HalfWidth, 0); }
	}
	DA->DistrictBounds = Bounds;

	// ============================== LANDMARKS ==============================
	const FVector PackageCenter = Bounds.IsValid ? Bounds.GetCenter() : FVector::ZeroVector;
	TArray<FVoidCinematicAnchor> NewAnchors;
	TMap<FName, int32> LandmarkAnchorCounter;
	int32 NumLandmarkActors = 0, NumLandmarkLights = 0;

	for (const FBuildingInfo& B : Buildings)
	{
		if (B.LandmarkId == NAME_None) { continue; }

		AVoidLandmarkLightingActor* LA = World->SpawnActor<AVoidLandmarkLightingActor>(FVector::ZeroVector, FRotator::ZeroRotator, SP);
		if (!LA) { continue; }
		const int32 Serial = LandmarkAnchorCounter.FindOrAdd(B.LandmarkId)++;
#if WITH_EDITOR
		LA->SetActorLabel(FString::Printf(TEXT("VoidLandmarkLight_%s_%d"), *B.LandmarkId.ToString(), Serial));
		LA->SetFolderPath(TEXT("VOID/Lighting"));
#endif
		LA->LandmarkId = B.LandmarkId;
		LA->BuildingId = B.Spec->Id.Value;
		LA->DistrictId = DistrictId;
		LA->Tier = B.LandmarkTier;
		LA->TierRule = Config.GetLandmarkTierRule(B.LandmarkTier);
		LA->Centroid = B.Centroid;
		LA->Radius = B.Radius;
		LA->Height = B.Height;
		LA->EmissiveParent = EmissiveParent;

		FVector Rim = FVector(B.Centroid.X - PackageCenter.X, B.Centroid.Y - PackageCenter.Y, 0.0f);
		if (Rim.Size2D() < 200.0f) { Rim = FVector(1, 0, 0); } // landmark sits at the package centre: back-light defaults to +X
		LA->BuildLighting(B.Corners, Rim.GetSafeNormal(), /*bCastShadows=*/false);
		NumLandmarkLights += LA->GetNumRealLights();
		++NumLandmarkActors;

		FVoidCinematicAnchor A;
		A.Id = MakeAnchorId(FString::Printf(TEXT("landmark.%s%s"), *B.LandmarkId.ToString(), Serial > 0 ? *FString::Printf(TEXT(".%d"), Serial) : TEXT("")));
		A.Type = EVoidAnchorType::Landmark;
		A.DistrictId = DistrictId;
		A.LandmarkId = B.LandmarkId;
		A.Location = B.Centroid + FVector(0, 0, B.Height * 0.5f);
		A.Radius = B.Radius;
		A.Height = B.Height;
		A.Tier = B.LandmarkTier;
		const float Dist = FMath::Max(B.Radius * 2.5f, B.Height * 1.6f);
		A.SuggestedLookAt = B.Centroid + FVector(0, 0, B.Height * 0.6f);
		A.SuggestedCameraLocation = B.Centroid - Rim.GetSafeNormal() * Dist + FVector(0, 0, B.Height * 0.35f);
		A.Rotation = (A.SuggestedLookAt - A.SuggestedCameraLocation).Rotation();
		NewAnchors.Add(A);
	}

	// ============================== ANCHORS ==============================
	{
		float MaxH = 0.0f;
		for (const FBuildingInfo& B : Buildings) { MaxH = FMath::Max(MaxH, B.Height); }

		if (Bounds.IsValid)
		{
			FVoidCinematicAnchor A;
			A.Id = MakeAnchorId(FString::Printf(TEXT("district.%s.center"), *DistrictId.ToString()));
			A.Type = EVoidAnchorType::DistrictCenter;
			A.DistrictId = DistrictId;
			A.Location = FVector(PackageCenter.X, PackageCenter.Y, Settings->BuildingGroundZ);
			A.Radius = FMath::Max(Bounds.GetExtent().X, Bounds.GetExtent().Y);
			A.Height = MaxH;
			A.SuggestedLookAt = A.Location + FVector(0, 0, MaxH * 0.4f);
			A.SuggestedCameraLocation = A.Location + FVector(-A.Radius * 1.4f, -A.Radius * 1.4f, FMath::Max(A.Radius * 0.9f, MaxH));
			A.Rotation = (A.SuggestedLookAt - A.SuggestedCameraLocation).Rotation();
			NewAnchors.Add(A);
		}

		for (const FBuiltRoadInfo& R : Roads)
		{
			if (R.Spec.RoadType != EVoidRoadType::Highway && R.Spec.RoadType != EVoidRoadType::Primary) { continue; }
			float Total = 0.0f;
			for (int32 i = 0; i + 1 < R.Points.Num(); ++i) { Total += FVector::Dist(R.Points[i], R.Points[i + 1]); }
			float Acc = 0.0f;
			for (int32 i = 0; i + 1 < R.Points.Num(); ++i)
			{
				const float L = FVector::Dist(R.Points[i], R.Points[i + 1]);
				if (L > KINDA_SMALL_NUMBER && Acc + L >= Total * 0.5f)
				{
					const float t = (Total * 0.5f - Acc) / L;
					const FVector Mid = FMath::Lerp(R.Points[i], R.Points[i + 1], t);
					const FVector T = (R.Points[i + 1] - R.Points[i]).GetSafeNormal();
					FVoidCinematicAnchor A;
					A.Id = MakeAnchorId(FString::Printf(TEXT("road.%s"), *R.Spec.Id.Value.ToString()));
					A.Type = EVoidAnchorType::MajorRoad;
					A.DistrictId = DistrictId;
					A.Location = Mid;
					A.Rotation = T.Rotation();
					A.Radius = R.HalfWidth;
					A.SuggestedCameraLocation = Mid - T * 2500.0f + FVector(0, 0, 500.0f);
					A.SuggestedLookAt = Mid + T * 3000.0f;
					NewAnchors.Add(A);
					break;
				}
				Acc += L;
			}
		}

		// Camera-interest: busiest junctions, plus roundabout centres.
		TArray<const FVoidRoadJunction*> Busy;
		for (const FVoidRoadJunction& J : Junctions)
		{
			if (J.ConnectedRoadIds.Num() >= 3) { Busy.Add(&J); }
		}
		Busy.Sort([](const FVoidRoadJunction* A, const FVoidRoadJunction* B) { return A->ConnectedRoadIds.Num() > B->ConnectedRoadIds.Num(); });
		for (int32 i = 0; i < FMath::Min(Busy.Num(), 6); ++i)
		{
			FVoidCinematicAnchor A;
			A.Id = MakeAnchorId(FString::Printf(TEXT("interest.%s.junction_%d"), *DistrictId.ToString(), i));
			A.Type = EVoidAnchorType::CameraInterest;
			A.DistrictId = DistrictId;
			A.Location = Busy[i]->Location;
			A.Radius = Busy[i]->PadRadius;
			A.SuggestedLookAt = A.Location;
			A.SuggestedCameraLocation = A.Location + FVector(-1500.0f, -1500.0f, 900.0f);
			A.Rotation = (A.SuggestedLookAt - A.SuggestedCameraLocation).Rotation();
			NewAnchors.Add(A);
		}
		for (const FBuiltRoadInfo& R : Roads)
		{
			if (R.Spec.RoadType != EVoidRoadType::Roundabout || R.Spec.CenterlinePoints.Num() == 0) { continue; }
			FVoidCinematicAnchor A;
			A.Id = MakeAnchorId(FString::Printf(TEXT("interest.%s.roundabout.%s"), *DistrictId.ToString(), *R.Spec.Id.Value.ToString()));
			A.Type = EVoidAnchorType::CameraInterest;
			A.DistrictId = DistrictId;
			A.Location = FVector(R.Spec.CenterlinePoints[0].X, R.Spec.CenterlinePoints[0].Y, R.Spec.ElevationUnits);
			A.Radius = R.Spec.RoundaboutRadiusUnits;
			A.SuggestedLookAt = A.Location;
			A.SuggestedCameraLocation = A.Location + FVector(-A.Radius * 2.5f, -A.Radius * 2.5f, A.Radius * 1.5f);
			A.Rotation = (A.SuggestedLookAt - A.SuggestedCameraLocation).Rotation();
			NewAnchors.Add(A);
		}

		// Skyline points: tallest buildings, greedily separated.
		TArray<int32> ByHeight;
		for (int32 i = 0; i < Buildings.Num(); ++i) { ByHeight.Add(i); }
		Algo::StableSort(ByHeight, [&Buildings](int32 A, int32 B) { return Buildings[A].Height > Buildings[B].Height; });
		TArray<FVector, TInlineAllocator<16>> Picked;
		for (const int32 Idx : ByHeight)
		{
			if (Picked.Num() >= Settings->SkylinePointCount) { break; }
			const FBuildingInfo& B = Buildings[Idx];
			bool bFar = true;
			for (const FVector& P : Picked) { if (FVector::Dist2D(P, B.Centroid) < Settings->SkylineMinSeparationUnits) { bFar = false; break; } }
			if (!bFar) { continue; }
			Picked.Add(B.Centroid);

			FVector Away = FVector(B.Centroid.X - PackageCenter.X, B.Centroid.Y - PackageCenter.Y, 0.0f);
			Away = Away.Size2D() < 200.0f ? FVector(-1, -1, 0).GetSafeNormal() : Away.GetSafeNormal();
			FVoidCinematicAnchor A;
			A.Id = MakeAnchorId(FString::Printf(TEXT("skyline.%s.%d"), *DistrictId.ToString(), Picked.Num() - 1));
			A.Type = EVoidAnchorType::SkylinePoint;
			A.DistrictId = DistrictId;
			A.Location = B.Centroid + FVector(0, 0, B.Height);
			A.Radius = B.Radius;
			A.Height = B.Height;
			A.SuggestedLookAt = A.Location;
			A.SuggestedCameraLocation = B.Centroid + Away * (B.Height * 3.0f + 4000.0f) + FVector(0, 0, B.Height * 0.5f);
			A.Rotation = (A.SuggestedLookAt - A.SuggestedCameraLocation).Rotation();
			NewAnchors.Add(A);
		}
	}
	Director->AddAnchors(NewAnchors);

	// ============================== FINALISE ==============================
	Director->RefreshRegistrations();
	Director->WorldBounds = FBox(ForceInit);
	for (AVoidDistrictLightingActor* D : Director->DistrictActors)
	{
		if (D) { Director->WorldBounds += D->DistrictBounds; }
	}

	DA->GenerationSummary = FString::Printf(TEXT("poles=%d lampHeads=%d realStreet=%d/%d windows=%d neon=%d traffic=%d ped=%d beacons=%d landmarks=%d"),
		NumPoles, NumLampHeads, NumRealPlaced, NumCandidates, TotalWindows, NumNeon, NumTrafficSets, NumPedSignals, NumBeacons, NumLandmarkActors);

	FName ToApply = Director->ActivePresetId;
	bool bKnown = false;
	for (const FVoidLightingPreset& P : Director->PresetLibrary) { if (P.Id == ToApply) { bKnown = true; break; } }
	if (!bKnown) { ToApply = Settings->DefaultPresetId; }
	if (!Director->ApplyPresetById(ToApply) && Director->PresetLibrary.Num() > 0)
	{
		Director->ApplyPresetById(Director->PresetLibrary[0].Id);
	}

	const double ElapsedMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
	Context.Log(FString::Printf(TEXT("Lighting for district '%s': %s"), *DistrictId.ToString(), *DA->GenerationSummary));
	Context.Log(FString::Printf(TEXT("Lighting: tunnel roads skipped for street lamps=%d, intersection masts=%d, junctions=%d, landmark real lights=%d."), NumTunnelSkipped, NumIntersectionMasts, Junctions.Num(), NumLandmarkLights));
	Context.Log(FString::Printf(TEXT("Lighting: %d cinematic anchors added. Director: %s"), NewAnchors.Num(), *Director->BuildStatsReport()));
	Context.Log(FString::Printf(TEXT("Lighting generation finished in %.2f ms."), ElapsedMs));
	UE_LOG(LogVoidLighting, Log, TEXT("Lighting generation finished for '%s' in %.2f ms: %s"), *DistrictId.ToString(), ElapsedMs, *DA->GenerationSummary);

	return true;
}
