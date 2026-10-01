// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Settings/VoidWorldBuilderSettings.h"

/**
 * FVoidWorldSpaceConfig
 *
 * Plain-data snapshot of the world-space settings, so FVoidWorldSpace is trivially unit-testable and
 * a long-running generation cannot change behaviour if Project Settings are edited mid-run.
 */
struct VOIDWORLDBUILDERCORE_API FVoidWorldSpaceConfig
{
	double SourceUnitsToUU = 1.0;
	EVoidSourceAxisConvention Axis = EVoidSourceAxisConvention::UnrealNative;
	FVector WorldOriginOffsetUU = FVector::ZeroVector;
	double MaxAbsCoordinateUU = 2097152.0;

	static FVoidWorldSpaceConfig FromSettings(const UVoidWorldBuilderSettings* Settings);
};

/**
 * FVoidWorldSpace
 *
 * THE single place where positions enter Unreal world space. No generator may convert coordinates
 * itself; they consume FVector world positions produced here (or already-normalized data such as
 * FVoidRoadNetworkOutput).
 *
 * Conventions (documented once, here):
 *   - Unreal units: 1 UU = 1 cm. Axes: +X forward, +Y right, +Z up (left-handed).
 *   - Math is done in double precision (FVector is double in UE5); nothing is narrowed to float here.
 *   - World origin = the Spire (Meridian's "radial and vertical origin anchor"), shifted by WorldOriginOffsetUU.
 *   - Polar layout frame (used for Meridian's band system): azimuth 0 deg = +X, increasing toward +Y.
 *   - Input sanitation: any non-finite value, or any resulting |X|,|Y|,|Z| > MaxAbsCoordinateUU, is
 *     REJECTED with an error string. Source data is never clamped or "fixed".
 *
 * Two input paths exist because the two data sources differ:
 *   1. Legacy FVoidDesignPackage: explicit 2D [x, y] design coordinates -> FromSource2D().
 *   2. Meridian: NO coordinates exist. Placement is Builder-derived from named radial bands ->
 *      FromPolar(). Callers must mark such output EVoidRoadSource::MeridianDerived.
 */
class VOIDWORLDBUILDERCORE_API FVoidWorldSpace
{
public:
	FVoidWorldSpace() = default;
	explicit FVoidWorldSpace(const FVoidWorldSpaceConfig& InConfig) : Config(InConfig) {}

	static FVoidWorldSpace FromSettings();

	const FVoidWorldSpaceConfig& GetConfig() const { return Config; }

	/** Legacy design-package point -> world. Returns false (OutWorld untouched) and fills OutError on invalid input. */
	bool TryFromSource2D(const FVector2D& Source, double SourceElevation, FVector& OutWorld, FString* OutError = nullptr) const;

	/** Builder polar layout -> world. Radius and elevation are already in Unreal units (layout settings are UU). */
	bool TryFromPolar(double RadiusUU, double AzimuthDegrees, double ElevationUU, FVector& OutWorld, FString* OutError = nullptr) const;

	/** True if Position is finite and within +/- MaxAbsCoordinateUU on every axis. */
	bool IsPositionAcceptable(const FVector& Position, FString* OutError = nullptr) const;

	/** World -> legacy source space (inverse of TryFromSource2D, ignoring elevation). Handy for tests and round-trip checks. */
	FVector2D ToSource2D(const FVector& World) const;

	/** Unit direction (XY plane) for an azimuth in the polar layout frame. */
	static FVector2D AzimuthToDirection2D(double AzimuthDegrees);

private:
	FVoidWorldSpaceConfig Config;
};
