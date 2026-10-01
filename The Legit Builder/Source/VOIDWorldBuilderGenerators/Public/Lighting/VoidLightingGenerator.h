// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IVoidGenerator.h"

class UWorld;

/**
 * FVoidLightingGenerator  (Phase 13 -- Agent 7)
 *
 * Registered under the id "Lighting". Consumes the same imported
 * FVoidDesignPackage the Road generator uses and builds, for that
 * package's district:
 *
 *   - AVoidDistrictLightingActor: instanced street poles / lamp heads,
 *     traffic + pedestrian signals, window plates, neon-sign hooks,
 *     tunnel-portal emergency beacons, and a budgeted set of real lights;
 *   - AVoidLandmarkLightingActor per Meridian-registered landmark building;
 *   - the (single) AVoidLightingDirector, sun/sky/fog/post-process
 *     adoption-or-spawn, preset library and cinematic anchors.
 *
 * Inputs are the package's roads + buildings (not the spawned road actors:
 * the road geometry is re-derived with the Road generator's own public
 * builders so lamp positions match the road surface exactly).
 *
 * Regenerating for the same district id replaces that district's lighting
 * actors; nothing else is touched. Runs inside one undo transaction.
 */
class VOIDWORLDBUILDERGENERATORS_API FVoidLightingGenerator : public IVoidGenerator
{
public:
	//~ Begin IVoidGenerator
	virtual FName GetGeneratorId() const override;
	virtual bool Generate(const FVoidDesignPackage& Package, FVoidGenerationContext& Context) override;
	//~ End IVoidGenerator

	/** Switches the world's director to a preset. @return false if there is no director or no such preset. */
	static bool ApplyPreset(UWorld* World, FName PresetId);

	/**
	 * Removes generated lighting actors. DistrictId == NAME_None removes every district. The director (and any
	 * adopted or spawned sun/sky/fog/post actors) is left alone unless bRemoveDirector; adopted actors are restored first.
	 * @return number of actors destroyed.
	 */
	static int32 ClearLighting(UWorld* World, FName DistrictId = NAME_None, bool bRemoveDirector = false);

	/** Step-1 audit: what lighting-relevant actors and CVars exist right now. Read-only. */
	static void AuditWorld(UWorld* World, TArray<FString>& OutLines);
};
