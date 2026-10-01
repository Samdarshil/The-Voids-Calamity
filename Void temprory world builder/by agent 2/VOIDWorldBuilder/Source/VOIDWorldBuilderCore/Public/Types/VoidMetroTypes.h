// Copyright VOID Engineering Studio. Internal tool - not for shipping content.
// Added by Agent 2 (Phase 6 Metro Generator). Additive: no existing Core type was changed by this file.

#pragma once

#include "CoreMinimal.h"
#include "VoidMetroTypes.generated.h"

/**
 * Which of Meridian's two transit networks a metro element belongs to.
 * MetroNetwork.json ("dual_network_never_merged") requires the Live and
 * Dead networks to stay separate at every level, so this is a closed enum
 * carried on every station/line/segment and checked by FVoidMetroValidator.
 */
UENUM(BlueprintType)
enum class EVoidMetroNetworkKind : uint8
{
	Live UMETA(DisplayName = "Live Network (Grav-Rail)"),
	Dead UMETA(DisplayName = "Dead Network (Pre-Council Subway)")
};

/** Vertical placement of a station or track run relative to ground. */
UENUM(BlueprintType)
enum class EVoidMetroGrade : uint8
{
	AtGrade,
	Elevated,
	Underground
};

/**
 * Derived from MetroNetwork.json networks[].platforms.condition
 * ("clean_maintained_..." vs "abandoned_deteriorated"). Drives colour
 * palette and whether a maintenance placeholder is generated.
 */
UENUM(BlueprintType)
enum class EVoidMetroPlatformCondition : uint8
{
	Maintained,
	Abandoned
};

/**
 * How the layout resolver should build geometry for a line when the
 * source data carries no coordinates. Inferred by the mapper from the
 * Meridian line's maps_to_road_category / segment_model fields; Authored
 * means explicit centerline points were supplied and nothing is inferred.
 */
UENUM(BlueprintType)
enum class EVoidMetroLineTopology : uint8
{
	Unspecified,
	Radial,
	Ring,
	LegacySegments,
	Authored
};

/** Where a resolved position came from. Placeholder positions are never canon. */
UENUM(BlueprintType)
enum class EVoidMetroPositionProvenance : uint8
{
	Authored,
	ResolvedPlaceholder
};
