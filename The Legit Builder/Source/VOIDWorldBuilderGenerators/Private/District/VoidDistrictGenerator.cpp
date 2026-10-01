// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "District/VoidDistrictGenerator.h"
#include "District/VoidDistrictActors.h"
#include "District/VoidDistrictGeometry.h"
#include "District/VoidDistrictGenerationSettings.h"
#include "District/VoidDistrictValidator.h"
#include "Meridian/VoidMeridianRegistryReader.h"
#include "Road/VoidRoadActor.h"
#include "Road/VoidRoadGenerator.h"
#include "Road/VoidRoadMeshBuilder.h"
#include "VoidGeneratorRegistry.h"
#include "VoidWorldBuilderLog.h"

#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "ScopedTransaction.h"
#include "ProceduralMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "VoidDistrictGenerator"

namespace VoidDistrictGeneratorPrivate
{
	static const FName BuildingGeneratorId(TEXT("Building"));

	static FString N(FName Name) { return Name.ToString(); }

	template <typename T>
	static T* SpawnGenerated(UWorld* World, const FString& Label, const FTransform& Transform)
	{
		FActorSpawnParameters Params;
		Params.Name = MakeUniqueObjectName(World->GetCurrentLevel(), T::StaticClass(), FName(*Label));
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		T* Actor = World->SpawnActor<T>(T::StaticClass(), Transform, Params);
		if (Actor)
		{
			Actor->Tags.AddUnique(VoidDistrictTags::Generated);
#if WITH_EDITOR
			Actor->SetActorLabel(Label);
#endif
		}
		return Actor;
	}

	static void SetFolder(AActor* Actor, const FString& Folder, bool bEnabled)
	{
#if WITH_EDITOR
		if (Actor && bEnabled)
		{
			Actor->SetFolderPath(FName(*Folder));
		}
#endif
	}

	static void Adopt(AActor* Child, AActor* Parent, FName DistrictTag, const TCHAR* Layer, const FString& Folder, bool bFolders)
	{
		if (!Child || !Parent) { return; }
		Child->Tags.AddUnique(VoidDistrictTags::Generated);
		if (!DistrictTag.IsNone()) { Child->Tags.AddUnique(DistrictTag); }
		Child->Tags.AddUnique(VoidDistrictTags::Layer(Layer));
		Child->AttachToActor(Parent, FAttachmentTransformRules::KeepWorldTransform);
		SetFolder(Child, Folder, bFolders);
	}

	static TSet<AActor*> SnapshotActors(UWorld* World)
	{
		TSet<AActor*> Set;
		for (TActorIterator<AActor> It(World); It; ++It) { Set.Add(*It); }
		return Set;
	}

	// ---- greybox mesh helpers (same winding convention as the Road mesh builder) ----

	static void AppendTri(FVoidRoadMeshSection& S, const FVector& A, const FVector& B, const FVector& C, const FLinearColor& Color)
	{
		FVector Normal = FVector::CrossProduct(B - A, C - A);
		Normal = Normal.IsNearlyZero() ? FVector::UpVector : Normal.GetSafeNormal();
		const FProcMeshTangent Tangent((B - A).GetSafeNormal(), false);
		const int32 Base = S.Vertices.Num();
		const FVector Verts[3] = { A, B, C };
		const FVector2D Uvs[3] = { FVector2D(0, 0), FVector2D(1, 0), FVector2D(0, 1) };
		for (int32 Index = 0; Index < 3; ++Index)
		{
			S.Vertices.Add(Verts[Index]);
			S.Normals.Add(Normal);
			S.UVs.Add(Uvs[Index]);
			S.VertexColors.Add(Color);
			S.Tangents.Add(Tangent);
			S.Triangles.Add(Base + Index);
		}
	}

	static FVector P3(const FVector2D& P, double Z) { return FVector(P.X, P.Y, Z); }

	static void AppendConvexPolygon(FVoidRoadMeshSection& S, const TArray<FVector2D>& Polygon, double Z, const FLinearColor& Color)
	{
		for (int32 Index = 1; Index + 1 < Polygon.Num(); ++Index)
		{
			AppendTri(S, P3(Polygon[0], Z), P3(Polygon[Index], Z), P3(Polygon[Index + 1], Z), Color);
		}
	}

	static void AppendAnnulus(FVoidRoadMeshSection& S, const TArray<FVector2D>& Outer, const FVector2D& Center, double HoleRadius, double Z, const FLinearColor& Color)
	{
		const int32 Num = Outer.Num();
		TArray<FVector2D> Inner;
		for (const FVector2D& Point : Outer)
		{
			const double DX = Point.X - Center.X, DY = Point.Y - Center.Y;
			const double Len = FMath::Max(FMath::Sqrt(DX * DX + DY * DY), 1e-6);
			Inner.Add(FVector2D(Center.X + DX / Len * HoleRadius, Center.Y + DY / Len * HoleRadius));
		}
		for (int32 Index = 0; Index < Num; ++Index)
		{
			const int32 Next = (Index + 1) % Num;
			AppendTri(S, P3(Outer[Index], Z), P3(Outer[Next], Z), P3(Inner[Next], Z), Color);
			AppendTri(S, P3(Outer[Index], Z), P3(Inner[Next], Z), P3(Inner[Index], Z), Color);
		}
	}

	static FLinearColor PublicSpaceColor(EVoidPublicSpaceType Type)
	{
		switch (Type)
		{
			case EVoidPublicSpaceType::Plaza:          return FLinearColor(0.80f, 0.72f, 0.50f);
			case EVoidPublicSpaceType::Park:           return FLinearColor(0.15f, 0.45f, 0.18f);
			case EVoidPublicSpaceType::Courtyard:      return FLinearColor(0.55f, 0.50f, 0.42f);
			case EVoidPublicSpaceType::CivicSpace:     return FLinearColor(0.55f, 0.62f, 0.78f);
			case EVoidPublicSpaceType::PedestrianZone: return FLinearColor(0.78f, 0.55f, 0.55f);
			default:                                   return FLinearColor(0.25f, 0.45f, 0.35f);
		}
	}

	static FLinearColor DistrictColor(FName Id)
	{
		const uint32 Hash = FVoidDistrictGeometry::StableHash(Id.ToString());
		return FLinearColor::MakeFromHSV8(static_cast<uint8>(Hash & 0xFF), 200, 255);
	}

	static void AppendOutline(FVoidRoadMeshSection& Out, const TArray<FVector2D>& Polygon, double Z, const FLinearColor& Color)
	{
		TArray<FVector> Points;
		for (const FVector2D& Point : Polygon) { Points.Add(P3(Point, Z)); }
		const FVoidRoadMeshSection Ribbon = FVoidRoadMeshBuilder::BuildRibbon(Points, 100.0f, -100.0f, 0.0f, Color, true);
		const int32 Base = Out.Vertices.Num();
		Out.Vertices.Append(Ribbon.Vertices);
		Out.Normals.Append(Ribbon.Normals);
		Out.UVs.Append(Ribbon.UVs);
		Out.VertexColors.Append(Ribbon.VertexColors);
		Out.Tangents.Append(Ribbon.Tangents);
		for (int32 Index : Ribbon.Triangles) { Out.Triangles.Add(Base + Index); }
	}

	static UStaticMesh* LoadBasicMesh(const TCHAR* Path)
	{
		return LoadObject<UStaticMesh>(nullptr, Path);
	}

	static UMaterialInterface* LoadVertexColorMaterial()
	{
		return LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
	}

	static UInstancedStaticMeshComponent* AddInstancedComponent(AVoidDistrictActor* Owner, const FString& Name, UStaticMesh* Mesh)
	{
		UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(Owner, FName(*Name), RF_Transactional);
		Component->SetStaticMesh(Mesh);
		Component->SetupAttachment(Owner->GetRootComponent());
		Component->RegisterComponent();
		Owner->AddInstanceComponent(Component);
		return Component;
	}

	static FVoidDistrictProfileData ToProfileData(const FVoidDistrictProfile& P)
	{
		FVoidDistrictProfileData Data;
		Data.StructuralModel = P.StructuralModel;
		Data.RadialBand = P.RadialBand;
		Data.VerticalTier = P.VerticalTier;
		switch (P.Density)
		{
			case EVoidDistrictDensity::NearZero: Data.Density = TEXT("NearZero"); break;
			case EVoidDistrictDensity::Sparse:   Data.Density = TEXT("Sparse"); break;
			case EVoidDistrictDensity::Moderate: Data.Density = TEXT("Moderate"); break;
			default:                             Data.Density = TEXT("High"); break;
		}
		Data.BuildingCoverage = P.BuildingCoverage;
		Data.MinStories = P.MinStories;
		Data.MaxStories = P.MaxStories;
		Data.OpenSpaceRatio = P.OpenSpaceRatio;
		Data.BlockSizeUnits = P.BlockSizeUnits;
		Data.CommercialShare = P.CommercialShare;
		Data.VegetationDensity = P.VegetationDensity;
		Data.PlazaCellsPerNode = P.PlazaCellsPerNode;
		Data.LandmarkCount = P.LandmarkCount;
		Data.Provenance = P.Provenance;
		return Data;
	}

	static void LogReport(FVoidGenerationContext& Context, const FVoidValidationReport& Report)
	{
		for (const FVoidValidationIssue& Issue : Report.Issues)
		{
			if (Issue.Severity == EVoidValidationSeverity::Info) { continue; }
			const TCHAR* Level = Issue.Severity == EVoidValidationSeverity::Warning ? TEXT("Warning") : TEXT("Error");
			Context.Log(FString::Printf(TEXT("[District %s] %s (%s)"), Level, *Issue.Message, *Issue.ErrorCode.ToString()));
		}
	}

	static void MergeReport(FVoidValidationReport& Into, const FVoidValidationReport& From)
	{
		for (const FVoidValidationIssue& Issue : From.Issues)
		{
			Into.Issues.Add(Issue);
			if (Issue.Severity == EVoidValidationSeverity::Error || Issue.Severity == EVoidValidationSeverity::Fatal)
			{
				Into.bIsValid = false;
			}
		}
	}

	static void BuildDistrictMeshes(AVoidDistrictActor* Actor, const FVoidPlannedDistrict& District, const FVoidDistrictLayoutParams& Params)
	{
		UMaterialInterface* Material = LoadVertexColorMaterial();

		// Boundary outline(s).
		if (Params.bGenerateBoundaryOutlines && (Params.bGenerateBelowGradeBoundaries || District.Boundary.Z > -500.0))
		{
			FVoidRoadMeshSection Outline;
			const FLinearColor Color = DistrictColor(District.Id);
			auto AddRegion = [&](const FVoidPlannedBoundary& Region)
			{
				AppendOutline(Outline, Region.Polygon, Region.Z + 12.0, Color);
				if (Region.HoleRadius > 0.0)
				{
					AppendOutline(Outline, FVoidDistrictGeometry::MakeCirclePolygon(Region.HoleCenter, Region.HoleRadius, 96), Region.Z + 12.0, Color);
				}
			};
			if (District.SubRegions.Num() > 0) { for (const FVoidPlannedBoundary& Region : District.SubRegions) { AddRegion(Region); } }
			else { AddRegion(District.Boundary); }

			if (!Outline.IsEmpty())
			{
				FVoidRoadMeshBuilder::CreateSection(Actor->BoundaryMesh, 0, Outline, false);
				if (Material) { Actor->BoundaryMesh->SetMaterial(0, Material); }
			}
		}

		// Public spaces: one section per type so tools can toggle them.
		TMap<int32, FVoidRoadMeshSection> ByType;
		for (const FVoidPlannedPublicSpace& Space : District.PublicSpaces)
		{
			FVoidRoadMeshSection& Section = ByType.FindOrAdd(static_cast<int32>(Space.Type));
			const FLinearColor Color = PublicSpaceColor(Space.Type);
			if (Space.HoleRadius > 0.0) { AppendAnnulus(Section, Space.Polygon, Space.HoleCenter, Space.HoleRadius, Space.Z + 3.0, Color); }
			else { AppendConvexPolygon(Section, Space.Polygon, Space.Z + 3.0, Color); }
		}
		int32 SectionIndex = 0;
		for (TPair<int32, FVoidRoadMeshSection>& Pair : ByType)
		{
			FVoidRoadMeshBuilder::CreateSection(Actor->PublicSpaceMesh, SectionIndex, Pair.Value, false);
			if (Material) { Actor->PublicSpaceMesh->SetMaterial(SectionIndex, Material); }
			++SectionIndex;
		}
	}

	static void BuildTreeMarkers(AVoidDistrictActor* Actor, const FVoidPlannedDistrict& District, const FVoidDistrictLayoutParams& Params, UStaticMesh* SphereMesh)
	{
		if (!Params.bGenerateTreeMarkers || !SphereMesh || !Actor->TreeMarkers || District.Profile.VegetationDensity <= 0.0)
		{
			return;
		}

		Actor->TreeMarkers->SetStaticMesh(SphereMesh);
		FVoidDistrictRng Rng(FVoidDistrictGeometry::StableHash(N(District.Id) + TEXT("_trees")) + static_cast<uint32>(Params.Seed));

		for (const FVoidPlannedPublicSpace& Space : District.PublicSpaces)
		{
			if (Space.Type != EVoidPublicSpaceType::Park || Space.Polygon.Num() != 4) { continue; }

			const FVector2D& P0 = Space.Polygon[0];
			const FVector2D Edge1(Space.Polygon[1].X - P0.X, Space.Polygon[1].Y - P0.Y);
			const FVector2D Edge2(Space.Polygon[3].X - P0.X, Space.Polygon[3].Y - P0.Y);
			const double Len1 = FMath::Sqrt(Edge1.X * Edge1.X + Edge1.Y * Edge1.Y);
			const double Len2 = FMath::Sqrt(Edge2.X * Edge2.X + Edge2.Y * Edge2.Y);
			if (Len1 < 1.0 || Len2 < 1.0) { continue; }

			const double Margin = Params.TreeSpacing * 0.5;
			for (double A = Margin; A < Len1 - Margin * 0.5; A += Params.TreeSpacing)
			{
				for (double B = Margin; B < Len2 - Margin * 0.5; B += Params.TreeSpacing)
				{
					if (Rng.NextUnit() > District.Profile.VegetationDensity) { continue; }
					const FVector Location(P0.X + Edge1.X / Len1 * A + Edge2.X / Len2 * B, P0.Y + Edge1.Y / Len1 * A + Edge2.Y / Len2 * B, 350.0);
					Actor->TreeMarkers->AddInstance(FTransform(FRotator::ZeroRotator, Location, FVector(3.0, 3.0, 3.0)));
				}
			}
		}
	}
}

FName FVoidDistrictGenerator::GetGeneratorId() const
{
	return FName(TEXT("District"));
}

int32 FVoidDistrictGenerator::RemoveExistingGeneration(UWorld* World)
{
	if (!World) { return 0; }

	TArray<AActor*> ToDestroy;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->Tags.Contains(VoidDistrictTags::Generated))
		{
			ToDestroy.Add(*It);
		}
	}

	for (AActor* Actor : ToDestroy)
	{
#if WITH_EDITOR
		World->EditorDestroyActor(Actor, true);
#else
		Actor->Destroy();
#endif
	}
	return ToDestroy.Num();
}

bool FVoidDistrictGenerator::Generate(const FVoidDesignPackage& Package, FVoidGenerationContext& Context)
{
	using namespace VoidDistrictGeneratorPrivate;

	const UVoidDistrictGenerationSettings* Settings = GetDefault<UVoidDistrictGenerationSettings>();
	if (!Settings || Settings->MeridianDataDirectory.Path.IsEmpty())
	{
		Context.GenerationValidationReport.AddError(
			TEXT("No Meridian data directory configured for the District Generator."),
			TEXT("settings.MeridianDataDirectory"), TEXT("VOID.District.NoDataDirectory"),
			TEXT("Set Project Settings -> Plugins -> VOID World Builder District Generation -> Meridian Data Directory to the folder containing Meridian_Master.json."));
		return false;
	}

	FVoidMeridianDataSet Data;
	FVoidValidationReport LoadReport;
	LoadReport.bIsValid = true;
	const bool bLoaded = FVoidMeridianRegistryReader::LoadFromDirectory(Settings->MeridianDataDirectory.Path, Data, LoadReport);
	MergeReport(Context.GenerationValidationReport, LoadReport);
	LogReport(Context, LoadReport);
	if (!bLoaded)
	{
		return false;
	}

	FVoidMeridianLayoutOverrides Overrides;
	FVoidValidationReport OverrideReport;
	OverrideReport.bIsValid = true;
	FVoidMeridianRegistryReader::LoadOverridesFromFile(Settings->LayoutOverridesFile.FilePath, Overrides, OverrideReport, false);
	MergeReport(Context.GenerationValidationReport, OverrideReport);
	if (!OverrideReport.bIsValid)
	{
		LogReport(Context, OverrideReport);
		return false;
	}

	FVoidDistrictLayoutInput Input;
	Input.Data = &Data;
	Input.Overrides = &Overrides;
	Input.Params = Settings->Layout;
	Input.CharacterOverrides = Settings->CharacterOverrides;

	FVoidDistrictSpawnOptions Options;
	Options.bPreferRegisteredBuildingGenerator = Settings->bPreferRegisteredBuildingGenerator;
	Options.bContinueOnValidationErrors = Settings->bContinueOnValidationErrors;
	Options.bUseRegionFolders = Settings->bUseRegionFolders;

	return GenerateFromInput(Input, Options, &Package, Context, nullptr);
}

bool FVoidDistrictGenerator::GenerateFromInput(const FVoidDistrictLayoutInput& Input, const FVoidDistrictSpawnOptions& Options, const FVoidDesignPackage* ExplicitPackage, FVoidGenerationContext& Context, FVoidDistrictGenerationStats* OutStats)
{
	using namespace VoidDistrictGeneratorPrivate;

	FVoidDistrictGenerationStats Stats;

	UWorld* World = Context.TargetWorld;
	if (!World)
	{
		Context.GenerationValidationReport.AddError(TEXT("District generation needs a target world."), TEXT("context.TargetWorld"), TEXT("VOID.District.NoWorld"));
		return false;
	}
	if (!Input.Data)
	{
		Context.GenerationValidationReport.AddError(TEXT("District generation needs Meridian data."), TEXT("input.Data"), TEXT("VOID.District.NoData"));
		return false;
	}

	// ---- 1. Plan + validate (no world changes yet) -------------------------------------------
	FVoidDistrictLayoutPlan Plan = FVoidDistrictLayoutBuilder::Build(Input);
	if (!Plan.Report.HasFatalIssue() && ExplicitPackage)
	{
		if (FVoidDistrictLayoutBuilder::ApplyExplicitPackage(Plan, *ExplicitPackage))
		{
			Context.Log(FString::Printf(TEXT("[District] Explicit package geometry applied to district '%s'."), *ExplicitPackage->District.DistrictId.Value.ToString()));
		}
	}
	if (!Plan.Report.HasFatalIssue())
	{
		FVoidDistrictValidator::Validate(Plan, *Input.Data, Input.Params);
	}

	MergeReport(Context.GenerationValidationReport, Plan.Report);
	LogReport(Context, Plan.Report);

	if (Plan.Report.HasFatalIssue() || (Plan.Report.NumErrors() > 0 && !Options.bContinueOnValidationErrors))
	{
		Context.Log(TEXT("[District] Aborted before touching the level: the plan has validation errors."));
		return false;
	}
	if (Context.IsCancelled()) { return false; }

	const FVoidDistrictLayoutParams& Params = Input.Params;
	const FScopedTransaction Transaction(LOCTEXT("GenerateDistricts", "Generate VOID Meridian Districts"));

	// ---- 2. Regeneration: remove exactly what a previous run created --------------------------
	Stats.NumRemovedActors = RemoveExistingGeneration(World);
	if (Stats.NumRemovedActors > 0)
	{
		Context.Log(FString::Printf(TEXT("[District] Removed %d actors from the previous generation."), Stats.NumRemovedActors));
	}

	// ---- 3. Ownership tree: Meridian root + one actor per district -------------------------------
	AVoidMeridianRootActor* Root = SpawnGenerated<AVoidMeridianRootActor>(World, TEXT("Meridian"), FTransform::Identity);
	if (!Root)
	{
		Context.GenerationValidationReport.AddError(TEXT("Failed to spawn the Meridian root actor."), TEXT("world"), TEXT("VOID.District.SpawnFailed"));
		return false;
	}
	Root->Tags.AddUnique(VoidDistrictTags::Root);
	Root->MeridianId = FName(TEXT("meridian"));
	Root->Seed = Params.Seed;
	SetFolder(Root, TEXT("Meridian"), Options.bUseRegionFolders);

	TMap<FName, AVoidDistrictActor*> DistrictActors;
	for (const FVoidPlannedDistrict& District : Plan.Districts)
	{
		AVoidDistrictActor* Actor = SpawnGenerated<AVoidDistrictActor>(World, FString::Printf(TEXT("District_%s"), *N(District.Id)), FTransform::Identity);
		if (!Actor) { continue; }

		Actor->DistrictId = District.Id;
		Actor->DisplayName = District.DisplayName;
		Actor->Profile = ToProfileData(District.Profile);
		Actor->NumRoads = District.Roads.Num();
		Actor->NumBuildings = District.Buildings.Num();
		Actor->NumPublicSpaces = District.PublicSpaces.Num();
		Actor->Tags.AddUnique(VoidDistrictTags::District(District.Id));
		Actor->AttachToActor(Root, FAttachmentTransformRules::KeepWorldTransform);
		SetFolder(Actor, FString::Printf(TEXT("Meridian/%s"), *N(District.Id)), Options.bUseRegionFolders);

		for (const FVoidPlannedNode& Node : District.Nodes)
		{
			FVoidDistrictNodeInfo Info;
			Info.Index = Node.Index;
			Info.bFlagship = Node.bFlagship;
			Info.Center = FVector(Node.Center.X, Node.Center.Y, 0.0);
			Info.WorldPartitionRegion = Node.WorldPartitionRegion;
			Actor->Nodes.Add(Info);
		}
		for (const FVoidPlannedPort& Port : District.Ports)
		{
			FVoidDistrictPortInfo Info;
			Info.Id = Port.Id;
			Info.Kind = Port.Kind;
			Info.Location = Port.Location;
			Info.Connects = Port.Connects;
			Actor->Ports.Add(Info);
		}

		Root->Districts.Add(Actor);
		DistrictActors.Add(District.Id, Actor);
		++Stats.NumDistricts;
	}
	if (Context.IsCancelled()) { return false; }

	// ---- 4. Roads: the existing Road generator, unchanged. Two calls so live and dead networks never share a junction graph. ----
	TMap<FName, FName> RoadOwner;
	FVoidDesignPackage LivePackage;
	FVoidDesignPackage DeadPackage;
	LivePackage.District.DistrictId = FVoidElementId(FName(TEXT("meridian_live_network")));
	DeadPackage.District.DistrictId = FVoidElementId(FName(TEXT("meridian_dead_network")));
	for (const FVoidPlannedRoad* Road : Plan.GatherAllRoads())
	{
		RoadOwner.Add(Road->Spec.Id.Value, Road->OwnerId);
		(Road->Spec.ElevationUnits > -500.0f ? LivePackage : DeadPackage).District.Roads.Add(Road->Spec);
	}

	TSharedPtr<IVoidGenerator> RoadGenerator = FVoidGeneratorRegistry::Get().FindGenerator(TEXT("Road"));
	TSharedPtr<FVoidRoadGenerator> FallbackRoadGenerator;
	if (!RoadGenerator.IsValid())
	{
		FallbackRoadGenerator = MakeShared<FVoidRoadGenerator>();
		RoadGenerator = FallbackRoadGenerator;
	}

	const TSet<AActor*> BeforeRoads = SnapshotActors(World);
	for (FVoidDesignPackage* Package : { &LivePackage, &DeadPackage })
	{
		if (Package->District.Roads.Num() == 0) { continue; }

		FVoidGenerationContext RoadContext;
		RoadContext.TargetWorld = World;
		RoadContext.IsCancellationRequested = Context.IsCancellationRequested;
		const bool bOk = RoadGenerator->Generate(*Package, RoadContext);
		Context.OutputLog.Append(RoadContext.OutputLog);
		MergeReport(Context.GenerationValidationReport, RoadContext.GenerationValidationReport);
		if (!bOk)
		{
			Context.Log(FString::Printf(TEXT("[District] Road generator reported failure for '%s'."), *Package->District.DistrictId.Value.ToString()));
			Context.GenerationValidationReport.AddError(TEXT("Road generation failed during district generation."), TEXT("roads"), TEXT("VOID.District.RoadGenerationFailed"));
			return false;
		}
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (BeforeRoads.Contains(Actor)) { continue; }

		FName Owner = NAME_None;
		bool bJunction = false;
		if (const AVoidRoadActor* RoadActor = Cast<AVoidRoadActor>(Actor))
		{
			if (const FName* Found = RoadOwner.Find(RoadActor->RoadId.Value)) { Owner = *Found; }
		}
		else if (const AVoidRoadJunctionActor* Junction = Cast<AVoidRoadJunctionActor>(Actor))
		{
			bJunction = true;
			for (const FVoidElementId& ConnectedId : Junction->ConnectedRoadIds)
			{
				if (const FName* Found = RoadOwner.Find(ConnectedId.Value)) { Owner = *Found; break; }
			}
		}
		else
		{
			continue;
		}

		AVoidDistrictActor** DistrictActor = DistrictActors.Find(Owner);
		if (DistrictActor && *DistrictActor)
		{
			Adopt(Actor, *DistrictActor, VoidDistrictTags::District(Owner), TEXT("Roads"), FString::Printf(TEXT("Meridian/%s/Roads"), *N(Owner)), Options.bUseRegionFolders);
			(*DistrictActor)->RoadActors.Add(Actor);
		}
		else
		{
			Adopt(Actor, Root, VoidDistrictTags::District(VoidDistrictNames::LiveNetworkOwner), TEXT("Roads"), TEXT("Meridian/LiveNetwork"), Options.bUseRegionFolders);
		}
		bJunction ? ++Stats.NumJunctionActors : ++Stats.NumRoadActors;
	}
	if (Context.IsCancelled()) { return false; }

	// ---- 5. Per-district: buildings, public spaces, landmarks -----------------------------------------
	UStaticMesh* CubeMesh = LoadBasicMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* SphereMesh = LoadBasicMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	TSharedPtr<IVoidGenerator> BuildingGenerator = Options.bPreferRegisteredBuildingGenerator ? FVoidGeneratorRegistry::Get().FindGenerator(BuildingGeneratorId) : TSharedPtr<IVoidGenerator>();

	for (const FVoidPlannedDistrict& District : Plan.Districts)
	{
		AVoidDistrictActor** Found = DistrictActors.Find(District.Id);
		if (!Found || !*Found) { continue; }
		AVoidDistrictActor* Actor = *Found;
		const FName DistrictTag = VoidDistrictTags::District(District.Id);

		BuildDistrictMeshes(Actor, District, Params);
		Stats.NumPublicSpaces += District.PublicSpaces.Num();
		BuildTreeMarkers(Actor, District, Params, SphereMesh);

		// Buildings.
		bool bBuildingsDone = District.Buildings.Num() == 0;
		if (!bBuildingsDone && BuildingGenerator.IsValid())
		{
			FVoidDesignPackage BuildingPackage;
			BuildingPackage.District.DistrictId = FVoidElementId(District.Id);
			for (const FVoidPlannedBuilding& Building : District.Buildings) { BuildingPackage.District.Buildings.Add(Building.Spec); }

			const TSet<AActor*> Before = SnapshotActors(World);
			FVoidGenerationContext BuildingContext;
			BuildingContext.TargetWorld = World;
			BuildingContext.IsCancellationRequested = Context.IsCancellationRequested;
			const bool bOk = BuildingGenerator->Generate(BuildingPackage, BuildingContext);
			Context.OutputLog.Append(BuildingContext.OutputLog);
			MergeReport(Context.GenerationValidationReport, BuildingContext.GenerationValidationReport);

			for (TActorIterator<AActor> It(World); It; ++It)
			{
				AActor* NewActor = *It;
				if (Before.Contains(NewActor)) { continue; }
				Adopt(NewActor, Actor, DistrictTag, TEXT("Buildings"), FString::Printf(TEXT("Meridian/%s/Buildings"), *N(District.Id)), Options.bUseRegionFolders);
				Actor->BuildingActors.Add(NewActor);
			}

			if (bOk)
			{
				bBuildingsDone = true;
				Stats.bUsedRegisteredBuildingGenerator = true;
			}
			else
			{
				Context.Log(FString::Printf(TEXT("[District] Registered Building generator failed for '%s'; using greybox instancing."), *N(District.Id)));
			}
		}

		if (!bBuildingsDone && CubeMesh)
		{
			TMap<FName, UInstancedStaticMeshComponent*> ByCategory;
			for (const FVoidPlannedBuilding& Building : District.Buildings)
			{
				UInstancedStaticMeshComponent** Component = ByCategory.Find(Building.CategoryId);
				if (!Component)
				{
					UInstancedStaticMeshComponent* Created = AddInstancedComponent(Actor, FString::Printf(TEXT("Buildings_%s"), *N(Building.CategoryId)), CubeMesh);
					Actor->BuildingComponents.Add(Created);
					Component = &ByCategory.Add(Building.CategoryId, Created);
				}

				const double Height = Building.Spec.HeightUnits;
				const FVector Location(Building.Center.X, Building.Center.Y, Building.BaseZ + Height * 0.5);
				const FRotator Rotation(0.0, FMath::RadiansToDegrees(Building.YawRadians), 0.0);
				const FVector Scale(Building.HalfExtent.X * 2.0 / 100.0, Building.HalfExtent.Y * 2.0 / 100.0, Height / 100.0);
				(*Component)->AddInstance(FTransform(Rotation, Location, Scale));
			}
		}
		Stats.NumBuildings += District.Buildings.Num();

		// Landmarks: actors other systems query. Stand-alone volumes get a proxy; building-backed ones reuse the building geometry.
		for (const FVoidPlannedLandmark& Landmark : District.Landmarks)
		{
			AVoidDistrictLandmarkActor* LandmarkActor = SpawnGenerated<AVoidDistrictLandmarkActor>(World, FString::Printf(TEXT("Landmark_%s"), *N(Landmark.Id)),
				FTransform(FRotator(0.0, Landmark.YawDegrees, 0.0), Landmark.Location));
			if (!LandmarkActor) { continue; }

			LandmarkActor->LandmarkId = Landmark.RegistryId;
			LandmarkActor->InstanceId = Landmark.Id;
			LandmarkActor->DistrictId = Landmark.OwnerId;
			LandmarkActor->LandmarkType = Landmark.Type;
			LandmarkActor->VisibilityTier = Landmark.VisibilityTier;
			LandmarkActor->bInterior = Landmark.bInterior;
			LandmarkActor->bBelowGrade = Landmark.bBelowGrade;
			LandmarkActor->bPositionIsPlaceholder = Landmark.bPositionIsPlaceholder;
			LandmarkActor->OpenFlagId = Landmark.OpenFlagId;
			LandmarkActor->BackingBuildingId = Landmark.BackingBuildingId;
			LandmarkActor->SightlineVisibleFrom = Landmark.SightlineVisibleFrom;
			LandmarkActor->SightlineExcluded = Landmark.SightlineExcluded;
			LandmarkActor->HalfExtent = Landmark.HalfExtent;
			LandmarkActor->Height = Landmark.Height;
			LandmarkActor->Tags.AddUnique(VoidDistrictTags::Landmark);
			Adopt(LandmarkActor, Actor, DistrictTag, TEXT("Landmarks"), FString::Printf(TEXT("Meridian/%s/Landmarks"), *N(District.Id)), Options.bUseRegionFolders);

			const bool bStandaloneVolume = Landmark.BackingBuildingId.IsNone() && Landmark.HalfExtent.X > 0.0 && Landmark.Height > 0.0;
			if (bStandaloneVolume && CubeMesh && LandmarkActor->ProxyMesh)
			{
				LandmarkActor->ProxyMesh->SetStaticMesh(CubeMesh);
				LandmarkActor->ProxyMesh->SetRelativeLocation(FVector(0.0, 0.0, Landmark.Height * 0.5));
				LandmarkActor->ProxyMesh->SetRelativeScale3D(FVector(Landmark.HalfExtent.X * 2.0 / 100.0, Landmark.HalfExtent.Y * 2.0 / 100.0, Landmark.Height / 100.0));
			}
			else if (LandmarkActor->ProxyMesh)
			{
				LandmarkActor->ProxyMesh->DestroyComponent();
				LandmarkActor->ProxyMesh = nullptr;
			}

			Actor->Landmarks.Add(LandmarkActor);
			++Stats.NumLandmarks;
		}
	}

	Context.Log(FString::Printf(TEXT("[District] Generated %d districts: %d road actors, %d junctions, %d buildings (%s), %d public spaces, %d landmarks."),
		Stats.NumDistricts, Stats.NumRoadActors, Stats.NumJunctionActors, Stats.NumBuildings,
		Stats.bUsedRegisteredBuildingGenerator ? TEXT("Building generator") : TEXT("greybox instancing"), Stats.NumPublicSpaces, Stats.NumLandmarks));

	if (OutStats) { *OutStats = Stats; }
	return true;
}

#undef LOCTEXT_NAMESPACE
