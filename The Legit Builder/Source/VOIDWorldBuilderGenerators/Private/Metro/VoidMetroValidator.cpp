// Copyright VOID Engineering Studio. Internal tool - not for shipping content.
// Added by Agent 2 (Phase 6 Metro Generator).

#include "Metro/VoidMetroValidator.h"

namespace VoidMetroValidatorPrivate
{
	static bool IsFinite2D(const FVector2D& V)
	{
		return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y);
	}
}

FVoidValidationReport FVoidMetroValidator::Validate(const FVoidMetroData& Data)
{
	using namespace VoidMetroValidatorPrivate;

	FVoidValidationReport Report;
	Report.bIsValid = true;

	if (!Data.HasAnyContent())
	{
		Report.AddError(TEXT("Package contains no metro stations or lines."), TEXT("metro"), TEXT("VOID.Metro.NoContent"),
			TEXT("Load Meridian's MetroNetwork.json through FVoidMetroNetworkMapper, or add a 'metro' block to the package."));
		return Report;
	}

	// ---- Unique ids ----------------------------------------------------------
	TMap<FName, EVoidMetroNetworkKind> StationNetwork;
	TSet<FName> DeadStationDistricts;
	for (int32 i = 0; i < Data.Stations.Num(); ++i)
	{
		const FVoidMetroStationSpec& S = Data.Stations[i];
		const FString Path = FString::Printf(TEXT("metro.stations[%d]"), i);
		if (!S.Id.IsValid())
		{
			Report.AddError(FString::Printf(TEXT("%s has an empty id."), *Path), Path, TEXT("VOID.Metro.DuplicateId"), TEXT("Give every station a unique id."));
			continue;
		}
		if (StationNetwork.Contains(S.Id.Value))
		{
			Report.AddError(FString::Printf(TEXT("Duplicate station id '%s'."), *S.Id.Value.ToString()), Path, TEXT("VOID.Metro.DuplicateId"), TEXT("Station ids must be unique across BOTH networks."));
		}
		StationNetwork.Add(S.Id.Value, S.Network);
		if (S.Network == EVoidMetroNetworkKind::Dead) { DeadStationDistricts.Add(S.DistrictId); }

		if (S.bHasPosition && !IsFinite2D(S.Position))
		{
			Report.AddError(FString::Printf(TEXT("Station '%s' has a non-finite position."), *S.Id.Value.ToString()), Path + TEXT(".position"), TEXT("VOID.Metro.InvalidPosition"), TEXT("Use finite numbers in Unreal units (cm)."));
		}

		// Sector 0-style hard entrance limit.
		if (S.MaxEntrances > 0 && S.Entrances.Num() > S.MaxEntrances)
		{
			Report.AddError(FString::Printf(TEXT("Station '%s' declares %d entrances but is limited to %d (Meridian hard constraint)."), *S.Id.Value.ToString(), S.Entrances.Num(), S.MaxEntrances),
				Path + TEXT(".entrances"), TEXT("VOID.Metro.EntranceLimit"), TEXT("Remove the extra entrances; MetroNetwork.json forbids generating additional entrances for this station."));
		}
		for (int32 e = 0; e < S.Entrances.Num(); ++e)
		{
			if (S.Entrances[e].bHasPosition && !IsFinite2D(S.Entrances[e].Position))
			{
				Report.AddError(FString::Printf(TEXT("Entrance '%s' has a non-finite position."), *S.Entrances[e].Id.Value.ToString()), FString::Printf(TEXT("%s.entrances[%d]"), *Path, e),
					TEXT("VOID.Metro.InvalidPosition"), TEXT("Use finite numbers in Unreal units (cm)."));
			}
		}
	}

	TMap<FName, EVoidMetroNetworkKind> LineNetwork;
	for (int32 i = 0; i < Data.Lines.Num(); ++i)
	{
		const FVoidMetroLineSpec& L = Data.Lines[i];
		const FString Path = FString::Printf(TEXT("metro.lines[%d]"), i);
		if (!L.Id.IsValid())
		{
			Report.AddError(FString::Printf(TEXT("%s has an empty id."), *Path), Path, TEXT("VOID.Metro.DuplicateId"), TEXT("Give every line a unique id."));
			continue;
		}
		if (LineNetwork.Contains(L.Id.Value))
		{
			Report.AddError(FString::Printf(TEXT("Duplicate line id '%s'."), *L.Id.Value.ToString()), Path, TEXT("VOID.Metro.DuplicateId"), TEXT("Line ids must be unique across BOTH networks."));
		}
		LineNetwork.Add(L.Id.Value, L.Network);

		for (const FVector2D& Pt : L.CenterlinePoints)
		{
			if (!IsFinite2D(Pt))
			{
				Report.AddError(FString::Printf(TEXT("Line '%s' has a non-finite centerline point."), *L.Id.Value.ToString()), Path + TEXT(".centerlinePoints"), TEXT("VOID.Metro.InvalidPosition"), TEXT("Use finite numbers in Unreal units (cm)."));
				break;
			}
		}

		// Explicit membership must resolve and stay inside the line's network.
		for (const FVoidElementId& Sid : L.StationIds)
		{
			const EVoidMetroNetworkKind* SN = StationNetwork.Find(Sid.Value);
			if (!SN)
			{
				Report.AddError(FString::Printf(TEXT("Line '%s' references unknown station '%s'."), *L.Id.Value.ToString(), *Sid.Value.ToString()), Path + TEXT(".stationIds"),
					TEXT("VOID.Metro.UnknownStationRef"), TEXT("Fix the station id, or remove it from stationIds."));
			}
			else if (*SN != L.Network)
			{
				Report.AddError(FString::Printf(TEXT("Line '%s' (%s network) includes station '%s' from the other network."), *L.Id.Value.ToString(),
					L.Network == EVoidMetroNetworkKind::Live ? TEXT("Live") : TEXT("Dead"), *Sid.Value.ToString()), Path + TEXT(".stationIds"),
					TEXT("VOID.Metro.NetworkMerge"), TEXT("The Live and Dead networks are never merged (dual_network_never_merged)."));
			}
		}
	}

	// ---- Interchanges: Live only, lines must resolve and share a network ----------
	for (int32 i = 0; i < Data.Interchanges.Num(); ++i)
	{
		const FVoidMetroInterchangeSpec& X = Data.Interchanges[i];
		const FString Path = FString::Printf(TEXT("metro.interchanges[%d]"), i);
		bool bHaveLive = false, bHaveDead = false;
		for (const FVoidElementId& Lid : X.ConnectedLineIds)
		{
			const EVoidMetroNetworkKind* LN = LineNetwork.Find(Lid.Value);
			if (!LN)
			{
				Report.AddError(FString::Printf(TEXT("Interchange '%s' references unknown line '%s'."), *X.Id.Value.ToString(), *Lid.Value.ToString()), Path + TEXT(".connects_lines"),
					TEXT("VOID.Metro.UnknownLineRef"), TEXT("Fix the line id."));
				continue;
			}
			(*LN == EVoidMetroNetworkKind::Live ? bHaveLive : bHaveDead) = true;
		}
		if (bHaveDead)
		{
			Report.AddError(FString::Printf(TEXT("Interchange '%s' involves a Dead-network line. The Dead network has no formal interchanges."), *X.Id.Value.ToString()), Path,
				TEXT("VOID.Metro.DeadInterchange"), TEXT("Remove the Dead-network line from the interchange."));
		}
		if (bHaveLive && bHaveDead)
		{
			Report.AddError(FString::Printf(TEXT("Interchange '%s' would join the Live and Dead networks."), *X.Id.Value.ToString()), Path,
				TEXT("VOID.Metro.NetworkMerge"), TEXT("No traversal path may transition directly between Live and Dead networks."));
		}
	}

	// ---- Tunnel links: both ends must be dead-network districts -------------------------
	TSet<FName> TunnelIds;
	for (int32 i = 0; i < Data.TunnelLinks.Num(); ++i)
	{
		const FVoidMetroTunnelLink& T = Data.TunnelLinks[i];
		const FString Path = FString::Printf(TEXT("metro.tunnelLinks[%d]"), i);
		TunnelIds.Add(T.Id.Value);
		if (!DeadStationDistricts.Contains(T.DistrictA) || !DeadStationDistricts.Contains(T.DistrictB))
		{
			Report.AddError(FString::Printf(TEXT("Tunnel '%s' connects '%s' and '%s', but at least one of them has no Dead-network station."), *T.Id.Value.ToString(), *T.DistrictA.ToString(), *T.DistrictB.ToString()), Path,
				TEXT("VOID.Metro.UnresolvedTunnelLink"), TEXT("A dead-network tunnel may only join districts that have a Dead-network station."));
		}
	}
	for (const FVoidMetroStationSpec& S : Data.Stations)
	{
		if (S.SharesTunnelId != NAME_None && !TunnelIds.Contains(S.SharesTunnelId))
		{
			Report.AddWarning(FString::Printf(TEXT("Station '%s' shares tunnel '%s', which has no tunnel link (RoadNetwork.json tunnel_relationships not loaded?)."), *S.Id.Value.ToString(), *S.SharesTunnelId.ToString()),
				TEXT("metro.stations"), TEXT("VOID.Metro.TunnelIdUnresolved"), TEXT("Load RoadNetwork.json tunnel relationships through FVoidMetroNetworkMapper."));
		}
	}

	// ---- district_connectivity -------------------------------------------------------
	for (int32 i = 0; i < Data.DistrictLinks.Num(); ++i)
	{
		const FVoidMetroDistrictLink& D = Data.DistrictLinks[i];
		const FString Path = FString::Printf(TEXT("metro.districtLinks[%d]"), i);
		const EVoidMetroNetworkKind* SN = StationNetwork.Find(D.StationRef.Value);
		if (!SN)
		{
			Report.AddError(FString::Printf(TEXT("District '%s' references unknown station '%s'."), *D.DistrictId.ToString(), *D.StationRef.Value.ToString()), Path,
				TEXT("VOID.Metro.UnknownStationRef"), TEXT("Fix station_ref."));
		}
		else if (*SN != D.Network)
		{
			Report.AddError(FString::Printf(TEXT("District '%s' says network differs from station '%s' network."), *D.DistrictId.ToString(), *D.StationRef.Value.ToString()), Path,
				TEXT("VOID.Metro.NetworkMerge"), TEXT("A district's station must be on the network the district connects through."));
		}
	}

	return Report;
}

void FVoidMetroValidator::ValidateResolved(const FVoidMetroResolvedLayout& Layout, FVoidValidationReport& Report)
{
	// Live and Dead must never share infrastructure at any point. Compare stations in 3D
	// (a dead station directly below a live one at a different depth is fine).
	for (int32 i = 0; i < Layout.Stations.Num(); ++i)
	{
		for (int32 j = i + 1; j < Layout.Stations.Num(); ++j)
		{
			const FVoidMetroResolvedStation& A = Layout.Stations[i];
			const FVoidMetroResolvedStation& B = Layout.Stations[j];
			if (A.Network != B.Network && FVector::DistSquared(A.Location, B.Location) < 100.0f * 100.0f)
			{
				Report.AddError(FString::Printf(TEXT("Live station '%s' and Dead station '%s' resolve to the same place."), *(A.Network == EVoidMetroNetworkKind::Live ? A.Id : B.Id).ToString(),
					*(A.Network == EVoidMetroNetworkKind::Dead ? A.Id : B.Id).ToString()), TEXT("metro.stations"), TEXT("VOID.Metro.NetworkOverlap"),
					TEXT("The two networks do not share infrastructure at any point; move one station."));
			}
		}
	}
}
