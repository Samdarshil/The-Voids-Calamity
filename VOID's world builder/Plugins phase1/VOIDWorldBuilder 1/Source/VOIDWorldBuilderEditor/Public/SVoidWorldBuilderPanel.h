// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * SVoidWorldBuilderPanel
 *
 * Phase 1 UI: lets a designer or engineer point at a design package JSON
 * file, run it through the import pipeline, and see the resulting
 * validation report. Intentionally does not offer a "Generate" button
 * yet -- there is nothing to generate until Phase 3+.
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
	FText GetLogText() const;

	FString PackagePath;
	FString LogText;
};
