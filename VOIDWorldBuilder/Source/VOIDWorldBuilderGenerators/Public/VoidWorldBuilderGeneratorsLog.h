// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

/**
 * Shared log category for every generator in VOIDWorldBuilderGenerators
 * (Road today; Building, Navigation, World Partition, Data Layer, and
 * Gameplay Volume in later phases). One category per module -- matching
 * LogVoidWorldBuilder (Core) and LogVoidImport (Import) -- rather than
 * one category per generator, so a designer can filter to "everything
 * about generation" without needing to know in advance which generator
 * produced a given line, while still keeping it separate from import-time
 * noise.
 */
VOIDWORLDBUILDERGENERATORS_API DECLARE_LOG_CATEGORY_EXTERN(LogVoidGenerators, Log, All);
