# VOID: THE UNSEEN HAND
## Gameplay Package — CHANGELOG

---

## [Unreleased]
### In Progress
- `Gameplay_Index.md` — final master index

---

## [0.14.0] — 2026-07-30
### Added
- `README.md`, `GameplayCompatibility.md`, `EngineeringHandoff.md`, `GameplayValidation.json`
### Fixed
- `GameplayCompatibility.md`'s stable/provisional extension-point counts were originally miscounted as 18/7; verified against real `GameplayManifest.json` data and corrected to the actual 20/5.

---

## [0.11.0] — 2026-07-29
### Added
- All seven framework files: `CharacterFramework.json`, `PlayerFramework.json`, `InteractionFramework.json`, `SaveFramework.json`, `ProgressionFramework.json`, `SkillFramework.json`, `AbilityFramework.json`
### Notes
- Built in the requested order (Player before Character, Skill before Ability, Save before three of its six dependencies existed), which repeatedly created temporary cross-file dependency gaps. Each was flagged explicitly at the point it appeared and closed within one or two subsequent files — never left unresolved.
- `SaveFramework.json`'s citation of the six tracked state variables was verified directly against the real, current `Meridian_Master/GameplayGraph.json` file rather than recalled from memory — the single most important integrity check in this package, since it's the one place a citation drifting from locked canon would matter most.
- Final reconciliation after all seven frameworks existed confirmed: 25/25 extension points implemented, all `depends_on` fields matching the formal dependency graph exactly, all seven files schema-valid, the full dependency graph confirmed an acyclic DAG by computation, and all seven namespace prefixes unique.

---

## [0.4.0] — 2026-07-28
### Added
- `Gameplay_Master.json`, `GameplayManifest.json`, `GameplayRules.json`, `GameplaySchema.json`
### Fixed
- `Gameplay_Master.json`'s original `generation_pipeline.import_order` listed `player_framework` before `character_framework`, contradicting the dependency graph formalized minutes later in `GameplayRules.json` (Player depends on Character, not the reverse). Caught and corrected before any framework file was built.
### Notes
- `GameplaySchema.json` was built schema-first: three $defs entries validated immediately against files that already existed, eight $defs entries forward-defined (contract-first design) for files that didn't exist yet — each later validated for real as its file was built, with the schema itself corrected transparently where a forward-defined shape didn't quite match the real content, mirroring how `MetroNetwork.schema.json` and `BuilderRules.schema.json` were corrected in the Meridian_Master package.

---

*Gameplay Package CHANGELOG — continuing with `Gameplay_Index.md` next, the final file in this package.*
