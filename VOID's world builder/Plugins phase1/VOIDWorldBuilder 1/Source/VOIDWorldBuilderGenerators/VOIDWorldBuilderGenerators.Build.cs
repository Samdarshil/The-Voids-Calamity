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
			"VOIDWorldBuilderImport"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});

		// Generator-specific engine dependencies (ProceduralMeshComponent,
		// NavigationSystem, WorldPartitionEditor, UnrealEd, etc.) are added
		// in the phase that introduces the generator requiring them
		// (Phases 3-9), not here. Phase 1 contains only the generator
		// registry, which has no dependency beyond Core/Import.
	}
}
