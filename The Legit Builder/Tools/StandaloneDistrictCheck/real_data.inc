#include "Meridian/VoidMeridianRegistry.h"
static void FillRealMeridian(FVoidMeridianDataSet& D){
{FVoidMeridianDistrictEntry E; E.Id=FName("olympus_spire");E.DisplayName="Olympus Spire";E.RadialBand="core";E.VerticalTier="absolute_top";E.StructuralModel="singular_civic_centerpiece";E.GenerationPriority=1;E.BuildOrderIndex=5;E.DistrictDataFile="Olympus_Spire_data.json";
E.WorldPartitionRegion="WP_OlympusSpire";E.WorldPartitionTemplate="";E.Instancing="single_instance";
D.Districts.Add(E);}
{FVoidMeridianDistrictEntry E; E.Id=FName("white_zones");E.DisplayName="White Zones";E.RadialBand="mid_tier_rings";E.VerticalTier="grade_street_level";E.StructuralModel="typology_plus_flagship_instance";E.GenerationPriority=3;E.BuildOrderIndex=2;E.DistrictDataFile="White_Zones_data.json";
E.DependsOn.Add(FName("olympus_spire"));
E.WorldPartitionRegion="WP_WhiteZones_Flagship";E.WorldPartitionTemplate="WP_WhiteZones_NetworkNode_Template";E.Instancing="single_flagship_plus_reusable_template";
D.Districts.Add(E);}
{FVoidMeridianDistrictEntry E; E.Id=FName("metro_archives");E.DisplayName="Metro Archives";E.RadialBand="seam_zone";E.VerticalTier="grade_plus_below_grade_aging";E.StructuralModel="singular_bespoke_facility";E.GenerationPriority=5;E.BuildOrderIndex=3;E.DistrictDataFile="Metro_Archives_data.json";
E.DependsOn.Add(FName("white_zones"));
E.DependsOn.Add(FName("undercroft"));
E.WorldPartitionRegion="WP_MetroArchives";E.WorldPartitionTemplate="";E.Instancing="single_instance";
E.bHasDistrictData=true;E.bCommercialPresence=false;E.bCorporatePresence=false;
D.Districts.Add(E);}
{FVoidMeridianDistrictEntry E; E.Id=FName("undercroft");E.DisplayName="The Undercroft";E.RadialBand="inner_mid_outer_continuous";E.VerticalTier="below_grade_living";E.StructuralModel="internal_three_belt_gradient";E.GenerationPriority=4;E.BuildOrderIndex=1;E.DistrictDataFile="VOID_Location_01_Undercroft_data.json";
E.DependsOn.Add(FName("olympus_spire"));
E.WorldPartitionRegion="WP_Undercroft";E.WorldPartitionTemplate="";E.Instancing="single_instance_three_internal_cells";
D.Districts.Add(E);}
{FVoidMeridianDistrictEntry E; E.Id=FName("sector_0");E.DisplayName="Sector 0";E.RadialBand="off_gradient";E.VerticalTier="absolute_bottom_dead";E.StructuralModel="singular_narrative_hub";E.GenerationPriority=6;E.BuildOrderIndex=4;E.DistrictDataFile="Sector_0_data.json";
E.DependsOn.Add(FName("undercroft"));
E.WorldPartitionRegion="WP_Sector0";E.WorldPartitionTemplate="";E.Instancing="single_instance";
D.Districts.Add(E);}
D.AdjacencyEdges.Add({FName("undercroft"),FName("metro_archives"),"shared_transit_seam_boundary"});
D.AdjacencyEdges.Add({FName("undercroft"),FName("sector_0"),"shared_entrance_tunnel_threshold"});
D.UnreachablePairs.Add(TPair<FName,FName>(FName("white_zones"),FName("undercroft")));
D.UnreachablePairs.Add(TPair<FName,FName>(FName("white_zones"),FName("metro_archives")));
D.UnreachablePairs.Add(TPair<FName,FName>(FName("white_zones"),FName("sector_0")));
D.UnreachablePairs.Add(TPair<FName,FName>(FName("olympus_spire"),FName("undercroft")));
D.UnreachablePairs.Add(TPair<FName,FName>(FName("olympus_spire"),FName("metro_archives")));
D.UnreachablePairs.Add(TPair<FName,FName>(FName("olympus_spire"),FName("sector_0")));
D.MacroGenerationOrder.Add(FName("olympus_spire"));
D.MacroGenerationOrder.Add(FName("live_network_spine"));
D.MacroGenerationOrder.Add(FName("white_zones_network_nodes"));
D.MacroGenerationOrder.Add(FName("undercroft_substrate"));
D.MacroGenerationOrder.Add(FName("metro_archives"));
D.MacroGenerationOrder.Add(FName("sector_0"));
{FVoidMeridianRoute R;R.Id=FName("spire_radial_spine");R.Category="radial_arterial";R.Network="live_network";R.HierarchyTier=1;
R.ServesDistricts.Add(FName("olympus_spire"));
R.ServesDistricts.Add(FName("white_zones"));
D.Routes.Add(R);}
{FVoidMeridianRoute R;R.Id=FName("mid_tier_ring_road");R.Category="ring_road";R.Network="live_network";R.HierarchyTier=2;
R.ServesDistricts.Add(FName("white_zones"));
D.Routes.Add(R);}
{FVoidMeridianRoute R;R.Id=FName("undercroft_maintenance_layer");R.Category="service_maintenance_route";R.Network="dead_network";R.HierarchyTier=3;
R.SubBeltsServed.Add(FName("spire_adjacent_foundation"));
R.SubBeltsServed.Add(FName("mid_ring_foundation"));
R.SubBeltsServed.Add(FName("outer_ring_foundation"));
R.OwningDistrict=FName("undercroft");
D.Routes.Add(R);}
{FVoidMeridianRoute R;R.Id=FName("pre_council_subway_service_layer");R.Category="service_maintenance_route";R.Network="dead_network";R.HierarchyTier=3;
R.SharedByDistricts.Add(FName("undercroft"));
R.SharedByDistricts.Add(FName("metro_archives"));
R.SharedByDistricts.Add(FName("sector_0"));
D.Routes.Add(R);}
{FVoidMeridianLandmarkEntry L;L.Id=FName("olympus_spire_silhouette");L.DistrictId=FName("olympus_spire");L.Type="mega_landmark_whole_district";L.InstancingPolicy="";
L.VisibilityTier=1;
L.SightlineVisibleFrom.Add(FName("undercroft_rare_upward_shafts"));
L.SightlineVisibleFrom.Add(FName("white_zones_open_plazas"));
L.SightlineVisibleFrom.Add(FName("metro_archives_public_floor_windows"));
L.SightlineExcluded.Add(FName("sector_0_enclosed_by_design"));
D.Landmarks.Add(L);}
{FVoidMeridianLandmarkEntry L;L.Id=FName("continuity_monument");L.DistrictId=FName("olympus_spire");L.Type="public_sculpture";L.InstancingPolicy="";
L.VisibilityTier=2;
D.Landmarks.Add(L);}
{FVoidMeridianLandmarkEntry L;L.Id=FName("elite_residential_towers");L.DistrictId=FName("olympus_spire");L.Type="secondary_tower_cluster";L.InstancingPolicy="";
L.VisibilityTier=2;
D.Landmarks.Add(L);}
{FVoidMeridianLandmarkEntry L;L.Id=FName("atrium_of_perfect_memory");L.DistrictId=FName("olympus_spire");L.Type="landmark_interior_boss_arena";L.InstancingPolicy="";
L.VisibilityTierLabel="interior_not_applicable_to_skyline";
D.Landmarks.Add(L);}
{FVoidMeridianLandmarkEntry L;L.Id=FName("olympus_spire_server_hub");L.DistrictId=FName("olympus_spire");L.Type="landmark_interior_flagged";L.InstancingPolicy="";
L.VisibilityTierLabel="interior_not_applicable_to_skyline";
D.Landmarks.Add(L);}
{FVoidMeridianLandmarkEntry L;L.Id=FName("white_zones_weave_display_column");L.DistrictId=FName("white_zones");L.Type="standardized_branded_civic_fixture";L.InstancingPolicy="generated_per_network_node_identical_citywide";
L.VisibilityTier=4;
D.Landmarks.Add(L);}
{FVoidMeridianLandmarkEntry L;L.Id=FName("metro_archives_building");L.DistrictId=FName("metro_archives");L.Type="bespoke_singular_building";L.InstancingPolicy="";
L.VisibilityTier=3;
D.Landmarks.Add(L);}
{FVoidMeridianLandmarkEntry L;L.Id=FName("metro_archives_reading_room");L.DistrictId=FName("metro_archives");L.Type="landmark_interior";L.InstancingPolicy="";
L.VisibilityTierLabel="interior_not_applicable_to_skyline";
D.Landmarks.Add(L);}
{FVoidMeridianLandmarkEntry L;L.Id=FName("metro_archives_sealed_exhibit_case");L.DistrictId=FName("metro_archives");L.Type="landmark_interior_narrative_critical";L.InstancingPolicy="";
L.VisibilityTierLabel="interior_not_applicable_to_skyline";
D.Landmarks.Add(L);}
{FVoidMeridianLandmarkEntry L;L.Id=FName("sector_0_entrance_threshold");L.DistrictId=FName("sector_0");L.Type="landmark_narrative_transition";L.InstancingPolicy="";
L.VisibilityTierLabel="not_applicable_below_grade_enclosed";
D.Landmarks.Add(L);}
{FVoidMeridianLandmarkEntry L;L.Id=FName("sector_0_echo_relay_station");L.DistrictId=FName("sector_0");L.Type="landmark_narrative_critical";L.InstancingPolicy="";
L.VisibilityTierLabel="not_applicable_below_grade_enclosed";
D.Landmarks.Add(L);}
{FVoidMeridianLandmarkEntry L;L.Id=FName("undercroft_riggs_safehouse");L.DistrictId=FName("undercroft");L.Type="landmark_npc_hub_reclassified";L.InstancingPolicy="";
L.VisibilityTierLabel="not_applicable_below_grade";
D.Landmarks.Add(L);}
{FVoidMeridianLandmarkEntry L;L.Id=FName("undercroft_yaras_stall");L.DistrictId=FName("undercroft");L.Type="landmark_npc_hub_reclassified";L.InstancingPolicy="";
L.VisibilityTierLabel="not_applicable_below_grade";
D.Landmarks.Add(L);}
{FVoidMeridianLandmarkEntry L;L.Id=FName("undercroft_coens_atrium");L.DistrictId=FName("undercroft");L.Type="landmark_npc_hub_reclassified";L.InstancingPolicy="";
L.VisibilityTierLabel="not_applicable_below_grade";
D.Landmarks.Add(L);}
}