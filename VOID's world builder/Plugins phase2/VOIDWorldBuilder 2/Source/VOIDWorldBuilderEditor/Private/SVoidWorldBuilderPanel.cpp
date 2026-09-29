// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "SVoidWorldBuilderPanel.h"
#include "VoidDesignPackageImporter.h"
#include "VoidWorldBuilderLog.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Layout/SBorder.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Interfaces/IMainFrameModule.h"
#include "GenericPlatform/GenericWindow.h"
#include "Modules/ModuleManager.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/MessageDialog.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "SVoidWorldBuilderPanel"

void SVoidWorldBuilderPanel::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)

		// --- Browse / Import row ---------------------------------------
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SEditableTextBox)
				.IsReadOnly(true)
				.Text(this, &SVoidWorldBuilderPanel::GetPackagePathText)
				.HintText(LOCTEXT("PathHint", "No design package selected."))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("BrowseButton", "Browse..."))
				.OnClicked(this, &SVoidWorldBuilderPanel::OnBrowseClicked)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("ImportButton", "Import && Validate"))
				.OnClicked(this, &SVoidWorldBuilderPanel::OnImportClicked)
			]
		]

		// --- Import Summary ---------------------------------------------
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 0.0f, 8.0f, 8.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(6.0f)
			[
				SNew(STextBlock)
				.Text(this, &SVoidWorldBuilderPanel::GetSummaryText)
				.AutoWrapText(true)
			]
		]

		// --- Validation Panel --------------------------------------------
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(8.0f, 0.0f, 8.0f, 8.0f)
		[
			SAssignNew(IssueListView, SListView<TSharedPtr<FVoidValidationIssue>>)
			.ListItemsSource(&IssueRows)
			.OnGenerateRow(this, &SVoidWorldBuilderPanel::OnGenerateIssueRow)
			.SelectionMode(ESelectionMode::Single)
		]
	];
}

FReply SVoidWorldBuilderPanel::OnBrowseClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return FReply::Handled();
	}

	void* ParentWindowHandle = nullptr;
	IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
	const TSharedPtr<SWindow> ParentWindow = MainFrameModule.GetParentWindow();
	if (ParentWindow.IsValid() && ParentWindow->GetNativeWindow().IsValid())
	{
		ParentWindowHandle = ParentWindow->GetNativeWindow()->GetOSWindowHandle();
	}

	TArray<FString> OutFiles;
	const bool bOpened = DesktopPlatform->OpenFileDialog(
		ParentWindowHandle,
		TEXT("Select a VOID Design Package"),
		TEXT(""),
		TEXT(""),
		TEXT("Design Package JSON (*.json)|*.json"),
		EFileDialogFlags::None,
		OutFiles);

	if (bOpened && OutFiles.Num() > 0)
	{
		PackagePath = OutFiles[0];
	}

	return FReply::Handled();
}

FReply SVoidWorldBuilderPanel::OnImportClicked()
{
	if (PackagePath.IsEmpty())
	{
		return FReply::Handled();
	}

	FVoidImportResult Result;
	{
		// A real design package will eventually be large enough that this
		// isn't instantaneous (Phase 5+ multi-district packages). The
		// slow task is wired in now, at Phase 2's small scale, so the UI
		// contract (a progress dialog appears during import) is already
		// correct rather than being retrofitted later.
		FScopedSlowTask ImportProgress(1.0f, LOCTEXT("ImportingPackage", "Importing design package..."));
		ImportProgress.MakeDialog();

		Result = FVoidDesignPackageImporter::LoadFromFile(PackagePath);
		ImportProgress.EnterProgressFrame(1.0f);
	}

	bHasImportedOnce = true;
	bLastImportSucceeded = Result.WasSuccessful();
	DistrictName = Result.Package.District.DistrictId.Value.ToString();
	LastContext = Result.Context;
	NumInfo = Result.ValidationReport.NumInfo();
	NumWarnings = Result.ValidationReport.NumWarnings();
	NumErrors = Result.ValidationReport.NumErrors();
	NumFatal = Result.ValidationReport.NumFatal();

	IssueRows.Reset();
	for (const FVoidValidationIssue& Issue : Result.ValidationReport.Issues)
	{
		IssueRows.Add(MakeShared<FVoidValidationIssue>(Issue));
	}
	if (IssueListView.IsValid())
	{
		IssueListView->RequestListRefresh();
	}

	if (NumFatal > 0)
	{
		FString FatalMessages;
		for (const FVoidValidationIssue& Issue : Result.ValidationReport.Issues)
		{
			if (Issue.Severity == EVoidValidationSeverity::Fatal)
			{
				FatalMessages += FString::Printf(TEXT("- %s\n"), *Issue.Message);
			}
		}

		FMessageDialog::Open(
			EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("FatalImportErrorFormat", "Import could not complete:\n\n{0}"),
				FText::FromString(FatalMessages)));
	}

	UE_LOG(LogVoidWorldBuilder, Log, TEXT("Import run from Editor panel: '%s' -> %s"), *PackagePath, bLastImportSucceeded ? TEXT("VALID") : TEXT("INVALID"));

	return FReply::Handled();
}

TSharedRef<ITableRow> SVoidWorldBuilderPanel::OnGenerateIssueRow(TSharedPtr<FVoidValidationIssue> Issue, const TSharedRef<STableViewBase>& OwnerTable)
{
	const FLinearColor SeverityColor = Issue.IsValid() ? GetSeverityColor(Issue->Severity) : FLinearColor::White;
	const FText SeverityText = Issue.IsValid() ? GetSeverityDisplayText(Issue->Severity) : FText::GetEmpty();
	const FText MessageText = Issue.IsValid() ? FText::FromString(Issue->Message) : FText::GetEmpty();
	const FText DetailText = Issue.IsValid()
		? FText::FromString(FString::Printf(TEXT("%s  (%s)"), *Issue->FieldPath, *Issue->ErrorCode.ToString()))
		: FText::GetEmpty();

	return SNew(STableRow<TSharedPtr<FVoidValidationIssue>>, OwnerTable)
		.Padding(FMargin(4.0f, 2.0f))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(SeverityText)
				.ColorAndOpacity(FSlateColor(SeverityColor))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(MessageText)
					.AutoWrapText(true)
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(DetailText)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
				]
			]
		];
}

FLinearColor SVoidWorldBuilderPanel::GetSeverityColor(EVoidValidationSeverity Severity)
{
	switch (Severity)
	{
		case EVoidValidationSeverity::Info:    return FLinearColor(0.4f, 0.7f, 1.0f);
		case EVoidValidationSeverity::Warning: return FLinearColor(1.0f, 0.8f, 0.2f);
		case EVoidValidationSeverity::Error:   return FLinearColor(1.0f, 0.35f, 0.3f);
		case EVoidValidationSeverity::Fatal:   return FLinearColor(1.0f, 0.05f, 0.05f);
		default:                               return FLinearColor::White;
	}
}

FText SVoidWorldBuilderPanel::GetSeverityDisplayText(EVoidValidationSeverity Severity)
{
	switch (Severity)
	{
		case EVoidValidationSeverity::Info:    return LOCTEXT("SeverityInfo", "[INFO]");
		case EVoidValidationSeverity::Warning: return LOCTEXT("SeverityWarning", "[WARN]");
		case EVoidValidationSeverity::Error:   return LOCTEXT("SeverityError", "[ERROR]");
		case EVoidValidationSeverity::Fatal:   return LOCTEXT("SeverityFatal", "[FATAL]");
		default:                               return FText::GetEmpty();
	}
}

FText SVoidWorldBuilderPanel::GetPackagePathText() const
{
	return FText::FromString(PackagePath);
}

FText SVoidWorldBuilderPanel::GetSummaryText() const
{
	if (!bHasImportedOnce)
	{
		return LOCTEXT("NoImportYet", "No package imported yet.");
	}

	return FText::Format(
		LOCTEXT("SummaryFormat", "District: {0}\nResult: {1}\nInfo: {2}   Warnings: {3}   Errors: {4}   Fatal: {5}\nElapsed: {6} ms   Source: {7}"),
		FText::FromString(DistrictName),
		bLastImportSucceeded ? LOCTEXT("ResultValid", "VALID") : LOCTEXT("ResultInvalid", "INVALID"),
		FText::AsNumber(NumInfo),
		FText::AsNumber(NumWarnings),
		FText::AsNumber(NumErrors),
		FText::AsNumber(NumFatal),
		FText::AsNumber(LastContext.ElapsedMilliseconds),
		FText::FromString(LastContext.SourceDescription));
}

#undef LOCTEXT_NAMESPACE
