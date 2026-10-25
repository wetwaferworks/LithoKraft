# Archie SoftRaster Results - 2026-10-25

## Workstation results (JD runs)
- Bottleneck: ~940k triangles/frame; time sits in per-triangle setup and the row loop.
- Striping: ~11% faster at r8, ~8% at r16; p99 ~140 ms -> ~118 ms.
- `--sw-scale 0.5`: no gain, which fits a geometry-bound renderer rather than a fill-bound one.

## Box (Linux, GCC 14.2, Xvfb) follow-up - Cortana, 2026-10-25 ET
The box is slower than the workstation (mesh queue backs up), so compare ratios, not absolute ms.

| run (r8, 1920x1080) | avg ms | p99 ms | tris/frame | scanned px | covered px | shaded px | strips ms |
|---|---|---|---|---|---|---|---|
| flyover baseline | 234.8 | 410.7 | 135.8k | 9.09M | 2.43M | 1.76M | 113.2 |
| flyover + span clip (LL_SW_P2=39) | 254.2 | 391.5 | 132.8k | 3.56M | 2.41M | 1.74M | 118.1 |
| still baseline | 24.9 | 36.6 | 42.9k | 0.50M | 45k | 22k | 23.9 |
| still + span clip | 25.5 | 46.4 | 44.5k | 0.33M | 47k | 23k | 22.9 |
| still + SIMD shade (LL_SW_SIMD_SHADE=1, P2=31) | 25.1 | 36.5 | 44.4k | 0.51M | 47k | 23k | 25.0 |

Findings:
1. The row loop walked the full bbox width: scanned px are 3.7x covered px (flyover) and ~11x (still, thin triangles).
   The span-clip prototype cuts scanned px by 61%. Frame time did not move (within box noise).
   So the empty-pixel walk is cheap. The cost is per-triangle fixed work plus shading of covered pixels.
2. On the still path: 24 ms of strips for ~43k tris and ~0.5M scanned px. That is roughly 0.5 us per triangle,
   which points at the setup itself (backface test, dither class, mip log2, SwTriSetup, the per-vertex
   emissive loop over frameEmit_, the Hi-Z tile test).
3. SIMD span shading is opt-in (LL_SW_SIMD_SHADE + P2 bit 16). It covers ~45% of spans when on and gains nothing measurable, which confirms point 2.
4. Span clip is not bit-identical. LL_SW_AB frame 60 shows 126 of 2,073,600 px differ (max delta 103),
   because edge values are re-evaluated at the clipped start instead of accumulated. It needs an eye check before any default-on.

## Proposed next steps
1. Hoist the per-vertex emissive light loop out of triangle setup. Compute it once per vertex per chunk
   (vertices are shared by 2 triangles per quad, and frameEmit_ is per frame). This should be bit-identical.
2. Per-chunk early-out: test the chunk's screen bbox against Hi-Z before transforming and setting up any of its triangles.
   That is one test instead of one per triangle/row for occluded chunks (Hi-Z already skips 67% of rows *after* setup).
3. Small-triangle cull in setup: if the bbox has no pixel centre (floor/ceil test on the snapped bbox), drop the triangle
   before SwTriSetup. This is bit-identical because such a triangle covers no sample.
4. LOD/geometry: at ~940k tris the cheapest triangle is one never emitted. Merge coplanar quads (greedy meshing)
   for far chunks on the SW path. This is a visual change, so it needs an eye check.
5. Keep span clip (bit 32) as an opt-in. To make it bit-identical, advance sw by accumulated adds to the clipped start, which keeps the
   cheap walk but drops the branch work. Only worth it if steps 1-3 make scan matter again.
6. Linux ctest: lithokraft_selftest "hot reload diff" fails on the box only (likely mtime granularity). This needs a separate look.

## Steps 1 and 3 prototyped (box, 2026-10-25 ~1:30 PM ET)
Both are on by default (LL_SW_P2 default now 0xC7). LL_SW_P2=7 restores the earlier path.
- Step 1 (bit 64): reuse each vertex's emissive sum across a quad's two triangles (same ops, same bits).
  Emitters are off by default (render.emissive_light), so LL_SW_EMIT=1 is a profiling override that leaves settings untouched.
  It saved ~133k vertex light sums/frame on flyover, but the frame-time effect is in the noise: 200.8 -> 196.8 ms avg.
- Step 3 (bit 128): drop triangles whose bbox contains no pixel centre before setup. This removes ~35% of triangles
  (flyover ~450k -> ~295k; still 44.5k -> 28.3k).
- A/B: LL_SW_AB showed 0 diff px in every check (step 1 alone with emitters lit; step 3 alone; both at flyover f30 and f60, still f200).
  Hashes: 24c52df2be14789c x3, cities on 24c52df2be14789c, cities off 43693d018d046b25.

| run | P2=7 avg / p99 ms | default (0xC7) avg / p99 ms |
|---|---|---|
| flyover run 1 | 207.3 / 303.0 | 195.4 / 291.7 |
| flyover run 2 | (232.6 / 425.4, streaming outlier) | 194.9 / 283.6 |
| still | 30.8 / 47.9 | 24.0 / 33.5 |
| flyover, emitters forced on | 200.8 / 277.1 | 196.8 / 285.0 |

Flyover is about 5-6% faster. Still is about 20% faster on avg and p99, but the box is noisy, so this needs confirming on the workstation.
