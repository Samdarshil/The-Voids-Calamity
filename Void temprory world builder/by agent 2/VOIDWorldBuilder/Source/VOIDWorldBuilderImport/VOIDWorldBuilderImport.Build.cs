using UnrealBuildTool;

public class VOIDWorldBuilderImport : ModuleRules
{
	public VOIDWorldBuilderImport(ReadOnlyTargetRules Target) : base(Target)
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
			"Json"
		});

		// JsonUtilities (FJsonObjectConverter) is deliberately not linked here.
		// FVoidJsonPackageReader maps JSON to FVoidDesignPackage by hand (see
		// VoidJsonPackageReader.cpp's MapJsonObjectToPackage) so each field can
		// carry a specific validation-friendly mapping issue instead of a
		// generic USTRUCT-from-JSON converter failing silently on a mismatch.
		// Revisit if the schema grows enough that hand mapping becomes the
		// maintenance burden instead.
	}
}
