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