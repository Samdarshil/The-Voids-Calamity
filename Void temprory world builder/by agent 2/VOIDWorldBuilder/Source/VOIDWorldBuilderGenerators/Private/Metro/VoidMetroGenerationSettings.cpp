// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Metro/VoidMetroGenerationSettings.h"

UVoidMetroGenerationSettings::UVoidMetroGenerationSettings()
{
	CategoryName = TEXT("Plugins");

	// Band names: DistrictRegistry.json hierarchy.radial_bands_ordered (+ 'inner_mid_outer_continuous', the Undercroft's own band label).
	// Radii: PLACEHOLDERS (cm). Not canon.
	BandRadiusUnits = {
		{ TEXT("core"), 0.0f },
		{ TEXT("inner_rings_unbuilt"), 200000.0f },
		{ TEXT("mid_tier_rings"), 400000.0f },
		{ TEXT("seam_zone"), 550000.0f },
		{ TEXT("outer_rings"), 700000.0f },
		{ TEXT("off_gradient"), 800000.0f },
		{ TEXT("inner_mid_outer_continuous"), 400000.0f },
	};

	// Copied from DistrictRegistry.json districts[].radial_band.
	DistrictBand = {
		{ TEXT("olympus_spire"), TEXT("core") },
		{ TEXT("white_zones"), TEXT("mid_tier_rings") },
		{ TEXT("metro_archives"), TEXT("seam_zone") },
		{ TEXT("undercroft"), TEXT("inner_mid_outer_continuous") },
		{ TEXT("sector_0"), TEXT("off_gradient") },
	};

	// PLACEHOLDER bearings. Meridian gives none.
	DistrictAzimuthDegrees = {
		{ TEXT("olympus_spire"), 0.0f },
		{ TEXT("white_zones"), 0.0f },
		{ TEXT("undercroft"), 200.0f },
		{ TEXT("metro_archives"), 130.0f },
		{ TEXT("sector_0"), 260.0f },
	};

	MetroMaterial = FSoftObjectPath(TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
}
