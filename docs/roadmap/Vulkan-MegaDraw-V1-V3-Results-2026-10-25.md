# Vulkan mega-draw V1-V3: results (2026-10-25, ~3:10 AM ET)

For Archie and Elfie. Direction and final verify JD. Design Archie (mega-draw briefs). Implementation Cortana.

## What landed (local, main)
- `0046ff2` V1: `vkCmdDrawIndexedIndirect` loader entry, `multiDrawIndirect` + `drawIndirectFirstInstance` enabled when offered, `LL_VK_MEGA` flag (0 default).
- `c92fb64` V2 (`LL_VK_MEGA=1`): the 80-byte per-draw record (offset|pass, pulse P0/P1, trace P0/P1) moves from push constants to a per-slot host-visible buffer, binding 1, instance rate, firstInstance = draw index. `LITHO_MEGA` variants of chunk.vert / chunk12.vert / chunk.frag (frag reads flat varyings 8..12). New pipelines `chunkMega_` / `chunkBlendMega_`.
- `d6f74a6` V3 (`LL_VK_MEGA=2`): one `vkCmdDrawIndexedIndirect` per run of same-block draws per pass. Blend pass keeps far->near order. Fallbacks: no MDI -> 1-command indirect loop; no firstInstance -> V2; pipeline/buffer failure -> old path.
- Perf log line: `vulkan mega: mode, logical chunk draws/frame, draw API calls/frame, chunk record cpu ms/frame`.

Every step gated: build-gate Release, ctest 48/48, hashes 24c52df2be14789c x3, cities on 24c52df2be14789c / off 43693d018d046b25.

## Picture (still path, r16, 1280x720, shot at 8 s)
- Old vs old (noise): 0.10-0.33% px differ, >16 levels 0.049-0.062%.
- V2 vs old: 1.8-1.9% px differ (almost all 1-2 levels), >16 levels 0.048%.
- V3 vs old: 0.52% px differ, >16 levels 0.078%.
- Diffs sit on the HUD counters and scattered time-animated spots (pulse phase at a slightly different capture time); no structural difference seen.

## Bench (flyover 60 s, RTX 3050 Ti Laptop, packed12)
| radius | mode | FPS | logical draws | API calls | GPU ms | chunk CPU record ms |
|---|---|---|---|---|---|---|
| 8 | old (0) | 702.7 | 582 | 582 | 1.17 | 0.142 |
| 8 | mega (2) | 605.7 | 582 | 55 | 1.42 | 0.137 |
| 32 | old (0) | 161.8 | 3474 | 3474 | 5.99 | 1.144 |
| 32 | mega (2) | 139.7 | 3426 | 708 | 6.87 | 0.896 |
| 48 | old (0) | 88.6 | 6722 | 6722 | 10.46 | 2.580 |
| 48 | mega (2) | 75.8 | 6544 | 1506 | 12.03 | 2.005 |

API calls drop 4.5-10x and CPU record time drops 22-23% at r32/48, but GPU time rises 15-21%. This run is GPU-bound, so FPS drops 14%.

## Read
Flag-on is **not** safe as the default yet: it is slower on this GPU. The likely cost is the 5 instance-rate vec4 attributes per vertex plus 5 flat vec4 varyings (20 extra interpolant components per fragment). Proposed V2b: pass one flat uint draw index, then read pulse/trace in the fragment shader from an SSBO indexed by it (and offset in the vertex shader from the same SSBO). V4 (face buckets) and G1 (GL43 MDI) are on hold until V2b shows a GPU-time win. IHwGpu draw-path registration was not wired up: the seat lives in the host layer and the renderer has no link to it yet.
## V2b update (~3:40 AM ET): `d3b036d`, `LL_VK_MEGA=3`
V3 indirect draws. Each record sits in a storage buffer (set 0, binding 5) and is read by `gl_InstanceIndex` (= firstInstance). Only one flat uint goes to the fragment stage, which reads pass/pulse/trace from the buffer. The default stays 0.
Profiling aids: the perf line now has chunk-pass GPU timestamps. `LL_VK_MEGA_PROBE=1` selects a variant that reads the buffer in the vertex stage and passes flat vec4 varyings. `LL_VK_FREEZE_TIME=<s>` pins the shader clocks.

**Where the GPU time went (r48 flyover, chunk-pass GPU ms):** old 10.27 | V2 direct, instance attrs 11.95 | V3 indirect 11.91 | SSBO + flat vec4 varyings 10.67 | V2b 10.37 (repeat 10.43 vs old 10.29).
- The instance-rate attributes were the real cost: about 1.3 ms.
- The 5 flat vec4 varyings cost about 0.3 ms.
- The indirect calls themselves cost nothing measurable.
- Fragment-shader buffer reads instead of push constants leave about 0.1 ms (~1%).

**Bench (flyover 60 s):**
| r | mode | FPS | API calls | GPU ms | chunk GPU ms | chunk CPU ms |
|---|---|---|---|---|---|---|
| 8 | 0 | 710.8 | 580 | 1.19 | 1.06 | 0.140 |
| 8 | 2 | 607.2 | 55 | 1.43 | 1.29 | 0.129 |
| 8 | 3 (V2b) | 676.7 | 56 | 1.23 | 1.10 | 0.111 |
| 32 | 0 | 161.8 | 3429 | 5.95 | 5.86 | 1.041 |
| 32 | 2 | 138.5 | 705 | 6.94 | 6.85 | 0.906 |
| 32 | 3 (V2b) | 160.0 | 706 | 6.01 | 5.92 | 0.879 |
| 48 | 0 | 88.9 / 88.0 | 6712 | 10.37 / 10.38 | 10.27 / 10.29 | 2.54 / 2.49 |
| 48 | 2 | 76.6 | 1510 | 12.00 | 11.91 | 2.116 |
| 48 | 3 (V2b) | 88.3 / 88.3 | 1535 | 10.47 / 10.53 | 10.37 / 10.43 | 2.07 / 2.12 |

**Frozen-clock screenshot diff (still, r16):** pixels differing by more than 16 levels: old vs old 0.044%, mode 2 0.064%, V2b 0.057%. With the clocks frozen, the earlier 1.9% "V2" spread disappears, which confirms it was animation phase. The remaining spots are the HUD text and blocks that twinkle or sparkle; old-vs-old shows the same amount.

**Decision: stopped before V4.** V2b leaves the GPU about 1% slower than mode 0, not equal, so the go-on condition is not met. Also, the chunk pass is about 99% of GPU time and it is shading-bound: cutting API calls 4-10x saves CPU (~15-20%) but no GPU. Face-direction buckets (V4) would gain by culling back-facing buckets. That win does not depend on the mega path, so it should be briefed as its own experiment. G1 is on hold. V2b is the best mega variant. Flag-on is safe to try (no picture change, equal FPS), but it is not a default upgrade on this GPU.
