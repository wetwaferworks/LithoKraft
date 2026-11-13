# Pass 2 (2026-10-16): software renderer, ideas by Ginny (COO, Argon); built by Cortana

The tree is R:\repos\LithoLattice-Workflow\Pass2-2026-10-16, a copy of the main tree, which was not touched. JD reviews it before any merge.
Runtime switch: LL_SW_P2=<mask> (1 quad, 2 parallel, 4 facecull, 8 morton, 16 dispatch). The default is 7. LL_SW_P2=0 gives the pre-Pass-2 renderer.
Proof tool: LL_SW_AB=<frame> renders that frame with mask A (LL_SW_AB_A, default 0) and then with the current mask, and logs the pixel diff.

1. Native GreedyQuad setup (Ginny): rasterizeTrisShaded does one setup and one row walk per quad. ON. Byte-identical.
2. Lockless two-phase setup (Ginny):
   - Dynamic mesh hand-out with thread-local fill cursors.
   - Row spans moved into phase 1.
   - Clear moved into the strip workers.
   - hardware_concurrency vs band count logged.
   - ON. Byte-identical.
   - Note: the existing per-mesh CSR bins were already lockless and thread-owned. I did not use local_bins[T][strips] because it would change the draw order.
3. Face-direction culling (Ginny):
   - 6 buckets per mesh, taken from each quad's own winding, plus bucket 6 that is never culled.
   - Each bucket has a limiting plane. The eye point comes from the projection.
   - Culled quads are skipped before transform and before binning. About 45% of quads are culled on walk.
   - ON. Byte-identical.
4. Morton 16x16 texel swizzle via kSwMortonU/kSwMortonV, done at load. OPT-IN (mask 8). Byte-identical, no gain, because the tiles already sit in L1.
5. ISA dispatch scalar/SSE2/SSE4.1/AVX2 via CPUID, overridable with LL_SW_ISA:
   - Separate TUs: software_simd_sse2.cpp, software_simd_sse41.cpp, software_simd_span4.inl.
   - No arch flags. No ymm/VEX code in the baseline objects (checked with objdump).
   - The SSE2/SSE4.1 span kernels sit behind the existing LL_SW_SIMD_SHADE opt-in.
   - The 4-wide quad transform is OPT-IN (mask 16), byte-identical, no gain.
Verify (Windows and Linux): hash e015e51cf1559089 on 2 runs after every step. Selftest ALL OK. A/B pixel diff = 0 on walk/orbit for every step and every ISA.
Overall, Windows (4x8 s interleaved): mask 0 -> 7, walk 51.9 -> 61.2, orbit 51.9 -> 61.0.
Backups: engine\src\render\*.orig-2026-10-16-0822/0833/0838/0843/0852.

# Code review and bug-fix run (2026-10-16), on the Pass2 tree

Clean full rebuild on Windows (--clean-first) because of the stale-timestamp issue.

## 0.0.1.49/0.0.1.50 — Vulkan push (2026-10-16, Proposed by JD, Built by Cortana)

Vulkan-only effects, each one a Graphics toggle (Settings > Graphics > "Vulkan effects"; `vkFx` bits in settings.json; `--vkfx N` CLI for benches). GL33 and software keep their current look. Everything is render-only: sim hashes match with effects on and off.

1. **CloudAtlas art (CloudAtlas-2026-10-16).** Trace Bulbs use a new ghost shell (`{red,blue}_trace_bulb_ghost`): the cutout dither at 30% alpha with the lens disc replaced by the glass core. Each shell has `_normal` and `_glow_mask` maps. The glass-core textures, normals and glow masks are registered in pack_32/64. Etched normals (`glass`, `red_trace_cross`, `wafer_ground_trace` `_etched_normal`) are registered in all 3 packs. Bulb faces are `kShadeAlphaBlend` (the Solid shell still turns them emissive). The `.lthm` models are under `assets/models/trace_bulb` and `assets/models/etched`. The new bindings loader (`content/material_bindings.{h,cpp}`) reads `assets/lithokraft/materials/bindings/<model-id>.json`:
   - glass rims 40/42 alpha-blend
   - cores 41/43 glow
   - Trace 5/6 glow
   - etched base 50, line 51

   Selftests cover it.
2. **Per-pixel lighting and normal maps.** Sun + hemisphere + Blinn-Phong, with normal maps on a cotangent frame built from screen-space derivatives (no stored tangents). Per-layer normal/glow maps come from naming conventions (`X_normal`, `X_etched_normal`, `X_glow_mask`).
3. **HDR bloom.** The glow masks feed an RGBA16F glow target. A half-res separable 9-tap Gaussian runs 2 iterations, then the composite pass adds `1-exp(-g)`. Lines and the HUD draw after the bloom, so they are not bloomed. **Pulsing glow** uses mask G as the phase on the sim clock (tick-synced, render-only).
4. **GPU particles.** Procedural Trace sparks (3072) with no vertex buffer, using a hash of `gl_VertexIndex` and a camera-wrapped cube. They draw additively into the frame and the glow target.
5. **Distance fog.** Exp² fog that thins with height, combined via max() with the existing fog.
6. **Blended bulb pass.** Meshes with alpha-blend faces draw again far→near (depth test on, depth write off). The opaque pass drops those faces.

Also: the window title now shows only `LithoKraft - Release 0.0.1.xx` (or Debug). Debug info stays in the in-game overlay. (Proposed by JD, Built by Cortana.)

Bench/screenshot helpers: the still bench logs its pose. `LL_BENCH_SKY="r,g,b"` overrides the sky in still benches (night shots).

### Credits / inspirations (technique only; clean-room, no code copied)
- Normal mapping without precomputed tangents: Christian Schüler, "Followup: Normal Mapping Without Precomputed Tangents" (blog, thetenthplanet.de).
- Integer hash for particles: Chris Wellons, hash-prospector, https://github.com/skeeto/hash-prospector (The Unlicense / public domain).
- Bloom (bright target + separable Gaussian ping-pong): LearnOpenGL "Bloom" by Joey de Vries, https://github.com/JoeyDeVries/LearnOpenGL (code samples CC BY-NC 4.0; only the technique was referenced).
- Multi-pass bloom in Vulkan (render pass / framebuffer structure): Sascha Willems, https://github.com/SaschaWillems/Vulkan (MIT).
- Exponential height fog: Iñigo Quílez, "Better fog" (article, iquilezles.org).

### Files
engine: assets/shaders/vk/{frame.glsl, chunk.vert, chunk.frag, particle.vert, particle.frag, post.vert, post.frag}, assets/shaders/chunk_shading.glsl (all new code under `#ifdef LITHO_VK_FX`), src/render_vk/{vk_frame_uniforms.h, vk_pipelines.h/.cpp, vk_swapchain.h/.cpp, vk_renderer.cpp, CMakeLists.txt}, src/render/{block_materials.cpp, include/.../block_materials.h, include/.../renderer.h, mesher.cpp}.
game: src/content/{appearance.cpp, content_selftest.cpp, CMakeLists.txt, material_bindings.cpp, include/lithokraft/content/material_bindings.h}, src/game/{settings.cpp, include/lithokraft/game/settings.h, game_ui.cpp, game.cpp}, src/app/main.cpp, assets/packs/pack_{16,32,64}/resources.json + textures, assets/models/{trace_bulb,etched}/*.lthm, assets/lithokraft/materials/bindings/*.json.
Backups: `.orig-2026-10-16-2030` (game.cpp also `-2045`).
Note: resources.json was edited directly. Re-running tools/convert_pack_manifest.py from manifest.json would drop these entries until the manifests are updated.

### Known issues (from the 0.0.1.50 Vulkan captures)
- With effects on, Trace Bulbs look like solid, bright gems instead of a ghostly shell. The blend pass alpha plus glow mask plus bloom come out too opaque and too hot. With `--vkfx 0`, they show the intended ghost shell (screen-door). Next step: tune the blend-pass alpha source and the ghost glow-mask gain.
- At a stream radius of 64, the Vulkan vertex arena runs out: "out of vertex memory; sub-chunk not drawn" appears 6.7k–9.9k times per 20 s bench. The 33-64 ring holds about 3.1–3.5 GB of the roughly 4.5 GB of mesh. That ring needs LOD, or the arena needs to be larger.

## Tooling (2026-10-16 late)
- **Desktop shortcuts follow the gated build** (Proposed by JD, Built by Cortana, 2026-10-16 23:25): `tools\update_desktop_shortcuts.py` points the 3 'LithoKraft Alpha - <renderer> (Test World)' shortcuts at `build\Release\RECOMMENDED.txt` (now LithoKraft-0.0.1.53-a-direct.exe); only builds in `GATED.txt` are accepted. Hook: `p2logs\gate.ps1` (backup `.orig-2026-10-16-2325`) records a fully passing gate in GATED.txt; `-Promote` also writes RECOMMENDED.txt and updates the shortcuts; otherwise it runs `--if-changed`. Not a CMake post-build step.

## 0.0.1.54 - Far LOD for the 33-64 ring + Argon brief 2 (3a, 3d) (2026-10-16)

Parked builds in `build\Release`: `LithoKraft-0.0.1.54-a-lod.exe`, `-b-align64.exe`, `-c-nearstrip.exe` (final, all items). `LithoKraft.exe` is still 0.0.1.44. Backups: `.orig-2026-10-16-2300`. Everything is render-only, and game/world state is untouched (the LOD lives in the stream-bench mesher path).
**Recommendation:** keep **0.0.1.53** as the build to play. 0.0.1.54 meets the memory target (no host spill) but not the fps target. See the bench below.

### Gates (one per item)
- **054a (LOD):**
  - Selftests 1344/0; hashes x2 on `cb32c824f8b3ea98` / off `e015e51cf1559089`; 7 shots.
  - Two shots logged one ERROR: a mod-script "CPU quota exceeded" throttle while build b was compiling in parallel. Environmental.
- **054b (3a):**
  - Selftests: the first run had 1 FAIL (`streaming: worker-thread decode ... 440 Hz`, an audio timing test, unrelated). The rerun was 1344/0.
  - Hashes OK; 7 shots.
- **054c (3d):** selftests 1344/0; hashes OK; 7 shots.

### JD item: Far LOD (Proposed by JD, Built by Cortana) [C54-1]
- **Mesh levels by column distance** (Chebyshev distance d in chunks from the camera):
  - d <= 32: full mesh.
  - 33-48: 2x2x2 downsampled.
  - 49+: 4x4x4 downsampled.
- **Downsampling:** the mesh job builds a coarse snapshot (one representative block per f^3 cell), meshes it with the normal mesher, and scales positions by f.
  - **Representative block:** a Custom-shape or shell block (Traces, bulbs) wins, so emissive features stay readable at distance (shown f times larger). Otherwise the most frequent non-air block, if at least 1/4 of the cell is non-air.
  - **Seams:** conservative padding. A negative side is solid only if the whole fine border patch is opaque, and the positive side stays open. Result: no holes, and boundary faces are always emitted (hidden by neighbours). Fog covers the 528+ block distance.
- **Hysteresis:**
  - A column goes finer as soon as it crosses an edge, but coarser only 2 chunks past it.
  - Transitions are remeshed (at most 48 columns per scan, every 2nd frame). The old mesh stays drawn until the new one lands, and stale results are dropped by generation id.
- **Toggle:**
  - Settings > Graphics > "Far LOD (chunks 33-64)", `vkFarLod` in settings.json, on by default, live.
  - `LL_VK_FAR_LOD=0/1` overrides.
- **Fixes and tooling:**
  - The bench text summary line is fixed: the format string was missing `rthreads=%u`, so every field after it was shifted.
  - `LL_BENCH_SHOT_AT=<s>` delays `--ui-shot`, for far-view shots [C54-2].
- **Not done:** the game's own world mesh path (`Game::uploadMeshes`) does not use the LOD yet.

### Argon items (Proposed by Argon (Archie), Chief Optimization Officer and Chief Optimization Operand, the Bitmaster and the Bitmasker; Built by Cortana)
- **3a [A54-3a]:** vertex arena `kAlignment` 256 -> 64.
  - Every consumer checked: `baseVertex = offset/16` needs offset % 16 (static_assert added); the arena memory is HOST_COHERENT (no atom rounding); `vkCmdCopyBuffer` has no offset alignment rule.
  - The ReBAR / worker-direct paths use the same `alignUp`.
- **3d [A54-3d]:** columns 3+ chunks out drop their kShadeLodNear (foliage near-detail) quads at mesh time.
  - The sub-chunk centre is then at least 40 blocks away, and chunk.vert only draws near quads within 32 blocks, so nothing visible disappears.
  - Hysteresis: back to full at d <= 2, stripped again at d > 3.
  - `LL_VK_STRIP_NEAR=0` opts out.
  - Effect: ring 0-16 drops from 277 to 191 MB and ring 17-32 from 982 to 635 MB.
- **Deferred to the next session** (with notes in VULKAN_HANDOFF):
  - 3b 8-byte vertex.
  - The lossless 12-byte GpuVertex12 follow-up.
  - Brief item 2 (IGN dither cross-fade, greedy UV clamp).
  - Brief item 1 (bloom mip chain, max-lum texture mips).
  - 3c (SSBO quad pulling).

### Bench, same session 23:10-23:14 ET (RTX 3050 Ti Laptop, 1280x720, immediate). Windows open: Chrome, VS Code, Gemini, Grok Bot (busier than the 0.0.1.53 session).
| run | fps | avg / p50 / p99 ms | update / upload ms | GPU ms (last) | arena device / host (spill) MB | RAM peak MB | mesh MB 0-16 / 17-32 / 33-64 / >64 | cols/s |
|---|---|---|---|---|---|---|---|---|
| 0.0.1.53 fx127 | 16.1 | 62.1 / 49.4 / 196.9 | 52.3 / 3.2 | 30.4 | 2752 / **1312** | 4167 | 277 / 982 / 3959 / 60 (5279) | 828 |
| **0.0.1.54 fx127 (LOD on)** | 13.5 | 74.2 / 70.4 / 209.2 | 65.6 / 3.7 | **8.1** | 1536 / **0** | **2274** | 191 / 635 / **680** / 6 (**1512**) | 699 |
| 0.0.1.54 fx0 (LOD on) | 15.0 | 66.6 / 58.4 / 224.8 | 57.7 / 4.0 | 4.3 | 1344 / 0 | 2378 | 191 / 635 / 732 / 6 | 716 |
| 0.0.1.54 fx127, LL_VK_FAR_LOD=0 | 19.8 | 50.5 / 41.1 / 197.4 | 40.0 / 3.8 | 40.5 | 2752 / 2592 | 5417 | 277 / 982 / 3959 / 67 | 829 |

- **Memory target met:**
  - Mesh drops from 5.3 GB to 1.5 GB (ring 33-64 is down 83%).
  - Spill to system memory drops from 1312-2592 MB to 0.
  - GPU time drops from 30-40 ms to 8 ms, and the RAM working set from 4.2-5.4 GB to 2.3 GB.
- **fps target not met:**
  - With LOD on, the frame is bound by main-thread `update` (65 ms). That is gameplay catch-up plus world streaming, starved by the extra mesh work (LOD remeshes, mesh job avg 0.6 -> 1.9 ms) on the shared worker threads.
  - Run-to-run noise is large in this busier session: the 0.0.1.53 run had a 1.4 s hitch, and the LOD-off run of the same 0.0.1.54 exe was the fastest.
  - Next step: cheaper downsampling and a lower or adaptive remesh budget, plus a gameplay-side update budget (not render-only).
- **Sim 32 @512 (0.0.1.54):** TPS 510.6-514.9 / 512, 0 dropped (rerun). The first run had a 5 s startup hitch (460 dropped ticks) while other apps were busy. The rerun was clean, and nothing in 0.0.1.54 touches the still/sim path.
- **Screenshots** (`/workspace/vulkan-push/`): `054_farview_lod_on.png`, `054_farview_lod_off.png`, `054_farview_lod_off_top_on_bottom.png` (stream bench at 16 s). The camera pose is time-based, so the frames match closely. At far range, thin foliage reads sparser and blockier with LOD on; the horizon is fog-covered in both.

## 0.0.1.53 - Worker-thread mesh upload (ReBAR direct writes) + heartbeat bloom (2026-10-16, Proposed by JD, Built by Cortana)

Parked as `build\Release\LithoKraft-0.0.1.53-a-direct.exe`. The instrumentation-only build is `-0.0.1.53-i-instr` (benched, not gated). `LithoKraft.exe` is still 0.0.1.44 (same hash as `-0.0.1.44-keep`). Backups: `.orig-2026-10-16-2240`. Everything is render-only.
Gate 053a: selftests 1344 PASS / 0 FAIL; sim hashes x2, cities on `cb32c824f8b3ea98`, off `e015e51cf1559089` (unchanged); Vulkan smoke run with 7 shots (`/workspace/vulkan-push/053_*`). Items 2 and 3 went through this one combined gate.

1. **Instrumentation [C53-1].**
   - `update-split` (Game::update sections) and `stream-split` (BenchStreamer phases) are logged once a second in stream benches.
   - `vulkan upload split` (arena alloc vs copy, worker-published meshes) is logged with `vulkan perf:` every 120 frames.
   - Finding (0.0.1.52 code, radius 64): the "upload" figure was `BenchStreamer::update`. Gen + main-thread `snapshotForMeshing` took 10-54 ms/frame and the upload loop took 17-23 ms/frame. Inside the upload loop, the renderer's own alloc + ReBAR memcpy was only 2-7 ms; the rest was the per-vertex blend scan, map bookkeeping and freeing the vertex vectors on the main thread.
2. **Worker-thread direct upload [C53-2].**
   - **Mesh jobs do the work:**
     - The snapshot moved into the mesh job, which reads the bench `ChunkMap` under a shared lock. The main thread takes the lock exclusively only to insert or erase a column. A column dropped before its job runs is skipped.
     - With a ReBAR (or host) arena, the job allocates its arena slice and writes the packed vertices straight into persistently mapped memory. It also does the blend scan and frees the CPU vertex vector.
     - The main thread only publishes the handle (`IRenderer::prepareChunkMeshAsync` / `publishChunkMesh` / `discardChunkMesh`; GL and software keep the default `false`, which means the old path).
   - **Publish budget:** 512 meshes/frame. The 6 MB/frame budget still applies to meshes that go through the staging ring (device-only blocks, `LL_VK_DIRECT_UPLOAD=0`).
   - **VertexArena thread safety:** allocate/free/clear take a mutex. Blocks live in a fixed 1024-entry table (never reallocated), so `buffer()`/`data()` need no lock. `kind`, byte counts and the block count are atomics.
   - **Opt-out:** `LL_VK_DIRECT_UPLOAD=0`.
3. **Heartbeat bloom [C53-3].**
   - The wafer heartbeat has its own bloom gain, 1.75 (was the shared 1.35; traces stay 1.35).
   - It is tunable via `vkHeartbeatGlow` in settings.json (0..4) or `LL_VK_HEARTBEAT_GLOW` (env, wins), and passed in `uPulse.z`.
   - It is still only written when the bloom bit (`vkFx` 4) is on. GL is unchanged: the define falls back to `PROC_GLOW_GAIN`.
   - A/B at the wafer-bloom pose: gain 0 -> 1.35 changes 5909 px, 1.35 -> 1.75 changes 4165 px. Shots: `053_heartbeat_{0,1.35,1.75}.png`.
4. **"update ~16 ms" check: not render-side, so not trimmed.**
   - About 1-40 ms is locomotion / vitals / sprite / AI catch-up (`loco`: sim-tick-driven gameplay, scales with ticks per frame).
   - About 6-34 ms is `pumpStreaming` (the game world's own column streamer).
   - Render-side pieces are small: ui 0.4-6 ms, game `uploadMeshes` 0.7-10 ms.
   - Changing either of the large parts would touch gameplay or world state, which is outside a render-only item.

### Bench, same session 22:44-22:47 ET (Vulkan, RTX 3050 Ti Laptop, 1280x720 windowed, immediate; windows open: Chrome, Gemini, Grok Bot, Acer Purified Voice, Realtek audio console)
| run | fps avg | frame avg / p50 / p99 / max ms | main upload ms | main update ms | GPU ms (last perf) | RAM peak MB | cols/s | mesh MB loaded at end | misses |
|---|---|---|---|---|---|---|---|---|---|
| 0.0.1.52 stream64 fx127 | 18.6 | 53.7 / 36.5 / 252.5 / 365.3 | 30.8 | 19.9 | 10.9 | 3342 | 493 | 3031 (8.8k cols) | 0 |
| **0.0.1.53 stream64 fx127** | 18.4 | 54.4 / 48.1 / **153.0** / **264.7** | **3.0** | 44.5 | 51.7 | 5312 | **829** | **5284 (16.6k cols, full radius)** | 0 |
| 0.0.1.53 stream64 fx0 | 20.7 | 48.4 / 42.7 / 173.9 / 310.9 | 3.0 | 38.7 | 26.6 | 5590 | 829 | 5284 | 0 |
| 0.0.1.53 sim 32 @512 | - | TPS 508-517 / 512, 0 dropped; tick 1.90 avg / 1.95 p99 ms | | | | 1562 | | | 0 |

- **Main-thread upload: 30.8 -> 3.0 ms/frame.** BenchStreamer phases went from ~35 ms to 1-3 ms/frame. The streamer now keeps up (829 vs 493 columns/s) and loads the **whole** radius-64 disc within the 20 s bench (5.3 GB of mesh vs 3.0 GB).
- **Same fps, different workload.** fps avg is flat because 0.0.1.53 draws far more: up to 5066 draws/frame vs 1060, with 2.6 GB of vertex data spilled to host memory and read over PCIe. GPU time rises to 52 ms, so the frame is now GPU- and update-bound, not upload-bound. p99 improves by 40% (252 -> 153 ms).
- **RAM peak 5.3 GB** is the full disc; host-spill blocks count in the working set. The next lever is LOD for ring 33-64, which holds 3959 MB of the 5284 MB.
- **Sim 32 @512:** tick 1.90 / 1.95 ms (0.0.1.52: 1.90 / 1.97), 0 vertex misses.

## 0.0.1.52 - Argon's Vulkan optimisation brief + texture smear fix (2026-10-16)

Parked as `build\Release\LithoKraft-0.0.1.52-argon.exe` (per-item builds: `-0.0.1.52-a-shaders`, `-b-idx16`, `-c-draws`, `-d-staging`). `LithoKraft.exe` is still 0.0.1.44. Backups: `.orig-2026-10-16-2150`. Everything is render-only.
Every item went through its own gate: build, selftests (1344 PASS / 0 FAIL), sim hashes x2 (cities on `cb32c824f8b3ea98`, off `e015e51cf1559089`), and a Vulkan smoke run with 7 shots. All 6 gates passed.

### Argon's items (Proposed by Argon (Archie), Chief Optimization Officer and Chief Optimization Operand, the Bitmaster and the Bitmasker; Built by Cortana)
1. **Shader pass** (`chunk_shading.glsl`, `vk/chunk.vert`, `vk/post.frag`).
   - **Wafer bloom:** the wafer heartbeat and the ground traces go into `procGlow`, which is added to the colour and also written to `fragGlow` (gain `PROC_GLOW_GAIN` 1.35). Bloom toggle.
   - **Trace AA:** per-axis `fwidth` widens the smoothstep edge to about 1.25 px. Energy check of Argon's formula: `1/max(fw*1.5,1)` does not attenuate until fw > 0.67, while the line has already grown about 10x wider, so distant lines would over-brighten. The build uses the width ratio `0.08/upper`, clamped to 0.25..1, per axis (H-set uses fw.z, V-set uses fw.x). It is tied to the per-pixel lighting bit and is bit-identical when that bit is off and on GL.
   - **Normal mapping:** a face-index lookup table replaces the derivative frame. `chunk.vert` packs the face into `vRot` bits 2..4. `FACE_T`/`FACE_B` follow mesher `faceUV` (T = +u, B = -v), and `tn.xy` is turned back by `rot&3`. `gWdx`/`gWdy`, the 4 `cross()` calls and `inversesqrt` are gone. Every `vRot` reader masks `&3`.
   - **Specular AA:** Toksvig-style. The Blinn-Phong exponent drops with normal variance and the lobe stays energy-normalised.
   - **rotUV:** branchless (selects instead of a loop).
   - **Blur:** `post.frag` takes 5 bilinear fetches at offsets 1.3846/3.2308 with weights 0.2270/0.3162/0.0703. These match the old 9-tap weights exactly.
   - **White-hot composite:** composite channels above 1 are pulled toward white.
   - **Grid vs Wafer palette:** added as tunable constants only (`GRID_OBSIDIAN`, `WAFER_SILICON`, `TRACE_RED`, `TRACE_BLUE`, `WAFER_TINT`=0, `TRACE_FADE`=0, i.e. off), so the CloudAtlas look is unchanged.
2. **uint16 quad index buffer:** max index 16384*4-1 = 65535 (static_assert), and `primitiveRestartEnable = VK_FALSE` is now set explicitly.
3. **Draw compaction** (`vk_renderer.cpp drawMesh`).
   - Visible meshes are sorted by vertex buffer, then by column.
   - `vkCmdBindVertexBuffers` runs only when the VkBuffer changes, at offset 0, and each draw uses `baseVertex = slice.offset/16`. There is a static_assert that 256 % sizeof(PackedVertex) == 0, plus an assert per draw.
   - The 64-byte column payload is pushed only when it changes; otherwise only the first 16 bytes (offset + pass) go out. Same stage flags as the 80-byte range.
   - Counters plus a GPU timestamp pair are logged as `vulkan perf:` every 120 frames.
4. **Staging and ReBAR.**
   - `stage()` no longer waits early. Uploads made while the slot is still busy are parked in CPU memory and drained after renderFrame's own fence wait.
   - The ring is now 32 MB per slot (was 8). A full ring spills into a per-slot overflow buffer instead of `submitOnce`/`vkQueueWaitIdle`.
   - **Auto-ReBAR:** a DEVICE_LOCAL|HOST_VISIBLE heap larger than 256 MB (the size of the old fixed BAR window) upgrades the `device` arena to `rebar`. `LL_VK_NO_REBAR=1` opts out. ReBAR blocks now count against the 70% device budget too.
   - **RTX 3050 Ti Laptop: Resizable BAR = yes** (3962 MB heap).
   - **Blur passes:** their own colour-only subpass dependencies (no DEPTH_STENCIL stages). Not BY_REGION: the blur taps neighbouring texels, which is not framebuffer-local, so BY_REGION would be invalid here.

### Cortana / JD items (Proposed by JD, Built by Cortana)
5. **Texture smear fix (Vulkan).**
   - Anisotropic filtering at 8x: the device feature is now enabled; `vkGetPhysicalDeviceFeatures` was added to the loader.
   - Minification LOD bias -0.25. Magnification stays NEAREST, so close-ups stay crisp. Mips are still linear blits plus coverage mips for cutouts.
   - UNORM is kept: the art and GL are display-referred, so sRGB would shift the look.
   - Bloom and fog do not blur the base texture: bloom is additive from the glow target only, and fog is a colour mix. Fog density is unchanged (0.0045).
   - `LL_VK_ANISO` / `LL_VK_LODBIAS` override for A/B shots.
6. **BUG-0013:** running config.lth rewrote settings.json, which is what made the 0.0.1.51 effects-off bulb shot look Solid. The file is now saved again after the restore. The "effects-off ghost regression" was this, not a shader bug.

### Bench (Vulkan, RTX 3050 Ti Laptop, 1280x720 windowed, immediate). Chrome was open at this run, so not the 8:50 clean-system state.
| run | fps avg | frame avg / p50 / p99 ms | RAM peak MB | vertex misses |
|---|---|---|---|---|
| 0.0.1.50 stream64 fx127 (8:56, clean) | 33.2 | 30.1 / 20.0 / 180.4 | 3470 | 6709 |
| 0.0.1.51 stream64 fx127 (same session) | 20.6 | 48.6 / 32.1 / 220.5 | 3561 | 0 |
| **0.0.1.52 stream64 fx127** | 21.5 | 46.5 / 38.8 / 195.9 | 3274 | **0** |
| 0.0.1.52 stream64 fx0 | 19.5 | 51.4 / 44.3 / 145.5 | 2987 | 0 |
| 0.0.1.52 stream64 fx127, no ReBAR | 19.6 | 50.9 / 34.1 / 332.3 | 3521 | 0 |

- 0.0.1.50 looked faster only because it dropped about 6.7k sub-chunks (never copied or drawn). From 0.0.1.51 on, everything is drawn, and the frame is bound by main-thread upload (about 27 ms) plus update (about 16 ms). GPU time stays 2-9 ms.
- **Binds per frame** (perf log, stream64 fx127): 782 draws -> 233 VB binds (was 782), push 12.6 KB/frame (was 62.6 KB).
- **Sim 32 @512 TPS:** 501.7-522.7 TPS, 0 dropped (0.0.1.50: 627 dropped at the end), tick 1.90 avg / 1.97 p99 ms.
- Remaining cost: the main-thread mesh upload copy (27-32 ms/frame at radius 64). Next step: move the memcpy to a worker, or add LOD for the 33-64 ring.

### Credits
Argon items 1-4: Proposed by Argon (Archie), Chief Optimization Officer and Chief Optimization Operand, the Bitmaster and the Bitmasker; Built by Cortana. Bilinear-pair Gaussian: technique from Daniel Rakos, "Efficient Gaussian blur with linear sampling" (rastergrid.com blog, 2010; technique only). Toksvig specular AA: M. Toksvig, "Mipmapping Normal Maps" (NVIDIA tech report, 2004; technique only).

## 0.0.1.51 - Trace Bulb look + Vulkan memory/upload fixes (2026-10-16, Proposed by JD, Built by Cortana) - PAUSED

Parked as `build\Release\LithoKraft-0.0.1.51-bulbfix.exe`. `LithoKraft.exe` is still 0.0.1.44 (same hash as `-0.0.1.44-keep`). Backups `.orig-2026-10-16-2100`.

### Done (built; went through the verify gate)
1. **Trace Bulb look with effects on (JD canon).** In the blend pass the shell alpha is capped at 0.22 and the shell is not bloomed. Only the glass core/lens (glow mask R) turns solid and writes to the glow target (`chunk_shading.glsl`).
   Gate g51: selftests ALL OK; hashes cities on `cb32c824f8b3ea98` x2 and off `e015e51cf1559089` x2 (unchanged); Vulkan smoke + shots: bulbs on/off, night, texture close-up/oblique.
2. **BUG: config.lth left bulb meshes Solid.** The config.lth exec now calls a remesh after restoring `render.bulb_shell` (`game.cpp`).
3. **`--vkfx` is runtime-only.** It now goes into `GameConfig::vkFxOverride` and is no longer written to settings.json (`game.h`, `main.cpp`, `game.cpp`).
4. **Vertex arena spill (code in, not benched yet).** It budgets 70% of the largest device-local heap. After that, new blocks go to host-visible memory instead of failing with "out of vertex memory" (`vk_vertex_arena.h/.cpp`).
5. **Stream-bench upload budget (code in, not benched yet).** Mesh uploads are capped at 6 MB/frame, and the rest waits in a queue (`bench_stream.h/.cpp`).

### Remaining (paused by JD)
- (Done in 0.0.1.52.) Re-bench the 64-chunk stream with effects on: fps avg/min, p50/p99, RAM, mesh MB, gen rate, ring breakdown, and the count of vertex-memory misses (target 0). Also the sim 32 @512 TPS stability run.
- Texture smear: enable anisotropic filtering (query `samplerAnisotropy`, set it in `vk_texture_array`), check LOD bias, mip generation, and sRGB vs UNORM, and maybe lower the fog density. The before shots are captured; the after shots are not.
- With effects OFF, the 0.0.1.51 Vulkan shot shows the bulb shells opaque (the gate's `bulbs_off`) instead of the 0.0.1.50 screen-door ghost. Needs a check (the shell setting or the non-blend path) before 0.0.1.52.
- Optimization pass (render-only: buffer pooling, culling, shader cost).
## Fixed (backups .orig-2026-10-16-0910 on Windows, matching box backups)
- **Walk-bridging consumed an item even when the sim rejected the place** (open item).
  - A Command edit with `x == kPlaceReceipt` now receipts whether its target cell changed, in the same ordered queue as Place receipts.
  - The game now queues a refund for each bridge place, and refundRejectedPlaces gives back rejected ones.
  - World state and the hash are untouched.
  - Files: `engine/src/sim/sim_thread.cpp`, `sim_thread.h`, `game/.../game.cpp`.
- **player.lat validation** (open item): stat/vital entries are now kept only if the read was intact and the name is 1-64 printable ASCII bytes. A torn or garbage name is dropped. Values were already clamped by the registries and unknown names were already ignored. File: `game/.../world_save.cpp`.
- Pass 2 log nit: a misplaced comment on the swprof facecull line.

## Deferred
- **Receipt queue / refund queue misalignment** past 4096 outstanding entries: both sides cap at 4096. This predates the review and can't be reached in normal play. Fix would be a shared sequence id.
- **LL_SW_AB (debug tool)** toggles the global Pass 2 mask, and its frame counter is shared across SoftwareRenderer instances. A concurrent MCP vision capture could render one frame with mask A. Only matters when the env var is set.
- **Naming (I-prefix):** ComponentBehavior, procgen Node, SignalPropagationPolicy and TextEditBase have pure virtuals without the I prefix. These are the open naming questions for Master Chief, so they need his ruling before an API rename.
- **Naming (lowercase -or):** no violations found in identifiers.
- Not every file was read line by line. The review focused on the Pass 2 changes, the open items, and the sim/save edit paths.

## Verification after fixes
- Windows: clean rebuild OK; hash e015e51cf1559089 x2; selftest ALL OK (1298); ctest 46/48 (baseline systerian, storage); A/B diff 0 for default, SSE4.1 and AVX2 (walk + orbit).
- Linux: build OK; hash x2; selftest ALL OK (1299); ctest 47/48 (systerian); A/B diff 0 for SSE2, SSE4.1 and AVX2.
- FPS, mask 0 -> 7:
  - Windows (median of 4): walk 50.5 -> 60.5, orbit 45.8 -> 60.1.
  - Linux, base binary -> Pass 2 build:
    - 8 threads: walk 39.9 -> 42.4, orbit 35.7 -> 40.2.
    - 2 threads: walk 20.4 -> 22.2, orbit 18.9 -> 20.0.

## 2026-10-16 ~09:20-09:45 EDT: playtest test world, Pattern ROM screen, Repeator rename (Pass2 only)
Backups: box `.orig-2026-10-16-0921`, Windows `.orig-2026-10-16-0936`.
- **Test world** (`--test-world`, which is the same as `--bench-scene testworld`): `content/test_world.{h,cpp}` builds a flat Wafer pad around the spawn.
  - Every registry block is enumerated at run time (ids 1..size-1, the same list as the E palette): 87 of 87 placed, 0 missing.
  - Rows are grouped WIRE / IO / LOGIC / LAMP / MACHINE / BLOCK1-2 / PLANT, each with a 3x5 ceramic block-font label on the floor and a ceramic strip underneath so blocks can't couple through the floor.
  - Saves are off by default; pass `--save-dir` to keep a save.
- **42 slots:** all 41 carry slots besides the Fragment start with 512-stacks, components first. Placing never consumes items in the test world.
  - The E dialog now also shows a "Carry (42)" grid: hotbar 1-8, offhand L/R, main 1-32. Clicking a slot swaps it into the selected hotbar slot.
  - New callback `GameUi::Callbacks::pickCarry`, added last so existing positional initialisers still work.
- **Pattern ROM** (`content/pattern_rom.{h,cpp}`, `smiley_rom_frames.inc`): plays CloudAtlas's 16x16 smiley loop (smile, blink, smile, wink, grin) on a 32x32 Color Lamp 64 wall (2x nearest-neighbour; `--rom-screen 64` gives 4x).
  - It is ticked from `SignalWorldSimulation::tick`, once per sim tick. Frame lengths are 19/5/19/13/22 ticks (CloudAtlas's ms values at 32 TPS), so the loop runs 16x faster at 512 TPS.
  - It re-asserts the pixels every tick, because the signal sim zeroes unfed lamps after the build.
  - Yellow is vga64 index 60 (#FFFF00), i.e. strength 61. CloudAtlas's placeholder 12 is #00FF00 (green) in vga64 and #FF5555 in cga16/vga32. The outline is index 0 (black), strength 1.
- **New flags:** `--capture` (F3, controls panel and hint text off; F3 stays off), `--bench-path still` (with an `LL_BENCH_POSE` override), `--ui-view inventory`.
- **Repeater → Repeator** (JD): identifiers, display name, behaviour name, string id `lithokraft:repeator`, texture refs, MODDING/README/ARCHITECTURE/ROADMAP/RESOURCE_PACKS. `repeater.h` is now `repeator.h`.
  - The alias `lithokraft:repeater` → `lithokraft:repeator` handles old string ids.
  - The Repeator is registered with the pre-rename stable id `fnv("block:lithokraft:repeater")`. Chunks, saves and the handshake hash store this id; renaming the id alone moved the hash to 5e7feb5c5b417bf9.
  - Engine `PackStack::addFileFallback("repeator","repeater")` loads the old texture files until the repeator_* art arrives.
- Verified: box and Windows both build; hash e015e51cf1559089 x2 on both; selftest 1299 (box) and 1298 (Windows) all OK; Windows SW A/B diff 0, walk 63.9 / orbit 63.5 fps.

## 2026-10-16 ~09:44-10:25 EDT: SW colour lamps, crystal leaves (cutout / fancy-fast / coverage mips), crystal twinkle (Pass2 only)
Backups: box `.orig-2026-10-16-0944` (SW palette) and `.orig-2026-10-16-1000` (leaves), Windows `.orig-2026-10-16-1015`.
- **Bug 1 fixed: software renderer colour lamps 16/32/64.** It now matches GL33/Vulkan.
  - `CpuTextureArray` keeps the palette rows.
  - `SoftwareRenderer` stores the tier palette, mask and rim bindings.
  - A `lampShade` CPU port of `chunk_shading.glsl` vMode 5-7 runs after shading. The SIMD span path skips the palette modes.
  - The smiley ROM is yellow on SW (box and Windows).
  - Other scenes: 0 px diff against the pre-change binary (SW and GL33, fixed poses), and `LL_SW_AB` diff=0.
- **Crystal leaves** (CloudAtlas `LithoKraft-Leaves-Traced-2026-10-16.rar`). Four blocks are appended at the end of the registry (ids 88-91): `lithokraft:crystal_leaves_{a,b,reddom,bluedom}`, flags kSolid | kTransparent | new engine flag `kLeaves` (1<<10).
  - Textures for pack_16/32/64 and 16x16 HUD icons.
  - Each model has `faces` = cutout, `active` = `_opaque`, `masks.glow` = glow.
  - The test world adds four leaf clumps left of the ROM screen (x -12..13, z 124..127).
- **Cutout alpha test at 0.5, no blending, on all three renderers:** new `kShadeLeaf` (17), shared GLSL `MODE_LEAF` (GL33 and Vulkan), and the SW pixel loop.
- **`render.leaves`** (-1 auto / 0 fast / 1 fancy; env `LL_LEAVES=fast|fancy`):
  - **Fancy:** two-sided faces (each face emitted again with reversed winding, so cull state is unchanged) and no leaf-to-leaf culling.
  - **Fast:** leaves count as opaque (normal culling and AO) and draw the opaque texture with kShadeTex, so the SIMD path and greedy meshing apply.
  - **Auto:** fast on machines with 4 or fewer logical cores (Chromebook class), else fancy.
  - Changing the setting remeshes.
- **Coverage-preserving mips** (`render/texture_coverage.h`, Castano): each mip's alpha is scaled so the fraction of texels at or above 0.5 matches level 0.
  - GL33 overwrites the driver mips for opted-in layers.
  - Vulkan copies CPU levels over the blit chain.
  - The SW 32->16 downsample uses the same scaling.
  - Only layers named `*leaves*` opt in (not `_opaque` / `_glow`), so every other texture is unchanged.
- **Crystal twinkle:** `k = R/255 * max(0, sin(2pi(t + G/256)))^3 * 0.8`, lerped toward white. R = strength, G = phase (from the glow mask).
  - GPU: `texelFetch` at level 0 (nearest, never mip-smeared) through `uLeafGlow[4]` / `uTwinkle`. Vulkan has new `FrameUniforms` tail fields (1808/1872, size 1888).
  - SW: an integer 256-entry LUT (no libm at run time). Glow layers are nearest-downsampled.
  - Clock: render clock `FrameParams::timeSec` only. It is never sim state.
  - **`render.twinkle`** (-1 auto / 0 off / 1 static / 2 on; env `LL_TWINKLE`). Auto = static when leaves resolve to fast (the Chromebook tier). Static = frozen at t=0.
  - Capture aid: `LL_RENDER_TIME=<sec>` pins the render clock.
- **Repeator art** (`repeator_2026-10-16.rar`): placed in pack_32/64 `textures/blocks/repeator/` and `assets/art_incoming/repeator_2026-10-16/`.
  - It is the new base + tintmask, 4-delay system, not a drop-in for the live `repeater_*` faces, so it needs tint-shader wiring.
  - The live textures still load through the `repeator` -> `repeater` fallback.
- **Selftest:** the foliage check now allows blocks after id 87, and there is a new crystal-leaves check.
- **Checks:** hash `e015e51cf1559089` x2 (box and Windows). Selftest box 1300 PASS / 0 FAIL, Windows 1299 PASS (ALL OK).
- Credit: the SW coverage and Morton-tile work builds on Ginny (Argon)'s Pass 2 SW tiling and SIMD-span ideas; the SIMD-span exclusions for the new modes follow her span rules.


- **10:25 JD follow-up:** the test-world grove is now 8 trees that vary: red-dominant (reddom) and blue-dominant (bluedom) mains with sparse a/b/opposite-colour accents. Twinkle is now a travelling wave across the canopy (world-space sweep dot(pos,(0.11,0.05,0.07)), clock x0.8; G only jitters each crystal along it) so it reads like current running through Trace. Mirrored in the SW integer LUT. The test world is not in the hash scenario: hash still e015e51cf1559089 x2. Windows backups .orig-2026-10-16-1030.

## 2026-10-16 11:25 ET: WaferBeta art integration (CloudAtlas delivery, 192 PNGs)
- Art: CloudAtlas's 62 block textures, 9 masks, and 25 icons per pack were validated (RGBA, 32/64 px, alpha 0/255) and copied into pack_32 and pack_64. Existing files were backed up as `.orig-2026-10-16-1105`.
- resources.json: each directional machine (signal transmittor/receivor, dish, overclockor, portor, fusor, chipor, chipor_viewport, lithographor) now uses front/back/side/top/bottom=side faces and an active front. The lithographor also gets an active top. Etchor gets active faces. The Repeator model now uses the base + tint-mask set.
- Engine: new `kShadeTraceTint` (mode 18) on SW, GL33, and Vulkan. A model mask `"tint:<texture>": "<mask>"` tints the masked texels with the Trace ramp of the channel behind the back port (red/blue). The ramp is dim when idle and bright plus emissive when powered. `kPaletteWidth` is now 65, so index 64 of the Trace ramp is reachable.
- Engine fix: `BlockMaterials` named-texture lookup was capped at 256 textures (left over from the 8-bit layers). The packs now have 330, so the cap is lifted.
- Repeator: now a textured 3/16 slab. The d2/d4/d8/d16 tops come from CloudAtlas's tint masks, which replace `repeator_delay_mask` and the voxel buttons.
- Lithographor: re-enabled at the END of the registry (id 92) as a Custom cuboid model (body, lens column, cap) ported from CloudAtlas's lithographor.cuboids.json. It has no sim behaviour yet.
- Chipor Viewport: has its own tex `chipor_viewport` with per-face body art. The top stays glass because CloudAtlas's tops are opaque.
- Test world: added a Trace-tint demo behind the spawn (4 rows: red/blue × idle/powered). Hash e015e51cf1559089 is unchanged.
- Selftests: added checks for the Lithographor (last id) and the viewport tex. Repeator material expectations updated.

## 2026-10-16 11:30 ET: F3 status bar shows renderer and release info, plus the Alpha build counter (JD 11:25 / 11:27)
- The F3 overlay, the window title bar, and the log now show `LithoKraft Alpha a.b.c.d (commit) Release|Debug built <date time>` and the renderer: `Software <isa> x<N> threads`, `GL33`, or `Vulkan`.
- Build counter: `build_counter.txt` at the tree root is tracked and starts at 0.0.1.0. The always-run CMake target `lk_build_counter` (cmake/bump_build_counter.cmake) bumps it on every build of the game or exe, Debug or Release. Each field runs 0..63; a field that reaches 64 resets and carries to the left. The target writes the generated `lithokraft_build_counter.h`, which only build_version.cpp includes.
- The version is used only for display and the log; it never feeds the determinism hash (e015e51cf1559089 is unchanged).
- New selftest for the carry logic: 0.0.1.63 → 0.0.2.0, 0.0.63.63 → 0.1.0.0, 0.63.63.63 → 1.0.0.0, and full wrap.
- The software renderer's ISA tier comes from swsimd::activePath, and its thread count from SoftwareRenderer::renderThreads (building on Ginny/Argon's thread-count telemetry and SIMD dispatch).

## 2026-10-16 12:10 ET - Alpha 0.0.1.3: inventory/hotbar +25% (JD playtest); built by Cortana
- New GameUi::kInventoryScale = 1.25f (game_ui.h). It multiplies the hotbar slot size and spacing, the inventory palette and carry-grid cells (icons fill the slot, so they grow too), and the HUD offsets above the hotbar (slot-name label, controls panel). The UI is renderer-agnostic, so software, GL33 and Vulkan all get it.
- The palette scroll shows 5 visible rows (was 6) so the larger dialog fits at 1366x768 and 1920x1080.
- Pick block in fly mode: the root cause has not been found. A code read found no fly gate, so it is logged as OPEN in BUGFIX_LOG.md, which is new.
- Selftests: ALL OK (1302 PASS). Hash e015e51cf1559089, stable over 2 runs.
- Backups: game_ui.cpp / game_ui.h / CHANGES.md .orig-2026-10-16-1200.
## 2026-10-16 12:35 ET - Alpha 0.0.1.5: software ("Litho") renderer visual pass. Suggested and implemented by Cortana
JD approved five upgrades. Two shipped in this build; three are not done yet (see below). GL33/Vulkan behaviour is unchanged (renderer.h setBloom is a no-op for them).
1. Texture-preserving AA (software_aa.cpp). FXAA now takes the depth buffer and filters only pixels whose 3x3 neighbourhood straddles a depth step. CMAA, when depth is available, marks depth edges only, not luma edges. Texture interiors are no longer smoothed. render.aa is unchanged and still defaults to "off".
   - BUG-0003 is NOT fixed by this. The real cause is the 16x16 box-filtered software texture tiles; see BUGFIX_LOG.md.
2. Bloom (software_aa.cpp applyBloomPost; hooked up in SoftwareRenderer::renderFrame after AA, before ASCII/overlays). Bright pass at luma > 0.72 (sky excluded by depth), quarter-res downsample, separable 9-tap binomial blur, additive bilinear composite.
   - New cvar render.bloom: -1 auto (on if > 4 cores), 0 off (default), 1 on. LL_BLOOM=0|1 env override. Live, no remesh.
   - Cost, testworld bench at 1080p: about +11 ms/frame (walk p50 16.2 -> 27.1 ms). That is why it defaults off; the pass needs threading/SIMD before auto-on.
3. Emissive block light: NOT done this build.
4. Corner AO: NOT done this build. The mesher already bakes classic per-vertex AO into PackedVertex.faceAo bits 3-4 (mesher.cpp), but the software rasterizer ignores those bits. It needs an AO varying in the shaded span path (rasterizeTrisShaded + SIMD span kernels).
5. Lithography shimmer: NOT done this build. It needs a Trace mask in the span shader; LL_RENDER_TIME already pins the render clock for it.
- Checks: selftests ALL OK (1302 PASS). Hash e015e51cf1559089 on 2 runs (render-only change).
- Backups: .orig-2026-10-16-1215 on software_aa.cpp/.h, software_renderer.cpp/.h, renderer.h, game.cpp, settings.h, settings_cvars.cpp, CHANGES.md, BUGFIX_LOG.md.
## 2026-10-16 12:45 ET - Alpha 0.0.1.8: full-resolution software textures (BUG-0003) + --ui-shot fix (BUG-0004). Suggested and implemented by Cortana
- Software ("Litho") renderer: the texture tiles follow the loaded pack (pack_64 -> 64x64, pack_32 -> 32x32) instead of a fixed 16x16 box downsample. Runtime tile shift/mask/area/scale go through FragmentInput, all scalar sample sites, ShadeSpanBasic and the SSE2/SSE4.1/AVX2 span kernels. At 16 the output is bit-identical to before (same u*15 mapping).
- Cap: 64 by default, 32 on <= 4 logical cores; env LL_SW_TEXRES=16|32|64. CpuTextureArray::setMaxTile() exists, but there is no render.tex_res cvar yet (follow-up; it takes effect on texture upload or F9 reload).
- Morton swizzle (opt-in LL_SW_P2 mask 8) is 16x16-only and is skipped at larger tiles.
- fps, testworld bench at 1080p, 6 s, presenter near the 60 Hz cap: walk 373 -> 364 frames (p50 16.1 -> 16.4 ms), orbit 350 -> 357 frames. No measurable cost on this machine.
- --ui-shot without --bench now runs a 2 s self-closing bench (BUG-0004).
- New selftests: 1304 PASS (was 1302). Hash e015e51cf1559089 on 2 runs.
- Not done this round: corner AO (render.ao), Trace shimmer (render.shimmer), emissive block light.
- Backups: .orig-2026-10-16-1230 on every file edited.
## 2026-10-16 13:10 ET - Alpha 0.0.1.12: software corner AO, Trace shimmer, distance mipmaps (BUG-0005), Graphics settings tab. Suggested and implemented by Cortana
- render.ao (default on, live): uses the mesher's baked corner AO (PackedVertex.faceAo bits 3-4) as a perspective-correct per-pixel factor (0.55/0.70/0.85/1.0) in the scalar span path. Triangles with any occluded corner shade scalar; fully open ones still take the SIMD span. Bit-identical when off or unoccluded.
- render.shimmer (default on, live): a slow diagonal brightness wave (+-16%) on POWERED Trace (strip-tint and Trace-tint modes), driven by the render clock FrameParams::timeSec, so LL_RENDER_TIME pins it.
- render.mipmaps (default on, live): software distance texture LOD (BUG-0005).
- render.tex_res (0 auto / 16 / 32 / 64) now wired to CpuTextureArray::setMaxTile. It applies on the next texture load (F9 or restart).
- New Settings tab "Graphics" (game_ui.cpp GameUi::openSettings): Texture resolution, Distance mipmaps, Anti-aliasing, Bloom, Corner AO, Trace shimmer. Each is also a console cvar (settings_cvars.cpp) stored in GameSettings (settings.h).
- Not done: emissive block light (D).
- Checks: selftests ALL OK, 1304 PASS (the game-ui tab-count test was updated to 4 tabs). Hash e015e51cf1559089 x2.
- fps (testworld 1080p, 6 s): walk 364 -> 342 frames (p50 16.4 -> 17.6 ms), orbit 357 -> 317 (16.9 -> 19.0 ms).
- Backups: .orig-2026-10-16-1250.
## 2026-10-16 14:45 ET - Alpha 0.0.1.14: emissive block light (render.emissive_light). Suggested and implemented by Cortana
- Software renderer only, render-only (hash e015e51cf1559089 x2 unchanged).
- Emitters are collected at mesh upload: one per block cell of kShadeEmissive faces and of POWERED Trace (strip tint red/blue, Trace tint), coloured red / blue / warm white, max 64 per sub-chunk. Each frame takes the 32 nearest within 32 blocks of the camera.
- Each lit triangle gets per-vertex point light (radius 6 blocks, quadratic falloff, perspective-correct interpolation, colour-multiply plus a small additive lift). Lit triangles shade on the scalar path.
- Toggle: Settings > Graphics > "Emissive block light" / cvar render.emissive_light. Default OFF: on the Trace-dense test world it roughly doubles frame time (walk 342 -> 169 frames in 6 s).
- 4-core check (affinity 0xF, --render-threads 4, Auto-tier settings: tex 32, leaves fast, bloom off, emissive off): walk 186 frames / 6 s = ~31 fps (p50 32.4 ms).
- Selftests ALL OK, 1304 PASS. Builds use /m:2.
- Backups: .orig-2026-10-16-1420.
## 2026-10-16 14:53 ET - Alpha 0.0.1.23: pick block fix (BUG-0002), unpowered tint fix (BUG-0006), LED wall tilt + ceiling mount, renames, red look-at line
Backups: .orig-2026-10-16-1440 beside every changed file. Hash e015e51cf1559089 on 2 runs. Selftest ALL OK (1305 PASS) plus headless harness OK (new pick tests).
- **BUG-0002 pick block** (Discovered by J.D., Fixed by Cortana): middle click now always puts the looked-at block in your hand: hotbar hit selects it, a carry-inventory stack is swapped into the selected slot, and creative fills the selected slot (the old stack moves to the carry inventory). Cause: the full creative hotbar sent picks into the carry inventory unseen. See BUGFIX_LOG. New headless selftest (walking + flying).
- **BUG-0006 unpowered tint** (Discovered by J.D., Fixed by Cortana): Repeator and other tint-masked machines are neutral grey until powered; colour + glow only when on. chunk_shading.glsl (GL + Vulkan) and the software mirror. Render-only.
- **Debug overlay (requested by J.D.)**: the F3 "look:" line (the block you're looking at) is red; the other lines are unchanged. game_ui.cpp/.h. Display-only.
- **Fragment -> Logbook (requested by J.D.)**: display name "The Logbook"; screenshot console messages say "Logbook gallery". Internal ids kept (kItemFragment, lithokraft:fragment, FragmentState/Gallery blobs) so saves load. Using it: Use (right click) on a block in reach dispatches kCmdItemUse to the registered Fragment handler, which logs "decision-pending (JD C3)" and fails closed, so nothing happens in game. No new behaviour invented.
- **Torches -> Red LED / Blue LED (requested by J.D.)**: display names only; ids lithokraft:red_torch / lue_torch kept (saves).
- **LED wall tilt + bottom mount (requested by J.D.)**: on a wall the LED is drawn upright, raised 3/16, its base at the wall, its top leaning 4/16 out (Minecraft wall-torch style; mesher.cpp, render-only). LEDs can now hang under a block (attach face 2; signal_world.cpp canAttach). New selftest "LED bottom attach". Already-placed content is unchanged and the hash held.
- **Trace Tip -> Red Trace Bulb / Blue Trace Bulb (name confirmed by J.D.)**: display names only; ids lithokraft:trace_tip_red (40) / 	race_tip_blue (41) kept.
- Note: JD's running LithoKraft (started 14:41) locked LithoKraft.exe, so the old exe was renamed to build\Release\LithoKraft.inuse-1451.exe (still running, not killed) and the new build linked beside it. Delete that file after he closes the game.
## Alpha 0.0.1.31 (2026-10-16, round 2)
- Via power-state art: neutral grey faces (rev2_rivets sides, rev1 top/bottom) tinted by own channel when carrying signal; rev1 tint/emissive masks registered. Art by CloudAtlas.
- Red/Blue Trace Bulb: dithered cutout shell (fallback path) on all renderers using CloudAtlas's dither + shell-opacity/lens-glow masks (masks registered, blended pass TODO). Art by CloudAtlas.
- HUD: looked-at block display name shown above held-item name whenever a block is targeted (requested by J.D.). Red F3 look line kept.
- Testworld logs origin for screenshot poses.
- KNOWN: software walk bench 52 -> 30 fps vs 0.0.1.23; cause under investigation.
- Note: build\Release\LithoKraft.inuse-1451.exe is the renamed exe held by JD's running game; delete after it closes.

## Alpha 0.0.1.36 (2026-10-16 15:40, perf fix; found by Cortana's build check)
- Software fps regression 0.0.1.23 -> 0.0.1.31 (52 -> 30 fps, testworld walk) traced to the Trace Bulb shell: dropping kOpaque made every bulb-bulb inner face and every face behind a bulb render, plus the per-pixel cutout path. Via art and HUD label cost ~0.
- Fix: see-through cubes no longer emit faces against the same block (mesher). 30 -> ~39 fps.
- Software emitter/shimmer for kShadeTraceTint now keys on the powered bit (0x80) instead of value>0 (Via tint slot index made unpowered Vias count as lights). Measured ~0 fps on this path but correct.
- HUD look-name cached per target change.
- A/B switch LL_P2_PERF_OFF (render/display only): 1 hide look label, 2 Via untinted, 4 solid emissive bulbs (old look, ~50 fps), 8 legacy emitter test, 16 no bulb self-cull, 32 bulbs occlude neighbours.

## Alpha 0.0.1.40 (2026-10-16 16:15) - Trace Bulb shell setting (JD decision)
- Graphics tab: 'Trace Bulb shell' dropdown Ghostly / Solid (cvar render.bulb_shell, saved in settings.json as bulbShellSolid). Default Ghostly on all renderers and tiers. Live: changing it re-meshes.
- Solid = old opaque glowing bulb (engine flag kShell; mesher treats kShell as opaque + emissive). Render-only; hash unchanged.
- Fix: the stale config.lth cvar no longer overrides the saved bulb setting at startup.
- Software walk bench: Ghostly ~35 fps, Solid ~44-48 fps (3 runs each, this machine; 0.0.1.23 baseline measured 52 earlier, machine load varies).
- LL_P2_PERF_OFF bits 4/32 retired (bulb solid is the setting now); bits 1/2/8/16 remain for A/B.

# Silicon Cities + Aluminum Ramp (Alpha 0.0.1.44, 2026-10-16), Pass2 tree
Status: **Approved / Implemented / Ongoing** (JD approved 16:29; ON by default per JD 16:30).
Design and math: **Argon**. Implementation: **Cortana**.

**New hash baseline (cities ON, the default): `cb32c824f8b3ea98`**, stable on 2 runs. Approved content change.
With `--cities off` (or `"enabled": false`) the hash is still **`e015e51cf1559089`** (2 runs). Adding the ramp block does not move it.
(The intermediate cities-on hash before ramps was f78be3fefb9ff1b9 on 0.0.1.42. It is superseded.)

## Silicon Cities (worldgen stage `silicon_cities`)
- New files:
  - `engine/src/procgen/silicon_cities.{h,cpp}`
  - `engine/src/procgen/silicon_cities_selftest.cpp`
  - `engine/src/procgen/include/litholattice/procgen/ramp_shape.h`
- Declared via `stages.h`. Loaded by `generator_loader` from the `silicon_cities` feature in `wafer.json`, appended last.
- The feature RNG is keyed by name, so the other features don't shift.
- Integer math only (no floats, no `<cmath>`). C++20 shifts. Seeds come from `deriveSeed(dimSeed, nameHash("city"))` plus `latticeHash`. Every block is a pure function of (dimension seed, x, z), and each column computes its own part, so cities line up across chunk and sector borders.
- **Sectors:** one hub per 256x256 die sector (x>>8, z>>8), with the hub offset in 64..191.
- **Districts:** from Argon's 8-entry table: Quarry 3/8, SRAM 2/8, Clock 1/8, ALU 1/8, LithoHub 1/8.
- **Bus width** `4u<<((h>>24)&3)` sets the pad radius: 14, 16, 20 or 28.
- **Pads:** flat at base_y 264, with QuinticQ16 (clamped) terraced skirts (`&= ~3`).
- **Placeholder shapes** from existing blocks: ALU step pyramid, Clock spire, SRAM block grid, LithoHub beacon, Quarry plain pad.
- **Bus decks:**
  - NS M1 at +8 and EW M2 at +16, on Manhattan routes between neighbouring hubs.
  - Roadbed d<=8, Red Trace channels at d = 2, 4 and 6, guardrail at d=8, copper pillars every 16.
  - 1:2 ramps (M1 rises exactly 8, M2 rises 16).
- **Live vs dormant:** about 3/16 of sectors are live (Red Trace); the rest are dormant (Fossil Trace, dead memory). 1/8 of bus links are left unconnected (12.8% measured).
- **On/off:**
  - JSON `"enabled"` defaults to true.
  - **`--cities on|off` on the command line overrides the JSON**, for marketing captures: the default Wafer vs the interchange Wafer on the same seed.
  - When off, the stage is never built, so the output is byte-identical to before.
- I did not use `struct_registry`: it seals at boot, before worldgen loads. The per-column pure layout covers chunk borders without overlap replay.
- `city_capture.exe <outdir> [frames]`: headless stills plus flyover frames from the real wafer path, with the game appearance. It never opens a window.

## Aluminum Ramp (block id 93, `lithokraft:aluminum_ramp`)
- **Block:** appended last, after the Lithographor (id 92). Custom shape, solid, not opaque, directional. Shares the aluminum_block model.
- **State:** 5 bits in the low value bits: facing 2 + shape 2 (straight/inner/outer) + hanging 1. Right-hand corners are stored as left corners of the next facing.
  - The hanging bit (upside down) is needed for the deck-underside brackets.
- **Mesh:** planar quads only, 5 to 7 per ramp (base, sides or triangles, slope). Full faces against opaque neighbours are culled.
  - New engine API `MeshWriter::quad` (free corners).
  - Software renderer: these quads go in the never-culled bucket. The greedy pass never merges them; plain cubes still merge as before.
- **Corner snapping** (Minecraft stair rules):
  - **Worldgen:** ramps go on deck step tops, step undersides (hanging) and under deck edges (hanging brackets). Neighbour ramps are evaluated from the same pure layout, so corners agree across chunk borders.
  - **Player placement:** in `SignalWorldSimulation::onBlockChanged`. Facing comes from the look direction. The ramp hangs when placed under an overhang (air below, solid above). Straight neighbours re-snap; existing corners keep their shape.
- **Collision is still a full cell**, like the Repeator (block shapes are deferred).

## Tests
- Selftest ALL OK, 1,339 PASS.
- 31 silicon_cities checks:
  - identical output twice; negative coords
  - no hub-hash collisions (4096 sectors) or city-seed collisions (2048 seeds); seed 0 at the origin isn't zero
  - district mix 1480/1009/547/558/502 of 4096
  - live 767/4096; unlinked 1054/8192
  - ramp rises exactly 8 (in-world 120 -> 128)
  - enabled:false is byte-identical; `--cities` override both ways
  - ramp state packing; ramp corners; MC snapping left/right/blocked
  - 4162 worldgen ramps (3037 hanging), 5 corners
- Content: Aluminum Ramp appended at id 93; the Lithographor test was updated for the new last block.
- Software fps (bench walk/orbit, 2 runs, 10 s each): cities on 34.9/46.4 and 34.2/46.3; off 33.9/46.9 and 32.3/44.6. No measurable cost. The bench paths stay near spawn, so few city chunks are in view.

## Files changed (backups `.orig-2026-10-16-1630`)
- **Engine:**
  - `engine/src/procgen/`: generator_loader.cpp, CMakeLists.txt, procgen_selftest.cpp, include/.../stages.h
  - `engine/src/render/`: mesher.cpp, include/.../block_appearance.h
- **Game app:** `game/LithoKraft/src/app/`: main.cpp (`--cities`), CMakeLists.txt (city_capture)
- **Game content:** `game/LithoKraft/src/content/`: blocks.cpp, include/.../blocks.h, appearance.cpp, signal_world.cpp, content_selftest.cpp
- **Data:** `game/LithoKraft/assets/worldgen/wafer.json`
- **New:** silicon_cities.{h,cpp}, silicon_cities_selftest.cpp, ramp_shape.h, app/city_capture.cpp

## Shots
- Stills: `C:\Users\jdwetwork\GrokBot-Transfer\SiliconCities-2026-10-16\` (overview, interchange, street, underside, ramp_close).
- Flyover: `C:\Users\jdwetwork\GrokBot-Transfer\SiliconCities-Flyover-2026-10-16.mp4` (25 s, 1280x720, 24 fps, offline software-renderer frames).

## Left (Ongoing)
- Vehicles on the decks.
- Portor docking at hubs.
- Real district art (all shapes are placeholders).
- Ramp collision and walk-up (it's a full cell now).
- Re-snapping existing corner ramps when a neighbour changes.
- Neighbour re-mesh across sub-chunk seams after a player-placed snap.
- A home reserve, so the spawn and test area stay clear if JD wants that. Cities can reach the 8-chunk area today.
- A real-time city bench path.
# Highway Ramp + streaming / distances (2026-10-16, 0.0.1.48 compile-check build). Proposed by JD, Built by Cortana

Status: code done and compile-checked. NOT promoted: `build\Release\LithoKraft.exe` is still 0.0.1.44 (shortcuts unchanged). The new exe is parked at `build\Release\LithoKraft-0.0.1.48-hwramp.exe`. The final full build and benches are on hold until CloudAtlas's normal maps / lighting / octree models are folded in.

## Highway Ramp (`lithokraft:highway_ramp`, id 94)
- A gentle 1-in-2 slope over a paired 2-cell footprint. Only for highways, byways and interchanges: straight run, 4 facings, no corners, no hanging. It never snaps like a stair or roof block. The one-block Aluminum Ramp is unchanged.
- State: facing bits 0-1 (the high side), bit 2 half (0 low, 1 high). Helpers are in `procgen/ramp_shape.h` (`ramp::highway`).
- Placement: the clicked cell becomes the low half and the high half goes one cell toward your look direction. If that cell is taken, the clicked cell becomes the high half and the low half goes behind it. If both are taken, the pair turns to the first free side. canPlace refuses if no side is free. Breaking either half removes both.
- Mesh: base, slope quad, side trapezoids/triangles and end faces. The seam between the halves isn't drawn. Uses the Aluminum Ramp art (aluminum_block, CloudAtlas style).
- Player collision: smooth. The ramp isn't a full cell for the player; after each locomotion tick the body sits on the 1-in-2 surface, with a 0.2 block stick when walking down. Other entities still treat it as a full cell.
- Creative: shows up automatically (the palette lists every registry block).
- Worldgen option, default OFF: add `"highway_ramp": "lithokraft:highway_ramp"` to the silicon_cities stage in wafer.json. Each 1:2 deck step top then gets one Highway Ramp pair; undersides and edge brackets keep the Aluminum Ramp. Not in wafer.json, so existing worlds are byte-identical.

## Streaming / distances (JD)
- Graphics: "World streaming" toggle plus "Render / stream distance" 0-64 (0 = auto: Vulkan 64, software / GL 3.3 32). Hard cap 64. Applies on restart.
- Sim: "Simulation distance" (was "Tick radius"). Default 32, was 8. Settings files still on the old default 8 migrate to 32 once (`simDistanceV2`).
- CLI: `--stream-radius N` (1-64), `--streaming on|off`.
- Streamer and eviction radius = max(sim, stream). The bench `stream` path uses the stream radius (was a fixed 8) and logs `ringstats` lines (cols / draws / verts / tris / mesh MB per 0-16, 17-32, 33-64 ring) once a second, for the LOD decision.

## Verify (0.0.1.48)
- Hash cities on: cb32c824f8b3ea98 x2. Cities off: e015e51cf1559089 x2. `--streaming on/off`: cb32c824f8b3ea98 both (the handshake scenario doesn't depend on streaming).
- Selftest ALL OK, 1341 [PASS], 0 [FAIL]. That includes 2 new highway ramp checks; the settings and content tests were updated.

## Files (backups `.orig-2026-10-16-1955`)
- Content: blocks.cpp, blocks.h, appearance.cpp, signal_world.cpp, content_selftest.cpp
- Procgen: ramp_shape.h, silicon_cities.h/.cpp, generator_loader.cpp
- Sim: sim_thread.h (`getBlockAndState`)
- Game: game.cpp, game.h, player_locomotion.h, settings.h/.cpp, game_ui.cpp, game_ui_selftest.cpp, bench_stream.h/.cpp
- App: main.cpp

## Left
- Benches: Vulkan 64 (fps min/avg, RAM/VRAM, gen rate, ringstats), sim-32 tick stability at 512 TPS, ramp walk test in-game.
- The partner half placed across a sub-chunk seam may wait for the next re-mesh (same known seam issue as the Aluminum Ramp).
- CloudAtlas art staged in `_incoming\CloudAtlas-2026-10-16\`, not integrated yet.

## 0.0.1.55 (2026-10-16 11:40 PM ET): Vulkan review + bit-drop fixes + sim distance 16 (Built by Cortana)
- docs/VULKAN_REVIEW_2026-10-16.md: targeted review, severity-ranked with file:line. Two open bugs (#1 submit-failure fence hang / no DEVICE_LOST handling, #2 clear vs in-flight worker upload). Not a full line-by-line read (time).
- Fixes (bit-drop findings Proposed by Argon (Archie), Chief Optimization Officer and Chief Optimization Operand, the Bitmaster and the Bitmasker; Fixed by Cortana): BUG-0017 rot mask, BUG-0018 negative box coords clamp + log, BUG-0019 TraceTint comment; slice-alignment comment.
- JD: simulation distance (tickRadius) default 32 -> 16; 32 is still selectable in the existing tick-radius setting. Settings files without `simDistanceV3` move 32 -> 16 once. `LL_SIM_TICK_RADIUS=<0..512>` overrides settings.json, and CLI `--radius` still wins. NOTE: tickRadius is a radius around the player, so 16 means 33x33 chunks and 32 means 65x65.
- Sim hashes: the handshake scenario pins its own radius 8 (handshake_test.cpp:394), so the hashes do not depend on this setting. Both modes reproduce cb32c824f8b3ea98 / e015e51cf1559089 (gate run1 = 32, run2 = 16, 3 gates). There is no separate 16x16 hash baseline, because none can differ.
- gate.ps1 (backup gate.ps1.orig55): hash run1 uses LL_SIM_TICK_RADIUS=32 and run2 uses 16; both must match the baselines.
- Stream bench (Vulkan, 20 s, single run each, JD's session possibly active): radius 32: avg 56.9 ms (17.6 fps), p99 172.8, update 49.4 ms. Radius 16: avg 23.5 ms (42.6 fps), p99 90.0, update 15.0 ms.
- Build: LithoKraft-0.0.1.55-a-fixes.exe (based on 0.0.1.54-c, so far LOD + near-strip are included). Gates 055a/b/c: selftests OK (055a had one audio-streaming flake that passed alone), hashes 4/4 every run. Each run had one intermittent shot-log ERROR on a different shot (bulbs_on, bulbs_on, night), so it was NOT a full pass and was NOT promoted. 0.0.1.53 stays recommended.
- Remaining: review findings #1/#2, the unreviewed areas listed in the review doc, a validation-layer run, an in-game test-world tick-radius measurement, and the F3 FPS readout.

## 0.0.1.56 - overnight Review / Fix / Verify (2026-10-17 00:00-03:00 ET, Built by Cortana)
Builds are parked in build\Release and none was promoted: RECOMMENDED.txt and the desktop shortcuts still point at 0.0.1.53-a-direct. LithoKraft.exe is 0.0.1.44.
- `LithoKraft-0.0.1.56-a-fixes.exe`: F56-1..7. Gate FULL PASS.
- `LithoKraft-0.0.1.56-b-pacing.exe`: 056a + F56-8/9. Gate FULL PASS.
- `LithoKraft-0.0.1.56-c-sim8.exe`: 056b + F56-10 (default sim radius 8). Gate FULL PASS.
- `LithoKraft-0.0.1.56-d-logfix.exe`: 056c + F56-11 (second-instance log fallback). Gate FULL PASS (056d2). **Promotable candidate.**

1. **Vulkan robustness (review R1-R4, R6, R8; Discovered by Cortana review, Fixed by Cortana).** BUG-0020..0025: submit-failure recovery and DEVICE_LOST clean close (F56-1), mesh epoch vs clear (F56-2), screenshot host barrier (F56-3), bounded staging while minimized (F56-5), null-swapchain pacing (F56-8), and a one-time kMaxQuads truncation warning (F56-9). The signing fatal-exit path was not touched.
2. **Performance readout (F56-4, Proposed by JD, Built by Cortana).** Settings > Graphics > "Performance readout (FPS, frame / update / GPU ms)" (`fpsCounter`, off by default). It uses the existing HUD debug-text path: `FPS n  avg x ms  p99 x ms  update x ms  GPU x ms`, refreshed every second. GPU ms comes from the Vulkan timestamp queries (EMA) and shows "n/a" on GL33/software. Hidden in --capture shots. The old software mini strip is retired.
3. **Round simulation area (F56-7, Proposed by JD, Built by Cortana).** Settings > Simulation > "Round simulation area (restart)" (`roundSimArea`) and env `LL_SIM_ROUND_AREA=0|1`.
   - Rule: a chunk column ticks when |dx|,|dz| <= r and dx^2+dz^2 <= r^2+r. The r^2+r disc (the circle of radius r+0.5 in chunk units) keeps the 4 axis tips, so the radius reads the same as square.
   - Column counts, round vs square: r=8 225 vs 289, r=16 861 vs 1089, r=32 3313 vs 4225.
   - It covers the engine TickArea (block ticks, random ticks and their cache) and the SignalSim (Trace/redstone) area through one predicate.
   - **Default ON**: the hashes held and every 0.0.1.56 gate passed.
4. **Default sim radius 8 (F56-10, JD steer).** `tickRadius` defaults to 8. 16 and 32 stay available as stress-test values (settings field, `--radius`, `LL_SIM_TICK_RADIUS`). Existing settings files at the old default 16 move to 8 once (`simDistanceV4`).
5. **Radius override proof (F56-6).** The startup log has `sim tick radius N chunks (from <source>), <square|round> area (from <source>)` and `sim: tick area radius N, ..., C chunk columns`. 0.0.1.55's line had been logged before the log file opened.
6. **Gate (p2logs\gate.ps1, Proposed by JD, Built by Cortana).**
   - Hash run 1 uses square and run 2 round (the env is set, and the scenario pins its own area).
   - New r4 round-area scenario (`LL_HS_RADIUS=4`, `LL_HS_ROUND_AREA=0|1`, 2 runs each): stability is checked, plus hard baselines square cb32c824f8b3ea98 / round 966be55811fb23a7.
   - Shots print their ERROR text.
   - Baselines unchanged: cb32c824f8b3ea98 (cities on) and e015e51cf1559089 (cities off). The default scenario stays square r=8, independent of settings.
7. **Gate shot noise (F56-11/12, Discovered by Cortana).** BUG-0027: a second instance could not open the shared log, and the gate read the other instance's file. Each shot now has its own `--log`. BUG-0028: the recurring ERROR is the documented-benign first-overrun throttle of `hello.as` onLoad. It is the only classified line; everything else still fails.

## 0.0.1.57 - R21 second-instance stall fix (2026-10-17 03:16-04:30 ET, approved by JD; Discovered by Cortana, Fixed by Cortana)
- `LithoKraft-0.0.1.57-a-simprio.exe` (parked; added to GATED.txt only by a full gate pass; NOT promoted. RECOMMENDED stays 0.0.1.56-d-logfix, as JD promoted). LithoKraft.exe is 0.0.1.44.
- F57-1: the simulation threads keep running at full speed while another program loads. They now run one step above normal priority, and the process and sim threads opt out of Windows background power throttling.
- F57-2: a sim stall check re-tests completion before acting (latent misfire).
- Result: with a second instance starting and exiting twice, the running game at 512 Hz keeps 0 dropped ticks and does not freeze (0.0.1.56-d: 398 dropped). See BUG-0029.
- Gate: FULL PASS (tag 057a): selftests 1344/0, hashes cb32c824f8b3ea98 / e015e51cf1559089 x2 each, round-area r4 cb32c824f8b3ea98 / 966be55811fb23a7 x2, 7 shots 0 errors (1 known-benign onLoad throttle line, not counted)

## 0.0.1.58-a-blendcull (2026-10-17, gated FULL PASS, not promoted)
Proposed by Argon (Archie), Chief Optimization Officer and Chief Optimization Operand, the Bitmaster and the Bitmasker; Built by Cortana.
Visual-baseline lock (Priority 1, before any vertex-size change). Argon led item 1.
- B58-1 (BUG-0030), Vulkan: the sorted alpha-blend pipeline (Trace Bulb ghost shells, Laysor) now back-face culls like the opaque chunk pipeline. It used to copy the chunk pipeline description after the entity pipeline had switched culling off, so it drew the far faces of every bulb through the near ones. That caused the doubled, offset "floating" red/blue crosses and the flat streaks from the bottom faces. The opaque pass, GL33 and software are unchanged.
- Shared chunk_shading.glsl: comment only. The normal-mapping header no longer describes the retired screen-space cotangent frame; the face-table (FACE_T/FACE_B) code was already in place.
- Verified, no change: ground pulse and traces add into procGlow, and on Vulkan into fragGlow (attachment 1, bit 4). GL33 has no glow attachment or bloom by design (since 0.0.1.49) and adds procGlow to colour. lithoPerturbNormal has no derivative math; the remaining dFdx/dFdy/fwidth calls are texture gradients, trace AA and Toksvig specular AA.
- Gate: selftests 1344/0, hashes cb32c824f8b3ea98 / e015e51cf1559089 x2, r4 square/round cb32c824f8b3ea98 / 966be55811fb23a7 x2, 7/7 shots with 0 errors. GL33 shots are pixel-identical to 0.0.1.57-a.

## 0.0.1.59-b-packed12 (2026-10-17, gated FULL PASS with the toggle on and off, not promoted; supersedes 0.0.1.59-a)
Proposed by Argon (Archie), Chief Optimization Officer and Chief Optimization Operand, the Bitmaster and the Bitmasker; layout correction and build by Cortana.
- **P59-1 Packed12, Vulkan chunk vertices 16 → 12 bytes** (bit-drop review layout "12Z", approved by JD).
  - Bytes 0..11 of PackedVertex are kept, and z also carries the layer's high bits: z[0..8] | layer[8..14] << 9.
  - Decode: z = zl & 511, layer = layerLo | (zl >> 9) << 8. All 2048 layers (kMaxLayers) are lossless.
  - Out-of-domain values (z > 511, layer > 32767) are clamped, counted and logged; they never wrap.
- **Files:**
  - New: render_vk/include/litholattice/render_vk/packed_vertex12.h (packer), assets/shaders/vk/chunk12.vert, assets/shaders/vk/chunk_vert_body.glsl.
  - Edited: assets/shaders/vk/chunk.vert (now the 16-byte wrapper over the shared body), vk_pipelines.cpp/.h, vk_renderer.cpp, vk_vertex_arena.cpp/.h, render_vk/CMakeLists.txt, tests/engine_tests.cpp.
- **Toggle:** LL_VK_PACKED12 (1/on/true/yes or 0/off/false/no). Default **on**; =0 is the 16-byte path, unchanged from 0.0.1.58-a.
- **Arena alignment:** 192 (lcm(12, 64)) for Packed12; 64 on the 16-byte path.
- **Unchanged:** entities stay on 16-byte vertices (their own pipeline desc), as do GL33 and software. The CPU PackedVertex, meshing and every sim hash are unchanged.
- **Results (vs 0.0.1.58-a):**
  - Mesh memory −25.7% (stream r64: 1224 vs 1648 MB live slices; arena 1247 vs 1696-1728 MB). Test world: 350 vs 466 MB.
  - fps and p99: within run-to-run noise.
  - Gate shots: no change beyond the animation noise floor.
- **Tests:** engine round-trip tests all pass. They cover exhaustive z 0..511 × layer 0..2047, every byte-field value, 2M random vertices, the guard clamps, the GLSL decode formula, and the unaligned batch packer.
- 0.0.1.59-b: review fixes F59-1 (toggle parsing, BUG-0031) and F59-2 (shader comment, BUG-0032).
- 2026-10-17: review item F59-6 is closed with no code change. F59-6 - Discovered by Cortana (review), Investigated by Cortana: artifact of single-window GPU sampling on a streaming bench; Packed12 is ~17% lower GPU ms at matched draw counts (121/121 windows); no code change.

## Tooling (2026-10-17): whole-run Vulkan GPU ms in bench output (Proposed by JD, Built by Cortana)
- New `engine\tools\vk-perf-summary.ps1` reads the existing `vulkan perf:` log lines (no engine change, no hash impact). It reports gpuLast@draws (the old value, now with its draw count), gpuRunAvg (frame-weighted), gpuRunMed, gpuLoadedAvg (windows within 10% of peak draws) and gpuPer1kDraws, plus fps and p99 from the bench JSON.
- `engine\tools\run-bench-suite.ps1` prints a BENCH line per Vulkan run, and summary.md gains GPU last@draws / run avg / run med columns (backup .orig-2026-10-17-0629).
- New `p2logs\vkbench.ps1`: interleaved toggle A/B (default LL_VK_PACKED12) with alternating order. Runs are self-closing and do not steal focus, and the script waits for any gate or bench already running.
- Example: the original F59-6 logs read gpuLast 3.11@2639 vs 2.29@2720 draws, but gpuRunAvg 1.20 vs 1.24 ms.

## 0.0.1.60-a-locktiming (2026-10-17, gate FULL PASS with LL_LOCK_TIMING off and on, hashes x2 each; NOT promoted)
- Diagnostic lock-timing toggle: `LL_LOCK_TIMING=1` (default off; zero behavior change when off) logs a `lock timing:` summary of main-thread lock waits every 120 frames. `engine\tools\vk-perf-summary.ps1` adds a `| LOCK wait ...` section to BENCH lines when present. Example: `$env:LL_LOCK_TIMING='1'` before a run, or `p2logs\vkbench.ps1 -Toggle LL_LOCK_TIMING` for interleaved on/off. Proposed by JD, Built by Cortana.
- Measurement only: no locking or sim behavior changed. Findings are kept in JD's private notes.
- Tooling: `p2logs\run-guard.ps1` (wait-for-other-runs now excludes the runner's own process tree and the tail console, with a 120 s timeout) fixes a console stuck on 'waiting for other run'. `vkbench.ps1` uses it.
- Exe SHA-256 6CA4255FA210F7D1AE4D3E0E5B691FCB35400B5AAEF18F0CDC27D9082227CDA3.
