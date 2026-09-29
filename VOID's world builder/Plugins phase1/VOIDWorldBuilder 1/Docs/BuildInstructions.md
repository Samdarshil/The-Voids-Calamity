# Build Instructions

Target engine: **Unreal Engine 5.6**.

## 1. Compiling

### Via IDE (Visual Studio / Rider)
1. Place this plugin's `VOIDWorldBuilder` folder under `<YourProject>/Plugins/`.
2. Right-click your `.uproject` → **Generate Visual Studio project files**
   (or open directly in Rider, which regenerates automatically).
3. Build the `Development Editor` configuration for your target platform.
   All four modules (`VOIDWorldBuilderCore`, `VOIDWorldBuilderImport`,
   `VOIDWorldBuilderGenerators`, `VOIDWorldBuilderEditor`) build as part of
   the normal project build — no separate build step is required for the
   plugin.

### Via command line (UnrealBuildTool)
```
"<EngineRoot>/Engine/Build/BatchFiles/Build.bat" ^
  <YourProjectName>Editor Win64 Development ^
  -Project="<PathTo>/<YourProject>.uproject" ^
  -WaitMutex
```
Replace `Win64` and `Build.bat` with your platform's equivalents on
macOS/Linux (`Build.sh`, `Mac`/`Linux`).

## 2. Running Automation Tests

1. Open the project in the Editor.
2. **Window → Developer Tools → Session Frontend → Automation** tab.
3. Filter for `VOID.WorldBuilder`. Phase 1 ships four Import tests and one
   Core test:
   - `VOID.WorldBuilder.Core.ValidationReportTracksErrorsAndWarnings`
   - `VOID.WorldBuilder.Import.ValidPackageParsesAndValidates`
   - `VOID.WorldBuilder.Import.UnapprovedPackageFailsValidation`
   - `VOID.WorldBuilder.Import.MalformedJsonFailsGracefully`
   - `VOID.WorldBuilder.Import.DuplicateIdsAreRejected`
4. Run selected tests; all five should pass on a clean Phase 1 build.

Command-line equivalent (useful for CI):
```
"<EngineRoot>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" ^
  "<PathTo>/<YourProject>.uproject" ^
  -ExecCmds="Automation RunTests VOID.WorldBuilder; Quit" ^
  -unattended -nopause -nullrhi
```

## 3. Running the Commandlet

The commandlet imports and validates a single design package headlessly,
returning exit code `0` on success and `1` on any validation error or
missing argument — suitable for a CI gate on design-package exports.

```
"<EngineRoot>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" ^
  "<PathTo>/<YourProject>.uproject" ^
  -run=VoidWorldBuilder -Package="C:/DesignExports/district_04.json"
```

## 4. Icon

`Resources/Icon128.png` is a placeholder. Replace with a studio-approved
128x128 icon before this plugin leaves internal-tool status; it has no
effect on compilation either way.
