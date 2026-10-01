// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"

/**
 * VoidPlan -- the pure placement planner shared by the Environment and Props
 * generators (Agent 6, Phases 12 + 14).
 *
 * DESIGN RULES
 *  - Pure: no UObject, no UWorld, no USTRUCT. Inputs and outputs are plain
 *    structs, so the whole planner is unit-testable (and was tested outside
 *    the editor, see Docs/Agent6_TestNotes.md) and can run on a worker thread.
 *  - Deterministic: every random decision is a hash of
 *    (GlobalSeed, DistrictId, RuleId, SourceElementId, Index). No FRandomStream
 *    is consumed in iteration order, so adding a road or building never
 *    reshuffles the props of elements that were already there.
 *  - Category / Context / Domain are FNames, not enums, so new categories are
 *    pure data (a DataTable row) rather than a C++ change.
 */
namespace VoidPlan
{
	// ---- Well-known ids (data may add more; these are just the built-ins) --

	namespace Context
	{
		static constexpr const TCHAR* Roadside  = TEXT("Roadside");   // kerb-side band of any road with a shoulder/sidewalk
		static constexpr const TCHAR* Sidewalk  = TEXT("Sidewalk");   // needs bHasSidewalk
		static constexpr const TCHAR* Curbside  = TEXT("Curbside");   // on the road surface at the lane edge (parked vehicles)
		static constexpr const TCHAR* Alley     = TEXT("Alley");      // Alley/Service roads only (by tier)
		static constexpr const TCHAR* Commercial= TEXT("Commercial"); // sidewalk band next to a Commercial building
		static constexpr const TCHAR* Junction  = TEXT("Junction");   // per approach at a junction
		static constexpr const TCHAR* Park      = TEXT("Park");       // area scatter inside Park areas
		static constexpr const TCHAR* Plaza     = TEXT("Plaza");      // area scatter inside Plaza areas
		static constexpr const TCHAR* Site      = TEXT("Site");       // area scatter inside areas whose Use == rule.RequiredAreaUse
		static constexpr const TCHAR* Rooftop   = TEXT("Rooftop");    // one per matching building, on the roof
		static constexpr const TCHAR* Facade    = TEXT("Facade");     // on building edges that face a road
	}

	enum class ELateralMode : uint8 { SidewalkBand = 0, RoadEdge = 1, Median = 2 };
	enum class EYawMode : uint8 { AlongTraffic = 0, FaceRoad = 1, Random = 2, OutwardFromArea = 3 };

	// ---- Inputs ----------------------------------------------------------

	struct FRoad
	{
		FName Id;
		int32 Tier = 3;               // 0 = Highway ... 5 = Alley (see FVoidEnvironmentGenerator)
		TArray<FVector> Points;       // final built points (elevation/bridge already applied)
		float HalfWidth = 300.0f;     // half of the driving surface
		float BandInner = 300.0f;     // lateral distance where the sidewalk/shoulder band starts
		float BandOuter = 450.0f;     // ...and ends
		bool bHasSidewalk = false;
		bool bHasMedian = false;
		float BandZOffset = 0.0f;     // curb height: sidewalk-band items stand on the raised sidewalk
		bool bBridge = false;
		bool bTunnel = false;
		bool bClosedLoop = false;
	};

	struct FApproach
	{
		FVector2D Dir = FVector2D(1, 0); // unit vector from the junction OUT along the road
		float HalfWidth = 300.0f;
		float BandOuter = 450.0f;
		FName RoadId;
	};

	struct FJunction
	{
		FName Key;                    // stable key, e.g. "J_<sorted road ids>"
		FVector Location = FVector::ZeroVector;
		float PadRadius = 300.0f;
		int32 NumRoads = 0;
		TArray<FApproach> Approaches;
	};

	struct FArea
	{
		FName Id;
		FName Kind;                   // "Park", "Plaza" or "Building"
		FName Use;                    // "Commercial", "Residential", "Construction", ... (NAME_None = unknown)
		TArray<FVector2D> Polygon;
		float Height = 0.0f;
	};

	struct FDistrictParams
	{
		float Polish = 0.5f;          // 0 = neglected, 1 = pristine (Meridian polish gradient)
		float DensityMultiplier = 1.0f;
		TMap<FName, float> CategoryMultiplier;
	};

	struct FRule
	{
		FName RuleId;
		FName Domain;                 // "Environment" or "Props"
		FName Category;
		FName SlotFilter;             // NAME_None = any slot of the category
		FName Context;

		// Linear contexts
		float SpacingUnits = 1500.0f;
		float StartMargin = 200.0f;
		ELateralMode LateralMode = ELateralMode::SidewalkBand;
		float LateralFraction = 0.5f;     // 0 = inner edge of band, 1 = outer edge
		float LateralInsetUnits = 150.0f; // RoadEdge mode: distance in from the road edge
		float LateralJitterUnits = 0.0f;
		float AlongJitterFraction = 0.2f; // of SpacingUnits
		bool bBothSides = true;
		bool bAlternateSides = false;
		float JunctionClearance = 0.0f;   // extra clearance beyond the junction pad
		bool bRequireSidewalk = false;
		bool bRequireMedian = false;
		bool bSkipOnBridge = false;
		int32 MinTier = 0;
		int32 MaxTier = 99;

		// Area contexts
		float DensityPer100SqM = 0.0f;    // 100 m^2 = 1,000,000 cm^2
		FName RequiredAreaUse;
		float MinBuildingHeight = 0.0f;
		float AreaProximityUnits = 1200.0f; // Commercial: max distance from a matching building
		float ZOffset = 0.0f;             // Rooftop/Facade: height above roof/ground

		// Junction context
		int32 MinJunctionRoads = 3;
		int32 MaxJunctionRoads = 99;

		// Shared
		EYawMode YawMode = EYawMode::AlongTraffic;
		float YawJitterDegrees = 0.0f;
		float ScaleMin = 1.0f;
		float ScaleMax = 1.0f;
		float Probability = 1.0f;         // base keep-probability
		float PriorityBase = 0.5f;        // budget trimming: higher survives
		float PolishResponse = 0.0f;      // -1 (clutter: fades as polish rises) .. +1 (upkeep: grows with polish)
		int32 ClusterMin = 1;
		int32 ClusterMax = 1;
		float ClusterRadius = 100.0f;
		int32 MaxPerDistrict = 0;         // 0 = unlimited
	};

	struct FInputs
	{
		FName DistrictId;
		TArray<FRoad> Roads;
		TArray<FJunction> Junctions;
		TArray<FArea> Areas;
		TArray<FVector> ExtraFocusPoints; // cinematic camera areas, landmarks (design-provided or settings-provided)
		FDistrictParams District;

		uint64 GlobalSeed = 0x564F4944ull; // "VOID"
		float DensityScale = 1.0f;
		float MinImportanceDensity = 0.35f; // density kept at importance 0 (low-value places) as a fraction
		float FocusRadiusUnits = 6000.0f;
		int32 MaxInstances = 200000;

		bool bLeftHandTraffic = false;
		bool bUseBounds = false;          // spatial filter (2D)
		FVector2D BoundsMin = FVector2D::ZeroVector;
		FVector2D BoundsMax = FVector2D::ZeroVector;
		TArray<FName> CategoryAllowList;  // empty = all
		TFunction<bool()> IsCancelled;
	};

	// ---- Outputs ---------------------------------------------------------

	struct FInstance
	{
		uint64 StableId = 0;
		FName RuleId;
		FName Domain;
		FName Category;
		FName SlotFilter;
		FName Context;
		FName SourceId;               // road / junction / area that produced it
		FVector Location = FVector::ZeroVector;
		float YawDegrees = 0.0f;
		float UniformScale = 1.0f;
		float Importance = 0.0f;
		float Priority = 0.0f;
	};

	struct FStats
	{
		int32 Candidates = 0;
		int32 RejectedBounds = 0;
		int32 RejectedContext = 0;      // wrong road tier / no sidewalk / tunnel / no matching area
		int32 RejectedJunctionClearance = 0;
		int32 RejectedInsideBuilding = 0;
		int32 RejectedDensity = 0;      // deterministic probability / importance culling
		int32 RejectedRuleCap = 0;
		int32 RejectedBudget = 0;
		int32 Kept = 0;
		bool bCancelled = false;
		TMap<FName, int32> KeptPerCategory;
	};

	// ---- API -------------------------------------------------------------

	/** Plans every rule against Inputs. Output is sorted by StableId (canonical order). */
	VOIDWORLDBUILDERGENERATORS_API TArray<FInstance> Plan(const FInputs& Inputs, const TArray<FRule>& Rules, FStats* OutStats = nullptr);

	// Exposed for tests / other generators:
	VOIDWORLDBUILDERGENERATORS_API uint64 Mix64(uint64 X);
	VOIDWORLDBUILDERGENERATORS_API uint64 HashName(FName Name);          // stable across sessions (string CRC, not FName index)
	VOIDWORLDBUILDERGENERATORS_API uint64 MakeStableId(uint64 Seed, FName District, FName RuleId, FName Source, uint64 Index);
	VOIDWORLDBUILDERGENERATORS_API float  Hash01(uint64 Id, uint32 Salt = 0);
	VOIDWORLDBUILDERGENERATORS_API bool   PointInPolygon(const FVector2D& P, const TArray<FVector2D>& Poly);
	VOIDWORLDBUILDERGENERATORS_API float  DistanceToPolygonEdges(const FVector2D& P, const TArray<FVector2D>& Poly);
}
