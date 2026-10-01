// Copyright VOID Engineering Studio. Internal tool - not for shipping content.
// Added by Agent 2 (Phase 6 Metro Generator).

#include "VoidMetroNetworkMapper.h"
#include "VoidJsonReader.h"
#include "VoidWorldBuilderImportLog.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace VoidMetroMapperPrivate
{
	static bool TryParseNetworkKind(const FString& In, EVoidMetroNetworkKind& Out)
	{
		if (In.Equals(TEXT("live_network"), ESearchCase::IgnoreCase) || In.Equals(TEXT("live"), ESearchCase::IgnoreCase))
		{
			Out = EVoidMetroNetworkKind::Live;
			return true;
		}
		if (In.Equals(TEXT("dead_network"), ESearchCase::IgnoreCase) || In.Equals(TEXT("dead"), ESearchCase::IgnoreCase))
		{
			Out = EVoidMetroNetworkKind::Dead;
			return true;
		}
		return false;
	}

	static bool TryParseGrade(const FString& In, EVoidMetroGrade& Out)
	{
		if (In.Equals(TEXT("AtGrade"), ESearchCase::IgnoreCase) || In.Equals(TEXT("grade"), ESearchCase::IgnoreCase)) { Out = EVoidMetroGrade::AtGrade; return true; }
		if (In.Equals(TEXT("Elevated"), ESearchCase::IgnoreCase)) { Out = EVoidMetroGrade::Elevated; return true; }
		if (In.Equals(TEXT("Underground"), ESearchCase::IgnoreCase)) { Out = EVoidMetroGrade::Underground; return true; }
		return false;
	}

	static bool ReadVector2(const TSharedPtr<FJsonValue>& Value, FVector2D& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Value.IsValid() && Value->TryGetArray(Arr) && Arr->Num() >= 2)
		{
			Out = FVector2D((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber());
			return true;
		}
		return false;
	}

	static void ReadNameArray(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, TArray<FName>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Obj->TryGetArrayField(Field, Arr))
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				FString S;
				if (V.IsValid() && V->TryGetString(S) && !S.IsEmpty())
				{
					Out.Add(FName(*S));
				}
			}
		}
	}

	static void ReadIdArray(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, TArray<FVoidElementId>& Out)
	{
		TArray<FName> Names;
		ReadNameArray(Obj, Field, Names);
		for (const FName& N : Names)
		{
			Out.Add(FVoidElementId(N));
		}
	}

	static EVoidMetroPlatformCondition ConditionFromString(const FString& In)
	{
		return In.Contains(TEXT("abandon"), ESearchCase::IgnoreCase) || In.Contains(TEXT("deteriorat"), ESearchCase::IgnoreCase)
			? EVoidMetroPlatformCondition::Abandoned
			: EVoidMetroPlatformCondition::Maintained;
	}

	static void MapEntrances(const TSharedPtr<FJsonObject>& StationObj, const FString& Ctx, FVoidMetroStationSpec& Station, FVoidValidationReport& Report)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!StationObj->TryGetArrayField(TEXT("entrances"), Arr))
		{
			return;
		}
		for (int32 i = 0; i < Arr->Num(); ++i)
		{
			const TSharedPtr<FJsonObject>* E = nullptr;
			const FString EC = FString::Printf(TEXT("%s.entrances[%d]"), *Ctx, i);
			if (!(*Arr)[i]->TryGetObject(E))
			{
				Report.AddError(FString::Printf(TEXT("Non-object entry at %s."), *EC), EC, TEXT("VOID.Import.MalformedElement"), TEXT("Each entrance must be a JSON object."));
				continue;
			}
			FVoidMetroEntranceSpec Ent;
			FString IdStr;
			if ((*E)->TryGetStringField(TEXT("id"), IdStr) && !IdStr.IsEmpty())
			{
				Ent.Id = FVoidElementId(FName(*IdStr));
			}
			else
			{
				Report.AddError(FString::Printf(TEXT("%s is missing a non-empty 'id'."), *EC), EC + TEXT(".id"), TEXT("VOID.Import.MissingField"), TEXT("Add a non-empty 'id' string."));
			}
			const TArray<TSharedPtr<FJsonValue>>* PosArr = nullptr;
			if ((*E)->TryGetArrayField(TEXT("position"), PosArr) && PosArr->Num() >= 2)
			{
				Ent.bHasPosition = true;
				Ent.Position = FVector2D((*PosArr)[0]->AsNumber(), (*PosArr)[1]->AsNumber());
			}
			double Yaw = 0.0;
			if ((*E)->TryGetNumberField(TEXT("yawDegrees"), Yaw))
			{
				Ent.YawDegrees = static_cast<float>(Yaw);
			}
			Station.Entrances.Add(MoveTemp(Ent));
		}
	}
}

bool FVoidMetroNetworkMapper::LoadMeridianMetroNetworkFile(const FString& FilePath, FVoidMetroData& OutMetro, FVoidValidationReport& OutReport)
{
	TSharedPtr<FJsonObject> Root;
	FString Error;
	if (!FVoidJsonReader::ReadFromFile(FilePath, Root, Error))
	{
		OutReport.AddFatal(Error, TEXT("metro"), TEXT("VOID.Import.UnreadableFile"), TEXT("Check the MetroNetwork.json path and that it contains valid JSON."));
		UE_LOG(LogVoidImport, Error, TEXT("Failed to read metro file '%s': %s"), *FilePath, *Error);
		return false;
	}
	MapMeridianMetroNetwork(Root, OutMetro, OutReport);
	return true;
}

bool FVoidMetroNetworkMapper::LoadMeridianTunnelRelationshipsFile(const FString& RoadNetworkFilePath, FVoidMetroData& OutMetro, FVoidValidationReport& OutReport)
{
	TSharedPtr<FJsonObject> Root;
	FString Error;
	if (!FVoidJsonReader::ReadFromFile(RoadNetworkFilePath, Root, Error))
	{
		OutReport.AddFatal(Error, TEXT("roadNetwork"), TEXT("VOID.Import.UnreadableFile"), TEXT("Check the RoadNetwork.json path and that it contains valid JSON."));
		return false;
	}
	AppendMeridianTunnelRelationships(Root, OutMetro, OutReport);
	return true;
}

void FVoidMetroNetworkMapper::MapMeridianMetroNetwork(const TSharedPtr<FJsonObject>& Root, FVoidMetroData& OutMetro, FVoidValidationReport& OutReport)
{
	using namespace VoidMetroMapperPrivate;

	if (!Root.IsValid())
	{
		OutReport.AddFatal(TEXT("MetroNetwork root JSON object was invalid."), TEXT("metro"), TEXT("VOID.Import.MalformedJson"), TEXT("Check the JSON is well-formed."));
		return;
	}

	FString SchemaId;
	if (!Root->TryGetStringField(TEXT("$schema"), SchemaId) || SchemaId != TEXT("void_metro_network_schema_v1"))
	{
		OutReport.AddWarning(TEXT("MetroNetwork root has no '$schema' of 'void_metro_network_schema_v1'; mapping anyway."), TEXT("metro.$schema"), TEXT("VOID.Import.MetroSchemaMismatch"),
			TEXT("Confirm this is Meridian's MetroNetwork.json."));
	}

	const TArray<TSharedPtr<FJsonValue>>* Networks = nullptr;
	if (!Root->TryGetArrayField(TEXT("networks"), Networks))
	{
		OutReport.AddError(TEXT("MetroNetwork is missing a 'networks' array."), TEXT("metro.networks"), TEXT("VOID.Import.MissingField"), TEXT("Add the 'networks' array (live_network and dead_network)."));
		return;
	}

	for (int32 n = 0; n < Networks->Num(); ++n)
	{
		const FString NC = FString::Printf(TEXT("metro.networks[%d]"), n);
		const TSharedPtr<FJsonObject>* NetObj = nullptr;
		if (!(*Networks)[n]->TryGetObject(NetObj))
		{
			OutReport.AddError(FString::Printf(TEXT("Non-object entry at %s."), *NC), NC, TEXT("VOID.Import.MalformedElement"), TEXT("Each network must be a JSON object."));
			continue;
		}

		FString NetId;
		EVoidMetroNetworkKind Kind = EVoidMetroNetworkKind::Live;
		if (!(*NetObj)->TryGetStringField(TEXT("id"), NetId) || !TryParseNetworkKind(NetId, Kind))
		{
			OutReport.AddError(FString::Printf(TEXT("%s has missing/unrecognized network 'id' ('%s')."), *NC, *NetId), NC + TEXT(".id"), TEXT("VOID.Import.MetroUnknownNetwork"),
				TEXT("Use 'live_network' or 'dead_network'."));
			continue;
		}

		FVoidMetroNetworkSpec Net;
		Net.Id = FVoidElementId(FName(*NetId));
		Net.Kind = Kind;
		(*NetObj)->TryGetStringField(TEXT("display_name"), Net.DisplayName);

		const TSharedPtr<FJsonObject>* Platforms = nullptr;
		if ((*NetObj)->TryGetObjectField(TEXT("platforms"), Platforms))
		{
			FString Cond;
			(*Platforms)->TryGetStringField(TEXT("condition"), Cond);
			Net.PlatformCondition = ConditionFromString(Cond);
		}
		else
		{
			// Fall back on network identity only if the file omitted the field; Dead is the abandoned one.
			Net.PlatformCondition = (Kind == EVoidMetroNetworkKind::Dead) ? EVoidMetroPlatformCondition::Abandoned : EVoidMetroPlatformCondition::Maintained;
			OutReport.AddInfo(FString::Printf(TEXT("%s has no 'platforms'; platform condition inferred from network kind."), *NC), NC + TEXT(".platforms"), TEXT("VOID.Import.MissingField"));
		}

		const TSharedPtr<FJsonObject>* Maint = nullptr;
		if ((*NetObj)->TryGetObjectField(TEXT("maintenance_access"), Maint))
		{
			FString Model;
			(*Maint)->TryGetStringField(TEXT("model"), Model);
			Net.bHasMaintenanceAccess = !Model.IsEmpty() && !Model.StartsWith(TEXT("none"), ESearchCase::IgnoreCase);
		}
		OutMetro.Networks.Add(Net);

		// --- Lines ---
		const TArray<TSharedPtr<FJsonValue>>* Lines = nullptr;
		if ((*NetObj)->TryGetArrayField(TEXT("lines"), Lines))
		{
			for (int32 l = 0; l < Lines->Num(); ++l)
			{
				const FString LC = FString::Printf(TEXT("%s.lines[%d]"), *NC, l);
				const TSharedPtr<FJsonObject>* LineObj = nullptr;
				if (!(*Lines)[l]->TryGetObject(LineObj))
				{
					OutReport.AddError(FString::Printf(TEXT("Non-object entry at %s."), *LC), LC, TEXT("VOID.Import.MalformedElement"), TEXT("Each line must be a JSON object."));
					continue;
				}
				FVoidMetroLineSpec Line;
				Line.Network = Kind;
				FString IdStr;
				if ((*LineObj)->TryGetStringField(TEXT("id"), IdStr) && !IdStr.IsEmpty())
				{
					Line.Id = FVoidElementId(FName(*IdStr));
				}
				else
				{
					OutReport.AddError(FString::Printf(TEXT("%s is missing a non-empty 'id'."), *LC), LC + TEXT(".id"), TEXT("VOID.Import.MissingField"), TEXT("Add a non-empty 'id' string."));
					continue;
				}

				FString Cat, Route, SegModel;
				(*LineObj)->TryGetStringField(TEXT("maps_to_road_category"), Cat);
				(*LineObj)->TryGetStringField(TEXT("shares_route_id"), Route);
				(*LineObj)->TryGetStringField(TEXT("segment_model"), SegModel);
				Line.MapsToRoadCategory = FName(*Cat);
				Line.SharesRouteId = FName(*Route);
				ReadNameArray(*LineObj, TEXT("connects_bands"), Line.ConnectsBands);

				if (Kind == EVoidMetroNetworkKind::Dead || SegModel.Contains(TEXT("disconnected"), ESearchCase::IgnoreCase))
				{
					Line.Topology = EVoidMetroLineTopology::LegacySegments;
				}
				else if (Cat.Contains(TEXT("ring"), ESearchCase::IgnoreCase))
				{
					Line.Topology = EVoidMetroLineTopology::Ring;
					Line.bClosedLoop = true;
				}
				else if (Cat.Contains(TEXT("radial"), ESearchCase::IgnoreCase))
				{
					Line.Topology = EVoidMetroLineTopology::Radial;
				}
				else
				{
					OutReport.AddWarning(FString::Printf(TEXT("Line '%s' has no recognizable topology (maps_to_road_category='%s'); it will be skipped unless authored geometry is supplied."), *IdStr, *Cat),
						LC, TEXT("VOID.Import.MetroUnknownTopology"), TEXT("Supply an authored 'centerlinePoints' for this line."));
				}
				OutMetro.Lines.Add(MoveTemp(Line));
			}
		}

		// --- Stations ---
		const TArray<TSharedPtr<FJsonValue>>* Stations = nullptr;
		if ((*NetObj)->TryGetArrayField(TEXT("stations"), Stations))
		{
			for (int32 s = 0; s < Stations->Num(); ++s)
			{
				const FString SC = FString::Printf(TEXT("%s.stations[%d]"), *NC, s);
				const TSharedPtr<FJsonObject>* StObj = nullptr;
				if (!(*Stations)[s]->TryGetObject(StObj))
				{
					OutReport.AddError(FString::Printf(TEXT("Non-object entry at %s."), *SC), SC, TEXT("VOID.Import.MalformedElement"), TEXT("Each station must be a JSON object."));
					continue;
				}
				FVoidMetroStationSpec St;
				St.Network = Kind;
				St.PlatformCondition = Net.PlatformCondition;
				St.bHasMaintenanceFacility = Net.bHasMaintenanceAccess;

				FString IdStr, District, Sub, Tier, Type, Tunnel;
				if ((*StObj)->TryGetStringField(TEXT("id"), IdStr) && !IdStr.IsEmpty())
				{
					St.Id = FVoidElementId(FName(*IdStr));
				}
				else
				{
					OutReport.AddError(FString::Printf(TEXT("%s is missing a non-empty 'id'."), *SC), SC + TEXT(".id"), TEXT("VOID.Import.MissingField"), TEXT("Add a non-empty 'id' string."));
					continue;
				}
				if ((*StObj)->TryGetStringField(TEXT("district"), District) && !District.IsEmpty())
				{
					St.DistrictId = FName(*District);
				}
				else
				{
					OutReport.AddError(FString::Printf(TEXT("%s is missing a non-empty 'district'."), *SC), SC + TEXT(".district"), TEXT("VOID.Import.MissingField"), TEXT("Add the Meridian district id."));
				}
				if ((*StObj)->TryGetStringField(TEXT("sub_location"), Sub)) { St.SubLocationId = FName(*Sub); }
				(*StObj)->TryGetStringField(TEXT("tier"), Tier);
				(*StObj)->TryGetStringField(TEXT("type"), Type);
				St.SourceTier = Tier;
				St.SourceType = Type;
				if ((*StObj)->TryGetStringField(TEXT("shares_tunnel_id"), Tunnel)) { St.SharesTunnelId = FName(*Tunnel); }

				St.bIsTerminus = Tier.Contains(TEXT("terminus"), ESearchCase::IgnoreCase) || Type.Contains(TEXT("terminus"), ESearchCase::IgnoreCase);

				// Sector 0: "access_point_count": 1 and a "no_additional_entrances_may_be_generated" hard constraint.
				double AccessPointCount = 0.0;
				if ((*StObj)->TryGetNumberField(TEXT("access_point_count"), AccessPointCount) && AccessPointCount >= 1.0)
				{
					St.MaxEntrances = FMath::RoundToInt(AccessPointCount);
				}
				FString Hard;
				if ((*StObj)->TryGetStringField(TEXT("hard_constraint"), Hard) && Hard.Contains(TEXT("no_additional_entrances"), ESearchCase::IgnoreCase))
				{
					St.MaxEntrances = 1;
				}

				OutMetro.Stations.Add(MoveTemp(St));
			}
		}

		// --- Interchanges (Live network only in this data; the Dead network declares none) ---
		const TArray<TSharedPtr<FJsonValue>>* Ich = nullptr;
		if ((*NetObj)->TryGetArrayField(TEXT("interchanges"), Ich))
		{
			for (int32 i = 0; i < Ich->Num(); ++i)
			{
				const FString IC = FString::Printf(TEXT("%s.interchanges[%d]"), *NC, i);
				const TSharedPtr<FJsonObject>* IObj = nullptr;
				if (!(*Ich)[i]->TryGetObject(IObj))
				{
					OutReport.AddError(FString::Printf(TEXT("Non-object entry at %s."), *IC), IC, TEXT("VOID.Import.MalformedElement"), TEXT("Each interchange must be a JSON object."));
					continue;
				}
				FVoidMetroInterchangeSpec Spec;
				FString IdStr;
				if ((*IObj)->TryGetStringField(TEXT("id"), IdStr) && !IdStr.IsEmpty())
				{
					Spec.Id = FVoidElementId(FName(*IdStr));
				}
				else
				{
					OutReport.AddError(FString::Printf(TEXT("%s is missing a non-empty 'id'."), *IC), IC + TEXT(".id"), TEXT("VOID.Import.MissingField"), TEXT("Add a non-empty 'id' string."));
					continue;
				}
				ReadIdArray(*IObj, TEXT("connects_lines"), Spec.ConnectedLineIds);
				(*IObj)->TryGetStringField(TEXT("location_rule"), Spec.LocationRule);
				OutMetro.Interchanges.Add(MoveTemp(Spec));
			}
		}
	}

	// --- district_connectivity ---
	const TArray<TSharedPtr<FJsonValue>>* DC = nullptr;
	if (Root->TryGetArrayField(TEXT("district_connectivity"), DC))
	{
		for (int32 d = 0; d < DC->Num(); ++d)
		{
			const FString DCx = FString::Printf(TEXT("metro.district_connectivity[%d]"), d);
			const TSharedPtr<FJsonObject>* DObj = nullptr;
			if (!(*DC)[d]->TryGetObject(DObj))
			{
				OutReport.AddError(FString::Printf(TEXT("Non-object entry at %s."), *DCx), DCx, TEXT("VOID.Import.MalformedElement"), TEXT("Each entry must be a JSON object."));
				continue;
			}
			FVoidMetroDistrictLink Link;
			FString District, Net, Ref;
			(*DObj)->TryGetStringField(TEXT("district"), District);
			(*DObj)->TryGetStringField(TEXT("network"), Net);
			(*DObj)->TryGetStringField(TEXT("station_ref"), Ref);
			if (District.IsEmpty() || Ref.IsEmpty() || !TryParseNetworkKind(Net, Link.Network))
			{
				OutReport.AddError(FString::Printf(TEXT("%s needs 'district', 'network' (live_network/dead_network) and 'station_ref'."), *DCx), DCx, TEXT("VOID.Import.MissingField"),
					TEXT("Fill in district, network and station_ref."));
				continue;
			}
			Link.DistrictId = FName(*District);
			Link.StationRef = FVoidElementId(FName(*Ref));
			OutMetro.DistrictLinks.Add(MoveTemp(Link));
		}
	}

	UE_LOG(LogVoidImport, Log, TEXT("Mapped Meridian metro: %d networks, %d lines, %d stations, %d interchanges, %d district links."),
		OutMetro.Networks.Num(), OutMetro.Lines.Num(), OutMetro.Stations.Num(), OutMetro.Interchanges.Num(), OutMetro.DistrictLinks.Num());
}

void FVoidMetroNetworkMapper::AppendMeridianTunnelRelationships(const TSharedPtr<FJsonObject>& RoadNetworkRoot, FVoidMetroData& OutMetro, FVoidValidationReport& OutReport)
{
	if (!RoadNetworkRoot.IsValid())
	{
		OutReport.AddFatal(TEXT("RoadNetwork root JSON object was invalid."), TEXT("roadNetwork"), TEXT("VOID.Import.MalformedJson"), TEXT("Check the JSON is well-formed."));
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* Tunnels = nullptr;
	if (!RoadNetworkRoot->TryGetArrayField(TEXT("tunnel_relationships"), Tunnels))
	{
		OutReport.AddInfo(TEXT("RoadNetwork has no 'tunnel_relationships'; no dead-network tunnel links were added."), TEXT("roadNetwork.tunnel_relationships"), TEXT("VOID.Import.MissingField"));
		return;
	}

	for (int32 t = 0; t < Tunnels->Num(); ++t)
	{
		const FString TC = FString::Printf(TEXT("roadNetwork.tunnel_relationships[%d]"), t);
		const TSharedPtr<FJsonObject>* TObj = nullptr;
		if (!(*Tunnels)[t]->TryGetObject(TObj))
		{
			OutReport.AddError(FString::Printf(TEXT("Non-object entry at %s."), *TC), TC, TEXT("VOID.Import.MalformedElement"), TEXT("Each tunnel must be a JSON object."));
			continue;
		}

		FString Type, IdStr;
		(*TObj)->TryGetStringField(TEXT("type"), Type);
		(*TObj)->TryGetStringField(TEXT("id"), IdStr);
		if (IdStr.IsEmpty())
		{
			OutReport.AddError(FString::Printf(TEXT("%s is missing a non-empty 'id'."), *TC), TC + TEXT(".id"), TEXT("VOID.Import.MissingField"), TEXT("Add a non-empty 'id' string."));
			continue;
		}
		// Only dead-network tunnels are metro-relevant ("shared_dead_network_tunnel", "dead_network_terminus_with_narrative_gate").
		if (!Type.Contains(TEXT("dead_network"), ESearchCase::IgnoreCase))
		{
			continue;
		}

		TArray<FName> Connects;
		VoidMetroMapperPrivate::ReadNameArray(*TObj, TEXT("connects"), Connects);
		if (Connects.Num() != 2)
		{
			OutReport.AddError(FString::Printf(TEXT("Tunnel '%s' must connect exactly two districts (found %d)."), *IdStr, Connects.Num()), TC + TEXT(".connects"),
				TEXT("VOID.Import.MetroTunnelEndpoints"), TEXT("List exactly two district ids in 'connects'."));
			continue;
		}

		FVoidMetroTunnelLink Link;
		Link.Id = FVoidElementId(FName(*IdStr));
		Link.DistrictA = Connects[0];
		Link.DistrictB = Connects[1];
		FString Dir;
		Link.bDirectional = (*TObj)->TryGetStringField(TEXT("directionality"), Dir) && Dir.Contains(TEXT("directional"), ESearchCase::IgnoreCase);
		OutMetro.TunnelLinks.Add(MoveTemp(Link));
	}
}

void FVoidMetroNetworkMapper::MapPackageMetroBlock(const TSharedPtr<FJsonObject>& MetroObject, FVoidMetroData& OutMetro, FVoidValidationReport& OutReport)
{
	using namespace VoidMetroMapperPrivate;

	if (!MetroObject.IsValid())
	{
		return;
	}

	// --- stations ---
	const TArray<TSharedPtr<FJsonValue>>* Stations = nullptr;
	if (MetroObject->TryGetArrayField(TEXT("stations"), Stations))
	{
		for (int32 s = 0; s < Stations->Num(); ++s)
		{
			const FString SC = FString::Printf(TEXT("metro.stations[%d]"), s);
			const TSharedPtr<FJsonObject>* Obj = nullptr;
			if (!(*Stations)[s]->TryGetObject(Obj))
			{
				OutReport.AddError(FString::Printf(TEXT("Non-object entry at %s."), *SC), SC, TEXT("VOID.Import.MalformedElement"), TEXT("Each station must be a JSON object."));
				continue;
			}
			FVoidMetroStationSpec St;
			FString IdStr, NetStr, District, Grade, Cond;
			if ((*Obj)->TryGetStringField(TEXT("id"), IdStr) && !IdStr.IsEmpty())
			{
				St.Id = FVoidElementId(FName(*IdStr));
			}
			else
			{
				OutReport.AddError(FString::Printf(TEXT("%s is missing a non-empty 'id'."), *SC), SC + TEXT(".id"), TEXT("VOID.Import.MissingField"), TEXT("Add a non-empty 'id' string."));
				continue;
			}
			if (!(*Obj)->TryGetStringField(TEXT("networkId"), NetStr) || !TryParseNetworkKind(NetStr, St.Network))
			{
				OutReport.AddError(FString::Printf(TEXT("%s needs 'networkId' of live_network/dead_network."), *SC), SC + TEXT(".networkId"), TEXT("VOID.Import.MetroUnknownNetwork"), TEXT("Set networkId."));
				continue;
			}
			if ((*Obj)->TryGetStringField(TEXT("districtId"), District)) { St.DistrictId = FName(*District); }
			St.PlatformCondition = (St.Network == EVoidMetroNetworkKind::Dead) ? EVoidMetroPlatformCondition::Abandoned : EVoidMetroPlatformCondition::Maintained;
			if ((*Obj)->TryGetStringField(TEXT("platformCondition"), Cond)) { St.PlatformCondition = ConditionFromString(Cond); }
			St.bHasMaintenanceFacility = (St.Network == EVoidMetroNetworkKind::Live);
			(*Obj)->TryGetBoolField(TEXT("isTerminus"), St.bIsTerminus);
			double MaxE = 0.0;
			if ((*Obj)->TryGetNumberField(TEXT("maxEntrances"), MaxE)) { St.MaxEntrances = FMath::Max(0, FMath::RoundToInt(MaxE)); }
			FString Tunnel;
			if ((*Obj)->TryGetStringField(TEXT("sharesTunnelId"), Tunnel)) { St.SharesTunnelId = FName(*Tunnel); }

			const TArray<TSharedPtr<FJsonValue>>* PosArr = nullptr;
			if ((*Obj)->TryGetArrayField(TEXT("position"), PosArr) && PosArr->Num() >= 2)
			{
				St.bHasPosition = true;
				St.Position = FVector2D((*PosArr)[0]->AsNumber(), (*PosArr)[1]->AsNumber());
			}
			if ((*Obj)->TryGetStringField(TEXT("grade"), Grade))
			{
				if (TryParseGrade(Grade, St.Grade)) { St.bHasGradeOverride = true; }
				else
				{
					OutReport.AddWarning(FString::Printf(TEXT("%s has unrecognized grade '%s'; ignored."), *SC, *Grade), SC + TEXT(".grade"), TEXT("VOID.Import.MetroUnknownGrade"), TEXT("Use AtGrade, Elevated or Underground."));
				}
			}
			MapEntrances(*Obj, SC, St, OutReport);
			OutMetro.Stations.Add(MoveTemp(St));
		}
	}

	// --- lines ---
	const TArray<TSharedPtr<FJsonValue>>* Lines = nullptr;
	if (MetroObject->TryGetArrayField(TEXT("lines"), Lines))
	{
		for (int32 l = 0; l < Lines->Num(); ++l)
		{
			const FString LC = FString::Printf(TEXT("metro.lines[%d]"), l);
			const TSharedPtr<FJsonObject>* Obj = nullptr;
			if (!(*Lines)[l]->TryGetObject(Obj))
			{
				OutReport.AddError(FString::Printf(TEXT("Non-object entry at %s."), *LC), LC, TEXT("VOID.Import.MalformedElement"), TEXT("Each line must be a JSON object."));
				continue;
			}
			FVoidMetroLineSpec Ln;
			FString IdStr, NetStr, Grade;
			if ((*Obj)->TryGetStringField(TEXT("id"), IdStr) && !IdStr.IsEmpty())
			{
				Ln.Id = FVoidElementId(FName(*IdStr));
			}
			else
			{
				OutReport.AddError(FString::Printf(TEXT("%s is missing a non-empty 'id'."), *LC), LC + TEXT(".id"), TEXT("VOID.Import.MissingField"), TEXT("Add a non-empty 'id' string."));
				continue;
			}
			if (!(*Obj)->TryGetStringField(TEXT("networkId"), NetStr) || !TryParseNetworkKind(NetStr, Ln.Network))
			{
				OutReport.AddError(FString::Printf(TEXT("%s needs 'networkId' of live_network/dead_network."), *LC), LC + TEXT(".networkId"), TEXT("VOID.Import.MetroUnknownNetwork"), TEXT("Set networkId."));
				continue;
			}
			ReadIdArray(*Obj, TEXT("stationIds"), Ln.StationIds);
			(*Obj)->TryGetBoolField(TEXT("closedLoop"), Ln.bClosedLoop);

			const TArray<TSharedPtr<FJsonValue>>* Pts = nullptr;
			if ((*Obj)->TryGetArrayField(TEXT("centerlinePoints"), Pts))
			{
				for (const TSharedPtr<FJsonValue>& P : *Pts)
				{
					FVector2D V;
					if (ReadVector2(P, V)) { Ln.CenterlinePoints.Add(V); }
				}
				if (Ln.CenterlinePoints.Num() >= 2) { Ln.Topology = EVoidMetroLineTopology::Authored; }
				else
				{
					OutReport.AddError(FString::Printf(TEXT("%s has fewer than 2 valid centerlinePoints."), *LC), LC + TEXT(".centerlinePoints"), TEXT("VOID.Import.InvalidGeometry"), TEXT("Provide at least 2 [x, y] points."));
				}
			}
			if ((*Obj)->TryGetStringField(TEXT("grade"), Grade))
			{
				if (TryParseGrade(Grade, Ln.Grade)) { Ln.bHasGradeOverride = true; }
				else
				{
					OutReport.AddWarning(FString::Printf(TEXT("%s has unrecognized grade '%s'; ignored."), *LC, *Grade), LC + TEXT(".grade"), TEXT("VOID.Import.MetroUnknownGrade"), TEXT("Use AtGrade, Elevated or Underground."));
				}
			}
			OutMetro.Lines.Add(MoveTemp(Ln));
		}
	}

	// --- tunnelLinks ---
	const TArray<TSharedPtr<FJsonValue>>* Links = nullptr;
	if (MetroObject->TryGetArrayField(TEXT("tunnelLinks"), Links))
	{
		for (int32 t = 0; t < Links->Num(); ++t)
		{
			const FString TC = FString::Printf(TEXT("metro.tunnelLinks[%d]"), t);
			const TSharedPtr<FJsonObject>* Obj = nullptr;
			if (!(*Links)[t]->TryGetObject(Obj))
			{
				OutReport.AddError(FString::Printf(TEXT("Non-object entry at %s."), *TC), TC, TEXT("VOID.Import.MalformedElement"), TEXT("Each tunnel link must be a JSON object."));
				continue;
			}
			FString IdStr, A, B;
			(*Obj)->TryGetStringField(TEXT("id"), IdStr);
			(*Obj)->TryGetStringField(TEXT("districtA"), A);
			(*Obj)->TryGetStringField(TEXT("districtB"), B);
			if (IdStr.IsEmpty() || A.IsEmpty() || B.IsEmpty())
			{
				OutReport.AddError(FString::Printf(TEXT("%s needs 'id', 'districtA' and 'districtB'."), *TC), TC, TEXT("VOID.Import.MissingField"), TEXT("Fill in id, districtA and districtB."));
				continue;
			}
			FVoidMetroTunnelLink Link;
			Link.Id = FVoidElementId(FName(*IdStr));
			Link.DistrictA = FName(*A);
			Link.DistrictB = FName(*B);
			(*Obj)->TryGetBoolField(TEXT("directional"), Link.bDirectional);
			OutMetro.TunnelLinks.Add(MoveTemp(Link));
		}
	}
}

void FVoidMetroNetworkMapper::MergeAuthoredOverrides(FVoidMetroData& Base, const FVoidMetroData& Authored, FVoidValidationReport& OutReport)
{
	TMap<FName, int32> StationIndex;
	for (int32 i = 0; i < Base.Stations.Num(); ++i) { StationIndex.Add(Base.Stations[i].Id.Value, i); }
	TMap<FName, int32> LineIndex;
	for (int32 i = 0; i < Base.Lines.Num(); ++i) { LineIndex.Add(Base.Lines[i].Id.Value, i); }

	for (const FVoidMetroStationSpec& A : Authored.Stations)
	{
		if (const int32* Idx = StationIndex.Find(A.Id.Value))
		{
			FVoidMetroStationSpec& B = Base.Stations[*Idx];
			if (B.Network != A.Network)
			{
				OutReport.AddError(FString::Printf(TEXT("Authored station '%s' is on a different network than the Meridian station with the same id."), *A.Id.Value.ToString()),
					TEXT("metro.stations"), TEXT("VOID.Metro.NetworkMerge"), TEXT("Live and Dead networks are never merged; fix the networkId."));
				continue;
			}
			if (A.bHasPosition) { B.bHasPosition = true; B.Position = A.Position; }
			if (A.bHasGradeOverride) { B.bHasGradeOverride = true; B.Grade = A.Grade; }
			if (A.Entrances.Num() > 0) { B.Entrances = A.Entrances; }
			if (A.MaxEntrances > 0) { B.MaxEntrances = (B.MaxEntrances > 0) ? FMath::Min(B.MaxEntrances, A.MaxEntrances) : A.MaxEntrances; } // never loosen a hard limit
		}
		else
		{
			StationIndex.Add(A.Id.Value, Base.Stations.Num());
			Base.Stations.Add(A);
		}
	}

	for (const FVoidMetroLineSpec& A : Authored.Lines)
	{
		if (const int32* Idx = LineIndex.Find(A.Id.Value))
		{
			FVoidMetroLineSpec& B = Base.Lines[*Idx];
			if (B.Network != A.Network)
			{
				OutReport.AddError(FString::Printf(TEXT("Authored line '%s' is on a different network than the Meridian line with the same id."), *A.Id.Value.ToString()),
					TEXT("metro.lines"), TEXT("VOID.Metro.NetworkMerge"), TEXT("Live and Dead networks are never merged; fix the networkId."));
				continue;
			}
			if (A.CenterlinePoints.Num() >= 2) { B.CenterlinePoints = A.CenterlinePoints; B.Topology = EVoidMetroLineTopology::Authored; B.bClosedLoop = A.bClosedLoop; }
			if (A.StationIds.Num() > 0) { B.StationIds = A.StationIds; }
			if (A.bHasGradeOverride) { B.bHasGradeOverride = true; B.Grade = A.Grade; }
		}
		else
		{
			LineIndex.Add(A.Id.Value, Base.Lines.Num());
			Base.Lines.Add(A);
		}
	}

	TSet<FName> KnownLinks;
	for (const FVoidMetroTunnelLink& L : Base.TunnelLinks) { KnownLinks.Add(L.Id.Value); }
	for (const FVoidMetroTunnelLink& L : Authored.TunnelLinks)
	{
		if (!KnownLinks.Contains(L.Id.Value)) { Base.TunnelLinks.Add(L); }
	}
}
