# VOID: THE UNSEEN HAND
## Gameplay Package — Engineering Handoff

**Audience:** An engineer starting work on a Combat, AI, Missions, Dialogue, Economy, or Vehicles package, who needs to know what foundation already exists before writing a single line of implementation.

---

## 1. Purpose of This Document

This package is architecture, not implementation — interfaces, contracts, and extension points, never gameplay logic. This document explains how to actually build on top of it without violating the scope discipline that makes it useful as a foundation.

---

## 2. Purpose of Every File

| File | Purpose |
|---|---|
| `README.md` | Top-level orientation |
| `CHANGELOG.md` | Real construction history |
| `Gameplay_Master.json` | **Entry point.** Scope declaration, upstream dependency on locked Meridian_Master, canon references, import order |
| `GameplayManifest.json` | **The extension-point registry** — read this to know exactly what you're allowed to hook into |
| `GameplayRules.json` | Framework dependency graph, ID namespacing rules, scope enforcement rules |
| `GameplaySchema.json` | Consolidated schema for all eleven data files |
| `CharacterFramework.json` | Base entity interface, character registry, non-corporeal support |
| `PlayerFramework.json` | Identity selection, input mapping contract, camera mode contract |
| `InteractionFramework.json` | Interactable object contract, prompt system, dialogue/minigame stubs |
| `SaveFramework.json` | Persistent state registry, save slot contract, checkpoint trigger |
| `ProgressionFramework.json` | Progression resource registry, grant interface, threshold contract |
| `SkillFramework.json` | Skill node contract, tree registry, prerequisite graph rules |
| `AbilityFramework.json` | Ability contract, activation lifecycle, slot system, Echo Shift reference instance |
| `GameplayValidation.json` | The 8 formal cross-package validation rules |
| `GameplayCompatibility.md` | Schema/version/stability reference |
| `EngineeringHandoff.md` | This document |
| `Gameplay_Index.md` | *(pending — master file index, generated last)* |

---

## 3. How to Consume This Package

Unlike Meridian_Master, this package has no single "Builder" reading it automatically. **You**, the engineer building a future gameplay package, are the consumer. Concretely:

1. Read `Gameplay_Master.json` first — confirm your package's concept fits within `architectural_scope.in_scope` and doesn't touch anything in `explicitly_out_of_scope`.
2. Read `GameplayManifest.json` — find the specific `extension_points` your package needs. Note their `stability` value.
3. Read the framework file(s) that expose those extension points in full — don't guess their shape from the manifest summary alone.
4. Read `GameplayRules.json`'s `consumer_package_governance` before writing any code — it tells you what you're allowed and not allowed to do as a consumer.
5. Follow `GameplayManifest.json`'s `package_registration_protocol` to formally register your package's namespace prefix and consumed extension points.

---

## 4. How Validation Works

`GameplayValidation.json` defines 8 rules across 8 categories. The two most important for a new consumer package:

- **GVR-006 (cross-file referential integrity):** if your package references an ID from this foundation, that ID must actually exist in the real file — not be assumed from a description. This package's own construction caught three real cases where this needed checking, not assuming (CharacterFramework/PlayerFramework, SaveFramework/PlayerFramework, SkillFramework/ProgressionFramework).
- **GVR-007 (upstream canon fidelity):** if you cite Meridian_Master content, re-read the actual locked file at citation time. This package's own construction caught the risk directly — SaveFramework.json's tracked-variable citation was verified against the real file, not recalled from earlier in this same project, and it's the single most important check category for any package touching canon.

---

## 5. The Seven-Framework Dependency Order

```
character_framework (root)
    ├── player_framework
    │       ├── interaction_framework
    │       └── ability_framework
    │               └── skill_framework
    └── progression_framework
            └── skill_framework

save_framework depends on ALL SIX of the above (cross-cutting persistence layer)
```

If you're building a package that needs to reference multiple frameworks, this order tells you which ones are more foundational — `character_framework` is safe to depend on from anywhere; `save_framework` should generally not be depended on by anything except a true persistence concern, since it already sits at the top of this graph.

---

## 6. Extension Point Consumption Workflow

Example walkthrough for a hypothetical Combat package needing to add a new ability:

1. Check `GameplayManifest.json` — `EP-ABILITY-01` (`AbilityContract`) is the relevant extension point, `stability: stable`.
2. Read `AbilityFramework.json`'s `ability_contract` section in full — note the required fields (`ability_id`, `activation_contract_ref`, `resource_cost_ref`) and the `ABIL_` namespace rule.
3. Register a new `ABIL_` prefixed ID for your ability, following the same pattern as the existing reference instance (`ABIL_echo_shift`) — but note that instance is non-combat; your combat ability's *effect* (damage, targeting, etc.) is entirely your package's own content, never added to `AbilityFramework.json` itself.
4. Confirm your new ability doesn't violate Scope Enforcement Rule SR-03 by checking whether you've introduced any damage formula, hit detection, or targeting logic into the *foundation* files — that content belongs entirely in your own Combat package.

---

## 7. Scope Discipline — What NOT to Add

The single hardest thing about extending this package correctly: contracts for combat-adjacent, AI-adjacent, or dialogue-adjacent systems will keep *feeling* like they belong here, because they're related concepts. They don't. Concretely:

- An ability's *existence* and *activation lifecycle* → this package. An ability's *combat effect* → Combat package.
- A character's *entity registration* → this package. A character's *behavior* → AI package.
- An interaction's *trigger contract* → this package. An interaction's *dialogue content* → Dialogue package.
- A skill's *node structure and prerequisites* → this package. A skill's *specific combat bonus* → Combat package.

If you're unsure which side of that line something falls on, it almost always belongs to your own package, not this foundation — this foundation should feel slightly too generic to be interesting on its own. That's by design.

---

## 8. Future Package Onboarding Checklist

- [ ] Confirmed my package's content fits `architectural_scope.in_scope`
- [ ] Confirmed my package doesn't touch `explicitly_out_of_scope`
- [ ] Identified every extension point I need from `GameplayManifest.json`
- [ ] Read the full framework file(s) exposing those extension points, not just the manifest summary
- [ ] Requested and reserved a new namespace prefix, distinct from CHAR/PLYR/INTR/SAVE/PROG/SKIL/ABIL
- [ ] Verified my package doesn't introduce a cycle into `framework_dependency_graph`
- [ ] Re-read any Meridian_Master content I cite directly from the locked file, not from memory
- [ ] Ran my own package's manifest against `GameplayValidation.json`'s 8 rules

---

## 9. Version Compatibility

See `GameplayCompatibility.md` in full. This package targets `void_gameplay_master_schema_v1` and requires Meridian_Master at its locked 1.0.0 state.

---

*End of EngineeringHandoff.md.*
