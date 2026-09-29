// Copyright VOID Engineering Studio. Internal tool - not for shipping content.

#include "Road/VoidRoadValidator.h"

FVoidValidationReport FVoidRoadValidator::Validate(const FVoidDistrictData& District)
{
	FVoidValidationReport Report;
	Report.bIsValid = true;

	ValidateBridgeTunnelExclusivity(District, Report);
	ValidateConnectionIdsResolve(District, Report);
	ValidateRoundabouts(District, Report);

	return Report;
}

void FVoidRoadValidator::ValidateBridgeTunnelExclusivity(const FVoidDistrictData& District, FVoidValidationReport& Report)
{
	for (int32 Index = 0; Index < District.Roads.Num(); ++Index)
	{
		const FVoidRoadSpec& Road = District.Roads[Index];
		if (Road.bIsBridge && Road.bIsTunnel)
		{
			Report.AddError(
				FString::Printf(TEXT("Road '%s' is flagged as both a bridge and a tunnel."), *Road.Id.Value.ToString()),
				FString::Printf(TEXT("district.roads[%d]"), Index),
				TEXT("VOID.RoadGen.BridgeTunnelConflict"),
				TEXT("Set only one of isBridge / isTunnel to true."));
		}
	}
}

void FVoidRoadValidator::ValidateConnectionIdsResolve(const FVoidDistrictData& District, FVoidValidationReport& Report)
{
	TSet<FName> KnownIds;
	KnownIds.Add(District.DistrictId.Value);
	for (const FVoidRoadSpec& Road : District.Roads)
	{
		KnownIds.Add(Road.Id.Value);
	}

	for (int32 Index = 0; Index < District.Roads.Num(); ++Index)
	{
		const FVoidRoadSpec& Road = District.Roads[Index];
		for (const FVoidElementId& ConnectionId : Road.ConnectionIds)
		{
			if (!KnownIds.Contains(ConnectionId.Value))
			{
				Report.AddError(
					FString::Printf(TEXT("Road '%s' references unknown connectionId '%s'."), *Road.Id.Value.ToString(), *ConnectionId.Value.ToString()),
					FString::Printf(TEXT("district.roads[%d].connectionIds"), Index),
					TEXT("VOID.RoadGen.UnresolvableConnection"),
					TEXT("Fix the id, or remove it from connectionIds if it doesn't refer to another road in this district."));
			}
		}
	}
}

void FVoidRoadValidator::ValidateRoundabouts(const FVoidDistrictData& District, FVoidValidationReport& Report)
{
	for (int32 Index = 0; Index < District.Roads.Num(); ++Index)
	{
		const FVoidRoadSpec& Road = District.Roads[Index];
		if (Road.RoadType != EVoidRoadType::Roundabout)
		{
			continue;
		}

		if (Road.RoundaboutRadiusUnits <= 0.0f)
		{
			Report.AddError(
				FString::Printf(TEXT("Roundabout '%s' has no positive roundaboutRadiusUnits and will not generate."), *Road.Id.Value.ToString()),
				FString::Printf(TEXT("district.roads[%d].roundaboutRadiusUnits"), Index),
				TEXT("VOID.RoadGen.InvalidRoundabout"),
				TEXT("Set a positive roundaboutRadiusUnits."));
		}

		if (Road.CenterlinePoints.Num() == 0)
		{
			Report.AddError(
				FString::Printf(TEXT("Roundabout '%s' has no centerlinePoints to use as its center."), *Road.Id.Value.ToString()),
				FString::Printf(TEXT("district.roads[%d].centerlinePoints"), Index),
				TEXT("VOID.RoadGen.InvalidRoundabout"),
				TEXT("Provide exactly one [x, y] point to use as the roundabout's center."));
		}

		const bool bHasAnySpur = District.Roads.ContainsByPredicate([&Road](const FVoidRoadSpec& Other)
		{
			return Other.Id.Value != Road.Id.Value && Other.ConnectionIds.ContainsByPredicate([&Road](const FVoidElementId& Id) { return Id.Value == Road.Id.Value; });
		});

		if (!bHasAnySpur)
		{
			Report.AddWarning(
				FString::Printf(TEXT("Roundabout '%s' has no other road referencing it via connectionIds; it may still connect via coincident endpoints, but consider adding explicit connectionIds for clarity."), *Road.Id.Value.ToString()),
				FString::Printf(TEXT("district.roads[%d]"), Index),
				TEXT("VOID.RoadGen.UnconnectedRoundabout"));
		}
	}
}
