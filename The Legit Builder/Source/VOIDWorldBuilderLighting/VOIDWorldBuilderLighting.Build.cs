using UnrealBuildTool;

public class VOIDWorldBuilderLighting : ModuleRules
{
	public VOIDWorldBuilderLighting(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
			"VOIDWorldBuilderCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Json",
			"JsonUtilities",
			"Projects"
		});

		// Agent 7 (Lighting). Deliberately a Runtime module with NO editor
		// dependencies: the director / district / landmark actors and the
		// preset data must keep working in PIE, Movie Render Queue and
		// packaged builds. Everything that needs UnrealEd (transactions,
		// material asset creation, world scanning for generation) lives in
		// VOIDWorldBuilderGenerators/Private/Lighting instead.
	}
}
