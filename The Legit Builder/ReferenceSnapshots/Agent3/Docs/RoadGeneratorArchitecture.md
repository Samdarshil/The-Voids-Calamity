# Road Generator Architecture Notes — Phase 3

## Why `FVoidRoadSpec` grew by ten fields

Phase 2's `FVoidRoadSpec` had exactly three fields: `Id`, `CenterlinePoints`,
`WidthUnits`. Phase 3's brief explicitly requires reading Road Type, Lane
Count, Elevation, Connections, Speed Classification, Sidewalk/Median
presence, and Bridge/Tunnel flags from the design package. That data has
to live somewhere, and the only honest place is `FVoidRoadSpec` itself.

Every addition is optional with a safe default, so every Phase 2 package
remains valid unchanged:

| Field | Default | Rationale |
|---|---|---|
| `RoadType` | `Local` | Closed enum (unlike `BuildingType`'s free-form string) because the set is small, fixed, and drives real branching -- `Roundabout` produces circular geometry, not a ribbon. |
| `LaneCount` | `0` (unset) | Falls back to the road type's profile default. |
| `SpeedLimitUnits` | `0` (unset) | Same. |
| `ElevationUnits` | `0.0` | A flat, uniform elevation offset -- independent of bridge/tunnel ramps, which are a separate post-process. |
| `bHasSidewalk`, `bHasMedian` | `false` | Plain bools have no "unset" sentinel the way `0` works for the numeric fields above, so these are honored exactly as authored -- see "Why bools don't inherit profile defaults" below. |
| `bIsBridge`, `bIsTunnel` | `false` | Mutually exclusive -- `FVoidRoadValidator` rejects a road with both set. |
| `bCulDeSacAtEnd` | `false` | Only meaningful for a road's end point that connects to nothing else. |
| `RoundaboutRadiusUnits` | `0.0` | Only meaningful when `RoadType == Roundabout`; `CenterlinePoints[0]` becomes the center. |
| `ConnectionIds` | empty | Optional explicit adjacency; falls back to coincident-endpoint detection. |

This is the one explicit architecture revision Phase 3 makes to a
previously "immutable" struct -- called out here rather than silently
changed, the same way Phase 2 called out introducing `FVoidDistrictData`.

## Why bools don't inherit profile defaults

`FVoidRoadTypeProfile` has `bDefaultHasSidewalk`/`bDefaultHasMedian`
fields, but the Road Generator never reads them to fill in a road spec's
own `bHasSidewalk`/`bHasMedian`. Unlike `WidthUnits <= 0` or
`LaneCount == 0` -- unambiguous "not specified" sentinels, since neither
is ever a sensible real value -- a plain bool has no safe unused value.
"False" could mean "this road explicitly has no sidewalk" or "the author
didn't think about sidewalks." Guessing wrong in either direction is a
real correctness bug, not a convenience. So these two profile fields are
purely informational (DataTable-authoring guidance), and the spec's own
bools are honored exactly as authored.

## Why generation-time validation is separate from import-time validation

Phase 2's `FVoidPackageValidator` checks "is this data shaped correctly."
Phase 3's `FVoidRoadValidator` checks "can this actually be built" --
a road can pass import validation and still be un-generatable (flagged
as both a bridge and a tunnel is structurally valid JSON, but a
contradiction no generator can act on). Keeping these separate means
Phase 2's validator never has to know anything about generation
semantics, and a future Building/Navigation generator adds its own
generation-time validator following the same pattern without touching
Phase 2's code.

## The reader/generator registry pattern, reused

`FVoidRoadGenerator` is registered with `FVoidGeneratorRegistry` under
the id `"Road"` in `FVOIDWorldBuilderGeneratorsModule::StartupModule()`,
exactly mirroring how `FVoidJsonPackageReader` was registered with
`FVoidPackageReaderRegistry` in Phase 2. The Editor panel calls
`FVoidGeneratorRegistry::Get().FindGenerator(TEXT("Road"))` and invokes
the generic `IVoidGenerator::Generate()` -- it never references
`FVoidRoadGenerator` by name. This is why a future Building generator
needs zero Editor-module changes: implement `IVoidGenerator`, register it
under a new id, and the same "Generate" button pattern extends naturally
(a future phase may add a generator picker; Phase 3 has exactly one
generator, so the button is hardcoded to "Road" for now -- see
`Docs/RoadGeneratorExtensionGuide.md`).

## "Road Generator Module" -- organizational choice

The Phase 3 brief asks for a "Road Generator Module." Phase 1's approved
architecture already established `VOIDWorldBuilderGenerators` as the
single home for every generator (Phase 3 through 9), with a `Road/`
subfolder specifically anticipated for this purpose. Rather than create
a new top-level Unreal plugin module (which would contradict "do NOT
redesign previous phases"), Phase 3 implements the Road Generator as a
cohesive, self-contained subfolder (`Source/VOIDWorldBuilderGenerators/{Public,Private}/Road/`)
within that already-approved module. This satisfies "a Road Generator
Module" as a real, organized unit of code while honoring the immutable
Phase 1 module boundary.

## Greybox scope -- what this deliberately does not attempt

Every geometry function in Phase 3 (ribbon strips, junction pads,
crosswalk stripes, roundabout loops, bridge/tunnel ramps) targets
blockout fidelity: flat-shaded triangles, simple linear/circular offsets,
no material assets, no banking or superelevation, no clothoid transition
curves, no true per-approach intersection blending. This is the correct
scope for "generate placeholder blockout, not final art" (Volume IV),
not a shortfall relative to a higher standard this phase isn't meant to
meet. Where a simplification was made, it's named explicitly:

- **Intersections** get a flat N-gon pad sized to the widest connected
  road, not a per-approach blended mesh.
- **Roundabout spurs** get a simple pad at the ring's edge, not true
  geometric blending into the ring.
- **Bridges/tunnels** get a linear ramp-flat-ramp elevation profile with
  placeholder pier/portal markers (Engine's built-in `/Engine/BasicShapes/Cube`),
  not real structural geometry.
- **Crosswalks** are alternating-color vertex-painted stripes, not a
  material-based crosswalk texture.

## Why Data Layer assignment isn't implemented

The Data Layer API surface changed meaningfully across UE5.0-5.6 minor
versions (the introduction of `UDataLayerManager`, deprecation of older
`UWorld`-level accessors). Guessing at the exact signature to implement
an unrequested "assign to Data Layer" feature risked a real compile
break for something Phase 3 didn't strictly need -- generated actors are
already World Partition-safe by virtue of being spawned through the
standard `UWorld::SpawnActor` path, which is the actual "World Partition
compatibility" requirement. Data Layer assignment is deferred to
whichever phase first has a concrete, testable need for it against a
pinned engine version.

## Why Navigation and PCG integration are out of scope

Both are explicitly parenthesized "(future)" in the Phase 3 brief. The
road surface mesh section has collision enabled (`bEnableCollision = true`
in `FVoidRoadMeshBuilder::CreateSection`'s call site for section 0), so a
NavMesh could build against it once Navigation integration is actually
implemented -- but building that integration now, with no concrete
NavMesh generation step requesting it yet, would be exactly the kind of
speculative feature this project's studio rules discourage.
