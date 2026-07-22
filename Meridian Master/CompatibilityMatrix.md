# VOID: THE UNSEEN HAND
## Meridian_Master Package — Compatibility Matrix

**Package Version:** 1.0.0
**Canon Version:** Volumes I–IV, Final
**Generated:** 2026-07-20

---

## 1. Overview

This document is the human-readable companion to `Meridian_Master.json`'s `version_compatibility` field and `BuilderManifest.json`'s `version_targeting` field. It does not introduce new version data — it presents what's already declared in those two files in a form easier for a reviewing engineer to scan than raw JSON.

---

## 2. Schema Version Matrix

| Schema ID | Target File | Status |
|---|---|---|
| `void_meridian_master_schema_v1` | `Meridian_Master.json` | ✅ File exists, cross-validated |
| `void_district_registry_schema_v1` | `DistrictRegistry.json` | ✅ File exists, cross-validated |
| `void_road_network_schema_v1` | `RoadNetwork.json` | ✅ File exists, cross-validated |
| `void_metro_network_schema_v1` | `MetroNetwork.json` | ✅ File exists, cross-validated |
| `void_navigation_graph_schema_v1` | `NavigationGraph.json` | ✅ File exists, cross-validated |
| `void_gameplay_graph_schema_v1` | `GameplayGraph.json` | ✅ File exists, cross-validated |
| `void_landmark_registry_schema_v1` | `LandmarkRegistry.json` | ✅ File exists, cross-validated |
| `void_infrastructure_graph_schema_v1` | `InfrastructureGraph.json` | ✅ File exists, cross-validated |
| `void_world_partition_schema_v1` | `WorldPartition.json` | ✅ File exists, cross-validated |
| `void_data_layers_schema_v1` | `DataLayers.json` | ✅ File exists, cross-validated |
| `void_streaming_strategy_schema_v1` | `StreamingStrategy.json` | ✅ File exists, cross-validated |
| `void_builder_rules_schema_v1` | `BuilderRules.json` | ✅ File exists, cross-validated |
| `void_world_location_schema_v1` | *(none — see Sec. 5)* | ⚠️ Referenced by all five district `_data.json` files; no standalone schema file was ever authored |

Every "cross-validated" entry above means exactly that: `jsonschema.validate()` was run against the real file pair during construction, not assumed from syntax validity alone. Two real schema bugs were caught and fixed this way — see `ValidationSchema.json` rule VR-002 for both precedents.

---

## 3. Canon Version Compatibility

| Source | Version | Compatibility |
|---|---|---|
| Volumes I–IV | Final | This entire package is built against this version. Any future volume revision requires re-validation of every cross-reference in this package. |
| Undercroft package | Approved | Locked. Checksummed in `PackageManifest.json`. |
| White Zones package | Approved | Locked. Checksummed in `PackageManifest.json`. |
| Metro Archives package | Approved | Locked. Checksummed in `PackageManifest.json`. |
| Sector 0 package | Approved | Locked. Checksummed in `PackageManifest.json`. |
| Olympus Spire package | Approved | Locked. Checksummed in `PackageManifest.json`. |
| Meridian_Master_Plan.md | Approved | Locked. Checksummed in `PackageManifest.json`. |

---

## 4. Engine Version Targeting

**Engine:** Unreal Engine 5 (no specific point version pinned).

This package targets UE5's World Partition and Data Layer systems generically. No fabricated specific version number (e.g. a invented "5.4" or "5.5") is declared anywhere in this package, consistent with the project's own stated platform target and the standing rule against fabricated engineering precision.

---

## 5. Known Gap: `void_world_location_schema_v1`

All five district `_data.json` files declare `"$schema": "void_world_location_schema_v1"` and have done so consistently since the Undercroft package. No standalone schema document for this schema ID was ever authored — unlike every schema in this Meridian_Master package, which was built and cross-validated against its real target file.

**This is a genuine gap, not a design choice.** It was surfaced honestly in `BuilderManifest.json`'s `version_targeting.schema_targets_referenced_but_not_yet_materialized` field rather than silently omitted or backfilled without flagging it.

**Recommendation:** author `void_world_location.schema.json` in a future pass, applying the same rigor already used for the twelve schemas in this package — build it, then actually run it against all five district `_data.json` files and fix whatever it catches, rather than writing it once and assuming correctness.

---

## 6. Backward Compatibility Policy

Per `Meridian_Master.json`: future generation versions must remain readable against this manifest's shape. Concretely, this means:

- New fields may be **added** to any schema in a future minor version without breaking this package.
- Existing required fields must **not** be removed or repurposed without a major version bump across every affected schema and its target file simultaneously.
- The five approved district packages and `Meridian_Master_Plan.md` are permanently locked at their current content — "backward compatibility" for this package means everything built on top of them stays compatible with them, never the reverse.

---

## 7. Upgrade & Migration Notes for Future Phases

- **Phase 2 (connective tissue):** any new road, transit, or generic residential content must extend `DistrictRegistry.json`, `RoadNetwork.json`, and `WorldPartition.json` — per `BuilderRules.json`'s `future_expansion_rules`, partial registration in only one file is insufficient.
- **Phase 3 (new districts):** any new district must pass all nine Non-Negotiable World-Design Constraints, receive writing-team sign-off per `Meridian_Master.json`'s `new_district_review_gate`, and extend every registry this package defines — not just the district-specific data file a new district package would bring with it.
- **Schema evolution:** if any of the twelve schemas in this package are revised, re-run the full cross-validation pass documented in `ValidationSchema.json` against all affected files before treating the revision as complete.

---

*End of CompatibilityMatrix.md.*
