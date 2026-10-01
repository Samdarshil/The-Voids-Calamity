using UnrealBuildTool;

public class VOIDWorldBuilderCore : ModuleRules
{
	public VOIDWorldBuilderCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});

		// Deliberately no editor-only dependencies here. Core must stay
		// Runtime-safe so its data types (FVoidDesignPackage, FVoidValidationReport)
		// could in principle be consumed at runtime in a packaged build,
		// without dragging UnrealEd or Slate into that build.
	}
}
