# Validation & Integration Architecture (Agent 3)

## Module layout and dependency direction

```
VOIDWorldBuilderCore        (Runtime)  report types, IVoidValidator, FVoidValidationContext,
      ^                                FVoidValidatorRegistry, FVoidGeneratedTags, IVoidGenerator
VOIDWorldBuilderImport      (Editor)   importer, Phase 2 package validator
      ^
VOIDWorldBuilderGenerators  (Editor)   registry, Road generator (+ any other generator)
      ^
VOIDWorldBuilderValidation  (Editor)   Meridian validator, package/road/building/world validators,
                                       contract auditor, pipeline runner, exporter, commandlet
VOIDWorldBuilderEditor      (Editor)   UI (unchanged)
```

**Validation is a leaf.** Nothing may depend on it. A generator module that wants its output validated
implements `IVoidValidator` and registers with `FVoidValidatorRegistry` (both in Core) - no dependency
on Validation is needed, so there is no cycle. `Tools/static_lint.py` fails the build if a cycle or a
missing `Build.cs` dependency appears.

## Pipeline

```
[Meridian package] --FVoidMeridianValidator--> FVoidMeridianSummary (district ids, route ids, blocked content...)
[Design package]   --importer--> FVoidImportResult
                       |
   FVoidValidationPipeline::Run / RunFromImport   (steps are DATA: FVoidPipelineDefinition)
                       |
   0. registry audit           (FVoidGeneratorContractAuditor)
   1. PostImport validators    Data, Roads, Buildings            -> ERROR aborts: no generator runs
   2. Generate Road            -> report captured + attributed -> PostRoad validators (+ World)
   3. Generate District        -> PostDistrict validators
   4. Generate Building        -> PostBuilding validators
   5. Generate Environment     -> PostEnvironment validators
   6. Generate Lighting        -> PostLighting validators
   7. Generate Props           -> PostProps validators
   8. Generate Metro           -> PostMetro validators
   9. Final validators         (always runs, even after an abort, on whatever was generated)
```

- Generator order comes from the definition, **never** from `FVoidGeneratorRegistry::GetAllGenerators()` (TMap order is unspecified).
- Default definition requires only `Road`; other missing generators are WARNINGs (partial integration builds work).
  `MakeDefault(true)` requires all seven - use it for the final integration run.
- Each generator gets its own `FVoidGenerationContext`; its `GenerationValidationReport` is merged, attributed
  (`Subsystem = generator id`, `Source = "Generator:<id>"`) and de-duplicated immediately, because the Phase 3
  Road generator **overwrites** that single slot on every call.
- `bValidateOnly` runs every validator but spawns nothing and needs no world.

## Adding a validator (any agent, any module)

```cpp
class FVoidBuildingWorldValidator : public IVoidValidator
{
public:
    virtual FName GetValidatorId() const override { return TEXT("VOID.Building.World"); }   // unique, convention VOID.<Area>.<Name>
    virtual FName GetSubsystem()  const override { return TEXT("Building"); }
    virtual bool  AppliesToStage(EVoidValidationStage S) const override { return S == EVoidValidationStage::PostBuilding; }
    virtual void  Validate(EVoidValidationStage, const FVoidValidationInput& In, FVoidValidationContext& Ctx) const override
    {
        // In.Package / In.World may be null - tolerate it.
        Ctx.Error(TEXT("VOID.Building.NoMesh"), TEXT("Building 'b_12' has no mesh"), TEXT("b_12"), TEXT("district.buildings[12]"), TEXT("Assign a mesh in the Building profile"));
    }
};
// StartupModule:  FVoidValidatorRegistry::Get().Register(MakeShared<FVoidBuildingWorldValidator>());
// ShutdownModule: FVoidValidatorRegistry::Get().Unregister(TEXT("VOID.Building.World"));
```

And, in every generator, tag what you spawn so world validation/cleanup can find it:

```cpp
FVoidGeneratedTags::Apply(Actor, TEXT("Building"), BuildingSpec.Id.Value.ToString());
```

## Issue anatomy (what a human debugging a Meridian world sees)

`[ERROR] VOID.World.MissingActor {World}` / description / `object:` / `source:` / `path:` / `at: (x, y, z)` / `fix:`.
Text, Markdown and JSON exports order most-severe-first (`FVoidValidationReportExporter`). Blocking issues should never
have an empty suggested fix (enforced for the validators in this module by the automation tests).

## Headless / CI

```
UnrealEditor-Cmd Project.uproject -run=VoidValidate -Meridian="<dir>" [-Package="<file>.json"] [-Out=report.json] [-AllowMissingDistricts] [-Strict]
python Tools/meridian_reference_validator.py "<MeridianDir>"      # no Unreal needed; same rules
python Tools/static_lint.py Plugins/VOIDWorldBuilder              # mechanical consistency
```

## What is deliberately NOT here

No generators, no orchestrator UI, no retry/partial-regeneration, no cleanup/undo of generated actors, no editor-panel wiring.
The Editor module is untouched; it still calls the Road generator directly (see risk R-09).
