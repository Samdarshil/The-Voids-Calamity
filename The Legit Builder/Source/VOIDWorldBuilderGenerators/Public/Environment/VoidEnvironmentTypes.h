// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Environment/VoidPlacementPlanner.h"
#include "VoidEnvironmentTypes.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UInstancedStaticMeshComponent;

/** Well-known category ids. Categories are FNames (data), so new ones need no C++ -- these are just the built-ins with placeholders. */
namespace VoidDressing
{
	namespace Category
	{
		// Environment (Phase 12)
		static constexpr const TCHAR* Tree = TEXT("Tree");
		static constexpr const TCHAR* Grass = TEXT("Grass");
		static constexpr const TCHAR* Bush = TEXT("Bush");
		static constexpr const TCHAR* Rock = TEXT("Rock");
		static constexpr const TCHAR* Planter = TEXT("Planter");
		static constexpr const TCHAR* Bench = TEXT("Bench");
		static constexpr const TCHAR* Streetlight = TEXT("Streetlight");
		static constexpr const TCHAR* Pole = TEXT("Pole");
		static constexpr const TCHAR* TrafficLight = TEXT("TrafficLight");
		static constexpr const TCHAR* SidewalkDetail = TEXT("SidewalkDetail");
		static constexpr const TCHAR* StreetProp = TEXT("StreetProp");
		// Shared
		static constexpr const TCHAR* Sign = TEXT("Sign");
		static constexpr const TCHAR* Barrier = TEXT("Barrier");
		static constexpr const TCHAR* Bin = TEXT("Bin");
		static constexpr const TCHAR* Trash = TEXT("Trash");
		// Props (Phase 14)
		static constexpr const TCHAR* Vehicle = TEXT("Vehicle");
		static constexpr const TCHAR* Container = TEXT("Container");
		static constexpr const TCHAR* Crate = TEXT("Crate");
		static constexpr const TCHAR* Pipe = TEXT("Pipe");
		static constexpr const TCHAR* Billboard = TEXT("Billboard");
		static constexpr const TCHAR* ConstructionAsset = TEXT("ConstructionAsset");
	}

	namespace Domain
	{
		static constexpr const TCHAR* Environment = TEXT("Environment");
		static constexpr const TCHAR* Props = TEXT("Props");
	}
}

UENUM(BlueprintType)
enum class EVoidLateralMode : uint8
{
	SidewalkBand UMETA(DisplayName = "Sidewalk / Shoulder Band"),
	RoadEdge     UMETA(DisplayName = "Road Edge (inset from surface edge)"),
	Median       UMETA(DisplayName = "Centerline / Median")
};

UENUM(BlueprintType)
enum class EVoidYawMode : uint8
{
	AlongTraffic    UMETA(DisplayName = "Along Traffic Direction"),
	FaceRoad        UMETA(DisplayName = "Face The Road"),
	Random          UMETA(DisplayName = "Random"),
	OutwardFromArea UMETA(DisplayName = "Outward / Away From Road")
};

/**
 * FVoidAssetSlotRow -- one replaceable asset slot. Row name = SlotId.
 *
 * This is THE hook for "Placeholder Tree -> AAA Tree": add a row with the same
 * Category and a real Mesh. Leave Mesh empty (or omit the row) and the built-in
 * placeholder for that Category is used. Multiple rows per Category give
 * deterministic weighted variation.
 */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERGENERATORS_API FVoidAssetSlotRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Asset Slot")
	FName Category;

	/** Optional. Empty = use the built-in placeholder for this Category. Never assumed to exist. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Asset Slot")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Asset Slot")
	TArray<TSoftObjectPtr<UMaterialInterface>> Materials;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Asset Slot")
	FVector LocalOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Asset Slot")
	FRotator LocalRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Asset Slot")
	FVector ScaleMultiplier = FVector::OneVector;

	/** Relative selection weight among slots of the same Category. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Asset Slot", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Asset Slot")
	bool bUseHISM = true;

	/** 0 = use the category default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Asset Slot")
	float CullStartUnits = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Asset Slot")
	float CullEndUnits = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Asset Slot")
	bool bCastShadow = true;

	/** Free-form tag copied onto the generated component so a future replacement pass can find "everything that was slot X". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID Asset Slot")
	FName ReplacementTag;
};

/** FVoidPlacementRuleRow -- one data-driven placement rule. Row name = RuleId. Mirrors VoidPlan::FRule. */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERGENERATORS_API FVoidPlacementRuleRow : public FTableRowBase
{
	GENERATED_BODY()

	/** "Environment" or "Props". Decides which generator runs the rule. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
	FName Domain = FName(TEXT("Environment"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
	FName Category;

	/** Optional: restrict to one slot id of the category. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
	FName SlotFilter;

	/** Roadside, Sidewalk, Curbside, Alley, Commercial, Junction, Park, Plaza, Site, Rooftop, Facade. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
	FName Context = FName(TEXT("Sidewalk"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Linear")
	float SpacingUnits = 1500.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Linear")
	float StartMargin = 200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Linear")
	EVoidLateralMode LateralMode = EVoidLateralMode::SidewalkBand;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Linear")
	float LateralFraction = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Linear")
	float LateralInsetUnits = 150.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Linear")
	float LateralJitterUnits = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Linear")
	float AlongJitterFraction = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Linear")
	bool bBothSides = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Linear")
	bool bAlternateSides = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Linear")
	float JunctionClearance = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Linear")
	bool bRequireSidewalk = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Linear")
	bool bRequireMedian = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Linear")
	bool bSkipOnBridge = false;
	/** 0 Highway, 1 Primary/Roundabout, 2 Secondary, 3 Local, 4 Service, 5 Alley. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Linear")
	int32 MinTier = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Linear")
	int32 MaxTier = 99;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area")
	float DensityPer100SqM = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area")
	FName RequiredAreaUse;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area")
	float MinBuildingHeight = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area")
	float AreaProximityUnits = 1200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area")
	float ZOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Junction")
	int32 MinJunctionRoads = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Junction")
	int32 MaxJunctionRoads = 99;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variation")
	EVoidYawMode YawMode = EVoidYawMode::AlongTraffic;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variation")
	float YawJitterDegrees = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variation")
	float ScaleMin = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variation")
	float ScaleMax = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variation")
	int32 ClusterMin = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variation")
	int32 ClusterMax = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variation")
	float ClusterRadius = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Density")
	float Probability = 1.0f;
	/** Budget priority: when the instance budget is exceeded, lower values are dropped first. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Density")
	float PriorityBase = 0.5f;
	/** -1 = clutter (fades as the district gets more polished) .. +1 = upkeep (grows with polish). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Density", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float PolishResponse = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Density")
	int32 MaxPerDistrict = 0;

	VoidPlan::FRule ToPlanRule(FName RuleId) const;
};

/** FVoidDistrictEnvProfileRow -- per-district dressing character. Row name = DistrictId (e.g. "white_zones"). */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERGENERATORS_API FVoidDistrictEnvProfileRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 0 = neglected, 1 = pristine. Drives clutter vs upkeep rules via each rule's PolishResponse. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "District", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Polish = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "District", meta = (ClampMin = "0.0"))
	float DensityMultiplier = 1.0f;

	/** Per-category multiplier, e.g. Tree = 0.0 to forbid trees in an enclosed district. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "District")
	TMap<FName, float> CategoryMultipliers;

	VoidPlan::FDistrictParams ToPlanParams() const;
};

/** FVoidBuildingUseTokenRow -- maps free-form BuildingType text to a placement Kind/Use. First matching row wins (row order). */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERGENERATORS_API FVoidBuildingUseTokenRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Case-insensitive substring searched in FVoidBuildingSpec::BuildingType. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Use Token")
	FString Token;

	/** "Building" (default), "Park" or "Plaza". Park/Plaza footprints are open areas, not obstacles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Use Token")
	FName Kind = FName(TEXT("Building"));

	/** e.g. Commercial, Residential, Civic, Industrial, Construction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Use Token")
	FName Use;
};

/** Metadata kept per instanced bucket so a future asset-replacement pass knows exactly what it is looking at. */
USTRUCT(BlueprintType)
struct VOIDWORLDBUILDERGENERATORS_API FVoidDressingBucketInfo
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Dressing")
	FName Category;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Dressing")
	FName SlotId;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Dressing")
	int32 PartIndex = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Dressing")
	bool bPlaceholder = true;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Dressing")
	FName ReplacementTag;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Dressing")
	TObjectPtr<UInstancedStaticMeshComponent> Component;
	/** Stable id of instance i (parallel to the component's instance index at generation time). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VOID Dressing")
	TArray<int64> InstanceIds;
};
