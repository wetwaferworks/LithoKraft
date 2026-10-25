# LithoPipeline Phase 4: results (2026-10-25, ~6:30 AM ET)

Direction and final verify JD. LithoPipeline (halves LithoVoxel and LithoTexture) designed by Archie, implemented by Cortana; Phase 4 research compiled by Elfie (`Research-Phase4-Streaming-Culling-Elfie-2026-10-25-0520-EDT.md`). LithoPipeline inspired by id Software's Doom 2016 (id Tech 6) and the MegaTexture work that led to it.

## What landed

Commit `2f6fe56`: Phase 4.0, instrumentation only. Zero behavior change. Rollback: `git reset --hard 861b7ac`.

- `LL_STREAM_COUNTERS` (CMake `LITHO_STREAM_COUNTERS`, default ON; a shipping build can turn it OFF). It adds a `vulkan stream:` log line every 120 frames with:
  - uploads and worker publishes per frame, and upload MB per frame;
  - evictions vs remeshes;
  - chunks tested vs frustum-culled. Occlusion-culled is always 0 because there is no occlusion pass;
  - arena MB used vs reserved.
- The quad-rejection counter for Archie: a `vulkan pull reasons` line, active with `LL_VK_PULL=1`.
- A `bench lows:` line with:
  - 1% and 0.1% low FPS, computed as 1000 / mean of the slowest 1% or 0.1% of frames;
  - the worst 10 frames with their time after warmup;
  - mesh-queue depth avg / p99 / max.
- New bench paths:
  - `whip` (T1): a 180° pan in 0.12 s every 1.5 s, held 20 blocks over the spawn;
  - `sprint` (T2/T3): +x at 60 blocks/s.
- `vk_renderer.cpp` line endings are back to CRLF.
- Gate: build rc=0, ctest 48/48, hash `24c52df2be14789c` x3, cities on `24c52df2be14789c`, cities off `43693d018d046b25`.
- Frozen picture, mode 3 r16 vs the Phase 2 base: 0.399% of pixels differ by more than 16 levels, which is within the 0.47% noise floor.

## Raymarching

None. No render path raymarches. DDA appears only in AI line of sight, audio occlusion and picking.

## T1 whip pan (borderless, 30 s)

| r | mode | FPS | 1% low | 0.1% low | cold 1% | warm 1% | GPU ms | uploads/f | tested | frustum-culled |
|---|---|---|---|---|---|---|---|---|---|---|
| 16 | 3 | 410.5 | 285.7 | 226.9 | 276.2 | 293.0 | 2.41 | 0.16 | 2155 | 1408 (65%) |
| 16 | 3p | 402.7 | 275.0 | 188.1 | 269.9 | 289.1 | 2.45 | 0.17 | 2158 | 1411 |
| 32 | 3 | 168.3 | 117.6 | 81.0 | 103.5 | 131.3 | 5.57 | 1.30 | 6744 | 4543 (67%) |
| 32 | 3p | 168.0 | 110.1 | 65.4 | 91.1 | 128.8 | 5.69 | 1.39 | 6922 | 4660 |

- The mesh queue stayed at 0 (max 1) and there were no evictions.
- The worst frames (10–17 ms) fall in the first ~0.8 s and at isolated pans. "Cold" here means first-visit costs during the first 10 s.
- Pull (3p) is slightly worse on the 0.1% low at r32 (65 vs 81).

## Quad-rejection tally for Archie (whip, LL_VK_PULL=1, cumulative quads)

| Outcome | Quads | Share |
|---|---|---|
| ok (pulled) | 851,501 | ~8% |
| blend mesh (whole mesh skipped) | 6,815,252 | ~67% |
| sub-cell | 2,477,891 | ~24% |
| uv-or-bit mismatch | 8,752 | <0.1% |
| no-origin | 5,831 | <0.1% |
| bad-face, out-of-range, degenerate, distance-overflow, block ≥ 64 | 0 | 0 |

The 8% coverage comes mostly from blend meshes. Any sub-chunk containing a blended quad stays wholly on the old path. The next lever is to split blend quads into their own stream so the rest of the mesh can pull, then to handle 1/16 sub-cell positions.

## T2 / T3 sprint: blocked by an existing issue

The bench streaming harness does not stream ahead of a fast camera. On both the existing `stream` path and the new `sprint` path:

- generation stops after the first ~830 columns, with only ~33 columns later on;
- the camera flies out over empty space, so every chunk tested is frustum-culled and FPS reads ~1,490.

This predates Phase 4; the old `stream` path shows the same thing. The sprint numbers are therefore not valid, and T3 (r32/64/128) could not run. The harness is game-side streaming, not rendering, so I left it alone. Fixing it needs its own approval.

## Decision: 4.1 vs 4.2

The baselines convict neither, so neither was implemented.

- **4.1 streaming governor:** the mesh queue never backs up (depth 0) and uploads are ≤1.4 per frame. The game already budgets uploads at 64 sub-chunks and 3 MB per frame. There is nothing to govern until a real streaming load can be measured, which is T2.
- **4.2 GPU frustum culling into indirect args:** CPU frustum culling already rejects 65–67% of chunks. CPU chunk record time is ~0.2 ms per frame, and the GPU is shading-bound (2.4–5.7 ms). Moving culling to the GPU would save roughly 0.2 ms of CPU time and no GPU time. HZB/occlusion has no measured gap that would justify it.

## Recommendation

1. Fix the bench streaming harness (a game-side change that needs JD's approval), then rerun T2/T3. That is the only test that could convict 4.1.
2. Biggest renderer lever: blend-quad split for LithoVoxel pulling (67% of quads), then sub-cell support. Coverage would rise from 8% to potentially ~90%. It is worth asking Archie whether that is the next phase.
3. Shading cost, not draw submission, sets the frame rate on this GPU.
## Update ~6:50 AM ET: T2/T3 run after JD's 6:30 approval

- e2a84e1: the bench streamer follows the bench camera. The cause was that update() reset player_.pos from the locomotion body before the streamer read it. Rollback: git reset --hard b352684.
- T2/T3 sprint: r32 mode 3 at 115 FPS (1% low 69), r64 mode 3 at 92 FPS (1% low 25; warm 74 FPS), r64 3p at 68 FPS. Peak RAM 3.4 GB at r64. r128 was not run because the CLI caps --stream-radius at 64. Full table: Report-for-Archie-Phase4-Questions-2026-10-25.md.
- This run convicted the CPU frustum loop, which takes 5.6-7.3 ms per frame at r64 (about 25k hash-map nodes). That is the 4.2 family, so I implemented 585db08, the Phase 4.2a compact cull list (LL_VK_FASTCULL=1, default off). Same decisions; r64 FPS went from 84.5 to 104.6 and cull from 3.0 to 1.7 ms. Rollback: git reset --hard e2a84e1.
- The 1% lows did not move (40-55 ms streaming spikes). 4.1, the governor, is next but unproven; it needs per-spike attribution first.
- Direction and final verify JD. LithoPipeline designed by Archie, implemented by Cortana; Phase 4 research compiled by Elfie.

## Reference rig / minimum spec (JD decision, 7:05 AM ET)

JD decided that the 4 GB NVIDIA GeForce RTX 3050 Ti Laptop GPU (driver 621.01, Vulkan 1.4) is LithoKraft's minimum spec and reference rig. Every Phase 4 number in this doc was measured on that GPU; the logs confirm the selected device was 'vulkan: using NVIDIA GeForce RTX 3050 Ti Laptop GPU'. That makes the 4 GB VRAM budget a hard limit: at r64 the arena used 2.1 GB, which has to fit with room to spare.

- Direction and final verify JD. LithoPipeline designed by Archie, implemented by Cortana; Phase 4 research compiled by Elfie.

## Phase 4 A-D (2026-10-25, ~7:15-7:55 AM ET; Archie's 07:10 reply: `Archie-Phase4-Reply-2026-10-25-0710-EDT.md`)

Direction and final verify JD. LithoPipeline designed by Archie (incl. guidance in her 2026-10-25 07:10 reply), implemented by Cortana. All benches borderless, mode 3 + 4.2a, circuits scene, RTX 3050 Ti Laptop GPU (minimum spec).

| Item | Result | Code |
|---|---|---|
| A: 3p upload/eviction "mystery" (Q8) | Mostly a per-frame normalization artifact. Per second at r64 sprint: uploads 722 vs 807 (+12%), evictions 414 vs 432 (+4%); per frame it looked like +52% / +42%, but the FPS ratio (92.3/67.6 = 1.37) explains most of it. Report per-second rates. The real 3p FPS cost (pull path) remains. | none |
| B: blend split (Q1) | Pull coverage up: r32 sprint 22.7% -> 56.6% of quads, r64 sprint 9.9% -> 28.2%. FPS: r32 117.8 -> 138.0/146.8 (+17-25%); r64 61.1/64.4 -> 42.8/43.6/39.5 (about -33%, repeated). Default on under `LL_VK_PULL=1`; `LL_VK_BLENDSPLIT=0` opts out. Deviation: the blend faces stay Packed12 (not pulled). | packed_quad64.h, vk_renderer.cpp |
| C: sub-cell (Q3) | Tuple census only (`LL_VK_SUBCELL_CENSUS=1`): sub-cell is now the biggest rejection (r32 16.8 M / r64 33.2 M quads). Unique tuples 6,660 (r32) / 7,158 (r64); top 256 cover only 33.8% / 27.9%; 90% needs 2,987 / 3,905 tuples. Neither a scale flag nor a small LUT reaches 90%. **Packing not implemented**: it needs a wider quad format. Decision is Archie's. | packed_quad64.h, vk_renderer.cpp (census only) |
| D: fragment-bound test | Flat stub (`LL_VK_FLATFRAG=1`) cuts GPU ms 25% at r48 (5.28 -> 3.98) and 23% at r32. A quarter of the pixels (960x540 windowed) cuts only 4% at r48 (5.09 -> 4.89) and 10% at r32. Not ROP/fill-bound. About 25% of GPU time is fragment-shader cost that does not scale with resolution (likely small-triangle quad shading or fixed FX cost); about 75% is pre-raster. r48 runs during JD's window move (7:30-7:33 ET) were rerun. | chunk.frag, render_vk CMakeLists.txt, vk_pipelines.cpp |

Raw data: `rename-run\mega\fragD.txt`, `fragD_r48rerun.txt`, `pullB.txt`, `pullB_rep.txt`, `pullC_census.txt`.

Gate: build rc=0, ctest 48/48, `24c52df2be14789c` x3, cities on `24c52df2be14789c`, cities off `43693d018d046b25`.
