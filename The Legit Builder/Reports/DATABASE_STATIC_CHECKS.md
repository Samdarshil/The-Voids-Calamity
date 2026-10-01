# Automated package checks

- Meridian JSON files parsed: 46
- JSON syntax errors: 0
- Direct same-basename schema pairs validated: 12
- Schema validation errors: 0
- District IDs in DistrictRegistry: ['metro_archives', 'olympus_spire', 'sector_0', 'undercroft', 'white_zones']
- Expected five IDs all present: True
- Extra registry district IDs: []

## Schema pairs
- Meridian_Master/GameplayGraph.json against Meridian_Master/GameplayGraph.schema.json: PASS
- Meridian_Master/Meridian_Master.json against Meridian_Master/Meridian_Master.schema.json: PASS
- Meridian_Master/LandmarkRegistry.json against Meridian_Master/LandmarkRegistry.schema.json: PASS
- Meridian_Master/RoadNetwork.json against Meridian_Master/RoadNetwork.schema.json: PASS
- Meridian_Master/StreamingStrategy.json against Meridian_Master/StreamingStrategy.schema.json: PASS
- Meridian_Master/DistrictRegistry.json against Meridian_Master/DistrictRegistry.schema.json: PASS
- Meridian_Master/DataLayers.json against Meridian_Master/DataLayers.schema.json: PASS
- Meridian_Master/InfrastructureGraph.json against Meridian_Master/InfrastructureGraph.schema.json: PASS
- Meridian_Master/NavigationGraph.json against Meridian_Master/NavigationGraph.schema.json: PASS
- Meridian_Master/MetroNetwork.json against Meridian_Master/MetroNetwork.schema.json: PASS
- Meridian_Master/BuilderRules.json against Meridian_Master/BuilderRules.schema.json: PASS
- Meridian_Master/WorldPartition.json against Meridian_Master/WorldPartition.schema.json: PASS

## Interpretation
These checks cover syntax and direct schema pairs only. They do not independently reproduce every cross-file integrity assertion in the source package ValidationReport.md, nor prove that the C++ importer currently understands every schema. The package itself documents one missing standalone schema for `void_world_location_schema_v1`.
