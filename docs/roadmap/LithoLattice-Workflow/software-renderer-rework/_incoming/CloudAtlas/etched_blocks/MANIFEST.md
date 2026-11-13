# Etched block samples, 2026-10-16 (CloudAtlas)

Source textures are read-only, from `../refresh128/wafer_2026-10-15/pack_64/textures/blocks/`: `wafer_ground_trace.png`, `red_trace_cross.png` and `glass.png`. Mere Glass has no file of its own, so plain `glass` stands in for it. Nothing existing was overwritten.

## Method
- Line mask: each texel's luminance is compared with the texture median, in standard-deviation units. Below -1 sd counts as a dark line and becomes a **groove**. Above +1 sd counts as a bright line and becomes a **ridge**.
- Normal maps: tangent space, flat = (128,128,255), tiling, depth 2. Output is `pack_{64,32,16}/textures/masks/<block>_etched_normal.png`.
- Models: `.lthm` v1, using the same FORMAT.md writer as the bulb set, with merged octree leaves. They show top-face relief only: a solid body, a surface layer that is absent over grooves (with the groove floor in line material), and a ridge layer one cell up.
  - Normal versions are L4 (16^3).
  - `-lowspec` versions are L3 (8^3).
  - Proposed material ids: 50 = base, 51 = line.
- Previews: `previews/<block>_before_after_lit.png`. Left is the flat normal, right is the etched normal. Both use the same Lambert light from the upper left.

| model | leaves |
|---|---|
| wafer-ground-trace-etched (L4 / L3) | 109 / 28 |
| red-trace-cross-etched (L4 / L3) | 348 / 132 |
| glass-etched (L4 / L3) | 365 / 133 |

## Notes and open questions
- Transparent texels in glass and trace_cross show as black in the previews. Their models treat those texels as base material.
- Materials 50/51 need a colour or texture binding. As with the bulbs, .lthm cannot reference a texture.
- The models only relieve the top face. Side faces would need the same treatment if wanted.
- No tessellation is needed. Grooves are either real voxels or normal-map only.

Rebuild: `python3 build.py` (imports the writer from `../trace_bulb_glass_core_2026-10-16/build.py`).
