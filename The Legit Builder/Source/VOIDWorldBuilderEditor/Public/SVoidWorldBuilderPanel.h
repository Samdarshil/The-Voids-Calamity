// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Data/VoidValidationReport.h"
#include "Data/VoidImportContext.h"
#include "Data/VoidDesignPackage.h"

class SVoidWorldBuilderPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SVoidWorldBuilderPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    FReply OnBrowseClicked();
    FReply OnImportClicked();
    FReply OnGenerateRoadsClicked();
    FReply OnGenerateDistrictsClicked();
    FReply OnGenerateBuildingsClicked();
    FReply OnGenerateEnvironmentPropsClicked();
    FReply OnGenerateLightingClicked();
    FReply OnClearLightingClicked();
    FReply OnPresetClicked(FName PresetId);
    TSharedRef<SWidget> BuildPresetButtons();

    /** Shared by package-backed generator buttons. */
    FReply RunGenerator(FName GeneratorId, const FText& ProgressText);

    FText GetPackagePathText() const;
    FText GetImportSummaryText() const;
    FText GetGenerationSummaryText() const;
    FText GetGenerationLogText() const;

    bool IsGenerateRoadsEnabled() const;
    bool IsGenerateDistrictsEnabled() const;

    TSharedRef<ITableRow> OnGenerateIssueRow(TSharedPtr<FVoidValidationIssue> Issue, const TSharedRef<STableViewBase>& OwnerTable);

    static FLinearColor GetSeverityColor(EVoidValidationSeverity Severity);
    static FText GetSeverityDisplayText(EVoidValidationSeverity Severity);

    FString PackagePath;

    FString DistrictName;
    bool bLastImportSucceeded = false;
    bool bHasImportedOnce = false;
    FVoidImportContext LastImportContext;
    int32 NumImportInfo = 0;
    int32 NumImportWarnings = 0;
    int32 NumImportErrors = 0;
    int32 NumImportFatal = 0;

    /** Last successfully imported package used by Road/Building/Environment/Props/Lighting. */
    FVoidDesignPackage LastImportedPackage;

    TArray<TSharedPtr<FVoidValidationIssue>> ImportIssueRows;
    TSharedPtr<SListView<TSharedPtr<FVoidValidationIssue>>> ImportIssueListView;

    bool bHasGeneratedOnce = false;
    bool bLastGenerationSucceeded = false;
    double LastGenerationElapsedMs = 0.0;
    int32 NumGenerationWarnings = 0;
    int32 NumGenerationErrors = 0;
    TArray<FString> LastGenerationLog;

    TArray<TSharedPtr<FVoidValidationIssue>> GenerationIssueRows;
    TSharedPtr<SListView<TSharedPtr<FVoidValidationIssue>>> GenerationIssueListView;
};
