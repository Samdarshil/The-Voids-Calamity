// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VoidDesignPackageImporter.h"
#include "VoidPackageReaderRegistry.h"
#include "VoidJsonPackageReader.h"
#include "VoidPackageValidator.h"
#include "VoidImportSettings.h"
#include "VoidWorldBuilderImportLog.h"
#include "Misc/Paths.h"
#include "HAL/PlatformTime.h"

FVoidImportResult FVoidDesignPackageImporter::LoadFromFile(const FString& FilePath)
{
	const double StartSeconds = FPlatformTime::Seconds();

	FVoidImportResult Result;
	FVoidImportContext Context = MakeContext(FilePath);

	const TSharedPtr<IVoidPackageReader> Reader = FVoidPackageReaderRegistry::Get().FindReaderForFile(FilePath);
	if (!Reader.IsValid())
	{
		Result.ValidationReport.AddFatal(
			FString::Printf(TEXT("No package reader registered for extension '.%s'."), *FPaths::GetExtension(FilePath)),
			TEXT("file"),
			TEXT("VOID.Import.UnsupportedFormat"),
			TEXT("Use a supported format (currently: .json), or implement and register an IVoidPackageReader for this extension."));
		Result.Context = Context;
		LogImportSummary(Result, StartSeconds);
		return Result;
	}

	Reader->TryRead(FilePath, Context, Result.Package, Result.ValidationReport);

	if (!Result.ValidationReport.HasFatalIssue())
	{
		Result.ValidationReport = FVoidPackageValidator::Validate(Result.Package, Result.ValidationReport);
	}

	Context.ElapsedMilliseconds = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
	Result.Context = Context;

	LogImportSummary(Result, StartSeconds);
	return Result;
}

FVoidImportResult FVoidDesignPackageImporter::LoadFromJsonString(const FString& RawJson)
{
	const double StartSeconds = FPlatformTime::Seconds();

	FVoidImportResult Result;
	FVoidImportContext Context = MakeContext(TEXT("<in-memory JSON string>"));

	const FVoidJsonPackageReader JsonReader;
	JsonReader.TryReadFromString(RawJson, Context, Result.Package, Result.ValidationReport);

	if (!Result.ValidationReport.HasFatalIssue())
	{
		Result.ValidationReport = FVoidPackageValidator::Validate(Result.Package, Result.ValidationReport);
	}

	Context.ElapsedMilliseconds = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
	Result.Context = Context;

	LogImportSummary(Result, StartSeconds);
	return Result;
}

FVoidImportContext FVoidDesignPackageImporter::MakeContext(const FString& SourceDescription)
{
	FVoidImportContext Context;
	Context.SourceDescription = SourceDescription;

	if (const UVoidImportSettings* Settings = GetDefault<UVoidImportSettings>())
	{
		Context.bFailOnUnknownFields = Settings->bFailOnUnknownFields;
		Context.MinimumSupportedSchemaVersion = Settings->MinimumSupportedSchemaVersion;
	}

	return Context;
}

void FVoidDesignPackageImporter::LogImportSummary(const FVoidImportResult& Result, double StartSeconds)
{
	const double ElapsedMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;

	UE_LOG(LogVoidImport, Log, TEXT("--- Import Summary: %s ---"), *Result.Context.SourceDescription);
	UE_LOG(LogVoidImport, Log, TEXT("District: %s"), *Result.Package.District.DistrictId.Value.ToString());
	UE_LOG(LogVoidImport, Log, TEXT("Valid: %s"), Result.WasSuccessful() ? TEXT("true") : TEXT("false"));
	UE_LOG(LogVoidImport, Log, TEXT("--- Validation Summary ---"));
	UE_LOG(LogVoidImport, Log, TEXT("Info: %d   Warnings: %d   Errors: %d   Fatal: %d"),
		Result.ValidationReport.NumInfo(),
		Result.ValidationReport.NumWarnings(),
		Result.ValidationReport.NumErrors(),
		Result.ValidationReport.NumFatal());
	UE_LOG(LogVoidImport, Log, TEXT("--- Performance Summary ---"));
	UE_LOG(LogVoidImport, Log, TEXT("Elapsed: %.2f ms"), ElapsedMs);

	for (const FVoidValidationIssue& Issue : Result.ValidationReport.Issues)
	{
		const TCHAR* SeverityText = TEXT("INFO");
		switch (Issue.Severity)
		{
			case EVoidValidationSeverity::Warning: SeverityText = TEXT("WARN");  break;
			case EVoidValidationSeverity::Error:   SeverityText = TEXT("ERROR"); break;
			case EVoidValidationSeverity::Fatal:   SeverityText = TEXT("FATAL"); break;
			default: break;
		}

		UE_LOG(LogVoidImport, Log, TEXT("  [%s] (%s) %s -- %s"),
			SeverityText, *Issue.ErrorCode.ToString(), *Issue.Message, *Issue.FieldPath);
	}
}
