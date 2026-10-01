# Road Generator Class Diagram

```
Core (VOIDWorldBuilderCore) -- extended, not redesigned
============================================================
EVoidRoadType (enum)                  Highway/Primary/Secondary/Local/Service/Alley/Roundabout
FVoidRoadSpec (extended)               + RoadType, LaneCount, SpeedLimitUnits, ElevationUnits,
                                         bHasSidewalk, bHasMedian, bIsBridge, bIsTunnel,
                                         bCulDeSacAtEnd, RoundaboutRadiusUnits, ConnectionIds
IVoidGenerator (extended)              FVoidGenerationContext gained:
                                         + TFunction<bool()> IsCancellationRequested
                                         + FVoidValidationReport GenerationValidationReport

Generators Module (VOIDWorldBuilderGenerators) -- new, Road/ subfolder
============================================================
IVoidGenerator (Core)
      ^
      | implements
      |
FVoidRoadGenerator                     GetGeneratorId() -> "Road"
      |                                Generate(Package, Context) -- the orchestrator
      | uses
      +--> FVoidRoadValidator          Validate(District) -> FVoidValidationReport
      |       (bridge/tunnel conflict, unresolvable connectionIds, invalid roundabout)
      |
      +--> FVoidRoadTypeProfileLibrary
      |       GetBuiltInDefault(RoadType) -> FVoidRoadTypeProfile
      |       ResolveProfile(RoadType, DataTable*) -> FVoidRoadTypeProfile
      |
      +--> FVoidRoadSplineBuilder
      |       BuildCenterlinePoints(RoadSpec) -> TArray<FVector>
      |       BuildRoundaboutLoopPoints(RoadSpec, N) -> TArray<FVector>
      |       ApplyPointsToSpline(USplineComponent*, Points, bClosedLoop)
      |
      +--> FVoidRoadBridgeTunnelBuilder
      |       ApplyElevationRamp(RoadSpec, Points&, RampLen, BridgeH, TunnelD)
      |       ComputeBridgePierPositions(...) -> TArray<FVector>
      |       ComputeTunnelPortalPositions(...) -> TArray<FVector>
      |
      +--> FVoidRoadMeshBuilder
      |       BuildRibbon(Points, LeftOff, RightOff, HeightOff, Color, bClosedLoop) -> FVoidRoadMeshSection
      |       CreateSection(UProceduralMeshComponent*, Index, Section, bCollision)
      |
      +--> FVoidRoadIntersectionBuilder
      |       BuildJunctionGraph(TArray<FVoidBuiltRoad>, Tolerance) -> TArray<FVoidRoadJunction>
      |       BuildJunctionPad(Junction, Color) -> FVoidRoadMeshSection
      |       BuildCrosswalkStripe(...) -> FVoidRoadMeshSection
      |
      +--> spawns --> AVoidRoadActor            (AActor)
      |                 RootScene (USceneComponent)
      |                 RoadSpline (USplineComponent)
      |                 RoadMesh (UProceduralMeshComponent, multi-section)
      |                 StructureMarkers (UInstancedStaticMeshComponent -- piers or portals)
      |                 RoadId, RoadType
      |
      +--> spawns --> AVoidRoadJunctionActor     (AActor)
                        JunctionMesh (UProceduralMeshComponent -- pad + crosswalk sections)
                        ConnectedRoadIds

FVoidGeneratorRegistry (Core-facing, Import-module-pattern-mirrored)
      RegisterGenerator(TSharedRef<IVoidGenerator>)   <- FVoidRoadGenerator registered here
      FindGenerator(FName) -> TSharedPtr<IVoidGenerator>

UVoidRoadGenerationSettings (UDeveloperSettings)
      RoadTypeProfileTable (TSoftObjectPtr<UDataTable>)
      RampLengthUnits, DefaultBridgeHeightUnits, DefaultTunnelDepthUnits, BridgePierSpacingUnits
      JunctionToleranceUnits
      bDrawDebugVisualization, DebugVisualizationDurationSeconds

FVoidRoadTypeProfile (FTableRowBase)
      DefaultWidthUnits, DefaultLaneCount, DefaultSpeedLimitUnits
      bDefaultHasSidewalk, bDefaultHasMedian (informational only -- see ArchitectureNotes)
      SidewalkWidthUnits, CurbWidthUnits, CurbHeightUnits, MedianWidthUnits

Editor Module (VOIDWorldBuilderEditor) -- extended, generator-agnostic
============================================================
SVoidWorldBuilderPanel
      Import section (Phase 2, unchanged)
      + Generate Roads button -> FVoidGeneratorRegistry::Get().FindGenerator("Road")->Generate(...)
      + Generation Summary (result, warnings, errors, elapsed ms)
      + Generation Validation Panel (SListView<FVoidValidationIssue>, same row renderer as Import)
      + Generation Log (Context.OutputLog, scrollable text)
      -- never references FVoidRoadGenerator or any Road/* type directly
```
