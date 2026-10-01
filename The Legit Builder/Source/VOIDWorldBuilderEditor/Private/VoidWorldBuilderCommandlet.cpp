// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VoidWorldBuilderCommandlet.h"
#include "VoidDesignPackageImporter.h"
#include "VoidWorldBuilderLog.h"
#include "Misc/Parse.h"
#include "Meridian/VoidMeridianRegistryReader.h"
#include "District/VoidDistrictLayoutBuilder.h"
#include "District/VoidDistrictValidator.h"
#include "District/VoidDistrictGenerationSettings.h"

UVoidWorldBuilderCommandlet::UVoidWorldBuilderCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

namespace
{
	/** Phase 5 CI mode: load Meridian, plan districts, validate. Touches no world, so it runs without a level. */
	int32 RunMeridianPlanCheck(const FString& MeridianDir, const FString& OverridesPath)
	{
		FVoidMeridianDataSet Data;
		FVoidValidationReport LoadReport;
		LoadReport.bIsValid = true;
		if (!FVoidMeridianRegistryReader::LoadFromDirectory(MeridianDir, Data, LoadReport))
		{
			for (const FVoidValidationIssue& Issue : LoadReport.Issues)
			{
				UE_LOG(LogVoidWorldBuilder, Error, TEXT("[Meridian] %s (%s)"), *Issue.Message, *Issue.ErrorCode.ToString());
			}
			return 1;
		}

		FVoidMeridianLayoutOverrides Overrides;
		FVoidValidationReport OverrideReport;
		OverrideReport.bIsValid = true;
		if (!FVoidMeridianRegistryReader::LoadOverridesFromFile(OverridesPath, Overrides, OverrideReport, false))
		{
			return 1;
		}

		const UVoidDistrictGenerationSettings* Settings = GetDefault<UVoidDistrictGenerationSettings>();
		FVoidDistrictLayoutInput Input;
		Input.Data = &Data;
		Input.Overrides = &Overrides;
		Input.Params = Settings->Layout;
		Input.CharacterOverrides = Settings->CharacterOverrides;

		FVoidDistrictLayoutPlan Plan = FVoidDistrictLayoutBuilder::Build(Input);
		if (!Plan.Report.HasFatalIssue())
		{
			FVoidDistrictValidator::Validate(Plan, Data, Input.Params);
		}

		for (const FVoidValidationIssue& Issue : Plan.Report.Issues)
		{
			const ELogVerbosity::Type Verbosity = (Issue.Severity == EVoidValidationSeverity::Error || Issue.Severity == EVoidValidationSeverity::Fatal) ? ELogVerbosity::Error
				: (Issue.Severity == EVoidValidationSeverity::Warning ? ELogVerbosity::Warning : ELogVerbosity::Log);
			UE_LOG(LogVoidWorldBuilder, Log, TEXT("[District %s] %s (%s)"), Verbosity == ELogVerbosity::Error ? TEXT("Error") : (Verbosity == ELogVerbosity::Warning ? TEXT("Warning") : TEXT("Info")), *Issue.Message, *Issue.ErrorCode.ToString());
		}

		const bool bOk = Plan.Report.NumErrors() == 0 && !Plan.Report.HasFatalIssue();
		UE_LOG(LogVoidWorldBuilder, Log, TEXT("VoidWorldBuilderCommandlet: Meridian district plan '%s' -> %s (%d errors, %d warnings)."), *MeridianDir, bOk ? TEXT("VALID") : TEXT("INVALID"), Plan.Report.NumErrors(), Plan.Report.NumWarnings());
		return bOk ? 0 : 1;
	}
}

int32 UVoidWorldBuilderCommandlet::Main(const FString& Params)
{
	// Phase 5: -MeridianDir="<folder>" [-Overrides="<json>"] validates the district plan instead of importing a package.
	FString MeridianDir;
	if (FParse::Value(*Params, TEXT("MeridianDir="), MeridianDir) && !MeridianDir.IsEmpty())
	{
		FString OverridesPath;
		FParse::Value(*Params, TEXT("Overrides="), OverridesPath);
		return RunMeridianPlanCheck(MeridianDir, OverridesPath);
	}

	FString PackagePath;
	if (!FParse::Value(*Params, TEXT("Package="), PackagePath) || PackagePath.IsEmpty())
	{
		UE_LOG(LogVoidWorldBuilder, Error, TEXT("VoidWorldBuilderCommandlet requires -Package=\"<path to json>\""));
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
