# HANDOFF — Agent 7: Lighting + Cinematic Presentation (Phase 13)

Target: Unreal Engine 5.8.2. Plugin: `Plugins/VOIDWorldBuilder` (extended in place).

## 0. Read this first — verification status

**Nothing in this submission has been compiled, loaded, or run.** The authoring sandbox had no Unreal Engine and no network. What *was* done:

- Brace/paren/bracket balance check on all 99 source files (clean).
- Every `.generated.h` include is present and last in its header.
- Both JSON config files parse as valid JSON.
- Every VOID-internal API the new code calls (road builders, junction graph, `FVoidDesignPackage`, `IVoidGenerator`, registry) was cross-checked by grep against the supplied headers.

What was **not** done, and therefore is **not claimed**: compile, plugin load, lighting generation, preset switching, light placement, regeneration, performance measurement. Automation tests exist (section 10) but have never been executed.

**Engine APIs written from memory and most likely to need a touch-up on first compile** (none were checked against 5.8.2 headers):

- `UDirectionalLightComponent::SetAtmosphereSunLightIndex`, `SetAtmosphereSunLight`
- `USkyAtmosphereComponent::SetSkyLuminanceFactor`, `SetMieScatteringScale`, `SetHeightFogContribution`
- `UExponentialHeightFogComponent::SetVolumetricFogAlbedo` (takes `FColor`), `SetVolumetricFogDistance`, `SetFogInscatteringColor`
- `USkyLightComponent::SetRealTimeCaptureEnabled`, `SetLowerHemisphereColor`, `RecaptureSky`
- `FPostProcessSettings` override flags / fields (`DynamicGlobalIlluminationMethod`, `ReflectionMethod`, `ColorGradingLUT`, `AutoExposureMin/MaxBrightness`)
- `UMaterialEditingLibrary::CreateMaterialExpression / ConnectMaterialExpressions / ConnectMaterialProperty`, `UMaterial::bUsedWithInstancedStaticMeshes`, `UMaterialExpressionPerInstanceCustomData::DataIndex / ConstDefaultValue`
- `AActor::SetFolderPath`, `UInstancedStaticMeshComponent::SetCustomDataValue` argument order
- `FJsonObjectConverter` mapping of `FName`, enums (by entry name, e.g. `"Highway"`), and `FLinearColor` (`{"r","g","b","a"}`) in the two JSON files

The numeric defaults in the presets/profiles (lux, lumens, emissive, Kelvin, fog density) are starting points. None has been seen on screen.

## 1. What the supplied project actually contained

The brief assumes generated Roads, Buildings, Districts, Environment and Props. The supplied plugin implements **only Phase 1–3: package import and the Road generator.** There is no Building, District, Environment or Prop generator and no building actors. Consequences, all handled explicitly:

- Lighting consumes the **design package** (`FVoidDesignPackage`: roads + building footprints/heights/types) rather than spawned building actors. Road geometry is re-derived with the Road generator's own public builders so lamp positions match the road surface.
- Building footprints are in "district-local space" with no elevation. They are used as world XY at `BuildingGroundZ` (setting, default 0). If a future Building generator places districts at an offset, this is the one thing to revisit.
- Props and Environment are not inputs (nothing exists to read). The audit logs this.
- The plugin descriptor said `EngineVersion 5.6.0`; changed to `5.8.0`. The existing Phase 1–3 code was not audited against 5.8.2.

Meridian metadata used: `DistrictRegistry.json`, `LandmarkRegistry.json` (from the Meridian Master folder), plus lighting philosophy text in `Meridian_Master_Plan.md` / `Metro_Archives.md`. **`Sector_0.md` (the approved definition of Sector 0's four dynamic lighting states) was not in the package.** The four Sector 0 states in `LightingProfiles.json` are flagged `bProvisional` placeholders. Landmarks are never invented: a landmark exists only if the registry lists it (or you bind it explicitly).

## 2. Files added

```
Plugins/VOIDWorldBuilder/
  Config/VOIDLighting/LightingPresets.json
  Config/VOIDLighting/LightingProfiles.json
  Source/VOIDWorldBuilderLighting/            (new Runtime module)
    VOIDWorldBuilderLighting.Build.cs
    Public/  VoidLightingTypes.h  VoidLightingSettings.h  VoidLightingConfig.h  VoidLightingMetadata.h
             VoidLightingActorBase.h  VoidDistrictLightingActor.h  VoidLandmarkLightingActor.h
             VoidLightingDirector.h  VoidLightingLog.h  VOIDWorldBuilderLightingModule.h
    Private/ (matching .cpp) + Tests/VoidLightingDataTests.cpp
  Source/VOIDWorldBuilderGenerators/Public/Lighting/VoidLightingGenerator.h
  Source/VOIDWorldBuilderGenerators/Private/Lighting/VoidLightingGenerator.cpp
  Source/VOIDWorldBuilderGenerators/Private/Lighting/VoidLightingMaterialBuilder.{h,cpp}
  Source/VOIDWorldBuilderGenerators/Private/Tests/VoidLightingGeneratorTests.cpp
HANDOFF_AGENT7_LIGHTING.md   LIGHTING_PRESET_REFERENCE.md
```

## 3. Files changed

| File | Change |
|---|---|
| `VOIDWorldBuilder.uplugin` | EngineVersion 5.6.0 → 5.8.0; version 0.2.0-agent7-lighting; new `VOIDWorldBuilderLighting` Runtime module |
| `VOIDWorldBuilderGenerators.Build.cs` | + `VOIDWorldBuilderLighting`, `MaterialEditor`, `AssetRegistry` |
| `VOIDWorldBuilderEditor.Build.cs` | + `VOIDWorldBuilderLighting` |
| `VOIDWorldBuilderGeneratorsModule.cpp` | registers/unregisters generator id `Lighting` |
| `SVoidWorldBuilderPanel.{h,cpp}` | *Generate Lighting*, *Clear Lighting*, one button per preset; `RunGenerator` helper (Roads button untouched) |
| `README.md` | status note |

## 4. Architecture in one paragraph

JSON presets/profiles → `FVoidLightingConfig`. The `Lighting` generator (editor-only) turns package roads/buildings + Meridian registries into **one `AVoidDistrictLightingActor` per district**, **one `AVoidLandmarkLightingActor` per registered landmark building**, and **one `AVoidLightingDirector` per world**. Emissive things (lamp heads, windows, neon, signals, beacons) are **instanced static meshes sharing one emissive material**; per-instance custom data (on-threshold, warm/cool blend) means a preset switch is a few material-parameter writes, not per-instance work. Real lights are a small budgeted set. Actors have **no Tick**. The runtime module has no editor dependencies, so generated content works in PIE, Movie Render Queue and packaged builds (the preset library is copied onto the director).

## 5. Lighting API

**Editor / generation** (`FVoidLightingGenerator`, generator id `Lighting`, also reachable from the panel):
- `Generate(Package, Context)` — regenerates that district's lighting (replaces, never duplicates), one undo transaction
- `static ApplyPreset(World, PresetId)` · `ClearLighting(World, DistrictId=None, bRemoveDirector=false)` · `AuditWorld(World, OutLines)`

**Runtime** (`AVoidLightingDirector`, Blueprint-callable): `ApplyPresetById`, `GetPresetIds`, `SetEmergencyMode`, `SetGlobalLandmarkEmphasis`, `RefreshRegistrations`, `EnsureEnvironmentActors`, `RestoreOriginalLighting`, `BuildStatsReport`; data: `PresetLibrary`, `ActivePresetId`, `Anchors`, `WorldBounds`.
`AVoidDistrictLightingActor`: `ApplyPreset`, `SetStateIndex`. `AVoidLandmarkLightingActor`: `ApplyPreset`, `SetControls`, `Refresh`, editable `Controls`.

**Project Settings → Plugins → VOID World Builder Lighting** (`UVoidLightingSettings`): Meridian metadata directory, landmark bindings, config override dir, default preset, existing-lighting policy, post-process volume on/off + priority, LUT, material path/override, all budgets, skyline count, `BuildingGroundZ`.

## 6. Presets

`Day`, `GoldenHour`, `Night`, `Overcast` — fully documented in `LIGHTING_PRESET_REFERENCE.md`. Add more by editing the JSON; no C++ needed. Presets switch instantly (no blending/crossfade — see limitations).

## 7. Existing lighting / project settings

- **No project settings, `.ini` files, console variables or World Settings are modified.** Nothing is written to `DefaultEngine.ini`.
- Step-1 audit logs existing sun/sky/atmosphere/fog/post actors and the CVars `r.DynamicGlobalIlluminationMethod`, `r.ReflectionMethod`, `r.Shadow.Virtual.Enable`, `r.Nanite`, `r.VolumetricFog`, extended-luminance default.
- Policy `Adopt` (default): drives the first existing sun/sky/atmosphere/fog, after storing a snapshot (`RestoreOriginalLighting` puts it back). Policy `LeaveExisting`: existing actors are untouched; missing ones are still spawned; presets then only drive generated lights and our post volume. Spawned actors are tagged `VOID.Lighting.Generated`.
- Post-process is **never adopted**: we add our own unbound volume at priority −10 so hand-authored volumes and cinematic-camera post-process win. Depth of field is deliberately never overridden.
- Lumen is not forced (`bForceLumen` defaults false). **Recommended project setup, to be checked by the first-look team, not applied by this plugin:** Lumen GI + reflections, virtual shadow maps, Nanite on the generated geometry.

## 8. Performance design

- No Tick anywhere. Preset switch = ~10 component setters + one sky recapture + one loop over a handful of actors.
- Window/lamp/sign/signal emissives are ISMs with static mobility and no shadows, no collision, no nav. One material, one MID per element per actor.
- Real (dynamic, Movable) lights are capped: `MaxRealStreetLightsPerDistrict` (default 256) chosen by road priority, shadowless by default; landmark lights fixed by tier (≤4 uplights + 1 halo each); `MaxLandmarkBuildingsPerLandmark` caps clusters.
- Window caps per building and per district; over-cap buildings get a coarser window grid (same coverage), landmark buildings are processed first. Caps log a warning, never fail.
- Volumetric fog is on only in `Night`. Real lights with < 1 lumen after scaling are hidden.
- Unmeasured: no frame-time, draw-call, or memory numbers exist. Treat the budgets as guesses until profiled.

## 9. Cinematic hooks (data only — no camera script)

`AVoidLightingDirector::Anchors` (`FVoidCinematicAnchor`: id, type, district, landmark id, location, rotation, radius, height, tier, suggested camera location / look-at) of types `Landmark`, `DistrictCenter`, `MajorRoad` (Highway/Primary midpoints with tangent), `CameraInterest` (busiest junctions, roundabouts), `SkylinePoint` (tallest separated buildings). Query with `GetAnchorsOfType`, `FindAnchor`, `FindLandmarkAnchor`; `WorldBounds`, `GetWorldCenter/Extent`. Anchors are regenerated per district. Landmark dials for Sequencer/Details: `Emphasis`, `NightIllumination`, `SilhouetteReadability`, `AtmosphericEmphasis`, `bBeaconEnabled`. Post grade hooks per preset: exposure bias, optional exposure range, bloom, vignette, saturation, contrast, gain tint, optional LUT.

## 10. Tests (written, not run)

`VOID.WorldBuilder.Lighting.Config.*` (shipped JSON loads; all four presets; road hierarchy ordering; building classification incl. residential-before-commercial; district profile spectrum; registry parsing incl. interior landmarks) and `VOID.WorldBuilder.Lighting.Generator.GeneratesAndSwitchesPresets` (generate, anchors, tunnel beacons, preset switching changes the sun, regeneration keeps actor count, clear). The generator test assumes it may create a world and write a material under `/Engine/Transient`; it may need adjusting.

## 11. Known limitations

1. Not compiled or run (section 0).
2. No building/prop/environment actors exist to integrate with; footprints assumed world-space at `BuildingGroundZ`.
3. One district per package (the package format holds one). Anchors/skyline selection are per district; world bounds are the union.
4. Landmark matching is by exact id, `BuildingType`, or `<landmarkId>_*` prefix against the registry, plus explicit bindings. A registry landmark with no matching building gets no lighting (logged by omission, not invented).
5. Sector 0 states are provisional placeholders.
6. Traffic signals show one static lens per approach; cycling would need a timer, deliberately avoided. Pedestrian signals and traffic poles are simple boxes.
7. Street lamps skip tunnel roads; bridges get lamps at their elevated deck points but no extra bridge-specific lighting. Roundabout lamps are outside only.
8. Neon signs are placeholder boxes plus `NeonHooks` transforms for a later art pass. Road-facing edge detection is brute-force (O(edges × road segments)) — fine for greybox scale, slow for very large districts.
9. Preset switching is instant; no crossfade/time-of-day animation.
10. Window plates are flat boxes on the footprint walls; no interior/parallax, and footprints with very short edges (< 150 u) get no windows.
11. Skyline windows disappear if you set `InstanceEndCullDistance > 0` (off by default).
12. Sky atmosphere night look depends on the moon directional light taking atmosphere-light index 1; if the project already uses index 1, change it on the `VOID_Moon` actor.
13. Emissive material is created on first generate at `/Game/VOID/Lighting/M_VOID_LightingEmissive` (saved to disk). If creation fails, generation continues and warns; emissives then render black.

## 12. Integration instructions

1. Copy `Plugins/VOIDWorldBuilder` into the project's `Plugins/` (replacing the Phase 3 copy), regenerate project files, build for the editor target. Expect to fix small API mismatches from section 0.
2. Enable the plugin. Project Settings → Plugins → **VOID World Builder Lighting**: set *Meridian Metadata Directory* to the `Meridian Master` folder (enables landmark tiers and below-grade detection).
3. Import a package and run *Generate Roads*, then *Generate Lighting* (panel), or call the `Lighting` generator programmatically.
4. Pick a preset from the panel buttons, or select `VOID_LightingDirector` → *Preview Preset Id* → *Apply Preview Preset*.
5. Read `BuildStatsReport()` and the generation summary for counts; tune budgets in settings and values in the JSON (re-run *Generate Lighting* to copy the library onto the director).
6. When a Building generator exists: if it offsets districts, add that offset in the lighting generator's footprint → world conversion; use `LandmarkBindings` if its building ids don't equal registry landmark ids.
