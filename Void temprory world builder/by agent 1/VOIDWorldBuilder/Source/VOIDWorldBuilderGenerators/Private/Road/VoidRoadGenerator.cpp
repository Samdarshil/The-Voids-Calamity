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
#include "GameFramework/Actor.h"

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


namespace VoidRoadGeneratorPrivate
{
	static const FName TagGenerated(TEXT("VOID.Generated"));
	static const FName TagGeneratorRoad(TEXT("VOID.Generator.Road"));

	static int32 LegacyTier(EVoidRoadType Type)
	{
		switch (Type)
		{
			case EVoidRoadType::Highway:
			case EVoidRoadType::Primary:
			case EVoidRoadType::Roundabout: return 1;
			case EVoidRoadType::Secondary:  return 2;
			case EVoidRoadType::Local:      return 3;
			default:                        return 4; // Service, Alley
		}
	}

	static EVoidIntersectionKind ToKind(EVoidRoadJunctionType Type)
	{
		switch (Type)
		{
			case EVoidRoadJunctionType::DeadEnd:         return EVoidIntersectionKind::DeadEnd;
			case EVoidRoadJunctionType::CulDeSac:        return EVoidIntersectionKind::CulDeSac;
			case EVoidRoadJunctionType::TwoWayJoin:      return EVoidIntersectionKind::TwoWayJoin;
			case EVoidRoadJunctionType::TJunction:       return EVoidIntersectionKind::TJunction;
			case EVoidRoadJunctionType::FourWayJunction: return EVoidIntersectionKind::FourWayJunction;
			case EVoidRoadJunctionType::RoundaboutSpur:  return EVoidIntersectionKind::RoundaboutSpur;
			default:                                     return EVoidIntersectionKind::Complex;
		}
	}

	static void TagActor(AActor* Actor, const FName Id, const TArray<FName>& Districts)
	{
		Actor->Tags.AddUnique(TagGenerated);
		Actor->Tags.AddUnique(TagGeneratorRoad);
		Actor->Tags.AddUnique(FName(*FString::Printf(TEXT("VOID.Road.%s"), *Id.ToString())));
		for (const FName D : Districts) { Actor->Tags.AddUnique(FName(*FString::Printf(TEXT("VOID.District.%s"), *D.ToString()))); }
	}
}

FName FVoidRoadGenerator::GetGeneratorId() const
{
	return FName(TEXT("Road"));
}

FText FVoidRoadGenerator::GetDisplayName() const
{
	return NSLOCTEXT("VoidRoadGenerator", "DisplayName", "Road Generator");
}

void FVoidRoadGenerator::Reset(FVoidGenerationContext& Context)
{
	int32 NumDestroyed = 0;
	for (const TWeakObjectPtr<AActor>& Weak : SpawnedActors)
	{
		if (AActor* Actor = Weak.Get())
		{
			Actor->Destroy();
			++NumDestroyed;
		}
	}
	SpawnedActors.Reset();
	if (Context.RoadOutput.IsValid()) { Context.RoadOutput->Reset(); }
	Context.Results.Remove(GetGeneratorId());
	if (NumDestroyed > 0) { Context.Log(FString::Printf(TEXT("Road generator reset: destroyed %d actor(s)."), NumDestroyed)); }
}

bool FVoidRoadGenerator::Generate(const FVoidDesignPackage& Package, FVoidGenerationContext& Context)
{
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

	FBuildRequest Request;
	Request.DistrictLabel = District.DistrictId.Value;
	Request.bSpecsAreWorldSpace = false;
	for (const FVoidRoadSpec& Spec : District.Roads)
	{
		FRoadMeta Meta;
		Meta.Source = EVoidRoadSource::LegacyDesignPackage;
		Meta.HierarchyTier = VoidRoadGeneratorPrivate::LegacyTier(Spec.RoadType);
		Meta.Network = EVoidNetworkKind::Live;
		Meta.bWidthIsBuilderDefault = (Spec.WidthUnits <= 0.0f);
		if (District.DistrictId.IsValid()) { Meta.ServedDistricts.Add(District.DistrictId.Value); }
		Request.MetaByRoadId.Add(Spec.Id.Value, Meta);
	}
	return BuildRoads(District.Roads, Request, Context);
}

bool FVoidRoadGenerator::GenerateFromMeridian(const FVoidMeridianWorld& World, FVoidGenerationContext& Context)
{
	const UVoidWorldBuilderSettings* Settings = UVoidWorldBuilderSettings::Get();
	const FVoidMeridianLayout Layout = FVoidMeridianLayout::FromSettings(Settings);

	FVoidMeridianRoadPlan Plan;
	FVoidValidationReport PlanReport;
	PlanReport.bIsValid = true;
	FVoidMeridianRoadPlanner::BuildPlan(World, Context.WorldSpace, Layout, Plan, PlanReport);

	for (const FVoidValidationIssue& Issue : PlanReport.Issues)
	{
		switch (Issue.Severity)
		{
			case EVoidValidationSeverity::Warning: Context.ReportWarning(Issue.Message, Issue.FieldPath, Issue.ErrorCode, Issue.SuggestedFix); break;
			case EVoidValidationSeverity::Error:   Context.ReportError(Issue.Message, Issue.FieldPath, Issue.ErrorCode, Issue.SuggestedFix); break;
			case EVoidValidationSeverity::Fatal:   Context.ReportFatal(Issue.Message, Issue.FieldPath, Issue.ErrorCode, Issue.SuggestedFix); break;
			default: Context.Log(Issue.Message); break;
		}
	}

	if (Plan.Roads.Num() == 0)
	{
		Context.ReportError(TEXT("Meridian road plan produced no buildable roads."), TEXT("RoadNetwork"), TEXT("VOID.RoadPlan.Empty"),
			TEXT("Check the band radii in Project Settings > VOID World Builder and the messages above."));
		return false;
	}

	FBuildRequest Request;
	Request.DistrictLabel = FName(TEXT("meridian_world"));
	Request.bSpecsAreWorldSpace = true;
	Request.Crossings = Plan.Crossings;
	Request.TopologyOnly = Plan.TopologyOnly;

	TArray<FVoidRoadSpec> Specs;
	for (const FVoidPlannedRoad& Planned : Plan.Roads)
	{
		Specs.Add(Planned.Spec);
		FRoadMeta Meta;
		Meta.Source = EVoidRoadSource::MeridianDerived;
		Meta.HierarchyTier = Planned.HierarchyTier;
		Meta.Network = Planned.Network;
		Meta.CategoryId = Planned.CategoryId;
		Meta.ServedDistricts = Planned.ServedDistricts;
		Meta.bWidthIsBuilderDefault = true; // Meridian specifies no widths.
		Request.MetaByRoadId.Add(Planned.Spec.Id.Value, Meta);
	}

	Context.Log(FString::Printf(TEXT("Meridian road plan: %d road(s), %d crossing(s), %d topology-only record(s)."), Plan.Roads.Num(), Plan.Crossings.Num(), Plan.TopologyOnly.Num()));
	return BuildRoads(Specs, Request, Context);
}

bool FVoidRoadGenerator::BuildRoads(const TArray<FVoidRoadSpec>& Roads, const FBuildRequest& Request, FVoidGenerationContext& Context)
{
	using namespace VoidRoadGeneratorPrivate;

	const double StartSeconds = FPlatformTime::Seconds();

	if (!Context.TargetWorld)
	{
		Context.ReportError(TEXT("No target world provided."), FString(), TEXT("VOID.Road.NoWorld"), TEXT("Set FVoidGenerationContext::TargetWorld."));
		UE_LOG(LogVoidGenerators, Error, TEXT("FVoidRoadGenerator: TargetWorld is null."));
		return false;
	}

	if (!Context.RoadOutput.IsValid()) { Context.RoadOutput = MakeShared<FVoidRoadNetworkOutput>(); }
	FVoidRoadNetworkOutput& Output = *Context.RoadOutput;

	const UVoidRoadGenerationSettings* Settings = GetDefault<UVoidRoadGenerationSettings>();
	const float JunctionTolerance = Settings ? Settings->JunctionToleranceUnits : 50.0f;
	const float RampLength = Settings ? Settings->RampLengthUnits : 500.0f;
	const float BridgeHeight = Settings ? Settings->DefaultBridgeHeightUnits : 300.0f;
	const float TunnelDepth = Settings ? Settings->DefaultTunnelDepthUnits : 300.0f;
	const float PierSpacing = Settings ? Settings->BridgePierSpacingUnits : 800.0f;
	const bool bDrawDebug = Settings && Settings->bDrawDebugVisualization;
	const float DebugDuration = Settings ? Settings->DebugVisualizationDurationSeconds : 15.0f;
	const UDataTable* ProfileTable = (Settings && !Settings->RoadTypeProfileTable.IsNull()) ? Settings->RoadTypeProfileTable.LoadSynchronous() : nullptr;

	const double SourceScale = Context.WorldSpace.GetConfig().SourceUnitsToUU;

	const FScopedTransaction Transaction(NSLOCTEXT("VoidRoadGenerator", "GenerateRoadsTransaction", "Generate VOID Roads"));

	// Topology-only records first: they have no geometry but consumers can still see them and their hierarchy.
	for (const FVoidTopologyOnlyRoute& T : Request.TopologyOnly)
	{
		FVoidRoadRecord Rec;
		Rec.RoadId = T.Id; Rec.Source = EVoidRoadSource::MeridianDerived;
		Rec.HierarchyTier = T.HierarchyTier; Rec.Network = T.Network; Rec.CategoryId = T.CategoryId;
		Rec.RoadType = FVoidMeridianRoadPlanner::MapTierToRoadType(T.HierarchyTier);
		Rec.bHasGeometry = false; Rec.ServedDistrictIds = T.ServedDistricts;
		Output.AddRoad(MoveTemp(Rec));
		Context.Log(FString::Printf(TEXT("Recorded '%s' as topology only: %s"), *T.Id.ToString(), *T.Reason));
	}

	TArray<FVoidBuiltRoad> BuiltRoads;
	BuiltRoads.Reserve(Roads.Num());
	int32 NumRoadsBuilt = 0;

	for (const FVoidRoadSpec& SourceSpec : Roads)
	{
		if (Context.IsCancelled())
		{
			Context.Log(TEXT("Generation cancelled by user."));
			UE_LOG(LogVoidGenerators, Warning, TEXT("Road generation cancelled after %d of %d roads."), NumRoadsBuilt, Roads.Num());
			break;
		}

		const FName RoadName = SourceSpec.Id.Value;
		const FRoadMeta* FoundMeta = Request.MetaByRoadId.Find(RoadName);
		const FRoadMeta Meta = FoundMeta ? *FoundMeta : FRoadMeta();

		// ---- Coordinate normalization: the ONLY place design coordinates become world coordinates. ----
		FVoidRoadSpec RoadSpec = SourceSpec;
		if (!Request.bSpecsAreWorldSpace)
		{
			bool bConverted = true;
			for (FVector2D& P : RoadSpec.CenterlinePoints)
			{
				FVector World; FString Err;
				if (!Context.WorldSpace.TryFromSource2D(P, SourceSpec.ElevationUnits, World, &Err))
				{
					Context.ReportError(FString::Printf(TEXT("Road '%s' skipped: %s"), *RoadName.ToString(), *Err), RoadName.ToString(), TEXT("VOID.Road.CoordinateRejected"),
						TEXT("Fix the source coordinates or adjust World Space settings; data is never clamped."));
					bConverted = false;
					break;
				}
				P = FVector2D(World.X, World.Y);
				RoadSpec.ElevationUnits = static_cast<float>(World.Z);
			}
			if (!bConverted) { continue; }
			RoadSpec.WidthUnits = static_cast<float>(RoadSpec.WidthUnits * SourceScale);
			RoadSpec.RoundaboutRadiusUnits = static_cast<float>(RoadSpec.RoundaboutRadiusUnits * SourceScale);
		}

		const bool bIsRoundabout = (RoadSpec.RoadType == EVoidRoadType::Roundabout);
		const bool bClosed = bIsRoundabout || RoadSpec.bClosedLoop;
		const FVoidRoadTypeProfile Profile = FVoidRoadTypeProfileLibrary::ResolveProfile(RoadSpec.RoadType, ProfileTable);

		TArray<FVector> Points = bIsRoundabout
			? FVoidRoadSplineBuilder::BuildRoundaboutLoopPoints(RoadSpec)
			: FVoidRoadSplineBuilder::BuildCenterlinePoints(RoadSpec);

		if (Points.Num() < 2)
		{
			Context.Log(FString::Printf(TEXT("Skipped road '%s': fewer than 2 usable points."), *RoadName.ToString()));
			continue;
		}

		if (!bIsRoundabout)
		{
			FVoidRoadBridgeTunnelBuilder::ApplyElevationRamp(RoadSpec, Points, RampLength, BridgeHeight, TunnelDepth);
		}

		// Post-ramp sanity: ramps must not push a point out of the allowed range either.
		{
			FString Err;
			bool bAllOk = true;
			for (const FVector& P : Points) { if (!Context.WorldSpace.IsPositionAcceptable(P, &Err)) { bAllOk = false; break; } }
			if (!bAllOk)
			{
				Context.ReportError(FString::Printf(TEXT("Road '%s' skipped: %s"), *RoadName.ToString(), *Err), RoadName.ToString(), TEXT("VOID.Road.CoordinateRejected"), TEXT("Reduce extents or raise the coordinate limit deliberately."));
				continue;
			}
		}

		FVoidBuiltRoad BuiltRoad;
		BuiltRoad.Spec = RoadSpec;
		BuiltRoad.Points = Points; // world space (junction graph + output records)
		BuiltRoads.Add(BuiltRoad);

		// ---- Large-world safety: actor at its own bounding-box centre; geometry relative to it. ----
		FBox Bounds(ForceInit);
		for (const FVector& P : Points) { Bounds += P; }
		const FVector BoundsCenter = Bounds.GetCenter();
		const FVector ActorOrigin(BoundsCenter.X, BoundsCenter.Y, 0.0);
		TArray<FVector> LocalPoints;
		LocalPoints.Reserve(Points.Num());
		for (const FVector& P : Points) { LocalPoints.Add(P - ActorOrigin); }

		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = MakeUniqueObjectName(Context.TargetWorld, AVoidRoadActor::StaticClass(), FName(*FString::Printf(TEXT("VoidRoad_%s"), *RoadName.ToString())));
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AVoidRoadActor* RoadActor = Context.TargetWorld->SpawnActor<AVoidRoadActor>(AVoidRoadActor::StaticClass(), FTransform(ActorOrigin), SpawnParams);
		if (!RoadActor)
		{
			Context.ReportError(FString::Printf(TEXT("Failed to spawn actor for road '%s'."), *RoadName.ToString()), RoadName.ToString(), TEXT("VOID.Road.SpawnFailed"));
			BuiltRoads.Pop();
			continue;
		}
		SpawnedActors.Add(RoadActor);
		TagActor(RoadActor, RoadName, Meta.ServedDistricts);

#if WITH_EDITOR
		RoadActor->SetActorLabel(FString::Printf(TEXT("VoidRoad_%s"), *RoadName.ToString()));
#endif

		RoadActor->RoadId = RoadSpec.Id;
		RoadActor->RoadType = RoadSpec.RoadType;

		FVoidRoadSplineBuilder::ApplyPointsToSpline(RoadActor->RoadSpline, LocalPoints, bClosed);

		const float RoadWidth = (RoadSpec.WidthUnits > 0.0f ? RoadSpec.WidthUnits : Profile.DefaultWidthUnits);
		const float HalfWidth = RoadWidth * 0.5f;
		const FLinearColor& SurfaceColor = bIsRoundabout ? RoundaboutSurfaceColor : RoadSurfaceColor;

		FVoidRoadMeshSection Surface = FVoidRoadMeshBuilder::BuildRibbon(LocalPoints, -HalfWidth, HalfWidth, 0.0f, SurfaceColor, bClosed, FMath::Max(HalfWidth * 2.0f, 100.0f));
		FVoidRoadMeshBuilder::CreateSection(RoadActor->RoadMesh, 0, Surface, true);

		if (RoadSpec.bHasMedian && !bIsRoundabout)
		{
			const float MedianHalfWidth = Profile.MedianWidthUnits * 0.5f;
			FVoidRoadMeshSection Median = FVoidRoadMeshBuilder::BuildRibbon(LocalPoints, -MedianHalfWidth, MedianHalfWidth, 2.0f, MedianColor, bClosed, Profile.MedianWidthUnits * 2.0f);
			FVoidRoadMeshBuilder::CreateSection(RoadActor->RoadMesh, 1, Median, false);
		}

		if (RoadSpec.bHasSidewalk)
		{
			BuildSidewalkCurbSections(RoadActor, LocalPoints, HalfWidth, Profile, bClosed);
		}

		if (RoadSpec.bIsBridge)
		{
			const TArray<FVector> PierPositions = FVoidRoadBridgeTunnelBuilder::ComputeBridgePierPositions(RoadSpec, LocalPoints, RampLength, PierSpacing);
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
			const TArray<FVector> PortalPositions = FVoidRoadBridgeTunnelBuilder::ComputeTunnelPortalPositions(RoadSpec, LocalPoints);
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

		// ---- Publish the road record (world-space centerline). ----
		FVoidRoadRecord Rec;
		Rec.RoadId = RoadName;
		Rec.Source = Meta.Source;
		Rec.RoadType = RoadSpec.RoadType;
		Rec.HierarchyTier = Meta.HierarchyTier;
		Rec.Network = Meta.Network;
		Rec.CategoryId = Meta.CategoryId;
		Rec.WidthUU = RoadWidth;
		Rec.bWidthIsBuilderDefault = Meta.bWidthIsBuilderDefault;
		Rec.LaneCount = RoadSpec.LaneCount > 0 ? RoadSpec.LaneCount : Profile.DefaultLaneCount;
		Rec.bClosedLoop = bClosed;
		Rec.bIsBridge = RoadSpec.bIsBridge;
		Rec.bIsTunnel = RoadSpec.bIsTunnel;
		Rec.bHasMedian = RoadSpec.bHasMedian && !bIsRoundabout;
		Rec.bHasSidewalk = RoadSpec.bHasSidewalk;
		if (RoadSpec.bHasSidewalk)
		{
			Rec.SidewalkInnerOffsetUU = HalfWidth + FMath::Max(Profile.CurbWidthUnits, 0.0f);
			Rec.SidewalkOuterOffsetUU = Rec.SidewalkInnerOffsetUU + Profile.SidewalkWidthUnits;
		}
		Rec.bHasGeometry = true;
		Rec.Centerline = Points;
		Rec.ServedDistrictIds = Meta.ServedDistricts;
		Rec.Actor = RoadActor;
		Output.AddRoad(MoveTemp(Rec));

		++NumRoadsBuilt;
	}

	Context.Log(FString::Printf(TEXT("Built %d of %d roads."), NumRoadsBuilt, Roads.Num()));

	// --- Junctions (endpoint clustering, unchanged algorithm) ---------------------------------
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

		// Pad geometry is built around the junction's own origin (large-world safety).
		const FVector JunctionOrigin(Junction.Location.X, Junction.Location.Y, 0.0);
		FVoidRoadJunction LocalJunction = AdjustedJunction;
		LocalJunction.Location = FVector(0.0, 0.0, Junction.Location.Z);

		FVoidRoadMeshSection Pad = FVoidRoadIntersectionBuilder::BuildJunctionPad(LocalJunction, JunctionPadColor);
		if (Pad.IsEmpty())
		{
			continue;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AVoidRoadJunctionActor* JunctionActor = Context.TargetWorld->SpawnActor<AVoidRoadJunctionActor>(AVoidRoadJunctionActor::StaticClass(), FTransform(JunctionOrigin), SpawnParams);
		if (!JunctionActor)
		{
			continue;
		}
		SpawnedActors.Add(JunctionActor);
		JunctionActor->Tags.AddUnique(TagGenerated);
		JunctionActor->Tags.AddUnique(TagGeneratorRoad);

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

				const FVector CrosswalkStart = LocalJunction.Location + FVector(ApproachDirection.X, ApproachDirection.Y, 0.0f) * AdjustedJunction.PadRadius;

				FVoidRoadMeshSection Crosswalk = FVoidRoadIntersectionBuilder::BuildCrosswalkStripe(CrosswalkStart, ApproachDirection, ConnectedWidth, 200.0f, CrosswalkColorA, CrosswalkColorB);
				FVoidRoadMeshBuilder::CreateSection(JunctionActor->JunctionMesh, CrosswalkSectionIndex++, Crosswalk, false);
			}
		}

		if (bDrawDebug)
		{
			DrawJunctionDebug(Context.TargetWorld, AdjustedJunction, DebugDuration);
		}

		FVoidIntersectionRecord IRec;
		IRec.IntersectionId = FName(*FString::Printf(TEXT("Junction_%d"), NumJunctionsBuilt));
		IRec.Location = Junction.Location;
		IRec.Kind = ToKind(Junction.Type);
		for (const FVoidElementId& Id : Junction.ConnectedRoadIds) { IRec.RoadIds.AddUnique(Id.Value); }
		IRec.PadRadiusUU = AdjustedJunction.PadRadius;
		Output.AddIntersection(MoveTemp(IRec));

		++NumJunctionsBuilt;
	}

	// --- Analytic crossings (e.g. radial spine x ring road): a pad + an output record, no crosswalks (Meridian specifies none). ---
	int32 NumCrossings = 0;
	for (const FVoidPlannedCrossing& Crossing : Request.Crossings)
	{
		const FVoidRoadRecord* A = Output.FindRoad(Crossing.RoadA);
		const FVoidRoadRecord* B = Output.FindRoad(Crossing.RoadB);
		if (!A || !B || !A->bHasGeometry || !B->bHasGeometry) { continue; }

		FVoidRoadJunction CrossJunction;
		CrossJunction.Location = FVector(0.0, 0.0, Crossing.Location.Z);
		CrossJunction.Type = EVoidRoadJunctionType::FourWayJunction;
		CrossJunction.PadRadius = static_cast<float>(0.75 * FMath::Max(A->WidthUU, B->WidthUU));

		FVoidRoadMeshSection Pad = FVoidRoadIntersectionBuilder::BuildJunctionPad(CrossJunction, JunctionPadColor);
		if (!Pad.IsEmpty())
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AVoidRoadJunctionActor* CrossActor = Context.TargetWorld->SpawnActor<AVoidRoadJunctionActor>(AVoidRoadJunctionActor::StaticClass(), FTransform(FVector(Crossing.Location.X, Crossing.Location.Y, 0.0)), SpawnParams);
			if (CrossActor)
			{
				SpawnedActors.Add(CrossActor);
				CrossActor->Tags.AddUnique(TagGenerated);
				CrossActor->Tags.AddUnique(TagGeneratorRoad);
				CrossActor->ConnectedRoadIds = { FVoidElementId(Crossing.RoadA), FVoidElementId(Crossing.RoadB) };
				FVoidRoadMeshBuilder::CreateSection(CrossActor->JunctionMesh, 0, Pad, true);
#if WITH_EDITOR
				CrossActor->SetActorLabel(FString::Printf(TEXT("VoidCrossing_%s_%s"), *Crossing.RoadA.ToString(), *Crossing.RoadB.ToString()));
#endif
			}
		}

		FVoidIntersectionRecord IRec;
		IRec.IntersectionId = FName(*FString::Printf(TEXT("Crossing_%s_%s"), *Crossing.RoadA.ToString(), *Crossing.RoadB.ToString()));
		IRec.Location = Crossing.Location;
		IRec.Kind = EVoidIntersectionKind::Crossing;
		IRec.RoadIds = { Crossing.RoadA, Crossing.RoadB };
		IRec.PadRadiusUU = CrossJunction.PadRadius;
		Output.AddIntersection(MoveTemp(IRec));
		++NumCrossings;
	}

	Output.RebuildConnectivity();

	Context.Log(FString::Printf(TEXT("Built %d junctions and %d crossings."), NumJunctionsBuilt, NumCrossings));

	const double ElapsedMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
	Context.Log(FString::Printf(TEXT("Road generation finished in %.2f ms."), ElapsedMs));
	UE_LOG(LogVoidGenerators, Log, TEXT("Road generation finished for '%s': %d roads, %d junctions, %d crossings, %.2f ms."),
		*Request.DistrictLabel.ToString(), NumRoadsBuilt, NumJunctionsBuilt, NumCrossings, ElapsedMs);

	return NumRoadsBuilt > 0;
}
