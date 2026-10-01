// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Road/VoidRoadGenerator.h"
#include "Road/VoidRoadTypeProfile.h"
#include "Road/VoidRoadGenerationSettings.h"
#include "Road/VoidRoadSplineBuilder.h"
#include "Road/VoidRoadBridgeTunnelBuilder.h"
#include "Road/VoidRoadMeshBuilder.h"
#include "Road/VoidRoadIntersectionBuilder.h"
#include "Road/VoidRoadValidator.h"
#include "Road/VoidRoadActor.h"
#include "VoidWorldBuilderGeneratorsLog.h"
#include "Engine/World.h"
#include "ScopedTransaction.h"
#include "DrawDebugHelpers.h"
#include "HAL/PlatformTime.h"

namespace VoidRoadGeneratorPrivate
{
	/** Vertex colors are the only visual differentiation available at greybox scope -- no material assets are referenced. */
	static const FLinearColor RoadSurfaceColor(0.30f, 0.30f, 0.30f);
	static const FLinearColor RoundaboutSurfaceColor(0.35f, 0.35f, 0.42f);
	static const FLinearColor MedianColor(0.85f, 0.75f, 0.15f);
	static const FLinearColor CurbColor(0.55f, 0.55f, 0.55f);
	static const FLinearColor SidewalkColor(0.65f, 0.63f, 0.60f);
	static const FLinearColor JunctionPadColor(0.28f, 0.28f, 0.28f);
	static const FLinearColor CrosswalkColorA(0.9f, 0.9f, 0.9f);
	static const FLinearColor CrosswalkColorB(0.3f, 0.3f, 0.3f);

	static void BuildSidewalkCurbSections(AVoidRoadActor* RoadActor, const TArray<FVector>& Points, float HalfWidth, const FVoidRoadTypeProfile& Profile, bool bClosedLoop)
	{
		int32 SectionIndex = 2; // 0 = surface, 1 = median (added by caller if present)

		float SidewalkInner = HalfWidth;

		if (Profile.CurbWidthUnits > 0.0f)
		{
			const float CurbOuter = HalfWidth + Profile.CurbWidthUnits;

			FVoidRoadMeshSection LeftCurb = FVoidRoadMeshBuilder::BuildRibbon(Points, -CurbOuter, -HalfWidth, Profile.CurbHeightUnits, CurbColor, bClosedLoop, Profile.CurbWidthUnits * 4.0f);
			FVoidRoadMeshBuilder::CreateSection(RoadActor->RoadMesh, SectionIndex++, LeftCurb, false);

			FVoidRoadMeshSection RightCurb = FVoidRoadMeshBuilder::BuildRibbon(Points, HalfWidth, CurbOuter, Profile.CurbHeightUnits, CurbColor, bClosedLoop, Profile.CurbWidthUnits * 4.0f);
			FVoidRoadMeshBuilder::CreateSection(RoadActor->RoadMesh, SectionIndex++, RightCurb, false);

			SidewalkInner = CurbOuter;
		}

		const float SidewalkOuter = SidewalkInner + Profile.SidewalkWidthUnits;

		FVoidRoadMeshSection LeftSidewalk = FVoidRoadMeshBuilder::BuildRibbon(Points, -SidewalkOuter, -SidewalkInner, Profile.CurbHeightUnits, SidewalkColor, bClosedLoop, Profile.SidewalkWidthUnits * 2.0f);
		FVoidRoadMeshBuilder::CreateSection(RoadActor->RoadMesh, SectionIndex++, LeftSidewalk, true);

		FVoidRoadMeshSection RightSidewalk = FVoidRoadMeshBuilder::BuildRibbon(Points, SidewalkInner, SidewalkOuter, Profile.CurbHeightUnits, SidewalkColor, bClosedLoop, Profile.SidewalkWidthUnits * 2.0f);
		FVoidRoadMeshBuilder::CreateSection(RoadActor->RoadMesh, SectionIndex++, RightSidewalk, true);
	}

	static void DrawJunctionDebug(UWorld* World, const FVoidRoadJunction& Junction, float Duration)
	{
		const bool bPersistent = Duration <= 0.0f;
		DrawDebugSphere(World, Junction.Location, FMath::Max(Junction.PadRadius, 25.0f), 12, FColor::Yellow, bPersistent, Duration);
	}

	static void DrawSplineDebug(UWorld* World, const TArray<FVector>& Points, float Duration)
	{
		const bool bPersistent = Duration <= 0.0f;
		for (int32 Index = 0; Index + 1 < Points.Num(); ++Index)
		{
			DrawDebugLine(World, Points[Index], Points[Index + 1], FColor::Green, bPersistent, Duration, 0, 3.0f);
		}
	}
}

FName FVoidRoadGenerator::GetGeneratorId() const
{
	return FName(TEXT("Road"));
}

bool FVoidRoadGenerator::Generate(const FVoidDesignPackage& Package, FVoidGenerationContext& Context)
{
	using namespace VoidRoadGeneratorPrivate;

	const double StartSeconds = FPlatformTime::Seconds();
	const FVoidDistrictData& District = Package.District;

	Context.GenerationValidationReport = FVoidRoadValidator::Validate(District);

	UE_LOG(LogVoidGenerators, Log, TEXT("Road generation starting for district '%s' (%d roads). Errors: %d, Warnings: %d."),
		*District.DistrictId.Value.ToString(), District.Roads.Num(),
		Context.GenerationValidationReport.NumErrors(), Context.GenerationValidationReport.NumWarnings());
	Context.Log(FString::Printf(TEXT("Starting road generation for district '%s' (%d roads)."), *District.DistrictId.Value.ToString(), District.Roads.Num()));

	if (Context.GenerationValidationReport.HasFatalIssue())
	{
		Context.Log(TEXT("Aborting: generation-time validation reported a Fatal issue."));
		return false;
	}

	if (!Context.TargetWorld)
	{
		Context.Log(TEXT("Aborting: no target world provided."));
		UE_LOG(LogVoidGenerators, Error, TEXT("FVoidRoadGenerator::Generate called with a null TargetWorld."));
		return false;
	}

	const UVoidRoadGenerationSettings* Settings = GetDefault<UVoidRoadGenerationSettings>();
	const float JunctionTolerance = Settings ? Settings->JunctionToleranceUnits : 50.0f;
	const float RampLength = Settings ? Settings->RampLengthUnits : 500.0f;
	const float BridgeHeight = Settings ? Settings->DefaultBridgeHeightUnits : 300.0f;
	const float TunnelDepth = Settings ? Settings->DefaultTunnelDepthUnits : 300.0f;
	const float PierSpacing = Settings ? Settings->BridgePierSpacingUnits : 800.0f;
	const bool bDrawDebug = Settings && Settings->bDrawDebugVisualization;
	const float DebugDuration = Settings ? Settings->DebugVisualizationDurationSeconds : 15.0f;
	const UDataTable* ProfileTable = (Settings && !Settings->RoadTypeProfileTable.IsNull()) ? Settings->RoadTypeProfileTable.LoadSynchronous() : nullptr;

	const FScopedTransaction Transaction(NSLOCTEXT("VoidRoadGenerator", "GenerateRoadsTransaction", "Generate VOID Roads"));

	TArray<FVoidBuiltRoad> BuiltRoads;
	BuiltRoads.Reserve(District.Roads.Num());
	int32 NumRoadsBuilt = 0;

	for (const FVoidRoadSpec& RoadSpec : District.Roads)
	{
		if (Context.IsCancelled())
		{
			Context.Log(TEXT("Generation cancelled by user."));
			UE_LOG(LogVoidGenerators, Warning, TEXT("Road generation cancelled after %d of %d roads."), NumRoadsBuilt, District.Roads.Num());
			break;
		}

		const bool bIsRoundabout = (RoadSpec.RoadType == EVoidRoadType::Roundabout);
		const FVoidRoadTypeProfile Profile = FVoidRoadTypeProfileLibrary::ResolveProfile(RoadSpec.RoadType, ProfileTable);

		TArray<FVector> Points = bIsRoundabout
			? FVoidRoadSplineBuilder::BuildRoundaboutLoopPoints(RoadSpec)
			: FVoidRoadSplineBuilder::BuildCenterlinePoints(RoadSpec);

		if (Points.Num() < 2)
		{
			Context.Log(FString::Printf(TEXT("Skipped road '%s': fewer than 2 usable points."), *RoadSpec.Id.Value.ToString()));
			continue;
		}

		if (!bIsRoundabout)
		{
			FVoidRoadBridgeTunnelBuilder::ApplyElevationRamp(RoadSpec, Points, RampLength, BridgeHeight, TunnelDepth);
		}

		FVoidBuiltRoad BuiltRoad;
		BuiltRoad.Spec = RoadSpec;
		BuiltRoad.Points = Points;
		BuiltRoads.Add(BuiltRoad);

		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = MakeUniqueObjectName(Context.TargetWorld, AVoidRoadActor::StaticClass(), FName(*FString::Printf(TEXT("VoidRoad_%s"), *RoadSpec.Id.Value.ToString())));
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AVoidRoadActor* RoadActor = Context.TargetWorld->SpawnActor<AVoidRoadActor>(AVoidRoadActor::StaticClass(), FTransform::Identity, SpawnParams);
		if (!RoadActor)
		{
			Context.Log(FString::Printf(TEXT("Failed to spawn actor for road '%s'."), *RoadSpec.Id.Value.ToString()));
			continue;
		}

#if WITH_EDITOR
		RoadActor->SetActorLabel(FString::Printf(TEXT("VoidRoad_%s"), *RoadSpec.Id.Value.ToString()));
#endif

		RoadActor->RoadId = RoadSpec.Id;
		RoadActor->RoadType = RoadSpec.RoadType;

		FVoidRoadSplineBuilder::ApplyPointsToSpline(RoadActor->RoadSpline, Points, bIsRoundabout);

		const float HalfWidth = (RoadSpec.WidthUnits > 0.0f ? RoadSpec.WidthUnits : Profile.DefaultWidthUnits) * 0.5f;
		const FLinearColor& SurfaceColor = bIsRoundabout ? RoundaboutSurfaceColor : RoadSurfaceColor;

		FVoidRoadMeshSection Surface = FVoidRoadMeshBuilder::BuildRibbon(Points, -HalfWidth, HalfWidth, 0.0f, SurfaceColor, bIsRoundabout, FMath::Max(HalfWidth * 2.0f, 100.0f));
		FVoidRoadMeshBuilder::CreateSection(RoadActor->RoadMesh, 0, Surface, true);

		if (RoadSpec.bHasMedian && !bIsRoundabout)
		{
			const float MedianHalfWidth = Profile.MedianWidthUnits * 0.5f;
			FVoidRoadMeshSection Median = FVoidRoadMeshBuilder::BuildRibbon(Points, -MedianHalfWidth, MedianHalfWidth, 2.0f, MedianColor, false, Profile.MedianWidthUnits * 2.0f);
			FVoidRoadMeshBuilder::CreateSection(RoadActor->RoadMesh, 1, Median, false);
		}

		if (RoadSpec.bHasSidewalk)
		{
			BuildSidewalkCurbSections(RoadActor, Points, HalfWidth, Profile, bIsRoundabout);
		}

		if (RoadSpec.bIsBridge)
		{
			const TArray<FVector> PierPositions = FVoidRoadBridgeTunnelBuilder::ComputeBridgePierPositions(RoadSpec, Points, RampLength, PierSpacing);
			for (const FVector& PierPosition : PierPositions)
			{
				const float PierHeight = FMath::Max(PierPosition.Z, 50.0f);
				const FVector PierLocation = PierPosition - FVector(0.0f, 0.0f, PierHeight * 0.5f);
				const FTransform PierTransform(FRotator::ZeroRotator, PierLocation, FVector(HalfWidth * 0.002f, HalfWidth * 0.002f, PierHeight * 0.01f));
				RoadActor->StructureMarkers->AddInstance(PierTransform);
			}
		}
		else if (RoadSpec.bIsTunnel)
		{
			const TArray<FVector> PortalPositions = FVoidRoadBridgeTunnelBuilder::ComputeTunnelPortalPositions(RoadSpec, Points);
			for (const FVector& PortalPosition : PortalPositions)
			{
				const FTransform PortalTransform(FRotator::ZeroRotator, PortalPosition, FVector(HalfWidth * 0.002f, HalfWidth * 0.002f, 0.3f));
				RoadActor->StructureMarkers->AddInstance(PortalTransform);
			}
		}

		if (bDrawDebug)
		{
			DrawSplineDebug(Context.TargetWorld, Points, DebugDuration);
		}

		++NumRoadsBuilt;
	}

	Context.Log(FString::Printf(TEXT("Built %d of %d roads."), NumRoadsBuilt, District.Roads.Num()));

	// --- Junctions ---------------------------------------------------
	const TArray<FVoidRoadJunction> Junctions = FVoidRoadIntersectionBuilder::BuildJunctionGraph(BuiltRoads, JunctionTolerance);

	TMap<FName, const FVoidBuiltRoad*> RoadLookup;
	for (const FVoidBuiltRoad& BuiltRoad : BuiltRoads)
	{
		RoadLookup.Add(BuiltRoad.Spec.Id.Value, &BuiltRoad);
	}

	int32 NumJunctionsBuilt = 0;

	for (const FVoidRoadJunction& Junction : Junctions)
	{
		if (Context.IsCancelled())
		{
			Context.Log(TEXT("Generation cancelled by user during junction pass."));
			break;
		}

		FVoidRoadJunction AdjustedJunction = Junction;
		if (Junction.Type == EVoidRoadJunctionType::CulDeSac)
		{
			AdjustedJunction.PadRadius *= 1.8f; // Turnaround bulb reads larger than a plain dead-end cap.
		}

		FVoidRoadMeshSection Pad = FVoidRoadIntersectionBuilder::BuildJunctionPad(AdjustedJunction, JunctionPadColor);
		if (Pad.IsEmpty())
		{
			continue;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AVoidRoadJunctionActor* JunctionActor = Context.TargetWorld->SpawnActor<AVoidRoadJunctionActor>(AVoidRoadJunctionActor::StaticClass(), FTransform::Identity, SpawnParams);
		if (!JunctionActor)
		{
			continue;
		}

#if WITH_EDITOR
		JunctionActor->SetActorLabel(FString::Printf(TEXT("VoidJunction_%d"), NumJunctionsBuilt));
#endif

		JunctionActor->ConnectedRoadIds = Junction.ConnectedRoadIds;
		FVoidRoadMeshBuilder::CreateSection(JunctionActor->JunctionMesh, 0, Pad, true);

		// Crosswalks only make sense at a real branching intersection.
		const bool bWantsCrosswalks = (Junction.Type == EVoidRoadJunctionType::TJunction
			|| Junction.Type == EVoidRoadJunctionType::FourWayJunction
			|| Junction.Type == EVoidRoadJunctionType::Complex);

		if (bWantsCrosswalks)
		{
			int32 CrosswalkSectionIndex = 1;
			for (const FVoidElementId& ConnectedId : Junction.ConnectedRoadIds)
			{
				const FVoidBuiltRoad* const* ConnectedRoadPtr = RoadLookup.Find(ConnectedId.Value);
				if (!ConnectedRoadPtr || (*ConnectedRoadPtr)->Points.Num() < 2)
				{
					continue;
				}

				const FVoidBuiltRoad& ConnectedRoad = **ConnectedRoadPtr;
				const bool bJunctionIsNearStart = FVector::DistSquared(ConnectedRoad.Points[0], Junction.Location) < FVector::DistSquared(ConnectedRoad.Points.Last(), Junction.Location);
				const FVector NearPoint = bJunctionIsNearStart ? ConnectedRoad.Points[0] : ConnectedRoad.Points.Last();
				const FVector InwardPoint = bJunctionIsNearStart
					? ConnectedRoad.Points[FMath::Min(1, ConnectedRoad.Points.Num() - 1)]
					: ConnectedRoad.Points[FMath::Max(0, ConnectedRoad.Points.Num() - 2)];

				FVector2D ApproachDirection(InwardPoint.X - NearPoint.X, InwardPoint.Y - NearPoint.Y);
				if (ApproachDirection.IsNearlyZero())
				{
					continue;
				}
				ApproachDirection.Normalize();

				const float ConnectedWidth = (ConnectedRoad.Spec.WidthUnits > 0.0f)
					? ConnectedRoad.Spec.WidthUnits
					: FVoidRoadTypeProfileLibrary::ResolveProfile(ConnectedRoad.Spec.RoadType, ProfileTable).DefaultWidthUnits;

				const FVector CrosswalkStart = Junction.Location + FVector(ApproachDirection.X, ApproachDirection.Y, 0.0f) * AdjustedJunction.PadRadius;

				FVoidRoadMeshSection Crosswalk = FVoidRoadIntersectionBuilder::BuildCrosswalkStripe(CrosswalkStart, ApproachDirection, ConnectedWidth, 200.0f, CrosswalkColorA, CrosswalkColorB);
				FVoidRoadMeshBuilder::CreateSection(JunctionActor->JunctionMesh, CrosswalkSectionIndex++, Crosswalk, false);
			}
		}

		if (bDrawDebug)
		{
			DrawJunctionDebug(Context.TargetWorld, AdjustedJunction, DebugDuration);
		}

		++NumJunctionsBuilt;
	}

	Context.Log(FString::Printf(TEXT("Built %d junctions."), NumJunctionsBuilt));

	const double ElapsedMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
	Context.Log(FString::Printf(TEXT("Road generation finished in %.2f ms."), ElapsedMs));
	UE_LOG(LogVoidGenerators, Log, TEXT("Road generation finished for district '%s': %d roads, %d junctions, %.2f ms."),
		*District.DistrictId.Value.ToString(), NumRoadsBuilt, NumJunctionsBuilt, ElapsedMs);

	return NumRoadsBuilt > 0;
}
