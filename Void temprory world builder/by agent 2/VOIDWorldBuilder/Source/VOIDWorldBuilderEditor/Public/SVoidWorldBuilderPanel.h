// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Data/VoidValidationReport.h"
#include "Data/VoidImportContext.h"
#include "Data/VoidDesignPackage.h"

/**
 * SVoidWorldBuilderPanel
 *
 * Import section (Phase 2): browse to a design package, import it
 * (behind a progress dialog), see an Import Summary and a Validation
 * Panel listing every issue. A Fatal issue pops a blocking error dialog.
 *
 * Generation section (Phase 3): once a package has imported
 * successfully, "Generate Roads" calls through the generic
 * FVoidGeneratorRegistry -- this panel never references FVoidRoadGenerator
 * or any Road-specific type directly, so a future Building/Navigation/etc.
 * generator needs zero changes here, only its own registry registration.
 * Generation also runs behind a cancellable progress dialog and shows its
 * own generation-time Validation Summary using the same list-view pattern
 * as the import section.
 */
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

	// --- Agent 2 (Phase 6 Metro): additive; the Road/import paths above are unchanged. ---
	FReply OnLoadMetroClicked();
	FReply OnGenerateMetroClicked();
	bool IsGenerateMetroEnabled() const;

	FText GetPackagePathText() const;
	FText GetImportSummaryText() const;
	FText GetGenerationSummaryText() const;
	FText GetGenerationLogText() const;

	bool IsGenerateRoadsEnabled() const;

	TSharedRef<ITableRow> OnGenerateIssueRow(TSharedPtr<FVoidValidationIssue> Issue, const TSharedRef<STableViewBase>& OwnerTable);

	static FLinearColor GetSeverityColor(EVoidValidationSeverity Severity);
	static FText GetSeverityDisplayText(EVoidValidationSeverity Severity);

	FString PackagePath;

	/** Import Summary state, refreshed after each import. */
	FString DistrictName;
	bool bLastImportSucceeded = false;
	bool bHasImportedOnce = false;
	FVoidImportContext LastImportContext;
	int32 NumImportInfo = 0;
	int32 NumImportWarnings = 0;
	int32 NumImportErrors = 0;
	int32 NumImportFatal = 0;

	/** The last successfully imported package -- what "Generate Roads" acts on. Only meaningful when bLastImportSucceeded is true. */
	FVoidDesignPackage LastImportedPackage;

	/**
	 * Agent 2: metro-only package. "Load Meridian Metro" maps the MetroNetwork.json named in the
	 * path box (plus a sibling RoadNetwork.json for tunnel links, if present) into MetroPackage.Metro.
	 * A normally imported package whose "metro" block is non-empty is used in preference.
	 */
	FVoidDesignPackage MetroPackage;
	bool bMetroLoaded = false;

	/** Import Validation Panel state. */
	TArray<TSharedPtr<FVoidValidationIssue>> ImportIssueRows;
	TSharedPtr<SListView<TSharedPtr<FVoidValidationIssue>>> ImportIssueListView;

	/** Generation Summary state, refreshed after each "Generate Roads" click. */
	bool bHasGeneratedOnce = false;
	bool bLastGenerationSucceeded = false;
	double LastGenerationElapsedMs = 0.0;
	int32 NumGenerationWarnings = 0;
	int32 NumGenerationErrors = 0;
	TArray<FString> LastGenerationLog;

	/** Generation Validation Panel state -- same row-rendering function as the import one, separate backing array. */
	TArray<TSharedPtr<FVoidValidationIssue>> GenerationIssueRows;
	TSharedPtr<SListView<TSharedPtr<FVoidValidationIssue>>> GenerationIssueListView;
};
