// Copyright VOID Engineering Studio. Internal tool - not for shipping content.
// Added by Agent 2 (Phase 6 Metro Generator + selected Phase 9 World Partition support).
//
// These are UE Automation tests. They were WRITTEN against the source but NOT RUN by the author
// (no Unreal Engine in the authoring environment). Run in-editor: Session Frontend > Automation >
// filter "VOID.WorldBuilder.Metro".

#include "Misc/AutomationTest.h"
#include "VoidMetroNetworkMapper.h"
#include "VoidJsonReader.h"
#include "Metro/VoidMetroLayoutResolver.h"
#include "Metro/VoidMetroValidator.h"
#include "Metro/VoidMetroGenerationSettings.h"
#include "WorldPartition/VoidWorldPartitionHelper.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace VoidMetroTestFixtures
{
	// Single quotes stand in for double quotes so the JSON stays readable. Content mirrors the real
	// Meridian MetroNetwork.json / RoadNetwork.json (ids, districts, tiers, hard constraints).
	static FString Q(const TCHAR* In) { return FString(In).Replace(TEXT("'"), TEXT("\"")); }

	static FString MeridianMetroJson()
	{
		return Q(TEXT("{'$schema':'void_metro_network_schema_v1','networks':["
			"{'id':'live_network','display_name':'Grav-Rail',"
			"'platforms':{'condition':'clean_maintained_standard_civic_infrastructure'},"
			"'maintenance_access':{'model':'centrally_scheduled_council_civic_services'},"
			"'lines':[{'id':'radial_line','maps_to_road_category':'radial_arterial','shares_route_id':'spire_radial_spine','connects_bands':['core','inner_rings_unbuilt','mid_tier_rings']},"
			"{'id':'ring_line','maps_to_road_category':'ring_road','shares_route_id':'mid_tier_ring_road','connects_bands':['mid_tier_rings']}],"
			"'stations':[{'id':'spire_base_plaza_station','district':'olympus_spire','sub_location':'base_plaza_monument','tier':'terminus_anchor'},"
			"{'id':'white_zones_node_station','district':'white_zones','instancing_policy':'generated_per_network_node_not_individually_enumerated'}],"
			"'interchanges':[{'id':'radial_ring_interchange','connects_lines':['radial_line','ring_line'],'location_rule':'wherever_a_ring_road_crosses_the_radial_arterial_spine'}]},"
			"{'id':'dead_network','display_name':'Pre-Council Subway',"
			"'platforms':{'condition':'abandoned_deteriorated'},"
			"'maintenance_access':{'model':'none_abandoned_infrastructure'},"
			"'lines':[{'id':'legacy_subway_line','status':'deprecated_unmapped_no_current_coordinates','segment_model':'multiple_disconnected_segments'}],"
			"'stations':[{'id':'undercroft_access_layer','district':'undercroft','type':'informal_maintenance_access'},"
			"{'id':'metro_archives_sub_basement_seam','district':'metro_archives','type':'seam_tap_point','shares_tunnel_id':'undercroft_metro_archives_transit_seam'},"
			"{'id':'sector_0_entrance_threshold','district':'sector_0','type':'terminus_single_access_point','shares_tunnel_id':'undercroft_sector_0_entrance_threshold','access_point_count':1,'hard_constraint':'no_additional_entrances_may_be_generated_per_sector_0_sec_11'}],"
			"'interchanges':[]}],"
			"'district_connectivity':["
			"{'district':'olympus_spire','network':'live_network','station_ref':'spire_base_plaza_station'},"
			"{'district':'white_zones','network':'live_network','station_ref':'white_zones_node_station'},"
			"{'district':'undercroft','network':'dead_network','station_ref':'undercroft_access_layer'},"
			"{'district':'metro_archives','network':'dead_network','station_ref':'metro_archives_sub_basement_seam'},"
			"{'district':'sector_0','network':'dead_network','station_ref':'sector_0_entrance_threshold'}]}"));
	}

	static FString MeridianRoadTunnelsJson()
	{
		return Q(TEXT("{'tunnel_relationships':["
			"{'id':'undercroft_metro_archives_transit_seam','type':'shared_dead_network_tunnel','connects':['undercroft','metro_archives']},"
			"{'id':'undercroft_sector_0_entrance_threshold','type':'dead_network_terminus_with_narrative_gate','connects':['undercroft','sector_0'],'directionality':'hard_one_directional_narrative_gate'},"
			"{'id':'some_live_tunnel_that_is_not_metro','type':'live_road_tunnel','connects':['a','b']}]}"));
	}

	static bool LoadMeridian(FVoidMetroData& Out, FVoidValidationReport& Report)
	{
		TSharedPtr<FJsonObject> Root, Road;
		FString Err;
		if (!FVoidJsonReader::ParseString(MeridianMetroJson(), Root, Err)) { return false; }
		if (!FVoidJsonReader::ParseString(MeridianRoadTunnelsJson(), Road, Err)) { return false; }
		FVoidMetroNetworkMapper::MapMeridianMetroNetwork(Root, Out, Report);
		FVoidMetroNetworkMapper::AppendMeridianTunnelRelationships(Road, Out, Report);
		return true;
	}

	static const FVoidMetroResolvedSegment* FindSeg(const FVoidMetroResolvedLayout& L, const TCHAR* Id)
	{
		return L.Segments.FindByPredicate([Id](const FVoidMetroResolvedSegment& S) { return S.Id == FName(Id); });
	}
}

// ---------------------------------------------------------------------------------------------
// Mapper
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidMetroMapperMeridianTest, "VOID.WorldBuilder.Metro.Mapper.MapsMeridianTopology", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidMetroMapperMeridianTest::RunTest(const FString& Parameters)
{
	FVoidMetroData Metro;
	FVoidValidationReport Report;
	Report.bIsValid = true;
	TestTrue(TEXT("fixture parses"), VoidMetroTestFixtures::LoadMeridian(Metro, Report));

	TestEqual(TEXT("2 networks"), Metro.Networks.Num(), 2);
	TestEqual(TEXT("3 lines (radial, ring, legacy)"), Metro.Lines.Num(), 3);
	TestEqual(TEXT("5 stations"), Metro.Stations.Num(), 5);
	TestEqual(TEXT("1 interchange"), Metro.Interchanges.Num(), 1);
	TestEqual(TEXT("5 district links"), Metro.DistrictLinks.Num(), 5);
	TestEqual(TEXT("only the 2 dead-network tunnels are kept"), Metro.TunnelLinks.Num(), 2);

	const FVoidMetroStationSpec* S0 = Metro.Stations.FindByPredicate([](const FVoidMetroStationSpec& S) { return S.Id.Value == FName("sector_0_entrance_threshold"); });
	if (TestNotNull(TEXT("Sector 0 station exists"), S0))
	{
		TestEqual(TEXT("Sector 0 is limited to exactly 1 entrance"), S0->MaxEntrances, 1);
		TestTrue(TEXT("Sector 0 is a terminus"), S0->bIsTerminus);
		TestEqual(TEXT("dead platforms are abandoned"), S0->PlatformCondition, EVoidMetroPlatformCondition::Abandoned);
		TestFalse(TEXT("dead network has no maintenance facility"), S0->bHasMaintenanceFacility);
		TestFalse(TEXT("no coordinates are invented"), S0->bHasPosition);
	}
	const FVoidMetroStationSpec* WZ = Metro.Stations.FindByPredicate([](const FVoidMetroStationSpec& S) { return S.Id.Value == FName("white_zones_node_station"); });
	if (TestNotNull(TEXT("White Zones station exists"), WZ))
	{
		TestTrue(TEXT("live network has a maintenance facility"), WZ->bHasMaintenanceFacility);
		TestEqual(TEXT("live platforms are maintained"), WZ->PlatformCondition, EVoidMetroPlatformCondition::Maintained);
	}

	const FVoidMetroLineSpec* Ring = Metro.Lines.FindByPredicate([](const FVoidMetroLineSpec& L) { return L.Id.Value == FName("ring_line"); });
	if (TestNotNull(TEXT("ring line exists"), Ring))
	{
		TestEqual(TEXT("ring topology"), Ring->Topology, EVoidMetroLineTopology::Ring);
		TestTrue(TEXT("ring is closed"), Ring->bClosedLoop);
	}
	const FVoidMetroLineSpec* Radial = Metro.Lines.FindByPredicate([](const FVoidMetroLineSpec& L) { return L.Id.Value == FName("radial_line"); });
	if (TestNotNull(TEXT("radial line exists"), Radial))
	{
		TestEqual(TEXT("radial topology"), Radial->Topology, EVoidMetroLineTopology::Radial);
	}
	const FVoidMetroTunnelLink* Gate = Metro.TunnelLinks.FindByPredicate([](const FVoidMetroTunnelLink& T) { return T.Id.Value == FName("undercroft_sector_0_entrance_threshold"); });
	if (TestNotNull(TEXT("Sector 0 tunnel link exists"), Gate))
	{
		TestTrue(TEXT("Sector 0 gate is directional"), Gate->bDirectional);
	}
	TestEqual(TEXT("mapping raises no errors"), Report.NumErrors(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidMetroMapperAuthoredMergeTest, "VOID.WorldBuilder.Metro.Mapper.AuthoredOverridesWin", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidMetroMapperAuthoredMergeTest::RunTest(const FString& Parameters)
{
	FVoidMetroData Metro;
	FVoidValidationReport Report;
	Report.bIsValid = true;
	VoidMetroTestFixtures::LoadMeridian(Metro, Report);

	TSharedPtr<FJsonObject> Block;
	FString Err;
	const FString Json = VoidMetroTestFixtures::Q(TEXT("{'stations':[{'id':'white_zones_node_station','networkId':'live_network','position':[12345,-6789],'grade':'Elevated'}],"
		"'lines':[{'id':'ring_line','networkId':'live_network','closedLoop':true,'centerlinePoints':[[0,0],[1000,0],[1000,1000],[0,1000]]}]}"));
	TestTrue(TEXT("authored block parses"), FVoidJsonReader::ParseString(Json, Block, Err));

	FVoidMetroData Authored;
	FVoidMetroNetworkMapper::MapPackageMetroBlock(Block, Authored, Report);
	FVoidMetroNetworkMapper::MergeAuthoredOverrides(Metro, Authored, Report);

	const FVoidMetroStationSpec* WZ = Metro.Stations.FindByPredicate([](const FVoidMetroStationSpec& S) { return S.Id.Value == FName("white_zones_node_station"); });
	if (TestNotNull(TEXT("station kept"), WZ))
	{
		TestTrue(TEXT("authored position applied"), WZ->bHasPosition);
		TestEqual(TEXT("authored X"), WZ->Position.X, 12345.0);
		TestTrue(TEXT("authored grade applied"), WZ->bHasGradeOverride && WZ->Grade == EVoidMetroGrade::Elevated);
		TestEqual(TEXT("Meridian district retained"), WZ->DistrictId, FName("white_zones"));
	}
	TestEqual(TEXT("merge did not duplicate stations"), Metro.Stations.Num(), 5);

	// Authored geometry must resolve WITHOUT placeholder flags.
	FVoidMetroLayoutParams P = FVoidMetroLayoutParams::MakeBuiltInDefaults();
	FVoidValidationReport R2;
	R2.bIsValid = true;
	const FVoidMetroResolvedLayout Layout = FVoidMetroLayoutResolver::Resolve(Metro, P, R2);
	const FVoidMetroResolvedSegment* Ring = VoidMetroTestFixtures::FindSeg(Layout, TEXT("ring_line"));
	if (TestNotNull(TEXT("authored ring resolved under its own id"), Ring))
	{
		TestFalse(TEXT("authored ring is not a placeholder"), Ring->bPlaceholder);
	}
	const FVoidMetroResolvedStation* St = Layout.FindStation(FName("white_zones_node_station"));
	if (TestNotNull(TEXT("authored station resolved"), St))
	{
		TestFalse(TEXT("authored station is not a placeholder"), St->bPlaceholderPosition);
		TestEqual(TEXT("authored elevated grade => elevated Z"), St->Location.Z, static_cast<double>(P.ElevatedHeightUnits));
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Resolver
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidMetroResolverPlaceholderTest, "VOID.WorldBuilder.Metro.Resolver.PlaceholderLayoutIsFlaggedAndStructured", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidMetroResolverPlaceholderTest::RunTest(const FString& Parameters)
{
	FVoidMetroData Metro;
	FVoidValidationReport Load;
	Load.bIsValid = true;
	VoidMetroTestFixtures::LoadMeridian(Metro, Load);

	const FVoidMetroLayoutParams P = FVoidMetroLayoutParams::MakeBuiltInDefaults();
	FVoidValidationReport Report;
	Report.bIsValid = true;
	const FVoidMetroResolvedLayout L = FVoidMetroLayoutResolver::Resolve(Metro, P, Report);

	TestTrue(TEXT("placeholder use is recorded"), L.bUsedPlaceholder);
	TestTrue(TEXT("placeholder warning is raised"), Report.Issues.ContainsByPredicate([](const FVoidValidationIssue& I) { return I.ErrorCode == FName("VOID.Metro.PlaceholderLayout"); }));
	TestEqual(TEXT("all 5 stations placed"), L.Stations.Num(), 5);

	const FVoidMetroResolvedStation* Spire = L.FindStation(FName("spire_base_plaza_station"));
	if (TestNotNull(TEXT("spire station"), Spire))
	{
		TestTrue(TEXT("Spire station is at the world origin (canon-explicit origin)"), FVector::Dist2D(Spire->Location, FVector::ZeroVector) < 1.0f);
		TestEqual(TEXT("live default grade is at-grade"), Spire->Grade, EVoidMetroGrade::AtGrade);
		TestTrue(TEXT("live station gets a service placeholder"), Spire->bHasServiceFacility);
	}

	// Radial spoke + closed ring.
	const FVoidMetroResolvedSegment* Spoke = VoidMetroTestFixtures::FindSeg(L, TEXT("radial_line__to__white_zones_node_station"));
	const FVoidMetroResolvedSegment* Ring = VoidMetroTestFixtures::FindSeg(L, TEXT("ring_line__ring"));
	TestNotNull(TEXT("radial spoke to White Zones"), Spoke);
	if (TestNotNull(TEXT("ring segment"), Ring))
	{
		TestTrue(TEXT("ring is closed"), Ring->bClosed);
		const FVoidMetroResolvedStation* WZ = L.FindStation(FName("white_zones_node_station"));
		if (WZ)
		{
			float Yaw = 0.0f, Dist = 0.0f;
			TestTrue(TEXT("ring finds nearest span"), FVoidMetroLayoutResolver::NearestPathYawDegrees(Ring->Points, true, WZ->Location, Yaw, Dist));
			TestTrue(TEXT("White Zones station lies on the ring polyline"), Dist < 1.0f);
		}
	}

	// Station at the radial/ring crossing becomes the interchange instead of a duplicate hub.
	const FVoidMetroResolvedStation* WZ = L.FindStation(FName("white_zones_node_station"));
	if (TestNotNull(TEXT("White Zones station"), WZ))
	{
		TestTrue(TEXT("radial/ring crossing at the station marks it as an interchange"), WZ->bIsInterchange);
	}
	TestEqual(TEXT("no duplicate stand-alone hub at that crossing"), L.InterchangeHubs.Num(), 0);

	// Dead network: two tunnel segments, underground, separate from live.
	const FVoidMetroResolvedSegment* Seam = VoidMetroTestFixtures::FindSeg(L, TEXT("undercroft_metro_archives_transit_seam"));
	const FVoidMetroResolvedSegment* Gate = VoidMetroTestFixtures::FindSeg(L, TEXT("undercroft_sector_0_entrance_threshold"));
	if (TestNotNull(TEXT("seam tunnel segment"), Seam) && TestNotNull(TEXT("sector 0 tunnel segment"), Gate))
	{
		TestEqual(TEXT("dead segment network"), Seam->Network, EVoidMetroNetworkKind::Dead);
		for (const FVector& Pt : Seam->Points) { if (!FMath::IsNearlyEqual(Pt.Z, -P.UndergroundDepthUnits, 1.0f)) { AddError(TEXT("dead tunnel point not at underground depth")); break; } }
		TestEqual(TEXT("dead tunnels are not shared with live segments"), L.Segments.FilterByPredicate([](const FVoidMetroResolvedSegment& S) { return S.Network == EVoidMetroNetworkKind::Dead; }).Num(), 2);
	}
	TestEqual(TEXT("no portals: dead stations and track are all underground"), L.Portals.Num(), 0);
	TestEqual(TEXT("no elevated spans by default"), L.ElevatedSpans.Num(), 0);

	// Sector 0: exactly one entrance even though placeholders default to 1 and could be raised.
	const FVoidMetroResolvedStation* S0 = L.FindStation(FName("sector_0_entrance_threshold"));
	if (TestNotNull(TEXT("Sector 0 station"), S0))
	{
		TestEqual(TEXT("Sector 0 has exactly one entrance"), S0->Entrances.Num(), 1);
		TestFalse(TEXT("dead station has no service facility"), S0->bHasServiceFacility);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidMetroResolverSector0LimitTest, "VOID.WorldBuilder.Metro.Resolver.Sector0EntranceLimitClampsPlaceholders", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidMetroResolverSector0LimitTest::RunTest(const FString& Parameters)
{
	FVoidMetroData Metro;
	FVoidValidationReport Load;
	Load.bIsValid = true;
	VoidMetroTestFixtures::LoadMeridian(Metro, Load);

	FVoidMetroLayoutParams P = FVoidMetroLayoutParams::MakeBuiltInDefaults();
	P.DefaultEntrancesPerStation = 4;
	FVoidValidationReport Report;
	Report.bIsValid = true;
	const FVoidMetroResolvedLayout L = FVoidMetroLayoutResolver::Resolve(Metro, P, Report);

	const FVoidMetroResolvedStation* S0 = L.FindStation(FName("sector_0_entrance_threshold"));
	const FVoidMetroResolvedStation* WZ = L.FindStation(FName("white_zones_node_station"));
	if (TestNotNull(TEXT("Sector 0 station"), S0)) { TestEqual(TEXT("still exactly 1 entrance"), S0->Entrances.Num(), 1); }
	if (TestNotNull(TEXT("WZ station"), WZ)) { TestEqual(TEXT("unrestricted station honours the default of 4"), WZ->Entrances.Num(), 4); }
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidMetroResolverNoPlaceholderTest, "VOID.WorldBuilder.Metro.Resolver.NoPlaceholderMeansNothingIsInvented", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidMetroResolverNoPlaceholderTest::RunTest(const FString& Parameters)
{
	FVoidMetroData Metro;
	FVoidValidationReport Load;
	Load.bIsValid = true;
	VoidMetroTestFixtures::LoadMeridian(Metro, Load);

	FVoidMetroLayoutParams P = FVoidMetroLayoutParams::MakeBuiltInDefaults();
	P.bAllowPlaceholderLayout = false;
	FVoidValidationReport Report;
	Report.bIsValid = true;
	const FVoidMetroResolvedLayout L = FVoidMetroLayoutResolver::Resolve(Metro, P, Report);

	TestTrue(TEXT("no coordinates in the data => empty layout"), L.IsEmpty());
	TestFalse(TEXT("no placeholder claimed"), L.bUsedPlaceholder);
	TestTrue(TEXT("skips are reported"), Report.NumWarnings() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidMetroResolverDeterminismTest, "VOID.WorldBuilder.Metro.Resolver.IsDeterministic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidMetroResolverDeterminismTest::RunTest(const FString& Parameters)
{
	FVoidMetroData Metro;
	FVoidValidationReport Load;
	Load.bIsValid = true;
	VoidMetroTestFixtures::LoadMeridian(Metro, Load);
	const FVoidMetroLayoutParams P = FVoidMetroLayoutParams::MakeBuiltInDefaults();
	FVoidValidationReport R1, R2;
	R1.bIsValid = R2.bIsValid = true;
	const FVoidMetroResolvedLayout A = FVoidMetroLayoutResolver::Resolve(Metro, P, R1);
	const FVoidMetroResolvedLayout B = FVoidMetroLayoutResolver::Resolve(Metro, P, R2);

	TestEqual(TEXT("same station count"), A.Stations.Num(), B.Stations.Num());
	TestEqual(TEXT("same segment count"), A.Segments.Num(), B.Segments.Num());
	for (int32 i = 0; i < FMath::Min(A.Segments.Num(), B.Segments.Num()); ++i)
	{
		TestEqual(TEXT("same segment id order"), A.Segments[i].Id, B.Segments[i].Id);
		TestEqual(TEXT("same point count"), A.Segments[i].Points.Num(), B.Segments[i].Points.Num());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidMetroGeometryHelpersTest, "VOID.WorldBuilder.Metro.Resolver.GeometryHelpers", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidMetroGeometryHelpersTest::RunTest(const FString& Parameters)
{
	// Densify
	{
		const TArray<FVector> Path = { FVector(0, 0, 0), FVector(1000, 0, 0) };
		const TArray<FVector> D = FVoidMetroLayoutResolver::Densify(Path, 250.0f, false);
		TestEqual(TEXT("1000 / 250 => 5 points (4 spans)"), D.Num(), 5);
		TestTrue(TEXT("endpoints preserved"), D[0].Equals(Path[0]) && D.Last().Equals(Path[1]));
	}
	// Grade ramp + portal crossing: tunnel body at -1000 rising to a ground-level station at each end.
	{
		TArray<FVector> Path = FVoidMetroLayoutResolver::Densify({ FVector(0, 0, 0), FVector(20000, 0, 0) }, 500.0f, false);
		FVoidMetroLayoutResolver::ApplyGradeRamps(Path, -1000.0f, 0.0f, 0.0f, 3000.0f);
		TestTrue(TEXT("starts at ground"), FMath::IsNearlyEqual(Path[0].Z, 0.0f, 1.0f));
		TestTrue(TEXT("ends at ground"), FMath::IsNearlyEqual(Path.Last().Z, 0.0f, 1.0f));
		TestTrue(TEXT("body is at depth"), FMath::IsNearlyEqual(Path[Path.Num() / 2].Z, -1000.0f, 1.0f));

		TArray<FVector> Portals;
		TArray<float> Yaws;
		FVoidMetroLayoutResolver::FindPortalCrossings(Path, Portals, Yaws);
		TestEqual(TEXT("one portal per end of the tunnel"), Portals.Num(), 2);
		if (Portals.Num() == 2) { TestTrue(TEXT("portals sit at ground level"), FMath::IsNearlyZero(Portals[0].Z) && FMath::IsNearlyZero(Portals[1].Z)); }
	}
	// Elevated run
	{
		const TArray<FVector> Path = { FVector(0, 0, 0), FVector(1, 0, 500), FVector(2, 0, 500), FVector(3, 0, 0) };
		TArray<TPair<FVector, FVector>> Runs;
		FVoidMetroLayoutResolver::FindElevatedRuns(Path, 200.0f, Runs);
		TestEqual(TEXT("one elevated run"), Runs.Num(), 1);
	}
	// Contacts: a spoke that ends on a ring, and a clean crossing.
	{
		const TArray<FVector> Line = { FVector(-500, 0, 0), FVector(500, 0, 0) };
		const TArray<FVector> Cross = { FVector(0, -500, 0), FVector(0, 500, 0) };
		TArray<FVector> Out;
		FVoidMetroLayoutResolver::FindPolylineContacts2D(Line, false, Cross, false, 50.0f, Out);
		TestEqual(TEXT("perpendicular lines cross once"), Out.Num(), 1);

		const TArray<FVector> Far = { FVector(0, 5000, 0), FVector(0, 6000, 0) };
		Out.Reset();
		FVoidMetroLayoutResolver::FindPolylineContacts2D(Line, false, Far, false, 50.0f, Out);
		TestEqual(TEXT("distant lines do not touch"), Out.Num(), 0);
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// Validator
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidMetroValidatorMeridianCleanTest, "VOID.WorldBuilder.Metro.Validator.RealMeridianDataIsValid", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidMetroValidatorMeridianCleanTest::RunTest(const FString& Parameters)
{
	FVoidMetroData Metro;
	FVoidValidationReport Load;
	Load.bIsValid = true;
	VoidMetroTestFixtures::LoadMeridian(Metro, Load);
	const FVoidValidationReport R = FVoidMetroValidator::Validate(Metro);
	TestTrue(TEXT("Meridian's own data passes validation"), R.bIsValid);
	TestEqual(TEXT("no errors"), R.NumErrors(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidMetroValidatorRulesTest, "VOID.WorldBuilder.Metro.Validator.EnforcesMeridianConstraints", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidMetroValidatorRulesTest::RunTest(const FString& Parameters)
{
	auto HasCode = [](const FVoidValidationReport& R, const TCHAR* Code) { return R.Issues.ContainsByPredicate([Code](const FVoidValidationIssue& I) { return I.ErrorCode == FName(Code); }); };

	// Empty
	{
		const FVoidValidationReport R = FVoidMetroValidator::Validate(FVoidMetroData());
		TestFalse(TEXT("empty metro data is rejected"), R.bIsValid);
		TestTrue(TEXT("NoContent code"), HasCode(R, TEXT("VOID.Metro.NoContent")));
	}

	FVoidMetroData Base;
	FVoidValidationReport Load;
	Load.bIsValid = true;
	VoidMetroTestFixtures::LoadMeridian(Base, Load);

	// Sector 0 second entrance
	{
		FVoidMetroData M = Base;
		for (FVoidMetroStationSpec& S : M.Stations)
		{
			if (S.Id.Value == FName("sector_0_entrance_threshold"))
			{
				FVoidMetroEntranceSpec E1, E2;
				E1.Id = FVoidElementId(FName("e1"));
				E2.Id = FVoidElementId(FName("e2"));
				S.Entrances = { E1, E2 };
			}
		}
		const FVoidValidationReport R = FVoidMetroValidator::Validate(M);
		TestFalse(TEXT("2 entrances at Sector 0 is an error"), R.bIsValid);
		TestTrue(TEXT("EntranceLimit code"), HasCode(R, TEXT("VOID.Metro.EntranceLimit")));
	}
	// Live line listing a dead station
	{
		FVoidMetroData M = Base;
		for (FVoidMetroLineSpec& L : M.Lines)
		{
			if (L.Id.Value == FName("radial_line")) { L.StationIds.Add(FVoidElementId(FName("sector_0_entrance_threshold"))); }
		}
		const FVoidValidationReport R = FVoidMetroValidator::Validate(M);
		TestFalse(TEXT("cross-network station membership is an error"), R.bIsValid);
		TestTrue(TEXT("NetworkMerge code"), HasCode(R, TEXT("VOID.Metro.NetworkMerge")));
	}
	// Interchange with the dead line
	{
		FVoidMetroData M = Base;
		M.Interchanges[0].ConnectedLineIds.Add(FVoidElementId(FName("legacy_subway_line")));
		const FVoidValidationReport R = FVoidMetroValidator::Validate(M);
		TestFalse(TEXT("dead-network interchange is an error"), R.bIsValid);
		TestTrue(TEXT("DeadInterchange code"), HasCode(R, TEXT("VOID.Metro.DeadInterchange")));
		TestTrue(TEXT("NetworkMerge code"), HasCode(R, TEXT("VOID.Metro.NetworkMerge")));
	}
	// Unresolvable district link
	{
		FVoidMetroData M = Base;
		M.DistrictLinks[0].StationRef = FVoidElementId(FName("no_such_station"));
		const FVoidValidationReport R = FVoidMetroValidator::Validate(M);
		TestFalse(TEXT("dangling station_ref is an error"), R.bIsValid);
		TestTrue(TEXT("UnknownStationRef code"), HasCode(R, TEXT("VOID.Metro.UnknownStationRef")));
	}
	// Duplicate id across networks
	{
		FVoidMetroData M = Base;
		FVoidMetroStationSpec Dup = M.Stations[0];
		Dup.Network = EVoidMetroNetworkKind::Dead;
		M.Stations.Add(Dup);
		const FVoidValidationReport R = FVoidMetroValidator::Validate(M);
		TestFalse(TEXT("duplicate station id is an error"), R.bIsValid);
		TestTrue(TEXT("DuplicateId code"), HasCode(R, TEXT("VOID.Metro.DuplicateId")));
	}
	// Overlapping live/dead stations after resolve
	{
		FVoidMetroResolvedLayout L;
		FVoidMetroResolvedStation A, B;
		A.Id = FName("a"); A.Network = EVoidMetroNetworkKind::Live; A.Location = FVector(0, 0, 0);
		B.Id = FName("b"); B.Network = EVoidMetroNetworkKind::Dead; B.Location = FVector(10, 0, 0);
		L.Stations = { A, B };
		FVoidValidationReport R;
		R.bIsValid = true;
		FVoidMetroValidator::ValidateResolved(L, R);
		TestFalse(TEXT("coincident live and dead stations are an error"), R.bIsValid);
		TestTrue(TEXT("NetworkOverlap code"), HasCode(R, TEXT("VOID.Metro.NetworkOverlap")));
	}
	return true;
}

// ---------------------------------------------------------------------------------------------
// World Partition helper + settings
// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidWorldPartitionHelperTest, "VOID.WorldBuilder.WorldPartition.Helper.CellsAndChunks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidWorldPartitionHelperTest::RunTest(const FString& Parameters)
{
	// Floor semantics for negative coordinates.
	TestEqual(TEXT("(-1,-1) is in cell (-1,-1)"), FVoidWorldPartitionHelper::GetCellForLocation(FVector(-1, -1, 0), 1000.0f), FIntPoint(-1, -1));
	TestEqual(TEXT("(999,1000) is in cell (0,1)"), FVoidWorldPartitionHelper::GetCellForLocation(FVector(999, 1000, 0), 1000.0f), FIntPoint(0, 1));

	// A 5 km straight path with 1 km cells => 5 chunks, contiguous (shared boundary vertex), path order.
	{
		const TArray<FVector> Path = { FVector(0, 500, 0), FVector(5000, 500, 0) };
		const TArray<FVoidPathChunk> Chunks = FVoidWorldPartitionHelper::SplitPathByCells(Path, false, 1000.0f);
		TestEqual(TEXT("5 chunks"), Chunks.Num(), 5);
		for (int32 i = 0; i + 1 < Chunks.Num(); ++i)
		{
			TestTrue(TEXT("consecutive chunks share a boundary vertex (no gaps)"), Chunks[i].Points.Last().Equals(Chunks[i + 1].Points[0], 0.01f));
			TestEqual(TEXT("chunk cells advance along X"), Chunks[i].Cell.X, i);
		}
		TestTrue(TEXT("first point preserved"), Chunks[0].Points[0].Equals(Path[0]));
		TestTrue(TEXT("last point preserved"), Chunks.Last().Points.Last().Equals(Path[1]));
	}
	// Re-entering a cell yields a new chunk with a higher IndexInCell (distinct, stable ids).
	{
		const TArray<FVector> Path = { FVector(500, 500, 0), FVector(2500, 500, 0), FVector(2500, 1500, 0), FVector(500, 1500, 0), FVector(500, 500, 0) };
		const TArray<FVoidPathChunk> Chunks = FVoidWorldPartitionHelper::SplitPathByCells(Path, false, 1000.0f);
		TSet<FString> Keys;
		for (const FVoidPathChunk& C : Chunks) { Keys.Add(FString::Printf(TEXT("%d_%d_%d"), C.Cell.X, C.Cell.Y, C.IndexInCell)); }
		TestEqual(TEXT("chunk keys are unique"), Keys.Num(), Chunks.Num());
	}
	TestEqual(TEXT("folder path"), FVoidWorldPartitionHelper::MakeFolderPath(TEXT("VOID/Meridian/Metro/"), TEXT("Live"), TEXT("Track")), FString(TEXT("VOID/Meridian/Metro/Live/Track")));
	TestFalse(TEXT("null world is not partitioned"), FVoidWorldPartitionHelper::IsWorldPartitioned(nullptr));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoidMetroSettingsMatchDefaultsTest, "VOID.WorldBuilder.Metro.Settings.DefaultsMatchResolverDefaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FVoidMetroSettingsMatchDefaultsTest::RunTest(const FString& Parameters)
{
	const UVoidMetroGenerationSettings* S = GetDefault<UVoidMetroGenerationSettings>();
	const FVoidMetroLayoutParams Built = FVoidMetroLayoutParams::MakeBuiltInDefaults();
	TestEqual(TEXT("band table size"), S->BandRadiusUnits.Num(), Built.BandRadiusUnits.Num());
	TestEqual(TEXT("district band table size"), S->DistrictBand.Num(), Built.DistrictBand.Num());
	TestEqual(TEXT("ring sample default"), S->RingSampleCount, Built.RingSampleCount);
	const float* Core = S->BandRadiusUnits.Find(FName("core"));
	if (TestNotNull(TEXT("core band present"), Core)) { TestEqual(TEXT("core radius is the world origin"), *Core, 0.0f); }
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
