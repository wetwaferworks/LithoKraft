#!/usr/bin/env python3
"""Mechanical GLSL 330 -> GLSL ES 3.00 translation for the WASM pack.

Copies engine/assets into wasm/stage/engine-assets with the top-level
shaders/ files translated:
  - "#version 330 core" -> "#version 300 es"
  - fragment shaders gain default precision statements after the version
    line (ES fragment shaders have no default float precision, and ES 3.00
    gives no default precision to sampler2DArray/sampler3D either — ANGLE
    rejects an unqualified `uniform sampler2DArray`; ES 3.00 vertex
    shaders default to highp, so vertex files need nothing)
  - .glsl include files carry no #version and pass through byte-identical
Everything else is byte-identical. Every transform and every hazard token
found is written to wasm/stage/SHADER-TRANSLATION-LOG.md. The native tree
is never modified — this staging exists only to be --preload-file'd.

Known non-goal: chunk_pbr.frag uses `uniform samplerBuffer` (buffer
textures are GLSL ES 3.10, not 3.00). It is translated like the rest and
its compile verdict is left to the runtime log — no silent redesign.
"""
import re, shutil, sys
from pathlib import Path

TREE = Path(__file__).resolve().parents[1]
SRC = TREE / "engine" / "assets"
DST = TREE / "wasm" / "stage" / "engine-assets"
LOG = TREE / "wasm" / "stage" / "SHADER-TRANSLATION-LOG.md"

HAZARDS = ["texture2D", "gl_FragColor", "textureGather", "samplerBuffer",
           "gl_FragDepth", "attribute ", "varying "]

def main() -> int:
    if DST.exists():
        shutil.rmtree(DST)
    shutil.copytree(SRC, DST)
    lines = ["# Shader translation log (GLSL 330 -> ES 3.00, mechanical)",
             "", f"source: {SRC}", f"staging: {DST}", ""]
    shaders = sorted((DST / "shaders").glob("*"))
    for f in shaders:
        if f.suffix not in (".vert", ".frag", ".glsl"):
            continue
        text = f.read_text()
        notes = []
        if "#version 330 core" in text:
            text = text.replace("#version 330 core", "#version 300 es", 1)
            notes.append("version line -> `#version 300 es`")
            if f.suffix == ".frag":
                text = text.replace(
                    "#version 300 es\n",
                    "#version 300 es\nprecision highp float;\nprecision highp int;\n"
                    "precision highp sampler2DArray;\nprecision highp sampler3D;\n", 1)
                notes.append("fragment: + `precision highp float;` + `precision highp int;`"
                             " + `precision highp sampler2DArray;` + `precision highp sampler3D;`")
        elif f.suffix in (".vert", ".frag"):
            notes.append("WARNING: no `#version 330 core` line found — left untouched")
        for hz in HAZARDS:
            for i, ln in enumerate(text.splitlines(), 1):
                if hz in ln and not ln.strip().startswith("//"):
                    notes.append(f"hazard `{hz}` at line {i}: {ln.strip()[:90]}")
        f.write_text(text)
        lines.append(f"## {f.name}")
        lines += [f"- {n}" for n in notes] or ["- byte-identical copy"]
        lines.append("")
    LOG.write_text("\n".join(lines))
    print(f"staged {DST} ({sum(1 for _ in DST.rglob('*') if _.is_file())} files); log: {LOG}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
