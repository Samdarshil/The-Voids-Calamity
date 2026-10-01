// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "VoidValidationReportExporter.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace
{
	int32 Rank(EVoidValidationSeverity S) { return static_cast<int32>(S); } // Fatal(3) > Error(2) > Warning(1) > Info(0)

	FString LocationString(const FVoidValidationIssue& I)
	{
		return I.bHasLocation ? FString::Printf(TEXT("(%.0f, %.0f, %.0f)"), I.Location.X, I.Location.Y, I.Location.Z) : FString();
	}

	FString Verdict(const FVoidValidationReport& R)
	{
		return R.NumBlocking() == 0 ? TEXT("PASS (no blocking issues)") : TEXT("FAIL (generation cannot safely continue)");
	}

	TMap<FString, int32> CountBySubsystem(const FVoidValidationReport& R, bool bBlockingOnly)
	{
		TMap<FString, int32> Counts;
		for (const FVoidValidationIssue& I : R.Issues)
		{
			if (!bBlockingOnly || I.IsBlocking()) { Counts.FindOrAdd(I.Subsystem.IsNone() ? FString(TEXT("(unattributed)")) : I.Subsystem.ToString(), 0) += 1; }
		}
		return Counts;
	}
}

TArray<const FVoidValidationIssue*> FVoidValidationReportExporter::GetSortedIssues(const FVoidValidationReport& Report)
{
	TArray<const FVoidValidationIssue*> Sorted;
	Sorted.Reserve(Report.Issues.Num());
	for (const FVoidValidationIssue& I : Report.Issues) { Sorted.Add(&I); }
	Sorted.StableSort([](const FVoidValidationIssue* A, const FVoidValidationIssue* B)
	{
		if (A->Severity != B->Severity) { return Rank(A->Severity) > Rank(B->Severity); }
		if (A->Subsystem != B->Subsystem) { return A->Subsystem.ToString() < B->Subsystem.ToString(); }
		if (A->ErrorCode != B->ErrorCode) { return A->ErrorCode.ToString() < B->ErrorCode.ToString(); }
		return A->ObjectId < B->ObjectId;
	});
	return Sorted;
}

FString FVoidValidationReportExporter::ToText(const FVoidValidationReport& Report, const FString& Title, const FVoidPipelineResult* Pipeline)
{
	FString Out;
	Out += FString::Printf(TEXT("=== %s ===\nResult: %s\nFatal %d | Error %d | Warning %d | Info %d\n"), *Title, *Verdict(Report), Report.NumFatal(), Report.NumErrors(), Report.NumWarnings(), Report.NumInfo());
	if (Pipeline)
	{
		Out += TEXT("\nPipeline steps:\n");
		for (const FVoidPipelineStepResult& S : Pipeline->Steps)
		{
			Out += FString::Printf(TEXT("  %-28s %-20s %8.1f ms  E:%d W:%d\n"), *S.StepId.ToString(), FVoidPipelineResult::StatusToString(S.Status), S.ElapsedMs, S.NumErrors, S.NumWarnings);
		}
		if (!Pipeline->AbortedAtStep.IsNone()) { Out += FString::Printf(TEXT("  ABORTED at step '%s'\n"), *Pipeline->AbortedAtStep.ToString()); }
	}
	Out += TEXT("\n");
	for (const FVoidValidationIssue* I : GetSortedIssues(Report))
	{
		Out += FString::Printf(TEXT("[%s] %s  {%s}\n"), FVoidValidationReport::SeverityToString(I->Severity), *I->ErrorCode.ToString(), *I->Subsystem.ToString());
		Out += FString::Printf(TEXT("    %s\n"), *I->Message);
		if (!I->ObjectId.IsEmpty())     { Out += FString::Printf(TEXT("    object: %s\n"), *I->ObjectId); }
		if (!I->Source.IsEmpty())       { Out += FString::Printf(TEXT("    source: %s\n"), *I->Source); }
		if (!I->FieldPath.IsEmpty())    { Out += FString::Printf(TEXT("    path:   %s\n"), *I->FieldPath); }
		if (I->bHasLocation)            { Out += FString::Printf(TEXT("    at:     %s\n"), *LocationString(*I)); }
		if (!I->SuggestedFix.IsEmpty()) { Out += FString::Printf(TEXT("    fix:    %s\n"), *I->SuggestedFix); }
	}
	return Out;
}

FString FVoidValidationReportExporter::ToMarkdown(const FVoidValidationReport& Report, const FString& Title, const FVoidPipelineResult* Pipeline)
{
	FString Out = FString::Printf(TEXT("# %s\n\n**Result:** %s\n\n| Fatal | Error | Warning | Info |\n|---|---|---|---|\n| %d | %d | %d | %d |\n\n"), *Title, *Verdict(Report), Report.NumFatal(), Report.NumErrors(), Report.NumWarnings(), Report.NumInfo());

	const TMap<FString, int32> Blocking = CountBySubsystem(Report, true);
	if (Blocking.Num() > 0)
	{
		Out += TEXT("**Blocking issues by subsystem:** ");
		TArray<FString> Parts;
		for (const TPair<FString, int32>& P : Blocking) { Parts.Add(FString::Printf(TEXT("%s (%d)"), *P.Key, P.Value)); }
		Parts.Sort();
		Out += FString::Join(Parts, TEXT(", ")) + TEXT("\n\n");
	}
	if (Pipeline)
	{
		Out += TEXT("## Pipeline\n\n| Step | Status | ms | Errors | Warnings |\n|---|---|---|---|---|\n");
		for (const FVoidPipelineStepResult& S : Pipeline->Steps)
		{
			Out += FString::Printf(TEXT("| %s | %s | %.1f | %d | %d |\n"), *S.StepId.ToString(), FVoidPipelineResult::StatusToString(S.Status), S.ElapsedMs, S.NumErrors, S.NumWarnings);
		}
		if (!Pipeline->AbortedAtStep.IsNone()) { Out += FString::Printf(TEXT("\n**Aborted at step `%s`.**\n"), *Pipeline->AbortedAtStep.ToString()); }
		Out += TEXT("\n");
	}
	Out += TEXT("## Issues\n\n| Severity | Subsystem | Code | Object | Source / path | Location | Description | Suggested fix |\n|---|---|---|---|---|---|---|---|\n");
	auto Cell = [](FString S) { S.ReplaceInline(TEXT("|"), TEXT("\\|")); S.ReplaceInline(TEXT("\n"), TEXT(" ")); return S; };
	for (const FVoidValidationIssue* I : GetSortedIssues(Report))
	{
		Out += FString::Printf(TEXT("| %s | %s | `%s` | %s | %s %s | %s | %s | %s |\n"), FVoidValidationReport::SeverityToString(I->Severity), *I->Subsystem.ToString(), *I->ErrorCode.ToString(), *Cell(I->ObjectId), *Cell(I->Source), *Cell(I->FieldPath), *LocationString(*I), *Cell(I->Message), *Cell(I->SuggestedFix));
	}
	return Out;
}

FString FVoidValidationReportExporter::ToJson(const FVoidValidationReport& Report, const FString& Title, const FVoidPipelineResult* Pipeline)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("title"), Title);
	Root->SetBoolField(TEXT("valid"), Report.NumBlocking() == 0);

	TSharedRef<FJsonObject> Counts = MakeShared<FJsonObject>();
	Counts->SetNumberField(TEXT("fatal"), Report.NumFatal());
	Counts->SetNumberField(TEXT("error"), Report.NumErrors());
	Counts->SetNumberField(TEXT("warning"), Report.NumWarnings());
	Counts->SetNumberField(TEXT("info"), Report.NumInfo());
	Root->SetObjectField(TEXT("counts"), Counts);

	if (Pipeline)
	{
		TArray<TSharedPtr<FJsonValue>> Steps;
		for (const FVoidPipelineStepResult& S : Pipeline->Steps)
		{
			TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
			O->SetStringField(TEXT("step"), S.StepId.ToString());
			O->SetStringField(TEXT("status"), FVoidPipelineResult::StatusToString(S.Status));
			O->SetNumberField(TEXT("elapsedMs"), S.ElapsedMs);
			O->SetNumberField(TEXT("errors"), S.NumErrors);
			O->SetNumberField(TEXT("warnings"), S.NumWarnings);
			Steps.Add(MakeShared<FJsonValueObject>(O));
		}
		Root->SetArrayField(TEXT("pipelineSteps"), Steps);
		Root->SetStringField(TEXT("abortedAtStep"), Pipeline->AbortedAtStep.ToString());
	}

	TArray<TSharedPtr<FJsonValue>> Issues;
	for (const FVoidValidationIssue* I : GetSortedIssues(Report))
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("severity"), FVoidValidationReport::SeverityToString(I->Severity));
		O->SetStringField(TEXT("subsystem"), I->Subsystem.ToString());
		O->SetStringField(TEXT("code"), I->ErrorCode.ToString());
		O->SetStringField(TEXT("objectId"), I->ObjectId);
		O->SetStringField(TEXT("source"), I->Source);
		O->SetStringField(TEXT("fieldPath"), I->FieldPath);
		O->SetStringField(TEXT("description"), I->Message);
		O->SetStringField(TEXT("suggestedFix"), I->SuggestedFix);
		if (I->bHasLocation)
		{
			TArray<TSharedPtr<FJsonValue>> Loc;
			Loc.Add(MakeShared<FJsonValueNumber>(I->Location.X));
			Loc.Add(MakeShared<FJsonValueNumber>(I->Location.Y));
			Loc.Add(MakeShared<FJsonValueNumber>(I->Location.Z));
			O->SetArrayField(TEXT("location"), Loc);
		}
		Issues.Add(MakeShared<FJsonValueObject>(O));
	}
	Root->SetArrayField(TEXT("issues"), Issues);

	FString Out;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Root, Writer);
	return Out;
}

bool FVoidValidationReportExporter::WriteToFile(const FString& Path, const FVoidValidationReport& Report, const FString& Title, const FVoidPipelineResult* Pipeline)
{
	const FString Ext = FPaths::GetExtension(Path).ToLower();
	const FString Content = Ext == TEXT("json") ? ToJson(Report, Title, Pipeline) : (Ext == TEXT("md") ? ToMarkdown(Report, Title, Pipeline) : ToText(Report, Title, Pipeline));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	return FFileHelper::SaveStringToFile(Content, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
