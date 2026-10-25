# Report for Elfie: Vulkan mega-draw, 2026-10-25

Direction and final verify JD. Design Archie (mega-draw and Mega-Pipeline briefs). Implementation Cortana.

Elfie, this is the overnight Vulkan mega-draw work: what was built, what it measured, and what I think it means for your web and GL Phase 2 leg. I didn't touch your web mega code, `wasm/*` or `lk_webio.js`. All work is local commits on main; there is no remote. Times are ET.

## What was built

Every step sits behind the `LL_VK_MEGA` environment flag, keeps the old path selectable, and passed the full gate on its own commit. The gate is: build-gate Release, ctest 48/48, scenario hashes 24c52df2be14789c three times, and the cities hashes, on 24c52df2be14789c and off 43693d018d046b25. Each rollback line returns the tree to the commit before that step.

- **V1, `0046ff2`.** Adds the `vkCmdDrawIndexedIndirect` entry point, enables the multiDrawIndirect and drawIndirectFirstInstance device features when the GPU offers them, and adds the flag. No behaviour change. Rollback: `git reset --hard 1954f22`.
- **V2, `c92fb64`, mode 1.** The 80-byte per-draw record (sub-chunk offset and pass, pulse P0/P1, trace P0/P1) moves out of push constants into a per-frame buffer, read as instance-rate vertex attributes and passed to the fragment shader as flat varyings. Rollback: `git reset --hard 0046ff2`.
- **V3, `d6f74a6`, mode 2.** Draws on the same arena block go out as one `vkCmdDrawIndexedIndirect` per pass. The translucent pass keeps its far-to-near order, because only consecutive draws on the same block are grouped. Rollback: `git reset --hard c92fb64`.
- **V2b, `d3b036d`, mode 3.** Same indirect submission, but the records live in a storage buffer (set 0, binding 5). The vertex shader finds its record through `gl_InstanceIndex`, which carries the draw index in firstInstance. Only a single flat uint reaches the fragment shader, which reads pass, pulse and trace from the buffer. This commit also adds three test aids:
  - GPU timestamps around the chunk pass.
  - `LL_VK_MEGA_PROBE=1`, a profiling-only variant.
  - `LL_VK_FREEZE_TIME=<seconds>`, which pins the shader clocks so screenshots can be compared.

  Rollback: `git reset --hard 70b02ca`.
- **Default flip (JD's call, 3:41 AM).** Mode 3 becomes the default; `LL_VK_MEGA=0` still selects the old path. The fallbacks stay: no firstInstance support drops to direct draws, and missing pipelines or buffers drop to the old path. The same commit adds `LL_BENCH_FRAMES=<file>`, which dumps each benchmark frame time for the smoothness numbers. The commit hash and rollback line will be filled in once it is gated.

## Where the GPU time goes

Measured on an RTX 3050 Ti Laptop GPU (Vulkan 1.4, packed12 vertices), flyover at stream radius 48. "Chunk GPU ms" is the time between the timestamps around the chunk pass, which is about 99% of the frame.

| Path | Chunk GPU ms |
|---|---|
| Old path: push constants, one draw call per sub-chunk | 10.27 (repeat 10.29) |
| V2: per-draw data as instance attributes, direct draws | 11.95 |
| V3: same data, indirect draws | 11.91 |
| Probe: storage buffer read in the vertex shader, five flat vec4 varyings | 10.67 |
| V2b: storage buffer, one flat index | 10.37 (repeat 10.43) |

Reading the differences:
- Indirect submission cost nothing measurable.
- The instance-rate attributes were the expensive part, about 1.3 ms.
- Passing five vec4s to every pixel cost another 0.3 ms.
- V2b's remaining gap to the old path is about 0.1 ms, roughly 1%. It comes from the fragment shader reading a storage buffer where it used to read push constants.

## Bench: average FPS (flyover, 60 s)

| Radius | Mode | FPS | Draw calls | GPU ms | CPU record ms |
|---|---|---|---|---|---|
| 8 | 0 old | 710.8 | 580 | 1.19 | 0.140 |
| 8 | 2 V3 | 607.2 | 55 | 1.43 | 0.129 |
| 8 | 3 V2b | 676.7 | 56 | 1.23 | 0.111 |
| 32 | 0 old | 161.8 | 3429 | 5.95 | 1.041 |
| 32 | 2 V3 | 138.5 | 705 | 6.94 | 0.906 |
| 32 | 3 V2b | 160.0 | 706 | 6.01 | 0.879 |
| 48 | 0 old | 88.9 / 88.0 | 6712 | 10.37 / 10.38 | 2.54 / 2.49 |
| 48 | 2 V3 | 76.6 | 1510 | 12.00 | 2.12 |
| 48 | 3 V2b | 88.3 / 88.3 | 1535 | 10.47 / 10.53 | 2.07 / 2.12 |

Smoothness numbers (frame-time p50, p99, p99.9, max, hitches, standard deviation) will be added below once they are measured.

## Picture check

Still camera, stream radius 16, 1280x720, captured at 8 s with `LL_VK_FREEZE_TIME=100`. The figure is the share of pixels that differ by more than 16 levels from an old-path capture.

- Old path against another old-path run: 0.044%. This is the noise floor.
- V3: 0.064%.
- V2b: 0.057%.

The remaining spots are the HUD numbers and blocks that twinkle or sparkle; two old-path runs differ in the same places. An earlier, unfrozen comparison showed a 1.9% low-level spread for V2. Freezing the clocks removed it, which confirms it was animation phase, not a picture change.

## The main finding

On this GPU, chunk rendering is limited by shading, not by draw calls. Cutting draw calls four to ten times saved 15-20% of the CPU time spent recording the chunk pass, but no GPU time, and the GPU sets the frame rate here. Mega-draw on desktop Vulkan can match the old path but not beat it on GPU time. The payoff on this machine is lower CPU cost and headroom for weaker CPUs or more streamed chunks, not more FPS. The GPU wins have to come from doing less vertex and fragment work: culling back-facing face buckets (Mega-Pipeline Phase 3) and slimmer vertices (Phase 2).

## What it means for your web and GL Phase 2 leg

On WebGL and GL33, each draw call costs much more, because of the browser's validation layer, ANGLE translation and the GL driver. Your `WEBGL_multi_draw` instanced mega-call is likely to pay off where the Vulkan version did not, so I would keep going with it. One caution from the profile: on this GPU, feeding per-draw data as instance-rate attributes (your attributes 4-8 under LK_MEGADRAW) cost about 12% GPU time against push constants. WebGL2 has no storage buffers, so the V2b fix isn't available to you. Two cheaper options:
- Send only a draw index, and fetch the pulse and trace values from a small texture with `texelFetch`.
- Pack the values into fewer components.

Either should keep the fragment inputs down. Please check GPU time on your side, not just draw counts, before calling it a win.

## Open questions for Elfie and Archie

1. Is the 12% instance-attribute cost also visible on the web path, or does the per-call saving outweigh it there?
2. Mega-Pipeline Phase 2 (64-bit packed quads with vertex pulling) assumes full-cell, axis-aligned quads with 6-bit coordinates. Our mesher also emits sub-block positions (1/16 units), rotated faces, foliage LOD tags and per-corner UVs. Should non-cube geometry stay on today's vertex format as a second stream?
3. The 6-bucket normal split needs the mesher to sort each sub-chunk's quads by face direction. How does that interact with your decideOnce culling on the web path?
4. Should the smoothness numbers (p99.9, hitches) become part of the standard bench JSON, so all paths report the same metrics?

Phase 2 and Phase 3 results will be added to this report as they land.