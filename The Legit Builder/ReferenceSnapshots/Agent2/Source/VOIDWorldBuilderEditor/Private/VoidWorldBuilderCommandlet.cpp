// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VoidWorldBuilderCommandlet.h"
#include "VoidDesignPackageImporter.h"
#include "VoidWorldBuilderLog.h"
#include "Misc/Parse.h"

UVoidWorldBuilderCommandlet::UVoidWorldBuilderCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UVoidWorldBuilderCommandlet::Main(const FString& Params)
{
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
