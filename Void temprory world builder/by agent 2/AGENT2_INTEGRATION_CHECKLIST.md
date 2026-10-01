# AGENT 2 — Integration Checklist (Metro Generator + World Partition support)

Do these in order. Nothing here has been compiled (see `HANDOFF_AGENT2.md` §0). Step 6 is the first real verification.

## 1. Baseline
- [ ] Start from the final Builder (Agent 1 + Agent 3 merged). Note whether it still has `FVoidDesignPackage`, `FVoidJsonPackageReader::MapJsonObjectToPackage`, `FVoidGeneratorRegistry`, `IVoidGenerator` with the Phase 3 signatures (`GetGeneratorId()`, `Generate(const FVoidDesignPackage&, FVoidGenerationContext&)`). If any changed, adapt only the call sites named below.

## 2. Copy the additive files (no conflicts expected)
Copy from this ZIP's `VOIDWorldBuilder/` into the final plugin, same relative paths:
- [ ] `Source/VOIDWorldBuilderCore/Public/Types/VoidMetroTypes.h`
- [ ] `Source/VOIDWorldBuilderCore/Public/Data/VoidMetroData.h`
- [ ] `Source/VOIDWorldBuilderImport/Public/VoidMetroNetworkMapper.h` and `Private/VoidMetroNetworkMapper.cpp`
- [ ] `Source/VOIDWorldBuilderGenerators/Public/Metro/*` and `Private/Metro/*`
- [ ] `Source/VOIDWorldBuilderGenerators/Public/WorldPartition/*` and `Private/WorldPartition/*`
- [ ] `Source/VOIDWorldBuilderGenerators/Private/Tests/VoidMetroTests.cpp`
- [ ] `Docs/MetroGeneratorArchitecture.md`, `Docs/Samples/MetroAuthoredBlock.example.json`
- [ ] Append the "Metro Codes" section of `Docs/ValidationRules.md` to the final copy (merge, don't overwrite).

## 3. Apply the shared-file edits by hand (these are the merge-conflict points)

**3a. `Core/Public/Data/VoidDesignPackage.h`** — add the include and one member:
```cpp
#include "Data/VoidMetroData.h"
// ...inside USTRUCT FVoidDesignPackage, after District:
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VOID World Builder")
FVoidMetroData Metro;
```
If Agent 1 already added a normalized metro member, keep theirs and add a small adapter that fills `FVoidMetroData`; do **not** keep two representations.

**3b. `Import/Private/VoidJsonPackageReader.cpp`** — include `"VoidMetroNetworkMapper.h"`; add `TEXT("metro")` to `RootKnownFields`; before the district block in `MapJsonObjectToPackage`:
```cpp
const TSharedPtr<FJsonObject>* MetroObject = nullptr;
if (JsonObject->TryGetObjectField(TEXT("metro"), MetroObject))
{
    FVoidMetroNetworkMapper::MapPackageMetroBlock(*MetroObject, OutPackage.Metro, OutReport);
}
```
If Agent 1 owns a Meridian loader, have it call `FVoidMetroNetworkMapper::MapMeridianMetroNetwork` and `AppendMeridianTunnelRelationships`, then assign the result to `Package.Metro`. Optional: `MergeAuthoredOverrides` for authored coordinates.

**3c. `Generators/Private/VOIDWorldBuilderGeneratorsModule.cpp`** — include `"Metro/VoidMetroGenerator.h"`; in `StartupModule` add `FVoidGeneratorRegistry::Get().RegisterGenerator(MakeShared<FVoidMetroGenerator>());`; in `ShutdownModule` add `UnregisterGenerator(TEXT("Metro"));`.

**3d. `Generators/VOIDWorldBuilderGenerators.Build.cs`** — ensure `"DeveloperSettings"` is in `PublicDependencyModuleNames`; `ProceduralMeshComponent` and `UnrealEd` must stay in Private deps.

**3e. `VOIDWorldBuilder.uplugin`** — `"EngineVersion": "5.8.0"`; add `"Plugins": [{ "Name": "ProceduralMeshComponent", "Enabled": true }]`. Keep the module list as the final Builder has it.

**3f. Editor panel (optional UI)** — copy the Metro row, `OnLoadMetroClicked`, `OnGenerateMetroClicked`, `IsGenerateMetroEnabled` and the two members (`MetroPackage`, `bMetroLoaded`) from this ZIP's `SVoidWorldBuilderPanel.*`. If the panel was rewritten, skip it: call `FindGenerator("Metro")->Generate(...)` from whatever UI/commandlet drives generation instead.

## 4. Project settings (before first generate)
- [ ] Project Settings → Plugins → *VOID World Builder World Partition*: set `ChunkCellSizeUnits` to your map's runtime-grid cell size. Leave runtime grid names `None` unless those grids exist in World Settings.
- [ ] *VOID World Builder Metro Generation*: decide `bAllowPlaceholderLayout` (see HANDOFF §1). Confirm `MetroMaterial` loads (default `/Engine/EngineDebugMaterials/VertexColorMaterial`); if not, set a vertex-colour material.
- [ ] Optionally set `LiveNetworkDefaultGrade = Elevated` for a viaduct read (design choice, not canon).

## 5. Feed real data
- [ ] Put `MetroNetwork.json` and `RoadNetwork.json` in the same folder. Panel: **Browse → MetroNetwork.json → Load Meridian Metro**, or call the mapper from Agent 1's loader.
- [ ] Replace placeholders with authored coordinates when available (format: `Docs/Samples/MetroAuthoredBlock.example.json`).

## 6. Compile & verify (first time anything is verified)
- [ ] Generate project files; build the editor target. Fix compile errors — start with the API list in HANDOFF §9.
- [ ] Confirm UHT passes and the plugin loads (Output Log: `VOIDWorldBuilderGenerators module started`).
- [ ] Automation → filter `VOID.WorldBuilder` → run all 11 metro/WP tests. Any failure is a real finding; report it rather than editing the test to pass.

## 7. Smoke test in an empty map
- [ ] Generate Metro once. Expect log line `Metro: 5 stations, 4 track segments …` (radial spoke, ring, 2 tunnels), a `VOID.Metro.PlaceholderLayout` warning if placeholders are on, and actors under Outliner folder `VOID/Meridian/Metro/{Live,Dead}/{Track,Station}`.
- [ ] Look for inside-out surfaces → toggle `bFlipTriangleWinding`. Look for missing colours → material issue (HANDOFF §9.5).
- [ ] Sector 0 station has exactly one headhouse. No Dead-network actor is parented to or overlaps a Live one.
- [ ] **Regenerate a second time**: actor count must not grow (cleanup by `OwnerKey`). Ctrl+Z once: the whole run should undo as one transaction.
- [ ] Failure path: add a second entrance to Sector 0 in an authored block → generation must abort with `VOID.Metro.EntranceLimit` and leave existing actors untouched.

## 8. World Partition check (partitioned map only)
- [ ] Generate with the whole area loaded. Verify actors are spatially loaded, land in the expected runtime grid, and are grouped in the outliner folders. Track chunks should be about one per grid cell, with no gaps at chunk boundaries.
- [ ] Unload a region, regenerate, then reload: expect leftovers from the unloaded region (documented limitation). Load first.

## 9. Cross-system hooks
- [ ] Road/Building/Navigation code that needs metro locations reads `FVoidMetroExportRegistry::Get().Layout()` after Metro generation; no dependency on metro actor classes. Do not modify `FVoidRoadGenerator` to connect metro.
- [ ] Run order: Road → Metro is fine (metro does not read road output); pair lines to roads by `SharesRouteId` / `MapsToRoadCategory`.

## 10. Done when
- [ ] Editor build green, plugin loads, `"Metro"` appears in the registry.
- [ ] 11/11 automation tests pass.
- [ ] Smoke test §7 passes, including regeneration without actor growth.
- [ ] Decision recorded on placeholder layout vs authored coordinates.
