using UnrealBuildTool;

public class VOIDWorldBuilderValidation : ModuleRules
{
	public VOIDWorldBuilderValidation(ReadOnlyTargetRules Target) : base(Target)
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
			"Json",
			// Needed only to read UProceduralMeshComponent::GetNumSections() when
			// snapshotting AVoidRoadActor for the world validator. Also requires the
			// ProceduralMeshComponent *plugin* to be declared in the .uplugin.
			"ProceduralMeshComponent"
		});

		// Dependency direction (no cycles):
		//   Core <- Import <- Generators <- Validation
		// Validation is a LEAF. Generator modules must NOT depend on it; they
		// register validators through IVoidValidator/FVoidValidatorRegistry,
		// which live in Core.
	}
}
