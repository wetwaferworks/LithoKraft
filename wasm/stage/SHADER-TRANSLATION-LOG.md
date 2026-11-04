# Shader translation log (GLSL 330 -> ES 3.00, mechanical)

source: /home/hatch/workspace/lk/engine/assets
staging: /home/hatch/workspace/lk/wasm/stage/engine-assets

## chunk.frag
- version line -> `#version 300 es`
- fragment: + `precision highp float;` + `precision highp int;` + `precision highp sampler2DArray;` + `precision highp sampler3D;`

## chunk.vert
- version line -> `#version 300 es`

## chunk_pbr.frag
- version line -> `#version 300 es`
- fragment: + `precision highp float;` + `precision highp int;` + `precision highp sampler2DArray;` + `precision highp sampler3D;`
- hazard `samplerBuffer` at line 36: uniform samplerBuffer uPbrMaterials;  // 12 R32F texels per material (48-byte rows)

## chunk_pbr.vert
- version line -> `#version 300 es`

## chunk_shading.glsl
- byte-identical copy

## hud.frag
- version line -> `#version 300 es`
- fragment: + `precision highp float;` + `precision highp int;` + `precision highp sampler2DArray;` + `precision highp sampler3D;`

## hud.vert
- version line -> `#version 300 es`

## line.frag
- version line -> `#version 300 es`
- fragment: + `precision highp float;` + `precision highp int;` + `precision highp sampler2DArray;` + `precision highp sampler3D;`

## line.vert
- version line -> `#version 300 es`

## pbr_shading.glsl
- byte-identical copy

## sprite.frag
- version line -> `#version 300 es`
- fragment: + `precision highp float;` + `precision highp int;` + `precision highp sampler2DArray;` + `precision highp sampler3D;`

## sprite.vert
- version line -> `#version 300 es`
