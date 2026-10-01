# THE VOID'S CALAMITY
## GAMEPLAY DESIGN MASTER — PHASE 1
### Canon Extraction + System Design

**Status:** Design authority document. Not implementation. Not the JSON Gameplay Package.
**Relationship to existing work:** This project already has an approved `Gameplay_Master.json` and full 17-file Gameplay Foundation package (Combat/AI/Missions/Dialogue/Economy/Vehicles explicitly excluded, same as here). This document is the source-grounded design rationale underneath that architecture — written after it chronologically, but positioned to read as if it came first, since that's the more honest way to present analysis of something already built. Where this analysis reveals the existing package assumed something canon doesn't actually establish, that's flagged explicitly, not smoothed over.
**Status legend used throughout:** `CANON` (explicitly stated in Volumes I–IV or an approved package) · `INFERRED` (reasonably implied, not stated outright) · `PROPOSED` (a reasonable design suggestion where the source is silent) · `UNDEFINED` (no basis exists; a decision is required) · `CONFLICTING` (two source passages disagree).

---

## 1 — GAMEPLAY VISION

**Player fantasy:** A ghost with nothing left to lose, hired for a job that turns into the only honest question left in a city built on a comfortable lie. `CANON` — Vol II Prologue: *"My name doesn't matter. They call me a ghost... I don't ask questions. Until the day a question asked me."*

**What the player should feel:** Competence under pressure that curdles into obligation, then grief, then a question the game genuinely will not answer for them. `CANON` — Vol I, 5.1: motivation arc is "Credits, first... Then truth... Then, underneath both of those, something closer to grief."

**What makes the gameplay distinct:** The player's core tool is *information asymmetry*, not firepower. Every confirmed mission (Tutorial Contract, Contract 2, Contract 3, Mission 5, Mission 7) foregrounds hacking, stealth, and reading the environment over direct confrontation. `CANON` — five for five confirmed missions lead with a non-combat verb in their Gameplay Purpose field.

**What makes Meridian a gameplay space rather than a setting:** The five approved districts (Undercroft, White Zones, Metro Archives, Sector 0, Olympus Spire) were independently built to each occupy a *distinct primary gameplay register* — analog evasion, social stealth, environmental puzzle, pure exploration, advanced social stealth — with an explicit no-overlap rule. `CANON` — `GameplayGraph.json` (Meridian_Master, locked), `register_overlap_policy`.

**What makes the experience systemic:** Six tracked state variables (`nyx_status`, `mira_saved`, `memory_fragment_count`, `compassion_flags`, `riggs_redemption_complete`, `talis_extracted`) propagate consequences across acts, missions, and the ending itself. `CANON` — Vol IV Ch. 2; `GameplayGraph.json`.

**What the player should spend most of their time doing:** Reading rooms, NPCs, and terminals for what they're not saying, then deciding how loud to be about acting on it. `CANON` — Vol I, 7.2: "Loadout choice before each contract is both a gameplay system and a character one — the player is, mission after mission, choosing how loud or how quiet they want their war with VOID to be."

**What the player should NOT be doing:** Engaging in sustained, high-frequency combat as the primary activity. Of five fully-built missions, only two (Contract 3's escape, Mission 5's branch) have any combat content at all, and two districts (Metro Archives, Sector 0) have a hard zero-combat design constraint. `CANON` — `Metro_Archives.md` Sec. 23; `Sector_0.md` Sec. 25.

---

## 2 — CORE GAMEPLAY LOOP

```
PLAYER INTENT (a contract, a question, curiosity)
      ↓
PLAYER ACTION (hack, sneak, talk, read the room)
      ↓
WORLD RESPONSE (VOID notices or doesn't; an NPC reacts; a room resets)
      ↓
RESULT / CONSEQUENCE (exposure risk changes; a tracked variable updates)
      ↓
REWARD / INFORMATION / PROGRESSION (a Memory Fragment; a truth; credits)
      ↓
NEW PLAYER OPPORTUNITY (the next contract, now harder to walk away from)
```
`INFERRED` — no single canon passage states this loop explicitly, but it is the consistent shape of every confirmed mission's Objectives → Discovery → Ending → Connection-to-Next-Mission structure (Vol II, all missions).

**Short-term loop:** Enter a space → read its threat model (patrol pattern, reset tier, behavioral screening) → resolve one interaction (hack, dialogue, minigame) → advance or retreat. `CANON`, drawn from Tutorial Contract and Contract 2's stated Gameplay Purpose fields.

**Mid-term loop:** Complete a mission → return to a hub (Riggs' safehouse pre-Act 2; Sector 0 from Act 2 onward) → absorb a connective scene that recontextualizes what just happened → select the next contract. `CANON` — Vol II hub structure; `GameplayGraph.json` `district_progression`.

**Long-term loop:** Accumulate Memory Fragments and compassion-flagged choices across the whole campaign → the accumulation silently gates whether a third ending option (TRANSCEND) even appears at the Final Decision. `CANON` — Vol IV Ch. 2, 2.2 (AND-condition threshold).

**How the loops connect:** The short-term loop's individual choices (spare or kill, explore or rush, loud or quiet) are what the long-term loop is silently counting. `INFERRED` from the explicit threshold mechanism plus the explicit design intent note in Vol IV: *"TRANSCEND should feel earned through consistent behavior across the whole game, not achievable via a single late checklist run."*

---

## 3 — PLAYER VERBS

| Verb | Purpose | Preconditions | Result | Systems Involved | World Response | Progression Link | Status |
|---|---|---|---|---|---|---|---|
| Hack (minigame) | Bypass digital security | Hacking deck equipped | Access granted/denied | InteractionFramework, AbilityFramework | Security tier may escalate | None directly | `CANON` — Tutorial Contract |
| Stealth takedown | Neutralize a threat quietly | Line of sight, undetected | Target incapacitated | InteractionFramework | Detection risk stays low | Non-lethal KO unlocked at max Echo level | `CANON` — Tutorial Contract; Addictive Hooks |
| Camera-loop manipulation | Create a blind window | Hacking deck equipped | Timed safe window | InteractionFramework | Loop anomalies over 4 seconds get flagged (Vol II, Riggs' warning) | None | `CANON` |
| Lockpick minigame | Open a sealed container | Physical proximity | Contents revealed | InteractionFramework | None described | Unlocks optional content (e.g. subway map) | `CANON` — Metro Archives exhibit case |
| Echo Shift (ability) | Remove self from VOID's perception temporarily | Unlocked at Act 2 hub | Pass monitored barriers | AbilityFramework | Perception drops to zero for duration | Grows more capable as Echo strengthens | `CANON` — Vol I 7.4 |
| Dialogue choice | Shape relationships and information | Conversation active | Branching outcome | InteractionFramework (stub only) | Reputation/relationship shifts | Feeds `compassion_flags` | `CANON` — multiple missions |
| Lethal / non-lethal resolution | Resolve a combat-eligible encounter | Combat-eligible target | Target dead or spared | AbilityFramework, future Combat package | Tracked variable set (e.g. `nyx_status`) | Gates ending content | `CANON` — Mission 5 branch |
| Social stealth (disguise) | Move through screened, elite spaces undetected | Disguise active | Passage without alarm | InteractionFramework, PlayerFramework | Behavioral (not credential) screening evaluates the player | None directly | `CANON` — Mission 7; White Zones/Spire security doctrine |
| Environmental exploit | Use physical infrastructure against a threat | Exploitable object present | Threat neutralized indirectly | InteractionFramework | None described beyond the immediate | None | `CANON` — Tutorial Contract (coolant line, hacked door) |
| Explore (optional) | Discover Memory Fragments, lore, secrets | None | Collectible found | ProgressionFramework | None | Feeds `memory_fragment_count` | `CANON` — pervasive across all districts |
| Traverse (rooftop/tunnel) | Move through analog, off-grid space | District-specific (Undercroft primary) | Position changed, off Live Network | NavigationFramework (unbuilt combat/AI-adjacent territory) | None — this *is* the off-grid safety mechanism | None | `CANON` — Undercroft Sec. 13 |
| Combat (direct) | Resolve a boss-scale threat | Boss encounter active | Encounter won or lost | Future Combat package | Adaptive bosses "learn" player patterns | None confirmed beyond mission gating | `CANON` for existence; mechanics `UNDEFINED` beyond named boss flavor text |

**Verbs explicitly NOT supported by any source material:** crafting, vehicle operation as a primary verb (drone-taxis are described as VOID-synchronized ambient infrastructure, never player-piloted, `CANON` Vol I 3.5), currency-spending as an ability-purchase mechanic (`UNDEFINED`, see Section 20).

---

## 4 — PLAYER CAPABILITIES

**Foundational capabilities** (available from the start or established as baseline):
- Basic movement, interaction, and observation. `INFERRED` — required by every mission but never itemized as a capability list anywhere in canon.
- Hacking deck with modular software loadouts: viruses (disruption), ICE-breakers (confrontation), passive tools (camera loops, credential spoofing). `CANON` — Vol I, 7.2.
- Dual playable identity (Kaevon or Veronica), selected at session start, cosmetic/performative difference only per available evidence. `CANON` — Vol IV Canon Lock.

**Unlockable / progressive capabilities:**
- Echo Shift — granted at the Act 2 Hub Upgrade, not available before. `CANON` — Vol II.
- Non-lethal "remember" takedown — unlocked at max Echo level (fragment-driven). `CANON` — Addictive Hooks.
- Absolution weapon mod — unlocked via completing SQ2. `CANON` — Vol III.
- Riggs' Redemption vignettes — playable-as-Riggs sequences using "restricted, period-appropriate tools," implying the player's own capability set does *not* apply during these — a distinct, temporary capability profile. `CANON` — Vol III SQ2.

**Explicitly `UNDEFINED`:** Whether stealth, combat, and hacking each have their own independent progression track, a shared one, or no formal "skill" progression at all beyond the two confirmed unlocks above. Canon confirms *outcomes* (non-lethal takedown becomes available) without ever confirming a *mechanism* (a skill tree, a stat increase, an equipment tier). The existing Gameplay Package's `SkillFramework.json` and `AbilityFramework.json` define the *interface* for such a system without asserting it's the one canon actually uses — this is worth restating here plainly: **that architecture is `PROPOSED`, not `CANON`**, and should not be read as confirming a skill-tree progression model exists in the approved narrative.

---

## 5 — WORLD INTERACTION

| Target Type | Player → Interaction → Target → System Response → World State Change | Status |
|---|---|---|---|
| Doors / access points | Player hacks or disguises past → door → security tier check → passage granted, possible flag raised | `CANON` |
| Terminals (Metro Archives) | Player reads terminal → Archive Maintenance Directive → content may already have reset | `CANON` — Metro_Archives.md Sec. 9 (flagged as this package's highest-narrative-weight new interpretation, not pure canon — see Section 19) |
| NPCs (dialogue-capable) | Player approaches → interaction trigger → dialogue branch → relationship/reputation shift | `CANON` for existence, `UNDEFINED` for the general-case mechanism beyond named story beats |
| Enforcer units | Player's pattern crosses a predictive-flagging threshold → VOID escalates → patrol response | `CANON` — Vol I, 6.2 |
| Metro (Live Network) | Player boards at a station → VOID's traffic-cognition layer tracks position | `CANON` — Vol I, 3.5 (real-time tracking of every registered vehicle) |
| Metro (Dead Network) | Player traces relays through dead infrastructure → no tracking exists | `CANON` — Vol I, 3.5; Contract 3 |
| Sealed exhibit cases / minigame objects | Player triggers minigame → success/failure → contents revealed or interaction fails | `CANON` |
| Restricted areas (White Zones, Spire) | Player's behavior (not credentials) evaluated → escalation if flagged | `CANON` — "the Spire doesn't screen faces at the door, it screens behavior" (Undercroft connective scene, locked) |
| Vehicles | No player-operated vehicle interaction exists in any confirmed source | `UNDEFINED` / likely `not applicable` per current canon |

---

## 6 — GAMEPLAY STATES

| State | Entry Condition | Exit Condition | Allowed Actions | Restricted Actions | Status |
|---|---|---|---|---|---|
| Exploration | Default state in any district | Interaction/mission/combat trigger | Movement, observation, optional interaction | — | `INFERRED` |
| Interaction | Player triggers an interactable | Resolution (success/fail/cancel) | Dialogue, minigame input | Movement (implied) | `CANON` for existence |
| Stealth | Player enters a monitored/patrolled space | Detection or safe exit | Evasion verbs, camera-loop, hiding | Loud/direct verbs (self-defeating, not mechanically blocked) | `CANON` — pervasive |
| Combat | A boss encounter or Enforcer response triggers | Victory, defeat/capture, or de-escalation | Combat-eligible verbs | — | `CANON` for existence, mechanics `UNDEFINED` |
| Pursuit / Escape | A chase sequence triggers (e.g. Contract 3 subway) | Escape (success) or capture (failure, non-lethal) | Timed movement/evasion | — | `CANON` — Contract 3 explicitly |
| Dialogue | Conversation triggered | Dialogue tree resolves | Dialogue choice selection | Most other verbs | `CANON` for existence |
| Mission (active objective) | Contract/Mission accepted | Objective complete or abandoned | Mission-scoped verbs | Mission-locked areas may restrict travel | `INFERRED` from mission structure |
| Failure / Recovery | Combat loss, detection escalation past a threshold | Respawn/retry/capture-loop | Limited (loading, retry) | Most verbs | `CANON` for the one confirmed instance (Contract 3 capture-loop); general case `UNDEFINED` |

**State-transition model:** `INFERRED` overall shape — Exploration is the hub state every other state returns to; Interaction, Stealth, and Dialogue are frequently nested inside Exploration; Combat and Pursuit are escalations that can resolve back into Exploration (success) or into Failure/Recovery (loss); Mission state wraps the others without replacing them. No canon source diagrams this explicitly — this model is a synthesis, not a citation.

---

## 7 — SYSTEM INTERACTION ARCHITECTURE

```
PLAYER
  ↓
ACTION (verb from Section 3)
  ↓
GAMEPLAY SYSTEM (Interaction / Ability / Character, per the existing Gameplay Package)
  ↓
WORLD EVENT (VOID perception update, NPC reaction, environmental reset)
  ↓
OTHER SYSTEMS (Progression reads the event for fragment/threshold updates; Save framework flags state-eligible changes)
  ↓
STATE CHANGE (a tracked variable updates, or a local/systemic/persistent consequence per Section 8)
  ↓
PERSISTENCE (SaveFramework's PersistentStateRegistry, per the existing Gameplay Package)
```

**Boundary note:** This diagram describes responsibility flow, not implementation. The existing Gameplay Package's seven frameworks (`CharacterFramework`, `PlayerFramework`, `InteractionFramework`, `AbilityFramework`, `SkillFramework`, `ProgressionFramework`, `SaveFramework`) map cleanly onto this diagram's boxes; Combat, AI, Mission, and Narrative systems are deliberately absent from that package and from this diagram's implementation, present only as the "OTHER SYSTEMS" and "WORLD EVENT" boxes' eventual owners.

---

## 8 — WORLD REACTIVITY

**Local consequences** (immediate, scene-scoped): A camera loop over four seconds gets flagged (Vol II); an ICE Sentinel's patrol reroutes after a hacked door (Vol II). `CANON`.

**Systemic consequences** (cross-scene, same session): VOID's predictive-flagging model escalates enforcement response based on accumulated pattern, not single incidents (Vol I, 6.2). Nyx's presence gets "flagged for the rest of the Act" after the Contract 3 encounter (Vol II). `CANON`.

**Persistent consequences** (cross-act, campaign-length): The six tracked state variables (Section 14) — `nyx_status` alone affects the Act 3 hub scene, Mission 8's siege difficulty, and the Final Boss's Phase 2 dialogue. `CANON` — Vol IV Ch. 2.

**Explicitly NOT invented here:** A generic "world heat" or "wanted level" meter. No canon source describes one; the closest analogue is VOID's predictive-flagging model, which is pattern-based and district-specific (see each approved district's own Security Doctrine section), not a single global numeric value. Treating it as a simple meter would be `PROPOSED`, not `CANON` — flagged in Section 20 as a decision point.

---

## 9 — PROGRESSION

**Progression philosophy, as evidenced:** Progression is almost entirely *narrative-gated and choice-gated*, not stat-gated. The only confirmed numeric-feeling progression mechanism is Memory Fragment accumulation feeding a percentage threshold (80% of total placed) for the TRANSCEND ending. `CANON` — Vol IV Ch. 2.2.

**Confirmed progression elements:**
- Memory Fragment collection → Echo's ability growth (unspecified mechanism) and ending-threshold contribution. `CANON`.
- Compassion-flagged dialogue choices → ending-threshold contribution. `CANON`.
- Reputation with Undercroft NPCs → permanent minor buff (unspecified mechanical effect). `CANON` — Vol III SQ1, mechanism `UNDEFINED`.
- Riggs' Redemption completion → unique weapon mod (Absolution) and a recontextualized late-campaign line. `CANON`.

**Explicitly NOT supported by any source and therefore NOT assumed here:** experience points, character levels, a named skill tree, a currency-for-upgrades economy. Credits exist narratively (the 5-million-credit bounty, Undercroft off-ledger economy) but are never once described as spendable on player capability upgrades — only as a narrative/economic concept. Treating credits as an upgrade currency would be inventing an economy system explicitly out of scope for this design phase and the existing Gameplay Package alike. `UNDEFINED` — recorded formally in Section 20.

---

## 10 — FAILURE AND RECOVERY

**The one fully confirmed failure state:** Contract 3's subway escape sequence — "failure results in capture rather than death, looping the player back to the tunnel entrance with Nyx's presence flagged for the rest of the Act." `CANON` — Vol II. This is a soft-fail, no-permadeath, consequence-bearing loop, not a hard game-over.

**The KB's own acknowledgment of an open question:** the `mira_saved` tracked variable's documentation states it is "tracked separately in case a future difficulty mode allows mission failure states" — meaning the canon material itself treats general mission failure as a *future, undetermined* design space, not a solved one. `CANON` (the acknowledgment itself is canon, even though the underlying system is not) — Vol IV Ch. 2.

**Combat failure/death:** No confirmed source states what happens if the player loses a boss encounter or dies in combat. `UNDEFINED`.

**Stealth detection failure:** No confirmed source states a generalized consequence for being detected outside the one Contract 3 instance. Individual missions imply escalation (e.g. Mission 5's Enforcer response) but never describe a hard fail state distinct from combat engagement. `INFERRED` that detection typically transitions into Combat or Pursuit state (Section 6) rather than an independent fail state.

**Checkpoints:** No confirmed source describes a checkpoint system. `UNDEFINED`.

---

## 11 — MISSION GAMEPLAY MODEL

Canon itself supplies an explicit, reusable mission template — every confirmed mission in Volume II uses the same field structure: **Story Purpose, Gameplay Purpose, Objectives, NPC Interactions, Important Dialogue, Boss (if any), Mission Ending, Connection to Next Mission, Optional Exploration & Environmental Storytelling.** `CANON` — this is not a synthesis, it is the literal repeated structure of every mission entry in Volume II.

**Generic mission-state model, derived from that template:**

```
[LOCKED] → (prerequisite met, per Vol IV Mission Flow table)
    ↓
[AVAILABLE] → (player accepts)
    ↓
[ACTIVE] → objectives tracked, NPC interactions available
    ↓
    ├─→ [BOSS ENCOUNTER] (if applicable) → win/lose
    ↓
[COMPLETE] → Mission Ending fires → tracked variables update (if applicable)
    ↓
[CONNECTED] → Connection to Next Mission unlocks the next node
```
`INFERRED` structure, `CANON` field content.

**Branches:** Confirmed only where canon explicitly branches (Mission 5's spare/kill; the Final Decision's three options). Not every mission branches — most have a single linear Objectives → Ending path. `CANON`.

**Side quests:** Explicitly non-blocking — "Side Quests 1–4 can be started and completed any time within their listed window and do not block progression on the main numbered path." `CANON` — Vol IV Ch. 3.

**World-state integration:** Missions read and write tracked state variables per Section 14; the mission-state model above should be understood as sitting *inside* the broader `GameplayGraph.json` mission cross-reference structure already established in the locked Meridian_Master package, not replacing it.

---

## 12 — AI / NPC GAMEPLAY RELATIONSHIP

**Detection/threat model:** VOID-driven predictive flagging, not investigation-based AI. "Officers rarely investigate; they respond to a probability score VOID has already calculated." `CANON` — Vol I, 6.2. This is a significant, specific design constraint: a future AI package should not build patrol AI that "notices" the player through simulated perception alone — the confirmed model is pattern-recognition against an already-computed score.

**Adaptive bosses:** Kestrel "uses adaptive combat patterns, learning from player tactics" (Vol II, Mission 6); VOID itself "adapts, learning patterns" in the Final Boss (Vol II). `CANON` for existence of adaptive AI in at least two confirmed encounters; the learning *mechanism* is `UNDEFINED`.

**Faction relationships:** World Council Enforcement, Riggs' Underground Network, the Historical Society, Stellar Holdings — four confirmed factions with described but not mechanically specified relationships to the player. `CANON` for existence (Vol I Ch. 6), `UNDEFINED` for any reputation-with-faction mechanic beyond the Undercroft-specific one confirmed in Section 9.

**NPC schedules:** No confirmed source describes NPC daily schedules or routines as a system. Ambient civilian behavior is described per-district in the locked Meridian_Master package (e.g. White Zones' queue culture) but explicitly as environmental flavor, not a simulated schedule system. `UNDEFINED` whether a full NPC schedule system is intended.

**Gameplay requirements this design phase can state without building AI:** enemy/NPC entities must be able to (a) read a VOID-computed flag score rather than simulate independent perception, (b) support an "adaptive" tag for specific named bosses, (c) support non-lethal-only resolution for at least one confirmed enemy type (the Erased Legion, who "cannot be killed with brute force"). `CANON`-grounded requirements list, `PROPOSED` as a requirements framing.

---

## 13 — NARRATIVE / GAMEPLAY INTEGRATION

```
NARRATIVE
   ↕ (dialogue choices set compassion_flags; mission outcomes set nyx_status, mira_saved, etc.)
GAMEPLAY
   ↕ (tracked variables gate content: Act 3 hub scene variant, Mission 8 difficulty, ending options)
WORLD STATE
```
`CANON` — this loop is the explicit function of the six tracked state variables (Vol IV Ch. 2) and the Nyx Status Dialogue Substitution Rule ("Any connective scene written with Nyx present has an Echo-voiced alternate version if `nyx_status = KILLED`... the scene must never play with Nyx absent and unaddressed" — Vol IV, 5.2).

**Player choices with confirmed gameplay consequence:** spare/kill Nyx (Mission 5); exploration thoroughness (Memory Fragment count); dialogue compassion flags (Missions 4, 5, 7 and side quests); SQ2/SQ4 completion. `CANON`.

**What this document does not do:** invent new dialogue, new branches, or new narrative consequences beyond what's cited above. Any future Dialogue package's actual conversation content is out of scope here, consistent with the existing Gameplay Package's own scope boundary.

---

## 14 — PERSISTENCE

**Must be persisted (CANON, explicit):**
- The six tracked state variables: `nyx_status`, `mira_saved`, `memory_fragment_count`, `compassion_flags`, `riggs_redemption_complete`, `talis_extracted`. Vol IV Ch. 2; already the authoritative content of the existing `SaveFramework.json`'s `PersistentStateRegistry`.
- Player identity selection (Kaevon/Veronica), set once at session start. `CANON`.
- Side quest completion state (SQ1–SQ4), since side quests are explicitly non-blocking and re-visitable across a wide window. `INFERRED` from their described structure.
- Interaction completion state, to prevent re-triggering one-time interactions. `INFERRED`.

**Temporary state (not required to persist across sessions):** in-progress mission objective sub-state, active dialogue tree position, active minigame state. `INFERRED` — these are mid-scene states with no confirmed cross-session relevance.

**Explicitly `UNDEFINED`:** whether NPC-level world state (e.g. individual civilian reactions, non-tracked NPC dialogue history) persists at all, or resets between sessions. No source confirms either answer.

---

## 15 — EMERGENT GAMEPLAY

Canon supports few but real systemic interactions:

**SYSTEM A (loud playstyle choice) + SYSTEM B (VOID predictive flagging) + WORLD STATE (accumulated pattern) = escalated enforcement response mid-mission**, even without a single triggering incident. `CANON` — Vol I 6.2's pattern-based model implies this, though no specific mission scripts it as a generic emergent system.

**SYSTEM A (Memory Fragment collection) + SYSTEM B (compassion-flagged choices) + SYSTEM C (Nyx spared) + WORLD STATE (all three thresholds met) = a fourth ending option appears that would not otherwise exist.** `CANON` — Vol IV Ch. 2.2, the clearest, most explicit emergent-systems interaction in the entire source material.

**SYSTEM A (Echo Shift ability) + SYSTEM B (district-specific Weave coverage, per Meridian_Master) + WORLD STATE (approaching a monitored barrier) = passage through content otherwise inaccessible.** `CANON`, cross-referencing the locked Meridian_Master package directly.

No other emergent interactions are manufactured here — the source material is sparse on this point, and inventing additional "systemic combos" would violate the explicit instruction against manufacturing artificial complexity.

---

## 16 — ACCESSIBILITY / PLAYER OPTIONS

No confirmed source material addresses accessibility. This entire section is `UNDEFINED`, with one narrow exception worth flagging: the KB's `mira_saved` note (Section 10) explicitly anticipates "a future difficulty mode," meaning at least one accessibility-adjacent axis (difficulty, and by extension failure-state severity) is *acknowledged as future scope* by the source material itself, rather than simply absent. Recorded formally in Section 20.

---

## 17 — SYSTEM BOUNDARIES

| Domain | Owns | Explicitly Does Not Own |
|---|---|---|
| **Gameplay Foundation** (existing package) | Player/Character entity architecture, interaction contracts, save/progression/skill/ability *interfaces* | Any specific combat, AI, mission, dialogue, economy, or vehicle content |
| **Combat** (future) | Damage resolution, hit detection, boss mechanics implementation, adaptive-AI combat logic | Ability *existence* (Foundation owns the contract) |
| **AI** (future) | NPC behavior, patrol logic reading VOID's predictive flags, adaptive learning implementation | Character entity registration (Foundation owns it) |
| **Missions** (future) | Objective scripting, mission-state implementation, trigger placement | The generic mission-state model (this document defines it; Missions implements against it) |
| **Dialogue / Narrative** (future) | All actual conversation content, branching text | Dialogue *trigger contracts* (Foundation owns the stub) |
| **Vehicles** (future, if ever built) | N/A per current canon — no player-operated vehicle content exists to build against | — |
| **Inventory / Economy** (future, if ever built) | Currency mechanics, if any are ever designed | Progression resource tracking (Foundation's `ProgressionFramework` already owns Memory-Fragment-style resources) |
| **UI** (future) | Prompt visuals, HUD, menu presentation | Interaction/prompt *trigger* contracts (Foundation owns them) |
| **Audio** (future) | All sound implementation | — |
| **Animation** (future) | All character/ability animation | — |
| **Cinematics** (future) | Cutscene direction and playback | — |
| **World Builder** (existing, locked) | World geometry, districts, streaming | Nothing gameplay-related — strict one-directional read-only relationship, confirmed throughout the existing Gameplay Package |

This table is `PROPOSED` in its exact shape but grounded directly in the existing, already-approved Gameplay Package's `architectural_scope` — it is restated here for this document's own completeness, not newly invented.

---

## 18 — FUTURE PACKAGE DEPENDENCIES

```
Gameplay Foundation (existing)
        ↓
   ┌────┴────┬─────────┬──────────┐
   ↓         ↓         ↓          ↓
Combat     AI      Missions   Dialogue
   ↓         ↓         ↓          ↓
   └────┬────┴─────────┴──────────┘
        ↓
  (Vehicles, Inventory/Economy — only if a future design phase
   determines they're needed; no canon basis confirms either yet)
```

**Why this order, not a stricter linear one:** Combat, AI, Missions, and Dialogue each depend on Gameplay Foundation directly and on each other only loosely (Missions will call into Combat/AI/Dialogue, but the source material gives no reason to sequence, say, Combat strictly before AI). `PROPOSED` ordering — canon does not dictate a build sequence for future packages, so this document does not invent false precision here.

**Explicit dependency:** any future package extending `GameplayManifest.json`'s extension points must follow its own `package_registration_protocol` (namespace prefix reservation, no cycles in the framework dependency graph) — already established, cited not restated.

---

## 19 — DESIGN TRACEABILITY

| System | Source | Evidence / Basis | Status | Dependencies | Future Package |
|---|---|---|---|---|---|
| Hacking minigames | Vol II, Tutorial Contract | "Tutorializes the core loop — hacking minigames, stealth takedowns, and camera-loop manipulation" | `CANON` | InteractionFramework | Combat/Foundation boundary |
| Loadout-driven playstyle | Vol I, 7.2 | Direct quote on loud/quiet choice | `CANON` | AbilityFramework | Combat |
| Echo Shift | Vol I 7.4; Vol II Hub Upgrade | Named, described ability with confirmed effect | `CANON` | AbilityFramework (reference instance already built) | — |
| Non-lethal "remember" takedown | Vol II, Addictive Hooks | "Max Echo level adds... the ability to remember enemies into non-lethal KO animations" | `CANON` | ProgressionFramework, AbilityFramework | Combat |
| Six tracked state variables | Vol IV Ch. 2 | Explicit table | `CANON` | SaveFramework | Missions, Dialogue |
| TRANSCEND threshold logic | Vol IV Ch. 2.2 | Explicit AND-condition | `CANON` | ProgressionFramework | Missions (Final Decision) |
| Predictive-flagging enforcement doctrine | Vol I, 6.2 | Direct quote | `CANON` | — | AI |
| Adaptive boss AI (Kestrel, VOID) | Vol II, Mission 6 and Final Boss | Direct quotes | `CANON` for existence | — | AI, Combat |
| Archive Maintenance Directive (reset mechanic) | `Metro_Archives.md` Sec. 9 | New interpretation, not pure canon | `PROPOSED` (flagged highest-narrative-weight new interpretation in its own package) | InteractionFramework | Missions |
| Spire Server Hub / Nyx boss encounter | Vol I 4.1 vs. Vol II mission structure | Two sources disagree | `CONFLICTING` — unresolved, highest-priority open item across the entire project | AbilityFramework, Combat (blocked) | Combat (blocked pending resolution) |
| Skill-tree / ability-point progression mechanism | Existing Gameplay Package `SkillFramework.json` | No canon citation exists for the underlying model, only for its two confirmed *outcomes* | `PROPOSED` architecture, not `CANON` fact | SkillFramework, ProgressionFramework | Combat |
| Currency-as-upgrade-resource | None | No source ever describes credits as spendable on capability | `UNDEFINED` | — | Economy (if built) |
| NPC schedule/routine system | None | No source describes one | `UNDEFINED` | — | AI |
| Combat death/failure consequence | None | Only one failure state (capture, Contract 3) is confirmed, and it's a chase sequence, not combat | `UNDEFINED` | — | Combat |
| Accessibility options | None | Not addressed, except an implicit acknowledgment of future difficulty modes | `UNDEFINED` (with a narrow `CANON` acknowledgment that it's future scope) | — | UI, Combat |

---

## 20 — UNRESOLVED DESIGN DECISIONS

| ID | Topic | Why It Matters | Affected Systems | Decision Required | Downstream Dependency | Priority |
|---|---|---|---|---|---|---|
| UDD-01 | Spire Server Hub / Nyx encounter discrepancy | Vol I states a Nyx boss duel occurs there; Vol II's mission structure contains none | Combat, AbilityFramework, Mission design | Writing-team resolution: invent nothing until resolved | Blocks any Server Hub combat content | **Highest** |
| UDD-02 | Combat death/failure consequence model | No confirmed source states what happens on combat loss or player death | Combat, Failure/Recovery | Design decision: permadeath vs. checkpoint vs. capture-style soft-fail (precedent exists for the last option) | Blocks Combat package's core loop | High |
| UDD-03 | Skill/ability progression mechanism | Canon confirms two specific unlock *outcomes* but no general mechanism (tree, points, tiers) | SkillFramework, ProgressionFramework, Combat | Design decision on the underlying progression model | Blocks any additional unlockable ability/skill beyond the two already confirmed | High |
| UDD-04 | Difficulty modes / mission failure states | KB explicitly flags this as future scope but does not resolve it | Missions, Failure/Recovery, Accessibility | Design decision on whether/how mission failure states exist | Blocks generalized mission failure design | Medium |
| UDD-05 | NPC schedule/routine system | No source confirms or denies one exists | AI | Design decision on ambient NPC simulation depth | Blocks AI package's ambient-world scope | Medium |
| UDD-06 | Currency-as-gameplay-resource | Credits exist narratively; no confirmed spend-on-capability mechanic | Economy (if built), ProgressionFramework | Design decision: does a spendable economy exist at all | Blocks Economy package entirely if never resolved | Low (Economy is not confirmed to be a required future package) |
| UDD-07 | Vehicle interaction | No player-operated vehicle content exists in any source | Vehicles (if built) | Design decision: does the Vehicles package have any canon basis to build from | Blocks Vehicles package entirely | Low |
| UDD-08 | General reputation/faction mechanic beyond Undercroft | Four factions confirmed to exist; only one (Undercroft) has a confirmed mechanical reputation effect | AI, Dialogue, Progression | Design decision: extend reputation mechanically to other factions or leave as narrative-only | Blocks any faction-reputation gameplay beyond the one confirmed instance | Medium |
| UDD-09 | Accessibility options scope | Entirely unaddressed by any source | UI, Combat, Missions | Design decision, informed by industry standard practice since canon gives no guidance either way | Blocks accessibility feature planning | Medium |
| UDD-10 | "World heat"/detection-meter representation | VOID's flagging model is pattern-based per canon; whether it should be represented to the player as a simple meter is unaddressed | AI, UI, Combat/Stealth | Design decision on player-facing detection feedback | Blocks stealth UX design | Medium |

---

## 21 — GAMEPLAY DESIGN VALIDATION

| Check | Result |
|---|---|
| No circular system responsibilities | ✅ — Section 7's flow and the existing Gameplay Package's verified-acyclic framework dependency graph both hold |
| No contradictory rules | ⚠️ — one confirmed contradiction exists (UDD-01), explicitly flagged, not hidden |
| No undefined critical dependency hidden as fact | ✅ — Section 20 lists every one found; none are silently treated as resolved |
| No duplicate system ownership | ✅ — Section 17's boundary table assigns each domain exactly once |
| No gameplay system depending directly on Builder internals | ✅ — consistent with the existing Gameplay Package's `upstream_dependency` policy; this document adds no new Builder coupling |
| No narrative canon silently altered | ✅ — every claim in this document traces to a citation or is explicitly marked `INFERRED`/`PROPOSED`/`UNDEFINED` |
| Player actions have defined outcomes where required | ⚠️ — true for confirmed verbs (Section 3); combat resolution mechanics remain `UNDEFINED` (UDD-02) |
| Important world reactions are represented | ✅ — Section 8's three-tier consequence model covers every confirmed reactivity instance |
| Persistence requirements are identified | ✅ — Section 14, matching the existing `SaveFramework.json` exactly |
| Future package boundaries are clear | ✅ — Section 17 |

---

## 22 — FINAL GAMEPLAY ARCHITECTURE

```
PLAYER
   ↓
PLAYER SYSTEMS (identity, input, camera — existing PlayerFramework)
   ↓
GAMEPLAY SYSTEMS (interaction, ability, progression, skill, save — existing frameworks)
   ↓
WORLD / NPC / MISSION / NARRATIVE SYSTEMS (future: Combat, AI, Missions, Dialogue)
   ↓
WORLD STATE (six tracked variables + district-level state, per locked Meridian_Master)
   ↓
PERSISTENCE (SaveFramework's PersistentStateRegistry)
```

**How future packages attach:** each future package (Combat, AI, Missions, Dialogue, Vehicles, Economy) registers against specific `GameplayManifest.json` extension points, reserves its own namespace prefix, and inserts itself into the "WORLD / NPC / MISSION / NARRATIVE SYSTEMS" layer above — never bypassing Gameplay Systems to reach Player Systems or World State directly. This is the same boundary discipline the existing Gameplay Package already enforces; this document confirms it holds up under source-grounded scrutiny rather than introducing a new rule.

---

## FINAL SELF-AUDIT

1. Every major gameplay claim has a source basis or explicit status. ✅ — verified against Section 19's traceability table.
2. No Meridian Master data was modified. ✅ — no file in the locked package was opened for writing during this document's construction.
3. No narrative canon was invented. ✅ — every new mechanical claim is tagged `PROPOSED` or `UNDEFINED`, never presented as `CANON`.
4. No future system was accidentally implemented prematurely. ✅ — no code, no Blueprints, no new JSON package generated in this response.
5. System boundaries are explicit. ✅ — Section 17.
6. Dependencies are explicit. ✅ — Section 18.
7. Undefined decisions are visible. ✅ — Section 20, ten items, none hidden.
8. Gameplay loops are coherent. ✅ — Section 2.
9. Player verbs connect to actual systems. ✅ — Section 3's table.
10. World reactivity is represented. ✅ — Section 8.
11. Persistence requirements are represented. ✅ — Section 14.
12. Future packages can be derived from this document. ✅ — Sections 17, 18, 22.

---

*End of Gameplay_Design_Master.md.*
