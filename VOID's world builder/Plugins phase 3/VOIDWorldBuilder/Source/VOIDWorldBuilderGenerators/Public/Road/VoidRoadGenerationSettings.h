// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/DataTable.h"
#include "VoidRoadGenerationSettings.generated.h"

/**
 * UVoidRoadGenerationSettings
 *
 * Project-wide configuration for the Road Generator, exposed under
 * Edit -> Project Settings -> Plugins -> VOID World Builder Road
 * Generation. Mirrors UVoidImportSettings's pattern (Phase 2) for
 * consistency: a UDeveloperSettings class rather than a hand-built
 * settings panel.
 */
UCLASS(Config = VOIDWorldBuilder, DefaultConfig, meta = (DisplayName = "VOID World Builder Road Generation"))
class VOIDWORLDBUILDERGENERATORS_API UVoidRoadGenerationSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UVoidRoadGenerationSettings();

	/**
	 * Optional DataTable of FVoidRoadTypeProfile rows, one per EVoidRoadType
	 * (row name must match the enum's display name, e.g. "Highway"). If
	 * unset, or if a given RoadType has no matching row, the Road Generator
	 * falls back to FVoidRoadTypeProfileLibrary::GetBuiltInDefault --
	 * generation always works even with this left empty.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Road Profiles", meta = (DisplayName = "Road Type Profile Table"))
	TSoftObjectPtr<UDataTable> RoadTypeProfileTable;

	/** Horizontal length, in Unreal units, over which a bridge or tunnel ramps from ground elevation to its target elevation at each end of the span. */
	UPROPERTY(Config, EditAnywhere, Category = "Bridges And Tunnels", meta = (DisplayName = "Ramp Length (Units)", ClampMin = "50.0"))
	float RampLengthUnits = 500.0f;

	/** Additional height a bridge deck sits above ground elevation when the road spec doesn't otherwise specify one via ElevationUnits. */
	UPROPERTY(Config, EditAnywhere, Category = "Bridges And Tunnels", meta = (DisplayName = "Default Bridge Deck Height (Units)"))
	float DefaultBridgeHeightUnits = 300.0f;

	/** Additional depth a tunnel sits below ground elevation when the road spec doesn't otherwise specify one via ElevationUnits. */
	UPROPERTY(Config, EditAnywhere, Category = "Bridges And Tunnels", meta = (DisplayName = "Default Tunnel Depth (Units)"))
	float DefaultTunnelDepthUnits = 300.0f;

	/** Spacing, in Unreal units, between instanced pier markers along a bridge span. */
	UPROPERTY(Config, EditAnywhere, Category = "Bridges And Tunnels", meta = (DisplayName = "Bridge Pier Spacing (Units)", ClampMin = "100.0"))
	float BridgePierSpacingUnits = 800.0f;

	/**
	 * Distance, in Unreal units, within which two different roads' endpoint
	 * coordinates are treated as the same junction when no explicit
	 * ConnectionIds are given. Design packages authored by hand or by a
	 * tool with floating-point drift may not produce exactly identical
	 * coordinates for what's meant to be the same intersection.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Intersections", meta = (DisplayName = "Junction Coincidence Tolerance (Units)", ClampMin = "1.0"))
	float JunctionToleranceUnits = 50.0f;

	/** If true, generation draws persistent debug lines/spheres along splines and at junctions -- useful for iterating on a design package before committing to full mesh generation. */
	UPROPERTY(Config, EditAnywhere, Category = "Debug", meta = (DisplayName = "Draw Debug Visualization"))
	bool bDrawDebugVisualization = false;

	/** How long debug visualization persists, in seconds. Ignored (drawn persistently until manually cleared) if set to 0 or less. */
	UPROPERTY(Config, EditAnywhere, Category = "Debug", meta = (DisplayName = "Debug Visualization Duration (Seconds)"))
	float DebugVisualizationDurationSeconds = 15.0f;
};
