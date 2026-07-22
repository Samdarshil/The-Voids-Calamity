# VOID: THE UNSEEN HAND
## Meridian_Master Package — Master Index

**Package Version:** 1.0.0 | **Canon Version:** Volumes I–IV, Final | **Generated:** 2026-07-20
**This is the final file in the Meridian_Master package.**

---

## 1. Complete File Index

| # | File | Purpose | Builder Module | Depends On | Validation |
|---|---|---|---|---|---|
| 1 | `README.md` | Package orientation, status table | — | — | ✅ |
| 2 | `CHANGELOG.md` | Real construction history | — | — | ✅ |
| 3 | `Meridian_Master.json` | **Single Builder import manifest** | Import | README.md | ✅ Schema-validated |
| 4 | `Meridian_Master.schema.json` | Validates #3 | Import | #3 | ✅ Self-validates against #3 |
| 5 | `DistrictRegistry.json` | District hierarchy, relationships, priority | Validation, Road Gen, District Gen | #3 | ✅ Schema-validated |
| 6 | `DistrictRegistry.schema.json` | Validates #5 | — | #5 | ✅ |
| 7 | `RoadNetwork.json` | Road hierarchy, bridges, tunnels | Road Generation | #5 | ✅ Schema-validated |
| 8 | `RoadNetwork.schema.json` | Validates #7 | — | #7 | ✅ |
| 9 | `MetroNetwork.json` | Dual Live/Dead transit network | Metro Generation | #7 | ✅ Schema-validated |
| 10 | `MetroNetwork.schema.json` | Validates #9 | — | #9 | ✅ (1 fix applied) |
| 11 | `NavigationGraph.json` | Pedestrian/vehicle/metro/underground/rooftop rules | Navigation Generation | #5, #7, #9 | ✅ Schema-validated |
| 12 | `NavigationGraph.schema.json` | Validates #11 | — | #11 | ✅ |
| 13 | `GameplayGraph.json` | Mission relationships, state variables | Gameplay Generation | #5, #11 | ✅ Schema-validated |
| 14 | `GameplayGraph.schema.json` | Validates #13 | — | #13 | ✅ |
| 15 | `LandmarkRegistry.json` | Landmark ownership, visibility, priority | (supports Navigation, Gameplay) | #5, #11 | ✅ Schema-validated |
| 16 | `LandmarkRegistry.schema.json` | Validates #15 | — | #15 | ✅ |
| 17 | `InfrastructureGraph.json` | Power, water/climate, comms, maintenance | Infrastructure Generation | #5 | ✅ Schema-validated |
| 18 | `InfrastructureGraph.schema.json` | Validates #17 | — | #17 | ✅ |
| 19 | `WorldPartition.json` | Streaming cells, cell dependencies | World Partition | #5 | ✅ Schema-validated |
| 20 | `WorldPartition.schema.json` | Validates #19 | — | #19 | ✅ |
| 21 | `DataLayers.json` | 8 type categories + 30 gating layers + 1 cross-district layer | Data Layers | #19 | ✅ Schema-validated (1 arithmetic fix applied) |
| 22 | `DataLayers.schema.json` | Validates #21 | — | #21 | ✅ |
| 23 | `StreamingStrategy.json` | Loading, priority, transition, memory, performance | (supports World Partition, Data Layers) | #19, #21 | ✅ Schema-validated |
| 24 | `StreamingStrategy.schema.json` | Validates #23 | — | #23 | ✅ |
| 25 | `BuilderRules.json` | **Complete 15-step execution pipeline** | All phases | #5, #7, #9, #11, #13, #15, #17, #19, #21, #23 | ✅ Schema-validated (1 fix applied) |
| 26 | `BuilderRules.schema.json` | Validates #25 | — | #25 | ✅ |
| 27 | `BuilderManifest.json` | Runtime execution contract | Import, Final Validation | #3, #25 | ✅ |
| 28 | `PackageManifest.json` | Real SHA-256 checksums, canon-lock source | Final Validation | #27 | ✅ Independently spot-checked |
| 29 | `DependencyGraph.json` | Verified two-tier dependency DAG | Final Validation | #28 | ✅ Computationally verified (networkx) |
| 30 | `ValidationSchema.json` | 12 formal cross-package validation rules | Validation, Final Validation | — | ✅ |
| 31 | `CompatibilityMatrix.md` | Schema/version compatibility reference | — | — | ✅ |
| 32 | `EngineeringHandoff.md` | Practical onboarding for engineers | — | — | ✅ |
| 33 | `ValidationReport.md` | Final integrity sign-off, all checks re-run live | Final Validation | All above | ✅ PASS |
| 34 | `Meridian_Master_Index.md` | **This file** | — | All above | ✅ |

---

## 2. Generation Order Summary

**Macro (district/region) order** — authoritative in `Meridian_Master.json`:
Olympus Spire → Live Network spine → White Zones nodes → Undercroft substrate → Metro Archives → Sector 0.

**Micro (content-type) order, per region** — authoritative in `BuilderRules.json`:
Import → Validation → Roads → Buildings → District Detail → Metro → Infrastructure → Environment → Gameplay → Navigation → Lighting → World Partition → Data Layers → Final Validation → Playable Greybox Meridian.

**File construction order** (this package's own build history) — verified as a valid DAG in `DependencyGraph.json`, reproduced in full there.

---

## 3. Relationships at a Glance

- Every `.schema.json` file validates exactly one `.json` file, named identically minus the `.schema` infix.
- `Meridian_Master.json` is upstream of everything — no other file should be read by the Builder before it.
- `BuilderRules.json` is downstream of all ten content registries — it's the synthesis point where district, road, metro, navigation, gameplay, landmark, infrastructure, world partition, data layer, and streaming data all come together into one executable pipeline.
- `PackageManifest.json` → `DependencyGraph.json` → `ValidationSchema.json` form the integrity chain: what exists (checksummed) → how it depends on itself (verified DAG) → how to check it stays correct (formal rules).
- The three documentation files (`CompatibilityMatrix.md`, `EngineeringHandoff.md`, this index) are the human-facing layer sitting on top of all the machine-facing JSON.

---

## 4. Final Package Statistics

- **34 files total**, all present and accounted for.
- **12 core JSON registries**, each with its own schema, all 12 cross-validated live at final report time.
- **19 locked input files** (5 district `.md` + 5 `_data.json` + 3 schematic `.svg` + 4 `ValidationReport.md` + `Meridian_Master_Plan.md`), all checksummed, zero drift detected.
- **2 real bugs found and fixed** during construction: a schema/data field mismatch (`MetroNetwork.schema.json`), a terminal-step field mismatch (`BuilderRules.schema.json`), plus **1 arithmetic error** (`DataLayers.json`'s declared total).
- **7 open items carried forward honestly**, none silently resolved, full detail in `ValidationReport.md` Sec. 15.

---

## 5. Closing

This index is the last file in the Meridian_Master package. Everything above it was built the same way: real construction, real validation against real data, real bugs caught and fixed rather than assumed away, and real limits acknowledged rather than papered over. The single most important thing for a human reviewer to act on before this package sees engine time is still the same one it's been since `Olympus_Spire.md` Sec. 0.1 first surfaced it: the Spire/Nyx Server Hub discrepancy. Everything else here can proceed to greybox without further sign-off.

---

*End of Meridian_Master_Index.md.*
