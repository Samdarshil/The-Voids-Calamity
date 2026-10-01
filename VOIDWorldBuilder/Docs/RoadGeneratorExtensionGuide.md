# Road Generator Extension Guide

## Adding a new road type

1. Add the value to `EVoidRoadType` (`VOIDWorldBuilderCore/Public/Types/VoidWorldBuilderTypes.h`).
2. Add its string mapping in `FVoidJsonPackageReader`'s `TryParseRoadType`
   lookup (`VoidJsonPackageReader.cpp`) so JSON's `"roadType"` field can
   name it.
3. Add its canonical row-name mapping in
   `VoidRoadTypeProfilePrivate::GetCanonicalRowName` (`VoidRoadTypeProfile.cpp`)
   -- keep this string identical to step 2's, so a DataTable authored
   against the JSON vocabulary works without translation.
4. Add a built-in default profile entry in
   `FVoidRoadTypeProfileLibrary::GetBuiltInDefault`.
5. If the new type needs genuinely different geometry (like Roundabout
   does), branch on it explicitly in `FVoidRoadGenerator::Generate` the
   same way Roundabout is branched today -- don't try to force a special
   case through the generic ribbon path if it doesn't fit.

## Adding a new road validation rule

1. Add the check to the appropriate `FVoidRoadValidator::Validate*`
   method, or add a new one and call it from `Validate`.
2. Choose a severity and a code following `VOID.RoadGen.<Reason>`.
3. Register the code in `Docs/ValidationRules.md`.
4. Add a test in `VoidRoadValidatorTests.cpp` covering both the failing
   and passing case.

## Adding a new flat strip surface (e.g. bike lanes)

`FVoidRoadMeshBuilder::BuildRibbon` already handles any flat strip
following a polyline at a lateral offset range -- a bike lane is just
another call to it at a different offset and color, following the same
pattern as curbs/sidewalks/medians in
`VoidRoadGeneratorPrivate::BuildSidewalkCurbSections`. Add a `bHasBikeLane`
field to `FVoidRoadSpec` (additive, default `false`) if the design
package needs to author it, following the same field-addition process as
`Docs/RoadGeneratorArchitecture.md` documents for the existing fields.

## Adding an entirely new generator (e.g. Building, Phase 4)

Follow the exact pattern this phase established for Road:

1. Create a subfolder under `VOIDWorldBuilderGenerators/{Public,Private}/`
   named after the generator (e.g. `Building/`).
2. Implement `IVoidGenerator` (from Core) -- `GetGeneratorId()` returns a
   unique `FName` (e.g. `"Building"`), `Generate()` does the work.
3. Add a generation-time validator (`FVoidBuildingValidator`, following
   `FVoidRoadValidator`'s pattern) for "can this actually be built" checks
   distinct from Phase 2's import-time validation.
4. Register it in `FVOIDWorldBuilderGeneratorsModule::StartupModule()`
   alongside the Road registration; unregister the matching id in
   `ShutdownModule()`.
5. Add any new `.Build.cs` dependencies your generator needs (e.g. a
   different component type) -- Phase 3's own addition of
   `ProceduralMeshComponent` and `UnrealEd` to `Generators.Build.cs` is
   the precedent.
6. **The Editor panel needs zero changes** if it's calling through
   `FVoidGeneratorRegistry` generically already. Phase 3's panel
   currently hardcodes the button to `FindGenerator(TEXT("Road"))` since
   there's only one generator to choose from; once a second generator
   exists, replace that hardcoded id with a small picker (a dropdown
   populated from `FVoidGeneratorRegistry::Get().GetAllGenerators()`) --
   this is the one place multi-generator support will need actual new
   UI, and it's a small, contained change.
