// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VoidGeneratedWorldValidator.h"
#include "Data/VoidGeneratedTags.h"
#include "Road/VoidRoadActor.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/InstancedStaticMeshComponent.h"

namespace
{
	const FName RoadGenerator(TEXT("Road"));
	const FName RoleRoad(TEXT("Road"));
	const FName RoleJunction(TEXT("Junction"));

	bool IsRoadDesignObject(const FVoidRoadSpec& Spec)
	{
		if (!Spec.Id.IsValid()) { return false; }
		if (Spec.RoadType == EVoidRoadType::Roundabout) { return Spec.CenterlinePoints.Num() >= 1 && Spec.RoundaboutRadiusUnits > 0.0f; }
		return Spec.CenterlinePoints.Num() >= 2;
	}
}

void FVoidGeneratedWorldValidator::Validate(EVoidValidationStage Stage, const FVoidValidationInput& Input, FVoidValidationContext& Context) const
{
	if (!Input.World)
	{
		Context.Info(TEXT("VOID.World.NoWorld"), TEXT("World validation skipped: no target world was provided."));
		return;
	}
	ValidateSnapshot(Stage, BuildSnapshot(Input.World), Input, Context);
}

void FVoidGeneratedWorldValidator::ValidateSnapshot(EVoidValidationStage Stage, const FVoidWorldSnapshot& Snapshot, const FVoidValidationInput& Input, FVoidValidationContext& Ctx)
{
	const FVoidValidationOptions& Opt = Ctx.Options;
	const bool bFinal = (Stage == EVoidValidationStage::Final);

	// Index road actors by design id; count claims per (generator, id).
	TMap<FString, int32> Claims;
	TSet<FString> RoadActorIds;
	for (const FVoidActorSnapshot& A : Snapshot.Actors)
	{
		if (!A.GeneratorId.IsNone() && !A.ObjectId.IsEmpty())
		{
			Claims.FindOrAdd(A.GeneratorId.ToString() + TEXT("|") + A.ObjectId, 0) += 1;
			if (A.GeneratorId == RoadGenerator && A.Role == RoleRoad) { RoadActorIds.Add(A.ObjectId); }
		}
	}

	TSet<FString> PackageRoadIds;
	if (Input.Package)
	{
		for (const FVoidRoadSpec& R : Input.Package->District.Roads) { if (R.Id.IsValid()) { PackageRoadIds.Add(R.Id.Value.ToString()); } }
	}

	// ---- per-actor checks ------------------------------------------------
	for (const FVoidActorSnapshot& A : Snapshot.Actors)
	{
		const bool bRoad = A.GeneratorId == RoadGenerator;
		if (!bFinal && !bRoad) { continue; }

		const FString Who = A.ObjectId.IsEmpty() ? A.ActorName : A.ObjectId;
		const FVector* Where = A.bTransformFinite ? &A.Location : nullptr;

		if (!A.bTransformFinite || A.Location.ContainsNaN() || A.Scale.ContainsNaN())
		{
			Ctx.Error(TEXT("VOID.World.InvalidTransform"), FString::Printf(TEXT("Actor '%s' (%s) has a NaN/infinite location or scale."), *A.ActorName, *A.ClassName.ToString()), Who, A.ActorName, TEXT("The generator produced a bad transform; check its input geometry for degenerate values."));
		}
		else
		{
			if (A.Location.GetAbsMax() > Opt.MaxCoordinateAbs)
			{
				Ctx.Error(TEXT("VOID.World.InvalidTransform"), FString::Printf(TEXT("Actor '%s' is at %s, beyond the plausible world extent (%.0f)."), *A.ActorName, *A.Location.ToString(), Opt.MaxCoordinateAbs), Who, A.ActorName, TEXT("Check units and origin handling in the generator."), Where);
			}
			if (FMath::IsNearlyZero(A.Scale.X) || FMath::IsNearlyZero(A.Scale.Y) || FMath::IsNearlyZero(A.Scale.Z))
			{
				Ctx.Error(TEXT("VOID.World.InvalidTransform"), FString::Printf(TEXT("Actor '%s' has a zero scale component (%s); it is invisible and non-collidable."), *A.ActorName, *A.Scale.ToString()), Who, A.ActorName, TEXT("Do not scale generated actors to zero; skip generating them instead."), Where);
			}
			else if (A.Scale.GetAbsMax() > 1.0e5)
			{
				Ctx.Warn(TEXT("VOID.World.InvalidTransform"), FString::Printf(TEXT("Actor '%s' has an extreme scale (%s)."), *A.ActorName, *A.Scale.ToString()), Who, A.ActorName, TEXT("Check the generator's unit handling."), Where);
			}
		}

		if (A.Role == RoleRoad)
		{
			if (A.NumMeshSections == 0)
			{
				Ctx.Error(TEXT("VOID.World.EmptyGeometry"), FString::Printf(TEXT("Road actor '%s' has a procedural mesh with zero sections: nothing will render or collide."), *A.ActorName), Who, A.ActorName, TEXT("Check the road's width/points; the ribbon builder returns empty geometry for degenerate input."), Where);
			}
			if (A.NumSplinePoints >= 0 && A.NumSplinePoints < 2)
			{
				Ctx.Error(TEXT("VOID.World.InvalidSpline"), FString::Printf(TEXT("Road actor '%s' has %d spline point(s); a road needs at least 2."), *A.ActorName, A.NumSplinePoints), Who, A.ActorName, TEXT("Check the road's centerlinePoints."), Where);
			}
			if (A.bInstancesMissingMesh)
			{
				Ctx.Error(TEXT("VOID.World.MissingAsset"), FString::Printf(TEXT("Road actor '%s' has %d bridge/tunnel marker instance(s) but no static mesh assigned (expected /Engine/BasicShapes/Cube)."), *A.ActorName, A.NumInstances), Who, A.ActorName, TEXT("The engine BasicShapes content failed to load in the actor constructor; confirm the engine content is available."), Where);
			}
		}
		else if (A.Role == RoleJunction)
		{
			if (A.NumMeshSections == 0)
			{
				Ctx.Error(TEXT("VOID.World.EmptyGeometry"), FString::Printf(TEXT("Junction actor '%s' has zero mesh sections."), *A.ActorName), Who, A.ActorName, TEXT("Check the junction pad radius (widest connected road)."), Where);
			}
			for (const FString& Ref : A.ReferencedIds)
			{
				if (!RoadActorIds.Contains(Ref) && !PackageRoadIds.Contains(Ref))
				{
					Ctx.Error(TEXT("VOID.World.DanglingReference"), FString::Printf(TEXT("Junction '%s' references road '%s', which exists neither as a generated actor nor in the design package."), *A.ActorName, *Ref), Who, A.ActorName, TEXT("The junction is stale (left over from a previous generation) - delete the generated actors and regenerate."), Where);
				}
			}
		}

		if (A.GeneratorId == TEXT("Building") && A.bBoundsValid && A.Bounds.Min.Z > Opt.FloatingToleranceUnits)
		{
			const FVector Loc = A.Bounds.GetCenter();
			Ctx.Warn(TEXT("VOID.World.FloatingGeometry"), FString::Printf(TEXT("Building actor '%s' bottoms out %.0f units above the ground plane."), *A.ActorName, A.Bounds.Min.Z), Who, A.ActorName, TEXT("Snap the base to terrain/ground Z or set the building's elevation."), &Loc);
		}
	}

	// ---- duplicates ------------------------------------------------------
	for (const TPair<FString, int32>& C : Claims)
	{
		if (C.Value > 1 && (bFinal || C.Key.StartsWith(TEXT("Road|"))))
		{
			FString Gen, Id;
			C.Key.Split(TEXT("|"), &Gen, &Id);
			Ctx.Error(TEXT("VOID.World.DuplicateActor"), FString::Printf(TEXT("%d actors claim to be generator '%s' object '%s'. Generators are not idempotent: running generation twice spawns a second copy."), C.Value, *Gen, *Id), Id, FString(), TEXT("Delete the generated actors before regenerating (or add a cleanup step to the generator)."));
		}
	}

	// ---- expectations from the package ----------------------------------
	if (Input.Package && Input.ExecutedGenerators.Contains(RoadGenerator))
	{
		int32 Expected = 0;
		for (int32 i = 0; i < Input.Package->District.Roads.Num(); ++i)
		{
			const FVoidRoadSpec& R = Input.Package->District.Roads[i];
			if (!IsRoadDesignObject(R)) { continue; }
			++Expected;
			if (!RoadActorIds.Contains(R.Id.Value.ToString()))
			{
				Ctx.Error(TEXT("VOID.World.MissingActor"), FString::Printf(TEXT("Road '%s' is in the design package but no generated road actor exists for it."), *R.Id.Value.ToString()), R.Id.Value.ToString(), FString::Printf(TEXT("district.roads[%d]"), i), TEXT("Check the Road generator log for a skipped/failed spawn for this id."));
			}
		}
		if (Expected > 0 && RoadActorIds.Num() == 0)
		{
			Ctx.Error(TEXT("VOID.World.GeneratorProducedNothing"), FString::Printf(TEXT("The Road generator ran but the world contains no road actors, although the package defines %d generatable road(s)."), Expected), FString(), FString(), TEXT("Check the Road generator's return value and log; also check the world it targeted."));
		}
		for (const FString& Id : RoadActorIds)
		{
			if (!PackageRoadIds.Contains(Id))
			{
				Ctx.Warn(TEXT("VOID.World.OrphanActor"), FString::Printf(TEXT("Road actor for id '%s' exists in the world but not in the design package (stale from an earlier generation?)."), *Id), Id, FString(), TEXT("Delete it, or re-import the package version that defines it."));
			}
		}
	}

	if (bFinal)
	{
		TMap<FName, int32> PerGenerator;
		int32 UntaggedVoid = 0;
		for (const FVoidActorSnapshot& A : Snapshot.Actors)
		{
			if (!A.GeneratorId.IsNone()) { PerGenerator.FindOrAdd(A.GeneratorId, 0) += 1; }
			else if (A.bVoidClass && !A.bTagged) { ++UntaggedVoid; }
		}
		for (const FName& G : Input.ExecutedGenerators)
		{
			if (G != RoadGenerator && !PerGenerator.Contains(G))
			{
				Ctx.Warn(TEXT("VOID.World.GeneratorProducedNothing"), FString::Printf(TEXT("Generator '%s' ran but no actors attributed to it were found. Either it produced nothing, or it does not tag its actors."), *G.ToString()), G.ToString(), FString(), TEXT("Have the generator call FVoidGeneratedTags::Apply on every actor it spawns."));
			}
		}
		if (UntaggedVoid > 0)
		{
			Ctx.Info(TEXT("VOID.World.UntaggedActors"), FString::Printf(TEXT("%d VOID actor(s) carry no VOID.Generated tag and cannot be attributed, de-duplicated or cleaned up by tooling."), UntaggedVoid), FString(), FString(), TEXT("Call FVoidGeneratedTags::Apply when spawning."));
		}
	}
}

FVoidWorldSnapshot FVoidGeneratedWorldValidator::BuildSnapshot(UWorld* World)
{
	FVoidWorldSnapshot Snapshot;
	if (!World) { return Snapshot; }

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor)) { continue; }

		FVoidActorSnapshot S;
		S.ActorName = Actor->GetName();
		S.ClassName = Actor->GetClass()->GetFName();
		const FString ClassString = S.ClassName.ToString();
		S.bVoidClass = ClassString.StartsWith(TEXT("Void")) || ClassString.StartsWith(TEXT("AVoid"));
		S.Location = Actor->GetActorLocation();
		S.Scale = Actor->GetActorScale3D();
		S.bTransformFinite = !S.Location.ContainsNaN() && !S.Scale.ContainsNaN();
		const FBox Box = Actor->GetComponentsBoundingBox(true);
		S.Bounds = Box;
		S.bBoundsValid = Box.IsValid != 0;

		FName TagGenerator; FString TagId;
		S.bTagged = FVoidGeneratedTags::TryRead(Actor, TagGenerator, TagId);
		S.GeneratorId = TagGenerator;
		S.ObjectId = TagId;

		// Adapter for the untagged Phase 3 road actors.
		if (const AVoidRoadActor* Road = Cast<AVoidRoadActor>(Actor))
		{
			if (S.GeneratorId.IsNone()) { S.GeneratorId = RoadGenerator; }
			if (S.ObjectId.IsEmpty()) { S.ObjectId = Road->RoadId.Value.IsNone() ? FString() : Road->RoadId.Value.ToString(); }
			S.Role = RoleRoad;
			S.NumSplinePoints = Road->RoadSpline ? Road->RoadSpline->GetNumberOfSplinePoints() : 0;
			S.NumMeshSections = Road->RoadMesh ? Road->RoadMesh->GetNumSections() : 0;
			if (Road->StructureMarkers)
			{
				S.NumInstances = Road->StructureMarkers->GetInstanceCount();
				S.bInstancesMissingMesh = S.NumInstances > 0 && Road->StructureMarkers->GetStaticMesh() == nullptr;
			}
		}
		else if (const AVoidRoadJunctionActor* Junction = Cast<AVoidRoadJunctionActor>(Actor))
		{
			if (S.GeneratorId.IsNone()) { S.GeneratorId = RoadGenerator; }
			S.Role = RoleJunction;
			S.NumMeshSections = Junction->JunctionMesh ? Junction->JunctionMesh->GetNumSections() : 0;
			for (const FVoidElementId& Id : Junction->ConnectedRoadIds) { S.ReferencedIds.Add(Id.Value.ToString()); }
		}

		// Only keep actors that are ours; the level may hold thousands of unrelated ones.
		if (S.bTagged || !S.GeneratorId.IsNone() || S.bVoidClass)
		{
			Snapshot.Actors.Add(MoveTemp(S));
		}
	}
	return Snapshot;
}
