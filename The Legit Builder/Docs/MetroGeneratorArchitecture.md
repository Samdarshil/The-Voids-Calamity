# Metro Generator & World Partition Support (Agent 2)

Scope: Phase 6 (Metro Generator) and the practical part of Phase 9 (World Partition support).
Built against the **Phase 3** Builder (Core / Import / Generators / Editor). It adds files and makes
a small number of additive shared-interface changes (see `HANDOFF_AGENT2.md`).

## Data flow

```
Meridian MetroNetwork.json ──┐
Meridian RoadNetwork.json  ──┼─► FVoidJsonReader (existing) ─► FVoidMetroNetworkMapper (Import module)
Authored "metro" block     ──┘                                         │
                                                                       ▼
                                                    FVoidDesignPackage::Metro  (FVoidMetroData, Core)
                                                                       │
                              FVoidGeneratorRegistry.FindGenerator("Metro")
                                                                       ▼
                                                            FVoidMetroGenerator
        validate ► FVoidMetroLayoutResolver ► validate resolved ► [abort here on any Error]
        ► transaction ► destroy owned actors ► spawn AVoidMetroTrackActor / AVoidMetroStationActor
        ► FVoidWorldPartitionHelper placement ► FVoidMetroExportRegistry.Publish
```

Generators never parse JSON. The mapper is the metro extension of the existing import layer, not a
second importer: it reuses `FVoidJsonReader` and reports through `FVoidValidationReport`.

## The one thing to understand: Meridian has no coordinates

`MetroNetwork.json` is topology only: networks, lines, stations, interchanges, district links.
`Meridian_Master.json` states `coordinate_policy: no_fabricated_coordinates_no_fabricated_transforms_no_fabricated_mesh_ids`
and `RoadNetwork.json` says "The Builder resolves actual geometry at generation time." There are no
track alignments, platforms, entrances, or elevated/underground sections in the supplied data.

So the generator separates *data* from *layout*:

1. **Authored data always wins.** Positions, grades, entrances and centerlines from the optional `metro`
   block override everything and are never flagged as placeholders.
2. **Otherwise a deterministic placeholder layout is derived** (`FVoidMetroLayoutResolver`): Olympus Spire at
   the world origin (canon-explicit), districts by radial band radius + azimuth from Project Settings.
   Band **names** and district→band mapping are copied from `DistrictRegistry.json`; **radii and azimuths are
   placeholders**. Everything derived is flagged (`bPlaceholder`, actor property `bIsPlaceholderLayout`, warning
   `VOID.Metro.PlaceholderLayout`). Nothing is written back into the data.
3. **If placeholders are disabled, unplaceable items are skipped with a warning.** Nothing is guessed silently.

Vertical placement follows the same rule: the Dead network is `Underground` (it is the pre-Council subway
service layer, housed in tunnels). The Live network's grade is *not* stated anywhere, so it defaults to
`AtGrade` (`LiveNetworkDefaultGrade`); set it to `Elevated` if you want a viaduct read, understanding that is
a design choice.

## What is generated

| Element | Live (Grav-Rail) | Dead (Pre-Council Subway) |
|---|---|---|
| Track | Guideway deck + two edge strips (no sleepers) | Ballast bed + two rails + instanced sleepers |
| Tunnels | Bore where grade < 0 | Bore for all track (underground) |
| Elevated | Girder + instanced piers where grade > 0 | (only if authored) |
| Portals | Where a run crosses ground level | Same |
| Surface trace | Thin ground marker above underground runs | Same |
| Stations | Side platforms, canopy (surface), podium (elevated) | Platforms only (always underground, so no canopy); sized to fit inside the bore |
| Entrances | Headhouse per entrance | Headhouse; Sector 0 threshold limited to exactly 1 |
| Escalator / elevator | Escalator + elevator when station is not at grade | Stair/escalator slab only (no elevator) |
| Service placeholder | Yes (`centrally_scheduled_council_civic_services`) | **No** (`none_abandoned_infrastructure`) |
| Interchange | Where radial × ring cross; attaches to an existing station if one is there | None (by design) |

Not implemented (out of scope by instruction): trains, passenger/train AI, ticketing, signalling, schedules,
economy, interiors, navigation.

## Networks are never merged
Separate actors, folders, tags, chunk sizes and runtime grids per network. `FVoidMetroValidator` errors on
cross-network station membership, mixed interchanges, dead-network interchanges, mismatched district links, and
live/dead stations resolving to the same 3D place. Generation aborts on any validation **Error** before the world is touched.

## Actor organization & cleanup
* Owner key: `UVoidMetroGenerationSettings::OwnerKey` (default `MeridianMetro`), stored on every actor
  (`AVoidMetroActorBase::OwnerKey`) and as tag `VOIDMetroOwner.<key>`.
* Deterministic ids/labels: `VoidMetro_<Live|Dead>_Track_<segmentId>_c<cellX>_<cellY>_<n>` and
  `VoidMetro_<Live|Dead>_Station_<stationId>`. The id is also stored in `GeneratedId`.
* Regeneration destroys all *loaded* actors with the same owner key inside the same undo transaction, then rebuilds. No
  accumulation. Limitation: actors in *unloaded* World Partition cells are not visible to the iterator; load the region (or
  the whole world) before regenerating.
* Outliner folder: `<OutlinerFolderRoot>/<Live|Dead>/<Track|Station>`.

## World Partition (placement only)
Implemented: detect partitioned world; split every track path into per-cell chunk actors (deterministic, gap-free, shared
boundary vertices) using `ChunkCellSizeUnits` (match your runtime grid cell size; Dead network uses a coarser multiple so it streams
as a related cluster per `StreamingStrategy.json`); stamp runtime grid / spatially-loaded / HLOD-relevance / HLOD layer / folder;
actors are pivoted at their chunk centre so bounds and cell assignment are sane.

Not implemented (by instruction): creating/converting a World Partition map, defining runtime grids, data layers, a streaming system.
Runtime grids named in settings must already exist in the map's World Settings. HLOD: procedural-mesh components do not contribute
to HLOD proxies; the setting is a hook for when geometry is converted to static meshes.

## Performance
No Tick anywhere. Track chunks are static actors; one procedural mesh (≤4 sections) per chunk; sleepers/piers are one batched
`AddInstances` on a shared engine cube; one spline per chunk. Resolver is O(stations × path points) for orientation and
box-rejected polyline contact tests for interchanges (no O(N²) over actors). With the placeholder layout, the ring is ~25 km
(≈98 chunk actors at 25 600 cm cells).

## Reading the layout from other systems
```cpp
#include "Metro/VoidMetroExports.h"
const FVoidMetroExportRegistry& Reg = FVoidMetroExportRegistry::Get();
if (Reg.HasLayout()) {
    for (const FVoidMetroResolvedStation& S : Reg.Layout().Stations) { /* S.Location, S.Entrances[], S.Grade */ }
    // Reg.Layout().Segments[].Points (track alignment), .Portals (tunnel entrances), .ElevatedSpans
}
```
`FVoidRoadGenerator` is untouched. `LineSpec.SharesRouteId` / `MapsToRoadCategory` are carried through so a road consumer can pair
`spire_radial_spine` / `mid_tier_ring_road` with the metro line that shadows it.
