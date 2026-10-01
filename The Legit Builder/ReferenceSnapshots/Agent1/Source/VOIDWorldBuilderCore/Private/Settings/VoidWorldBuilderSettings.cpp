// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Settings/VoidWorldBuilderSettings.h"

UVoidWorldBuilderSettings::UVoidWorldBuilderSettings()
{
	CategoryName = TEXT("Plugins");

	// GREYBOX PLACEHOLDER SCALE (Builder-authored, not Meridian data). 1 UU = 1 cm.
	// Widths chosen only so the placeholder city is a few km across and stays far inside MaxAbsCoordinateUU.
	// "off_gradient" (Sector 0) is deliberately absent: it does not participate in the radial gradient.
	BandRadii = {
		FVoidBandRadius(TEXT("core"),                0.0,      15000.0),
		FVoidBandRadius(TEXT("inner_rings_unbuilt"), 15000.0,  50000.0),
		FVoidBandRadius(TEXT("mid_tier_rings"),      50000.0,  100000.0),
		FVoidBandRadius(TEXT("seam_zone"),           100000.0, 120000.0),
		FVoidBandRadius(TEXT("outer_rings"),         120000.0, 180000.0)
	};
}

bool UVoidWorldBuilderSettings::FindBandRadius(FName BandId, double& OutInner, double& OutOuter) const
{
	for (const FVoidBandRadius& Band : BandRadii)
	{
		if (Band.BandId == BandId)
		{
			OutInner = Band.InnerRadiusUU;
			OutOuter = Band.OuterRadiusUU;
			return true;
		}
	}
	return false;
}

const UVoidWorldBuilderSettings* UVoidWorldBuilderSettings::Get()
{
	return GetDefault<UVoidWorldBuilderSettings>();
}
