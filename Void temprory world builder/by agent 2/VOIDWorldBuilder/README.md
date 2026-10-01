# VOID World Builder

Internal Unreal Engine 5 plugin for generating Meridian's greybox districts
from **approved design packages**. This tool implements design; it never
invents lore, layout intent, or gameplay logic that wasn't specified in the
input.

## Status: Phase 3 — Road Generator

Phase 1 delivered the plugin skeleton. Phase 2 delivered the import
layer. Phase 3 delivers the first working generator: roads, sidewalks,
curbs, medians, intersections (T/four-way/dead-end/cul-de-sac),
roundabouts, bridges, and tunnels, all generated as greybox geometry from
an imported design package. **Buildings, districts, navigation, and every
other future generator remain out of scope** -- see Phase 3's own
architecture notes for exactly what "greybox" means here.

## Module Map

| Module | Type | Contains |
|---|---|---|
| `VOIDWorldBuilderCore` | Runtime | Data structs (`FVoidDesignPackage`, `FVoidDistrictData`, `FVoidRoadSpec`, `FVoidValidationReport`, `FVoidSchemaVersion`, `FVoidImportContext`), the `IVoidGenerator` / `IVoidPackageReader` interfaces, shared log category. No editor dependencies. |
| `VOIDWorldBuilderImport` | Editor | Format detection, JSON reading/mapping, validation, project settings. |
| `VOIDWorldBuilderGenerators` | Editor | The generator registry, plus **Road** (`Source/.../Road/`): spline/mesh/intersection/bridge-tunnel builders, `FVoidRoadGenerator`, `AVoidRoadActor`/`AVoidRoadJunctionActor`, road-specific settings and validation. |
| `VOIDWorldBuilderEditor` | Editor | The Slate panel (import, then Generate Roads) and a headless commandlet. |

Dependency direction is one-way: `Editor → Generators → Import → Core`.

## Enabling the Plugin

1. Copy the `VOIDWorldBuilder` folder into your project's `Plugins/` directory.
2. Regenerate project files and build (see `Docs/BuildInstructions.md`).
3. Enable the plugin from **Edit → Plugins → World Building → VOID World Builder**.
4. Open the panel from **Window → VOID World Builder**: import a package,
   then click **Generate Roads**.
5. Configure import behavior at **Project Settings → Plugins → VOID World
   Builder Import**, and road generation behavior at **Project Settings →
   Plugins → VOID World Builder Road Generation**.

## Design Package Format (Phase 3 additions)

Roads now carry generation-relevant fields, all optional and additive to
Phase 2's shape:

```json
{
  "schemaVersion": "1.0",
  "metadata": { "sourceDocumentName": "Meridian District 04", "sourceDocumentVersion": "1.0", "isApproved": true },
  "district": {
    "districtId": "district_04_white_zone",
    "buildings": [],
    "roads": [
      {
        "id": "main_street",
        "roadType": "Primary",
        "widthUnits": 1000.0,
        "laneCount": 4,
        "speedLimitUnits": 60,
        "hasSidewalk": true,
        "hasMedian": true,
        "centerlinePoints": [[0,0],[500,200],[1000,0]]
      },
      {
        "id": "river_crossing",
        "roadType": "Secondary",
        "widthUnits": 800.0,
        "isBridge": true,
        "centerlinePoints": [[1000,0],[1000,2000]]
      },
      {
        "id": "roundabout_1",
        "roadType": "Roundabout",
        "roundaboutRadiusUnits": 500.0,
        "centerlinePoints": [[2000,2000]]
      }
    ]
  }
}
```

See `Docs/ValidationRules.md` for every field's validation rule and
`Docs/RoadGeneratorArchitecture.md` for the full field reference and the
reasoning behind each addition.

## What's Deliberately Not Here Yet

- No Building/Navigation/World Partition/Data Layer/Gameplay Volume
  generators -- Phases 4 through 9.
- No Phase 10 orchestrator.
- No Data Layer assignment for spawned actors -- the Data Layer API has
  changed across UE5 minor versions and guessing at it risked a compile
  break for an unrequested nice-to-have; spawned actors are otherwise
  fully World Partition-safe (standard `SpawnActor`).
- No real-world road engineering (banking, superelevation, clothoid
  curves, sight-distance grading). This is greybox blockout geometry --
  flat ribbons, simple offsets, simple ramps -- consistent with the
  project-wide "generate placeholder blockout, not final art" scope.

Do not add Building/District/Navigation logic to this branch. Phase 3's
job is roads only.

