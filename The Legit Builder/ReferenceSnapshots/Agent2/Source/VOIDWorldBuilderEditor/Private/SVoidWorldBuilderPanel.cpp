// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "SVoidWorldBuilderPanel.h"
#include "VoidDesignPackageImporter.h"
#include "VoidWorldBuilderLog.h"
#include "VoidGeneratorRegistry.h"
#include "VoidMetroNetworkMapper.h"
#include "Misc/Paths.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSeparator.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Interfaces/IMainFrameModule.h"
#include "GenericPlatform/GenericWindow.h"
#include "Modules/ModuleManager.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/MessageDialog.h"
#include "Styling/AppStyle.h"
#include "Editor.h"
#include "HAL/PlatformTime.h"

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
				.Text(this, &SVoidWorldBuilderPanel::GetImportSummaryText)
				.AutoWrapText(true)
			]
		]

		// --- Import Validation Panel --------------------------------------
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(8.0f, 0.0f, 8.0f, 4.0f)
		[
			SAssignNew(ImportIssueListView, SListView<TSharedPtr<FVoidValidationIssue>>)
			.ListItemsSource(&ImportIssueRows)
			.OnGenerateRow(this, &SVoidWorldBuilderPanel::OnGenerateIssueRow)
			.SelectionMode(ESelectionMode::Single)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 4.0f)
		[
			SNew(SSeparator)
		]

		// --- Generate Roads row -------------------------------------------
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 0.0f, 8.0f, 8.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("GenerateRoadsButton", "Generate Roads"))
				.IsEnabled(this, &SVoidWorldBuilderPanel::IsGenerateRoadsEnabled)
				.OnClicked(this, &SVoidWorldBuilderPanel::OnGenerateRoadsClicked)
			]
		]

		// --- Metro row (Agent 2) -------------------------------------------
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 0.0f, 8.0f, 8.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("LoadMetroButton", "Load Meridian Metro (Browse to MetroNetwork.json first)"))
				.OnClicked(this, &SVoidWorldBuilderPanel::OnLoadMetroClicked)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("GenerateMetroButton", "Generate Metro"))
				.IsEnabled(this, &SVoidWorldBuilderPanel::IsGenerateMetroEnabled)
				.OnClicked(this, &SVoidWorldBuilderPanel::OnGenerateMetroClicked)
			]
		]

		// --- Generation Summary --------------------------------------------
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 0.0f, 8.0f, 8.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(6.0f)
			[
				SNew(STextBlock)
				.Text(this, &SVoidWorldBuilderPanel::GetGenerationSummaryText)
				.AutoWrapText(true)
			]
		]

		// --- Generation Validation Panel -----------------------------------
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(8.0f, 0.0f, 8.0f, 4.0f)
		[
			SAssignNew(GenerationIssueListView, SListView<TSharedPtr<FVoidValidationIssue>>)
			.ListItemsSource(&GenerationIssueRows)
			.OnGenerateRow(this, &SVoidWorldBuilderPanel::OnGenerateIssueRow)
			.SelectionMode(ESelectionMode::Single)
		]

		// --- Generation Log -------------------------------------------------
		+ SVerticalBox::Slot()
		.FillHeight(0.6f)
		.Padding(8.0f, 0.0f, 8.0f, 8.0f)
		[
			SNew(SScrollBox)

			+ SScrollBox::Slot()
			[
				SNew(STextBlock)
				.Text(this, &SVoidWorldBuilderPanel::GetGenerationLogText)
				.AutoWrapText(true)
			]
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
	LastImportContext = Result.Context;
	NumImportInfo = Result.ValidationReport.NumInfo();
	NumImportWarnings = Result.ValidationReport.NumWarnings();
	NumImportErrors = Result.ValidationReport.NumErrors();
	NumImportFatal = Result.ValidationReport.NumFatal();

	if (bLastImportSucceeded)
	{
		LastImportedPackage = Result.Package;
	}

	ImportIssueRows.Reset();
	for (const FVoidValidationIssue& Issue : Result.ValidationReport.Issues)
	{
		ImportIssueRows.Add(MakeShared<FVoidValidationIssue>(Issue));
	}
	if (ImportIssueListView.IsValid())
	{
		ImportIssueListView->RequestListRefresh();
	}

	if (NumImportFatal > 0)
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

bool SVoidWorldBuilderPanel::IsGenerateRoadsEnabled() const
{
	return bLastImportSucceeded;
}

FReply SVoidWorldBuilderPanel::OnGenerateRoadsClicked()
{
	if (!bLastImportSucceeded)
	{
		return FReply::Handled();
	}

	const TSharedPtr<IVoidGenerator> RoadGenerator = FVoidGeneratorRegistry::Get().FindGenerator(TEXT("Road"));
	if (!RoadGenerator.IsValid())
	{
		UE_LOG(LogVoidWorldBuilder, Error, TEXT("No 'Road' generator is registered -- VOIDWorldBuilderGenerators may have failed to load."));
		return FReply::Handled();
	}

	UWorld* TargetWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TargetWorld)
	{
		UE_LOG(LogVoidWorldBuilder, Error, TEXT("No editor world available to generate into."));
		return FReply::Handled();
	}

	FVoidGenerationContext Context;
	Context.TargetWorld = TargetWorld;

	bool bResult = false;
	const double StartSeconds = FPlatformTime::Seconds();
	{
		FScopedSlowTask GenerationProgress(1.0f, LOCTEXT("GeneratingRoads", "Generating roads..."));
		GenerationProgress.MakeDialog(/*bShowCancelButton=*/true);

		// This panel never references FVoidRoadGenerator or FScopedSlowTask
		// from inside the Generators module -- the callback is how a
		// Slate-owned progress dialog communicates cancellation into
		// generator code that has no Slate dependency of its own.
		Context.IsCancellationRequested = [&GenerationProgress]()
		{
			return GenerationProgress.ShouldCancel();
		};

		bResult = RoadGenerator->Generate(LastImportedPackage, Context);
		GenerationProgress.EnterProgressFrame(1.0f);
	}

	bHasGeneratedOnce = true;
	bLastGenerationSucceeded = bResult;
	LastGenerationElapsedMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
	NumGenerationWarnings = Context.GenerationValidationReport.NumWarnings();
	NumGenerationErrors = Context.GenerationValidationReport.NumErrors();
	LastGenerationLog = Context.OutputLog;

	GenerationIssueRows.Reset();
	for (const FVoidValidationIssue& Issue : Context.GenerationValidationReport.Issues)
	{
		GenerationIssueRows.Add(MakeShared<FVoidValidationIssue>(Issue));
	}
	if (GenerationIssueListView.IsValid())
	{
		GenerationIssueListView->RequestListRefresh();
	}

	UE_LOG(LogVoidWorldBuilder, Log, TEXT("Road generation run from Editor panel: %s"), bLastGenerationSucceeded ? TEXT("SUCCEEDED") : TEXT("FAILED"));

	return FReply::Handled();
}

// -----------------------------------------------------------------------------
// Agent 2 (Phase 6 Metro). Additive. Uses the generic registry like Roads does.
// -----------------------------------------------------------------------------

bool SVoidWorldBuilderPanel::IsGenerateMetroEnabled() const
{
	return bMetroLoaded || (bLastImportSucceeded && LastImportedPackage.Metro.HasAnyContent());
}

FReply SVoidWorldBuilderPanel::OnLoadMetroClicked()
{
	FVoidValidationReport Report;
	FVoidMetroData Metro;

	const bool bRead = FVoidMetroNetworkMapper::LoadMeridianMetroNetworkFile(PackagePath, Metro, Report);
	if (bRead)
	{
		const FString RoadPath = FPaths::Combine(FPaths::GetPath(PackagePath), TEXT("RoadNetwork.json"));
		if (FPaths::FileExists(RoadPath))
		{
			FVoidMetroNetworkMapper::LoadMeridianTunnelRelationshipsFile(RoadPath, Metro, Report);
		}
		else
		{
			Report.AddWarning(TEXT("No RoadNetwork.json next to MetroNetwork.json; dead-network tunnel segments cannot be generated."), TEXT("roadNetwork"), TEXT("VOID.Metro.TunnelIdUnresolved"),
				TEXT("Place RoadNetwork.json in the same folder."));
		}
	}

	bMetroLoaded = bRead && Report.NumErrors() == 0 && Report.NumFatal() == 0;
	MetroPackage = FVoidDesignPackage();
	if (bMetroLoaded)
	{
		MetroPackage.Metro = MoveTemp(Metro);
	}

	LastGenerationLog.Reset();
	LastGenerationLog.Add(FString::Printf(TEXT("Metro data load from '%s': %s (%d errors, %d warnings)."), *PackagePath, bMetroLoaded ? TEXT("OK") : TEXT("FAILED"), Report.NumErrors() + Report.NumFatal(), Report.NumWarnings()));

	GenerationIssueRows.Reset();
	for (const FVoidValidationIssue& Issue : Report.Issues)
	{
		GenerationIssueRows.Add(MakeShared<FVoidValidationIssue>(Issue));
	}
	if (GenerationIssueListView.IsValid())
	{
		GenerationIssueListView->RequestListRefresh();
	}
	return FReply::Handled();
}

FReply SVoidWorldBuilderPanel::OnGenerateMetroClicked()
{
	if (!IsGenerateMetroEnabled())
	{
		return FReply::Handled();
	}

	const TSharedPtr<IVoidGenerator> MetroGenerator = FVoidGeneratorRegistry::Get().FindGenerator(TEXT("Metro"));
	if (!MetroGenerator.IsValid())
	{
		UE_LOG(LogVoidWorldBuilder, Error, TEXT("No 'Metro' generator is registered -- VOIDWorldBuilderGenerators may have failed to load."));
		return FReply::Handled();
	}

	UWorld* TargetWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TargetWorld)
	{
		UE_LOG(LogVoidWorldBuilder, Error, TEXT("No editor world available to generate into."));
		return FReply::Handled();
	}

	// Prefer an imported package that carries its own metro block; otherwise the metro-only package.
	const bool bUseImported = bLastImportSucceeded && LastImportedPackage.Metro.HasAnyContent();
	const FVoidDesignPackage& Source = bUseImported ? LastImportedPackage : MetroPackage;

	FVoidGenerationContext Context;
	Context.TargetWorld = TargetWorld;

	bool bResult = false;
	const double StartSeconds = FPlatformTime::Seconds();
	{
		FScopedSlowTask GenerationProgress(1.0f, LOCTEXT("GeneratingMetro", "Generating metro..."));
		GenerationProgress.MakeDialog(/*bShowCancelButton=*/true);
		Context.IsCancellationRequested = [&GenerationProgress]() { return GenerationProgress.ShouldCancel(); };
		bResult = MetroGenerator->Generate(Source, Context);
		GenerationProgress.EnterProgressFrame(1.0f);
	}

	bHasGeneratedOnce = true;
	bLastGenerationSucceeded = bResult;
	LastGenerationElapsedMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
	NumGenerationWarnings = Context.GenerationValidationReport.NumWarnings();
	NumGenerationErrors = Context.GenerationValidationReport.NumErrors();
	LastGenerationLog = Context.OutputLog;

	GenerationIssueRows.Reset();
	for (const FVoidValidationIssue& Issue : Context.GenerationValidationReport.Issues)
	{
		GenerationIssueRows.Add(MakeShared<FVoidValidationIssue>(Issue));
	}
	if (GenerationIssueListView.IsValid())
	{
		GenerationIssueListView->RequestListRefresh();
	}

	UE_LOG(LogVoidWorldBuilder, Log, TEXT("Metro generation run from Editor panel: %s"), bLastGenerationSucceeded ? TEXT("SUCCEEDED") : TEXT("FAILED"));
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

FText SVoidWorldBuilderPanel::GetImportSummaryText() const
{
	if (!bHasImportedOnce)
	{
		return LOCTEXT("NoImportYet", "No package imported yet.");
	}

	return FText::Format(
		LOCTEXT("ImportSummaryFormat", "Import -- District: {0}\nResult: {1}\nInfo: {2}   Warnings: {3}   Errors: {4}   Fatal: {5}\nElapsed: {6} ms   Source: {7}"),
		FText::FromString(DistrictName),
		bLastImportSucceeded ? LOCTEXT("ResultValid", "VALID") : LOCTEXT("ResultInvalid", "INVALID"),
		FText::AsNumber(NumImportInfo),
		FText::AsNumber(NumImportWarnings),
		FText::AsNumber(NumImportErrors),
		FText::AsNumber(NumImportFatal),
		FText::AsNumber(LastImportContext.ElapsedMilliseconds),
		FText::FromString(LastImportContext.SourceDescription));
}

FText SVoidWorldBuilderPanel::GetGenerationSummaryText() const
{
	if (!bHasGeneratedOnce)
	{
		return LOCTEXT("NoGenerationYet", "No roads generated yet. Import a package successfully, then click Generate Roads.");
	}

	return FText::Format(
		LOCTEXT("GenerationSummaryFormat", "Generation -- Result: {0}\nWarnings: {1}   Errors: {2}\nElapsed: {3} ms"),
		bLastGenerationSucceeded ? LOCTEXT("GenResultSuccess", "SUCCEEDED") : LOCTEXT("GenResultFailure", "FAILED"),
		FText::AsNumber(NumGenerationWarnings),
		FText::AsNumber(NumGenerationErrors),
		FText::AsNumber(LastGenerationElapsedMs));
}

FText SVoidWorldBuilderPanel::GetGenerationLogText() const
{
	if (LastGenerationLog.Num() == 0)
	{
		return FText::GetEmpty();
	}

	return FText::FromString(FString::Join(LastGenerationLog, TEXT("\n")));
}

#undef LOCTEXT_NAMESPACE
