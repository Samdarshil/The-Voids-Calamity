// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

/**
 * Import-pipeline-specific log category, distinct from the plugin-wide
 * LogVoidWorldBuilder category declared in VOIDWorldBuilderCore.
 *
 * Why two categories: LogVoidWorldBuilder covers cross-cutting, low-volume
 * concerns (module startup/shutdown, generator registration) that every
 * part of the plugin shares. LogVoidImport covers the import pipeline
 * specifically, which is comparatively high-volume once Verbose logging
 * is enabled (per-field mapping trace, per-issue detail) -- exactly the
 * kind of output a designer debugging an import failure wants to filter
 * to in isolation, without either drowning in it by default or losing
 * the ability to get it when they need it.
 *
 * Default level is Log (mapping/validation summaries always show).
 * Enable Verbose via -LogCmds="LogVoidImport Verbose" or the Output Log's
 * category filter for full per-field mapping trace.
 */
VOIDWORLDBUILDERIMPORT_API DECLARE_LOG_CATEGORY_EXTERN(LogVoidImport, Log, All);
