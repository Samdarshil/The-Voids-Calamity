#!/usr/bin/env python3
from pathlib import Path
import json, re, sys

ROOT = Path(__file__).resolve().parents[2]
errors=[]; warnings=[]

def check_file(rel):
    p=ROOT/rel
    if not p.exists():
        errors.append(f"Missing: {rel}")
    return p

# Descriptor
try:
    data=json.loads(check_file("VOIDWorldBuilder.uplugin").read_text())
    if data.get("EngineVersion") != "5.8.2":
        errors.append(f"uplugin EngineVersion is {data.get('EngineVersion')!r}, expected '5.8.2'")
    names={m["Name"] for m in data.get("Modules",[])}
    expected={"VOIDWorldBuilderCore","VOIDWorldBuilderLighting","VOIDWorldBuilderImport","VOIDWorldBuilderGenerators","VOIDWorldBuilderEditor"}
    missing=expected-names
    if missing: errors.append(f"uplugin missing modules: {sorted(missing)}")
    if not any(p.get("Name")=="ProceduralMeshComponent" and p.get("Enabled") for p in data.get("Plugins",[])):
        errors.append("uplugin missing enabled ProceduralMeshComponent dependency")
except Exception as e:
    errors.append(f"uplugin parse failure: {e}")

# Required registry ids
mod=check_file("Source/VOIDWorldBuilderGenerators/Private/VOIDWorldBuilderGeneratorsModule.cpp").read_text()
for gid in ["Road","Building","District","Environment","Props","Lighting"]:
    if f'RegisterGenerator(MakeShared<' not in mod:
        pass
if any(f'TEXT("{gid}")' not in mod for gid in ["Lighting","Props","Environment","District","Building","Road"]):
    warnings.append("One or more expected generator ids is not visible in the module shutdown/registration source.")

# Brace balance for C++/h
for p in ROOT.rglob("*"):
    if p.suffix.lower() in {".cpp",".h"} and "Tools/" not in str(p.relative_to(ROOT)) :
        txt=p.read_text(errors="ignore")
        # Strip comments and string/char literals before counting braces.
        out=[]; i=0; n=len(txt); state="code"
        while i<n:
            if state=="code":
                if txt.startswith("//", i): state="line"; out.append("  "); i+=2; continue
                if txt.startswith("/*", i): state="block"; out.append("  "); i+=2; continue
                if txt[i]=='"': state="str"; out.append(" "); i+=1; continue
                if txt[i]=="'": state="char"; out.append(" "); i+=1; continue
                out.append(txt[i]); i+=1
            elif state=="line":
                if txt[i]=="\\n": state="code"; out.append("\\n")
                else: out.append(" ")
                i+=1
            elif state=="block":
                if txt.startswith("*/", i): state="code"; out.append("  "); i+=2
                else:
                    out.append("\\n" if txt[i]=="\\n" else " "); i+=1
            elif state=="str":
                if txt[i]=="\\\\": out.append("  "); i+=2
                elif txt[i]=='"': state="code"; out.append(" "); i+=1
                else: out.append("\\n" if txt[i]=="\\n" else " "); i+=1
            else:
                if txt[i]=="\\\\": out.append("  "); i+=2
                elif txt[i]=="'": state="code"; out.append(" "); i+=1
                else: out.append("\\n" if txt[i]=="\\n" else " "); i+=1
        t="".join(out)
        if t.count("{") != t.count("}"):
            errors.append(f"Brace mismatch: {p.relative_to(ROOT)}")

# Generated.h should be last include when present
for p in ROOT.rglob("*.h"):
    txt=p.read_text(errors="ignore")
    if '#include "' in txt and '.generated.h"' in txt:
        incs=[l.strip() for l in txt.splitlines() if l.strip().startswith('#include "')]
        gen=[i for i,x in enumerate(incs) if '.generated.h"' in x]
        if gen and gen[-1] != len(incs)-1:
            warnings.append(f"generated.h is not the last quoted include in {p.relative_to(ROOT)}")

print(f"Static audit root: {ROOT}")
print(f"Errors: {len(errors)}")
for x in errors: print("ERROR:",x)
print(f"Warnings: {len(warnings)}")
for x in warnings[:50]: print("WARN:",x)
sys.exit(1 if errors else 0)
