// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

/**
 * Shared log category for the entire VOID World Builder plugin.
 *
 * A single shared category (rather than one per module) is intentional:
 * this is an internal tool used by designers and engineers who filter the
 * Output Log by category, and one "LogVoidWorldBuilder" filter that
 * captures import, validation, and (later) generation output is more
 * useful day-to-day than four categories they'd have to enable separately.
 */
VOIDWORLDBUILDERCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogVoidWorldBuilder, Log, All);
