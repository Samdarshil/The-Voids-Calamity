#!/usr/bin/env python3
"""
Static consistency lint for the VOID World Builder plugin.

THIS IS NOT A COMPILER. It cannot prove the code compiles against Unreal 5.8.2.
It catches the mechanical mistakes that otherwise cost a full UBT cycle:
  * unbalanced braces/parens (comments, strings and raw strings stripped)
  * #include "X" that resolves neither inside the plugin nor to a known engine header
  * .generated.h not the LAST include / missing GENERATED_BODY in UCLASS/USTRUCT headers
  * .uplugin modules vs Source/<Module> vs <Module>.Build.cs vs IMPLEMENT_MODULE
  * exported classes using the wrong MODULE_API macro
  * Public headers including Private headers
  * including another module's header without a Build.cs dependency
  * module dependency cycles
  * test sources not guarded by WITH_DEV_AUTOMATION_TESTS
Usage: static_lint.py <PluginRoot>      exit 1 on any finding
"""
import sys, os, re, json, collections

ENGINE_HEADER_PREFIXES = (
    "CoreMinimal.h", "Modules/", "Misc/", "HAL/", "Containers/", "Logging/", "Serialization/", "Dom/", "Internationalization/",
    "GameFramework/", "Engine/", "Components/", "Commandlets/", "UObject/", "Templates/", "Framework/", "Widgets/", "Styling/",
    "EngineUtils.h", "ProceduralMeshComponent.h", "Editor", "Subsystems/", "Kismet/", "Materials/", "Interfaces/IPluginManager.h", "Async/", "Algo/",
    "Math/", "Delegates/", "Stats/", "Tickable", "Slate", "ScopedTransaction.h", "LevelEditor", "ToolMenus.h", "PropertyEditor", "DesktopPlatform", "Features/", "JsonObjectConverter.h", "IDetailCustomization.h", "IDesktopPlatform.h", "GenericPlatform/", "WorkspaceMenuStructure", "DrawDebugHelpers.h", "Interfaces/IMainFrameModule.h")

def strip(src):
    out=[]; i=0; n=len(src)
    while i<n:
        c=src[i]
        if src.startswith("//",i):
            j=src.find("\n",i); i = n if j<0 else j; continue
        if src.startswith("/*",i):
            j=src.find("*/",i+2); i = n if j<0 else j+2; out.append(" "); continue
        m=re.match(r'(?:u8|u|U|L)?R"([^\s()\\]{0,16})\(', src[i:i+24])
        if m:
            end=")"+m.group(1)+'"'; j=src.find(end,i+m.end())
            if j<0: return "".join(out)
            i=j+len(end); out.append('""'); continue
        if c=='"':
            j=i+1
            while j<n and src[j]!='"':
                j+= 2 if src[j]=="\\" else 1
            i=j+1; out.append('""'); continue
        if c=="'":
            j=i+1
            while j<n and src[j]!="'":
                j+= 2 if src[j]=="\\" else 1
            i=j+1; out.append("''"); continue
        out.append(c); i+=1
    return "".join(out)

def main(root):
    F=[]
    src_root=os.path.join(root,"Source")
    modules=sorted(d for d in os.listdir(src_root) if os.path.isdir(os.path.join(src_root,d)))
    files={}
    for m in modules:
        for dp,_,fs in os.walk(os.path.join(src_root,m)):
            for f in fs:
                if f.endswith((".h",".cpp",".cs")): files[os.path.join(dp,f)]=m
    # uplugin
    up=json.load(open(next(os.path.join(root,f) for f in os.listdir(root) if f.endswith(".uplugin"))))
    up_mods={m["Name"] for m in up["Modules"]}
    if up_mods!=set(modules): F.append(f"uplugin modules {sorted(up_mods)} != Source dirs {modules}")
    if not up.get("EngineVersion","").startswith("5.8"): F.append(f"EngineVersion is {up.get('EngineVersion')!r}, expected 5.8.x")
    # Build.cs deps
    deps={}; pubdeps={}
    for m in modules:
        bp=os.path.join(src_root,m,m+".Build.cs")
        if not os.path.exists(bp): F.append(f"{m}: missing {m}.Build.cs"); continue
        t=open(bp).read()
        if f"class {m} " not in t and f"class {m}\n" not in t: F.append(f"{m}: Build.cs class name != module name")
        names=set(re.findall(r'"([A-Za-z0-9_]+)"', t)); deps[m]=names
        if not re.search(r"IMPLEMENT_(PRIMARY_GAME_)?MODULE\([A-Za-z0-9_]+,\s*"+m+r"\)", "".join(open(f).read() for f,mm in files.items() if mm==m and f.endswith(".cpp"))):
            F.append(f"{m}: no IMPLEMENT_MODULE(..., {m})")
    if any("ProceduralMeshComponent" in d for d in deps.values()):
        if not any(p.get("Name")=="ProceduralMeshComponent" for p in up.get("Plugins",[])): F.append("ProceduralMeshComponent module used but plugin not declared in .uplugin Plugins")
    # cycles
    local={m:{d for d in deps.get(m,set()) if d in modules} for m in modules}
    def cyc():
        st={}
        def dfs(n,p):
            st[n]=1
            for v in local[n]:
                if st.get(v)==1: return p+[n,v]
                if v not in st:
                    r=dfs(v,p+[n])
                    if r: return r
            st[n]=2
        for n in modules:
            if n not in st:
                r=dfs(n,[])
                if r: return r
    c=cyc()
    if c: F.append("module dependency cycle: "+" -> ".join(c))
    # header index
    hdr={}
    for f,m in files.items():
        if f.endswith(".h"):
            rel=f.split("/Public/",1)[1] if "/Public/" in f else (f.split("/Private/",1)[1] if "/Private/" in f else None)
            if rel: hdr.setdefault(rel,[]).append((f,m))
    for f,m in sorted(files.items()):
        if f.endswith(".cs"): continue
        raw=open(f,encoding="utf-8").read(); s=strip(raw)
        rel=os.path.relpath(f,root)
        for a,b in ("{}","()","[]"):
            if s.count(a)!=s.count(b): F.append(f"{rel}: unbalanced {a}{b} ({s.count(a)} vs {s.count(b)})")
        incs=re.findall(r'^\s*#include\s+"([^"]+)"', raw, re.M)
        is_public="/Public/" in f
        for k,inc in enumerate(incs):
            if inc.endswith(".generated.h"):
                if k!=len(incs)-1: F.append(f"{rel}: {inc} must be the last include")
                continue
            base=os.path.basename(inc)
            cands=hdr.get(inc) or [(p,mm) for r,l in hdr.items() if os.path.basename(r)==base and (r==inc or inc==base) for p,mm in l]
            if cands:
                tm=cands[0][1]
                if is_public and "/Private/" in cands[0][0]: F.append(f"{rel}: Public header includes Private header {inc}")
                if tm!=m and tm not in deps.get(m,set()): F.append(f"{rel}: includes {inc} from module {tm} but {m}.Build.cs does not depend on it")
                if tm!=m and "/Private/" in cands[0][0]: F.append(f"{rel}: includes another module's Private header {inc}")
            elif not inc.startswith(ENGINE_HEADER_PREFIXES):
                F.append(f"{rel}: cannot resolve #include \"{inc}\" (not in plugin, not a known engine header)")
        if f.endswith(".h") and re.search(r"\b(UCLASS|USTRUCT|UENUM)\s*\(", s):
            if not any(i.endswith(".generated.h") for i in incs): F.append(f"{rel}: has UCLASS/USTRUCT but no .generated.h include")
            if re.search(r"\bU(CLASS|STRUCT)\s*\(", s) and "GENERATED_BODY" not in s: F.append(f"{rel}: missing GENERATED_BODY")
        # API macro
        api=re.sub(r"[^A-Z]","",m.upper())+"_API"
        for mm in re.finditer(r"\b(?:class|struct)\s+([A-Z0-9_]+_API)\s+[A-Za-z0-9_]+", s):
            if mm.group(1)!=api and "/Public/" in f: F.append(f"{rel}: uses {mm.group(1)} but module {m} exports {api}")
        if "/Tests/" in f and f.endswith(".cpp") and "WITH_DEV_AUTOMATION_TESTS" not in raw: F.append(f"{rel}: test source not guarded by WITH_DEV_AUTOMATION_TESTS")
        if f.endswith(".cpp") and "DECLARE_LOG_CATEGORY_EXTERN" in s and False: pass
    print(f"Checked {len(files)} files in {len(modules)} modules: {', '.join(modules)}")
    for x in F: print("LINT:",x)
    print("LINT CLEAN" if not F else f"{len(F)} FINDING(S)")
    return 1 if F else 0
if __name__=="__main__": sys.exit(main(sys.argv[1]))
