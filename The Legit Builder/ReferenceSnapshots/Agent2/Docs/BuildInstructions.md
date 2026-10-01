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
3. Filter for `VOID.WorldBuilder`. Phase 3 ships these tests:
   - **Core:**
     - `VOID.WorldBuilder.Core.ValidationReportTracksErrorsAndWarnings`
     - `VOID.WorldBuilder.Core.SchemaVersionParsesMajorMinor`
     - `VOID.WorldBuilder.Core.SchemaVersionCurrentToolVersionIsStable`
   - **Import:**
     - `VOID.WorldBuilder.Import.ValidPackageParsesAndValidates`
     - `VOID.WorldBuilder.Import.UnapprovedPackageFailsValidation`
     - `VOID.WorldBuilder.Import.MalformedJsonIsFatalNotJustError`
     - `VOID.WorldBuilder.Import.DuplicateIdsAreRejected`
     - `VOID.WorldBuilder.Import.ElementIdCollidingWithDistrictIdIsRejected`
     - `VOID.WorldBuilder.Import.SchemaVersionCompatibility`
     - `VOID.WorldBuilder.Import.UnknownFieldsAreInfoByDefault`
     - `VOID.WorldBuilder.Import.UnsupportedFileExtensionFailsGracefully`
     - `VOID.WorldBuilder.Import.UnreadableFileFailsGracefully`
     - `VOID.WorldBuilder.Import.ModeratelySizedPackageImportsWithinBudget` (perf smoke test)
   - **Road Generator:**
     - `VOID.WorldBuilder.RoadGenerator.SplineBuilder.StraightRoadProducesCollinearPoints`
     - `VOID.WorldBuilder.RoadGenerator.SplineBuilder.CurvedRoadPreservesWayPoints`
     - `VOID.WorldBuilder.RoadGenerator.SplineBuilder.RoundaboutProducesCircleAtRadius`
     - `VOID.WorldBuilder.RoadGenerator.SplineBuilder.RoundaboutWithoutRadiusProducesNoPoints`
     - `VOID.WorldBuilder.RoadGenerator.BridgeTunnel.BridgeRampsUpThenFlattensInMiddle`
     - `VOID.WorldBuilder.RoadGenerator.BridgeTunnel.TunnelDipsDownThenFlattensInMiddle`
     - `VOID.WorldBuilder.RoadGenerator.BridgeTunnel.PlainRoadIsUnaffectedByRampLogic`
     - `VOID.WorldBuilder.RoadGenerator.BridgeTunnel.PiersOnlyPlacedInElevatedMiddleSpan`
     - `VOID.WorldBuilder.RoadGenerator.BridgeTunnel.TunnelHasExactlyTwoPortals`
     - `VOID.WorldBuilder.RoadGenerator.Intersections.UnconnectedEndpointIsDeadEnd`
     - `VOID.WorldBuilder.RoadGenerator.Intersections.FlaggedEndpointIsCulDeSac`
     - `VOID.WorldBuilder.RoadGenerator.Intersections.ThreeCoincidentRoadsAreTJunction`
     - `VOID.WorldBuilder.RoadGenerator.Intersections.FourCoincidentRoadsAreFourWayJunction`
     - `VOID.WorldBuilder.RoadGenerator.Intersections.NearlyCoincidentEndpointsMergeWithinTolerance`
     - `VOID.WorldBuilder.RoadGenerator.Intersections.SpurIntoRoundaboutIsClassifiedSeparately`
     - `VOID.WorldBuilder.RoadGenerator.Validator.BridgeAndTunnelTogetherIsRejected`
     - `VOID.WorldBuilder.RoadGenerator.Validator.UnresolvableConnectionIdIsRejected`
     - `VOID.WorldBuilder.RoadGenerator.Validator.ConnectionToRealRoadPasses`
     - `VOID.WorldBuilder.RoadGenerator.Validator.RoundaboutWithoutRadiusIsRejected`
     - `VOID.WorldBuilder.RoadGenerator.Generate.StraightAndCurvedRoadsSpawnActorsWithMeshes`
     - `VOID.WorldBuilder.RoadGenerator.Generate.BridgeAndTunnelGetStructureMarkers`
     - `VOID.WorldBuilder.RoadGenerator.Generate.TJunctionSpawnsJunctionActor`
     - `VOID.WorldBuilder.RoadGenerator.Generate.EmptyDistrictFailsGracefully`
     - `VOID.WorldBuilder.RoadGenerator.Generate.DuplicateRoadIdsDoNotCrash`
     - `VOID.WorldBuilder.RoadGenerator.Generate.CancellationStopsEarly`
     - `VOID.WorldBuilder.RoadGenerator.Generate.LargeNetworkGeneratesWithinBudget` (perf smoke test)
4. Run selected tests; all should pass on a clean Phase 3 build. The
   `Generate.*` tests spawn real actors into the currently open editor
   level and clean them up at the end of each test -- if a test is
   interrupted mid-run, check for stray `VoidRoad_*`/`VoidJunction_*`
   actors and delete them manually.

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
