# VOID World Builder

Internal Unreal Engine 5 plugin for generating Meridian's greybox districts
from **approved design packages**. This tool implements design; it never
invents lore, layout intent, or gameplay logic that wasn't specified in the
input.

## Status: Phase 2 — Import Pipeline & Validation System

Phase 1 delivered the plugin skeleton. Phase 2 delivers a complete,
production-ready import layer: a format-agnostic reader architecture,
4-level severity validation with error codes and suggested fixes, schema
versioning, project settings, and a richer Editor panel. **It still does
not generate any content.** Road, Building, District, Navigation, World
Partition, Data Layer, and Gameplay Volume generation arrive in Phases 3
through 9, per the approved roadmap.

## Module Map

| Module | Type | Contains |
|---|---|---|
| `VOIDWorldBuilderCore` | Runtime | Data structs (`FVoidDesignPackage`, `FVoidDistrictData`, `FVoidValidationReport`, `FVoidSchemaVersion`, `FVoidImportContext`), the `IVoidGenerator` and `IVoidPackageReader` interfaces, shared log category. No editor dependencies. |
| `VOIDWorldBuilderImport` | Editor | Format detection, JSON reading, JSON→struct mapping, validation, project settings. This is where Phase 2's logic lives. |
| `VOIDWorldBuilderGenerators` | Editor | The generator registry only. Empty of generators until Phase 3. |
| `VOIDWorldBuilderEditor` | Editor | The Slate import panel (browse, import, validation list, summary) and a headless commandlet for CI. |

Dependency direction is one-way: `Editor → Generators → Import → Core`.
No module depends on a sibling generator module. Core has **zero**
dependency on the Json module or any parsing library — see
`Docs/ArchitectureNotes.md` for why that separation matters.

## Enabling the Plugin

1. Copy the `VOIDWorldBuilder` folder into your project's `Plugins/` directory.
2. Regenerate project files and build (see `Docs/BuildInstructions.md`).
3. Enable the plugin from **Edit → Plugins → World Building → VOID World Builder**.
4. Open the panel from **Window → VOID World Builder**.
5. Configure import behavior at **Edit → Project Settings → Plugins → VOID World Builder Import**.

## Design Package Format (Phase 2)

```json
{
  "schemaVersion": "1.0",
  "_comment": "Any '_'-prefixed field is an annotation, always ignored.",
  "metadata": {
    "sourceDocumentName": "Meridian District 04",
    "sourceDocumentVersion": "1.0",
    "isApproved": true
  },
  "district": {
    "districtId": "district_04_white_zone",
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
}
```

Changed from Phase 1: `districtId`/`buildings`/`roads` now nest under a
`district` object (backing the new `FVoidDistrictData` type), and a
top-level `schemaVersion` field is expected (defaults to `"1.0"` with a
Warning if omitted). See `Docs/ValidationRules.md` for the full rule set
and `Docs/DeveloperGuide.md` for how severities and settings interact.

## What's Deliberately Not Here Yet

- No Road/Building/District/Navigation/World Partition/Data Layer/Gameplay
  Volume generators — Phases 3 through 9.
- No Phase 10 orchestrator.
- No second format reader (YAML/XML/Binary/Remote). The architecture
  supports adding one without touching the importer, Editor panel,
  commandlet, or generators — see `Docs/ExtensionGuide.md` — but Phase 2
  implements JSON only, since inventing a second format with no concrete
  need for it would be scope no one asked for.
- No Asset Registry integration. There is nothing generated yet to
  register or query against — see `Docs/ArchitectureNotes.md` for exactly
  when this becomes appropriate (Phase 4).

Do not add generator logic to this branch. Phase 2's job is the import
layer only.
