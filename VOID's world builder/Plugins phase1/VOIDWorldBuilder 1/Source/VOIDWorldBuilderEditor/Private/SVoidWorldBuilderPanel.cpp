// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "SVoidWorldBuilderPanel.h"
#include "VoidDesignPackageImporter.h"
#include "VoidWorldBuilderLog.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Interfaces/IMainFrameModule.h"
#include "GenericPlatform/GenericWindow.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "SVoidWorldBuilderPanel"

void SVoidWorldBuilderPanel::Construct(const FArguments& InArgs)
{
	LogText = TEXT("No package imported yet.");

	ChildSlot
	[
		SNew(SVerticalBox)

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

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(8.0f)
		[
			SNew(SScrollBox)

			+ SScrollBox::Slot()
			[
				SNew(STextBlock)
				.Text(this, &SVoidWorldBuilderPanel::GetLogText)
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
		LogText = TEXT("Select a design package file first.");
		return FReply::Handled();
	}

	const FVoidImportResult Result = FVoidDesignPackageImporter::LoadFromFile(PackagePath);

	TArray<FString> Lines;
	Lines.Add(FString::Printf(TEXT("District: %s"), *Result.Package.DistrictId.Value.ToString()));
	Lines.Add(FString::Printf(TEXT("Valid: %s"), Result.WasSuccessful() ? TEXT("YES") : TEXT("NO")));
	Lines.Add(FString::Printf(TEXT("Errors: %d   Warnings: %d"), Result.ValidationReport.NumErrors(), Result.ValidationReport.NumWarnings()));
	Lines.Add(FString());

	for (const FVoidValidationIssue& Issue : Result.ValidationReport.Issues)
	{
		const TCHAR* SeverityText = (Issue.Severity == EVoidValidationSeverity::Error) ? TEXT("ERROR") : TEXT("WARN");
		Lines.Add(FString::Printf(TEXT("[%s] %s (%s)"), SeverityText, *Issue.Message, *Issue.FieldPath));
	}

	LogText = FString::Join(Lines, TEXT("\n"));

	UE_LOG(LogVoidWorldBuilder, Log, TEXT("Import run from Editor panel: %s"), *LogText);

	return FReply::Handled();
}

FText SVoidWorldBuilderPanel::GetPackagePathText() const
{
	return FText::FromString(PackagePath);
}

FText SVoidWorldBuilderPanel::GetLogText() const
{
	return FText::FromString(LogText);
}

#undef LOCTEXT_NAMESPACE
