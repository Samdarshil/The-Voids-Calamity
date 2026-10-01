# Environment + Props Architecture — Agent 6 (Phases 12 and 14)

## No second architecture

Both generators are ordinary `IVoidGenerator`s registered on the existing
`FVoidGeneratorRegistry` in `FVOIDWorldBuilderGeneratorsModule::StartupModule()`:

| Id | Class | Domain |
|---|---|---|
| `Environment` | `FVoidEnvironmentGenerator` | trees, grass, bushes, rocks, planters, benches, poles, streetlights, traffic lights, sidewalk details, street props |
| `Props` | `FVoidPropGenerator` | vehicles, containers, crates, pipes, barriers, signs, billboards, trash, construction assets |

They consume the same `FVoidDesignPackage` the Road generator does, spawn into
`Context.TargetWorld`, log to `Context.Log`, report through
`Context.GenerationValidationReport`, honour `Context.IsCancelled()`, and wrap
work in an `FScopedTransaction`. They do **not** require the Road generator to
have run: road geometry is recomputed with the same builders
(`FVoidRoadSplineBuilder`, `FVoidRoadBridgeTunnelBuilder`,
`FVoidRoadIntersectionBuilder`) and the same profile resolution, so props land
on the roads that were (or will be) built.

Both generators share `FVoidDressingGeneratorBase`; a "domain" string on each
placement rule decides which generator runs it.

## Pipeline

```
FVoidDesignPackage
  -> BuildPlanInputs      roads (final points, band widths, tier), junctions (approach dirs),
                          areas (buildings -> Building/Park/Plaza + Use via token table)
  -> VoidPlan::Plan       PURE + deterministic (no UObject/UWorld). Rules -> instances.
  -> OnInstancesPlanned   delegate + optional JSON export  (= PCG hook, no PCG dependency)
  -> Regenerate           destroy previously owned actors for (generator, district)
  -> Spawn                one AVoidDressingActor per streaming cell; one (H)ISM component per
                          Category/Slot/Part bucket
```

Planning finishes (and can be cancelled) before anything is destroyed, so a
cancelled or failed plan leaves the previous generation intact.

## Placement contexts

| Context | Meaning | Key rule fields |
|---|---|---|
| `Roadside` | kerb-side band of any road (works without sidewalk: shoulder) | `LateralFraction`, `bAlternateSides`, `MinTier/MaxTier` |
| `Sidewalk` | only roads with `bHasSidewalk`; sits on the raised sidewalk (curb height) | same |
| `Curbside` | on the road surface at the lane edge (parked vehicles) | `LateralMode=RoadEdge`, `LateralInsetUnits` |
| `Alley` | Service/Alley tier only | `MinTier=4` |
| `Commercial` | sidewalk band, only near a building whose `Use` matches | `RequiredAreaUse`, `AreaProximityUnits` |
| `Junction` | per approach at junctions with N roads | `MinJunctionRoads`, `bBothSides` |
| `Park` / `Plaza` | area scatter, deterministic jittered grid | `DensityPer100SqM` (100 m² = 10⁶ uu²) |
| `Site` | area scatter inside buildings whose `Use` matches (e.g. Construction) | `RequiredAreaUse` |
| `Rooftop` | one per matching building above `MinBuildingHeight` | |
| `Facade` | edges that face a road within reach (never decorates unseen walls) | `ZOffset`, `AreaProximityUnits` |

Always rejected: tunnels, positions inside any building footprint (except
`Curbside`), positions within a junction pad + `JunctionClearance`, positions
outside the optional 2D bounds.

Road tiers: 0 Highway, 1 Primary/Roundabout, 2 Secondary, 3 Local, 4 Service, 5 Alley.

## Determinism

Every random decision is a hash of
`(GlobalSeed, DistrictId, RuleId, SourceElementId, Index)` using a CRC-of-string
(stable across sessions — no `FName` indices, no `FRandomStream` consumed in
iteration order). Consequences (each covered by a test):

- Same package + seed + settings ⇒ identical ids, locations, yaw, scale, slot choice.
- Adding a road or building never reshuffles existing elements' props.
- Rule order does not matter (output is sorted by `StableId`).
- Lowering density thins the *same* set (probability threshold on a fixed hash),
  so 50 % density is a strict subset of 100 %.
- Changing `GlobalSeed` produces a different but again stable variation.
- Junction keys come from the snapped junction location, not an index.

## Density, district variation, cinematic priority

Effective keep-probability =
`Rule.Probability × DensityScale × District.DensityMultiplier × District.CategoryMultiplier
 × polish factor × lerp(MinImportanceDensity, 1, Importance)`.

- **Polish** (`FVoidDistrictEnvProfileRow.Polish`, 0 neglected … 1 pristine) with
  each rule's `PolishResponse` (−1 clutter … +1 upkeep) gives district variation:
  trash/crates/pipes fade in polished districts, trees/planters grow.
- **Importance** = tier weight (60 %) + proximity to a focus point (40 %). Focus
  points: plazas, junctions with 3+ roads, and `CinematicFocusPoints` from
  settings. Low-importance places keep only `MinImportanceDensity` of their
  density — budget is spent where a camera looks.
- **Budget**: `MaxInstancesPerRun`; overflow drops lowest `Priority`
  (`PriorityBase + Importance`) first, ties by id (deterministic), with a warning.

## Instancing and performance

- One `AVoidDressingActor` per (generator, district, `CellSizeUnits` cell). Never one actor per prop.
- One `UHierarchicalInstancedStaticMeshComponent` (or ISM, per slot `bUseHISM`) per Category/Slot/Part.
- `Static` mobility, no collision, no overlap events, no nav impact, `bCanEverTick=false` on the actor.
- Per-category cull distances from placeholder/slot data.
- Placeholders are 1–2 `/Engine/BasicShapes` parts each; shared meshes across the whole city.
- Deterministic weighted slot choice by hash, so variation costs no extra components per instance.
- Planning is pure and thread-safe by construction (not yet moved off the game thread — see limitations).

## Asset slots (no hallucinated assets)

Nothing in this plugin or the supplied project references a game asset. With
every table empty, every category uses an engine-BasicShapes placeholder. To
replace: add an `FVoidAssetSlotRow` (row name = slot id) with the same
`Category` and a `Mesh`. Several rows per category give weighted variation.
A row whose mesh fails to load warns (`VOID.Env.SlotMeshMissing`) and falls back to the placeholder.

Replacement hooks on the output: per-actor `Buckets[]` (Category, SlotId, PartIndex,
`bPlaceholder`, `ReplacementTag`, component, stable instance ids), component tags
`VOID.Category.<x>`, `VOID.Slot.<x>`, `VOID.Placeholder`, and
`AVoidDressingActor::FindBucketComponent/SetBucketMesh`. The full
replacement workflow is intentionally not implemented.

## Data-driven configuration

`UVoidEnvironmentSettings` (Project Settings → Plugins → VOID World Builder Environment And Props):
optional `PlacementRuleTable` (replaces built-ins when set), `AssetSlotTable`,
`DistrictProfileTable`, `BuildingUseTokenTable`, plus seed, density, budget,
focus points, cell size, traffic handedness. All optional — the Road generator's
"table is an enhancement, never a prerequisite" contract.

`FVoidBuildingSpec::BuildingType` is free-form text, so it is mapped to
Kind/Use by ordered substring tokens (`Park`→Park, `Plaza`→Plaza,
`Commercial`/`Retail`→Commercial, `Construction`→Construction, …). Unmatched
types are plain obstacles with no use.

## PCG

The supplied plugin has no PCG dependency and this change adds none. The
integration seam is data, not a module link:

1. `FVoidDressingGeneratorBase::OnInstancesPlanned()` broadcasts every planned instance.
2. `bExportPlacementJson` writes `Saved/VOIDWorldBuilder/Placements/<district>_<domain>.json`
   (`void_placement_points_v1`: id, category, rule, context, source, xyz, yaw, scale, importance).

A project that uses PCG can import those points (or subscribe to the delegate
from its own module) and drive PCG graphs / Spawner nodes from the same
deterministic set, or disable a category here and let a graph own it. Making
PCG a hard dependency was rejected: it would add a plugin dependency to a
Builder whose existing modules are deliberately lean, for no benefit to the
placement logic, which is context/graph-of-roads reasoning PCG doesn't do out of the box.
No PCG graph or PCG-typed code was compiled or tested here.

## Regeneration and ownership

`AVoidDressingActor` carries `GeneratorId` + `DistrictId` (+ cell, seed) and actor
tags `VOIDGen`, `VOIDGen.<id>`, `VOIDDistrict.<id>`. A run destroys every actor
with the same (generator, district) pair, then spawns. Other generators' and
other districts' output is untouched. Instances have stable ids, exposed per
bucket. `ClearGenerated(World, District)` is public.
