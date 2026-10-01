// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "District/VoidDistrictGenerationSettings.h"

UVoidDistrictGenerationSettings::UVoidDistrictGenerationSettings()
{
	// Designer-tunable character defaults for qualities the Meridian data states as prose only.
	// White Zones: "wide, unobstructed, plazas" (Master Plan Sec. 16) -> more open space than the generic High band.
	FVoidDistrictCharacterOverride WhiteZones;
	WhiteZones.OpenSpaceRatio = 0.25;
	CharacterOverrides.Add(FName(TEXT("white_zones")), WhiteZones);
}
