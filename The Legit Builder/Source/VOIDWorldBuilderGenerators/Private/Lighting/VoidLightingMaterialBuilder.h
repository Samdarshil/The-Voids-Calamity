// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;

/**
 * Creates (once) the shared emissive master material used by every
 * instanced lighting element:
 *
 *   Emissive = lerp(LightColorA, LightColorB, CustomData[1])
 *              * Intensity
 *              * saturate(ceil(LitFraction - CustomData[0]))
 *
 * Base colour is black, so lit plates are pure emitters (they still feed
 * Lumen as emissive surfaces). One material + one MID per element per
 * actor means a preset switch is a handful of scalar/vector writes.
 */
class FVoidLightingMaterialBuilder
{
public:
	/** Loads the asset at AssetPath, or builds and saves it if missing. Returns null on failure (reason in OutError). */
	static UMaterialInterface* FindOrCreate(const FString& AssetPath, FString& OutError);
};
