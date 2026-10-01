// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Data/VoidValidationReport.h"
#include "VoidValidationPipeline.h"

/**
 * Renders a validation report for humans and CI.
 *
 * Text/Markdown order issues most-severe-first, then by subsystem, code and
 * object, so the thing to fix first is at the top. Every issue prints
 * severity, subsystem, code, object id, source, field path, location,
 * description and suggested fix -- the fields a person debugging a generated
 * Meridian world needs.
 */
class VOIDWORLDBUILDERVALIDATION_API FVoidValidationReportExporter
{
public:
	static FString ToText(const FVoidValidationReport& Report, const FString& Title, const FVoidPipelineResult* Pipeline = nullptr);
	static FString ToMarkdown(const FVoidValidationReport& Report, const FString& Title, const FVoidPipelineResult* Pipeline = nullptr);
	static FString ToJson(const FVoidValidationReport& Report, const FString& Title, const FVoidPipelineResult* Pipeline = nullptr);

	/** Writes Path (creating directories). Format is chosen by extension: .json, .md, otherwise text. */
	static bool WriteToFile(const FString& Path, const FVoidValidationReport& Report, const FString& Title, const FVoidPipelineResult* Pipeline = nullptr);

	/** Issues sorted most-severe-first, then subsystem, code, object id. Stable and deterministic. */
	static TArray<const FVoidValidationIssue*> GetSortedIssues(const FVoidValidationReport& Report);
};
