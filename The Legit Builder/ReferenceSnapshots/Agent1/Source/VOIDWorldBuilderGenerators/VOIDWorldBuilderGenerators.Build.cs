using UnrealBuildTool;

public class VOIDWorldBuilderGenerators : ModuleRules
{
	public VOIDWorldBuilderGenerators(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"VOIDWorldBuilderCore",
			"VOIDWorldBuilderImport",
			"DeveloperSettings",
			"ProceduralMeshComponent" // Public: VoidRoadMeshBuilder.h exposes UProceduralMeshComponent / FProcMeshTangent to consumers.
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd"
		});

		// Phase 3 (Road Generator) is the first generator to land, so this
		// is the first phase that actually needs UnrealEd (FScopedTransaction
		// for undo/redo around spawned actors) and ProceduralMeshComponent
		// (road/sidewalk/curb/median ribbon geometry). Future generators
		// (Building, Navigation, etc.) add their own dependencies here in
		// their own phase, following the same pattern.
	}
}
