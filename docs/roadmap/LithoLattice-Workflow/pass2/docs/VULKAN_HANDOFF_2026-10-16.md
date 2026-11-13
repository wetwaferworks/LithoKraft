# LithoKraft Vulkan renderer: handoff (2026-10-16, end of session; next Vulkan session after Oct 21)

Written by Cortana for JD / Elfie. The tree is `R:\repos\LithoLattice-Workflow\Pass2-2026-10-16` (Pass2 only; the main tree was not touched). Everything below is render-only: the sim determinism hashes (cities on `cb32c824f8b3ea98`, off `e015e51cf1559089`) match on every build listed. Full detail is in `CHANGES.md` (sections 0.0.1.49/50 through 0.0.1.53) and `BUGFIX_LOG.md` (BUG-0011..0015).

## 1. State as of the last passing build
- **Last passing build:** `build\Release\LithoKraft-0.0.1.53-a-direct.exe` (gate 053a, 2026-10-16 ~22:41 ET).
  - Selftests 1344 PASS / 0 FAIL.
  - Sim hashes x2 cities on/off unchanged.
  - Vulkan smoke run + 7 shots (bulbs on/off, night, texture close-up/oblique, wafer bloom, horizon).
- **`LithoKraft.exe` is still 0.0.1.44** (same hash as `LithoKraft-0.0.1.44-keep.exe`). Nothing is promoted yet; JD reviews before merge.
- **Parked builds (all in `build\Release`):**
  - older 0.0.1.50 builds, `-0.0.1.51-bulbfix`
  - `-0.0.1.52-a-shaders`, `-b-idx16`, `-c-draws`, `-d-staging`, `-0.0.1.52-argon` (0.0.1.52 final)
  - `-0.0.1.53-i-instr` (instrumentation only, benched, not gated), `-0.0.1.53-a-direct` (0.0.1.53 final)
- **Source backups:** `.orig-2026-10-16-2030/-2045` (0.0.1.50), `-2100` (0.0.1.51), `-2150` (0.0.1.52), `-2240` (0.0.1.53). Each one is the file before that version's edits.
- **Hardware used for every number here:** RTX 3050 Ti Laptop (4 GB, Resizable BAR heap 3962 MB), 1280x720 windowed, present mode immediate.

## 2. Vulkan features and toggles

### Effects (`vkFx` bits; settings.json `vkFx` 0..127, Settings > Graphics > "Vulkan effects", `--vkfx N` for one run without saving)
| bit | effect | notes |
|---|---|---|
| 1 | normal maps | face-index lookup-table TBN (0.0.1.52), etched normals; Toksvig specular AA |
| 2 | per-pixel lighting | sun + hemisphere + Blinn-Phong; also enables trace fwidth AA |
| 4 | bloom | RGBA16F glow target, half-res 5-tap bilinear Gaussian x2, `1-exp(-g)` composite with white-hot shoulder. Feeds: glow masks, wafer traces (gain 1.35), wafer heartbeat (gain 1.75, tunable) |
| 8 | pulsing glow | glow-mask G = phase, on the sim clock (render-only) |
| 16 | GPU particles | 3072 procedural trace sparks, no vertex buffer |
| 32 | distance fog | exp^2 fog thinning with height (density 0.0045) |
| 64 | bulb blend pass | alpha-blend faces drawn far->near after opaque; shell alpha capped 0.22, only the core/lens glows |

### Settings (settings.json)
- `vkFx` (127)
- `vkBloomStrength` (1.0, 0..4)
- `vkHeartbeatGlow` (1.75, 0..4; 0.0.1.53)
- `bulbShellSolid` (false = ghostly; cvar `render.bulb_shell`)
- `streamRadius` (0 = auto: Vulkan 64, others 32)
- Live cvars: `render.ground_pulse`, `render.ground_traces`.

### CLI
- `--renderer vulkan`
- `--vkfx N`
- `--vk-arena host|device|rebar` (default device, auto-upgraded to rebar when available)
- `--vk-validation`
- `--stream-radius N`
- Benches: `--bench S --bench-path still|stream|soak --bench-json F --capture --ui-shot F --no-save --mute`

### Environment (A/B and diagnostics)
| variable | effect |
|---|---|
| `LL_VK_NO_REBAR=1` | no auto-upgrade of the vertex arena to ReBAR |
| `LL_VK_DIRECT_UPLOAD=0` | 0.0.1.53 worker-direct mesh writes off (all uploads back on the main thread) |
| `LL_VK_HEARTBEAT_GLOW=x` | overrides `vkHeartbeatGlow` |
| `LL_VK_ANISO=n`, `LL_VK_LODBIAS=x` | block texture sampler anisotropy (default 8) / min LOD bias (default -0.25) |
| `LL_BENCH_POSE="x,y,z,yaw,pitch"`, `LL_BENCH_SKY="r,g,b"` | still-bench camera / sky override (screenshots) |

### Memory, upload and draw path
- **Vertex arena:** 32 MB blocks, first-fit with coalescing. Device budget is 70% of the largest device-local heap; past that, blocks spill to host-visible memory (no more "out of vertex memory"). ReBAR is auto-selected when the BAR heap is larger than 256 MB.
- **Thread safety (0.0.1.53):** allocate/free/clear take a mutex. The block table is fixed at 1024 entries, so `buffer()`/`data()` take no lock.
- **Worker-direct upload (0.0.1.53):** the stream-bench mesh jobs snapshot the map (shared lock), mesh, allocate, and memcpy into mapped ReBAR/host memory. The main thread only publishes the handles (512 per frame). Device-only blocks still go through the staging path.
  - Interface: `IRenderer::prepareChunkMeshAsync` / `publishChunkMesh` / `discardChunkMesh`. GL and software use the default (false = old path).
- **Staging:** 32 MB ring per frame slot, parked uploads (no early fence wait), and an overflow buffer instead of a queue-idle flush.
- **Index buffer:** uint16 quad indices, max 16384 quads per draw.
- **Draws:** sorted by vertex buffer. Buffers are bound only on change, with `baseVertex = offset/16`. The 64-byte per-column push goes out only on change (16 bytes otherwise).
- **Diagnostics:** GPU timestamps and `vulkan perf:` every 120 frames; `vulkan upload split:` every 120 frames. Stream benches add `update-split` / `stream-split` / `ringstats` every second.
- **Texture sampler:** mag NEAREST, min linear + linear mips, 8x anisotropic, LOD bias -0.25, UNORM.

## 3. Bench history (stream radius 64, effects on unless noted; 20 s stream bench)
| build (session) | fps avg | frame avg / p50 / p99 ms | main upload ms | GPU ms | RAM peak MB | vertex misses | notes |
|---|---|---|---|---|---|---|---|
| 0.0.1.50 (8:56 PM, clean) | 33.2 | 30.1 / 20.0 / 180.4 | - | - | 3470 | 6709 | fast only because ~6.7k sub-chunks were dropped (never drawn) |
| 0.0.1.51 (0.0.1.52 bench session, Chrome open) | 20.6 | 48.6 / 32.1 / 220.5 | ~27 | - | 3561 | 0 | arena spill to host: everything drawn |
| 0.0.1.52 (its own bench session, Chrome open) | 21.5 | 46.5 / 38.8 / 195.9 | 27-32 | 2-9 | 3274 | 0 | binds 782 -> 233/frame, push 62.6 -> 12.6 KB/frame |
| 0.0.1.52 fx0 | 19.5 | 51.4 / 44.3 / 145.5 | | | 2987 | 0 | |
| 0.0.1.52 no ReBAR | 19.6 | 50.9 / 34.1 / 332.3 | | | 3521 | 0 | |
| 0.0.1.52 (10:45 PM, same session as next row) | 18.6 | 53.7 / 36.5 / 252.5 | 30.8 | 10.9 | 3342 | 0 | 493 cols/s, 3.0 GB mesh loaded at end |
| **0.0.1.53** (10:45 PM) | 18.4 | 54.4 / 48.1 / **153.0** | **3.0** | 51.7 | 5312 | 0 | 829 cols/s, **full disc 5.3 GB** loaded; up to 5066 draws/frame |
| 0.0.1.53 fx0 | 20.7 | 48.4 / 42.7 / 173.9 | 3.0 | 26.6 | 5590 | 0 | |

- **Session conditions:** the 10:45 PM session had Chrome, Gemini and Grok Bot windows open. The 8:56 PM run was the only clean-system one.
- **Sim 32 @512 TPS (still bench):**
  - 0.0.1.50: 627 dropped ticks at the end.
  - 0.0.1.52: 501.7-522.7 TPS, 0 dropped, tick 1.90 / 1.97 ms.
  - 0.0.1.53: 508-517 TPS, 0 dropped, tick 1.90 / 1.95 ms.
- **How to read 0.0.1.53:** the main-thread upload cost is gone (30.8 -> 3.0 ms). Because the streamer now keeps up, the bench reaches the whole radius-64 disc and draws about 1.75x the geometry. That geometry is GPU-bound: 52 ms, with 2.6 GB of vertices spilled to host memory and read over PCIe. So avg fps is flat while p99 improves 40%.

## 4. Known issues / open bugs
1. **Radius 64 is GPU- and memory-bound at full load.**
   - Ring 33-64 holds 3959 of 5284 MB of mesh (12.4k columns, 40.8k draws).
   - About 2.6 GB spills to host memory, so the GPU reads it over PCIe; RAM working set peaks at 5.3-5.6 GB.
2. **Main-thread "update" is 20-45 ms in the stream bench, but it is not render work.**
   - Sim-tick-driven gameplay catch-up (locomotion / vitals / sprites / AI; `loco` in `update-split`) and the game world's own `pumpStreaming`.
   - Both grow when frames are slow. Out of scope for render-only items; needs a gameplay-side look (per-frame AI catch-up budget, streaming budget).
3. **Bench text summary line is garbled.** The `bench renderer=... avg=925760ms ...` log line prints shifted fields (seen in 0.0.1.52 and 0.0.1.53). The JSON output is correct; use `--bench-json`. Not yet fixed.
4. **Mesh borders in the stream bench.** Mesh borders depend on which neighbours were loaded when the job ran. Since 0.0.1.53 the snapshot is taken when the job starts (slightly later than before), so border faces can differ from 0.0.1.52 (render-only). Sub-chunks are not re-meshed when a neighbour arrives later; this was already true before.
5. **The worker-direct path is wired only into the stream bench.** The game's own world mesh queue (`Game::uploadMeshes`, 0.7-10 ms/frame) still uploads on the main thread.
6. **No PBR (BinaryQuad) pipeline on Vulkan.** It logs a warning once; PBR meshes are invisible on Vulkan.
7. **resources.json was edited directly for the CloudAtlas art.** Re-running `tools/convert_pack_manifest.py` would drop those entries until the manifests are updated.
8. **Grid/Wafer palette constants** (`WAFER_TINT`, `TRACE_FADE`, ...) exist but are 0/off. Deciding them is an art call.

## 5. Next steps (suggested order)
1. **LOD for ring 33-64.** Coarser meshes (2x2x2 or 4x4x4 voxel merge, or top-surface-only) for columns beyond chunk 32. Target: under 1 GB for that ring, no host spill, GPU under 15 ms. `ringstats` already logs the per-ring cost.
2. **Use `prepareChunkMeshAsync` in the game's world mesh path** (`Game::uploadMeshes`), with the same publish budget.
3. **GPU-side culling** of distant draws (frustum + distance), and/or multi-draw-indirect per vertex block to cut 5k draws/frame.
4. **Gameplay update budget** (catch-up caps for AI/sprites, a streaming budget). That is a non-render item, so it needs its own sim-hash gate.
5. **Fix the bench text summary format.**
6. **Promotion:** after JD's review, promote `-0.0.1.53-a-direct` (or a later build). LithoKraft.exe stays 0.0.1.44 until then.

## 6. Verify loop (used for every item)
- **Loop:** implement -> review -> verify -> fix -> integrate.
- **Verify means:**
  - Build (idle priority, 2 threads).
  - Selftests.
  - Sim hashes x2 cities on/off.
  - Vulkan smoke run with screenshots (self-closing, no-focus benches).
- **After each pass:** park as `LithoKraft-0.0.1.5x-*.exe` and restore `LithoKraft.exe` to 0.0.1.44.
- **Scripts:** `p2build2.ps1`, `p2logs\gate.ps1`, `p2logs\cycle.ps1`, `p2logs\bench52.ps1`, `p2logs\bench53.ps1`.

## 7. Credits
- **Argon's items** (0.0.1.52 shader pass, uint16 indices, draw compaction, staging/ReBAR; BUG-0014): Proposed by Argon (Archie), Chief Optimization Officer and Chief Optimization Operand, the Bitmaster and the Bitmasker; Built by Cortana.
- **Everything else** (0.0.1.49-0.0.1.51 Vulkan push, texture smear fix, BUG-0013, 0.0.1.53 worker-direct upload, heartbeat bloom, instrumentation): Proposed by JD, Built by Cortana.
- **Technique references** (clean-room, no code copied):
  - Schüler, normal mapping without precomputed tangents
  - Wellons, hash-prospector
  - de Vries, LearnOpenGL bloom
  - Willems, Vulkan samples
  - Quílez, better fog
  - Rakos, linear-sampling Gaussian
  - Toksvig, mipmapping normal maps

---
## Addendum 0.0.1.54 (2026-10-16 23:20 ET)
- **State:** parked `LithoKraft-0.0.1.54-a-lod`, `-b-align64`, `-c-nearstrip` (final). All gates passed (see CHANGES.md 0.0.1.54; one audio-timing selftest flake on 054b passed on rerun). **Recommended build to play: still 0.0.1.53** (`-0.0.1.53-a-direct`). Use 0.0.1.54 when memory matters. `LithoKraft.exe` is still 0.0.1.44.
- **New toggles:**
  - Graphics "Far LOD (chunks 33-64)" (`vkFarLod`, default on, live), `LL_VK_FAR_LOD=0/1`.
  - `LL_VK_STRIP_NEAR=0` (Argon 3d off).
  - `LL_BENCH_SHOT_AT=<s>` (delayed ui-shot).
- **Bench vs 0.0.1.53 (same session, busier system):**
  - Mesh 5279 -> 1512 MB; spill 1312 -> 0 MB; GPU 30 -> 8 ms; RAM 4.2 -> 2.3 GB.
  - fps 16.1 -> 13.5 (main-thread `update` 52 -> 65 ms under extra mesh work). Full table in CHANGES.md.
- **Known issues added:**
  1. LOD remesh + downsample load starves the main-thread gameplay update. Needs a cheaper downsample, an adaptive remesh budget, and a gameplay update budget.
  2. LOD is wired only into the stream-bench streamer, not the game's own world mesh path.
  3. At 33+ chunks, thin foliage reads sparser and blockier (cell threshold 1/4); Trace/bulb features are kept.
  4. No cross-fade between LOD levels yet: popping at the 32/48 chunk edges is softened by fog only.
- **Pending (JD):** on-screen FPS + frame-time readout as a Graphics toggle (off by default). The existing `fpsCounter` setting shows only "FPS n TPS n" (`main.cpp` mini overlay). Adding frame ms there is a small follow-up; not done this session.
- **Argon proposals, next session** (Proposed by Argon (Archie), Chief Optimization Officer and Chief Optimization Operand, the Bitmaster and the Bitmasker; to be Built by Cortana):
  1. **Lossless 12-byte GpuVertex12** (first 12 bytes of PackedVertex; drop slot:24; layerHi bit0 -> faceAo bit7, bit1 -> mode bit5).
     - Before doing it, verify:
       - faceAo bit7 is free (bits 0-2 face, 3-4 AO, 5-6 rot as decoded in chunk.vert).
       - Every ShadeMode value is < 32, so bit5 is free (mode bits 6-7 are the LOD tags).
       - The texture layer count is < 1024.
     - Arena alignment must then be a multiple of 12 and 16, e.g. 48 or 192, so `baseVertex = offset/12` stays integral.
     - Touches the vertex attributes, chunk.vert decode, `uploadChunkMesh`, `prepareChunkMeshAsync`, and the staging copy.
     - Gate: pixel diff vs 0.0.1.53 ~0.
  2. **8-byte vertex (3b):** only after Argon's full spec arrives, with an exhaustive round-trip test (x/y/z 0..256 in 9 bits, layer, value, mode ranges). LOD scaling keeps positions <= 256.
  3. **3c:** 16-byte-per-quad SSBO vertex pulling (`gl_VertexIndex>>2`).
  4. **Brief item 2:**
     - IGN dither cross-fade band 28..36 blocks for the near/far foliage LOD, using per-vertex snapped distance (also usable for the far-LOD ring edges).
     - Greedy UV seam clamp.
  5. **Brief item 1:**
     - Bloom as a 4-level mip chain (Karis-average first downsample, tent upsample).
     - Max-luminance-preserving mips for emissive/trace layers.

## Desktop shortcuts (2026-10-16 23:25)
- `python tools\update_desktop_shortcuts.py [--dry-run] [--exe <name>] [--if-changed]` keeps the 3 desktop test-world shortcuts on the exe named in `build\Release\RECOMMENDED.txt` (currently `LithoKraft-0.0.1.53-a-direct.exe`). It refuses `LithoKraft.exe` and any exe not in `build\Release\GATED.txt`.
- To promote a new build: `p2logs\gate.ps1 -exe <parked exe> -tag <t> -Promote`. A full pass (selftests, 4 hashes, 7 shots with 0 log errors) appends it to GATED.txt, writes RECOMMENDED.txt and updates and verifies the shortcuts. Without -Promote, a passing gate only records GATED.txt. Proposed by JD, Built by Cortana.

## Addendum 0.0.1.55 (11:40 PM ET)
- Review: docs/VULKAN_REVIEW_2026-10-16.md. OPEN: #1 a submit failure after vkResetFences leaves the fence unsignaled (hang) and there is no DEVICE_LOST path; #2 clearChunkMeshes vs in-flight worker direct upload. Not reviewed: descriptor lifetime across swapchain recreate, A52-4 parked-upload ordering, far-LOD hysteresis edges, post/bloom math, validation layers.
- Fixed: rot mask (mesher.cpp:77), negative box-coord clamp + log, TraceTint comment (5-bit slot is correct), 64-byte slice comment. Bit-drop findings Proposed by Argon (Archie), Chief Optimization Officer and Chief Optimization Operand, the Bitmaster and the Bitmasker; Fixed by Cortana.
- Sim distance default 16 (option 32, env LL_SIM_TICK_RADIUS). Hash baselines are unchanged in both modes because the scenario pins radius 8. Stream bench: 17.6 -> 42.6 fps, update 49.4 -> 15.0 ms.
- 0.0.1.55-a-fixes: not promoted (intermittent shot-log ERROR). 0.0.1.53 remains recommended.

## Addendum 0.0.1.56 (overnight 2026-10-17, 00:00-03:00 ET, Built by Cortana)
- **Builds** (parked in build\Release; nothing promoted; RECOMMENDED.txt and the shortcuts still point at 0.0.1.53-a-direct; LithoKraft.exe is 0.0.1.44):
  - `0.0.1.56-a-fixes`: Vulkan fixes, perf readout, round sim area, radius log.
  - `0.0.1.56-b-pacing`: + minimized pacing and the truncation warning.
  - `0.0.1.56-c-sim8`: + default sim radius 8.
  - `0.0.1.56-d-logfix`: + second-instance log fallback and per-shot gate logs.
  - a, b, c and d: gate FULL PASS. d passed as tag 056d2. Its first gate (056d) failed on one ERROR, the hello.as onLoad throttle, which is now the documented known-benign line (BUG-0028).
- **Promotable:** `LithoKraft-0.0.1.56-d-logfix.exe`. To promote: `p2logs\gate.ps1 -exe <exe> -tag <t> -Promote`.
- **Review:** docs/VULKAN_REVIEW_2026-10-16.md, findings R1-R21.
  - Fixed: R1 submit/device lost, R2 clear vs worker upload, R3 minimized staging growth, R4 grab barrier, R6 minimized spin, R8 truncation warning, R20 shared log file.
  - Open: R5 renderDone semaphore lifetime on recreate, R7 format fallback, R9-R11 info, R21: a running game can freeze its world (recoverable with resume) when a second LithoKraft instance starts. Pre-existing since at least 0.0.1.53. Don't run gates/benches while playing.
- **Validation layers:** not run, because VK_LAYER_KHRONOS_validation is not installed (no Vulkan SDK). Install the LunarG SDK, then run with the renderer's validation switch (vk_device.cpp) to close R5/R10.
- **New settings:**
  - Graphics > "Performance readout" (off by default): FPS, avg/p99 frame ms, update ms, GPU ms.
  - Simulation > "Round simulation area (restart)" (default ON; Proposed by JD, Built by Cortana): disc dx^2+dz^2 <= r^2+r. Env `LL_SIM_ROUND_AREA=0|1`.
  - Simulation distance default 8 (16/32 for stress tests). Env `LL_SIM_TICK_RADIUS`.
  - The startup log names the radius, the area shape and where each came from.
- **Round area at the ragged edge:** block ticks, random ticks and Trace/redstone all use the same contains() test. A column outside the disc behaves exactly like one outside the old square: frozen, with no propagation into it. Toggling it forces a full signal pass.
- **Note:** test runs rewrote `game\LithoKraft\config\settings.json` (tickRadius 16 -> 8, plus roundSimArea / simDistanceV4). Older builds ignore the new keys, but they now also start at radius 8.
- **Gate:** hash runs alternate square/round. The r4 round-area scenario is pinned to cb32c824f8b3ea98 (square) / 966be55811fb23a7 (round). Shot errors print their text, and each shot uses its own log file.
### Sim radius / area bench, 0.0.1.56-a-fixes (2026-10-17 00:02-00:15 ET, Vulkan, present=immediate, 20 s, 2 reps each, BelowNormal priority)
The override is proven: each run's log has `sim tick radius N chunks (from LL_SIM_TICK_RADIUS), square|round area (from LL_SIM_ROUND_AREA)` and `sim: tick area radius N, ..., <count> chunk columns`.

Stream path (`--bench-path stream`, world generation + streaming). Values are rep1 / rep2, then the mean:

| radius | area | columns | avg ms | FPS (mean) | p99 ms | update ms | sim tick ms |
|---|---|---|---|---|---|---|---|
| 8 | square | 289 | 17.28 / 14.46 (15.87) | 63.0 | 54.7 / 47.1 | 8.76 / 6.88 | 2.36 / 2.01 |
| 8 | round | 225 | 14.83 / 15.22 (15.03) | 66.5 | 43.8 / 52.6 | 7.06 / 7.68 | 2.07 / 2.12 |
| 16 | square | 1089 | 22.56 / 22.15 (22.36) | 44.7 | 86.2 / 81.4 | 14.19 / 13.67 | 3.18 / 3.16 |
| 16 | round | 861 | 18.72 / 24.38 (21.55) | 46.4 | 64.5 / 93.1 | 10.66 / 15.55 | 2.86 / 3.17 |
| 32 | square | 4225 | 56.01 / 56.26 (56.14) | 17.8 | 151.7 / 179.4 | 48.47 / 49.07 | 3.71 / 3.69 |

In-game test world (`--test-world --bench-path still`, self-closing, launched minimized with no focus change):

| radius | area | avg ms | FPS (mean) | p99 ms | update ms | sim tick ms |
|---|---|---|---|---|---|---|
| 8 | square | 10.21 / 10.34 (10.27) | 97.3 | 18.7 / 18.2 | 3.80 / 3.71 | 1.58 / 1.58 |
| 8 | round | 10.23 / 10.40 (10.32) | 96.9 | 18.5 / 18.3 | 4.06 / 3.66 | 1.63 / 1.50 |
| 16 | square | 10.72 / 11.66 (11.19) | 89.4 | 19.6 / 30.3 | 5.62 / 7.56 | 2.69 / 2.94 |
| 16 | round | 10.52 / 11.79 (11.16) | 89.6 | 17.9 / 36.0 | 4.84 / 7.61 | 2.37 / 2.78 |
| 32 | square | 33.11 / 29.57 (31.34) | 31.9 | 164.0 / 113.2 | 30.79 / 27.10 | 3.66 / 3.60 |

Reading:
- Radius is the big lever. 32 costs about 3x frame time against 16 on both paths, and almost all of that is main-thread `update` (49 ms stream, 29 ms test world). Sim-thread tick time hardly moves (3.2 -> 3.7 ms).
- Round vs square at r=8 (JD's ask): 22% fewer ticked columns (225 vs 289), but the frame difference is inside run-to-run noise on both paths (stream -5% avg, test world +0.5%). At r=16 it is also within noise. Round saves sim work (the sim-tick ms shows it) but does not speed up frames at r <= 16.
- Default is now 8 (JD steer, F56-10). 16 and 32 are stress-test values. At r=8, round vs square frame time is within noise. Round is ON by default because the hashes and every gate held.

## Addendum 0.0.1.57 (2026-10-17, 03:16-04:30 ET, Built by Cortana)
- Build `LithoKraft-0.0.1.57-a-simprio.exe` fixes R21: starting a second LithoKraft no longer stalls or freezes the running game's simulation (0 dropped ticks at 512 Hz; 0.0.1.56-d dropped 398).
- How, at a high level: the simulation threads get a small priority boost and are exempt from Windows background power throttling. A stall check was made robust against a race. Full technical notes are in JD's private folder, not here.
- Gate: FULL PASS (tag 057a): selftests 1344/0, hashes cb32c824f8b3ea98 / e015e51cf1559089 x2 each, round-area r4 cb32c824f8b3ea98 / 966be55811fb23a7 x2, 7 shots 0 errors (1 known-benign onLoad throttle line, not counted). Not promoted. JD promoted 0.0.1.56-d-logfix; RECOMMENDED.txt and the shortcuts are unchanged.
- Second instances stay allowed: gate shots and benches need them. They log to `logs\lithokraft.second-<ms>.log`. A second interactive window still takes focus when it opens, so the running game shows its normal pause menu (pause on focus loss, by design).
- Note: a process deliberately launched below normal priority (as the gate/bench scripts do) can still drop some ticks under heavy load. That is expected OS behaviour.

## 2026-10-17 0.0.1.58-a-blendcull: visual baseline lock (not promoted)
Proposed by Argon (Archie), Chief Optimization Officer and Chief Optimization Operand, the Bitmaster and the Bitmasker; Built by Cortana.
- Item 1 (FIXED, BUG-0030): the floating crosses came from the blended Trace Bulb shell, not particles. The blend pipeline was double-sided because it copied the chunk description after culling was turned off for entities. Fix: cull back faces in chunkBlend.
- Item 2 (ALREADY DONE): pulse/trace -> procGlow -> fragGlow on Vulkan. GL33 has no glow target by design.
- Item 3 (ALREADY DONE): face-table normals with no derivative math. Only the stale comment was updated.
- Open, needs JD: Argon's shell-only-ghost / core-only-glow spec needs real core geometry. The in-world bulb is a cube and the .lthm core model is not meshed.
- Item 1(b): the bulbs high in the sky are trace-tree tips on thin stems, and they are in the same place in GL33 and Vulkan blend-off. That points away from a far-LOD offset or a VK-only cause. Their "floating cross" look was BUG-0030. Orphaned tips at chunk borders were not exhaustively checked (worldgen; that would move hashes).
- Gate FULL PASS, hashes held. Screenshots: p2logs\shots58\{58pre,58probe,58post,58post2,58pre2}.

## 2026-10-17 0.0.1.59-b-packed12: 12-byte Vulkan chunk vertices (promoted by JD 2026-10-17 05:50 ET)
Proposed by Argon (Archie), Chief Optimization Officer and Chief Optimization Operand, the Bitmaster and the Bitmasker; layout correction and build by Cortana.
- **Toggle:** LL_VK_PACKED12, default on. =0 (or off/false/no) gives the 16-byte path, unchanged from 0.0.1.58-a. Entities, GL33 and software always use 16 bytes.
- **Layout:** z[0..8] | layer[8..14] << 9 in bytes 4..5; layer low byte in byte 9; the other bytes are as in PackedVertex. Arena alignment 192.
- **Results:**
  - Mesh memory −25.7% (stream r64: 1224 vs 1648 MB; test world 350 vs 466 MB).
  - fps and p99 within noise.
  - Gate FULL PASS with the toggle on and off, hashes held on both runs, round-trip tests pass.
- **Review:** "Packed12 review" in VULKAN_REVIEW (F59-1..6). F59-1 and F59-2 are fixed in 59-b. Still open: a deterministic frozen-time screenshot for an exact-zero pixel proof (F59-5), and a longer GPU-timer A/B (F59-6). **Update 2026-10-17: F59-6 CLOSED.** The gap was an artifact of single-window GPU sampling. Packed12 is ~17% lower GPU ms at matched draws (121/121 windows), with +8% fps at r8 and +13% fps at r64. See docs\F59-6_GPU_TIME_2026-10-17.md. Bench output now reports whole-run GPU ms (engine\tools\vk-perf-summary.ps1, p2logs\vkbench.ps1; Proposed by JD, Built by Cortana).
- **Visible-run mode (JD 2026-10-17, default for benches and smoke tests):** run `p2logs\vkbench.ps1` (switch `-ShowConsole`, default `$true`; use `-ShowConsole:$false` for no windows). It opens ONE console (`p2logs\vk-console-tail.ps1`, conhost) sized to the whole primary-monitor work area. While each run is going, the console is fed live from that run's engine `--log`: perf lines, chunk streaming, draws, GPU ms, hitches, the `update-split` and `bench-sec` lines, then the BENCH summary lines. The engine runs windowed Vulkan (never headless). Its window is CENTERED on the primary monitor at its standard windowed size, in front of the console (SWP_NOSIZE|SWP_NOACTIVATE: no resize, no focus steal). The engine is started with CREATE_NO_WINDOW because the exe is a console-subsystem app and would otherwise open a second, blank console. Everything closes itself: the game when its bench ends, the console 5 s after `results:`. The script is DPI-aware so the centering is right at 125% scaling. Example: `powershell -ExecutionPolicy Bypass -File p2logs\vkbench.ps1 -Exe LithoKraft-0.0.1.59-b-packed12.exe -Tag 59b -Scenarios smoke` (scenarios: smoke = 8 s orbit, stream64, still64, r8, testworld; `-Pairs N` for interleaved LL_VK_PACKED12 on/off; `-Screenshot x.png -ShotAt 12` grabs the desktop). Headless stays only for runs with no renderer: `--selftest`, the gate's `--handshake-scenario` hash runs, and run-bench-suite's `--perf-headless` step.
- **Next (Argon proposals):** an 8-byte vertex plus a per-quad SSBO, after a mesh range audit.

## 2026-10-17 0.0.1.60-a-locktiming (diagnostic build, not promoted)
- Diagnostic lock-timing toggle: `LL_LOCK_TIMING=1` (default off; zero behavior change when off) logs a `lock timing:` summary of main-thread lock waits every 120 frames. `engine\tools\vk-perf-summary.ps1` adds a `| LOCK wait ...` section to BENCH lines when present. Example: `$env:LL_LOCK_TIMING='1'` before a run, or `p2logs\vkbench.ps1 -Toggle LL_LOCK_TIMING` for interleaved on/off. Proposed by JD, Built by Cortana.
- Gate FULL PASS with the toggle off and on (selftests OK, 4 hashes x2 each, 7 shots, only the known-benign script-throttle line).
- Open: the visible-run mode's engine window does not reliably come to the front (JD, 07:14 ET). The window is centered via SetWindowPos without activation, so Windows may keep it behind the active app. Not fixed; parked.
- Bench note: Parakeet Live (GPU) has been running on TheWorkstation since about 06:52 ET. Absolute fps after that is not comparable with earlier runs; on/off pairs stay interleaved.
