// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "VoidImportResult.h"

/**
 * FVoidDesignPackageImporter
 *
 * Top-level entry point for the import pipeline, and the single entry
 * point every future generator (Phase 3+) will use to obtain a validated
 * FVoidDesignPackage. Public signatures are unchanged from Phase 1 --
 * only the internals changed -- so existing callers (the Editor panel,
 * the commandlet) needed no call-site changes for this phase's rework.
 *
 * LoadFromFile is format-agnostic: it resolves a reader from
 * FVoidPackageReaderRegistry by the file's extension, so adding support
 * for a new on-disk format never means touching this class.
 * LoadFromJsonString is a JSON-specific convenience for callers that
 * already have JSON text in memory (automated tests, or a caller that
 * received JSON from a network response) and bypasses the registry
 * entirely, going straight to FVoidJsonPackageReader.
 */
class VOIDWORLDBUILDERIMPORT_API FVoidDesignPackageImporter
{
public:
	/** Detects format by file extension, reads, maps, and validates a design package from disk. */
	static FVoidImportResult LoadFromFile(const FString& FilePath);

	/** Reads, maps, and validates a design package from an in-memory JSON string, bypassing format detection. */
	static FVoidImportResult LoadFromJsonString(const FString& RawJson);

private:
	/** Builds a fresh FVoidImportContext with the current UVoidImportSettings snapshotted into it. */
	static FVoidImportContext MakeContext(const FString& SourceDescription);

	/** Logs the Import/Validation/Performance summary block to LogVoidImport. Called on every path, including early Fatal returns, so a failed import is never silent. */
	static void LogImportSummary(const FVoidImportResult& Result, double StartSeconds);
};
