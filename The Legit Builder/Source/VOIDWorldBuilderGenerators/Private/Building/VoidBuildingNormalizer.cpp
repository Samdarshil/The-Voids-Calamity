// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Building/VoidBuildingNormalizer.h"
#include "Building/VoidBuildingGeometry.h"

namespace
{
	struct FKeyword
	{
		const TCHAR* Text;
		EVoidBuildingCategory Category;
	};

	// Order matters: first hit wins. Specific/compound tags come before generic ones.
	// This is a *classification vocabulary*, not a schema: the package has no category
	// field. Extend it per project through UVoidBuildingGenerationSettings::KeywordOverrides.
	const FKeyword GBuiltInKeywords[] =
	{
		{ TEXT("landmark"),   EVoidBuildingCategory::Landmark },
		{ TEXT("mixed"),      EVoidBuildingCategory::MixedUse },
		{ TEXT("hospital"),   EVoidBuildingCategory::Medical },
		{ TEXT("clinic"),     EVoidBuildingCategory::Medical },
		{ TEXT("medical"),    EVoidBuildingCategory::Medical },
		{ TEXT("government"), EVoidBuildingCategory::Government },
		{ TEXT("ministry"),   EVoidBuildingCategory::Government },
		{ TEXT("council"),    EVoidBuildingCategory::Government },
		{ TEXT("civic"),      EVoidBuildingCategory::Civic },
		{ TEXT("institution"),EVoidBuildingCategory::Institutional },
		{ TEXT("school"),     EVoidBuildingCategory::Institutional },
		{ TEXT("universit"),  EVoidBuildingCategory::Institutional },
		{ TEXT("library"),    EVoidBuildingCategory::Institutional },
		{ TEXT("industri"),   EVoidBuildingCategory::Industrial },
		{ TEXT("factory"),    EVoidBuildingCategory::Industrial },
		{ TEXT("warehouse"),  EVoidBuildingCategory::Industrial },
		{ TEXT("office"),     EVoidBuildingCategory::Office },
		{ TEXT("corporate"),  EVoidBuildingCategory::Office },
		{ TEXT("commerc"),    EVoidBuildingCategory::Commercial },
		{ TEXT("retail"),     EVoidBuildingCategory::Commercial },
		{ TEXT("shop"),       EVoidBuildingCategory::Commercial },
		{ TEXT("market"),     EVoidBuildingCategory::Commercial },
		{ TEXT("residen"),    EVoidBuildingCategory::Residential },
		{ TEXT("apartment"),  EVoidBuildingCategory::Residential },
		{ TEXT("housing"),    EVoidBuildingCategory::Residential },
	};
}

EVoidBuildingCategory FVoidBuildingNormalizer::Classify(const FString& TypeTag, const TMap<FString, EVoidBuildingCategory>& KeywordOverrides)
{
	if (TypeTag.IsEmpty())
	{
		return EVoidBuildingCategory::Unknown;
	}

	const FString Lower = TypeTag.ToLower();

	// Overrides: longest matching key wins; ties broken by string order so the result never
	// depends on TMap iteration order.
	bool bHaveOverride = false;
	FString BestKey;
	EVoidBuildingCategory BestCategory = EVoidBuildingCategory::Unknown;
	for (const TPair<FString, EVoidBuildingCategory>& Pair : KeywordOverrides)
	{
		const FString Key = Pair.Key.ToLower();
		if (Key.IsEmpty() || !Lower.Contains(Key))
		{
			continue;
		}
		if (!bHaveOverride || Key.Len() > BestKey.Len() || (Key.Len() == BestKey.Len() && Key < BestKey))
		{
			bHaveOverride = true;
			BestKey = Key;
			BestCategory = Pair.Value;
		}
	}
	if (bHaveOverride)
	{
		return BestCategory;
	}

	for (const FKeyword& Keyword : GBuiltInKeywords)
	{
		if (Lower.Contains(Keyword.Text))
		{
			return Keyword.Category;
		}
	}
	return EVoidBuildingCategory::Unknown;
}

float FVoidBuildingNormalizer::NominalFloorHeight(EVoidBuildingCategory Category)
{
	switch (Category)
	{
	case EVoidBuildingCategory::Residential:   return 300.0f;
	case EVoidBuildingCategory::Commercial:    return 400.0f;
	case EVoidBuildingCategory::Office:        return 400.0f;
	case EVoidBuildingCategory::Industrial:    return 600.0f;
	case EVoidBuildingCategory::Civic:         return 500.0f;
	case EVoidBuildingCategory::Institutional: return 400.0f;
	case EVoidBuildingCategory::Medical:       return 400.0f;
	case EVoidBuildingCategory::Government:    return 450.0f;
	case EVoidBuildingCategory::MixedUse:      return 350.0f;
	case EVoidBuildingCategory::Landmark:      return 450.0f;
	default:                                   return 350.0f;
	}
}

int32 FVoidBuildingNormalizer::MakeSeed(const FName& Id, int32 GlobalSeed)
{
	// Hash the id *string*: GetTypeHash(FName) is not stable across runs or platforms.
	const uint32 H = FVoidBuildingGeometry::StableHash(Id.ToString());
	return static_cast<int32>(H ^ (static_cast<uint32>(GlobalSeed) * 2654435761u));
}

const TCHAR* FVoidBuildingNormalizer::CategoryToString(EVoidBuildingCategory Category)
{
	switch (Category)
	{
	case EVoidBuildingCategory::Residential:   return TEXT("Residential");
	case EVoidBuildingCategory::Commercial:    return TEXT("Commercial");
	case EVoidBuildingCategory::Office:        return TEXT("Office");
	case EVoidBuildingCategory::Industrial:    return TEXT("Industrial");
	case EVoidBuildingCategory::Civic:         return TEXT("Civic");
	case EVoidBuildingCategory::Institutional: return TEXT("Institutional");
	case EVoidBuildingCategory::Medical:       return TEXT("Medical");
	case EVoidBuildingCategory::Government:    return TEXT("Government");
	case EVoidBuildingCategory::MixedUse:      return TEXT("MixedUse");
	case EVoidBuildingCategory::Landmark:      return TEXT("Landmark");
	default:                                   return TEXT("Unknown");
	}
}

FName FVoidBuildingNormalizer::MakeAssetCategory(const FVoidNormalizedBuilding& Building)
{
	if (Building.Category == EVoidBuildingCategory::Landmark)
	{
		return FName(TEXT("Building.Landmark"));
	}
	const TCHAR* HeightClass = Building.FloorCount <= 4 ? TEXT("LowRise")
		: Building.FloorCount <= 12 ? TEXT("MidRise")
		: Building.FloorCount <= 30 ? TEXT("HighRise")
		: TEXT("Skyscraper");
	return FName(*FString::Printf(TEXT("Building.%s.%s"), CategoryToString(Building.Category), HeightClass));
}

void FVoidBuildingNormalizer::Normalize(
	const FVoidDistrictData& District,
	const FVoidBuildingGenerationParams& Params,
	FVoidValidationReport& InOutReport,
	TArray<FVoidNormalizedBuilding>& OutBuildings)
{
	OutBuildings.Reset();
	OutBuildings.Reserve(District.Buildings.Num());

	TSet<FName> SeenIds;
	int32 NumUnknown = 0;

	for (int32 Index = 0; Index < District.Buildings.Num(); ++Index)
	{
		const FVoidBuildingSpec& Spec = District.Buildings[Index];
		const FString Field = FString::Printf(TEXT("district.buildings[%d]"), Index);

		if (!Spec.Id.IsValid())
		{
			InOutReport.AddError(FString::Printf(TEXT("%s has no id; skipped."), *Field), Field + TEXT(".id"), TEXT("VOID.Building.MissingId"));
			continue;
		}
		if (SeenIds.Contains(Spec.Id.Value))
		{
			InOutReport.AddWarning(FString::Printf(TEXT("Duplicate building id '%s'; later entry skipped."), *Spec.Id.Value.ToString()), Field + TEXT(".id"), TEXT("VOID.Building.DuplicateId"));
			continue;
		}
		SeenIds.Add(Spec.Id.Value);

		if (Spec.HeightUnits <= 0.0f)
		{
			InOutReport.AddError(FString::Printf(TEXT("Building '%s' has non-positive height; skipped."), *Spec.Id.Value.ToString()), Field + TEXT(".heightUnits"), TEXT("VOID.Building.InvalidHeight"));
			continue;
		}

		FVoidNormalizedBuilding B;
		B.Id = Spec.Id.Value;
		B.DistrictId = District.DistrictId.Value;
		B.TypeTag = Spec.BuildingType;

		B.SourceFootprint = Spec.FootprintCorners;
		FVoidBuildingGeometry::CleanPolygon(B.SourceFootprint);
		if (B.SourceFootprint.Num() < 3)
		{
			InOutReport.AddError(FString::Printf(TEXT("Building '%s' footprint has fewer than 3 distinct corners; skipped."), *B.Id.ToString()), Field + TEXT(".footprintCorners"), TEXT("VOID.Building.DegenerateFootprint"));
			continue;
		}

		if (!FVoidBuildingGeometry::IsSimple(B.SourceFootprint))
		{
			InOutReport.AddError(FString::Printf(TEXT("Building '%s' footprint is self-intersecting; skipped."), *B.Id.ToString()), Field + TEXT(".footprintCorners"), TEXT("VOID.Building.SelfIntersectingFootprint"));
			continue;
		}

		FVoidBuildingGeometry::EnsureCCW(B.SourceFootprint);
		B.Area = static_cast<float>(FVoidBuildingGeometry::SignedArea(B.SourceFootprint));
		if (B.Area < Params.MinFootprintAreaUnits2)
		{
			InOutReport.AddWarning(FString::Printf(TEXT("Building '%s' footprint area %.0f is below the minimum %.0f; skipped."), *B.Id.ToString(), B.Area, Params.MinFootprintAreaUnits2), Field + TEXT(".footprintCorners"), TEXT("VOID.Building.TinyFootprint"));
			continue;
		}

		B.Footprint = B.SourceFootprint;
		B.Category = Classify(B.TypeTag, Params.KeywordOverrides);
		B.bIsLandmark = (B.Category == EVoidBuildingCategory::Landmark);
		if (B.Category == EVoidBuildingCategory::Unknown)
		{
			++NumUnknown;
		}

		B.HeightUnits = Spec.HeightUnits;
		const float Nominal = NominalFloorHeight(B.Category);
		B.FloorCount = FMath::Clamp(FMath::RoundToInt(B.HeightUnits / Nominal), 1, 200);
		B.FloorHeight = B.HeightUnits / static_cast<float>(B.FloorCount);
		B.Seed = MakeSeed(B.Id, Params.GlobalSeed);

		B.Centroid = FVoidBuildingGeometry::Centroid(B.Footprint);
		FVoidBuildingGeometry::GetBounds(B.Footprint, B.BoundsMin, B.BoundsMax);

		OutBuildings.Add(MoveTemp(B));
	}

	OutBuildings.Sort([](const FVoidNormalizedBuilding& A, const FVoidNormalizedBuilding& B)
	{
		return A.Id.ToString().ToLower() < B.Id.ToString().ToLower();
	});

	if (NumUnknown > 0)
	{
		InOutReport.AddInfo(
			FString::Printf(TEXT("%d building(s) have a buildingType the classifier does not recognise; generated as plain blocks."), NumUnknown),
			TEXT("district.buildings"), TEXT("VOID.Building.UnknownCategory"),
			TEXT("Add a recognised keyword to the buildingType or extend KeywordOverrides in the Building Generation settings."));
	}
}
