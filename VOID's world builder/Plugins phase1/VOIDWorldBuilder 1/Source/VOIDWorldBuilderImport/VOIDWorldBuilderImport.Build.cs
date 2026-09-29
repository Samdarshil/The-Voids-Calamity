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
			"VOIDWorldBuilderCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Json"
		});

		// JsonUtilities (FJsonObjectConverter) is deliberately not linked here.
		// Phase 1 maps JSON to FVoidDesignPackage by hand (see
		// VoidDesignPackageImporter::MapJsonToPackage) so each field can carry
		// a specific validation-friendly mapping warning; a generic
		// USTRUCT-from-JSON converter would fail silently on the wrong
		// mismatch instead. Revisit if the schema grows enough that hand
		// mapping becomes the maintenance burden instead.
	}
}
