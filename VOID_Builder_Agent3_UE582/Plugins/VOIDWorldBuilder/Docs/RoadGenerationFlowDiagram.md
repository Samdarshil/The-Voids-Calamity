# Road Generation Flow Diagram

## Editor panel -> generator registry -> spawned actors

```
User clicks "Generate Roads" (enabled only after a successful import)
        |
        v
FVoidGeneratorRegistry::Get().FindGenerator(TEXT("Road"))
        |
        v
FScopedSlowTask(1.0f).MakeDialog(bShowCancelButton=true)
        |
        v
Context.IsCancellationRequested = [&SlowTask] { return SlowTask.ShouldCancel(); }
        |
        v
IVoidGenerator::Generate(LastImportedPackage, Context)   <-- resolves to FVoidRoadGenerator
        |
        v
====================== FVoidRoadGenerator::Generate =========================
        |
        v
Context.GenerationValidationReport = FVoidRoadValidator::Validate(District)
        |                                    (bridge+tunnel conflict, unresolvable
        |                                     connectionIds, invalid roundabouts)
        v
   Fatal issue? ---yes---> abort, return false
        |no
        v
Read UVoidRoadGenerationSettings (ramp length, pier spacing, junction
tolerance, debug viz toggle, optional RoadTypeProfileTable)
        |
        v
+-------------------------- per road (District.Roads) ------------------------+
|                                                                              |
|  Context.IsCancelled()? --yes--> break loop                                 |
|         |no                                                                 |
|         v                                                                   |
|  FVoidRoadTypeProfileLibrary::ResolveProfile(RoadType, ProfileTable)        |
|         |                                                                   |
|         v                                                                   |
|  RoadType == Roundabout ?                                                   |
|     yes -> FVoidRoadSplineBuilder::BuildRoundaboutLoopPoints                |
|     no  -> FVoidRoadSplineBuilder::BuildCenterlinePoints                    |
|         |                                                                   |
|         v                                                                   |
|  (non-roundabout) FVoidRoadBridgeTunnelBuilder::ApplyElevationRamp          |
|         |                                                                   |
|         v                                                                   |
|  Spawn AVoidRoadActor (FScopedTransaction-wrapped, undo/redo-safe)          |
|         |                                                                   |
|         v                                                                   |
|  FVoidRoadSplineBuilder::ApplyPointsToSpline(RoadSpline, Points)            |
|         |                                                                   |
|         v                                                                   |
|  FVoidRoadMeshBuilder::BuildRibbon (surface) -> CreateSection(0)            |
|  bHasMedian?  -> BuildRibbon (median) -> CreateSection(1)                   |
|  bHasSidewalk? -> curb ribbons -> CreateSection(2,3)                        |
|                -> sidewalk ribbons -> CreateSection(4,5)                   |
|         |                                                                   |
|         v                                                                   |
|  bIsBridge?  -> ComputeBridgePierPositions -> ISMC instances (piers)        |
|  bIsTunnel?  -> ComputeTunnelPortalPositions -> ISMC instances (2 portals)  |
|         |                                                                   |
|         v                                                                   |
|  bDrawDebugVisualization? -> DrawDebugLine along the spline                 |
|                                                                              |
+------------------------------------------------------------------------------+
        |
        v
FVoidRoadIntersectionBuilder::BuildJunctionGraph(BuiltRoads, JunctionTolerance)
        |               (Union-Find endpoint clustering + explicit ConnectionIds
        |                + roundabout-center coincidence -> classified junctions:
        |                DeadEnd / CulDeSac / TwoWayJoin / TJunction /
        |                FourWayJunction / Complex / RoundaboutSpur)
        v
+------------------------ per junction ----------------------------------+
|  Context.IsCancelled()? --yes--> break loop                            |
|         |no                                                            |
|         v                                                              |
|  CulDeSac? -> enlarge PadRadius (turnaround bulb)                      |
|         |                                                              |
|         v                                                              |
|  FVoidRoadIntersectionBuilder::BuildJunctionPad -> spawn                |
|  AVoidRoadJunctionActor -> CreateSection(0)                            |
|         |                                                              |
|         v                                                              |
|  T/FourWay/Complex? -> per connected road: BuildCrosswalkStripe         |
|                        -> CreateSection(1, 2, ...)                     |
|         |                                                              |
|         v                                                              |
|  bDrawDebugVisualization? -> DrawDebugSphere at the junction            |
+--------------------------------------------------------------------------+
        |
        v
Context.OutputLog += summary lines; UE_LOG(LogVoidGenerators, ...)
        |
        v
return (NumRoadsBuilt > 0)
        |
        v
Editor panel reads Context.GenerationValidationReport (Validation Panel),
Context.OutputLog (Generation Log), and elapsed time (Generation Summary)
```
