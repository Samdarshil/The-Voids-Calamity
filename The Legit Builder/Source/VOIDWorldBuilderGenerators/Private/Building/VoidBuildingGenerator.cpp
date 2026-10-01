// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Building/VoidBuildingGenerator.h"
#include "Building/VoidBuildingActor.h"
#include "Building/VoidBuildingGenerationSettings.h"
#include "Building/VoidBuildingNormalizer.h"
#include "VoidWorldBuilderGeneratorsLog.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ScopedTransaction.h"
#include "HAL/PlatformTime.h"

FName FVoidBuildingGenerator::GetGeneratorId() const
{
	return FName(TEXT("Building"));
}

int32 FVoidBuildingGenerator::RemoveGenerated(UWorld* World, FName DistrictId)
{
	if (!World) { return 0; }
	const FName DistrictTag = AVoidBuildingBatchActor::MakeDistrictTag(DistrictId);

	TArray<AActor*> ToDestroy;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->ActorHasTag(AVoidBuildingBatchActor::GeneratedTag) && Actor->ActorHasTag(DistrictTag))
		{
			ToDestroy.Add(Actor);
		}
	}
	for (AActor* Actor : ToDestroy)
	{
		World->DestroyActor(Actor);
	}
	return ToDestroy.Num();
}

bool FVoidBuildingGenerator::Generate(const FVoidDesignPackage& Package, FVoidGenerationContext& Context)
{
	const double Start = FPlatformTime::Seconds();
	const FVoidDistrictData& District = Package.District;

	if (!Context.TargetWorld)
	{
		Context.Log(TEXT("Aborting: no target world provided."));
		UE_LOG(LogVoidGenerators, Error, TEXT("FVoidBuildingGenerator::Generate called with a null TargetWorld."));
		return false;
	}

	const UVoidBuildingGenerationSettings* Settings = GetDefault<UVoidBuildingGenerationSettings>();
	const FVoidBuildingGenerationParams Params = Settings ? Settings->ToParams() : FVoidBuildingGenerationParams();

	Context.Log(FString::Printf(TEXT("Starting building generation for district '%s' (%d buildings, %d roads)."),
		*District.DistrictId.Value.ToString(), District.Buildings.Num(), District.Roads.Num()));

	TArray<FVoidBuildingBatchData> Batches;
	FVoidBuildingRunStats Stats;
	FVoidValidationReport Report;
	BuildBatches(District, Params, Report, Batches, Stats, [&Context]() { return Context.IsCancelled(); });

	// Merge into the context report (the Road Generator may already have written to it in the same run).
	for (const FVoidValidationIssue& Issue : Report.Issues)
	{
		Context.GenerationValidationReport.Issues.Add(Issue);
	}
	Context.GenerationValidationReport.bIsValid = (Context.GenerationValidationReport.NumErrors() == 0 && Context.GenerationValidationReport.NumFatal() == 0);

	if (Stats.NumGenerated == 0)
	{
		Context.Log(TEXT("No buildings generated. Existing generated buildings for this district were left untouched."));
		return false;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("VoidBuildingGenerator", "GenerateBuildingsTransaction", "Generate VOID Buildings"));

	// Regeneration: replace the previous run for this district.
	const int32 NumRemoved = RemoveGenerated(Context.TargetWorld, District.DistrictId.Value);
	if (NumRemoved > 0) { Context.Log(FString::Printf(TEXT("Removed %d actor(s) from the previous run."), NumRemoved)); }

	UMaterialInterface* Material = (Settings && !Settings->GreyboxMaterial.IsNull()) ? Settings->GreyboxMaterial.LoadSynchronous() : nullptr;
	const FName DistrictTag = AVoidBuildingBatchActor::MakeDistrictTag(District.DistrictId.Value);

	int32 NumActors = 0;
	for (FVoidBuildingBatchData& Batch : Batches)
	{
		FActorSpawnParameters Spawn;
		Spawn.Name = FName(*FString::Printf(TEXT("VoidBuildings_%s_%d_%d"), *District.DistrictId.Value.ToString(), Batch.Cell.X, Batch.Cell.Y));
		Spawn.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested; // stable name when free; never fails on a not-yet-collected old actor
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AVoidBuildingBatchActor* Actor = Context.TargetWorld->SpawnActor<AVoidBuildingBatchActor>(AVoidBuildingBatchActor::StaticClass(), FTransform(Batch.Origin), Spawn);
		if (!Actor)
		{
			Context.Log(FString::Printf(TEXT("Failed to spawn batch (%d,%d)."), Batch.Cell.X, Batch.Cell.Y));
			continue;
		}
#if WITH_EDITOR
		Actor->SetActorLabel(FString::Printf(TEXT("VoidBuildings_%s_%d_%d"), *District.DistrictId.Value.ToString(), Batch.Cell.X, Batch.Cell.Y));
#endif
		Actor->Tags.AddUnique(AVoidBuildingBatchActor::GeneratedTag);
		Actor->Tags.AddUnique(DistrictTag);
		Actor->DistrictId = District.DistrictId.Value;
		Actor->BatchCell = Batch.Cell;
		Actor->GenerationSignature = Batch.Signature;
		Actor->Buildings = Batch.Metadata;

		const TArray<FProcMeshTangent> NoTangents;
		for (int32 S = 0; S < static_cast<int32>(EVoidBuildingSurface::Count); ++S)
		{
			const FVoidBuildingMeshSection& Section = Batch.Mesh.Sections[S];
			if (Section.IsEmpty()) { continue; }
			const EVoidBuildingSurface Surface = static_cast<EVoidBuildingSurface>(S);
			const bool bCollide = Params.bGenerateCollision
				&& (Surface == EVoidBuildingSurface::Wall || Surface == EVoidBuildingSurface::Roof || Surface == EVoidBuildingSurface::Foundation);
			Actor->BuildingMesh->CreateMeshSection_LinearColor(S, Section.Vertices, Section.Triangles, Section.Normals, Section.UVs, Section.VertexColors, NoTangents, bCollide);
			if (Material) { Actor->BuildingMesh->SetMaterial(S, Material); }
		}
		++NumActors;

		// Asset-replacement hook: spawn the requested class in place of the greybox.
		for (const TPair<FName, FTransform>& Override : Batch.Overrides)
		{
			const FString* Path = Params.AssetOverrideClassPaths.Find(Override.Key);
			UClass* Class = Path ? StaticLoadClass(AActor::StaticClass(), nullptr, **Path) : nullptr;
			if (!Class)
			{
				Context.Log(FString::Printf(TEXT("Asset override for '%s' could not be loaded; building left empty."), *Override.Key.ToString()));
				continue;
			}
			FActorSpawnParameters OverrideSpawn;
			OverrideSpawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AActor* Replacement = Context.TargetWorld->SpawnActor<AActor>(Class, Override.Value, OverrideSpawn))
			{
				Replacement->Tags.AddUnique(AVoidBuildingBatchActor::GeneratedTag);
				Replacement->Tags.AddUnique(DistrictTag);
				Replacement->Tags.AddUnique(FName(*FString::Printf(TEXT("VoidBuilding:%s"), *Override.Key.ToString())));
			}
		}
	}

	FString Breakdown;
	for (int32 C = 0; C < 11; ++C)
	{
		if (Stats.CategoryCounts[C] > 0)
		{
			Breakdown += FString::Printf(TEXT("%s=%d "), FVoidBuildingNormalizer::CategoryToString(static_cast<EVoidBuildingCategory>(C)), Stats.CategoryCounts[C]);
		}
	}
	const double ElapsedMs = (FPlatformTime::Seconds() - Start) * 1000.0;
	Context.Log(FString::Printf(TEXT("Built %d of %d buildings in %d batch actor(s), %d triangles. Rejected: %d, skipped (road conflict): %d, adjusted: %d, no frontage: %d, landmarks: %d."),
		Stats.NumGenerated, Stats.NumInput, NumActors, Stats.NumTriangles, Stats.NumRejected, Stats.NumSkippedRoad, Stats.NumAdjusted, Stats.NumNoFrontage, Stats.NumLandmarks));
	Context.Log(FString::Printf(TEXT("Categories: %s"), *Breakdown));
	Context.Log(FString::Printf(TEXT("Building generation finished in %.2f ms."), ElapsedMs));
	UE_LOG(LogVoidGenerators, Log, TEXT("Building generation finished for district '%s': %d buildings, %d batches, %d tris, %.2f ms."),
		*District.DistrictId.Value.ToString(), Stats.NumGenerated, NumActors, Stats.NumTriangles, ElapsedMs);

	return NumActors > 0;
}
