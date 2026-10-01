// Copyright VOID Engineering Studio. Internal tool - not for shipping content.
// Added by Agent 2 (Phase 6 Metro Generator).

#pragma once

#include "CoreMinimal.h"
#include "Metro/VoidMetroLayoutResolver.h"

/**
 * FVoidMetroExportRegistry
 *
 * Read-only publication of the last generated metro layout, so other
 * generators (Road, Building, Navigation, cinematic tooling) can query
 *   - station coordinates          -> Layout().Stations[].Location
 *   - station entrances            -> Layout().Stations[].Entrances[]
 *   - track alignment              -> Layout().Segments[].Points
 *   - tunnel entrances (portals)   -> Layout().Portals[]
 *   - elevated sections            -> Layout().ElevatedSpans[]
 * WITHOUT any of them depending on metro actor classes or re-running the
 * resolver. The Road Generator is deliberately not modified; consumers pull.
 *
 * Editor-thread only. Holds a copy; safe to keep referencing until the next
 * Publish(). Cleared by Clear() (called on regenerate-failure and by tests).
 */
class VOIDWORLDBUILDERGENERATORS_API FVoidMetroExportRegistry
{
public:
	static FVoidMetroExportRegistry& Get();

	void Publish(const FVoidMetroResolvedLayout& InLayout, FName InOwnerKey);
	void Clear();

	bool HasLayout() const { return bHasLayout; }
	const FVoidMetroResolvedLayout& Layout() const { return CurrentLayout; }
	FName OwnerKey() const { return CurrentOwnerKey; }

	/** Convenience: station lookup by id (nullptr if absent). */
	const FVoidMetroResolvedStation* FindStation(FName StationId) const { return CurrentLayout.FindStation(StationId); }

	/** Fired after Publish()/Clear(). */
	FSimpleMulticastDelegate OnLayoutChanged;

private:
	FVoidMetroResolvedLayout CurrentLayout;
	FName CurrentOwnerKey;
	bool bHasLayout = false;
};
