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
            "DeveloperSettings",
            "VOIDWorldBuilderCore",
            "VOIDWorldBuilderImport",
            "VOIDWorldBuilderLighting",
            "ProceduralMeshComponent",
        });
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "UnrealEd",
            "Json",
            "MaterialEditor",
            "AssetRegistry",
        });
    }
}
