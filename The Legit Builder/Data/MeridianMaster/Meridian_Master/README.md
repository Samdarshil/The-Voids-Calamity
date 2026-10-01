# VOID: THE UNSEEN HAND
## Meridian_Master Package — README

**Version:** 1.0.0
**Canon Version:** Volumes I–IV, Final
**Status:** In Progress — see file status table below
**Target Consumer:** VOID World Builder (Unreal Engine 5 Plugin)

---

## 1. What This Package Is

Meridian_Master is the official engineering input for the VOID World Builder. It integrates the five approved, locked district packages — Undercroft, White Zones, Metro Archives, Sector 0, Olympus Spire — into one buildable world: registries, dependency graphs, streaming and generation rules, and validation metadata. It contains **no lore, no narrative prose, and no implementation geometry**. Volumes I–IV and the five district packages remain the sole narrative and canon authority; this package exists to relate their data to each other and to the Builder, never to restate or override it.

## 2. What This Package Is Not

- Not a redesign of any approved district.
- Not a source of new canon, lore, or story resolution.
- Not a source of literal Unreal Engine coordinates, transforms, or mesh IDs — the Builder calculates all implementation geometry at generation time. This package describes relationships, hierarchy, and rules only.

## 3. Canon Lock

The following are final, approved, and **immutable** as of this package's creation. Nothing in Meridian_Master modifies, renames, or contradicts any of them:

- Volumes I–IV (Foundation, Main Campaign, Side Content, Developer Bible)
- Undercroft (`VOID_Location_01_The_Undercroft.md` + companion data)
- White Zones (`White_Zones.md` + companions)
- Metro Archives (`Metro_Archives.md` + companions)
- Sector 0 (`Sector_0.md` + companions)
- Olympus Spire (`Olympus_Spire.md` + companions)
- `Meridian_Master_Plan.md`

## 4. How the Builder Should Use This Package

`Meridian_Master.json` is the single file the Builder imports. It references every other file in this package by name and defines the import order, generation order, and module dependency graph the Builder should follow. No other file in this package should be imported manually or out of the sequence `Meridian_Master.json` defines. Full detail in `EngineeringHandoff.md` once generated.

## 5. Package Contents & Status

| File | Purpose | Status |
|---|---|---|
| `README.md` | This file — top-level orientation | ✅ Complete |
| `CHANGELOG.md` | Version history for this package | ⏳ Pending |
| `Meridian_Master.json` | Single Builder import manifest | ✅ Complete |
| `Meridian_Master.schema.json` | Schema for `Meridian_Master.json` | ⏳ Pending |
| `DistrictRegistry.json` | District IDs, hierarchy, adjacency, generation priority | ⏳ Pending |
| `DistrictRegistry.schema.json` | Schema for `DistrictRegistry.json` | ⏳ Pending |
| `RoadNetwork.json` | Road hierarchy and district connectivity | ⏳ Pending |
| `RoadNetwork.schema.json` | Schema for `RoadNetwork.json` | ⏳ Pending |
| `MetroNetwork.json` | Live/Dead transit network structure | ⏳ Pending |
| `MetroNetwork.schema.json` | Schema for `MetroNetwork.json` | ⏳ Pending |
| `NavigationGraph.json` | Pedestrian/vehicle/metro/underground/rooftop traversal rules | ⏳ Pending |
| `NavigationGraph.schema.json` | Schema for `NavigationGraph.json` | ⏳ Pending |
| `GameplayGraph.json` | Mission relationships, district progression, exploration loops | ⏳ Pending |
| `GameplayGraph.schema.json` | Schema for `GameplayGraph.json` | ⏳ Pending |
| `LandmarkRegistry.json` | Landmark ownership, visibility, and importance hierarchy | ⏳ Pending |
| `LandmarkRegistry.schema.json` | Schema for `LandmarkRegistry.json` | ⏳ Pending |
| `InfrastructureGraph.json` | Power, water, comms, and utility relationships | ⏳ Pending |
| `InfrastructureGraph.schema.json` | Schema for `InfrastructureGraph.json` | ⏳ Pending |
| `WorldPartition.json` | Streaming cell organization and dependencies | ⏳ Pending |
| `WorldPartition.schema.json` | Schema for `WorldPartition.json` | ⏳ Pending |
| `DataLayers.json` | Master Data Layer inventory across all districts | ⏳ Pending |
| `DataLayers.schema.json` | Schema for `DataLayers.json` | ⏳ Pending |
| `StreamingStrategy.json` | Loading, priority, and transition rules | ⏳ Pending |
| `StreamingStrategy.schema.json` | Schema for `StreamingStrategy.json` | ⏳ Pending |
| `BuilderRules.json` | Complete Builder execution pipeline | ⏳ Pending |
| `BuilderRules.schema.json` | Schema for `BuilderRules.json` | ⏳ Pending |
| `BuilderManifest.json` | Builder-facing execution manifest | ⏳ Pending |
| `PackageManifest.json` | Full package file inventory and checksentity | ⏳ Pending |
| `DependencyGraph.json` | Consolidated cross-file dependency graph | ⏳ Pending |
| `ValidationSchema.json` | Cross-package validation rule definitions | ⏳ Pending |
| `CompatibilityMatrix.md` | Version/schema compatibility reference | ⏳ Pending |
| `EngineeringHandoff.md` | Full engineering usage documentation | ⏳ Pending |
| `ValidationReport.md` | Final integrity and completeness validation | ⏳ Pending |
| `Meridian_Master_Index.md` | Master index of every file, purpose, and status | ⏳ Pending |

## 6. Open Items Carried Into This Package

These are not resolved by Meridian_Master and should not be treated as blocking its use, only as flagged risk for the specific systems they touch:

1. **Highest priority:** the Volume I / Volume II discrepancy regarding a Nyx encounter at the Olympus Spire server hub (Olympus Spire package, Sec. 0.1). Blocks Server Hub combat-encounter generation specifically.
2. **Moderate priority:** the Archive Maintenance Directive's narrative-weight interpretation (Metro Archives package). Blocks the reset mechanic's technical implementation pending fuller writing-team review.
3. **Low priority (×3):** structural interpretations in the Undercroft, White Zones, and Sector 0 packages, each flagged for lightweight sign-off in their own validation reports.

## 7. Version Compatibility

This package targets `void_meridian_master_schema_v1` and requires all five district packages at `void_world_location_schema_v1`. See `CompatibilityMatrix.md` (pending) for full detail once generated.

---

*Meridian_Master README — file 1 of the expanded Meridian_Master Package. Continuing with `CHANGELOG.md` next.*
