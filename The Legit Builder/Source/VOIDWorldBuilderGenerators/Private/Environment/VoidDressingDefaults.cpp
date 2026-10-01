// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Environment/VoidDressingDefaults.h"

using namespace VoidDressing;

// ---------------------------------------------------------------------------
// Row -> plan conversion
// ---------------------------------------------------------------------------

VoidPlan::FRule FVoidPlacementRuleRow::ToPlanRule(FName RuleId) const
{
	VoidPlan::FRule R;
	R.RuleId = RuleId;
	R.Domain = Domain;
	R.Category = Category;
	R.SlotFilter = SlotFilter;
	R.Context = Context;
	R.SpacingUnits = SpacingUnits;
	R.StartMargin = StartMargin;
	R.LateralMode = static_cast<VoidPlan::ELateralMode>(static_cast<uint8>(LateralMode));
	R.LateralFraction = LateralFraction;
	R.LateralInsetUnits = LateralInsetUnits;
	R.LateralJitterUnits = LateralJitterUnits;
	R.AlongJitterFraction = AlongJitterFraction;
	R.bBothSides = bBothSides;
	R.bAlternateSides = bAlternateSides;
	R.JunctionClearance = JunctionClearance;
	R.bRequireSidewalk = bRequireSidewalk;
	R.bRequireMedian = bRequireMedian;
	R.bSkipOnBridge = bSkipOnBridge;
	R.MinTier = MinTier;
	R.MaxTier = MaxTier;
	R.DensityPer100SqM = DensityPer100SqM;
	R.RequiredAreaUse = RequiredAreaUse;
	R.MinBuildingHeight = MinBuildingHeight;
	R.AreaProximityUnits = AreaProximityUnits;
	R.ZOffset = ZOffset;
	R.MinJunctionRoads = MinJunctionRoads;
	R.MaxJunctionRoads = MaxJunctionRoads;
	R.YawMode = static_cast<VoidPlan::EYawMode>(static_cast<uint8>(YawMode));
	R.YawJitterDegrees = YawJitterDegrees;
	R.ScaleMin = ScaleMin;
	R.ScaleMax = FMath::Max(ScaleMin, ScaleMax);
	R.Probability = Probability;
	R.PriorityBase = PriorityBase;
	R.PolishResponse = PolishResponse;
	R.ClusterMin = ClusterMin;
	R.ClusterMax = ClusterMax;
	R.ClusterRadius = ClusterRadius;
	R.MaxPerDistrict = MaxPerDistrict;
	return R;
}

VoidPlan::FDistrictParams FVoidDistrictEnvProfileRow::ToPlanParams() const
{
	VoidPlan::FDistrictParams P;
	P.Polish = Polish;
	P.DensityMultiplier = DensityMultiplier;
	P.CategoryMultiplier = CategoryMultipliers;
	return P;
}

// ---------------------------------------------------------------------------
// Placeholders: BasicShapes composites, ground-contact pivot, sized in real-world uu (cm).
// ---------------------------------------------------------------------------

namespace
{
	FVoidPlaceholderPart Part(EVoidPlaceholderShape Shape, FVector Scale, FVector Offset, FRotator Rot = FRotator::ZeroRotator)
	{
		FVoidPlaceholderPart P;
		P.Shape = Shape;
		P.Scale = Scale;
		P.Offset = Offset;
		P.Rotation = Rot;
		return P;
	}

	FVoidPlaceholderDef Def(const TCHAR* Category, std::initializer_list<FVoidPlaceholderPart> Parts, float CullEnd, bool bShadow = true)
	{
		FVoidPlaceholderDef D;
		D.Category = FName(Category);
		for (const FVoidPlaceholderPart& P : Parts) { D.Parts.Add(P); }
		D.CullEndUnits = CullEnd;
		D.bCastShadow = bShadow;
		return D;
	}

	TArray<FVoidPlaceholderDef> BuildPlaceholders()
	{
		using S = EVoidPlaceholderShape;
		TArray<FVoidPlaceholderDef> D;
		// Vegetation
		D.Add(Def(Category::Tree,   { Part(S::Cylinder, FVector(0.30, 0.30, 3.0), FVector(0, 0, 150)), Part(S::Sphere, FVector(3.0, 3.0, 3.0), FVector(0, 0, 380)) }, 40000.f));
		D.Add(Def(Category::Bush,   { Part(S::Sphere, FVector(1.2, 1.2, 0.9), FVector(0, 0, 45)) }, 12000.f));
		D.Add(Def(Category::Grass,  { Part(S::Cone, FVector(0.4, 0.4, 0.5), FVector(0, 0, 25)) }, 6000.f, false));
		D.Add(Def(Category::Rock,   { Part(S::Sphere, FVector(1.0, 0.8, 0.6), FVector(0, 0, 25)) }, 12000.f));
		// Street furniture
		D.Add(Def(Category::Planter,{ Part(S::Cube, FVector(0.9, 0.9, 0.5), FVector(0, 0, 25)), Part(S::Sphere, FVector(0.7, 0.7, 0.6), FVector(0, 0, 85)) }, 12000.f));
		D.Add(Def(Category::Bench,  { Part(S::Cube, FVector(1.6, 0.5, 0.08), FVector(0, 0, 45)), Part(S::Cube, FVector(1.6, 0.08, 0.5), FVector(-22, 0, 70)) }, 10000.f));
		D.Add(Def(Category::Streetlight, { Part(S::Cylinder, FVector(0.15, 0.15, 6.0), FVector(0, 0, 300)), Part(S::Cube, FVector(0.9, 0.25, 0.12), FVector(35, 0, 600)) }, 25000.f));
		D.Add(Def(Category::Pole,   { Part(S::Cylinder, FVector(0.20, 0.20, 8.0), FVector(0, 0, 400)), Part(S::Cube, FVector(0.08, 1.6, 0.08), FVector(0, 0, 720)) }, 25000.f));
		D.Add(Def(Category::TrafficLight, { Part(S::Cylinder, FVector(0.15, 0.15, 5.0), FVector(0, 0, 250)), Part(S::Cube, FVector(0.3, 0.3, 0.9), FVector(0, 0, 480)) }, 25000.f));
		D.Add(Def(Category::Sign,   { Part(S::Cylinder, FVector(0.06, 0.06, 2.5), FVector(0, 0, 125)), Part(S::Cube, FVector(0.05, 0.6, 0.6), FVector(0, 0, 230)) }, 12000.f));
		D.Add(Def(Category::Barrier,{ Part(S::Cube, FVector(2.0, 0.4, 0.9), FVector(0, 0, 45)) }, 12000.f));
		D.Add(Def(Category::Bin,    { Part(S::Cylinder, FVector(0.5, 0.5, 0.9), FVector(0, 0, 45)) }, 8000.f));
		D.Add(Def(Category::Trash,  { Part(S::Cube, FVector(0.4, 0.3, 0.2), FVector(0, 0, 10)) }, 5000.f, false));
		D.Add(Def(Category::SidewalkDetail, { Part(S::Cylinder, FVector(0.7, 0.7, 0.02), FVector(0, 0, 1)) }, 8000.f, false));
		D.Add(Def(Category::StreetProp, { Part(S::Cylinder, FVector(0.3, 0.3, 0.8), FVector(0, 0, 40)), Part(S::Sphere, FVector(0.3, 0.3, 0.3), FVector(0, 0, 90)) }, 8000.f));
		// Props
		D.Add(Def(Category::Vehicle,{ Part(S::Cube, FVector(4.5, 1.9, 0.8), FVector(0, 0, 60)), Part(S::Cube, FVector(2.4, 1.7, 0.6), FVector(-30, 0, 135)) }, 20000.f));
		D.Add(Def(Category::Container, { Part(S::Cube, FVector(6.0, 2.4, 2.6), FVector(0, 0, 130)) }, 20000.f));
		D.Add(Def(Category::Crate,  { Part(S::Cube, FVector(1.0, 1.0, 1.0), FVector(0, 0, 50)) }, 8000.f));
		D.Add(Def(Category::Pipe,   { Part(S::Cylinder, FVector(0.4, 0.4, 3.0), FVector(0, 0, 20), FRotator(0, 0, 90)) }, 8000.f));
		D.Add(Def(Category::Billboard, { Part(S::Cylinder, FVector(0.3, 0.3, 4.0), FVector(0, 0, 200)), Part(S::Cube, FVector(0.3, 6.0, 3.0), FVector(0, 0, 550)) }, 0.0f)); // no cull: skyline element
		D.Add(Def(Category::ConstructionAsset, { Part(S::Cone, FVector(0.4, 0.4, 0.7), FVector(0, 0, 35)) }, 10000.f));
		return D;
	}

	FVoidPlacementRuleRow Row(const TCHAR* Domain, const TCHAR* Category, const TCHAR* Context)
	{
		FVoidPlacementRuleRow R;
		R.Domain = FName(Domain);
		R.Category = FName(Category);
		R.Context = FName(Context);
		return R;
	}

	TArray<TPair<FName, FVoidPlacementRuleRow>> BuildRules()
	{
		TArray<TPair<FName, FVoidPlacementRuleRow>> Rules;
		auto Add = [&Rules](const TCHAR* Id, FVoidPlacementRuleRow R) { Rules.Add(TPair<FName, FVoidPlacementRuleRow>(FName(Id), R)); };
		const TCHAR* Env = Domain::Environment;
		const TCHAR* Prp = Domain::Props;

		// ---- Environment: sidewalk / roadside ----
		{ FVoidPlacementRuleRow R = Row(Env, Category::Tree, TEXT("Sidewalk")); R.SpacingUnits = 1200; R.LateralFraction = 0.5f; R.MinTier = 1; R.MaxTier = 3; R.Probability = 0.85f; R.JunctionClearance = 300; R.YawMode = EVoidYawMode::Random; R.ScaleMin = 0.85f; R.ScaleMax = 1.25f; R.PolishResponse = 0.4f; R.PriorityBase = 0.6f; R.LateralJitterUnits = 10; Add(TEXT("Tree_Sidewalk"), R); }
		{ FVoidPlacementRuleRow R = Row(Env, Category::Bush, TEXT("Roadside")); R.bRequireMedian = true; R.LateralMode = EVoidLateralMode::Median; R.SpacingUnits = 900; R.MinTier = 0; R.MaxTier = 2; R.Probability = 0.8f; R.YawMode = EVoidYawMode::Random; R.ScaleMin = 0.8f; R.ScaleMax = 1.2f; R.PolishResponse = 0.5f; R.JunctionClearance = 300; R.PriorityBase = 0.4f; Add(TEXT("Bush_Median"), R); }
		{ FVoidPlacementRuleRow R = Row(Env, Category::Planter, TEXT("Sidewalk")); R.SpacingUnits = 2500; R.LateralFraction = 0.85f; R.MinTier = 2; R.MaxTier = 3; R.bSkipOnBridge = true; R.Probability = 0.6f; R.JunctionClearance = 400; R.YawMode = EVoidYawMode::FaceRoad; R.PolishResponse = 0.3f; R.PriorityBase = 0.5f; Add(TEXT("Planter_Sidewalk"), R); }
		{ FVoidPlacementRuleRow R = Row(Env, Category::Bench, TEXT("Sidewalk")); R.SpacingUnits = 3500; R.LateralFraction = 0.8f; R.MinTier = 2; R.MaxTier = 3; R.Probability = 0.5f; R.JunctionClearance = 400; R.YawMode = EVoidYawMode::FaceRoad; R.YawJitterDegrees = 3; R.PolishResponse = 0.2f; R.PriorityBase = 0.55f; Add(TEXT("Bench_Sidewalk"), R); }
		{ FVoidPlacementRuleRow R = Row(Env, Category::Streetlight, TEXT("Roadside")); R.SpacingUnits = 2400; R.LateralFraction = 0.1f; R.MinTier = 0; R.MaxTier = 3; R.bAlternateSides = true; R.JunctionClearance = 250; R.YawMode = EVoidYawMode::FaceRoad; R.PriorityBase = 0.9f; R.AlongJitterFraction = 0.05f; Add(TEXT("Streetlight_Roadside"), R); }
		{ FVoidPlacementRuleRow R = Row(Env, Category::Streetlight, TEXT("Roadside")); R.SpacingUnits = 3500; R.LateralFraction = 0.1f; R.MinTier = 4; R.MaxTier = 5; R.bBothSides = false; R.Probability = 0.5f; R.JunctionClearance = 250; R.YawMode = EVoidYawMode::FaceRoad; R.PriorityBase = 0.6f; Add(TEXT("Streetlight_Service"), R); }
		{ FVoidPlacementRuleRow R = Row(Env, Category::Pole, TEXT("Roadside")); R.SpacingUnits = 3000; R.LateralFraction = 0.05f; R.MinTier = 4; R.MaxTier = 5; R.bBothSides = false; R.Probability = 0.6f; R.JunctionClearance = 300; R.YawMode = EVoidYawMode::FaceRoad; R.PolishResponse = -0.5f; R.PriorityBase = 0.35f; Add(TEXT("Pole_Service"), R); }
		{ FVoidPlacementRuleRow R = Row(Env, Category::SidewalkDetail, TEXT("Roadside")); R.LateralMode = EVoidLateralMode::Median; R.SpacingUnits = 3000; R.MinTier = 1; R.MaxTier = 4; R.Probability = 0.4f; R.JunctionClearance = 500; R.YawMode = EVoidYawMode::Random; R.PolishResponse = -0.3f; R.bSkipOnBridge = true; R.PriorityBase = 0.2f; Add(TEXT("Manhole_Roadway"), R); }
		{ FVoidPlacementRuleRow R = Row(Env, Category::StreetProp, TEXT("Sidewalk")); R.SpacingUnits = 3000; R.LateralFraction = 0.15f; R.MinTier = 2; R.MaxTier = 3; R.Probability = 0.3f; R.JunctionClearance = 500; R.YawMode = EVoidYawMode::FaceRoad; R.PriorityBase = 0.3f; Add(TEXT("StreetProp_Sidewalk"), R); }
		// ---- Environment: commercial frontage ----
		{ FVoidPlacementRuleRow R = Row(Env, Category::Bin, TEXT("Commercial")); R.SpacingUnits = 2000; R.LateralFraction = 0.9f; R.MinTier = 2; R.MaxTier = 3; R.bRequireSidewalk = true; R.RequiredAreaUse = FName(TEXT("Commercial")); R.AreaProximityUnits = 900; R.Probability = 0.5f; R.JunctionClearance = 300; R.YawMode = EVoidYawMode::FaceRoad; R.PolishResponse = -0.2f; R.PriorityBase = 0.4f; Add(TEXT("Bin_Commercial"), R); }
		// ---- Environment: junction infrastructure ----
		{ FVoidPlacementRuleRow R = Row(Env, Category::TrafficLight, TEXT("Junction")); R.bBothSides = false; R.MinJunctionRoads = 3; R.LateralFraction = 0.1f; R.YawMode = EVoidYawMode::AlongTraffic; R.PriorityBase = 1.0f; Add(TEXT("TrafficLight_Junction"), R); }
		{ FVoidPlacementRuleRow R = Row(Env, Category::Sign, TEXT("Junction")); R.bBothSides = false; R.MinJunctionRoads = 3; R.LateralFraction = 0.6f; R.YawMode = EVoidYawMode::AlongTraffic; R.Probability = 0.8f; R.PriorityBase = 0.8f; Add(TEXT("Sign_Junction"), R); }
		{ FVoidPlacementRuleRow R = Row(Env, Category::Barrier, TEXT("Junction")); R.MinJunctionRoads = 1; R.MaxJunctionRoads = 1; R.bBothSides = false; R.YawMode = EVoidYawMode::AlongTraffic; R.PriorityBase = 0.7f; Add(TEXT("Barrier_DeadEnd"), R); }
		// ---- Environment: parks and plazas ----
		{ FVoidPlacementRuleRow R = Row(Env, Category::Tree, TEXT("Park")); R.DensityPer100SqM = 0.5f; R.ScaleMin = 0.8f; R.ScaleMax = 1.4f; R.YawMode = EVoidYawMode::Random; R.PolishResponse = 0.2f; R.PriorityBase = 0.55f; Add(TEXT("Tree_Park"), R); }
		{ FVoidPlacementRuleRow R = Row(Env, Category::Bush, TEXT("Park")); R.DensityPer100SqM = 0.6f; R.ScaleMin = 0.7f; R.ScaleMax = 1.3f; R.YawMode = EVoidYawMode::Random; R.PriorityBase = 0.4f; Add(TEXT("Bush_Park"), R); }
		{ FVoidPlacementRuleRow R = Row(Env, Category::Grass, TEXT("Park")); R.DensityPer100SqM = 6.0f; R.ScaleMin = 0.8f; R.ScaleMax = 1.3f; R.YawMode = EVoidYawMode::Random; R.PolishResponse = 0.3f; R.PriorityBase = 0.15f; Add(TEXT("Grass_Park"), R); }
		{ FVoidPlacementRuleRow R = Row(Env, Category::Rock, TEXT("Park")); R.DensityPer100SqM = 0.15f; R.ScaleMin = 0.6f; R.ScaleMax = 1.6f; R.YawMode = EVoidYawMode::Random; R.PriorityBase = 0.3f; Add(TEXT("Rock_Park"), R); }
		{ FVoidPlacementRuleRow R = Row(Env, Category::Bench, TEXT("Park")); R.DensityPer100SqM = 0.08f; R.YawMode = EVoidYawMode::Random; R.PriorityBase = 0.5f; Add(TEXT("Bench_Park"), R); }
		{ FVoidPlacementRuleRow R = Row(Env, Category::Planter, TEXT("Plaza")); R.DensityPer100SqM = 0.15f; R.YawMode = EVoidYawMode::Random; R.PolishResponse = 0.3f; R.PriorityBase = 0.7f; Add(TEXT("Planter_Plaza"), R); }
		{ FVoidPlacementRuleRow R = Row(Env, Category::Bench, TEXT("Plaza")); R.DensityPer100SqM = 0.12f; R.YawMode = EVoidYawMode::Random; R.PriorityBase = 0.7f; Add(TEXT("Bench_Plaza"), R); }
		{ FVoidPlacementRuleRow R = Row(Env, Category::Tree, TEXT("Plaza")); R.DensityPer100SqM = 0.1f; R.ScaleMin = 0.9f; R.ScaleMax = 1.3f; R.YawMode = EVoidYawMode::Random; R.PolishResponse = 0.4f; R.PriorityBase = 0.7f; Add(TEXT("Tree_Plaza"), R); }
		{ FVoidPlacementRuleRow R = Row(Env, Category::Streetlight, TEXT("Plaza")); R.DensityPer100SqM = 0.05f; R.YawMode = EVoidYawMode::Random; R.PriorityBase = 0.8f; Add(TEXT("Streetlight_Plaza"), R); }

		// ---- Props ----
		{ FVoidPlacementRuleRow R = Row(Prp, Category::Vehicle, TEXT("Curbside")); R.LateralMode = EVoidLateralMode::RoadEdge; R.LateralInsetUnits = 160; R.SpacingUnits = 1400; R.MinTier = 1; R.MaxTier = 4; R.Probability = 0.22f; R.JunctionClearance = 800; R.bSkipOnBridge = true; R.YawMode = EVoidYawMode::AlongTraffic; R.YawJitterDegrees = 2; R.PriorityBase = 0.7f; Add(TEXT("Vehicle_Curbside"), R); }
		{ FVoidPlacementRuleRow R = Row(Prp, Category::Container, TEXT("Alley")); R.SpacingUnits = 2600; R.LateralFraction = 0.5f; R.MinTier = 4; R.MaxTier = 5; R.Probability = 0.35f; R.JunctionClearance = 500; R.YawMode = EVoidYawMode::AlongTraffic; R.YawJitterDegrees = 6; R.PolishResponse = -0.7f; R.PriorityBase = 0.4f; Add(TEXT("Container_Alley"), R); }
		{ FVoidPlacementRuleRow R = Row(Prp, Category::Crate, TEXT("Alley")); R.SpacingUnits = 1200; R.LateralFraction = 0.5f; R.MinTier = 4; R.MaxTier = 5; R.Probability = 0.5f; R.ClusterMin = 2; R.ClusterMax = 5; R.ClusterRadius = 120; R.JunctionClearance = 400; R.YawMode = EVoidYawMode::Random; R.ScaleMin = 0.7f; R.ScaleMax = 1.2f; R.PolishResponse = -0.8f; R.PriorityBase = 0.3f; Add(TEXT("Crate_Alley"), R); }
		{ FVoidPlacementRuleRow R = Row(Prp, Category::Pipe, TEXT("Alley")); R.SpacingUnits = 2000; R.LateralFraction = 0.2f; R.MinTier = 4; R.MaxTier = 5; R.Probability = 0.3f; R.JunctionClearance = 400; R.YawMode = EVoidYawMode::AlongTraffic; R.PolishResponse = -0.6f; R.PriorityBase = 0.25f; Add(TEXT("Pipe_Alley"), R); }
		{ FVoidPlacementRuleRow R = Row(Prp, Category::Trash, TEXT("Roadside")); R.SpacingUnits = 500; R.LateralFraction = 0.3f; R.MinTier = 2; R.MaxTier = 5; R.Probability = 0.25f; R.ClusterMin = 1; R.ClusterMax = 3; R.ClusterRadius = 60; R.JunctionClearance = 200; R.YawMode = EVoidYawMode::Random; R.PolishResponse = -1.0f; R.PriorityBase = 0.1f; Add(TEXT("Trash_Roadside"), R); }
		{ FVoidPlacementRuleRow R = Row(Prp, Category::Billboard, TEXT("Rooftop")); R.RequiredAreaUse = FName(TEXT("Commercial")); R.MinBuildingHeight = 1500; R.Probability = 0.6f; R.YawMode = EVoidYawMode::Random; R.PriorityBase = 0.85f; Add(TEXT("Billboard_Rooftop"), R); }
		{ FVoidPlacementRuleRow R = Row(Prp, Category::Sign, TEXT("Facade")); R.RequiredAreaUse = FName(TEXT("Commercial")); R.ZOffset = 350; R.AreaProximityUnits = 1500; R.Probability = 0.5f; R.PriorityBase = 0.5f; Add(TEXT("Sign_Facade"), R); }
		{ FVoidPlacementRuleRow R = Row(Prp, Category::ConstructionAsset, TEXT("Site")); R.RequiredAreaUse = FName(TEXT("Construction")); R.DensityPer100SqM = 0.8f; R.YawMode = EVoidYawMode::Random; R.PriorityBase = 0.5f; Add(TEXT("Construction_Site"), R); }
		{ FVoidPlacementRuleRow R = Row(Prp, Category::Barrier, TEXT("Site")); R.RequiredAreaUse = FName(TEXT("Construction")); R.DensityPer100SqM = 0.4f; R.YawMode = EVoidYawMode::Random; R.PriorityBase = 0.5f; Add(TEXT("Barrier_Site"), R); }
		{ FVoidPlacementRuleRow R = Row(Prp, Category::Crate, TEXT("Site")); R.RequiredAreaUse = FName(TEXT("Construction")); R.DensityPer100SqM = 0.5f; R.ClusterMin = 2; R.ClusterMax = 4; R.ClusterRadius = 150; R.YawMode = EVoidYawMode::Random; R.PriorityBase = 0.4f; Add(TEXT("Crate_Site"), R); }
		return Rules;
	}

	TArray<FVoidBuildingUseTokenRow> BuildUseTokens()
	{
		TArray<FVoidBuildingUseTokenRow> T;
		auto Add = [&T](const TCHAR* Token, const TCHAR* Kind, const TCHAR* Use)
		{
			FVoidBuildingUseTokenRow R; R.Token = Token; R.Kind = FName(Kind); R.Use = FName(Use); T.Add(R);
		};
		// Order matters: first match wins. Open areas first, then use tags.
		Add(TEXT("Park"), TEXT("Park"), TEXT("Park"));
		Add(TEXT("Green"), TEXT("Park"), TEXT("Park"));
		Add(TEXT("Garden"), TEXT("Park"), TEXT("Park"));
		Add(TEXT("Plaza"), TEXT("Plaza"), TEXT("Plaza"));
		Add(TEXT("Square"), TEXT("Plaza"), TEXT("Plaza"));
		Add(TEXT("Courtyard"), TEXT("Plaza"), TEXT("Plaza"));
		Add(TEXT("Construction"), TEXT("Building"), TEXT("Construction"));
		Add(TEXT("Scaffold"), TEXT("Building"), TEXT("Construction"));
		Add(TEXT("Commercial"), TEXT("Building"), TEXT("Commercial"));
		Add(TEXT("Retail"), TEXT("Building"), TEXT("Commercial"));
		Add(TEXT("Shop"), TEXT("Building"), TEXT("Commercial"));
		Add(TEXT("Market"), TEXT("Building"), TEXT("Commercial"));
		Add(TEXT("Store"), TEXT("Building"), TEXT("Commercial"));
		Add(TEXT("Mall"), TEXT("Building"), TEXT("Commercial"));
		Add(TEXT("Residential"), TEXT("Building"), TEXT("Residential"));
		Add(TEXT("Housing"), TEXT("Building"), TEXT("Residential"));
		Add(TEXT("Apartment"), TEXT("Building"), TEXT("Residential"));
		Add(TEXT("Industrial"), TEXT("Building"), TEXT("Industrial"));
		Add(TEXT("Warehouse"), TEXT("Building"), TEXT("Industrial"));
		Add(TEXT("Factory"), TEXT("Building"), TEXT("Industrial"));
		Add(TEXT("Civic"), TEXT("Building"), TEXT("Civic"));
		Add(TEXT("Trust"), TEXT("Building"), TEXT("Civic"));
		return T;
	}
}

const TArray<FVoidPlaceholderDef>& FVoidDressingDefaults::GetPlaceholders()
{
	static const TArray<FVoidPlaceholderDef> Defs = BuildPlaceholders();
	return Defs;
}

const FVoidPlaceholderDef* FVoidDressingDefaults::FindPlaceholder(FName InCategory)
{
	for (const FVoidPlaceholderDef& D : GetPlaceholders())
	{
		if (D.Category == InCategory) { return &D; }
	}
	return nullptr;
}

const TArray<TPair<FName, FVoidPlacementRuleRow>>& FVoidDressingDefaults::GetBuiltInRules()
{
	static const TArray<TPair<FName, FVoidPlacementRuleRow>> Rules = BuildRules();
	return Rules;
}

const TArray<FVoidBuildingUseTokenRow>& FVoidDressingDefaults::GetBuiltInUseTokens()
{
	static const TArray<FVoidBuildingUseTokenRow> Tokens = BuildUseTokens();
	return Tokens;
}

FVoidDistrictEnvProfileRow FVoidDressingDefaults::GetBuiltInDistrictProfile(FName DistrictId)
{
	// ENGINEERING PLACEHOLDERS, not approved design values. The only Meridian-sourced content is the ORDERING of
	// Meridian_Master_Plan.md's polish gradient: Undercroft (honest imperfection) -> Metro Archives (tired neglect)
	// -> White Zones (clean institutional calm) -> Olympus Spire (most curated). Sector 0 is described as the one
	// dynamic exception, so it stays neutral. Numbers are tuning knobs; override via the DistrictProfileTable.
	FVoidDistrictEnvProfileRow P;
	const FString Id = DistrictId.ToString();
	if      (Id == TEXT("olympus_spire"))  { P.Polish = 0.95f; }
	else if (Id == TEXT("white_zones"))    { P.Polish = 0.85f; }
	else if (Id == TEXT("metro_archives")) { P.Polish = 0.35f; }
	else if (Id == TEXT("undercroft"))     { P.Polish = 0.15f; }
	else                                   { P.Polish = 0.5f; }
	return P;
}
