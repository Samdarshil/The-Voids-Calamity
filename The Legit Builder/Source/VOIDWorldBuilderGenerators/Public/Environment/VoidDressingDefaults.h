// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Environment/VoidEnvironmentTypes.h"

/** Engine BasicShapes used to compose placeholders. These ship with the engine (/Engine/BasicShapes); no project asset is assumed. */
enum class EVoidPlaceholderShape : uint8 { Cube, Cylinder, Sphere, Cone };

struct VOIDWORLDBUILDERGENERATORS_API FVoidPlaceholderPart
{
	EVoidPlaceholderShape Shape = EVoidPlaceholderShape::Cube;
	FVector Offset = FVector::ZeroVector;   // local, in uu, relative to the instance pivot (ground contact point)
	FRotator Rotation = FRotator::ZeroRotator;
	FVector Scale = FVector::OneVector;     // BasicShapes are 100uu, so Scale 1 == 1 m
};

struct VOIDWORLDBUILDERGENERATORS_API FVoidPlaceholderDef
{
	FName Category;
	TArray<FVoidPlaceholderPart> Parts;
	float CullStartUnits = 0.0f;
	float CullEndUnits = 0.0f;     // 0 = no culling
	bool bCastShadow = true;
};

/**
 * FVoidDressingDefaults
 *
 * Everything the generators need to work with ZERO authored data: built-in
 * placeholders per category, built-in placement rules, built-in district
 * profiles and building-use tokens. All of it is engineering tuning, not
 * approved design; each is overridden by the matching DataTable in
 * UVoidEnvironmentSettings.
 */
class VOIDWORLDBUILDERGENERATORS_API FVoidDressingDefaults
{
public:
	static const FVoidPlaceholderDef* FindPlaceholder(FName Category);
	static const TArray<FVoidPlaceholderDef>& GetPlaceholders();

	/** Named rows (RuleId -> row). */
	static const TArray<TPair<FName, FVoidPlacementRuleRow>>& GetBuiltInRules();

	/** Meridian polish-gradient placeholders. Unknown district => neutral profile (Polish 0.5). */
	static FVoidDistrictEnvProfileRow GetBuiltInDistrictProfile(FName DistrictId);

	static const TArray<FVoidBuildingUseTokenRow>& GetBuiltInUseTokens();
};
