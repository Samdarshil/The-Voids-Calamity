# VOID World Builder — Merge and Meridian database audit

## Scope
This source candidate combines Agent 1-3's Phase 3/validation/Metro/World Partition lineage with Agent 4-7's Building, District, Environment, Props and Lighting files. Historical Phase 1/2/3 snapshots are included for reference only; they must not overwrite the merged plugin. Canonical Meridian source files are copied under `Data/MeridianMaster/` for convenient, read-only input.

## Checks performed in packaging
- JSON syntax check across all JSON files in the supplied Meridian database.
- Direct JSON Schema validation for same-basename `*.json`/`*.schema.json` pairs where available.
- Cross-file district ID comparison for the five approved IDs.
- Source/module presence checks for Road, Metro, Building, District, Environment, Props, Lighting and Validation.
- ZIP integrity test.

## Not verified
- Unreal Build Tool compilation, Unreal Header Tool, plugin load, or Editor automation tests. No claim of UE 5.8.2 compile success is made.
- Full end-to-end generated-scene context between District -> Environment/Props/Lighting is not proven.
- Spatial realization is still an explicit first-look engineering task: canon supplies relative/topological constraints, not a final surveyed set of world transforms. Do not treat placeholder layout dimensions as canon.
- Collision, repeated-run idempotence, real assets, terrain snapping, and performance in the actual UE world remain to be tested.

## Important risks retained for tomorrow
1. Every generator must use one district-origin/world-transform mapping; current Agent 3 risk register flags roads spawning district-local positions at world identity.
2. Roads must be generated before buildings. Building shifts must be followed by neighbour-collision checks; Agent 4 reported this as unresolved.
3. Agent 4 reported duplicate roads on repeated direct runs. Run generators twice and compare actor counts.
4. Agents 6/7 may read imported `District.Roads/Buildings` rather than geometry synthesized by Agent 5. Verify the actual generated-scene data flow.
5. Meridian master package notes a missing standalone `void_world_location_schema_v1`; preserve as a documented schema gap, do not silently invent canonical fields.
6. `EngineVersion: 5.8.2` is descriptor metadata only, not proof of API compatibility.

## Database authority
Files under `Data/MeridianMaster/` are source inputs, not generated outputs. Never rewrite them to fit placeholder geometry. Put any layout tuning in a separate Builder configuration/override file.
