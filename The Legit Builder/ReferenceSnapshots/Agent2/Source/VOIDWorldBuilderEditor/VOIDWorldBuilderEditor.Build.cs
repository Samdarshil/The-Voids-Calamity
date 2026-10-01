using UnrealBuildTool;

public class VOIDWorldBuilderEditor : ModuleRules
{
	public VOIDWorldBuilderEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"VOIDWorldBuilderCore",
			"VOIDWorldBuilderImport",
			"VOIDWorldBuilderGenerators"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"Slate",
			"SlateCore",
			"ApplicationCore",
			"WorkspaceMenuStructure",
			"DesktopPlatform",
			"MainFrame"
		});
	}
}
