// Copyright VOID Engineering Studio. Internal tool - not for shipping content.
// Added by Agent 2 (Phase 6 Metro Generator).

#pragma once

#include "CoreMinimal.h"
#include "Data/VoidMetroData.h"
#include "Data/VoidValidationReport.h"

class FJsonObject;

/**
 * FVoidMetroNetworkMapper
 *
 * The metro extension of the EXISTING import layer -- not a second importer.
 * It reuses FVoidJsonReader for file/text parsing and reports through the
 * same FVoidValidationReport. It maps already-parsed FJsonObject trees into
 * the normalized FVoidMetroData that generators consume.
 *
 * Three inputs are understood:
 *
 *  1. Meridian MetroNetwork.json (schema void_metro_network_schema_v1):
 *     topology only (networks/lines/stations/interchanges/district_connectivity).
 *  2. Meridian RoadNetwork.json's "tunnel_relationships": supplies the
 *     endpoints (districts) of dead-network tunnels referenced by
 *     MetroNetwork.json's shares_tunnel_id. Only "dead_network" tunnels are read.
 *  3. An optional AUTHORED "metro" block (embedded in a DesignPackage.json, or
 *     supplied separately) that carries real coordinates/grades. Authored data
 *     overrides Meridian topology by id via MergeAuthoredOverrides.
 *
 * Meridian's own package supplies no coordinates by policy
 * (Meridian_Master.json: coordinate_policy = no_fabricated_coordinates...), so
 * nothing here invents any: absent positions stay absent (bHasPosition=false).
 */
class VOIDWORLDBUILDERIMPORT_API FVoidMetroNetworkMapper
{
public:
	/** Reads MetroNetwork.json from disk via FVoidJsonReader and maps it. Returns false only if the file could not be read/parsed (reported as Fatal). */
	static bool LoadMeridianMetroNetworkFile(const FString& FilePath, FVoidMetroData& OutMetro, FVoidValidationReport& OutReport);

	/** Reads RoadNetwork.json and appends its dead-network tunnel_relationships to OutMetro.TunnelLinks. */
	static bool LoadMeridianTunnelRelationshipsFile(const FString& RoadNetworkFilePath, FVoidMetroData& OutMetro, FVoidValidationReport& OutReport);

	/** Maps a parsed MetroNetwork.json root object. Appends to OutMetro (does not clear it). */
	static void MapMeridianMetroNetwork(const TSharedPtr<FJsonObject>& Root, FVoidMetroData& OutMetro, FVoidValidationReport& OutReport);

	/** Appends dead-network entries of a parsed RoadNetwork.json "tunnel_relationships" array to OutMetro.TunnelLinks. */
	static void AppendMeridianTunnelRelationships(const TSharedPtr<FJsonObject>& RoadNetworkRoot, FVoidMetroData& OutMetro, FVoidValidationReport& OutReport);

	/** Maps an authored "metro" object (see Docs/MetroGeneratorArchitecture.md for the shape). Appends to OutMetro. */
	static void MapPackageMetroBlock(const TSharedPtr<FJsonObject>& MetroObject, FVoidMetroData& OutMetro, FVoidValidationReport& OutReport);

	/**
	 * Applies Authored on top of Base by id: a matching station/line takes the
	 * authored position/centerline/grade/entrances; unmatched authored items
	 * are appended. Base topology fields not present in Authored are kept.
	 */
	static void MergeAuthoredOverrides(FVoidMetroData& Base, const FVoidMetroData& Authored, FVoidValidationReport& OutReport);
};
