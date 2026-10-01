# Coding Standards

This plugin follows Epic's official Unreal Engine coding standard. Notes
below cover the conventions actually exercised in Phase 1, for quick
reference during review — this is not a restatement of Epic's full
document.

## Naming

- `F` prefix: plain C++ structs/classes with no polymorphism requirement
  (`FVoidDesignPackage`, `FVoidJsonReader`).
- `U` prefix: `UObject`-derived classes (`UVoidWorldBuilderCommandlet`).
- `I` prefix: abstract interfaces (`IVoidGenerator`).
- `S` prefix: Slate widgets (`SVoidWorldBuilderPanel`).
- `E` prefix: enums (`EVoidValidationSeverity`).
- `b` prefix: boolean members (`bIsValid`, `bIsApproved`).
- Module names are `PascalCase` with no separators (`VOIDWorldBuilderCore`),
  matching folder and `.Build.cs` class names exactly.

## Formatting

- Allman brace style (opening brace on its own line) throughout, matching
  Epic's own engine source.
- Tabs for indentation.
- One class/struct per header where practical; small, tightly-related data
  structs (e.g. `FVoidBuildingSpec` and `FVoidRoadSpec`) may share a header
  when they're part of the same logical data model.

## Module Boundaries

- `Public/` exposes only what other modules need. Implementation detail
  types (e.g. the `VoidImportPrivate` helper namespace in
  `VoidDesignPackageImporter.cpp`) stay in `Private/` or in an anonymous/
  named namespace inside the `.cpp` file.
- Every cross-module include goes through a `Public/` header of the target
  module — no reaching into another module's `Private/` folder.
- `*_API` export macros (e.g. `VOIDWORLDBUILDERCORE_API`) are applied to
  every type and static function that a different module calls.

## Data vs. Behavior

- `Core` structs (`FVoidDesignPackage`, `FVoidValidationReport`, etc.) are
  data-only: trivial constructors, simple accessors, no I/O, no engine
  subsystem calls. This keeps them safe to use from Runtime and trivial to
  unit test.
- Behavior lives in stateless classes with `static` entry points
  (`FVoidJsonReader`, `FVoidPackageValidator`, `FVoidDesignPackageImporter`)
  rather than as methods bolted onto the data structs.

## Logging

- All plugin log output goes through the single shared
  `LogVoidWorldBuilder` category declared in `VoidWorldBuilderLog.h`.
  Don't add a second log category without a specific reason documented in
  the header.

## Comments

- Every public class/struct has a header comment explaining **why it
  exists**, not just what it does — matching this plugin's brief that every
  file needs to justify its own presence in the skeleton.
- Inline comments are reserved for non-obvious decisions (e.g. why
  `JsonUtilities` isn't linked, why validation happens after mapping rather
  than during it) — not for restating what the next line of code already
  says.

## Testing

- Automation tests live in a `Private/Tests/` subfolder inside the module
  they test, guarded by `#if WITH_DEV_AUTOMATION_TESTS`. This is the
  standard in-module convention and keeps tests compiled only into
  Development/Editor builds.
- Test names follow `VOID.WorldBuilder.<Module>.<WhatIsBeingAsserted>` so
  the Session Frontend's automation tree stays organized by module as more
  generators add their own tests in later phases.

## Logging (Phase 2 addendum)

- `LogVoidWorldBuilder` (Core): plugin-wide, low-volume, cross-cutting
  (module lifecycle, reader/generator registration).
- `LogVoidImport` (Import): the import pipeline specifically, including
  Verbose-level per-field mapping trace. See `Docs/ArchitectureNotes.md`
  for why these are kept separate rather than merged into one category.
- Don't add a third category without the same kind of documented reason.

## Validation error codes (Phase 2 addendum)

- Every `FVoidValidationIssue` gets a stable `ErrorCode` following
  `VOID.Import.<Reason>` (PascalCase reason, no further nesting).
- Every Error/Fatal issue must include a concrete `SuggestedFix` -- "add a
  non-empty X" beats "invalid X." Info/Warning issues may omit it when
  there's genuinely no action to suggest.
- Register every new code in `Docs/ValidationRules.md` in the same change
  that introduces it -- that document is generated from source via a
  `grep` for exactly this reason; don't let it drift.

## Phase 3 addendum: generators

- **`TObjectPtr` for new UPROPERTY object pointers.** Phase 1/2 had no
  UObject-typed UPROPERTY members to speak of; Phase 3's actors
  (`AVoidRoadActor`, `AVoidRoadJunctionActor`) use `TObjectPtr<T>` rather
  than raw `T*` for their component UPROPERTYs, matching current UE5
  best practice. Keep using `TObjectPtr` for any new object-pointer
  UPROPERTY going forward.
- **`LogVoidGenerators`** is the Generators-module-wide log category --
  shared by Road today and every future generator, the same one-category-
  per-module rule as `LogVoidWorldBuilder`/`LogVoidImport`. Don't add a
  `LogVoidRoadGenerator`-style per-generator category.
- **Generation-time validators are separate from import-time
  validators.** `FVoidRoadValidator` (Generators module) checks "can this
  be built"; `FVoidPackageValidator` (Import module, Phase 2) checks "is
  this data shaped correctly." A future Building/Navigation/etc. generator
  adds its own `FVoid<Name>Validator` following the same split -- don't
  fold generation-time checks into `FVoidPackageValidator`.
- **Error codes for generation-time issues use `VOID.RoadGen.<Reason>`**
  (or `VOID.<GeneratorName>Gen.<Reason>` for a future generator) --
  distinct namespace from `VOID.Import.<Reason>` so a log/report reader
  can tell at a glance which validation stage produced an issue.
- **Greybox geometry is the standard, not a shortfall.** Every mesh
  builder in `Road/` targets flat-shaded blockout fidelity deliberately
  (see `Docs/RoadGeneratorArchitecture.md`'s "Greybox scope" section).
  Don't add material-based detail, banking/superelevation, or true
  per-approach intersection blending without an explicit ask -- that's
  scope this project's own canon (Volume IV) says isn't the goal yet.
