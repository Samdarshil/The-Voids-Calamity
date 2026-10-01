// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VoidWorldBuilderCommandlet.h"
#include "VoidDesignPackageImporter.h"
#include "VoidWorldBuilderLog.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "VoidMeridianImporter.h"
#include "Road/VoidMeridianRoadPlanner.h"
#include "Coordinates/VoidWorldSpace.h"
#include "Settings/VoidWorldBuilderSettings.h"

UVoidWorldBuilderCommandlet::UVoidWorldBuilderCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

namespace
{
	/** -Meridian=<Meridian_Master.json or its folder> [-NoStrictCanonLock] [-PlanRoads]: headless import + validation (+ road plan dump). */
	int32 RunMeridian(const FString& Params, const FString& MeridianPath)
	{
		FVoidMeridianImportOptions Options = FVoidMeridianImportOptions::FromSettings();
		if (FParse::Param(*Params, TEXT("NoStrictCanonLock"))) { Options.bStrictCanonLock = false; }

		const FString MasterPath = FPaths::DirectoryExists(MeridianPath) ? FPaths::Combine(MeridianPath, TEXT("Meridian_Master.json")) : MeridianPath;
		const FVoidMeridianImportResult Result = FVoidMeridianImporter::LoadFromMasterFile(MasterPath, Options);

		UE_LOG(LogVoidWorldBuilder, Log, TEXT("VoidWorldBuilderCommandlet: Meridian '%s' -> %s (%d registries, %d district(s), %d route(s)). See LogVoidImport for every issue."),
			*MeridianPath, Result.WasSuccessful() ? TEXT("VALID") : TEXT("INVALID"), Result.LoadedRegistries.Num(),
			Result.World.Districts.Num(), Result.World.RoadNetwork.Routes.Num());

		if (!Result.WasSuccessful()) { return 1; }

		if (FParse::Param(*Params, TEXT("PlanRoads")))
		{
			FVoidMeridianRoadPlan Plan;
			FVoidValidationReport PlanReport;
			PlanReport.bIsValid = true;
			const bool bAny = FVoidMeridianRoadPlanner::BuildPlan(Result.World, FVoidWorldSpace::FromSettings(),
				FVoidMeridianLayout::FromSettings(UVoidWorldBuilderSettings::Get()), Plan, PlanReport);

			for (const FVoidPlannedRoad& R : Plan.Roads)
			{
				UE_LOG(LogVoidWorldBuilder, Log, TEXT("  Road '%s': tier %d, %d pts, radius %.0f..%.0f UU, azimuth %.1f deg%s"),
					*R.Spec.Id.Value.ToString(), R.HierarchyTier, R.Spec.CenterlinePoints.Num(), R.MinRadiusUU, R.MaxRadiusUU, R.AzimuthDegrees, R.bIsRing ? TEXT(", closed ring") : TEXT(""));
			}
			for (const FVoidPlannedCrossing& C : Plan.Crossings)
			{
				UE_LOG(LogVoidWorldBuilder, Log, TEXT("  Crossing '%s' x '%s' at (%.0f, %.0f, %.0f)"), *C.RoadA.ToString(), *C.RoadB.ToString(), C.Location.X, C.Location.Y, C.Location.Z);
			}
			for (const FVoidTopologyOnlyRoute& T : Plan.TopologyOnly)
			{
				UE_LOG(LogVoidWorldBuilder, Log, TEXT("  Topology only '%s': %s"), *T.Id.ToString(), *T.Reason);
			}
			for (const FVoidValidationIssue& I : PlanReport.Issues)
			{
				UE_LOG(LogVoidWorldBuilder, Log, TEXT("  [plan] (%s) %s"), *I.ErrorCode.ToString(), *I.Message);
			}
			if (!bAny || !PlanReport.bIsValid) { return 1; }
		}
		return 0;
	}
}

int32 UVoidWorldBuilderCommandlet::Main(const FString& Params)
{
	FString MeridianPath;
	if (FParse::Value(*Params, TEXT("Meridian="), MeridianPath) && !MeridianPath.IsEmpty())
	{
		return RunMeridian(Params, MeridianPath);
	}

	FString PackagePath;
	if (!FParse::Value(*Params, TEXT("Package="), PackagePath) || PackagePath.IsEmpty())
	{
		UE_LOG(LogVoidWorldBuilder, Error, TEXT("VoidWorldBuilderCommandlet requires -Package=\"<path to json>\" or -Meridian=\"<Meridian_Master.json or folder>\""));
		return 1;
	}

	// FVoidDesignPackageImporter::LoadFromFile already logs a full
	// Import/Validation/Performance summary (including every issue, its
	// severity, error code, and field path) to LogVoidImport -- see
	// FVoidDesignPackageImporter::LogImportSummary. The commandlet's job
	// is just to surface the top-line result and translate it into an
	// exit code CI can act on; duplicating the full issue dump here
	// would just print everything twice under two different log
	// categories.
	const FVoidImportResult Result = FVoidDesignPackageImporter::LoadFromFile(PackagePath);

	UE_LOG(LogVoidWorldBuilder, Log, TEXT("VoidWorldBuilderCommandlet: '%s' -> %s (%.2fms). See LogVoidImport above for full detail."),
		*PackagePath,
		Result.WasSuccessful() ? TEXT("VALID") : TEXT("INVALID"),
		Result.Context.ElapsedMilliseconds);

	// Non-zero exit code on validation failure so CI treats it as a failed check.
	return Result.WasSuccessful() ? 0 : 1;
}
