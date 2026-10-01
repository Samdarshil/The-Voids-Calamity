# VOID: THE UNSEEN HAND
## Meridian_Master Package — Engineering Handoff

**Audience:** An engineer encountering this package for the first time, tasked with running the VOID World Builder against it.

---

## 1. Purpose of This Document

Everything else in this package is either data (JSON) or narrative reference (the five district `.md` files and `Meridian_Master_Plan.md`). This document is neither — it's the practical "how do I actually use this" guide, written for someone who needs to be productive with this package in the next hour, not someone learning Meridian's lore.

---

## 2. Purpose of Every File

| File | Purpose |
|---|---|
| `README.md` | Top-level orientation, file status table |
| `CHANGELOG.md` | Real version history of this package's construction |
| `Meridian_Master.json` | **The single file the Builder imports.** Project metadata, canon lock, registry references, generation pipeline |
| `Meridian_Master.schema.json` | Validates `Meridian_Master.json`'s structure |
| `DistrictRegistry.json` | The five districts: hierarchy, relationships, adjacency, generation priority |
| `DistrictRegistry.schema.json` | Validates `DistrictRegistry.json` |
| `RoadNetwork.json` | Road hierarchy, categories, bridges, tunnels, macro traversal graph |
| `RoadNetwork.schema.json` | Validates `RoadNetwork.json` |
| `MetroNetwork.json` | The dual Live/Dead transit network model |
| `MetroNetwork.schema.json` | Validates `MetroNetwork.json` |
| `NavigationGraph.json` | Pedestrian, vehicle, metro, underground, rooftop traversal rules |
| `NavigationGraph.schema.json` | Validates `NavigationGraph.json` |
| `GameplayGraph.json` | Mission relationships, tracked state variables, exploration loops |
| `GameplayGraph.schema.json` | Validates `GameplayGraph.json` |
| `LandmarkRegistry.json` | Every confirmed landmark, visibility tier, ownership |
| `LandmarkRegistry.schema.json` | Validates `LandmarkRegistry.json` |
| `InfrastructureGraph.json` | Power, water/climate, comms (the Weave), maintenance per district |
| `InfrastructureGraph.schema.json` | Validates `InfrastructureGraph.json` |
| `WorldPartition.json` | Streaming cells, cell dependencies, per district |
| `WorldPartition.schema.json` | Validates `WorldPartition.json` |
| `DataLayers.json` | 8 layer-type categories + all 30 district gating layers + 1 cross-district layer |
| `DataLayers.schema.json` | Validates `DataLayers.json` |
| `StreamingStrategy.json` | Loading, priority, transition, memory, and performance strategy |
| `StreamingStrategy.schema.json` | Validates `StreamingStrategy.json` |
| `BuilderRules.json` | **The complete 15-step execution pipeline**, error handling, recovery |
| `BuilderRules.schema.json` | Validates `BuilderRules.json` |
| `BuilderManifest.json` | Runtime execution contract: modes, flags, required plugin capabilities |
| `PackageManifest.json` | Real SHA-256 checksums for every file, canon-lock integrity source |
| `DependencyGraph.json` | Computationally-verified two-tier dependency DAG |
| `ValidationSchema.json` | The 12 formal cross-package validation rules, with real bugs cited as precedent |
| `CompatibilityMatrix.md` | Schema/version compatibility reference |
| `EngineeringHandoff.md` | This document |
| `ValidationReport.md` | *(pending — final integrity sign-off, generated after this document)* |
| `Meridian_Master_Index.md` | *(pending — master file index, generated last)* |

---

## 3. How the Builder Imports the Package

**One rule, stated everywhere in this package for a reason: `Meridian_Master.json` is the only file you import manually.** Everything else is resolved through it. Concretely:

1. Load `Meridian_Master.json`.
2. Read `registry_references` — this tells you every other file's name and whether it's required.
3. Follow `generation_pipeline.import_order` to load the referenced registries in the correct sequence.
4. Do not open `DistrictRegistry.json`, `RoadNetwork.json`, etc. directly before step 3 — their content is only meaningful once `Meridian_Master.json`'s context (canon lock, version compatibility) is already loaded.

If you find yourself writing code that opens a registry file without first loading `Meridian_Master.json`, stop — that's the one hard rule this package asks you not to break.

---

## 4. How Validation Works

`ValidationSchema.json` defines 12 formal rules across 7 categories (syntax, schema conformance, referential integrity, arithmetic consistency, canon compliance, structural integrity, checksum integrity). Run them in this order:

1. **Syntax (VR-001):** every `.json` file parses.
2. **Schema conformance (VR-002):** every data file validates against its schema — not just parses.
3. **Referential integrity (VR-003, VR-004):** every cross-file ID reference resolves.
4. **Arithmetic consistency (VR-005):** every declared total matches its actual sum.
5. **Canon compliance (VR-006, VR-007, VR-008, VR-012):** no locked file modified, no sixth district, no unresolved mystery answered, no fabricated numeric precision.
6. **Structural integrity (VR-009, VR-010):** the dependency graph is a DAG; all 10 district-pair relationships are covered exactly once.
7. **Checksum integrity (VR-011):** locked-input checksums match `PackageManifest.json`.

These aren't theoretical. Every one of VR-001 through VR-005 and VR-009 through VR-011 caught a real issue during this package's own construction — see each rule's `precedent` field in `ValidationSchema.json` for the specific bug. If you're setting up CI for this package, replicate those checks exactly; they're proven to catch real mistakes, not hypothetical ones.

---

## 5. Expected Builder Workflow

Per `BuilderRules.json`, generation runs as a **two-tier pipeline**:

- **Macro (outer loop):** six steps, region by region — Olympus Spire, then the Live Network spine, then White Zones nodes, then the Undercroft substrate, then Metro Archives, then Sector 0 last.
- **Micro (inner loop):** for each region, fifteen content-type steps — import, validation, roads, buildings, district-specific detail, metro, infrastructure, environment, gameplay, navigation, lighting, world partition, data layers, final validation, terminal greybox state.

Steps 1, 2, 14, and 15 of the micro pipeline run once globally. Steps 3–13 run per macro region. See `BuilderRules.json` `execution_order.description` for the exact phrasing this rule is drawn from.

**Recommended first run:** use `BuilderManifest.json`'s `dry_run` execution mode before `full_generation`. It logs the planned order without writing anything — cheap insurance against discovering a dependency problem after committing engine resources.

---

## 6. Error Handling & Recovery

Three tiers of flagged content, each handled differently — do not treat them uniformly:

- **Highest priority (the Spire/Nyx discrepancy):** HALT generation of `olympus_spire_server_hub` combat content specifically. The room itself generates normally; only its encounter design is blocked.
- **Moderate priority (the Archive Maintenance Directive):** generate normally, flag for a feasibility review pass before final polish.
- **Low priority (three structural-interpretation flags):** generate normally, log a sign-off reminder, never block the pipeline.

On any HALT, the Builder does not auto-resume — per `BuilderRules.json` `recovery_rules.manual_override`, a human has to clear it. On partial failure, resume from the last completed micro-step within the current region; do not restart already-completed macro regions.

---

## 7. Future Package Updates

- **Phase 2 (connective tissue):** extend `DistrictRegistry.json`, `RoadNetwork.json`, and `WorldPartition.json` together — never just one.
- **Phase 3 (new districts):** requires writing-team sign-off (`Meridian_Master.json` `new_district_review_gate`) and must satisfy all nine World-Design Constraints before registry extension.
- **Any update to a schema file:** re-run the full `ValidationSchema.json` pass against every file that schema governs before considering the update complete.

---

## 8. Schema Migration Strategy

See `CompatibilityMatrix.md` Sec. 6 for the full backward-compatibility policy. The short version: additive changes to any schema are safe; removing or repurposing a required field needs a coordinated version bump across the schema and every file it validates, done together, not staggered.

---

## 9. Version Compatibility

See `CompatibilityMatrix.md` in full. One item worth surfacing here directly: `void_world_location_schema_v1` — the schema all five district `_data.json` files declare — was never actually authored as a standalone file. It's a real, flagged gap, not an oversight papered over. If you're the one who ends up writing it, `ValidationSchema.json`'s rules are a reasonable starting template for the rigor to apply.

---

## 10. Quick-Start Checklist

If you're picking this package up cold and need to be productive today:

1. Read `Meridian_Master.json` first — it's the map of everything else.
2. Skim `DistrictRegistry.json` to understand the five districts' relationships before touching any other registry.
3. Run the `ValidationSchema.json` checks against the whole package before writing any generation code — confirm you're starting from a known-good state.
4. Use `dry_run` mode before `full_generation`.
5. Read `BuilderRules.json` `error_handling` before your first real run — know what HALTs and why before you hit one.
6. If anything looks inconsistent that isn't already flagged in a `discrepancy_note`, `flagged_open_items`, or a validation rule's `precedent` field, it's probably a genuine new finding worth raising, not a misunderstanding on your part — this package has a track record of surfacing real issues rather than hiding them, and that standard should hold going forward.

---

*End of EngineeringHandoff.md.*
