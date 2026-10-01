// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Environment/VoidDressingGenerator.h"
#include "Environment/VoidDressingActor.h"
#include "Environment/VoidDressingDefaults.h"
#include "Environment/VoidEnvironmentSettings.h"
#include "Environment/VoidEnvironmentTypes.h"
#include "Road/VoidRoadBridgeTunnelBuilder.h"
#include "Road/VoidRoadGenerationSettings.h"
#include "Road/VoidRoadIntersectionBuilder.h"
#include "Road/VoidRoadSplineBuilder.h"
#include "Road/VoidRoadTypeProfile.h"
#include "VoidWorldBuilderGeneratorsLog.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"

using namespace VoidDressing;

namespace VoidDressingPrivate
{
	// ------------------------------------------------------------------
	// Package -> plan inputs (geometry mirrors FVoidRoadGenerator exactly so props sit on the roads that were built)
	// ------------------------------------------------------------------

	static int32 TierOf(EVoidRoadType Type)
	{
		switch (Type)
		{
		case EVoidRoadType::Highway:    return 0;
		case EVoidRoadType::Primary:    return 1;
		case EVoidRoadType::Roundabout: return 1;
		case EVoidRoadType::Secondary:  return 2;
		case EVoidRoadType::Local:      return 3;
		case EVoidRoadType::Service:    return 4;
		case EVoidRoadType::Alley:      return 5;
		}
		return 3;
	}

	static void ClassifyBuilding(const FString& BuildingType, const TArray<FVoidBuildingUseTokenRow>& Tokens, FName& OutKind, FName& OutUse)
	{
		OutKind = FName(TEXT("Building"));
		OutUse = NAME_None;
		for (const FVoidBuildingUseTokenRow& Row : Tokens)
		{
			if (!Row.Token.IsEmpty() && BuildingType.Contains(Row.Token, ESearchCase::IgnoreCase))
			{
				OutKind = Row.Kind.IsNone() ? FName(TEXT("Building")) : Row.Kind;
				OutUse = Row.Use;
				return;
			}
		}
	}

	static float ResolveWidth(const FVoidRoadSpec& Spec, const UDataTable* ProfileTable)
	{
		return (Spec.WidthUnits > 0.0f) ? Spec.WidthUnits : FVoidRoadTypeProfileLibrary::ResolveProfile(Spec.RoadType, ProfileTable).DefaultWidthUnits;
	}

	static void BuildRoadBand(const FVoidRoadSpec& Spec, const FVoidRoadTypeProfile& Profile, float HalfWidth, float Shoulder, float& OutInner, float& OutOuter, float& OutZ)
	{
		if (Spec.bHasSidewalk)
		{
			OutInner = HalfWidth + FMath::Max(Profile.CurbWidthUnits, 0.0f);
			OutOuter = OutInner + Profile.SidewalkWidthUnits;
			OutZ = Profile.CurbHeightUnits;
		}
		else
		{
			OutInner = HalfWidth;
			OutOuter = HalfWidth + Shoulder;
			OutZ = 0.0f;
		}
	}

	static void BuildPlanInputs(const FVoidDesignPackage& Package, const UVoidEnvironmentSettings& Settings, const TArray<FVoidBuildingUseTokenRow>& UseTokens,
		const FVoidDistrictEnvProfileRow& DistrictProfile, VoidPlan::FInputs& Out, int32& OutRoadsUsed)
	{
		const FVoidDistrictData& District = Package.District;
		const UVoidRoadGenerationSettings* RoadSettings = GetDefault<UVoidRoadGenerationSettings>();
		const float JunctionTolerance = RoadSettings ? RoadSettings->JunctionToleranceUnits : 50.0f;
		const float RampLength = RoadSettings ? RoadSettings->RampLengthUnits : 500.0f;
		const float BridgeHeight = RoadSettings ? RoadSettings->DefaultBridgeHeightUnits : 300.0f;
		const float TunnelDepth = RoadSettings ? RoadSettings->DefaultTunnelDepthUnits : 300.0f;
		const UDataTable* ProfileTable = (RoadSettings && !RoadSettings->RoadTypeProfileTable.IsNull()) ? RoadSettings->RoadTypeProfileTable.LoadSynchronous() : nullptr;

		Out.DistrictId = District.DistrictId.Value;
		Out.District = DistrictProfile.ToPlanParams();

		TArray<FVoidBuiltRoad> BuiltRoads;
		for (const FVoidRoadSpec& Spec : District.Roads)
		{
			const bool bRound = (Spec.RoadType == EVoidRoadType::Roundabout);
			TArray<FVector> Points = bRound ? FVoidRoadSplineBuilder::BuildRoundaboutLoopPoints(Spec) : FVoidRoadSplineBuilder::BuildCenterlinePoints(Spec);
			if (Points.Num() < 2) { continue; }
			if (!bRound) { FVoidRoadBridgeTunnelBuilder::ApplyElevationRamp(Spec, Points, RampLength, BridgeHeight, TunnelDepth); }

			const FVoidRoadTypeProfile Profile = FVoidRoadTypeProfileLibrary::ResolveProfile(Spec.RoadType, ProfileTable);
			VoidPlan::FRoad R;
			R.Id = Spec.Id.Value;
			R.Tier = TierOf(Spec.RoadType);
			R.HalfWidth = ResolveWidth(Spec, ProfileTable) * 0.5f;
			BuildRoadBand(Spec, Profile, R.HalfWidth, Settings.ShoulderWidthUnits, R.BandInner, R.BandOuter, R.BandZOffset);
			R.bHasSidewalk = Spec.bHasSidewalk;
			R.bHasMedian = Spec.bHasMedian;
			R.bBridge = Spec.bIsBridge;
			R.bTunnel = Spec.bIsTunnel;
			R.bClosedLoop = bRound;
			R.Points = Points;
			Out.Roads.Add(MoveTemp(R));

			FVoidBuiltRoad Built;
			Built.Spec = Spec;
			Built.Points = MoveTemp(Points);
			BuiltRoads.Add(MoveTemp(Built));
		}
		OutRoadsUsed = Out.Roads.Num();

		// Junctions: same graph the Road generator built pads for.
		TMap<FName, const FVoidBuiltRoad*> Lookup;
		for (const FVoidBuiltRoad& B : BuiltRoads) { Lookup.Add(B.Spec.Id.Value, &B); }

		for (const FVoidRoadJunction& J : FVoidRoadIntersectionBuilder::BuildJunctionGraph(BuiltRoads, JunctionTolerance))
		{
			VoidPlan::FJunction PJ;
			PJ.Location = J.Location;
			PJ.PadRadius = J.PadRadius * (J.Type == EVoidRoadJunctionType::CulDeSac ? 1.8f : 1.0f); // matches the Road generator's cul-de-sac pad
			PJ.NumRoads = J.ConnectedRoadIds.Num();
			// Key from snapped location: stable when unrelated roads are added (index-based keys would shift).
			PJ.Key = FName(*FString::Printf(TEXT("J_%lld_%lld"), FMath::RoundToInt64(J.Location.X / 10.0), FMath::RoundToInt64(J.Location.Y / 10.0)));

			for (const FVoidElementId& Id : J.ConnectedRoadIds)
			{
				const FVoidBuiltRoad* const* Found = Lookup.Find(Id.Value);
				if (!Found || (*Found)->Points.Num() < 2) { continue; }
				const FVoidBuiltRoad& Road = **Found;
				const bool bNearStart = FVector::DistSquared(Road.Points[0], J.Location) < FVector::DistSquared(Road.Points.Last(), J.Location);
				const FVector Near = bNearStart ? Road.Points[0] : Road.Points.Last();
				const FVector Inward = bNearStart ? Road.Points[1] : Road.Points[Road.Points.Num() - 2];
				FVector2D Dir(Inward.X - Near.X, Inward.Y - Near.Y);
				if (Dir.IsNearlyZero()) { continue; }
				Dir.Normalize();

				const FVoidRoadTypeProfile Profile = FVoidRoadTypeProfileLibrary::ResolveProfile(Road.Spec.RoadType, ProfileTable);
				VoidPlan::FApproach A;
				A.Dir = Dir;
				A.RoadId = Id.Value;
				A.HalfWidth = ResolveWidth(Road.Spec, ProfileTable) * 0.5f;
				float Inner, Z;
				BuildRoadBand(Road.Spec, Profile, A.HalfWidth, Settings.ShoulderWidthUnits, Inner, A.BandOuter, Z);
				PJ.Approaches.Add(A);
			}
			Out.Junctions.Add(MoveTemp(PJ));
		}

		// Areas from building specs (footprints in the same space as road centerlines).
		for (const FVoidBuildingSpec& B : District.Buildings)
		{
			if (B.FootprintCorners.Num() < 3) { continue; }
			VoidPlan::FArea A;
			A.Id = B.Id.Value;
			ClassifyBuilding(B.BuildingType, UseTokens, A.Kind, A.Use);
			A.Polygon = B.FootprintCorners;
			A.Height = B.HeightUnits;
			Out.Areas.Add(MoveTemp(A));
		}
	}

	// ------------------------------------------------------------------
	// Rules and district profile resolution
	// ------------------------------------------------------------------

	template<typename TRow>
	static void GetSortedRows(const UDataTable* Table, TArray<TPair<FName, const TRow*>>& Out, const TCHAR* Label)
	{
		if (!Table) { return; }
		if (Table->GetRowStruct() != TRow::StaticStruct())
		{
			UE_LOG(LogVoidGenerators, Warning, TEXT("%s table '%s' does not use the expected row struct; ignoring it."), Label, *Table->GetName());
			return;
		}
		for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
		{
			Out.Add(TPair<FName, const TRow*>(Pair.Key, reinterpret_cast<const TRow*>(Pair.Value)));
		}
		Out.Sort([](const TPair<FName, const TRow*>& A, const TPair<FName, const TRow*>& B) { return A.Key.ToString() < B.Key.ToString(); });
	}

	static TArray<VoidPlan::FRule> LoadRules(const UVoidEnvironmentSettings& Settings, FName Domain, bool& bOutUsedTable)
	{
		TArray<VoidPlan::FRule> Rules;
		bOutUsedTable = false;
		const UDataTable* Table = Settings.PlacementRuleTable.IsNull() ? nullptr : Settings.PlacementRuleTable.LoadSynchronous();
		if (Table && Table->GetRowStruct() == FVoidPlacementRuleRow::StaticStruct())
		{
			bOutUsedTable = true;
			TArray<TPair<FName, const FVoidPlacementRuleRow*>> Rows;
			GetSortedRows<FVoidPlacementRuleRow>(Table, Rows, TEXT("Placement rule"));
			for (const auto& Pair : Rows)
			{
				if (Pair.Value->Domain == Domain) { Rules.Add(Pair.Value->ToPlanRule(Pair.Key)); }
			}
			return Rules;
		}
		for (const TPair<FName, FVoidPlacementRuleRow>& Pair : FVoidDressingDefaults::GetBuiltInRules())
		{
			if (Pair.Value.Domain == Domain) { Rules.Add(Pair.Value.ToPlanRule(Pair.Key)); }
		}
		return Rules;
	}

	// ------------------------------------------------------------------
	// Asset slot resolution (real assets when configured, placeholders otherwise; nothing is assumed to exist)
	// ------------------------------------------------------------------

	struct FResolvedSlot
	{
		FName SlotId;
		FName Category;
		bool bPlaceholder = true;
		UStaticMesh* Mesh = nullptr;                    // real asset (null for placeholders)
		TArray<UMaterialInterface*> Materials;
		FTransform Local = FTransform::Identity;        // real-asset local transform
		float Weight = 1.0f;
		bool bHISM = true;
		float CullStart = 0.0f;
		float CullEnd = 0.0f;
		bool bShadow = true;
		FName ReplacementTag;
		const FVoidPlaceholderDef* Placeholder = nullptr;
	};

	class FSlotResolver
	{
	public:
		void Init(const UVoidEnvironmentSettings& Settings, FVoidGenerationContext& Context)
		{
			const UDataTable* Table = Settings.AssetSlotTable.IsNull() ? nullptr : Settings.AssetSlotTable.LoadSynchronous();
			TArray<TPair<FName, const FVoidAssetSlotRow*>> Rows;
			GetSortedRows<FVoidAssetSlotRow>(Table, Rows, TEXT("Asset slot"));
			for (const auto& Pair : Rows)
			{
				const FVoidAssetSlotRow& Row = *Pair.Value;
				if (Row.Category.IsNone()) { continue; }
				FResolvedSlot S;
				S.SlotId = Pair.Key;
				S.Category = Row.Category;
				S.Weight = FMath::Max(Row.Weight, 0.0f);
				S.bHISM = Row.bUseHISM;
				S.bShadow = Row.bCastShadow;
				S.ReplacementTag = Row.ReplacementTag;
				S.Placeholder = FVoidDressingDefaults::FindPlaceholder(Row.Category);
				S.CullStart = Row.CullStartUnits > 0.0f ? Row.CullStartUnits : 0.0f;
				S.CullEnd = Row.CullEndUnits > 0.0f ? Row.CullEndUnits : (S.Placeholder ? S.Placeholder->CullEndUnits : 0.0f);
				if (!Row.Mesh.IsNull())
				{
					UStaticMesh* Mesh = Row.Mesh.LoadSynchronous();
					if (Mesh)
					{
						S.bPlaceholder = false;
						S.Mesh = Mesh;
						S.Local = FTransform(Row.LocalRotation, Row.LocalOffset, Row.ScaleMultiplier);
						for (const TSoftObjectPtr<UMaterialInterface>& M : Row.Materials) { S.Materials.Add(M.IsNull() ? nullptr : M.LoadSynchronous()); }
					}
					else
					{
						Context.GenerationValidationReport.AddWarning(
							FString::Printf(TEXT("Asset slot '%s' references mesh '%s' which could not be loaded; using the built-in placeholder for '%s'."), *Pair.Key.ToString(), *Row.Mesh.ToString(), *Row.Category.ToString()),
							TEXT("AssetSlotTable"), FName(TEXT("VOID.Env.SlotMeshMissing")), TEXT("Fix the mesh path or clear the Mesh field to use the placeholder."));
					}
				}
				if (!S.bPlaceholder || S.Placeholder) { Slots.Add(MoveTemp(S)); }
			}
		}

		/** Deterministic weighted pick: same StableId => same slot. */
		const FResolvedSlot* Pick(FName Category, FName SlotFilter, uint64 StableId)
		{
			float Total = 0.0f;
			for (const FResolvedSlot& S : Slots)
			{
				if (S.Category == Category && (SlotFilter.IsNone() || S.SlotId == SlotFilter)) { Total += S.Weight; }
			}
			if (Total > 0.0f)
			{
				float R = VoidPlan::Hash01(StableId, 50) * Total;
				const FResolvedSlot* Last = nullptr;
				for (const FResolvedSlot& S : Slots)
				{
					if (S.Category != Category || (!SlotFilter.IsNone() && S.SlotId != SlotFilter)) { continue; }
					Last = &S;
					if (R < S.Weight) { return &S; }
					R -= S.Weight;
				}
				return Last;
			}
			// Placeholder slots are heap-allocated: buckets keep raw pointers to them while more categories are added,
			// and a TMap of values could rehash and invalidate those pointers.
			if (const TSharedPtr<FResolvedSlot>* Existing = PlaceholderSlots.Find(Category)) { return (*Existing)->Placeholder ? Existing->Get() : nullptr; }
			TSharedPtr<FResolvedSlot> P = MakeShared<FResolvedSlot>();
			P->Category = Category;
			P->SlotId = FName(*FString::Printf(TEXT("Placeholder_%s"), *Category.ToString()));
			P->bPlaceholder = true;
			P->Placeholder = FVoidDressingDefaults::FindPlaceholder(Category);
			if (P->Placeholder)
			{
				P->CullEnd = P->Placeholder->CullEndUnits;
				P->bShadow = P->Placeholder->bCastShadow;
			}
			PlaceholderSlots.Add(Category, P);
			return P.Get();
		}

		UStaticMesh* GetShapeMesh(EVoidPlaceholderShape Shape)
		{
			static const TCHAR* Paths[] = {
				TEXT("/Engine/BasicShapes/Cube.Cube"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder"),
				TEXT("/Engine/BasicShapes/Sphere.Sphere"), TEXT("/Engine/BasicShapes/Cone.Cone") };
			const int32 Index = static_cast<int32>(Shape);
			if (!ShapeMeshes.IsValidIndex(Index)) { ShapeMeshes.SetNumZeroed(4); bShapeTried.SetNumZeroed(4); }
			if (!bShapeTried[Index])
			{
				bShapeTried[Index] = 1;
				ShapeMeshes[Index] = LoadObject<UStaticMesh>(nullptr, Paths[Index]);
			}
			return ShapeMeshes[Index];
		}

	private:
		TArray<FResolvedSlot> Slots;
		TMap<FName, TSharedPtr<FResolvedSlot>> PlaceholderSlots;
		TArray<UStaticMesh*> ShapeMeshes;
		TArray<uint8> bShapeTried;
	};

	// ------------------------------------------------------------------
	// Sink: cell actors + instanced buckets
	// ------------------------------------------------------------------

	struct FBucket
	{
		FName Category;
		FName SlotId;
		int32 PartIndex = 0;
		const FResolvedSlot* Slot = nullptr;
		TArray<FTransform> Transforms;
		TArray<int64> Ids;
	};

	struct FCellData
	{
		TMap<FString, FBucket> Buckets;
	};

	static UInstancedStaticMeshComponent* MakeComponent(AVoidDressingActor* Actor, const FBucket& B, UStaticMesh* Mesh, const TArray<UMaterialInterface*>& Materials, bool bHISM, float CullStart, float CullEnd, bool bShadow)
	{
		const FName CompName(*FString::Printf(TEXT("ISM_%s_%s_%d"), *B.Category.ToString(), *B.SlotId.ToString(), B.PartIndex));
		UInstancedStaticMeshComponent* Comp = bHISM
			? NewObject<UHierarchicalInstancedStaticMeshComponent>(Actor, CompName, RF_Transactional)
			: NewObject<UInstancedStaticMeshComponent>(Actor, CompName, RF_Transactional);
		Comp->SetStaticMesh(Mesh);
		Comp->SetMobility(EComponentMobility::Static);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetGenerateOverlapEvents(false);
		Comp->SetCanEverAffectNavigation(false);
		Comp->SetCastShadow(bShadow);
		for (int32 I = 0; I < Materials.Num(); ++I) { if (Materials[I]) { Comp->SetMaterial(I, Materials[I]); } }
		if (CullEnd > 0.0f) { Comp->SetCullDistances(FMath::RoundToInt(CullStart), FMath::RoundToInt(CullEnd)); }
		Comp->SetupAttachment(Actor->GetRootComponent());
		Actor->AddInstanceComponent(Comp);
		Comp->RegisterComponent();
		Comp->AddInstances(B.Transforms, /*bShouldReturnIndices=*/false, /*bWorldSpace=*/false);
		return Comp;
	}

	static void ExportJson(const FVoidDesignPackage& Package, FName Domain, const TArray<VoidPlan::FInstance>& Instances, const UVoidEnvironmentSettings& Settings, FVoidGenerationContext& Context)
	{
		FString Dir = Settings.ExportDirectory;
		if (FPaths::IsRelative(Dir)) { Dir = FPaths::Combine(FPaths::ProjectSavedDir(), Dir); }
		const FString File = FPaths::Combine(Dir, FString::Printf(TEXT("%s_%s.json"), *Package.District.DistrictId.Value.ToString(), *Domain.ToString()));

		FString Json;
		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> W = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
		W->WriteObjectStart();
		W->WriteValue(TEXT("schema"), FString(TEXT("void_placement_points_v1")));
		W->WriteValue(TEXT("district"), Package.District.DistrictId.Value.ToString());
		W->WriteValue(TEXT("domain"), Domain.ToString());
		W->WriteValue(TEXT("seed"), Settings.GlobalSeed);
		W->WriteArrayStart(TEXT("instances"));
		for (const VoidPlan::FInstance& I : Instances)
		{
			W->WriteObjectStart();
			W->WriteValue(TEXT("id"), FString::Printf(TEXT("%016llx"), I.StableId));
			W->WriteValue(TEXT("category"), I.Category.ToString());
			W->WriteValue(TEXT("rule"), I.RuleId.ToString());
			W->WriteValue(TEXT("context"), I.Context.ToString());
			W->WriteValue(TEXT("source"), I.SourceId.ToString());
			W->WriteValue(TEXT("x"), static_cast<double>(I.Location.X));
			W->WriteValue(TEXT("y"), static_cast<double>(I.Location.Y));
			W->WriteValue(TEXT("z"), static_cast<double>(I.Location.Z));
			W->WriteValue(TEXT("yaw"), static_cast<double>(I.YawDegrees));
			W->WriteValue(TEXT("scale"), static_cast<double>(I.UniformScale));
			W->WriteValue(TEXT("importance"), static_cast<double>(I.Importance));
			W->WriteObjectEnd();
		}
		W->WriteArrayEnd();
		W->WriteObjectEnd();
		W->Close();

		if (FFileHelper::SaveStringToFile(Json, *File))
		{
			Context.Log(FString::Printf(TEXT("Exported %d placement points to %s"), Instances.Num(), *File));
		}
		else
		{
			Context.GenerationValidationReport.AddWarning(FString::Printf(TEXT("Could not write placement export '%s'."), *File), TEXT("ExportDirectory"), FName(TEXT("VOID.Env.ExportFailed")));
		}
	}
}

// ===========================================================================
// FVoidDressingGeneratorBase
// ===========================================================================

FVoidDressingGeneratorBase::FVoidDressingGeneratorBase(FName InGeneratorId, FName InDomain)
	: GeneratorId(InGeneratorId)
	, Domain(InDomain)
{
}

FVoidDressingPlannedDelegate& FVoidDressingGeneratorBase::OnInstancesPlanned()
{
	static FVoidDressingPlannedDelegate Delegate;
	return Delegate;
}

bool FVoidDressingGeneratorBase::Generate(const FVoidDesignPackage& Package, FVoidGenerationContext& Context)
{
	return GenerateWithOptions(Package, Context, FVoidDressingOptions(), nullptr);
}

int32 FVoidDressingGeneratorBase::ClearGenerated(UWorld* World, FName DistrictId) const
{
	if (!World) { return 0; }
	TArray<AVoidDressingActor*> ToDestroy;
	for (TActorIterator<AVoidDressingActor> It(World); It; ++It)
	{
		AVoidDressingActor* Actor = *It;
		if (IsValid(Actor) && Actor->GeneratorId == GeneratorId && (DistrictId.IsNone() || Actor->DistrictId == DistrictId))
		{
			ToDestroy.Add(Actor);
		}
	}
	for (AVoidDressingActor* Actor : ToDestroy) { World->DestroyActor(Actor); }
	return ToDestroy.Num();
}

bool FVoidDressingGeneratorBase::GenerateWithOptions(const FVoidDesignPackage& Package, FVoidGenerationContext& Context, const FVoidDressingOptions& Options, FVoidDressingRunResult* OutResult)
{
	using namespace VoidDressingPrivate;

	const double StartSeconds = FPlatformTime::Seconds();
	FVoidDressingRunResult LocalResult;
	FVoidDressingRunResult& Result = OutResult ? *OutResult : LocalResult;
	Result = FVoidDressingRunResult();

	const FName DistrictId = Package.District.DistrictId.Value;
	const UVoidEnvironmentSettings* Settings = GetDefault<UVoidEnvironmentSettings>();
	check(Settings);

	if (!Context.TargetWorld)
	{
		Context.Log(FString::Printf(TEXT("[%s] Aborting: no target world provided."), *GeneratorId.ToString()));
		UE_LOG(LogVoidGenerators, Error, TEXT("%s generator called with a null TargetWorld."), *GeneratorId.ToString());
		return false;
	}

	// --- Data: district profile, use tokens, rules ---------------------------------------------
	FVoidDistrictEnvProfileRow Profile = FVoidDressingDefaults::GetBuiltInDistrictProfile(DistrictId);
	if (const UDataTable* ProfTable = Settings->DistrictProfileTable.IsNull() ? nullptr : Settings->DistrictProfileTable.LoadSynchronous())
	{
		if (ProfTable->GetRowStruct() == FVoidDistrictEnvProfileRow::StaticStruct())
		{
			if (const FVoidDistrictEnvProfileRow* Row = ProfTable->FindRow<FVoidDistrictEnvProfileRow>(DistrictId, TEXT("VoidDistrictProfile"), false)) { Profile = *Row; }
		}
	}

	TArray<FVoidBuildingUseTokenRow> UseTokens;
	if (const UDataTable* TokTable = Settings->BuildingUseTokenTable.IsNull() ? nullptr : Settings->BuildingUseTokenTable.LoadSynchronous())
	{
		if (TokTable->GetRowStruct() == FVoidBuildingUseTokenRow::StaticStruct())
		{
			for (const FName& RowName : TokTable->GetRowNames())
			{
				if (const FVoidBuildingUseTokenRow* Row = TokTable->FindRow<FVoidBuildingUseTokenRow>(RowName, TEXT("VoidUseTokens"), false)) { UseTokens.Add(*Row); }
			}
		}
	}
	if (UseTokens.Num() == 0) { UseTokens = FVoidDressingDefaults::GetBuiltInUseTokens(); }

	bool bRulesFromTable = false;
	const TArray<VoidPlan::FRule> Rules = LoadRules(*Settings, Domain, bRulesFromTable);

	// --- Plan ---------------------------------------------------------------------------------
	VoidPlan::FInputs Inputs;
	int32 NumRoads = 0;
	BuildPlanInputs(Package, *Settings, UseTokens, Profile, Inputs, NumRoads);

	Inputs.GlobalSeed = static_cast<uint64>(static_cast<uint32>(Options.bOverrideSeed ? Options.SeedOverride : Settings->GlobalSeed));
	Inputs.DensityScale = Options.DensityScaleOverride >= 0.0f ? Options.DensityScaleOverride : Settings->DensityScale;
	Inputs.MinImportanceDensity = Settings->MinImportanceDensity;
	Inputs.FocusRadiusUnits = Settings->FocusRadiusUnits;
	Inputs.MaxInstances = Options.MaxInstancesOverride >= 0 ? Options.MaxInstancesOverride : Settings->MaxInstancesPerRun;
	Inputs.ExtraFocusPoints = Settings->CinematicFocusPoints;
	Inputs.bLeftHandTraffic = Settings->bLeftHandTraffic;
	Inputs.bUseBounds = Options.bUseBounds;
	Inputs.BoundsMin = Options.BoundsMin;
	Inputs.BoundsMax = Options.BoundsMax;
	Inputs.CategoryAllowList = Options.CategoryAllowList;
	Inputs.IsCancelled = [&Context]() { return Context.IsCancelled(); };

	Context.Log(FString::Printf(TEXT("[%s] District '%s': %d roads, %d junctions, %d areas/buildings, %d rules (%s), polish %.2f."),
		*GeneratorId.ToString(), *DistrictId.ToString(), NumRoads, Inputs.Junctions.Num(), Inputs.Areas.Num(), Rules.Num(),
		bRulesFromTable ? TEXT("from table") : TEXT("built-in"), Inputs.District.Polish));

	if (NumRoads == 0 && Inputs.Areas.Num() == 0)
	{
		Context.GenerationValidationReport.AddWarning(FString::Printf(TEXT("[%s] Package has no usable roads or buildings; nothing to dress."), *GeneratorId.ToString()),
			TEXT("District"), FName(TEXT("VOID.Env.NothingToDress")), TEXT("Import a package with roads and/or building footprints."));
		Result.ElapsedMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
		return false;
	}

	const TArray<VoidPlan::FInstance> Instances = VoidPlan::Plan(Inputs, Rules, &Result.PlanStats);
	Result.bCancelled = Result.PlanStats.bCancelled;
	Result.LogicalInstances = Instances.Num();

	if (Result.bCancelled)
	{
		Context.Log(FString::Printf(TEXT("[%s] Cancelled during planning; previous generation left untouched."), *GeneratorId.ToString()));
		return false;
	}

	OnInstancesPlanned().Broadcast(Domain, Instances);
	if (Settings->bExportPlacementJson || Options.bForceExportJson)
	{
		ExportJson(Package, Domain, Instances, *Settings, Context);
	}

	// Rejection summary (why things were NOT placed is as useful as what was).
	const VoidPlan::FStats& St = Result.PlanStats;
	Context.Log(FString::Printf(TEXT("[%s] Planned %d instances from %d candidates. Rejected: bounds %d, context %d, junction clearance %d, inside building %d, density/priority %d, rule cap %d, budget %d."),
		*GeneratorId.ToString(), St.Kept, St.Candidates, St.RejectedBounds, St.RejectedContext, St.RejectedJunctionClearance, St.RejectedInsideBuilding, St.RejectedDensity, St.RejectedRuleCap, St.RejectedBudget));
	if (St.RejectedBudget > 0)
	{
		Context.GenerationValidationReport.AddWarning(FString::Printf(TEXT("[%s] Instance budget (%d) exceeded; %d lowest-priority instances were dropped."), *GeneratorId.ToString(), Inputs.MaxInstances, St.RejectedBudget),
			TEXT("MaxInstancesPerRun"), FName(TEXT("VOID.Env.BudgetExceeded")), TEXT("Raise MaxInstancesPerRun or lower DensityScale."));
	}

	if (Options.bDryRun)
	{
		Result.bSucceeded = true;
		Result.ElapsedMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
		Context.Log(FString::Printf(TEXT("[%s] Dry run finished in %.2f ms; nothing spawned."), *GeneratorId.ToString(), Result.ElapsedMs));
		return true;
	}

	// --- Regenerate: remove what this generator previously owned for this district, then rebuild ---------
	const FScopedTransaction Transaction(NSLOCTEXT("VoidDressingGenerator", "GenerateTransaction", "Generate VOID Environment / Props"));
	Result.ActorsRemoved = ClearGenerated(Context.TargetWorld, DistrictId);

	FSlotResolver Resolver;
	Resolver.Init(*Settings, Context);

	TMap<FIntPoint, FCellData> Cells;
	TSet<FName> ReportedMissing;
	TSet<FName> PlaceholderCategories;
	const float CellSize = FMath::Max(Settings->CellSizeUnits, 1000.0f);

	for (const VoidPlan::FInstance& Inst : Instances)
	{
		const FResolvedSlot* Slot = Resolver.Pick(Inst.Category, Inst.SlotFilter, Inst.StableId);
		if (!Slot || (Slot->bPlaceholder && !Slot->Placeholder))
		{
			if (!ReportedMissing.Contains(Inst.Category))
			{
				ReportedMissing.Add(Inst.Category);
				Context.GenerationValidationReport.AddWarning(FString::Printf(TEXT("Category '%s' has no asset slot and no built-in placeholder; its instances were skipped."), *Inst.Category.ToString()),
					TEXT("AssetSlotTable"), FName(TEXT("VOID.Env.NoAssetForCategory")), TEXT("Add an FVoidAssetSlotRow for this Category."));
			}
			continue;
		}
		if (Slot->bPlaceholder) { PlaceholderCategories.Add(Inst.Category); }

		const FIntPoint CellKey(FMath::FloorToInt(Inst.Location.X / CellSize), FMath::FloorToInt(Inst.Location.Y / CellSize));
		FCellData& Cell = Cells.FindOrAdd(CellKey);
		const FTransform InstXf(FRotator(0.0f, Inst.YawDegrees, 0.0f), Inst.Location, FVector(Inst.UniformScale));

		const int32 NumParts = Slot->bPlaceholder ? Slot->Placeholder->Parts.Num() : 1;
		for (int32 P = 0; P < NumParts; ++P)
		{
			const FString Key = FString::Printf(TEXT("%s|%s|%d"), *Slot->Category.ToString(), *Slot->SlotId.ToString(), P);
			FBucket& B = Cell.Buckets.FindOrAdd(Key);
			if (!B.Slot)
			{
				B.Slot = Slot;
				B.Category = Slot->Category;
				B.SlotId = Slot->SlotId;
				B.PartIndex = P;
			}
			FTransform Local = Slot->Local;
			if (Slot->bPlaceholder)
			{
				const FVoidPlaceholderPart& Part = Slot->Placeholder->Parts[P];
				Local = FTransform(Part.Rotation, Part.Offset, Part.Scale);
			}
			B.Transforms.Add(Local * InstXf);
			if (P == 0) { B.Ids.Add(static_cast<int64>(Inst.StableId)); }
		}
	}

	TArray<FIntPoint> CellKeys;
	Cells.GenerateKeyArray(CellKeys);
	CellKeys.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.X != B.X ? A.X < B.X : A.Y < B.Y; });

	for (const FIntPoint& CellKey : CellKeys)
	{
		if (Context.IsCancelled())
		{
			Context.Log(FString::Printf(TEXT("[%s] Cancelled during spawn; the world now holds a partial generation. Re-run to complete it."), *GeneratorId.ToString()));
			Result.bCancelled = true;
			break;
		}

		FCellData& Cell = Cells[CellKey];
		FActorSpawnParameters Params;
		Params.Name = MakeUniqueObjectName(Context.TargetWorld, AVoidDressingActor::StaticClass(),
			FName(*FString::Printf(TEXT("VoidDressing_%s_%s_%d_%d"), *GeneratorId.ToString(), *DistrictId.ToString(), CellKey.X, CellKey.Y)));
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AVoidDressingActor* Actor = Context.TargetWorld->SpawnActor<AVoidDressingActor>(AVoidDressingActor::StaticClass(), FTransform::Identity, Params);
		if (!Actor) { continue; }

		Actor->GeneratorId = GeneratorId;
		Actor->DistrictId = DistrictId;
		Actor->Cell = CellKey;
		Actor->Seed = static_cast<int32>(Inputs.GlobalSeed);
		Actor->Tags.Add(FName(TEXT("VOIDGen")));
		Actor->Tags.Add(FName(*FString::Printf(TEXT("VOIDGen.%s"), *GeneratorId.ToString())));
		Actor->Tags.Add(FName(*FString::Printf(TEXT("VOIDDistrict.%s"), *DistrictId.ToString())));
#if WITH_EDITOR
		Actor->SetActorLabel(FString::Printf(TEXT("VoidDressing_%s_%s_%d_%d"), *GeneratorId.ToString(), *DistrictId.ToString(), CellKey.X, CellKey.Y));
		Actor->SetFolderPath(FName(*FString::Printf(TEXT("VOID/%s/%s"), *GeneratorId.ToString(), *DistrictId.ToString())));
#endif

		TArray<FString> BucketKeys;
		Cell.Buckets.GenerateKeyArray(BucketKeys);
		BucketKeys.Sort();
		for (const FString& BucketKey : BucketKeys)
		{
			FBucket& B = Cell.Buckets[BucketKey];
			const FResolvedSlot& Slot = *B.Slot;

			UStaticMesh* Mesh = nullptr;
			TArray<UMaterialInterface*> Materials;
			if (Slot.bPlaceholder)
			{
				Mesh = Resolver.GetShapeMesh(Slot.Placeholder->Parts[B.PartIndex].Shape);
			}
			else
			{
				Mesh = Slot.Mesh;
				Materials = Slot.Materials;
			}
			if (!Mesh)
			{
				Context.GenerationValidationReport.AddWarning(TEXT("Engine BasicShapes meshes could not be loaded; placeholder instances were skipped."), TEXT("Placeholders"), FName(TEXT("VOID.Env.BasicShapesMissing")));
				continue;
			}

			UInstancedStaticMeshComponent* Comp = MakeComponent(Actor, B, Mesh, Materials, Slot.bHISM, Slot.CullStart, Slot.CullEnd, Slot.bShadow);
			Comp->ComponentTags.Add(FName(*FString::Printf(TEXT("VOID.Category.%s"), *B.Category.ToString())));
			Comp->ComponentTags.Add(FName(*FString::Printf(TEXT("VOID.Slot.%s"), *B.SlotId.ToString())));
			if (Slot.bPlaceholder) { Comp->ComponentTags.Add(FName(TEXT("VOID.Placeholder"))); }
			if (!Slot.ReplacementTag.IsNone()) { Comp->ComponentTags.Add(Slot.ReplacementTag); }

			FVoidDressingBucketInfo Info;
			Info.Category = B.Category;
			Info.SlotId = B.SlotId;
			Info.PartIndex = B.PartIndex;
			Info.bPlaceholder = Slot.bPlaceholder;
			Info.ReplacementTag = Slot.ReplacementTag;
			Info.Component = Comp;
			Info.InstanceIds = B.Ids;
			Actor->Buckets.Add(MoveTemp(Info));

			++Result.ComponentsCreated;
			Result.InstancesCreated += B.Transforms.Num();
		}
		++Result.ActorsSpawned;
	}

	Result.PlaceholderCategoriesUsed = PlaceholderCategories.Num();
	Result.MissingCategories = ReportedMissing.Num();
	if (PlaceholderCategories.Num() > 0)
	{
		Context.GenerationValidationReport.AddInfo(FString::Printf(TEXT("[%s] %d categories are using built-in placeholder meshes; add FVoidAssetSlotRow entries to replace them."), *GeneratorId.ToString(), PlaceholderCategories.Num()),
			TEXT("AssetSlotTable"), FName(TEXT("VOID.Env.PlaceholdersInUse")));
	}

	Result.ElapsedMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
	Result.bSucceeded = !Result.bCancelled && (Result.ActorsSpawned > 0 || Instances.Num() == 0);

	FString CategorySummary;
	for (const TPair<FName, int32>& Pair : St.KeptPerCategory) { CategorySummary += FString::Printf(TEXT(" %s=%d"), *Pair.Key.ToString(), Pair.Value); }
	Context.Log(FString::Printf(TEXT("[%s] Replaced %d previous actors; spawned %d cell actors, %d instanced components, %d mesh instances for %d placements in %.2f ms.%s"),
		*GeneratorId.ToString(), Result.ActorsRemoved, Result.ActorsSpawned, Result.ComponentsCreated, Result.InstancesCreated, Result.LogicalInstances, Result.ElapsedMs, *CategorySummary));
	UE_LOG(LogVoidGenerators, Log, TEXT("%s generation finished for district '%s': %d placements, %d components, %d actors, %.2f ms."),
		*GeneratorId.ToString(), *DistrictId.ToString(), Result.LogicalInstances, Result.ComponentsCreated, Result.ActorsSpawned, Result.ElapsedMs);

	return Result.bSucceeded;
}
