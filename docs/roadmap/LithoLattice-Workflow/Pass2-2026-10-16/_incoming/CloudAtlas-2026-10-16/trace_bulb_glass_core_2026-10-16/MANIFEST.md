# Trace Bulb Glass Core, 2026-10-16 (CloudAtlas)

A glass sphere core that sits inside the existing Red/Blue Trace Bulb shells (`lithokraft:trace_tip_red` / `lithokraft:trace_tip_blue`). The outer shell art in `../trace_bulbs_2026-10-16/` is unchanged.

## Format
- Models: `.lthm` v1, written byte for byte per `ll-review/game/LithoKraft/src/content/model/FORMAT.md` ("LTHM", u16 version 1, flags 0, name, author, maxLevel, leafCount, 5-byte leaves sorted by level, then z, y, x, CRC32). Uniform 2x2x2 groups are merged into parent leaves, octree style. CRC is checked after writing. Models are not loaded by the engine yet (read-only tree, no build).
- Geometry is in the 64^3 block volume. The sphere centre is (32,32,32) and its radius is 12 micro-cells, matching the shell lens at the centre of the 64px texture. The shell radius is 20, so the core fits inside it.
  - Glass rim: a 2-cell skin.
  - Clear interior: empty.
  - Solid glowing core: radius 5.
  - Trace: an equator band, one meridian and a four-dot motif. The equator band and meridian are **raised ridges**, standing 1.5 cells proud of the glass (silhouette detail).
- Low-spec variants (level 5, 32^3) are `*-lowspec.lthm`.

| model | leaves |
|---|---|
| red/blue glass-core.lthm (L6) | 3584 each |
| red/blue glass-core-lowspec.lthm (L5) | 552 each |

## Proposed palette ids (open question, Cortana)
40 GlassRimRed, 41 BulbCoreRed, 42 GlassRimBlue, 43 BulbCoreBlue. These ids are not used in `models.h` (taken: 1-13, 32, 33). The Trace reuses 5 TraceRed and 6 TraceBlue. The rim needs alpha blending (alpha about 90/255), and the core and trace need emissive shading.

## Textures (pack_128 master, plus 64/32/16 versions)
- `blocks/<col>_trace_bulb_glass_core.png`: front-view RGBA texture in the shell's UV frame, with the glass rim translucent. Red is (235,20,30), a true red. Blue is (20,170,240), leaning cyan.
- `masks/<col>_trace_bulb_glass_core_normal.png` (also listed below)
- `masks/<col>_trace_bulb_glass_core_glow_mask.png`: R = strength, G = radial phase, B = 0, A = 0.
- `masks/<col>_trace_bulb_glass_core_normal.png`: tangent-space normal map, flat = (128,128,255). It encodes the sphere dome plus the raised Trace ridges.
- `previews/trace_bulbs_glass_core_before_after.png`: red before and after, then blue before and after. The core is drawn over the old opaque lens, inside the ghost shell.

## Linking to the outer texture
The .lthm format has no texture reference; materials are only uint8 indices. So the model **cannot** link to the shell's 2D texture inside the file. `binding_proposal.json` is a sidecar proposal: block id -> core model + outer shell texture + core texture. Either the appearance code or the block registry would have to honour it. It does nothing until Cortana wires it in.

## Open questions for Cortana
1. Approve palette ids 40-43, or assign others.
2. Render path: draw the shell as the existing 2D cutout/blend, with the .lthm core placed at the block centre? Or drop the old opaque lens disc so the core shows through?
3. Where the binding sidecar should live, if anywhere.

Rebuild: `python3 build.py`.
