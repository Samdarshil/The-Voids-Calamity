# AGENT 1 — FILE OWNERSHIP

Paths are relative to `VOIDWorldBuilder/` (the plugin root). “Core/Import/Generators/Editor” = `Source/VOIDWorldBuilder<Name>/`.

## SAFE TO MODIFY
Files nobody else should conflict on; Agents 2–7 may extend these freely (additively).

- `Core/Public/Data/VoidMeridianData.h` — add typed structs for more registries (Metro, Navigation, Landmark, …). Additive only; do not rename existing fields.
- `Import/Private/VoidMeridianImporter.cpp` — add typed-mapping blocks for other registries **after** the road-network block. Do not change import order, schema/checksum/spatial checks.
- `Generators/Private/Tests/*` and `Core/Private/Tests/*`, `Import/Private/Tests/*` — add tests; don't weaken existing ones.
- **New files** in new folders for your generator (e.g. `Generators/Public/Building/…`, `Generators/Private/Building/…`).
- `Tools/meridian_reference_check.py` — add cross-checks for registries you type (keep it mirroring the C++).

## SHARED — MODIFY ONLY WITH CARE
Used by every generator. Changes must be additive, default-safe, and announced; a bad edit breaks Agents 1–7.

- `Generators/Private/VOIDWorldBuilderGeneratorsModule.cpp` — one `RegisterGenerator` + one `UnregisterGenerator` line per generator; nothing else.
- `Generators/VOIDWorldBuilderGenerators.Build.cs`, `Core/…Build.cs`, `Import/…Build.cs`, `Editor/…Build.cs` — add dependencies only; never remove or move Public/Private of existing ones (ProceduralMeshComponent and Json must stay Public).
- `VOIDWorldBuilder.uplugin` — add `Plugins`/modules only.
- `Core/Public/Interfaces/IVoidGenerator.h` — the generator contract. Add members **with defaults** only. Do not change existing signatures/semantics.
- `Core/Public/Settings/VoidWorldBuilderSettings.h` — add new settings/sections; do not change existing defaults or meaning of world-space/layout fields.
- `Core/Public/Types/VoidWorldBuilderTypes.h` — append enums only; never reorder or renumber.
- `Core/Public/Data/VoidRoadOutput.h` — the road contract. Add queries/fields; never change meaning of existing ones (Left/Right, tier, units).
- `Core/Public/Data/VoidDesignPackage.h` — legacy schema; additive with default values only.
- `Generators/Public/VoidGeneratorPipeline.h` / `Private/VoidGeneratorPipeline.cpp` — ordering/skip semantics are relied on by all generators.
- `Generators/Public/VoidGeneratorRegistry.h` / cpp.
- `Editor/Private/VoidWorldBuilderCommandlet.cpp` — add new flags; keep `-Package=` and `-Meridian=` behaviour and exit codes.
- Panel/menu files in `Editor/` — UI changes for new generators, without altering existing Road/import behaviour.

## DO NOT MODIFY
Agent 1’s verified/locked behaviour. Request changes from Agent 1 (or the integrator) instead.

- `Core/Public/Coordinates/VoidWorldSpace.h` + `Core/Private/Coordinates/VoidWorldSpace.cpp` — **the one coordinate conversion.** Never add a second conversion anywhere.
- `Core/Public/Utilities/VoidSha256.h` + cpp — verified against NIST vectors.
- `Import/Public/VoidJsonSchemaLite.h` + cpp — keyword subset matches the Meridian schemas; do not swap in another validator.
- `Import/Public/VoidMeridianImporter.h` and the **validation portions** of `VoidMeridianImporter.cpp` (schema id checks, required-registry halt, reference/duplicate checks, VR-012 spatial scan, canon-lock checksums).
- `Generators/Public/Road/VoidMeridianRoadPlanner.h` + cpp — Meridian→road derivation rules (only what Meridian states).
- `Generators/Public|Private/Road/VoidRoadGenerator.*` — Road Generator (output publication, coordinate normalisation, local-origin rebasing, Reset).
- `Generators/Private/Road/VoidRoadIntersectionBuilder.cpp`, `…SplineBuilder`, `…MeshBuilder`, `…BridgeTunnelBuilder`, `…Validator`, `VoidRoadActor`, `VoidRoadTypeProfile`, `VoidRoadGenerationSettings` — existing road internals.
- `Core/Public/Data/VoidValidationReport.h`, `VoidSchemaVersion.h`, `VoidImportContext.h`, `Core/Public/VoidWorldBuilderLog.h` — shared diagnostics types.
- `Import/…/VoidDesignPackageImporter.*`, `VoidJsonPackageReader.*`, `VoidPackageValidator.*`, reader registry — legacy import path (a working system; preserve).
- The Meridian source data (`Meridian Master/*`) — locked/generated canon. Never edit to make the importer pass.
