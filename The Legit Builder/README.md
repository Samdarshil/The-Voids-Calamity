# VOID World Builder — Merged UE 5.8.2 Candidate

**Status: source-integrated candidate; not yet Unreal-compiled or runtime-verified.**

## Included
- Agent 1–3 base: Core, Import, Roads, Validation, Metro, World Partition helpers.
- Agent 4: Building Generator.
- Agent 5: District Generator.
- Agent 6: Environment + Props.
- Agent 7: Lighting and presets.
- Meridian database snapshot under `Data/MeridianMaster/`.
- Historical Phase 1/2/3 and Agent 1/2/3 snapshots under `ReferenceSnapshots/` for audit only.
- Merge notes and static database checks under `Reports/`.

## Install for tomorrow's first build
1. Back up your project's existing `Plugins/VOIDWorldBuilder` folder.
2. Extract this ZIP. Copy the top-level `VOIDWorldBuilder` folder into `<YourProject>/Plugins/` so the descriptor is at `Plugins/VOIDWorldBuilder/VOIDWorldBuilder.uplugin`.
3. Open the `.uproject` in Unreal Engine 5.8.2 and allow project files to regenerate if prompted.
4. Build the Editor target before opening or generating a world. Fix all UHT/UBT errors first.
5. If it builds and loads, import the canonical Meridian package, run validation-only, and inspect every Error before generation.

## Do not skip these gates
- This package has **not** been compiled with UBT/UHT in UE 5.8.2.
- Do not generate the whole city before resolving the shared district-origin/world-transform contract.
- Generate roads before buildings; validate road clearance and building-to-building collisions.
- Repeat generation twice and compare actor counts to detect duplicates.
- District → Environment/Props/Lighting shared generated-scene data flow remains unverified.
- Placeholder layout settings are engineering controls, not canonical Meridian coordinates.

See `Reports/MERGE_AND_DATABASE_AUDIT.md` and `Reports/DATABASE_STATIC_CHECKS.md` before first use.
