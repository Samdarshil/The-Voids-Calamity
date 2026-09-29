// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Data/VoidValidationReport.h"
#include "Data/VoidImportContext.h"

/**
 * SVoidWorldBuilderPanel
 *
 * The Phase 2 import/validate panel: browse to a design package, import
 * it (behind a real progress dialog via FScopedSlowTask), see an Import
 * Summary (district, pass/fail, counts by severity, elapsed time), and
 * a Validation Panel listing every issue with its severity, message,
 * field path, and error code. A Fatal issue additionally pops a blocking
 * error dialog, since Fatal means the import could not be completed at
 * all and deserves an interrupt rather than being buried in a scrollable
 * list alongside routine Info/Warning entries.
 *
 * Intentionally does not offer a "Generate" button -- there is nothing
 * to generate until Phase 3+.
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

	FText GetPackagePathText() const;
	FText GetSummaryText() const;

	TSharedRef<ITableRow> OnGenerateIssueRow(TSharedPtr<FVoidValidationIssue> Issue, const TSharedRef<STableViewBase>& OwnerTable);

	static FLinearColor GetSeverityColor(EVoidValidationSeverity Severity);
	static FText GetSeverityDisplayText(EVoidValidationSeverity Severity);

	FString PackagePath;

	/** Import Summary state, refreshed after each import. */
	FString DistrictName;
	bool bLastImportSucceeded = false;
	bool bHasImportedOnce = false;
	FVoidImportContext LastContext;
	int32 NumInfo = 0;
	int32 NumWarnings = 0;
	int32 NumErrors = 0;
	int32 NumFatal = 0;

	/** Validation Panel state. */
	TArray<TSharedPtr<FVoidValidationIssue>> IssueRows;
	TSharedPtr<SListView<TSharedPtr<FVoidValidationIssue>>> IssueListView;
};
