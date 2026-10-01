# Road Generator Developer Guide

## Calling the Road Generator directly (outside the Editor panel)

```cpp
#include "VoidGeneratorRegistry.h"
#include "Interfaces/IVoidGenerator.h"

const TSharedPtr<IVoidGenerator> RoadGenerator = FVoidGeneratorRegistry::Get().FindGenerator(TEXT("Road"));
if (RoadGenerator.IsValid())
{
	FVoidGenerationContext Context;
	Context.TargetWorld = MyWorld; // never null

	const bool bSucceeded = RoadGenerator->Generate(ImportedPackage, Context);

	// Context.GenerationValidationReport -- generation-time issues (distinct from import validation)
	// Context.OutputLog -- human-readable summary lines
}
```

Always call this on an already-imported, already-validated
`FVoidDesignPackage` (i.e. `FVoidImportResult::WasSuccessful()` was
true). The generator does not re-run Phase 2's import validation.

## Wiring up cancellation

`FVoidGenerationContext::IsCancellationRequested` is a `TFunction<bool()>`.
Bind it to whatever progress/cancel mechanism your caller uses:

```cpp
FScopedSlowTask Progress(1.0f, FText::FromString(TEXT("Generating...")));
Progress.MakeDialog(/*bShowCancelButton=*/true);
Context.IsCancellationRequested = [&Progress]() { return Progress.ShouldCancel(); };
```

Leave it unbound (default) if your caller has no cancellation mechanism
(e.g. a CI script) -- `Context.IsCancelled()` treats an unbound callback
as "never cancelled."

## Road Type Profiles

Every road field with an unambiguous "unset" sentinel (`WidthUnits <= 0`,
`LaneCount == 0`, `SpeedLimitUnits == 0`) falls back to
`FVoidRoadTypeProfileLibrary::ResolveProfile(RoadType, OptionalDataTable)`:

1. If `UVoidRoadGenerationSettings::RoadTypeProfileTable` is set and has a
   row named after the type (`"Highway"`, `"Primary"`, `"Secondary"`,
   `"Local"`, `"Service"`, `"Alley"`, `"Roundabout"`), that row's values win.
2. Otherwise, `FVoidRoadTypeProfileLibrary::GetBuiltInDefault` supplies
   sensible hardcoded values -- the tool works with zero DataTable
   authoring required.

`bHasSidewalk`/`bHasMedian` are **not** part of this fallback -- see
`Docs/RoadGeneratorArchitecture.md` for why a plain bool can't safely
inherit a profile default the way a numeric "unset" sentinel can.

## Bridges and Tunnels

- A road is a bridge or a tunnel, never both (`FVoidRoadValidator` rejects
  both flags set).
- The elevation ramp is a simple linear ramp-flat-ramp: `RampLengthUnits`
  (Project Settings) controls how much of the span, from each end, is the
  transition; the middle is fully elevated/depressed by
  `DefaultBridgeHeightUnits`/`DefaultTunnelDepthUnits`.
- Bridge piers are placed only in the fully-elevated middle span (never
  on the ramps), spaced by `BridgePierSpacingUnits`, using
  `UInstancedStaticMeshComponent` (many identical instances -- the
  textbook ISMC use case).
- Tunnel portals are exactly 2 markers, at the very start and end of the
  tunnel road's points.
- Piers/portals use Engine's built-in `/Engine/BasicShapes/Cube` --
  always present, no project-specific mesh dependency.

## Intersections

`FVoidRoadIntersectionBuilder::BuildJunctionGraph` clusters road
endpoints by:
1. Coincident position within `JunctionToleranceUnits` (Project Settings).
2. Explicit `FVoidRoadSpec::ConnectionIds`, which force-merge the closest
   endpoint pair between two named roads regardless of distance.

Classification is by how many *distinct* roads share a cluster:
1 -> Dead End (or Cul-de-Sac if `bCulDeSacAtEnd` is set on that road's end
point), 2 -> Two-Way Join, 3 -> T-Junction, 4 -> Four-Way, 5+ -> Complex.
Roundabouts are excluded from this graph entirely (they have no simple
start/end) and instead matched as `RoundaboutSpur` targets.

Crosswalk stripes are only generated at T-Junction/Four-Way/Complex
junctions -- a plain two-way join or dead end doesn't get one.

## Debug Visualization

`UVoidRoadGenerationSettings::bDrawDebugVisualization` (off by default).
When on, generation draws green lines along every road's spline and a
yellow sphere at every junction, for `DebugVisualizationDurationSeconds`
seconds (0 or less = persistent until manually cleared via `FlushPersistentDebugLines`).
Useful for iterating on a design package's geometry before committing to
full mesh generation.

## Extending this generator

See `Docs/RoadGeneratorExtensionGuide.md` for adding a new road type, a
new validation rule, or an entirely new generator following this same
pattern.
