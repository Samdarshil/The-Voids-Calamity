// Copyright VOID Engineering Studio. Internal tool - not for shipping content.
// Added by Agent 2 (Phase 6 Metro Generator).

#include "Metro/VoidMetroExports.h"

FVoidMetroExportRegistry& FVoidMetroExportRegistry::Get()
{
	static FVoidMetroExportRegistry Instance;
	return Instance;
}

void FVoidMetroExportRegistry::Publish(const FVoidMetroResolvedLayout& InLayout, FName InOwnerKey)
{
	CurrentLayout = InLayout;
	CurrentOwnerKey = InOwnerKey;
	bHasLayout = true;
	OnLayoutChanged.Broadcast();
}

void FVoidMetroExportRegistry::Clear()
{
	CurrentLayout = FVoidMetroResolvedLayout();
	CurrentOwnerKey = NAME_None;
	bHasLayout = false;
	OnLayoutChanged.Broadcast();
}
