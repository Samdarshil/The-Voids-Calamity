// Copyright VOID Engineering Studio. Internal tool - not for shipping content.
// Added by Agent 2 (Phase 6 Metro Generator).

#include "Metro/VoidMetroGenerator.h"
#include "Metro/VoidMetroGenerationSettings.h"
#include "Metro/VoidMetroLayoutResolver.h"
#include "Metro/VoidMetroValidator.h"
#include "Metro/VoidMetroMeshBuilder.h"
#include "Metro/VoidMetroActors.h"
#include "Metro/VoidMetroExports.h"
#include "WorldPartition/VoidWorldPartitionSettings.h"
#include "WorldPartition/VoidWorldPartitionHelper.h"
#include "VoidWorldBuilderGeneratorsLog.h"
#include "ProceduralMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "ScopedTransaction.h"
#include "DrawDebugHelpers.h"
#include "HAL/PlatformTime.h"

namespace VoidMetroGeneratorPrivate
{
	// ---- Palette: colour is the only visual differentiation available without authored materials. ----
	struct FPalette
	{
		FLinearColor Bed, Rail, Structure, Canopy, Tunnel, Portal, Accent, Service, Trace;
	};

	static FPalette MakePalette(EVoidMetroNetworkKind Network)
	{
		if (Network == EVoidMetroNetworkKind::Live)
		{
			// clean_maintained_platforms
			return { FLinearColor(0.72f, 0.76f, 0.82f), FLinearColor(0.15f, 0.75f, 0.95f), FLinearColor(0.82f, 0.84f, 0.86f), FLinearColor(0.90f, 0.92f, 0.95f),
				FLinearColor(0.60f, 0.63f, 0.68f), FLinearColor(0.70f, 0.70f, 0.72f), FLinearColor(0.20f, 0.65f, 0.90f), FLinearColor(0.35f, 0.42f, 0.40f), FLinearColor(0.15f, 0.60f, 0.90f) };
		}
		// abandoned_deteriorated
		return { FLinearColor(0.12f, 0.11f, 0.10f), FLinearColor(0.36f, 0.21f, 0.13f), FLinearColor(0.28f, 0.26f, 0.23f), FLinearColor(0.20f, 0.19f, 0.17f),
			FLinearColor(0.22f, 0.20f, 0.18f), FLinearColor(0.25f, 0.23f, 0.20f), FLinearColor(0.45f, 0.30f, 0.15f), FLinearColor(0.2f, 0.2f, 0.2f), FLinearColor(0.95f, 0.45f, 0.10f) };
	}

	static const TCHAR* NetworkName(EVoidMetroNetworkKind N) { return N == EVoidMetroNetworkKind::Live ? TEXT("Live") : TEXT("Dead"); }

	static void MergeSection(FVoidRoadMeshSection& Dest, const FVoidRoadMeshSection& Src)
	{
		const int32 Offset = Dest.Vertices.Num();
		Dest.Vertices.Append(Src.Vertices);
		Dest.Normals.Append(Src.Normals);
		Dest.UVs.Append(Src.UVs);
		Dest.VertexColors.Append(Src.VertexColors);
		Dest.Tangents.Append(Src.Tangents);
		for (int32 Tri : Src.Triangles) { Dest.Triangles.Add(Tri + Offset); }
	}

	static void UploadSection(UProceduralMeshComponent* Mesh, int32 Index, const FVoidRoadMeshSection& Section, bool bCollision, UMaterialInterface* Material)
	{
		if (Section.IsEmpty()) { return; }
		FVoidRoadMeshBuilder::CreateSection(Mesh, Index, Section, bCollision);
		if (Material) { Mesh->SetMaterial(Index, Material); }
	}

	/** Calls Fn(Location, ForwardDir) at every Spacing along the path (starting Spacing/2 in). */
	template <typename Fn>
	static void WalkPath(const TArray<FVector>& P, float Spacing, Fn&& Callback)
	{
		float Carry = Spacing * 0.5f;
		for (int32 i = 0; i + 1 < P.Num(); ++i)
		{
			const FVector D = P[i + 1] - P[i];
			const float Len = D.Size();
			if (Len < 0.01f) { continue; }
			const FVector Dir = D / Len;
			float At = Carry;
			while (At <= Len)
			{
				Callback(P[i] + Dir * At, Dir);
				At += Spacing;
			}
			Carry = At - Len;
		}
	}

	static void ApplyPlacement(AVoidMetroActorBase* Actor, const UVoidWorldPartitionSettings* WP, EVoidMetroNetworkKind Network, const TCHAR* Category, bool bWorldPartitioned)
	{
		FVoidWorldPartitionPlacement Placement;
		Placement.FolderPath = FVoidWorldPartitionHelper::MakeFolderPath(WP->OutlinerFolderRoot, NetworkName(Network), Category);
		if (WP->bApplyWorldPartitionPlacement)
		{
			Placement.RuntimeGrid = (Network == EVoidMetroNetworkKind::Live) ? WP->LiveNetworkRuntimeGrid : WP->DeadNetworkRuntimeGrid;
			Placement.bSpatiallyLoaded = WP->bSpatiallyLoaded;
			Placement.bIncludeInHLOD = WP->bIncludeInHLOD;
			Placement.HLODLayer = WP->HLODLayer;
		}
		FVoidWorldPartitionHelper::ApplyPlacement(Actor, Placement, bWorldPartitioned && WP->bApplyWorldPartitionPlacement);
	}

	template <typename T>
	static T* SpawnMetroActor(UWorld* World, const FVector& Origin, const FString& Label, FName Id, EVoidMetroNetworkKind Network, FName OwnerKey, FName District, bool bPlaceholder, FIntPoint Cell)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		T* Actor = World->SpawnActor<T>(T::StaticClass(), FTransform(Origin), Params);
		if (!Actor) { return nullptr; }
		Actor->GeneratedId = Id;
		Actor->OwnerKey = OwnerKey;
		Actor->DistrictId = District;
		Actor->NetworkKind = Network;
		Actor->bIsPlaceholderLayout = bPlaceholder;
		Actor->ChunkCell = Cell;
		Actor->Tags.AddUnique(FName(TEXT("VOIDMetro")));
		Actor->Tags.AddUnique(FName(*FString::Printf(TEXT("VOIDMetro.%s"), NetworkName(Network))));
		Actor->Tags.AddUnique(FName(*FString::Printf(TEXT("VOIDMetroOwner.%s"), *OwnerKey.ToString())));
		Actor->SetActorLabel(Label);
		return Actor;
	}

	static int32 DestroyOwnedActors(UWorld* World, FName OwnerKey)
	{
		TArray<AVoidMetroActorBase*> Owned;
		for (TActorIterator<AVoidMetroActorBase> It(World); It; ++It)
		{
			if (It->OwnerKey == OwnerKey) { Owned.Add(*It); }
		}
		for (AVoidMetroActorBase* A : Owned)
		{
			A->Modify();
			World->DestroyActor(A);
		}
		return Owned.Num();
	}

	// ---- Track chunk ----------------------------------------------------------------------
	static void BuildTrackChunk(AVoidMetroTrackActor* Actor, const FVoidMetroResolvedSegment& Seg, const TArray<FVector>& WorldPoints, const TArray<const FVoidMetroPortal*>& Portals,
		const FVector& Origin, const UVoidMetroGenerationSettings& S, UMaterialInterface* Material)
	{
		const FPalette C = MakePalette(Seg.Network);
		const bool bLive = Seg.Network == EVoidMetroNetworkKind::Live;

		TArray<FVector> P;
		P.Reserve(WorldPoints.Num());
		float MinZ = TNumericLimits<float>::Max(), MaxZ = TNumericLimits<float>::Lowest();
		for (const FVector& W : WorldPoints)
		{
			P.Add(W - Origin);
			MinZ = FMath::Min(MinZ, W.Z);
			MaxZ = FMath::Max(MaxZ, W.Z);
		}
		const float ElevatedThreshold = 200.0f;
		const float UndergroundThreshold = -50.0f;

		FVoidRoadMeshSection Body, Rails, Shell, Trace;

		if (bLive)
		{
			// Grav-rail guideway: a levitation deck with two edge strips (no wheels/sleepers).
			FVoidMetroMeshBuilder::AppendPathBoxes(Body, P, false, S.LiveGuidewayWidthUnits, 120.0f, 0.0f, -60.0f, C.Bed);
			const float Edge = S.LiveGuidewayWidthUnits * 0.5f - 30.0f;
			FVoidMetroMeshBuilder::AppendPathBoxes(Rails, P, false, 20.0f, 20.0f, Edge, 10.0f, C.Rail);
			FVoidMetroMeshBuilder::AppendPathBoxes(Rails, P, false, 20.0f, 20.0f, -Edge, 10.0f, C.Rail);
		}
		else
		{
			// Pre-Council subway: ballast bed, two steel rails, instanced sleepers.
			FVoidMetroMeshBuilder::AppendPathBoxes(Body, P, false, 420.0f, 40.0f, 0.0f, -20.0f, C.Bed);
			FVoidMetroMeshBuilder::AppendPathBoxes(Rails, P, false, 15.0f, 20.0f, S.DeadRailGaugeUnits * 0.5f, 10.0f, C.Rail);
			FVoidMetroMeshBuilder::AppendPathBoxes(Rails, P, false, 15.0f, 20.0f, -S.DeadRailGaugeUnits * 0.5f, 10.0f, C.Rail);
		}

		// Elevated: girder under the deck, piers via instancing.
		if (MaxZ > ElevatedThreshold)
		{
			for (int32 i = 0; i + 1 < P.Num(); ++i)
			{
				if (WorldPoints[i].Z > ElevatedThreshold * 0.5f && WorldPoints[i + 1].Z > ElevatedThreshold * 0.5f)
				{
					FVoidMetroMeshBuilder::AppendBoxAlong(Shell, P[i], P[i + 1], 350.0f, 250.0f, 0.0f, -245.0f, C.Structure);
				}
			}
		}

		// Underground: tunnel bore per contiguous underground run + surface trace so it is visible from above.
		if (MinZ < UndergroundThreshold)
		{
			int32 RunStart = INDEX_NONE;
			for (int32 i = 0; i <= WorldPoints.Num(); ++i)
			{
				const bool bUnder = (i < WorldPoints.Num()) && WorldPoints[i].Z < UndergroundThreshold;
				if (bUnder && RunStart == INDEX_NONE) { RunStart = i; }
				if (!bUnder && RunStart != INDEX_NONE)
				{
					TArray<FVector> Run, Projected;
					for (int32 k = RunStart; k < i; ++k)
					{
						Run.Add(P[k]);
						Projected.Add(FVector(WorldPoints[k].X, WorldPoints[k].Y, 6.0f) - Origin);
					}
					if (Run.Num() >= 2)
					{
						FVoidMetroMeshBuilder::AppendTunnelTube(Shell, Run, false, S.TunnelRadiusUnits, S.TunnelSides, S.TunnelRadiusUnits * 0.45f, C.Tunnel);
						if (S.bDrawSurfaceTraceForUnderground)
						{
							MergeSection(Trace, FVoidRoadMeshBuilder::BuildRibbon(Projected, -30.0f, 30.0f, 0.0f, C.Trace, false, 500.0f));
						}
					}
					RunStart = INDEX_NONE;
				}
			}
		}

		for (const FVoidMetroPortal* Portal : Portals)
		{
			FVoidMetroMeshBuilder::AppendPortalFrame(Shell, Portal->Location - Origin, Portal->YawDegrees, S.TunnelRadiusUnits * 1.6f, S.TunnelRadiusUnits * 1.1f, 300.0f, C.Portal);
		}

		UploadSection(Actor->MetroMesh, 0, Body, true, Material);
		UploadSection(Actor->MetroMesh, 1, Rails, false, Material);
		UploadSection(Actor->MetroMesh, 2, Shell, true, Material);
		UploadSection(Actor->MetroMesh, 3, Trace, false, Material);

		// Instanced sleepers (dead) and piers (elevated) -- one batched add each, no per-instance calls.
		if (!bLive && Actor->SleeperInstances)
		{
			TArray<FTransform> Xf;
			WalkPath(P, S.SleeperSpacingUnits, [&Xf](const FVector& Loc, const FVector& Dir)
			{
				const FQuat Rot = FQuat(FVector::UpVector, FMath::Atan2(Dir.Y, Dir.X));
				Xf.Add(FTransform(Rot, Loc + FVector(0, 0, -2.0f), FVector(0.25f, 2.6f, 0.15f)));
			});
			if (Xf.Num() > 0) { Actor->SleeperInstances->AddInstances(Xf, false); }
		}
		if (MaxZ > ElevatedThreshold && Actor->PierInstances)
		{
			TArray<FTransform> Xf;
			const FVector Org = Origin;
			WalkPath(WorldPoints, S.ViaductPierSpacingUnits, [&Xf, &Org](const FVector& Loc, const FVector&)
			{
				const float Height = Loc.Z - 370.0f; // deck 120 + girder 250 above the ground
				if (Height >= 150.0f && Loc.Z > 200.0f)
				{
					Xf.Add(FTransform(FQuat::Identity, FVector(Loc.X - Org.X, Loc.Y - Org.Y, Height * 0.5f - Org.Z), FVector(2.0f, 2.0f, Height / 100.0f)));
				}
			});
			if (Xf.Num() > 0) { Actor->PierInstances->AddInstances(Xf, false); }
		}

		// Spline for camera rigs / other systems, exactly matching the mesh polyline (linear points, world space).
		if (Actor->TrackSpline)
		{
			Actor->TrackSpline->ClearSplinePoints(false);
			for (int32 i = 0; i < WorldPoints.Num(); ++i)
			{
				Actor->TrackSpline->AddSplinePoint(WorldPoints[i], ESplineCoordinateSpace::World, false);
				Actor->TrackSpline->SetSplinePointType(i, ESplinePointType::Linear, false);
			}
			Actor->TrackSpline->UpdateSpline();
		}
	}

	// ---- Station ----------------------------------------------------------------------------
	static void BuildStation(AVoidMetroStationActor* Actor, const FVoidMetroResolvedStation& St, const FVector& Origin, const UVoidMetroGenerationSettings& S, UMaterialInterface* Material)
	{
		const FPalette C = MakePalette(St.Network);
		const bool bLive = St.Network == EVoidMetroNetworkKind::Live;
		const bool bUnder = St.Grade == EVoidMetroGrade::Underground;
		const FQuat Rot = FVoidMetroMeshBuilder::YawQuat(St.YawDegrees);
		const FVector StationLocal = St.Location - Origin; // (0,0,railZ)

		auto W = [&](const FVector& L) { return StationLocal + Rot.RotateVector(L); };

		const float TrackHalfClear = bLive ? S.LiveGuidewayWidthUnits * 0.5f : 200.0f;
		float PlatW = S.PlatformWidthUnits;
		if (bUnder)
		{
			// Underground stations live inside the bore: platforms must fit within the tunnel radius.
			PlatW = FMath::Clamp(S.TunnelRadiusUnits * 0.9f - TrackHalfClear, 100.0f, PlatW);
		}
		const float PlatLen = S.PlatformLengthUnits;
		const float PlatH = S.PlatformHeightUnits;
		const float Thick = 100.0f;

		FVoidRoadMeshSection Structure, Circulation, Service;

		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			const float SideF = static_cast<float>(Side);
			const FVector LocalC(0.0f, SideF * (TrackHalfClear + PlatW * 0.5f), (PlatH - Thick) * 0.5f);
			FVoidMetroMeshBuilder::AppendOrientedBox(Structure, W(LocalC), Rot, FVector(PlatLen * 0.5f, PlatW * 0.5f, (PlatH + Thick) * 0.5f), C.Structure);

			// Elevated station: podium from the ground up to under the platform.
			if (St.Location.Z > Thick + 50.0f)
			{
				const float Bottom = -St.Location.Z;
				const float Top = -Thick;
				FVoidMetroMeshBuilder::AppendOrientedBox(Structure, W(FVector(0.0f, SideF * (TrackHalfClear + PlatW * 0.5f), (Bottom + Top) * 0.5f)), Rot,
					FVector(PlatLen * 0.5f - 200.0f, PlatW * 0.5f - 60.0f, (Top - Bottom) * 0.5f), C.Tunnel);
			}

			if (!bUnder)
			{
				// Canopy columns.
				for (float X = -PlatLen * 0.5f + 300.0f; X <= PlatLen * 0.5f - 300.0f + 1.0f; X += 1200.0f)
				{
					FVoidMetroMeshBuilder::AppendOrientedBox(Structure, W(FVector(X, SideF * (TrackHalfClear + PlatW - 40.0f), S.CanopyHeightUnits * 0.5f)), Rot,
						FVector(20.0f, 20.0f, S.CanopyHeightUnits * 0.5f), C.Structure);
				}
			}
		}
		if (!bUnder)
		{
			FVoidMetroMeshBuilder::AppendOrientedBox(Structure, W(FVector(0.0f, 0.0f, S.CanopyHeightUnits + 15.0f)), Rot,
				FVector(PlatLen * 0.5f, TrackHalfClear + PlatW, 15.0f), C.Canopy);
		}
		if (St.bIsInterchange)
		{
			// Interchange plaza marker at ground level.
			FVoidMetroMeshBuilder::AppendOrientedBox(Structure, FVector(0.0f, 0.0f, 20.0f), Rot, FVector(700.0f, 700.0f, 20.0f), C.Accent);
		}

		// Entrances, escalator/elevator placeholders.
		for (const FVoidMetroResolvedEntrance& E : St.Entrances)
		{
			const FVector EL = E.Location - Origin; // ground level
			const FQuat ER = FVoidMetroMeshBuilder::YawQuat(E.YawDegrees);
			FVoidMetroMeshBuilder::AppendOrientedBox(Structure, EL + FVector(0, 0, 175.0f), ER, FVector(300.0f, 250.0f, 175.0f), bLive ? C.Structure : C.Tunnel);
			FVoidMetroMeshBuilder::AppendOrientedBox(Structure, EL + FVector(0, 0, 360.0f), ER, FVector(330.0f, 280.0f, 12.0f), C.Accent);
			Actor->EntranceLocations.Add(E.Location);

			if (St.Grade != EVoidMetroGrade::AtGrade)
			{
				const FVector Local = Rot.UnrotateVector(EL - StationLocal);
				const float SideF = Local.Y >= 0.0f ? 1.0f : -1.0f;
				const FVector Target = W(FVector(0.0f, SideF * (TrackHalfClear + PlatW * 0.5f), PlatH));
				FVoidMetroMeshBuilder::AppendBoxAlong(Circulation, EL + FVector(0, 0, 60.0f), Target, bLive ? 180.0f : 120.0f, 40.0f, 0.0f, 0.0f, bLive ? C.Accent : C.Tunnel);

				if (bLive) // elevator only where the network is maintained (dead network: abandoned)
				{
					const float Bottom = FMath::Min(0.0f, Target.Z);
					const float Top = FMath::Max(0.0f, Target.Z) + 250.0f;
					FVoidMetroMeshBuilder::AppendOrientedBox(Circulation, EL + ER.GetAxisY() * 450.0f + FVector(0, 0, (Bottom + Top) * 0.5f - EL.Z), ER,
						FVector(110.0f, 110.0f, (Top - Bottom) * 0.5f), C.Canopy);
				}
			}
		}

		// Service / maintenance placeholder: only for networks with a maintenance model (Live).
		if (St.bHasServiceFacility)
		{
			const FVector At = W(FVector(PlatLen * 0.5f + 600.0f, -(TrackHalfClear + PlatW + 500.0f), 0.0f));
			FVoidMetroMeshBuilder::AppendOrientedBox(Service, FVector(At.X, At.Y, 200.0f - Origin.Z), Rot, FVector(350.0f, 300.0f, 200.0f), C.Service);
		}

		UploadSection(Actor->MetroMesh, 0, Structure, true, Material);
		UploadSection(Actor->MetroMesh, 1, Circulation, true, Material);
		UploadSection(Actor->MetroMesh, 2, Service, true, Material);
	}
}

FName FVoidMetroGenerator::GetGeneratorId() const
{
	return TEXT("Metro");
}

bool FVoidMetroGenerator::Generate(const FVoidDesignPackage& Package, FVoidGenerationContext& Context)
{
	using namespace VoidMetroGeneratorPrivate;

	const double StartSeconds = FPlatformTime::Seconds();
	UWorld* World = Context.TargetWorld;
	if (!World)
	{
		UE_LOG(LogVoidGenerators, Error, TEXT("FVoidMetroGenerator::Generate called with a null TargetWorld."));
		return false;
	}

	const UVoidMetroGenerationSettings* Settings = GetDefault<UVoidMetroGenerationSettings>();
	const UVoidWorldPartitionSettings* WPSettings = GetDefault<UVoidWorldPartitionSettings>();
	FVoidMetroMeshBuilder::SetFlipWinding(Settings->bFlipTriangleWinding);

	// ---- Validate + resolve BEFORE touching the world, so a failure leaves existing actors intact. ----
	Context.GenerationValidationReport = FVoidMetroValidator::Validate(Package.Metro);
	FVoidValidationReport& Report = Context.GenerationValidationReport;

	FVoidMetroResolvedLayout Layout;
	if (Report.NumErrors() == 0 && !Report.HasFatalIssue())
	{
		const FVoidMetroLayoutParams Params = FVoidMetroLayoutParams::FromSettings(Settings);
		Layout = FVoidMetroLayoutResolver::Resolve(Package.Metro, Params, Report);
		FVoidMetroValidator::ValidateResolved(Layout, Report);
	}

	UE_LOG(LogVoidGenerators, Log, TEXT("Metro generation starting (%d stations, %d lines in data). Errors: %d, Warnings: %d."),
		Package.Metro.Stations.Num(), Package.Metro.Lines.Num(), Report.NumErrors(), Report.NumWarnings());

	if (Report.NumErrors() > 0 || Report.HasFatalIssue())
	{
		Context.Log(FString::Printf(TEXT("Metro generation aborted: %d validation error(s). No actors were changed."), Report.NumErrors()));
		UE_LOG(LogVoidGenerators, Error, TEXT("Metro generation aborted by validation. No actors were changed."));
		return false;
	}
	if (Layout.IsEmpty())
	{
		Context.Log(TEXT("Metro generation produced nothing: no station or track could be placed (see validation warnings)."));
		return false;
	}

	const FName OwnerKey = Settings->OwnerKey;
	const bool bWP = FVoidWorldPartitionHelper::IsWorldPartitioned(World);
	UMaterialInterface* Material = Settings->MetroMaterial.IsValid() ? Cast<UMaterialInterface>(Settings->MetroMaterial.TryLoad()) : nullptr;
	if (!Material && Settings->MetroMaterial.IsValid())
	{
		UE_LOG(LogVoidGenerators, Warning, TEXT("Metro material '%s' did not load; geometry will use the default material (vertex colours may not show)."), *Settings->MetroMaterial.ToString());
	}

	const FScopedTransaction Transaction(NSLOCTEXT("VoidMetroGenerator", "GenerateMetroTransaction", "Generate VOID Metro"));

	const int32 NumDestroyed = DestroyOwnedActors(World, OwnerKey);

	int32 NumTrackActors = 0;
	int32 NumStationActors = 0;
	bool bCancelled = false;

	// Live first, then Dead: the live radial spine is the reference the dead substrate sits under.
	const EVoidMetroNetworkKind Order[2] = { EVoidMetroNetworkKind::Live, EVoidMetroNetworkKind::Dead };

	for (EVoidMetroNetworkKind Kind : Order)
	{
		const float CellSize = WPSettings->ChunkCellSizeUnits * (Kind == EVoidMetroNetworkKind::Dead ? WPSettings->DeadNetworkChunkCellMultiplier : 1.0f);

		// --- Track ---
		for (const FVoidMetroResolvedSegment& Seg : Layout.Segments)
		{
			if (Seg.Network != Kind) { continue; }
			if (Context.IsCancelled()) { bCancelled = true; break; }

			const TArray<FVoidPathChunk> Chunks = FVoidWorldPartitionHelper::SplitPathByCells(Seg.Points, Seg.bClosed, CellSize);

			// Assign each portal to the chunk whose points are nearest (deterministic; each portal drawn once).
			TArray<TArray<const FVoidMetroPortal*>> PortalsPerChunk;
			PortalsPerChunk.SetNum(Chunks.Num());
			for (const FVoidMetroPortal& Portal : Layout.Portals)
			{
				if (Portal.SegmentId != Seg.Id) { continue; }
				int32 Best = INDEX_NONE;
				float BestD = TNumericLimits<float>::Max();
				for (int32 c = 0; c < Chunks.Num(); ++c)
				{
					for (const FVector& Pt : Chunks[c].Points)
					{
						const float D = FVector::DistSquared2D(Pt, Portal.Location);
						if (D < BestD) { BestD = D; Best = c; }
					}
				}
				if (Best != INDEX_NONE) { PortalsPerChunk[Best].Add(&Portal); }
			}

			for (int32 c = 0; c < Chunks.Num(); ++c)
			{
				const FVoidPathChunk& Chunk = Chunks[c];
				if (Chunk.Points.Num() < 2) { continue; }

				FBox Bounds(ForceInit);
				for (const FVector& Pt : Chunk.Points) { Bounds += Pt; }
				const FVector Origin(Bounds.GetCenter().X, Bounds.GetCenter().Y, 0.0f);

				const FString IdString = FString::Printf(TEXT("%s_c%d_%d_%d"), *Seg.Id.ToString(), Chunk.Cell.X, Chunk.Cell.Y, Chunk.IndexInCell);
				AVoidMetroTrackActor* Actor = SpawnMetroActor<AVoidMetroTrackActor>(World, Origin,
					FString::Printf(TEXT("VoidMetro_%s_Track_%s"), NetworkName(Kind), *IdString), FName(*IdString), Kind, OwnerKey, NAME_None, Seg.bPlaceholder, Chunk.Cell);
				if (!Actor)
				{
					Report.AddWarning(FString::Printf(TEXT("Failed to spawn track actor for '%s'."), *IdString), TEXT("metro"), TEXT("VOID.Metro.SpawnFailed"));
					continue;
				}
				Actor->LineId = Seg.LineId;
				Actor->SegmentId = Seg.Id;
				Actor->Grade = Seg.Grade;

				BuildTrackChunk(Actor, Seg, Chunk.Points, PortalsPerChunk[c], Origin, *Settings, Material);
				ApplyPlacement(Actor, WPSettings, Kind, TEXT("Track"), bWP);
				++NumTrackActors;
			}
		}
		if (bCancelled) { break; }

		// --- Stations ---
		auto BuildOneStation = [&](const FVoidMetroResolvedStation& St)
		{
			const FVector Origin(St.Location.X, St.Location.Y, 0.0f);
			const FIntPoint Cell = FVoidWorldPartitionHelper::GetCellForLocation(St.Location, CellSize);
			const FString IdString = St.Id.ToString();
			AVoidMetroStationActor* Actor = SpawnMetroActor<AVoidMetroStationActor>(World, Origin,
				FString::Printf(TEXT("VoidMetro_%s_Station_%s"), NetworkName(Kind), *IdString), St.Id, Kind, OwnerKey, St.DistrictId, St.bPlaceholderPosition, Cell);
			if (!Actor)
			{
				Report.AddWarning(FString::Printf(TEXT("Failed to spawn station actor for '%s'."), *IdString), TEXT("metro"), TEXT("VOID.Metro.SpawnFailed"));
				return;
			}
			Actor->StationId = St.Id;
			Actor->Grade = St.Grade;
			Actor->bIsInterchange = St.bIsInterchange;
			BuildStation(Actor, St, Origin, *Settings, Material);
			ApplyPlacement(Actor, WPSettings, Kind, TEXT("Station"), bWP);
			++NumStationActors;
		};

		for (const FVoidMetroResolvedStation& St : Layout.Stations)
		{
			if (St.Network != Kind) { continue; }
			if (Context.IsCancelled()) { bCancelled = true; break; }
			BuildOneStation(St);
		}
		if (bCancelled) { break; }

		if (Kind == EVoidMetroNetworkKind::Live)
		{
			const FVoidMetroLayoutParams Params = FVoidMetroLayoutParams::FromSettings(Settings);
			for (const FVoidMetroInterchangeHub& Hub : Layout.InterchangeHubs)
			{
				FVoidMetroResolvedStation Synthetic;
				Synthetic.Id = Hub.Id;
				Synthetic.Network = EVoidMetroNetworkKind::Live;
				Synthetic.Grade = Params.LiveDefaultGrade;
				Synthetic.Location = Hub.Location;
				Synthetic.YawDegrees = Hub.YawDegrees;
				Synthetic.bHasServiceFacility = true;
				Synthetic.bIsInterchange = true;
				Synthetic.bPlaceholderPosition = Layout.bUsedPlaceholder;
				BuildOneStation(Synthetic);
			}
		}
	}

	if (Settings->bDrawDebugVisualization)
	{
		FlushPersistentDebugLines(World);
		for (const FVoidMetroResolvedSegment& Seg : Layout.Segments)
		{
			const FColor Col = Seg.Network == EVoidMetroNetworkKind::Live ? FColor::Cyan : FColor::Orange;
			const int32 Spans = Seg.bClosed ? Seg.Points.Num() : Seg.Points.Num() - 1;
			for (int32 i = 0; i < Spans; ++i)
			{
				DrawDebugLine(World, Seg.Points[i], Seg.Points[(i + 1) % Seg.Points.Num()], Col, true, -1.0f, 0, 30.0f);
			}
		}
		for (const FVoidMetroResolvedStation& St : Layout.Stations) { DrawDebugSphere(World, St.Location, 300.0f, 12, FColor::Green, true); }
		for (const FVoidMetroPortal& P : Layout.Portals) { DrawDebugSphere(World, P.Location, 250.0f, 12, FColor::Red, true); }
	}

	if (bCancelled)
	{
		Context.Log(FString::Printf(TEXT("Metro generation cancelled: %d track and %d station actors were built before cancel."), NumTrackActors, NumStationActors));
		UE_LOG(LogVoidGenerators, Warning, TEXT("Metro generation cancelled."));
		return false;
	}

	FVoidMetroExportRegistry::Get().Publish(Layout, OwnerKey);

	const double ElapsedMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
	Context.Log(FString::Printf(TEXT("Metro: %d stations, %d track segments (%d track actors), %d station actors, %d portals, %d elevated spans, %d interchange hubs; %d previous actors replaced; World Partition world: %s; %.1f ms."),
		Layout.Stations.Num(), Layout.Segments.Num(), NumTrackActors, NumStationActors, Layout.Portals.Num(), Layout.ElevatedSpans.Num(), Layout.InterchangeHubs.Num(), NumDestroyed,
		bWP ? TEXT("yes") : TEXT("no"), ElapsedMs));
	if (Layout.bUsedPlaceholder)
	{
		Context.Log(TEXT("Metro: layout includes generator-derived PLACEHOLDER positions (not canon). See VOID.Metro.PlaceholderLayout."));
	}
	UE_LOG(LogVoidGenerators, Log, TEXT("Metro generation finished: %d track actors, %d station actors, %.1f ms."), NumTrackActors, NumStationActors, ElapsedMs);
	return NumTrackActors + NumStationActors > 0;
}
