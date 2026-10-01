# VOID: THE UNSEEN HAND
## Gameplay Package — README

**Version:** 1.0.0 | **Phase:** Phase 1 — Foundation | **Status:** In Progress

---

## 1. What This Package Is

The gameplay framework foundation for VOID: The Unseen Hand — interfaces, contracts, registries, lifecycle definitions, and extension points for player, character, interaction, save, progression, skill, and ability systems. It is consumed by future gameplay packages (Combat, AI, Missions, Dialogue, Economy, Vehicles), not by the VOID World Builder.

## 2. What This Package Is Not

- Not combat implementation, AI behavior, missions, dialogue, economy, or vehicles — those are separate, future packages.
- Not a modification of the locked Meridian_Master package — it is read-only upstream context here.
- Not narrative content of any kind. Every canon fact this package touches is cited to its source, never restated or extended.

## 3. Upstream Dependency

**Meridian_Master (locked, approved, 34 files, checksummed).** This package treats it exactly the way Meridian_Master itself treated Volumes I–IV: read, cited, never modified. See `Gameplay_Master.json` `upstream_dependency` for the full statement.

## 4. How to Use This Package

See `EngineeringHandoff.md` for the full onboarding guide. In short: read `Gameplay_Master.json`, then `GameplayManifest.json` for the specific extension points you need, then the relevant framework file(s) in full before writing any implementation.

## 5. Package Contents & Status

| File | Purpose | Status |
|---|---|---|
| `README.md` | This file | ✅ Complete |
| `CHANGELOG.md` | Version history | ⏳ Pending |
| `Gameplay_Master.json` | Entry-point manifest | ✅ Complete |
| `GameplayManifest.json` | Extension-point registry (25 points) | ✅ Complete |
| `GameplayRules.json` | Framework dependency graph, governance rules | ✅ Complete |
| `GameplaySchema.json` | Consolidated schema, all 11 data files | ✅ Complete |
| `CharacterFramework.json` | Base entity interface | ✅ Complete |
| `PlayerFramework.json` | Identity, input, camera contracts | ✅ Complete |
| `InteractionFramework.json` | Interactable/prompt/dialogue-stub contracts | ✅ Complete |
| `SaveFramework.json` | Persistent state registry | ✅ Complete |
| `ProgressionFramework.json` | Progression resource registry | ✅ Complete |
| `SkillFramework.json` | Skill node/tree contracts | ✅ Complete |
| `AbilityFramework.json` | Ability contract, Echo Shift reference instance | ✅ Complete |
| `GameplayValidation.json` | 8 formal validation rules | ✅ Complete |
| `GameplayCompatibility.md` | Schema/version/stability reference | ✅ Complete |
| `EngineeringHandoff.md` | Practical onboarding guide | ✅ Complete |
| `Gameplay_Index.md` | Master file index | ⏳ Pending |

**14 of 17 files complete.**

## 6. Version Compatibility

Targets `void_gameplay_master_schema_v1`, requires Meridian_Master at its locked 1.0.0 state. Full detail in `GameplayCompatibility.md`.

---

*Gameplay Package README — continuing with `CHANGELOG.md` next.*
