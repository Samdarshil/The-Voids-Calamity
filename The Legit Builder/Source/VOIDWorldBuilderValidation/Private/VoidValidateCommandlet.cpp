// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VoidValidateCommandlet.h"
#include "VoidMeridianValidator.h"
#include "VoidValidationReportExporter.h"
#include "VoidValidationLog.h"
#include "VoidDesignPackageImporter.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

UVoidValidateCommandlet::UVoidValidateCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UVoidValidateCommandlet::Main(const FString& Params)
{
	FString MeridianDir, PackagePath, OutPath;
	FParse::Value(*Params, TEXT("Meridian="), MeridianDir);
	FParse::Value(*Params, TEXT("Package="), PackagePath);
	FParse::Value(*Params, TEXT("Out="), OutPath);
	const bool bStrict = FParse::Param(*Params, TEXT("Strict"));

	if (MeridianDir.IsEmpty() && PackagePath.IsEmpty())
	{
		UE_LOG(LogVoidValidation, Error, TEXT("VoidValidate needs -Meridian=\"<dir>\" and/or -Package=\"<file.json>\"."));
		return 2;
	}

	FVoidValidationOptions Options;
	Options.bAllowMissingDistrictPackages = FParse::Param(*Params, TEXT("AllowMissingDistricts"));

	FVoidValidationReport Report;
	Report.bIsValid = true;
	FVoidMeridianSummary Summary;

	if (!MeridianDir.IsEmpty())
	{
		Report.Merge(FVoidMeridianValidator::ValidateDirectory(MeridianDir, Options, &Summary));
	}

	if (!PackagePath.IsEmpty())
	{
		const FVoidImportResult Import = FVoidDesignPackageImporter::LoadFromFile(PackagePath);
		Report.Merge(Import.ValidationReport, TEXT("Import"), PackagePath);

		if (!Import.ValidationReport.HasFatalIssue())
		{
			FVoidValidationInput Input;
			Input.Package = &Import.Package;
			Input.PackageSource = PackagePath;
			Input.KnownDistrictIds = Summary.DistrictIds;
			FVoidValidationReport DataReport;
			DataReport.bIsValid = true;
			FVoidValidatorRegistry::Get().RunStage(EVoidValidationStage::PostImport, Input, Options, DataReport);
			Report.Merge(DataReport);
		}
	}
	Report.RecomputeValidity();

	if (OutPath.IsEmpty()) { OutPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("VOID/Validation/FirstLook.json")); }
	const FString Title = TEXT("VOID First-Look Validation");
	FVoidValidationReportExporter::WriteToFile(OutPath, Report, Title);
	if (FPaths::GetExtension(OutPath).ToLower() == TEXT("json"))
	{
		FVoidValidationReportExporter::WriteToFile(FPaths::ChangeExtension(OutPath, TEXT("md")), Report, Title);
	}

	UE_LOG(LogVoidValidation, Log, TEXT("\n%s"), *FVoidValidationReportExporter::ToText(Report, Title));
	UE_LOG(LogVoidValidation, Log, TEXT("Report written to %s"), *OutPath);

	const bool bFailed = Report.NumBlocking() > 0 || (bStrict && Report.NumWarnings() > 0);
	return bFailed ? 1 : 0;
}
