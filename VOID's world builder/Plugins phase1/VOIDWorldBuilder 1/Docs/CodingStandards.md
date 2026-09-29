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
