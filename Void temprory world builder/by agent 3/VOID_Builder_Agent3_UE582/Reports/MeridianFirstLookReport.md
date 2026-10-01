# Meridian First-Look Validation Report

**Subject:** `The-Voids-Calamity-master.zip` -> `Meridian Master/` (39 files).  
**Produced by:** `Tools/meridian_reference_validator.py`, the Python reference implementation of the rules in `FVoidMeridianValidator`. The C++ validator itself has **not** been executed (no Unreal Engine 5.8.2 available); its tests were cross-checked against this oracle (15 seeded defects, all detected with the expected code).  
**Date:** 2026-10-01

**Result: FAIL** - Fatal 0 | Error 14 | Warning 10 | Info 12

All 14 errors are one cause: locked input files listed by the manifest are missing from the supplied zip. Everything that *can* be checked on the files that are present passes.

## Answers to the success questions

| # | Question | Answer |
|---|---|---|
| 1 | Did Meridian import correctly? | **No - and it cannot import at all with the current Builder.** Two separate reasons: (a) 14 locked files (4 of 5 districts) are not in the package; (b) even a complete package has no geometry and is not in the `FVoidDesignPackage` format the importer reads (risk R-01). |
| 2 | Is the source data valid? | **Yes, for the 39 files present.** All 29 JSON files parse (17 data + 12 schemas); all 12 data files that have a schema conform to it (subset checker: type, required, enum, const, additionalProperties, items, min/max); no fabricated coordinates (VR-012); the DataLayers total, district-pair coverage (10/10), and dependency DAGs check out. Seeded-defect mutation tests prove each check can fail. |
| 3 | Are IDs/references valid? | **Yes.** Every district, route, tunnel/bridge, station, region, cell and module reference resolves. One field (`GameplayGraph.mission_relationships[].district`) is polymorphic: 8 of 24 values are not district ids (Warning, risk R-05). No duplicate ids within any collection. |
| 4 | Did the generators register correctly? | **Not measurable from data.** Runtime question answered by `FVoidGeneratorContractAuditor` inside the editor: expect `Road` registered and six `VOID.Contract.GeneratorNotRegistered` Warnings until the other agents' modules land. |
| 5 | Did generation fail anywhere? | **Not run.** `FVoidValidationPipeline` reports this per step (`GeneratorFailed`, `RequiredGeneratorMissing`, ...). |
| 6 | Did generated objects contain obvious errors? | **Not run.** `FVoidGeneratedWorldValidator` covers missing/duplicate/orphan actors, empty meshes, invalid transforms, missing assets, dangling references. |
| 7 | Are there cross-generator interface problems? | **Yes - 24 recorded** in `INTEGRATION_RISK_REGISTER.md`. Most consequential: R-01 (no geometry source), R-15 (district-local vs world coordinates), R-10 (shared report slot overwritten), R-08 (Road generation not idempotent), R-24 (O(N^2) junction clustering). |
| 8 | Can the integration team identify exactly what needs fixing? | **Yes**: every issue carries severity, subsystem, code, object, source, JSON path, description and a suggested fix (table below; `Reports/` JSON has the full record). |

## Findings on the real package

| Severity | Code | Source | Object / path | Description |
|---|---|---|---|---|
| Error | `VOID.Meridian.LockedInputMissing` | PackageManifest.json | Olympus_Spire.md `$.locked_inputs` | Manifest locked_inputs entry 'Olympus_Spire.md' is not present in the package. |
| Error | `VOID.Meridian.LockedInputMissing` | PackageManifest.json | Olympus_Spire_ValidationReport.md `$.locked_inputs` | Manifest locked_inputs entry 'Olympus_Spire_ValidationReport.md' is not present in the package. |
| Error | `VOID.Meridian.LockedInputMissing` | PackageManifest.json | Olympus_Spire_data.json `$.locked_inputs` | Manifest locked_inputs entry 'Olympus_Spire_data.json' is not present in the package. |
| Error | `VOID.Meridian.LockedInputMissing` | PackageManifest.json | Olympus_Spire_schematic.svg `$.locked_inputs` | Manifest locked_inputs entry 'Olympus_Spire_schematic.svg' is not present in the package. |
| Error | `VOID.Meridian.LockedInputMissing` | PackageManifest.json | Sector_0.md `$.locked_inputs` | Manifest locked_inputs entry 'Sector_0.md' is not present in the package. |
| Error | `VOID.Meridian.LockedInputMissing` | PackageManifest.json | Sector_0_ValidationReport.md `$.locked_inputs` | Manifest locked_inputs entry 'Sector_0_ValidationReport.md' is not present in the package. |
| Error | `VOID.Meridian.LockedInputMissing` | PackageManifest.json | Sector_0_data.json `$.locked_inputs` | Manifest locked_inputs entry 'Sector_0_data.json' is not present in the package. |
| Error | `VOID.Meridian.LockedInputMissing` | PackageManifest.json | Sector_0_schematic.svg `$.locked_inputs` | Manifest locked_inputs entry 'Sector_0_schematic.svg' is not present in the package. |
| Error | `VOID.Meridian.LockedInputMissing` | PackageManifest.json | VOID_Location_01_The_Undercroft.md `$.locked_inputs` | Manifest locked_inputs entry 'VOID_Location_01_The_Undercroft.md' is not present in the package. |
| Error | `VOID.Meridian.LockedInputMissing` | PackageManifest.json | VOID_Location_01_Undercroft_data.json `$.locked_inputs` | Manifest locked_inputs entry 'VOID_Location_01_Undercroft_data.json' is not present in the package. |
| Error | `VOID.Meridian.LockedInputMissing` | PackageManifest.json | White_Zones.md `$.locked_inputs` | Manifest locked_inputs entry 'White_Zones.md' is not present in the package. |
| Error | `VOID.Meridian.LockedInputMissing` | PackageManifest.json | White_Zones_ValidationReport.md `$.locked_inputs` | Manifest locked_inputs entry 'White_Zones_ValidationReport.md' is not present in the package. |
| Error | `VOID.Meridian.LockedInputMissing` | PackageManifest.json | White_Zones_data.json `$.locked_inputs` | Manifest locked_inputs entry 'White_Zones_data.json' is not present in the package. |
| Error | `VOID.Meridian.LockedInputMissing` | PackageManifest.json | White_Zones_schematic.svg `$.locked_inputs` | Manifest locked_inputs entry 'White_Zones_schematic.svg' is not present in the package. |
| Warning | `VOID.Meridian.ManifestTotalsStale` | PackageManifest.json | totals `$.totals` | PackageManifest.totals.total_files=46 but the package directory holds 39 file(s). |
| Warning | `VOID.Meridian.PolymorphicDistrictField` | GameplayGraph.json | dr_elara_voss_buried_lab `$.mission_relationships[8].district` | 'district' value 'dr_elara_voss_buried_lab' is not a district id; in GameplayGraph.mission_relationships this field also carries sub-locations/transitions. |
| Warning | `VOID.Meridian.PolymorphicDistrictField` | GameplayGraph.json | metro_archives_unlock_undercroft_playback `$.mission_relationships[22].district` | 'district' value 'metro_archives_unlock_undercroft_playback' is not a district id; in GameplayGraph.mission_relationships this field also carries sub-locations/transitions. |
| Warning | `VOID.Meridian.PolymorphicDistrictField` | GameplayGraph.json | not_applicable_apartment_interior `$.mission_relationships[0].district` | 'district' value 'not_applicable_apartment_interior' is not a district id; in GameplayGraph.mission_relationships this field also carries sub-locations/transitions. |
| Warning | `VOID.Meridian.PolymorphicDistrictField` | GameplayGraph.json | riggs_old_laboratory `$.mission_relationships[13].district` | 'district' value 'riggs_old_laboratory' is not a district id; in GameplayGraph.mission_relationships this field also carries sub-locations/transitions. |
| Warning | `VOID.Meridian.PolymorphicDistrictField` | GameplayGraph.json | sector_0_threshold_to_core_gate `$.mission_relationships[20].district` | 'district' value 'sector_0_threshold_to_core_gate' is not a district id; in GameplayGraph.mission_relationships this field also carries sub-locations/transitions. |
| Warning | `VOID.Meridian.PolymorphicDistrictField` | GameplayGraph.json | the_core_gate `$.mission_relationships[19].district` | 'district' value 'the_core_gate' is not a district id; in GameplayGraph.mission_relationships this field also carries sub-locations/transitions. |
| Warning | `VOID.Meridian.PolymorphicDistrictField` | GameplayGraph.json | the_white_void `$.mission_relationships[21].district` | 'district' value 'the_white_void' is not a district id; in GameplayGraph.mission_relationships this field also carries sub-locations/transitions. |
| Warning | `VOID.Meridian.PolymorphicDistrictField` | GameplayGraph.json | undercroft_to_sector_0 `$.mission_relationships[5].district` | 'district' value 'undercroft_to_sector_0' is not a district id; in GameplayGraph.mission_relationships this field also carries sub-locations/transitions. |
| Warning | `VOID.Meridian.UnlistedFile` | PackageManifest.json |   | 7 file(s) on disk are not inventoried by PackageManifest: CompatibilityMatrix.md, DependencyGraph.json, EngineeringHandoff.md, Meridian_Master_Index.md, PackageManifest.json, ValidationReport.md, ValidationSchema.json. |
| Info | `VOID.Meridian.AmbiguousOrderFields` | DistrictRegistry.json |  `$.districts[*]` | DistrictRegistry has both build_order_index (authoring order) and generation_priority (generation order); they intentionally disagree. Generators must use Meridian_Master.generation_pipeline.generation_order only. |
| Info | `VOID.Meridian.FlaggedOpenItem` | Meridian_Master.json | archive_maintenance_directive_mechanism `$.flagged_open_items` | Open item 'archive_maintenance_directive_mechanism' (moderate, unresolved_pending_writing_team_review); blocks: ['reset_mechanic_technical_implementation']. |
| Info | `VOID.Meridian.FlaggedOpenItem` | Meridian_Master.json | sector_0_ownership_questions_format_deviation `$.flagged_open_items` | Open item 'sector_0_ownership_questions_format_deviation' (low, unresolved_pending_lightweight_signoff); blocks: nothing. |
| Info | `VOID.Meridian.FlaggedOpenItem` | Meridian_Master.json | spire_server_hub_nyx_discrepancy `$.flagged_open_items` | Open item 'spire_server_hub_nyx_discrepancy' (highest_in_project, unresolved_by_design); blocks: ['server_hub_combat_encounter_generation']. |
| Info | `VOID.Meridian.FlaggedOpenItem` | Meridian_Master.json | undercroft_internal_radial_belt_model `$.flagged_open_items` | Open item 'undercroft_internal_radial_belt_model' (low, unresolved_pending_lightweight_signoff); blocks: nothing. |
| Info | `VOID.Meridian.FlaggedOpenItem` | Meridian_Master.json | white_zones_kit_of_parts_typology `$.flagged_open_items` | Open item 'white_zones_kit_of_parts_typology' (low, unresolved_pending_lightweight_signoff); blocks: ['additional_network_node_generation']. |
| Info | `VOID.Meridian.NoGeometrySource` | Meridian_Master.json |  `$.builder_configuration` | Meridian_Master is coordinate-free by design (builder_configuration.coordinate_policy). FVoidDesignPackage/FVoidRoadGenerator require centerlinePoints and footprints; a layout stage must synthesise them. |
| Info | `VOID.Meridian.NoSchemaForFile` | /home/claude/master/The-Voids-Calamity-master/Meridian Master |   | 5 JSON file(s) have no companion .schema.json and were only syntax-checked: BuilderManifest.json, DependencyGraph.json, Metro_Archives_data.json, PackageManifest.json, ValidationSchema.json. |
| Info | `VOID.Meridian.SubLocationCheckSkipped` | WorldPartition.json | olympus_spire  | VR-004 for 'olympus_spire' skipped: Olympus_Spire_data.json is not present. |
| Info | `VOID.Meridian.SubLocationCheckSkipped` | WorldPartition.json | sector_0  | VR-004 for 'sector_0' skipped: Sector_0_data.json is not present. |
| Info | `VOID.Meridian.SubLocationCheckSkipped` | WorldPartition.json | undercroft  | VR-004 for 'undercroft' skipped: VOID_Location_01_Undercroft_data.json is not present. |
| Info | `VOID.Meridian.SubLocationCheckSkipped` | WorldPartition.json | white_zones  | VR-004 for 'white_zones' skipped: White_Zones_data.json is not present. |

## Missing files (VR-011 / locked inputs)

- `Olympus_Spire.md`
- `Olympus_Spire_ValidationReport.md`
- `Olympus_Spire_data.json`
- `Olympus_Spire_schematic.svg`
- `Sector_0.md`
- `Sector_0_ValidationReport.md`
- `Sector_0_data.json`
- `Sector_0_schematic.svg`
- `VOID_Location_01_The_Undercroft.md`
- `VOID_Location_01_Undercroft_data.json`
- `White_Zones.md`
- `White_Zones_ValidationReport.md`
- `White_Zones_data.json`
- `White_Zones_schematic.svg`

## Reproduce

```
python Tools/meridian_reference_validator.py "Meridian Master" --json Reports/MeridianFirstLookReport.json
```
