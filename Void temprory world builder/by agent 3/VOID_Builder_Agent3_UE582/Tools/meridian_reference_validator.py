#!/usr/bin/env python3
"""
Reference implementation of the Meridian package rules (VR-001..VR-012 plus
integration checks) that FVoidMeridianValidator implements in C++.

Purpose:
  * Lets CI / a human validate a Meridian package WITHOUT launching Unreal.
  * Acts as the oracle for the C++ automation tests: same inputs must yield
    the same error codes.
  * Produced the numbers in Reports/MeridianFirstLookReport.md.

Usage: meridian_reference_validator.py <MeridianDir> [--json out.json] [--allow-missing-districts]
Exit code 1 if any ERROR/FATAL issue was found.
Stdlib only (includes a JSON-Schema *subset* checker matching FVoidJsonSchemaSubset).
"""
import sys, os, re, json, glob, hashlib, itertools, collections

SUPPORTED_SCHEMA_MAJOR = 1
EXPECTED_DISTRICTS = {"olympus_spire", "white_zones", "metro_archives", "undercroft", "sector_0"}
# Fields whose values are district ids. mission_relationships[].district is
# deliberately NOT here: in the real data it is polymorphic (see POLY below).
DISTRICT_KEYS = {"origin_district", "owning_district", "district"}
DISTRICT_LIST_KEYS = {"serves_districts", "shared_by_districts"}
COORD_KEY_TOKENS = ("coordinate", "transform", "mesh_id", "centerline", "footprint", "vertices", "position_xyz", "location_xyz")

class Report:
    def __init__(self): self.issues = []
    def add(self, sev, code, msg, obj="", src="", path="", fix=""):
        self.issues.append(dict(severity=sev, code=code, message=msg, objectId=obj, source=src, fieldPath=path, suggestedFix=fix))
    def count(self, sev): return sum(1 for i in self.issues if i["severity"] == sev)

def sha256(b): return hashlib.sha256(b).hexdigest()

# ---- JSON Schema subset (mirrors FVoidJsonSchemaSubset) -------------------
def _type_ok(v, t):
    return {"object": isinstance(v, dict), "array": isinstance(v, list), "string": isinstance(v, str),
            "boolean": isinstance(v, bool),
            "integer": (isinstance(v, (int, float)) and not isinstance(v, bool) and float(v).is_integer()),
            "number": isinstance(v, (int, float)) and not isinstance(v, bool), "null": v is None}.get(t, True)
def schema_check(inst, sch, path, out):
    if "const" in sch and inst != sch["const"]: out.append((path, "VOID.Schema.ConstMismatch", f"expected constant {sch['const']!r}, found {inst!r}"))
    if "enum" in sch and inst not in sch["enum"]: out.append((path, "VOID.Schema.EnumMismatch", f"value {inst!r} not one of {sch['enum']}"))
    t = sch.get("type")
    if t:
        ts = t if isinstance(t, list) else [t]
        if not any(_type_ok(inst, x) for x in ts):
            out.append((path, "VOID.Schema.WrongType", f"expected {'/'.join(ts)}, found {type(inst).__name__}")); return
    if isinstance(inst, str):
        if "minLength" in sch and len(inst) < sch["minLength"]: out.append((path, "VOID.Schema.MinLength", "string shorter than minLength"))
        if "pattern" in sch and not re.search(sch["pattern"], inst): out.append((path, "VOID.Schema.Pattern", f"{inst!r} does not match {sch['pattern']}"))
    if isinstance(inst, (int, float)) and not isinstance(inst, bool):
        if "minimum" in sch and inst < sch["minimum"]: out.append((path, "VOID.Schema.Range", f"{inst} below minimum {sch['minimum']}"))
        if "maximum" in sch and inst > sch["maximum"]: out.append((path, "VOID.Schema.Range", f"{inst} above maximum {sch['maximum']}"))
    if isinstance(inst, list):
        if "minItems" in sch and len(inst) < sch["minItems"]: out.append((path, "VOID.Schema.ItemCount", f"{len(inst)} items, minimum {sch['minItems']}"))
        if "maxItems" in sch and len(inst) > sch["maxItems"]: out.append((path, "VOID.Schema.ItemCount", f"{len(inst)} items, maximum {sch['maxItems']}"))
        if sch.get("uniqueItems") and len({json.dumps(x, sort_keys=True) for x in inst}) != len(inst): out.append((path, "VOID.Schema.UniqueItems", "array items are not unique"))
        if "items" in sch:
            for i, x in enumerate(inst): schema_check(x, sch["items"], f"{path}[{i}]", out)
    if isinstance(inst, dict):
        for r in sch.get("required", []):
            if r not in inst: out.append((path, "VOID.Schema.MissingField", f"missing required field '{r}'"))
        if "minProperties" in sch and len(inst) < sch["minProperties"]: out.append((path, "VOID.Schema.PropertyCount", "too few properties"))
        if "maxProperties" in sch and len(inst) > sch["maxProperties"]: out.append((path, "VOID.Schema.PropertyCount", "too many properties"))
        props = sch.get("properties", {})
        for k, v in inst.items():
            if k in props: schema_check(v, props[k], f"{path}.{k}", out)
            else:
                ap = sch.get("additionalProperties", True)
                if ap is False: out.append((path, "VOID.Schema.UnknownField", f"unexpected field '{k}'"))
                elif isinstance(ap, dict): schema_check(v, ap, f"{path}.{k}", out)

def dag_cycle(g):
    color = {}
    def dfs(n, st):
        color[n] = 1
        for v in g.get(n, []):
            if color.get(v) == 1: return st + [n, v]
            if v not in color and v in g:
                r = dfs(v, st + [n])
                if r: return r
        color[n] = 2
        return None
    for n in list(g):
        if n not in color:
            r = dfs(n, [])
            if r: return r
    return None

def validate(root, allow_missing_districts=False, strict_lock=True):
    R = Report(); S = {}
    files = {f: open(os.path.join(root, f), "rb").read() for f in os.listdir(root) if os.path.isfile(os.path.join(root, f))}
    J = {}
    # VR-001
    for f, b in sorted(files.items()):
        if f.endswith(".json"):
            try: J[f] = json.loads(b.decode("utf-8-sig"))
            except Exception as e:
                R.add("Error", "VOID.Meridian.InvalidJson", f"{f} is not valid JSON: {e}", f, f, "", "Fix the syntax error; re-export the file.")
    if "Meridian_Master.json" not in J:
        R.add("Fatal", "VOID.Meridian.ManifestMissing", "Meridian_Master.json (the single import manifest) is missing or unreadable.", "Meridian_Master.json", root, "", "Restore Meridian_Master.json.")
        return R, S
    MM = J["Meridian_Master.json"]
    # required registry files
    reg_files = {}
    for key, ref in MM.get("registry_references", {}).items():
        f = ref.get("file"); reg_files[key] = f
        if ref.get("required") and f not in files:
            R.add("Error", "VOID.Meridian.MissingRequiredFile", f"Required registry '{key}' -> '{f}' is not present in the package.", key, "Meridian_Master.json", f"$.registry_references.{key}.file", "Add the file or remove it from registry_references.")
    # schema checks (VR-002) + schema version support
    no_schema = []
    for f in sorted(J):
        if f.endswith(".schema.json"): continue
        sf = f[:-5] + ".schema.json"
        decl = J[f].get("$schema") if isinstance(J[f], dict) else None
        if decl:
            m = re.search(r"_v(\d+)$", decl)
            if m and int(m.group(1)) != SUPPORTED_SCHEMA_MAJOR:
                R.add("Error", "VOID.Meridian.UnsupportedSchemaVersion", f"{f} declares schema '{decl}', this Builder supports v{SUPPORTED_SCHEMA_MAJOR}.", f, f, "$.$schema", "Re-export at a supported schema version or upgrade the Builder.")
        if sf in J:
            if J[sf].get("$id") != decl:
                R.add("Error", "VOID.Meridian.SchemaIdMismatch", f"{f} declares $schema '{decl}' but {sf} has $id '{J[sf].get('$id')}'.", f, f, "$.$schema", "Point the data file at the schema that describes it.")
            out = []
            schema_check(J[f], J[sf], "$", out)
            for p, c, m in out:
                R.add("Error", c, f"{f}: {m}", f, f, p, "Bring the file into conformance with " + sf)
        elif not f.startswith("Meridian_") or True:
            no_schema.append(f)
    S["files_without_schema"] = no_schema
    # ids
    DR = J.get("DistrictRegistry.json"); WP = J.get("WorldPartition.json"); RN = J.get("RoadNetwork.json"); MN = J.get("MetroNetwork.json"); DL = J.get("DataLayers.json"); BRl = J.get("BuilderRules.json")
    districts = set()
    if DR:
        ids = [d["id"] for d in DR.get("districts", [])]
        for i, c in collections.Counter(ids).items():
            if c > 1: R.add("Error", "VOID.Meridian.DuplicateId", f"District id '{i}' appears {c} times in DistrictRegistry.districts.", i, "DistrictRegistry.json", "$.districts", "Make district ids unique.")
        districts = set(ids)
        if districts != EXPECTED_DISTRICTS:
            R.add("Error", "VOID.Meridian.DistrictSetMismatch", f"District set {sorted(districts)} differs from the canon-locked five {sorted(EXPECTED_DISTRICTS)} (VR-007).", "", "DistrictRegistry.json", "$.districts", "Districts may only be added through the registry extension review gate.")
        # VR-010
        pairs = [tuple(sorted(r["pair"])) for r in DR.get("relationships", [])]
        allp = set(itertools.combinations(sorted(districts), 2))
        for p in sorted(allp - set(pairs)): R.add("Error", "VOID.Meridian.MissingRelationshipPair", f"No relationship documented for pair {p} (VR-010).", "|".join(p), "DistrictRegistry.json", "$.relationships", "Document the pair exactly once.")
        for p, c in collections.Counter(pairs).items():
            if c > 1: R.add("Error", "VOID.Meridian.DuplicateRelationshipPair", f"Pair {p} documented {c} times (VR-010).", "|".join(p), "DistrictRegistry.json", "$.relationships", "Keep exactly one entry per pair.")
        dep = DR.get("dependencies", {})
        cyc = dag_cycle(dep)
        if cyc: R.add("Error", "VOID.Meridian.DependencyCycle", "District dependency cycle: " + " -> ".join(cyc), cyc[0], "DistrictRegistry.json", "$.dependencies", "Break the cycle (VR-009).")
        for d in DR.get("districts", []):
            if sorted(d.get("depends_on", [])) != sorted(dep.get(d["id"], [])):
                R.add("Error", "VOID.Meridian.DependencyMismatch", f"District '{d['id']}' depends_on disagrees with the 'dependencies' map.", d["id"], "DistrictRegistry.json", "$.districts", "Make both dependency declarations agree.")
        S["order_note"] = "build_order_index and generation_priority are different orderings"
        R.add("Info", "VOID.Meridian.AmbiguousOrderFields", "DistrictRegistry has both build_order_index (authoring order) and generation_priority (generation order); they intentionally disagree. Generators must use Meridian_Master.generation_pipeline.generation_order only.", "", "DistrictRegistry.json", "$.districts[*]", "Never derive generation order from build_order_index.")
    # VR-003 references
    if RN:
        routes = {r["id"] for k in ("primary_routes", "secondary_routes", "service_routes") for r in RN.get(k, [])}
        struct = {t["id"] for k in ("tunnel_relationships", "bridge_relationships") for t in RN.get(k, [])}
        nodes = set(RN.get("traversal_graph", {}).get("nodes", []))
        for e in RN.get("traversal_graph", {}).get("edges", []):
            if e["via"] not in routes | struct: R.add("Error", "VOID.Meridian.UnresolvedReference", f"Traversal edge via '{e['via']}' matches no route/tunnel/bridge id.", e["via"], "RoadNetwork.json", "$.traversal_graph.edges", "Fix the id or define the route.")
            for k in ("from", "to"):
                if e[k] not in nodes: R.add("Error", "VOID.Meridian.UnresolvedReference", f"Traversal edge {k} '{e[k]}' is not in traversal_graph.nodes.", e[k], "RoadNetwork.json", "$.traversal_graph.edges", "Add the node or fix the id.")
        S["route_ids"] = sorted(routes)
        if MN:
            for net in MN.get("networks", []):
                for ln in net.get("lines", []):
                    if ln.get("shares_route_id") and ln["shares_route_id"] not in routes:
                        R.add("Error", "VOID.Meridian.UnresolvedReference", f"Metro line '{ln['id']}' shares_route_id '{ln['shares_route_id']}' matches no road route.", ln["id"], "MetroNetwork.json", "$.networks[*].lines", "Fix the route id.")
    if MN:
        stations = {s["id"] for n in MN.get("networks", []) for s in n.get("stations", [])}
        for c in MN.get("district_connectivity", []):
            if c.get("station_ref") and c["station_ref"] not in stations:
                R.add("Error", "VOID.Meridian.UnresolvedReference", f"district_connectivity station_ref '{c['station_ref']}' matches no station.", c["station_ref"], "MetroNetwork.json", "$.district_connectivity", "Fix the station id.")
    def walk(n, f, p="$"):
        if isinstance(n, dict):
            for k, v in n.items():
                q = f"{p}.{k}"
                if districts:
                    if k in DISTRICT_KEYS and isinstance(v, str) and v not in districts:
                        if f == "GameplayGraph.json" and re.match(r"^\$\.mission_relationships\[\d+\]\.district$", q):
                            R.add("Warning", "VOID.Meridian.PolymorphicDistrictField", f"'district' value '{v}' is not a district id; in GameplayGraph.mission_relationships this field also carries sub-locations/transitions.", v, f, q, "Ignore, or split into district + location fields in the next schema revision.")
                        else:
                            R.add("Error", "VOID.Meridian.UnresolvedReference", f"District reference '{v}' matches no district in DistrictRegistry.", v, f, q, "Fix the district id.")
                    if k in DISTRICT_LIST_KEYS and isinstance(v, list):
                        for x in v:
                            if isinstance(x, str) and x not in districts: R.add("Error", "VOID.Meridian.UnresolvedReference", f"District reference '{x}' matches no district in DistrictRegistry.", x, f, q, "Fix the district id.")
                # VR-012
                if any(t in k.lower() for t in COORD_KEY_TOKENS) and not isinstance(v, str):
                    R.add("Error", "VOID.Meridian.FabricatedGeometry", f"Key '{k}' carries coordinate/transform-shaped data; the package policy forbids fabricated engineering precision (VR-012).", k, f, q, "Remove it; geometry is computed by the Builder.")
                walk(v, f, q)
        elif isinstance(n, list):
            if 2 <= len(n) <= 3 and all(isinstance(x, (int, float)) and not isinstance(x, bool) for x in n):
                R.add("Warning", "VOID.Meridian.CoordinateShapedArray", f"Numeric tuple {n} looks like a coordinate (VR-012).", "", f, p, "Confirm it is not a world position.")
            for i, x in enumerate(n): walk(x, f, f"{p}[{i}]")
    for f in sorted(J):
        if not f.endswith(".schema.json") and f not in ("PackageManifest.json",): walk(J[f], f)
    # WorldPartition
    if WP and districts:
        rd = {r["district"] for r in WP.get("regions", [])}
        if rd != districts: R.add("Error", "VOID.Meridian.DistrictSetMismatch", f"WorldPartition regions cover {sorted(rd)} but registry has {sorted(districts)}.", "", "WorldPartition.json", "$.regions", "Give every district exactly the regions it needs.")
        regions = {r["id"] for r in WP["regions"]}
        for did, sr in (DR or {}).get("streaming_relationships", {}).items():
            if sr["region"] not in regions: R.add("Error", "VOID.Meridian.UnresolvedReference", f"Registry streaming region '{sr['region']}' for '{did}' missing from WorldPartition.", sr["region"], "DistrictRegistry.json", "$.streaming_relationships", "Fix the region id.")
        for r in WP["regions"]:
            ci = {c["id"] for c in r["streaming_cells"]}
            for cd in r.get("cell_dependencies", []):
                for k in ("cell", "streams_after"):
                    if cd[k] not in ci: R.add("Error", "VOID.Meridian.UnresolvedReference", f"Cell dependency {k} '{cd[k]}' is not a cell of {r['id']}.", cd[k], "WorldPartition.json", "$.regions", "Fix the cell id.")
        # VR-004: union of contains over the district's non-template regions must cover its sub_locations
        contained = collections.defaultdict(set)
        for r in WP["regions"]:
            if not r.get("template_based", False):
                for cell in r["streaming_cells"]: contained[r["district"]].update(cell["contains"])
        for d in DR["districts"]:
            dfile = d["data_reference"]
            subs = J.get(dfile, {}).get("sub_locations") if isinstance(J.get(dfile), dict) else None
            if not isinstance(subs, list):
                R.add("Info", "VOID.Meridian.SubLocationCheckSkipped", f"VR-004 for '{d['id']}' skipped: {dfile} is not present.", d["id"], "WorldPartition.json", "", "Supply the district data file.")
                continue
            ids = {x["id"] for x in subs if isinstance(x, dict) and "id" in x}
            for sid in sorted(ids - contained[d["id"]]): R.add("Error", "VOID.Meridian.SubLocationUncovered", f"Sub-location '{sid}' of '{d['id']}' is in no streaming cell (VR-004).", sid, "WorldPartition.json", "$.regions", "Add it to exactly one cell.")
            for cid in sorted(contained[d["id"]] - ids): R.add("Warning", "VOID.Meridian.CellContainsUnknownSubLocation", f"A '{d['id']}' streaming cell contains '{cid}', not a sub_location of {dfile}.", cid, "WorldPartition.json", "$.regions", "Fix the id or record an explicit exception.")
    # VR-005
    if DL:
        actual = sum(len(g.get("layers", [])) for g in DL.get("district_gating_layers", []))
        if DL.get("total_district_gating_layers") != actual:
            R.add("Error", "VOID.Meridian.ArithmeticMismatch", f"DataLayers declares total_district_gating_layers={DL.get('total_district_gating_layers')} but contains {actual} (VR-005).", "total_district_gating_layers", "DataLayers.json", "$.total_district_gating_layers", "Correct the declared total.")
    # VR-009 module deps + generation order
    gp = MM.get("generation_pipeline", {})
    md = gp.get("module_dependencies", {}); roots = set(MM.get("registry_references", {}).keys())
    for n, vs in md.items():
        for v in vs:
            if v not in md and v not in roots: R.add("Error", "VOID.Meridian.UnresolvedReference", f"module_dependencies: '{n}' depends on unknown module '{v}'.", v, "Meridian_Master.json", "$.generation_pipeline.module_dependencies", "Fix the module id.")
    cyc = dag_cycle(md)
    if cyc: R.add("Error", "VOID.Meridian.DependencyCycle", "Module dependency cycle: " + " -> ".join(cyc), cyc[0], "Meridian_Master.json", "$.generation_pipeline.module_dependencies", "Break the cycle (VR-009).")
    seen = set()
    for s in gp.get("generation_order", []):
        for d in s.get("depends_on", []):
            if d not in seen: R.add("Error", "VOID.Meridian.GenerationOrderViolation", f"Step {s['step']} '{s['target']}' depends on '{d}', which is not an earlier step.", s["target"], "Meridian_Master.json", "$.generation_pipeline.generation_order", "Reorder or fix the dependency.")
        seen.add(s["target"])
    S["generation_targets"] = [s["target"] for s in gp.get("generation_order", [])]
    # import_order vs module_dependencies
    order = {m: i for i, m in enumerate(gp.get("import_order", []))}
    for n, vs in md.items():
        for v in vs:
            if v in order and n in order and order[v] > order[n]:
                R.add("Error", "VOID.Meridian.ImportOrderViolation", f"import_order places '{n}' before its dependency '{v}'.", n, "Meridian_Master.json", "$.generation_pipeline.import_order", "Reorder import_order.")
    # flagged items
    for fl in MM.get("flagged_open_items", []):
        R.add("Info", "VOID.Meridian.FlaggedOpenItem", f"Open item '{fl['id']}' ({fl['priority']}, {fl['status']}); blocks: {fl['blocks'] or 'nothing'}.", fl["id"], "Meridian_Master.json", "$.flagged_open_items", "Do not generate blocked content without recorded sign-off." if fl["blocks"] else "Log sign-off reminder.")
    # VR-011 / manifest
    pm = J.get("PackageManifest.json")
    if pm:
        for sect in ("locked_inputs", "package_files"):
            for e in pm.get(sect, []):
                f = e["file"]
                if f not in files:
                    sev = "Warning" if (allow_missing_districts or not strict_lock) else "Error"
                    R.add(sev, "VOID.Meridian.LockedInputMissing", f"Manifest {sect} entry '{f}' is not present in the package.", f, "PackageManifest.json", f"$.{sect}", "Supply the file. Generation of the district it belongs to cannot be verified without it.")
                    continue
                h = sha256(files[f])
                if e.get("sha256") and h != e["sha256"]:
                    lf = sha256(files[f].replace(b"\r\n", b"\n"))
                    if lf == e["sha256"]: R.add("Warning", "VOID.Meridian.LineEndingChecksum", f"{f}: checksum differs only by CRLF line endings.", f, "PackageManifest.json", f"$.{sect}", "Normalise to LF (set .gitattributes eol=lf).")
                    else: R.add("Error", "VOID.Meridian.ChecksumMismatch", f"{f}: sha256 differs from the manifest (VR-011).", f, "PackageManifest.json", f"$.{sect}", "Restore the locked file or regenerate the manifest with sign-off.")
                elif e.get("size_bytes") and len(files[f]) != e["size_bytes"] and h == e.get("sha256"): pass
        listed = {e["file"] for s in ("locked_inputs", "package_files") for e in pm.get(s, [])}
        unlisted = sorted(f for f in files if f not in listed)
        if unlisted: R.add("Warning", "VOID.Meridian.UnlistedFile", f"{len(unlisted)} file(s) on disk are not inventoried by PackageManifest: {', '.join(unlisted)}.", "", "PackageManifest.json", "", "Regenerate PackageManifest.json so it inventories every file.")
        t = pm.get("totals", {})
        if t and t.get("total_files") is not None and t["total_files"] != len(files):
            R.add("Warning", "VOID.Meridian.ManifestTotalsStale", f"PackageManifest.totals.total_files={t['total_files']} but the package directory holds {len(files)} file(s).", "totals", "PackageManifest.json", "$.totals", "Regenerate the manifest.")
    # geometry source
    R.add("Info", "VOID.Meridian.NoGeometrySource", "Meridian_Master is coordinate-free by design (builder_configuration.coordinate_policy). FVoidDesignPackage/FVoidRoadGenerator require centerlinePoints and footprints; a layout stage must synthesise them.", "", "Meridian_Master.json", "$.builder_configuration", "Assign ownership of the Meridian -> FVoidDesignPackage layout/adapter step.")
    if no_schema: R.add("Info", "VOID.Meridian.NoSchemaForFile", f"{len(no_schema)} JSON file(s) have no companion .schema.json and were only syntax-checked: {', '.join(no_schema)}.", "", root, "", "Optional: author schemas (void_world_location_schema_v1 is known to be unmaterialised).")
    S["districts"] = sorted(districts)
    return R, S

def main(argv):
    if len(argv) < 2: print(__doc__); return 2
    R, S = validate(argv[1], allow_missing_districts="--allow-missing-districts" in argv)
    for i in R.issues: print(f"[{i['severity'].upper():7}] {i['code']}  {i['source']} {i['fieldPath']}\n           {i['message']}")
    print(f"\nFatal={R.count('Fatal')} Error={R.count('Error')} Warning={R.count('Warning')} Info={R.count('Info')}")
    if "--json" in argv:
        json.dump(dict(summary=S, issues=R.issues), open(argv[argv.index("--json") + 1], "w"), indent=2)
    return 1 if (R.count("Error") or R.count("Fatal")) else 0
if __name__ == "__main__": sys.exit(main(sys.argv))
