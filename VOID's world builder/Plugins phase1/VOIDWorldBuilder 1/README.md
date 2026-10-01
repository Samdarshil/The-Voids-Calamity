# VOID World Builder

Internal Unreal Engine 5 plugin for generating Meridian's greybox districts
from **approved design packages**. This tool implements design; it never
invents lore, layout intent, or gameplay logic that wasn't specified in the
input.

## Status: Phase 1 — Plugin Architecture, Module Structure, Import Pipeline

This is a compilable skeleton. It can import a JSON design package,
validate it, and show the result in an Editor panel or headless commandlet.
**It does not generate any content yet.** Road, Building, District,
Navigation, World Partition, Data Layer, and Gameplay Volume generation
arrive in Phases 3 through 9, per the approved roadmap.

## Module Map

| Module | Type | Contains |
|---|---|---|
| `VOIDWorldBuilderCore` | Runtime | Data structs (`FVoidDesignPackage`, `FVoidValidationReport`), the `IVoidGenerator` interface, shared log category. No editor dependencies. |
| `VOIDWorldBuilderImport` | Editor | JSON reading, JSON→struct mapping, and validation rules. This is where Phase 1's actual logic lives. |
| `VOIDWorldBuilderGenerators` | Editor | The generator registry only. Empty of generators until Phase 3. |
| `VOIDWorldBuilderEditor` | Editor | The Slate panel (import a package, see the validation report) and a headless commandlet for CI. |

Dependency direction is one-way: `Editor → Generators → Import → Core`.
No module depends on a sibling generator module.

## Enabling the Plugin

1. Copy the `VOIDWorldBuilder` folder into your project's `Plugins/` directory
   (or add it as an Engine plugin if multiple projects will share it).
2. Regenerate project files.
3. Open the project; if prompted to rebuild missing modules, accept.
4. Enable the plugin from **Edit → Plugins → World Building → VOID World Builder**
   if it isn't enabled by default (`EnabledByDefault` is currently `false`
   while the tool is pre-Phase-3).
5. Open the panel from **Window → VOID World Builder** (registered under the
   Tools workspace menu category).

See `Docs/BuildInstructions.md` for compiling from source and running the
commandlet, and `Docs/CodingStandards.md` for the conventions this codebase
follows.

## Design Package Format (Phase 1)

The importer expects a JSON object shaped like:

```json
{
  "districtId": "district_04_white_zone",
  "metadata": {
    "sourceDocumentName": "Meridian District 04",
    "sourceDocumentVersion": "1.0",
    "isApproved": true
  },
  "buildings": [
    {
      "id": "bldg_01",
      "heightUnits": 500.0,
      "buildingType": "CivicCenter",
      "footprintCorners": [[0,0],[0,100],[100,100],[100,0]]
    }
  ],
  "roads": [
    {
      "id": "road_01",
      "widthUnits": 12.0,
      "centerlinePoints": [[0,0],[500,0]]
    }
  ]
}
```

`metadata.isApproved` must be `true` or the package fails validation —
unapproved design cannot be generated into a shared level, by design.

## What's Deliberately Not Here Yet

- No Road/Building/District/Navigation/World Partition/Data Layer/Gameplay
  Volume generators — Phases 3 through 9.
- No Phase 10 orchestrator ("generate the whole city" button).
- No auto-registration for generators (manual `RegisterGenerator` calls
  are enough while the registry has zero entries).

Do not add generator logic to this branch. Phase 1's job is the skeleton
only.
