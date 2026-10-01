#!/usr/bin/env python3
"""
Reference checker for the Meridian_Master package (Agent 1).

Independent of Unreal. Mirrors the rules FVoidMeridianImporter / FVoidMeridianValidator
implement in C++, so CI can cross-check the two and so importer behaviour can be
compared against a known-good baseline.

Usage: python3 meridian_reference_check.py "<path to 'Meridian Master' folder>" [--strict-canon-lock]
Exit code: 0 = no errors, 1 = errors found.
Implements a *subset* of JSON-Schema 2020-12 sufficient for the 12 Meridian schemas:
type, required, additionalProperties, properties, items, enum, const, minItems, maxItems, minimum.
"""
import json, os, sys, hashlib, re

REGISTRY_KEYS = ["district_registry","road_network","metro_network","navigation_graph","gameplay_graph",
                 "landmark_registry","infrastructure_graph","world_partition","data_layers",
                 "streaming_strategy","builder_rules"]
errors, warnings, infos = [], [], []
def E(code,msg): errors.append((code,msg))
def W(code,msg): warnings.append((code,msg))
def I(code,msg): infos.append((code,msg))

def jtype(v):
    if v is None: return "null"
    if isinstance(v,bool): return "boolean"
    if isinstance(v,int): return "integer"
    if isinstance(v,float): return "number"
    if isinstance(v,str): return "string"
    if isinstance(v,list): return "array"
    return "object"

def tmatch(v,t):
    ts = t if isinstance(t,list) else [t]
    for x in ts:
        if jtype(v)==x or (x=="number" and jtype(v) in("integer","number")): return True
    return False

def validate(v, s, path, out):
    if "const" in s and v != s["const"]: out.append(f"{path}: expected const {s['const']!r}")
    if "enum" in s and v not in s["enum"]: out.append(f"{path}: {v!r} not in enum {s['enum']}")
    if "type" in s and not tmatch(v,s["type"]):
        out.append(f"{path}: expected {s['type']}, got {jtype(v)}"); return
    if isinstance(v,(int,float)) and not isinstance(v,bool) and "minimum" in s and v < s["minimum"]:
        out.append(f"{path}: {v} < minimum {s['minimum']}")
    if isinstance(v,dict):
        for r in s.get("required",[]):
            if r not in v: out.append(f"{path}: missing required '{r}'")
        props = s.get("properties",{})
        if s.get("additionalProperties") is False:
            for k in v:
                if k not in props: out.append(f"{path}: unexpected property '{k}'")
        for k,sub in props.items():
            if k in v: validate(v[k],sub,f"{path}.{k}",out)
    if isinstance(v,list):
        if "minItems" in s and len(v)<s["minItems"]: out.append(f"{path}: {len(v)} items < minItems {s['minItems']}")
        if "maxItems" in s and len(v)>s["maxItems"]: out.append(f"{path}: {len(v)} items > maxItems {s['maxItems']}")
        if "items" in s:
            for i,x in enumerate(v): validate(x,s["items"],f"{path}[{i}]",out)

SPATIAL_TOKENS = {"x","y","z","coord","coords","coordinate","coordinates","position","transform","location","elevation",
                  "altitude","latitude","longitude","units","unit","cm","meters","metres","km","feet","ft","width","radius"}
def _isnum(v): return isinstance(v,(int,float)) and not isinstance(v,bool)
def scan_spatial(o, path, hits):
    """Mirror of VoidMeridianPrivate::ScanSpatial in VoidMeridianImporter.cpp."""
    if isinstance(o,dict):
        for k,v in o.items():
            if (_isnum(v) or isinstance(v,list)) and any(p in SPATIAL_TOKENS for p in k.lower().split("_") if p):
                if _isnum(v) or any(_isnum(e) for e in v): hits.append(f"{path}.{k}")
            scan_spatial(v,f"{path}.{k}",hits)
    elif isinstance(o,list):
        if len(o) in (2,3) and all(_isnum(e) for e in o): hits.append(f"{path} (numeric tuple)")
        for i,v in enumerate(o): scan_spatial(v,f"{path}[{i}]",hits)

def main():
    root = sys.argv[1] if len(sys.argv)>1 else "."
    strict = "--strict-canon-lock" in sys.argv
    def load(fn):
        p=os.path.join(root,fn)
        try: return json.load(open(p,encoding="utf-8"))
        except FileNotFoundError: E("VOID.Meridian.MissingFile",fn); return None
        except json.JSONDecodeError as ex: E("VOID.Meridian.MalformedJson",f"{fn}: {ex}"); return None
    master = load("Meridian_Master.json")
    if not master: return report()
    refs = master.get("registry_references",{})
    data = {}
    for key in master["generation_pipeline"]["import_order"]:
        ref = refs.get(key)
        if not ref: E("VOID.Meridian.UnknownRegistryKey",key); continue
        data[key]=load(ref["file"])
    for key,ref in refs.items():
        if key=="validation_report": continue
        if ref.get("required") and not os.path.exists(os.path.join(root,ref["file"])): E("VOID.Meridian.MissingFile",ref["file"])
    # VR-002 schema conformance
    for fn in ["Meridian_Master"]+[os.path.splitext(refs[k]["file"])[0] for k in data]:
        sp=os.path.join(root,fn+".schema.json")
        if not os.path.exists(sp): W("VOID.Meridian.NoSchemaFile",fn); continue
        doc = master if fn=="Meridian_Master" else next(d for k,d in data.items() if os.path.splitext(refs[k]["file"])[0]==fn)
        if doc is None: continue
        out=[]; validate(doc,json.load(open(sp)),fn,out)
        for m in out: E("VOID.Meridian.SchemaViolation",m)
        if not out: I("VOID.Meridian.SchemaOK",fn)
    # $schema id per registry + import_order vs module_dependencies + generation_order
    for key,d in data.items():
        if d is not None and d.get("$schema")!=f"void_{key}_schema_v1": E("VOID.Meridian.SchemaIdMismatch",f"{key}: {d.get('$schema')}")
    order=master["generation_pipeline"]["import_order"]
    for mod,deps in master["generation_pipeline"].get("module_dependencies",{}).items():
        if mod in order:
            for dep in deps:
                if dep in order and order.index(dep)>order.index(mod): E("VOID.Meridian.ImportOrderViolation",f"{mod} loads before dependency {dep}")
    done=set()
    for st in master["generation_pipeline"]["generation_order"]:
        for dep in st.get("depends_on",[]):
            if dep not in done: E("VOID.Meridian.ImportOrderViolation",f"generation step {st['target']} depends on later/unknown {dep}")
        done.add(st["target"])
    # VR-003 road network referential integrity
    dr, rn = data.get("district_registry"), data.get("road_network")
    if dr and rn:
        districts={d["id"] for d in dr["districts"]}
        bands=set(dr["hierarchy"]["radial_bands_ordered"])
        cats={c["id"]:c for c in rn["road_categories"]}
        allroutes=[r for k in("primary_routes","secondary_routes","service_routes") for r in rn[k]]
        ids=[r["id"] for r in allroutes]+[t["id"] for t in rn["tunnel_relationships"]]+[b["id"] for b in rn["bridge_relationships"]]+list(cats)
        for i in {x for x in ids if ids.count(x)>1}: E("VOID.Meridian.DuplicateId",i)
        for h in rn["road_hierarchy"]:
            if h not in cats: E("VOID.Meridian.BrokenReference",f"road_hierarchy '{h}' not in road_categories")
        tiers=[cats[h]["hierarchy_tier"] for h in rn["road_hierarchy"] if h in cats]
        if tiers!=sorted(tiers): E("VOID.Meridian.HierarchyOrder","road_hierarchy not in ascending tier order")
        routeids={r["id"] for r in allroutes}; tunnelids={t["id"] for t in rn["tunnel_relationships"]}
        for r in allroutes:
            if r["category"] not in cats: E("VOID.Meridian.BrokenReference",f"{r['id']}.category {r['category']}")
            for d in r.get("serves_districts",[])+r.get("shared_by_districts",[])+[x for x in (r.get("origin_district"),r.get("owning_district")) if x]:
                if d not in districts: E("VOID.Meridian.BrokenReference",f"{r['id']} district {d}")
            for b in r.get("traverses_bands",[])+([r["radial_band"]] if "radial_band" in r else []):
                if b not in bands: E("VOID.Meridian.BrokenReference",f"{r['id']} band {b}")
            g=r.get("generation_dependency")
            if g and g not in districts|routeids: E("VOID.Meridian.BrokenReference",f"{r['id']}.generation_dependency {g}")
        for t in rn["tunnel_relationships"]:
            for c in t["connects"]:
                if c not in districts: E("VOID.Meridian.BrokenReference",f"{t['id']} connects {c}")
        g=rn["traversal_graph"]
        for e in g["edges"]:
            if e["via"] not in routeids|tunnelids: E("VOID.Meridian.BrokenReference",f"edge via {e['via']}")
            for n in(e["from"],e["to"]):
                if n not in g["nodes"]: E("VOID.Meridian.BrokenReference",f"edge node {n}")
        for c in rn["district_connectivity"]:
            if c["district"] not in districts: E("VOID.Meridian.BrokenReference",c["district"])
            for v in c["connected_via"]:
                if v not in cats: E("VOID.Meridian.BrokenReference",f"connectivity via {v}")
        for a,b in g["unreachable_pairs_by_design"]:
            for e in g["edges"]:
                if {e["from"],e["to"]}=={a,b}: E("VOID.Meridian.TraversalContradiction",f"{a}<->{b} declared unreachable but edge via {e['via']} exists")
        for b in rn["bridge_relationships"]:
            W("VOID.Meridian.UnresolvableExternalRef",f"bridge {b['id']} connects {b['connects']} (sub-location ids live in district data files not in this package)") if b["connects"] else None
    # VR-012 fabricated coordinate scan
    for key,d in list(data.items())+[("Meridian_Master",master)]:
        if d is None: continue
        hits=[]; scan_spatial(d,key,hits)
        for h in hits: E("VOID.Meridian.FabricatedSpatialData",h)
    if not any(c=="VOID.Meridian.FabricatedSpatialData" for c,_ in errors):
        I("VOID.Meridian.NoSpatialData","No coordinates/transforms/units found in any registry (consistent with VR-012)")
    # VR-011 checksums
    pm = load("PackageManifest.json")
    if pm:
        for grp in ("package_files","locked_inputs"):
            for e in pm[grp]:
                p=os.path.join(root,e["file"])
                if not os.path.exists(p):
                    (W if grp=="locked_inputs" else E)("VOID.Meridian.LockedInputMissing" if grp=="locked_inputs" else "VOID.Meridian.MissingFile",e["file"]); continue
                h=hashlib.sha256(open(p,"rb").read()).hexdigest()
                if h!=e["sha256"]: (E if (strict or grp=="package_files") else W)("VOID.Meridian.ChecksumMismatch",e["file"])
    return report()

def report():
    for tag,lst in(("ERROR",errors),("WARN",warnings),("INFO",infos)):
        for c,m in lst: print(f"[{tag}] {c}: {m}")
    print(f"\nSummary: {len(errors)} error(s), {len(warnings)} warning(s), {len(infos)} info")
    return 1 if errors else 0
sys.exit(main())
