# VOID: THE UNSEEN HAND
## MERIDIAN MASTER PLAN
### Meridian_Master_Plan.md | Integration Document — Synthesizes Locations 01–05 | Status: Draft for Review, First of Fourteen Files

---

## 0. Canon Compliance Statement

This document integrates the five approved, locked district packages — Undercroft, White Zones, Metro Archives, Sector 0, Olympus Spire — into a single coherent world. It does not redesign, rename, or modify any of them; every district-specific fact below is reproduced from its own approved package, cited by section, not reinterpreted. It does not introduce a sixth named district — where this document describes space between the five (roads, generic residential fabric, transit spine), that space is treated as infrastructure and generation scaffolding, not as a designed location with its own identity, history, or six-ownership-questions treatment. It does not answer any unresolved story question. Three carried-forward flags from the Phase 1 packages remain explicitly open and are consolidated, not resolved, in Sec. 31: the Vol I / Vol II Nyx-at-the-Spire discrepancy (highest priority), the Archive Maintenance Directive's narrative-weight interpretation, and the lower-risk structural interpretations from the Undercroft, White Zones, and Sector 0 packages.

**No fabricated engineering values appear anywhere in this document.** Where a number would normally appear (ring counts, distances, populations), this document either cites the one canon-confirmed figure (Vol I's ~30 million registered residents) or uses relative/qualitative bands and generation rules instead, consistent with the standard set across all five prior packages.

---

## 1. Executive Summary

Five districts, built one at a time and cross-checked against each other at every step, now complete a single structural claim first made in Volume I and never fully paid off until now: Meridian is concentric rings of trust radiating outward from a center, and the closer to that center, the more of the city's polish is spent making the promise look true. This document is where that claim stops being five separate design documents making the same argument in different rooms, and becomes one buildable world. It defines how the five approved anchors sit relative to each other, how a player physically and perceptually moves between them, and how the VOID World Builder plugin should sequence their generation — without touching a single word of what's already been locked.

---

## 2. World Design Philosophy

Four principles carried through all five prior packages and govern this one without exception:

1. **Canon-first, always.** Every integration claim below is cited to a specific section of an approved package or to Volumes I–IV directly. Nothing here is asserted without a source.
2. **Fill silence, never contradict signal.** The space between the five districts is real silence — canon never describes Meridian's ordinary streets in detail — and this document fills it as infrastructure, not as new lore.
3. **Flag, don't resolve.** Every open item carried into this document (Sec. 31) stays open. Integration work does not get to quietly close a mystery just because closing it would make the master plan tidier.
4. **Design WHAT exists, not HOW it's built.** This document specifies relationships, hierarchies, and generation rules. It does not specify Unreal Engine implementation, plugin code, or literal coordinates — that boundary held for all five districts and holds here too.

---

## 3. Meridian Overview

Meridian is the largest of the World Council's sealed megacities: a vertical, climate-controlled arcology of roughly thirty million registered residents (Vol I, Ch. 4 preamble), built in concentric rings of trust radiating outward from the World Council Spire at its center. The higher and closer to the Spire a district sits, the more of the Weave's polish is spent making it look like the world VOID promised everyone; the lower and further out, the more of the city's actual machinery — and its actual desperation — is left showing. Every one of the five approved packages was built as a specific, fully-detailed point proving some part of that one sentence true. This document is where all five points get plotted on the same map.

---

## 4. Overall World Scale

The only literal figure this document uses is canon's own: approximately thirty million registered residents (Vol I, Ch. 4). No district footprint, ring count, or distance value is stated as a fabricated number anywhere below. Where scale needs to be communicated for generation purposes, it is expressed as a **relative band system** (Sec. 6) — a set of named zones a Builder can iterate over and populate, rather than a fixed numeric grid this document has no canon basis to invent.

---

## 5. City Organization

Meridian organizes along two independent axes, both established piecemeal across the five approved packages and formalized here as one system for the first time:

- **The Radial Axis** — distance from the Spire at Meridian's center (Sec. 6).
- **The Vertical Axis** — height, from Sector 0's dead foundation at the absolute bottom to the Spire's soaring towers at the absolute top (Sec. 7).

Every approved district occupies a specific, citable position on one or both axes. Sector 0 is the deliberate exception to the radial axis specifically — it has no coordinates and does not participate in the trust gradient at all (Sector 0, Sec. 4.1) — and this document preserves that exception rather than forcing it onto the grid for tidiness.

---

## 6. Radial Hierarchy

| Band | Occupant(s) | Source |
|---|---|---|
| **Core** | Olympus Spire — the literal center every other district's radial position is measured as distance from | Spire, Sec. 4; Vol I Ch. 4 preamble |
| **Inner Rings** | Unbuilt — connective tissue and near-Spire residential fabric, Phase 2/3 scope | New, this document |
| **Mid-Tier Rings** | White Zones — distributed nodes across this entire band | White Zones, Sec. 2.1 |
| **Seam Zone** | Metro Archives — the boundary between the mid-tier band and the Undercroft's outer-ring foundation belt | Metro Archives, Sec. 3.2 |
| **Outer Rings** | Undercroft's outer-ring foundation belt (the Far Stacks) | Undercroft, Sec. 1.2 |
| **Off-Gradient** | Sector 0 — narrative "nowhere," production-only placement at the transit network's deepest terminus | Sector 0, Sec. 4.1–4.2 |

The Undercroft itself is not a single band — per its own document, it is a continuous foundation stratum with an **internal** radial gradient (Spire-Adjacent Foundation, Mid-Ring Foundation, Outer-Ring Foundation belts) running beneath the Inner, Mid-Tier, and Outer bands simultaneously (Undercroft, Sec. 1.2). This document treats that internal structure as already complete and does not re-derive it.

---

## 7. Vertical Hierarchy

| Tier | Occupant | Source |
|---|---|---|
| **Absolute Top** | Olympus Spire — soaring towers, tallest structures in Meridian by design margin | Spire, Sec. 4, 9 |
| **Grade / Street Level** | White Zones (2–4 stories, hard ceiling), Metro Archives (bespoke aboveground public floor) | White Zones, Sec. 2.2; Metro Archives, Sec. 3.1 |
| **Below Grade, Living** | Undercroft — literal foundation levels, still wired, still inhabited | Undercroft, Sec. 1.1 |
| **Below Grade, Aging** | Metro Archives sub-basement — confirmed second structural level | Metro Archives, Sec. 3.1 |
| **Absolute Bottom, Dead** | Sector 0 — beneath even the Undercroft's living foundation; dead, not derelict | Sector 0, Sec. 4.3 |

---

## 8. District Placement

Formal placement summary, each citing its own package's structural-position section directly:

- **Olympus Spire** — absolute center and absolute top (Spire, Sec. 4).
- **White Zones** — distributed discrete nodes across the mid-tier residential band; one fully-detailed flagship instance, one reusable typology for any future non-flagship node (White Zones, Sec. 2.1, 8, 25).
- **Metro Archives** — singular, bespoke, placed deliberately at the seam between the mid-tier band and the Undercroft's outer belt (Metro Archives, Sec. 3.2, 7).
- **Undercroft** — continuous substrate beneath the Inner, Mid-Tier, and Outer radial bands, with its own three-belt internal gradient (Undercroft, Sec. 1.2).
- **Sector 0** — off the radial gradient entirely; production-only placement at the deepest terminus of the old pre-Council transit network, beneath and beyond the Undercroft's outer belt (Sector 0, Sec. 4.2).

---

## 9. District Relationships

The master connection matrix, compiled from all five packages' own Meridian Integration sections (cited verbatim in intent, not altered):

| | Undercroft | White Zones | Metro Archives | Sector 0 | Olympus Spire |
|---|---|---|---|---|---|
| **Undercroft** | — | No direct connection; total contrast (concealment: noise vs. helpfulness) | Physical: shared pre-Council transit substrate | Physical: shared old transit substrate; thematic: "what the Undercroft would be if it had died" | Structural/vertical opposite; shared security-doctrine dialogue line reused |
| **White Zones** | No direct connection | — | No direct connection; purely contrastive (standardized vs. bespoke) | No connection beyond total contrast | Gameplay progression: Party Level fulfills the social-stealth rehearsal this document set up |
| **Metro Archives** | Physical: shared transit substrate | No direct connection | — | Physical + narrative primary: sub-basement is a smaller, still-active cousin of Sector 0's core; exhibit-case map bridges Contract 2 → Contract 3 | Thematic/technical lineage only: both part of the same continuity-maintenance hardware family |
| **Sector 0** | Physical: shared substrate | No connection | Technical lineage: Sector 0 is the original core, Metro Archives' node a smaller regional cousin | — | Thematic lineage: "what the Spire's server hub used to be before superseding it" |
| **Olympus Spire** | Opposite ends of radial + vertical axes | Fulfills social-stealth setup | Shares reality-bending proximity principle, scaled up | Total deliberate opposite (silence vs. voice) | — |

---

## 10. District Transition Rules

Formal rules governing how a player moves between approved districts, none of which contradict any package's own confirmed traversal logic:

1. **Undercroft ↔ Metro Archives:** via the shared pre-Council transit substrate (Undercroft, Sec. 7.7; Metro Archives, Sec. 10). No street-level route should be treated as equivalent or substitutable — the connection is specifically the old, dead subway network, not the modern grav-rail grid.
2. **Undercroft ↔ Sector 0:** via the Contract 3 entrance tunnel threshold, marked by the crackling energy field (Sector 0, Sec. 8.1). This is a **hard, one-directional narrative gate** — everything before the threshold is Undercroft-scope traversal (subway chase); everything after is Sector 0-scope (Sector 0, Sec. 0).
3. **White Zones ↔ anything:** White Zones has **no direct physical connection** to the Undercroft, Metro Archives, or Sector 0 (Sec. 9 above). Any route between a White Zone node and another approved district must pass through unbuilt Inner/Mid-Tier connective tissue (Sec. 6), never a bespoke shortcut invented to shorten the trip.
4. **Olympus Spire ↔ anything:** the Spire has no direct physical connection to any of the other four districts (Sec. 9 above). Access is exclusively via its own internal trust gradient (Spire, Sec. 10) — Base Plaza, then Party Level (invitation/disguise-gated), then upward.
5. **Weave-coverage transition (hard, binary):** crossing into Sector 0 is the only transition in the game where Weave coverage drops from present to **entirely absent**, with no partial/gradient signal permitted (Sector 0, Sec. 16). Every other transition in this document is a density gradient, not an on/off switch.

---

## 11. Road Hierarchy

No literal road names, widths, or coordinates are specified — the following is a generation-rule typology only:

- **Radial Arterials** — the primary roads a Builder should generate running from the Spire's Base Plaza outward through the Mid-Tier band, passing near (not through) White Zone nodes per their population-catchment placement rule (White Zones, Sec. 6).
- **Ring Roads** — circumferential connectors at a given radial band, linking White Zone nodes to each other and to Inner/Outer connective tissue without ever routing through Metro Archives or the Undercroft (both of which are accessed via substrate/tunnel routes, not surface arterials).
- **Service & Maintenance Routes** — the Undercroft's own off-grid layer (old maintenance tunnels, decommissioned subway, rooftop routes, Undercroft Sec. 13) sits entirely outside this surface hierarchy and should never be generated as a variant of it.

---

## 12. Metro Network

Two entirely separate transit networks exist in Meridian, and this document is explicit that they must never be merged into one system by the Builder:

1. **The Live Network (Grav-Rail + Drone-Taxi):** VOID-synchronized, zero collisions in sealed districts (Vol I, 3.5). Confirmed stop placement rule: within short walking distance of every White Zone Trust Center instance (White Zones, Sec. 11), plus a stop serving the Spire's Base Plaza. This network does not serve the Undercroft, Metro Archives, or Sector 0 — none of those districts are on the registered grid.
2. **The Dead Network (Pre-Council Subway):** Unmapped, off-grid, threading beneath the Undercroft, terminating at Metro Archives' sub-basement seam and at Sector 0's deepest point (Metro Archives, Sec. 10; Sector 0, Sec. 4.2). This network has exactly one confirmed access point into Sector 0 (Sector 0, Sec. 11) and should not be given additional invented entrances anywhere in the world.

---

## 13. Underground Infrastructure

The shared substrate binding three of the five districts, now formalized as one continuous system: Undercroft's living foundation (Undercroft, Sec. 1.1) → Metro Archives' sub-basement seam tap (Metro Archives, Sec. 8.5) → Sector 0's dead terminus (Sector 0, Sec. 4.2). This is the Dead Network's physical housing (Sec. 12) and should be modeled by the Builder as one continuous underground volume with three access/interest points, not three disconnected basements that happen to share a name.

---

## 14. Rooftop Traversal

A secondary, largely Undercroft-native traversal layer (Undercroft, Sec. 13: "rooftop routes that predate the current transit grid"), with exactly one confirmed exception elsewhere: the White Zones' Echo-Shift-gated Rooftop Mechanical Level (White Zones, Sec. 9, 13). This document does not extend rooftop traversal into Metro Archives, Sector 0, or the Spire's restricted levels — none of those packages confirm or imply it, and inventing it now would be new design content disguised as integration.

---

## 15. Vehicle Movement

Applies exclusively to the Live Network (Sec. 12.1) — VOID's traffic-cognition layer synchronizes every registered vehicle with zero fatal collisions in sealed districts in over forty years (Vol I, 3.5), and by the same logic, VOID knows the real-time location of every registered vehicle at all times. The Dead Network and the Undercroft's off-grid layer have no vehicle synchronization of any kind — this is precisely why off-grid movement is possible there and nowhere else.

---

## 16. Pedestrian Flow

| District | Pedestrian Character | Source |
|---|---|---|
| Undercroft | Irregular, retrofit, vertical-shaft-heavy | Undercroft, Sec. 13 |
| White Zones | Wide, unobstructed, no blind corners by hard design rule | White Zones, Sec. 10 |
| Metro Archives | Quiet, low-density, minimal circulation needs | Metro Archives, Sec. 14 |
| Sector 0 | Minimal, horizontal, "walking through a memory" | Sector 0, Sec. 22 |
| Olympus Spire | Curated, behavior-screened, vertically gated | Spire, Sec. 31, 35 |

---

## 17. Landmark Visibility

**The Olympus Spire should be visible from every approved district's skyline**, per the Spire package's own flagged (optional, non-blocking) suggestion (Spire, Sec. 8, 43). This document is the correct place to formalize that rule, since doing so here integrates the five packages without editing any of their locked files. This is now a **standing world-generation rule**: the Builder should ensure Spire sightlines from the Undercroft's rare upward shafts, White Zones' open plazas, and Metro Archives' public-floor windows, wherever geometry permits. Sector 0 is the sole exception — its entrance tunnel and interior are enclosed by design (Sector 0, Sec. 6) and should never be given a Spire sightline.

---

## 18. Skyline Composition

From any elevated vantage point in Meridian, the silhouette should read, in order of visual dominance: **Olympus Spire** (soaring, center, unmistakable) → **Elite Residential Towers** (Spire-adjacent cluster, Spire Sec. 19) → **Metro Archives** (bespoke, individually-designed, mid-rise) → **White Zone nodes** (low-rise, repeated, unmistakably uniform) → **nothing** where the Undercroft and Sector 0 sit, since both are entirely below grade and contribute no skyline silhouette of their own (Undercroft, Sec. 1.1; Sector 0, Sec. 4.3).

---

## 19. Environmental Transition Rules

Moving radially inward or vertically upward should read, environmentally, as a steady increase in polish, curation, and material perfection — the throughline every package built toward individually. Sec. 21 and Sec. 26 formalize the two specific spectrums (lighting, security) this applies to; this section is the general principle both are instances of.

---

## 20. Security Transition Rules

The complete enforcement-doctrine spectrum, compiled for the first time as one gradient:

| District | Doctrine | Intensity |
|---|---|---|
| Sector 0 | Not applicable — VOID cannot perceive this location at all | None (absolute floor) |
| Undercroft | Rare, scheduled sweeps, triggered only by coordinated-pattern flags | Low |
| Metro Archives | No physical presence; automated environmental response only | Low-Medium, non-confrontational |
| White Zones | Heavy, standing Enforcer patrol | High |
| Olympus Spire | Predictive, behavioral screening — the most sophisticated expression of the same doctrine | Highest |

Every tier is confirmed to be the **same underlying predictive-flagging doctrine** (Constraint 6, referenced identically in all five packages) applied to five different baseline risk profiles — this document does not introduce a sixth doctrine anywhere.

---

## 21. Lighting Transition Rules

The complete five-district lighting spectrum, first stated in full in the Spire package (Spire, Sec. 26) and reproduced here as the master reference:

**Undercroft** (honest imperfection) → **Metro Archives** (tired neglect) → **White Zones** (clean institutional calm) → **Sector 0** (the one dynamic exception — a cold-to-warm arc tied to story progression, not position) → **Olympus Spire** (filtered gold, the warmest and most curated light in the game).

---

## 22. Population Distribution

| District | Density | Character |
|---|---|---|
| Undercroft | High, informal | Ghosts, fixers, off-grid population |
| White Zones | High, formal | Registered civilians, constant queue culture |
| Metro Archives | Sparse | Niche researchers, Historical Society |
| Sector 0 | Near-zero, by design | Kaevon/Veronica, Echo, conditional Nyx |
| Olympus Spire | Sparse, exclusive | Elite residents, Council staff, chosen guests only |

---

## 23. Gameplay Flow

The five-district gameplay-register spread, complete for the first time as one system: **Undercroft** (analog evasion) → **White Zones** (social stealth, introductory) → **Metro Archives** (environmental puzzle tension) → **Sector 0** (pure exploration, narrative delivery) → **Olympus Spire** (social stealth, advanced/high-stakes). No two districts share a primary gameplay register, and this document treats that as a deliberate, load-bearing design achievement worth protecting in any future phase.

---

## 24. Mission Support

| Mission/Contract | Primary District | Status |
|---|---|---|
| Tutorial Contract (Data Heist) | Undercroft-adjacent (Stellar Holdings office) | In scope, background reference only |
| Contract 2 (Memory Hole) | Metro Archives | Fully built |
| Contract 3 (Ghost Data) | Undercroft (approach) → Sector 0 (threshold) | Fully built |
| Mission 4 (The Vanished Professor) | Dr. Elara Voss's buried lab | **Out of scope** — separate secondary location |
| Mission 5 (The Witness) | White Zones | Fully built |
| Mission 6 (The Architect's Blueprint) | Riggs' old laboratory | **Out of scope** — flagged in Undercroft package |
| Mission 7 (The Human Heart) | Olympus Spire | Fully built |
| Mission 8 (The Core Gate) | The Core Gate | **Out of scope** — flagged in Sector 0 package |
| Final Boss (VOID // Core) | The White Void | **Out of scope** — flagged in Sector 0 package |

---

## 25. Exploration Loops

| District | Side Content |
|---|---|
| Undercroft | SQ1, SQ3, Easter Eggs 1 & 2 |
| White Zones | SQ4 |
| Metro Archives | SQ1 unlock/delivery, Easter Egg 3 |
| Sector 0 | SQ2 playback, Easter Egg 5, post-game scene |
| Olympus Spire | None beyond Mission 7 itself — no side content currently placed here |

---

## 26. World Streaming Strategy

Five confirmed World Partition regions stream independently, per each package's own technical notes: `WP_Undercroft` (3 cells, per belt), `WP_WhiteZones_Flagship` + reusable `WP_WhiteZones_NetworkNode_Template`, `WP_MetroArchives` (single bespoke region), `WP_Sector0` (single bespoke region), `WP_OlympusSpire` (single bespoke region). Connective tissue between them (Sec. 6's Inner Rings, generic road/rail corridors) should stream via standard distance-based World Partition grid cells — this document does not specify bespoke per-segment streaming design for space none of the five packages ever detailed.

---

## 27. World Partition Strategy

Spatial adjacency between regions, as a generation-order-relevant graph (full JSON form in `WorldPartition.json`): `WP_Undercroft` shares a boundary/connection point with `WP_MetroArchives` (transit seam) and with `WP_Sector0` (entrance tunnel threshold). `WP_OlympusSpire` shares no physical boundary with any other region — it connects only through the unbuilt Inner Ring connective tissue. `WP_WhiteZones_Flagship` and its template instances share no physical boundary with `WP_Undercroft` or `WP_MetroArchives`, consistent with Sec. 9's "no direct connection" findings.

---

## 28. Data Layer Strategy

The master Data Layer inventory is the union of all layers already defined in the five approved packages (full list in `DataLayers.json`), plus exactly one new cross-district layer this document introduces: **`DL_Global_MemoryFragmentProgress`**, tracking `memory_fragment_count` (KB Sec. 5.1) across every district simultaneously, since fragments are collected everywhere but visualized specifically via Sector 0's relay-lighting system (Sector 0, Sec. 27). This is the only net-new layer in this entire integration pass, and it adds no new content — only a shared counter the existing systems already depend on.

---

## 29. Generation Dependencies

Recommended Builder generation order, as a dependency chain rather than a fixed schedule:

1. **Olympus Spire** generates first, establishing the radial and vertical origin anchor (Sec. 6, 7).
2. **Primary radial arterials and the Live Network spine** generate next, radiating from the Spire's Base Plaza (Sec. 11, 12.1).
3. **White Zone network nodes** generate along the mid-tier band per the population-catchment rule (White Zones, Sec. 6), referencing the arterial spine for stop placement.
4. **Undercroft's substrate** generates as the foundation layer beneath the Inner, Mid-Tier, and Outer bands (Sec. 6), independent of surface-level geometry above it.
5. **Metro Archives** generates at its flagged seam position, referencing both the mid-tier band's edge and the Undercroft's outer belt (Metro Archives, Sec. 3.2).
6. **Sector 0** generates last, since its production-only placement (Sector 0, Sec. 4.2) is defined entirely relative to the Undercroft substrate already existing.

Full machine-readable form of this order in `Meridian_Master.json` and `WorldPartition.json`.

---

## 30. Builder Integration Notes

The VOID World Builder plugin should treat this document as narrative and relational intent, and the accompanying JSON files (beginning with `Meridian_Master.json` as the single import manifest) as its structured data source. Per-district data already defined in each approved package's own `_data.json` export is **authoritative and should not be duplicated or overridden** by any file in this Meridian_Master package — this package's JSON files reference and relate that existing data; they do not restate it.

---

## 31. Canon Validation

| Check | Status |
|---|---|
| Modifies, renames, or contradicts any of the five approved packages | **Pass — none** |
| Introduces a sixth named district | **Pass — none; connective space is infrastructure only** |
| Answers any unresolved story question | **Pass — none** |
| Preserves the Spire/Nyx discrepancy (Spire, Sec. 0.1) | **Pass — carried forward, unresolved, still highest priority across the entire project** |
| Preserves the Archive Maintenance Directive flag (Metro Archives, Sec. 2 of its Validation Report) | **Pass — carried forward, unresolved** |
| Preserves the three lower-risk structural-interpretation flags (Undercroft, White Zones, Sector 0) | **Pass — carried forward, unresolved** |
| Fabricates engineering coordinates or meaningless numeric values | **Pass — none; only canon's own ~30 million figure is used, everything else is relative/qualitative** |
| Consistent with all nine Non-Negotiable World-Design Constraints (KB Sec. 12) across all five districts simultaneously | **Pass — verified via Sec. 9's relationship matrix and Sec. 20/21's transition-rule spectrums** |

---

## 32. Final Conclusions

Five districts, five validation reports, zero contradictions found between any of them, and one still-open discrepancy that deserves resolution before it gets built rather than after. This document does not close that gap, invent a sixth location to feel more complete, or quietly decide anything Volumes I–IV left for the writing team. What it does is confirm that the thing this whole project has been building toward — a city where every district earns its place in one coherent trust gradient — actually holds together now that all five points exist. The Meridian Master Plan is the world starting to feel like one place instead of five very good documents about five very good rooms.

---

*End of Meridian_Master_Plan.md — File 1 of 14. Per your instruction, stopping here for review before proceeding to Meridian_Master.json.*
