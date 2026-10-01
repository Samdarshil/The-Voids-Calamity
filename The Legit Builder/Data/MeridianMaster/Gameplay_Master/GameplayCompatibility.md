# VOID: THE UNSEEN HAND
## Gameplay Package — Compatibility Matrix

**Package Version:** 1.0.0
**Phase:** Phase 1 — Foundation
**Generated:** 2026-07-20

---

## 1. Overview

Companion to `Gameplay_Master.json`'s `upstream_dependency` and `intended_ue5_systems` fields, and to `GameplayManifest.json`'s `compatibility_contract`. Presented here in scannable form for a reviewing engineer.

---

## 2. Schema Coverage Matrix

All eleven data files in this package are covered by a single consolidated schema, `GameplaySchema.json`, rather than one schema file per data file (a deliberate departure from the Meridian_Master package's pattern, appropriate given this package's smaller, tightly-interrelated file set).

| File | Schema `$defs` Entry | Status |
|---|---|---|
| `Gameplay_Master.json` | `gameplay_master` | ✅ Validated |
| `GameplayManifest.json` | `gameplay_manifest` | ✅ Validated |
| `GameplayRules.json` | `gameplay_rules` | ✅ Validated |
| `CharacterFramework.json` | `character_framework` | ✅ Validated |
| `PlayerFramework.json` | `player_framework` | ✅ Validated |
| `ProgressionFramework.json` | `progression_framework` | ✅ Validated |
| `InteractionFramework.json` | `interaction_framework` | ✅ Validated |
| `AbilityFramework.json` | `ability_framework` | ✅ Validated |
| `SkillFramework.json` | `skill_framework` | ✅ Validated |
| `SaveFramework.json` | `save_framework` | ✅ Validated |
| `GameplayValidation.json` | `gameplay_validation` | ✅ Validated |

All eleven were schema-first defined (three retroactively against files that already existed; eight forward-defined before their files existed) and every one now validates live against its real, final content — none remain in the "planned, not yet built" state `GameplaySchema.json` originally flagged them with.

---

## 3. Upstream Compatibility: Meridian_Master (Locked)

| Requirement | Status |
|---|---|
| Meridian_Master package version this package was built against | 1.0.0, all 34 files, checksummed |
| Modification policy | Never modified — verified by direct re-read of `Meridian_Master/GameplayGraph.json` at citation time (GameplayValidation.json rule GVR-007), not assumed unchanged |
| Canon facts referenced | Dual player identity, Echo Shift, non-corporeal companion support, six tracked state variables, memory fragment progression, single first-person moment — each cited to an exact source, never restated as new narrative content |

If Meridian_Master is ever revised, every citation in this package (`Gameplay_Master.json canon_references`, and every framework file's own `canon_references` block) needs re-verification against the new version before this package can be considered still-compatible.

---

## 4. Engine Version Targeting

**Engine:** Unreal Engine 5 (no specific point version pinned, consistent with the Meridian_Master package's own policy).

**Intended UE5 systems**, per `Gameplay_Master.json`:
- Gameplay Ability System — target for `AbilityFramework.json`'s activation lifecycle contract
- Enhanced Input — target for `PlayerFramework.json`'s input action mapping contract
- State Trees — candidate target for future state-heavy consumer packages (not directly implemented by this foundation)
- Modular Gameplay Features / Game Feature Plugins — candidate target for how future packages (Combat, AI, etc.) attach as separate modules
- Animation Blueprints — candidate target for `CharacterFramework.json`'s corporeal character rendering (not applicable to the non-corporeal support path)

None of these are implemented in this package — naming them is architectural scoping, matching the same discipline applied to `BuilderManifest.json`'s `required_plugin_capabilities` in the Meridian_Master package.

---

## 5. Extension Point Stability Matrix

Of the 25 extension points in `GameplayManifest.json`:

| Stability | Count | Examples |
|---|---|---|
| `stable` | 20 | IdentitySelectionInterface, BaseCharacterEntityInterface, AbilityContract, PersistentStateRegistry |
| `provisional` | 5 | CompanionAttachmentInterface, DialogueTriggerStub, MinigameInteractionStub, CheckpointTriggerInterface, ProgressionGrantInterface |

Provisional interfaces are architecturally complete but have no confirmed consumer package yet. A future package that becomes their first real consumer may prompt a refinement — this is expected, not a defect, per `GameplayManifest.json`'s own `stability_values` definitions.

---

## 6. Backward Compatibility Policy

Identical in spirit to the Meridian_Master package's policy:

- Additive changes (new optional fields, new provisional extension points) are always safe.
- Changing the shape of a `stable`-tagged extension point requires a major version bump to `Gameplay_Master.json`, `GameplayManifest.json`, and every framework file implementing that point, done together.
- This package's own files are never modified once a future consumer package has registered against them — the same immutability discipline this package itself applies to the locked Meridian_Master package.

---

## 7. Future Package Compatibility Requirements

Before Combat, AI, Missions, Dialogue, Economy, or Vehicles packages can be built against this foundation:

1. They must declare which `extension_points` (by ID) they consume, per `GameplayManifest.json`'s `package_registration_protocol`.
2. They must request a new namespace prefix (not reuse CHAR/PLYR/INTR/SAVE/PROG/SKIL/ABIL) before registering any new entity IDs.
3. They must not introduce a cycle into `GameplayRules.json`'s `framework_dependency_graph` — any new package's dependency on this foundation's frameworks must remain one-directional.
4. They must pass `GameplayValidation.json`'s eight rules against their own manifest before integration is considered complete.

---

*End of GameplayCompatibility.md.*
