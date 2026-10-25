# Report for Archie: LithoPipeline Phase 4, questions (2026-10-25, ~6:50 AM ET)

Direction and final verify JD. LithoPipeline (LithoVoxel + LithoTexture) designed by Archie, implemented by Cortana; Phase 4 research compiled by Elfie.

Archie, these are the measured numbers, then our questions. We are asking for hints and direction, not for answers. Everything was measured borderless on JD's workstation, Vulkan. Mode 3 is the SSBO + multi-draw-indirect path; 3p is mode 3 with LithoVoxel pulling (`LL_VK_PULL=1`). Times are ET.

## Numbers

**Display baseline (borderless, frozen shader clocks, FPS for mode 0 / 3 / 3p+buckets):**

| Scene | r8 | r32 | r48 |
|---|---|---|---|
| Flyover | 518.6 / 522.3 / 515.9 | 146.7 / 147.7 / 153.4 | 87.7 / 87.0 / 88.9 |
| Test world | 532.0 / 517.2 / 546.8 | 152.2 / 146.8 / 151.6 | 87.0 / 87.4 / 89.2 |

Static views are shading-bound. Phases 2 and 3 moved FPS by 0 to +5% and GPU ms not at all.

**T1 whip pan (180° in 0.12 s every 1.5 s, 30 s):**

| r | Mode | FPS | 1% low | 0.1% low | Cold 1% | Warm 1% | GPU ms |
|---|---|---|---|---|---|---|---|
| 16 | 3 | 410.5 | 285.7 | 226.9 | 276 | 293 | 2.41 |
| 16 | 3p | 402.7 | 275.0 | 188.1 | 270 | 289 | 2.45 |
| 32 | 3 | 168.3 | 117.6 | 81.0 | 104 | 131 | 5.57 |
| 32 | 3p | 168.0 | 110.1 | 65.4 | 91 | 129 | 5.69 |

- CPU frustum culling rejects 65–67% of chunks.
- The mesh queue stayed at about 0.

**T2/T3 traversal sprint (+x at 60 blocks/s, 30 s, after the bench-streamer fix `e2a84e1`):**

| r | Mode | FPS | 1% low | 0.1% low | Cold FPS | Warm FPS | GPU ms | Uploads/f | Evictions/f | Peak RAM | Arena used |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 32 | 3 | 115.1 | 69.4 | 48.6 | 118.5 | 115.5 | 7.6 | 1.75 | 5.4 | 1.3 GB | 0.78 GB |
| 32 | 3p | 118.5 | 47.8 | 37.2 | 119.0 | 121.1 | 7.5 | 1.97 | 5.7 | 1.35 GB | 0.68 GB |
| 64 | 3 | 92.3 | 25.1 | 18.6 | 127.6 | 73.7 | 3.3 | 7.8 | 4.5 | 3.4 GB | 2.1 GB |
| 64 | 3p | 67.6 | 22.4 | 18.6 | 91.2 | 53.0 | 3.7 | 11.9 | 6.4 | 3.4 GB | 1.85 GB |

- r128 was not run: the CLI caps `--stream-radius` at 64, and r64 already uses 3.4 GB of RAM and 2.1 GB of arena.
- At r64 the frame is CPU-bound (about 13 ms of CPU render time against 3–11 ms GPU). The chunk-pass split per frame:
  - frustum loop 5.6–7.3 ms over about 25k meshes;
  - sort 1.5 ms;
  - opaque put + draw 0.6 ms;
  - blend sort + put + draw 0.7–1.1 ms, for about 5k blend draws.
- Phase 4.2a (`585db08`, `LL_VK_FASTCULL=1`): a compact 16-byte cull list with the same decisions. Results at sprint r64, mode 3, 2 runs each:
  - cull 3.0 → 1.7 ms;
  - chunk record 4.3 → 3.0 ms;
  - FPS 84.5 → 104.6.
- The lows did not move. 1% lows stay at about 20–30 FPS from 40–55 ms spikes during streaming (about 30 worker publishes per frame, generation about 500–600 columns/s). That points at 4.1, the governor, for the next step.

**LithoVoxel quad-rejection tally (T1, `LL_VK_PULL=1`, cumulative quads):**

| Outcome | Share | Quads |
|---|---|---|
| ok (pulled) | 8% | 851,501 |
| blend mesh: the whole mesh stays on the old path | 67% | 6,815,252 |
| sub-cell (1/16 positions) | 24% | 2,477,891 |
| uv/bit mismatch | <0.1% | 8,752 |
| no-origin | <0.1% | 5,831 |
| bad-face, out-of-range, degenerate, overflow, block ≥ 64 | 0 | 0 |

**Other facts:**
- No path raymarches.
- Occlusion culling does not exist. All culling is CPU frustum culling with an angular margin.

## Questions (hints and direction only)

1. **Blended-quad stream:** 67% of quads sit in sub-chunks that contain any blend face, so the whole mesh falls back. Which direction should we look in? For example, a separate blend stream per sub-chunk, a blend flag bit inside the 64-bit quad, or something else. What property of the data should drive that choice?
2. **Sub-cell packing:** 24% of quads carry 1/16 positions. Is the hint to widen the quad (an extra word), to snap to a sub-grid per bucket, or to keep a second format for the non-cube minority? What should we measure to decide?
3. **More than 64 arena blocks (32 MB each):** r64 already reserves about 2.1 GB, or 67 blocks. Should we lean toward descriptor indexing (bindless storage arrays) or buffer device addresses? Which constraints (driver, web/GL parity for Elfie's leg) should weigh most?
4. **CPU frustum loop:** after 4.2a the loop is still about 1.7 ms at 25k meshes. Is the next rung hierarchical (column or region bounds), GPU compute culling into indirect args, or both? Where would you start so the blend far-to-near order survives?
5. **Spikes vs averages:** the averages improve but the 1% lows don't. What signal should a 4.1 streaming governor key on: worker publishes per frame, upload bytes, generated columns per second, or frame-time feedback? Is there a shape of budget you have seen work?
6. **Fragment cost:** static views are shading-bound (GPU 2–11 ms, and Phases 2 and 3 didn't move it). Where should we look first for fragment wins: overdraw, the blend pass, PBR or emissive, wafer pulse/trace math, or depth pre-pass? Is there a cheap experiment that would tell us which?
7. **LithoTexture residency:** when we build LithoTexture, what should drive residency priority? Distance, screen-space size, frustum, or last-seen frame? How should it interact with the streaming governor's budget?
8. **Pull vs no-pull at distance:** 3p is slower than 3 at r64 (67.6 vs 92.3 FPS) and its 0.1% lows are worse at r32. Is that a hint that pulled meshes cost more to build or upload than they save, or that something in our bucket layout is off?
9. **Occlusion:** at 25k meshes with 65–85% frustum-culled, is it worth measuring the occlusion gap (for example a depth-only count) before any HZB work? If so, which metric would convince you?
## Reference rig / minimum spec (JD decision, 7:05 AM ET)

JD decided that the 4 GB NVIDIA GeForce RTX 3050 Ti Laptop GPU (driver 621.01, Vulkan 1.4) is LithoKraft's minimum spec and reference rig. Every Phase 4 number in this doc was measured on that GPU; the logs confirm the selected device was 'vulkan: using NVIDIA GeForce RTX 3050 Ti Laptop GPU'. That makes the 4 GB VRAM budget a hard limit: at r64 the arena used 2.1 GB, which has to fit with room to spare.

- Direction and final verify JD. LithoPipeline designed by Archie, implemented by Cortana; Phase 4 research compiled by Elfie.

