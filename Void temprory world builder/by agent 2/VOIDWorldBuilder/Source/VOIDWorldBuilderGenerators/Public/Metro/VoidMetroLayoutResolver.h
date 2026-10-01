// Copyright VOID Engineering Studio. Internal tool - not for shipping content.
// Added by Agent 2 (Phase 6 Metro Generator).

#pragma once

#include "CoreMinimal.h"
#include "Data/VoidMetroData.h"
#include "Data/VoidValidationReport.h"

class UVoidMetroGenerationSettings;

/**
 * Plain-data parameters for the layout resolver. Copied out of
 * UVoidMetroGenerationSettings so the resolver is a pure function that
 * automation tests can call without touching project settings.
 */
struct VOIDWORLDBUILDERGENERATORS_API FVoidMetroLayoutParams
{
	bool bAllowPlaceholderLayout = true;
	TMap<FName, float> BandRadiusUnits;
	TMap<FName, FName> DistrictBand;
	TMap<FName, float> DistrictAzimuthDegrees;
	float NodeAngularSpacingDegrees = 6.0f;
	int32 RingSampleCount = 180;
	EVoidMetroGrade LiveDefaultGrade = EVoidMetroGrade::AtGrade;
	float ElevatedHeightUnits = 1200.0f;
	float UndergroundDepthUnits = 1000.0f;
	float GradeRampLengthUnits = 3000.0f;
	float MaxSegmentLengthUnits = 2500.0f;
	int32 DefaultEntrancesPerStation = 1;
	float EntranceOffsetUnits = 1500.0f;
	float InterchangeToleranceUnits = 50.0f;
	float ElevatedSpanThresholdUnits = 200.0f;

	/** Same defaults as UVoidMetroGenerationSettings' constructor (kept in sync by a test). */
	static FVoidMetroLayoutParams MakeBuiltInDefaults();
	static FVoidMetroLayoutParams FromSettings(const UVoidMetroGenerationSettings* Settings);

	float GradeZ(EVoidMetroGrade Grade) const;
};

struct VOIDWORLDBUILDERGENERATORS_API FVoidMetroResolvedEntrance
{
	FName Id;
	FName StationId;
	FVector Location = FVector::ZeroVector; // ground level
	float YawDegrees = 0.0f;
	bool bPlaceholder = false;
};

struct VOIDWORLDBUILDERGENERATORS_API FVoidMetroResolvedStation
{
	FName Id;
	EVoidMetroNetworkKind Network = EVoidMetroNetworkKind::Live;
	FName DistrictId;
	FVector Location = FVector::ZeroVector; // Z = rail level for the station's grade
	float YawDegrees = 0.0f;                // direction of travel through the station
	EVoidMetroGrade Grade = EVoidMetroGrade::AtGrade;
	EVoidMetroPlatformCondition Condition = EVoidMetroPlatformCondition::Maintained;
	bool bPlaceholderPosition = false;
	bool bIsTerminus = false;
	bool bHasServiceFacility = false;
	bool bIsInterchange = false;
	TArray<FName> InterchangeLineIds;
	TArray<FVoidMetroResolvedEntrance> Entrances;
};

struct VOIDWORLDBUILDERGENERATORS_API FVoidMetroResolvedSegment
{
	FName Id;
	FName LineId;
	FName TunnelId; // set for dead-network tunnel segments
	EVoidMetroNetworkKind Network = EVoidMetroNetworkKind::Live;
	EVoidMetroGrade Grade = EVoidMetroGrade::AtGrade;
	EVoidMetroPlatformCondition Condition = EVoidMetroPlatformCondition::Maintained;
	TArray<FVector> Points;
	bool bClosed = false;
	bool bPlaceholder = false;
};

/** Where an underground run meets the surface. Exposed for the road side and used to draw portal frames. */
struct VOIDWORLDBUILDERGENERATORS_API FVoidMetroPortal
{
	FName Id;
	FName SegmentId;
	EVoidMetroNetworkKind Network = EVoidMetroNetworkKind::Live;
	FVector Location = FVector::ZeroVector;
	float YawDegrees = 0.0f;
};

struct VOIDWORLDBUILDERGENERATORS_API FVoidMetroElevatedSpan
{
	FName SegmentId;
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
};

/** Stand-alone interchange hub (only when no station already sits at the crossing). */
struct VOIDWORLDBUILDERGENERATORS_API FVoidMetroInterchangeHub
{
	FName Id;
	FName SpecId;
	FVector Location = FVector::ZeroVector;
	float YawDegrees = 0.0f;
	TArray<FName> LineIds;
};

/**
 * Fully resolved, world-space metro layout: the single structure generators
 * build from AND the structure other systems read (via FVoidMetroExportRegistry)
 * to find station coordinates, entrances, track alignment, tunnel portals and
 * elevated spans. Units: Unreal units (cm), world space, Z up.
 */
struct VOIDWORLDBUILDERGENERATORS_API FVoidMetroResolvedLayout
{
	TArray<FVoidMetroResolvedStation> Stations;
	TArray<FVoidMetroResolvedSegment> Segments;
	TArray<FVoidMetroPortal> Portals;
	TArray<FVoidMetroElevatedSpan> ElevatedSpans;
	TArray<FVoidMetroInterchangeHub> InterchangeHubs;

	/** True if ANY position/alignment in this layout was invented by the resolver rather than authored. */
	bool bUsedPlaceholder = false;

	bool IsEmpty() const { return Stations.Num() == 0 && Segments.Num() == 0; }
	const FVoidMetroResolvedStation* FindStation(FName Id) const;
};

/**
 * FVoidMetroLayoutResolver
 *
 * Turns normalized topology (FVoidMetroData) into world-space geometry inputs.
 * Rules, in priority order:
 *   1. Authored positions / centerlines are used exactly as given.
 *   2. Otherwise, if Params.bAllowPlaceholderLayout, a deterministic radial
 *      layout is derived (Spire at origin; districts placed by band radius and
 *      azimuth from Params) and every derived item is flagged bPlaceholder.
 *   3. Otherwise the item is skipped with a warning. Nothing is guessed silently.
 *
 * Never merges networks: Live and Dead items are resolved from their own
 * stations/lines only and never share geometry.
 *
 * Pure function of (Data, Params): same input => same output, so regeneration
 * produces the same actors in the same places.
 */
class VOIDWORLDBUILDERGENERATORS_API FVoidMetroLayoutResolver
{
public:
	static FVoidMetroResolvedLayout Resolve(const FVoidMetroData& Data, const FVoidMetroLayoutParams& Params, FVoidValidationReport& OutReport);

	// --- Reusable geometry helpers (public so tests and other generators can use them) ---

	/** Inserts points so no span exceeds MaxLength. Preserves original points. For closed loops the closing span is subdivided too. */
	static TArray<FVector> Densify(const TArray<FVector>& Points, float MaxLength, bool bClosed);

	/** Blends Z toward StartZ/EndZ over RampLength at each end of an open path, on top of BodyZ. */
	static void ApplyGradeRamps(TArray<FVector>& Points, float BodyZ, float StartZ, float EndZ, float RampLength);

	/** Points where Z drops through -1cm (or rises back through it): tunnel portals. */
	static void FindPortalCrossings(const TArray<FVector>& Points, TArray<FVector>& OutLocations, TArray<float>& OutYawDegrees);

	/** Contiguous runs where Z > Threshold. */
	static void FindElevatedRuns(const TArray<FVector>& Points, float Threshold, TArray<TPair<FVector, FVector>>& OutRuns);

	/** XY points where two polylines cross or come within Tolerance of each other. */
	static void FindPolylineContacts2D(const TArray<FVector>& A, bool bClosedA, const TArray<FVector>& B, bool bClosedB, float Tolerance, TArray<FVector>& OutPoints);

	/** Direction (yaw degrees) of the nearest segment of Points to Location (XY). Returns false if Points has < 2 entries. */
	static bool NearestPathYawDegrees(const TArray<FVector>& Points, bool bClosed, const FVector& Location, float& OutYawDegrees, float& OutDistance);
};
