# Report for Archie: Phase 4 A-D, r32 vs r64 (2026-10-25, ~7:55 AM ET)

Direction and final verify JD. LithoPipeline designed by Archie, implemented by Cortana. LithoPipeline inspired by id Software's Doom 2016 (id Tech 6) and the MegaTexture work that led to it.

Commit with the code: `62bc967` (local). Full write-up: `Phase4-Results-2026-10-25.md`, section "Phase 4 A-D". Your 07:10 reply: `Archie-Phase4-Reply-2026-10-25-0710-EDT.md`.

## Context

- Rig: RTX 3050 Ti Laptop GPU, 4 GB (minimum spec / reference rig). Every run used the discrete GPU.
- All benches: borderless 1920x1080, `LL_VK_MEGA=3` + 4.2a (`LL_VK_FASTCULL=1`), circuits scene, sprint path, 30 s.
- The pull path is opt-in (`LL_VK_PULL=1`). The default mode-3 path is unchanged.
- The gate is green on `62bc967`: ctest 48/48, `24c52df2be14789c` x3, cities on `24c52df2be14789c`, cities off `43693d018d046b25`.
- JD will run his own baselines. The numbers below are ours, from single 30 s runs (r64 repeated).

## B: blend split (your Q1)

Quads of the alpha-blend shade mode stay on Packed12 (the blend pass draws only the fallback range). The rest of a blended mesh is now pulled. Deviation: the blend faces themselves are not pulled. `LL_VK_BLENDSPLIT=0` opts out.

**JD decision: the blend split stays on by default with the pull path.**

### Pull coverage (share of quads pulled)

| Radius | Split off | Split on |
|---|---|---|
| r32 sprint | 22.7% | 56.6% |
| r64 sprint | 9.9% | 28.2% |

### FPS

| Radius | Split off | Split on |
|---|---|---|
| r32 sprint | 117.8 | 138.0 / 146.8 (+17-25%) |
| r64 sprint | 61.1 / 64.4 | 42.8 / 43.6 / 39.5 (about -33%, reproduced) |

### Last-sample log lines (one 120-frame window each, end of run)

| Run | GPU ms avg | Arena used | Worker-published/frame | Uploads/frame | Evictions/frame | Remeshes/frame | Chunks tested/frame | Blend draws/frame | CPU record (cull+sort+opaque+blend ms) |
|---|---|---|---|---|---|---|---|---|---|
| r64 off | 11.52 | 1860 MB of 1920 (96.9%) | 74.1 | 0.00 | 16.2 | 0.00 | 53,517 | 5,320 | 3.09 + 1.70 + 0.78 + 0.79 |
| r64 on | 8.18 | 1095 MB of 1152 (95.1%) | 110.1 | 13.68 (0.59 MB) | 26.7 | 0.75 | 42,054 | 4,760 | 3.01 + 1.86 + 0.85 + 0.94 |
| r32 off | 8.40 | 686 MB of 736 | 8.1 | 0.00 | 7.2 | 0.00 | 16,961 | 2,026 | 0.86 + 0.50 + 0.21 + 0.23 |
| r32 on | 6.41 | 425 MB of 512 | 6.1 | 0.00 | 5.5 | 0.00 | 16,607 | 2,020 | 0.87 + 0.53 + 0.22 + 0.24 |

What the data shows:
- The split makes the GPU cheaper at both radii: about -2 ms at r32 and -3.3 ms at r64.
- The split cuts arena use by about 38-41%.
- At r64 the FPS drop is therefore not GPU time.

### Hypotheses for the r64 regression (all UNVERIFIED)

1. **Streaming churn / CPU-side mesh build cost (most likely, given the data above).**
   - At r64 with the split on, the snapshot still shows streaming activity: 110 vs 74 worker publishes/frame, 13.7 vs 0 uploads/frame, 26.7 vs 16.2 evictions/frame, nonzero remeshes, and 42k vs 54k chunks tested.
   - With the split, `buildPulled` now runs (encode + bit-identical decode check, up to 81 UV-code tries per quad) for meshes that used to skip it entirely.
   - If that runs on the thread that feeds uploads, or slows workers, r64 never settles. r32 settles because it has far fewer chunks.
   - Not yet profiled. We don't know which thread runs `allocateMesh` / `buildPulled` under load.
2. **Arena reserve sizing / eviction feedback.**
   - The reserve is smaller with the split on (1152 vs 1920 MB) and is still 95% full.
   - If the reserve is sized from pulled-byte estimates, it may be too tight at r64 and cause evict/re-upload cycles.
   - Unverified: we have one snapshot only.
3. **Extra ranges/draws per blended mesh.**
   - A split mesh now has a pulled range (opaque) plus a Packed12 fallback range (blend + rejected quads), so opaque work can be two draws per mesh.
   - Opaque put+draw CPU time rose only slightly (0.78 -> 0.85 ms) and blend draws fell (5,320 -> 4,760), so this is probably small. Not measured as a draw count.
4. **Packed12 blend fallback cost.**
   - The blend faces stay in Packed12 and are drawn from the fallback range. Blend sort+put+draw rose slightly (0.79 -> 0.94 ms).
   - This looks minor on its own, but it compounds with 3.
5. **Sampling caveat.**
   - The table is the last 120-frame window, not the whole-run average.
   - FPS is the whole-run average, so warm-up / initial load differences at r64 could be a large part of the gap. JD's baselines should settle this.

## C: sub-cell quads (your Q3)

Census only (`LL_VK_SUBCELL_CENSUS=1`); no packing implemented. Sub-cell is now the largest rejection reason with the split on: 16.8 M quads at r32 (about 32% of quads) and 33.2 M at r64 (about 49%).

| | r32 | r64 |
|---|---|---|
| Unique tuples (face, normal-axis nibble, a/b min/max nibbles) | 6,660 | 7,158 |
| Top-16 coverage | 4.5% | 4.7% |
| Top-64 coverage | 13.3% | 12.4% |
| Top-256 coverage | 33.8% | 27.9% |
| Tuples needed for 90% | 2,987 | 3,905 |

The distribution is flat, so neither a scale flag (half/quarter cell) nor a small LUT reaches the 90% target.

Open question: a wider quad format (for example 1/16-unit origin and extents) does not fit in the current 64-bit quad plus material entry.

## D: fragment-bound test

| Case | r32 GPU ms | r48 GPU ms |
|---|---|---|
| Borderless, normal shader | 1.33 | 5.28 |
| Borderless, flat fragment stub (`LL_VK_FLATFRAG=1`) | 1.02 (-23%) | 3.98 (-25%) |
| Windowed 1920x1080 | 1.35 | 5.09 |
| Windowed 960x540 (1/4 pixels) | 1.21 (-10%) | 4.89 (-4%) |

Still camera. The r48 runs disturbed by a window move were rerun.

Conclusion:
- The GPU is not ROP/fill-bound.
- About 25% of GPU time is fragment-shader cost that does not scale with resolution (likely small-triangle quad shading or fixed FX cost).
- About 75% is before raster.

## A: the 3p upload/eviction "mystery" (your Q8)

It is mostly a per-frame normalization artifact. At r64 sprint, mode 3 vs 3p:

| Metric | Mode 3 | Mode 3p | Difference |
|---|---|---|---|
| Uploads per second | 722 | 807 | +12% |
| Evictions per second | 414 | 432 | +4% |

Per frame the gap looked like +52% / +42%, but the FPS ratio (92.3 / 67.6 = 1.37) explains most of it. From here on we report per-second rates. The real 3p FPS cost of the pull path remains.

## Questions for Archie

1. **r64 regression:** do you agree it looks CPU/streaming-side rather than GPU (GPU ms fell 11.5 -> 8.2 while FPS fell)? Which counter would you add first: per-thread `buildPulled` time, or arena reserve/evict cycles?
2. **Encode cost:** should `buildPulled` get cheaper first (for example derive the UV sign codes directly instead of trying up to 81), or move off the critical thread?
3. **Sub-cell:** with about 7k flat-distributed tuples, do you want a second quad format (wider, maybe 96/128-bit for sub-cell quads only), or should sub-cell geometry stay on Packed12 for good?
4. **Fragment cost:** given D (about 25% fragment cost that doesn't scale with resolution), is it worth trimming the FX fragment path or a distance LOD for small triangles before more pull work?
5. **Pulling blend faces:** is it worth pulling the blend faces too (sorted blend pass from the pulled format), or is Packed12 for blend final?
