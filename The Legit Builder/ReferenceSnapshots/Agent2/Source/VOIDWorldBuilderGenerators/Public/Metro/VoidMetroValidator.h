// Copyright VOID Engineering Studio. Internal tool - not for shipping content.
// Added by Agent 2 (Phase 6 Metro Generator).

#pragma once

#include "CoreMinimal.h"
#include "Data/VoidMetroData.h"
#include "Data/VoidValidationReport.h"
#include "Metro/VoidMetroLayoutResolver.h"

/**
 * FVoidMetroValidator
 *
 * Generation-time validation for metro data (distinct from import-time shape
 * checks, same split as FVoidRoadValidator). Enforces the Meridian
 * constraints that are stated in the supplied data:
 *   - Live and Dead networks are never merged (MetroNetwork.json network_model).
 *   - The Dead network has no interchanges (interchanges: []).
 *   - Sector 0 has a single access point; no additional entrances may be generated.
 *   - Every cross-file id reference resolves (BuilderRules cross_file_inconsistency_error).
 * Errors abort generation (see FVoidMetroGenerator).
 */
class VOIDWORLDBUILDERGENERATORS_API FVoidMetroValidator
{
public:
	/** Data-level checks (before any layout is resolved). Result.bIsValid is true iff no Error/Fatal issue was added. */
	static FVoidValidationReport Validate(const FVoidMetroData& Data);

	/** Layout-level checks (after resolution): networks share no infrastructure. Appends to Report. */
	static void ValidateResolved(const FVoidMetroResolvedLayout& Layout, FVoidValidationReport& Report);
};
