# CHANGES: Cortana (Grok Bot) on Elfie's LithoLattice / LithoKraft, 2026-10-15

This lists every source file changed since the package arrived. Times are ET (HHMM). Each change has a backup beside it as `<file>.orig-2026-10-15-HHMM` in the working tree. Each changed file starts with a `// Cortana 2026-10-15 HHMM:` marker (`#` in CMake/Python), and the individual edits carry dated finding-ID comments.

The determinism hash `aa82e7afb1eff9d3` holds in Release and Debug after every batch, and it was never repinned. No sim or circuit math changed.

| Layer | File | Time(s) | Finding ID | What changed |
|---|---|---|---|---|
| TOOLS | tools/build.py | 0440 | PORT | The VS generator is no longer hardcoded: it uses the newest VS, with `LL_CMAKE_GENERATOR` as an override. |
| ENGINE | engine/CMakeLists.txt | 0440, 0504 | PORT | Fallback for the engine/LithoKraft symlink on Windows. MSVC-only `/STACK:8388608`. |
| ENGINE | engine/src/crypto/include/litholattice/crypto/u128.h (new) | 0438 | PORT | `ll_u128` is `unsigned __int128` on GCC/Clang and a hi/lo struct on MSVC. |
| ENGINE | engine/src/crypto/include/litholattice/crypto/sha512.h, sha512.cpp, ed25519.cpp | 0438 | PORT | `__uint128_t` becomes `ll_u128`. The NIST and RFC 8032 vectors still pass. |
| ENGINE | engine/src/crypto/signing.cpp | 0449 | PORT | POSIX headers are guarded. |
| ENGINE | engine/src/crypto/crypto_selftest.cpp | 0459 | PORT | Closes the file before remove_all (Windows can't delete an open file). |
| ENGINE | engine/src/mcp/http_server.cpp | 0449 | PORT | windows.h is included before bcrypt.h. |
| ENGINE | engine/src/procgen/struct_registry_selftest.cpp | 0450 | PORT | MSVC narrowing fix (static_cast). |
| ENGINE | engine/src/render/binary_greedy_mesher.cpp | 0452 | PORT | `__builtin_ctz` becomes `llCtz32/64` (_BitScanForward on MSVC). |
| ENGINE | engine/src/render/include/litholattice/render/software_shaders.h | 0452 | PORT | `__builtin_sqrtf` becomes `LL_SQRTF`. |
| ENGINE | engine/tests/engine_tests.cpp | 0455 | PORT | Zero-size array becomes `empty[1]`. |
| GAME | game/LithoKraft/src/app/selftest.cpp, game/.../game_ui_selftest.cpp, game/.../input_selftest.cpp | 0455 | PORT | getpid comes from process.h on _WIN32. |
| GAME | game/LithoKraft/src/game/screenshot_selftest.cpp | 0520 | PORT | Test root uses weakly_canonical (the Windows TEMP folder is an 8.3 short path). |
| GAME | game/LithoKraft/src/content/model/litho_formula_gallery_selftest.cpp | 0452 | PORT | Windows path fix. |
| ENGINE | engine/src/assets/include/litholattice/assets/mod_signature.h | 0733 | SIGNING | The old test key is retired. The new dev public key `04c1766e…011c` is baked in. The private key stays outside the tree and out of this package. The fatal-exit check is intact. |
| GAME | game/LithoKraft/src/app/biome_capture.cpp | 0750 | PORT | Portable `sourceRoot()` replaces the /home/hatch paths. |
| ENGINE | engine/src/sim/include/litholattice/sim/sim_thread.h | 0825 | CHUNK-STALL | Adds `holdBetweenTicks()`. |
| ENGINE | engine/src/sim/sim_thread.cpp | 0825 (SIM-H1 tried at 1010, reverted at 1035) | CHUNK-STALL / SIM-H1 | The post-tick section runs under postLock. The SIM-H1 unlock/yield between catch-up steps was reverted because engine_sim went from 1.7 s to a 900 s timeout. The bad copy is saved as `sim_thread.cpp.reverted-SIM-H1-2026-10-15-1035`. |
| GAME | game/LithoKraft/src/game/game.cpp | 0825, 0935, 1020 | CHUNK-STALL, ALT-TAB, GAME-H1 | Stream insert/evict hold the sim between ticks instead of stopping and restarting it. Adds pausedByFocus_, click-in-world resume and the Esc toggle. Entity origin is snapped to 16 blocks, with uint16 offsets in 1/16 units. |
| GAME | game/LithoKraft/src/game/include/lithokraft/game/game.h | 0935 | ALT-TAB | Adds pausedByFocus_. |
| ENGINE | engine/src/console/concmd.cpp | 0928, 1010 | CONSOLE, CON-H2 | Dangling pointer fixed (`const std::string tmp(raw)`). Query blocks/count use the saturated cell cap. |
| ENGINE | engine/src/platform/glfw_input_backend.h/.cpp | 0935 | ALT-TAB | Focus edge counters and takeFocusEdges(). |
| ENGINE | engine/src/platform/include/litholattice/platform/window.h, window.cpp | 0935, 1045 | ALT-TAB, LAUNCH-FOCUS | Adds cursorReallyCaptured() and takeFocusEdges(). Adds raiseAndFocus() and setTopmost() via GLFW_FLOATING. |
| GAME | game/LithoKraft/src/app/main.cpp | 0935, 1045 | ALT-TAB, LAUNCH-FOCUS | Focus handling is event-driven. Launch order is focus, then cursor capture, then always-on-top. Topmost is dropped on focus loss (auto-pause) and restored when a click in the world resumes. |
| ENGINE | engine/src/console/include/litholattice/console/iworldquery.h | 1010 | MCP-H1, CON-H2 | Adds `queryCellsSaturated()`, an overflow-safe cell count with kMaxQueryCells = 4194304. |
| ENGINE | engine/src/mcp/server.cpp | 1010, 1020 | MCP-H1, MCP-H2 | Saturated query cap. Windows stdio reads with `_read(0)`. |
| ENGINE | engine/src/mcp/sockets.cpp | 1010 | MCP-H3 | SO_EXCLUSIVEADDRUSE on Windows. |
| GAME | game/LithoKraft/src/game/world_query.cpp | 1010 | MCP-H1 | The backend cap is checked before pauseSim. Loops use int64. |
| ENGINE | engine/src/render/mesher.cpp | 1020 | RND-H1 | Per-job thread_local block-def table. |
| ENGINE | engine/src/storage/anvil_region.cpp | 0452, 1020 | PORT, SAV-H1 | Uses path.string(). Staging is copy-on-write. Sidecars are written to .tmp and atomically renamed. |
| ENGINE | engine/src/storage/sandboxed_io.cpp | 1020 | PLT-H1 | On Windows, the open is reparse-point safe and the final path is checked against the sandbox. |
| ENGINE | engine/src/render/include/litholattice/render/renderer.h | 1020 | GAME-H1 | Adds a virtual setEntityOrigin(). |
| ENGINE | engine/src/render/gl33/gl33_renderer.cpp | 1020 | GAME-H1 | Entity origin is passed via uOffset. |
| ENGINE | engine/src/render/include/litholattice/render/software_renderer.h | 1010 to 1112 | RND-H4, GAME-H1, RND-MT | Adds drawOrder_ and entityOrigin_. Draw state is thread_local. Adds per-mesh row spans and frameScreen_. |
| ENGINE | engine/src/render/software_renderer.cpp | 1010 to 1116 | RND-H4, RND-H3, RND-H2, GAME-H1, RND-MT | Draws front to back. One divide per triangle, incremental edges, and perspective-correct varyings. Near-plane clipping. A persistent band pool transforms each mesh once per frame, then rasterises 32-row strips handed out dynamically. |
| ENGINE | engine/src/render/CMakeLists.txt | 1105 | RND-MT | AVX2 for software_renderer.cpp only, on x86-64 (opt out with `LL_SW_AVX2=OFF`). |
| ENGINE | engine/src/assets/assets_selftest.cpp | 1050 | LOW (symlink) | Prints [SKIP] when Windows lacks the symlink privilege (error 1314). |
| ENGINE | engine/src/input/input_selftest.cpp | 1145 | LOW input timer | The overflow test waits for the real overrun, up to 10 s, instead of a fixed 2.5 s sleep. Same assertions. |
| GAME | game/LithoKraft/src/game/include/lithokraft/game/settings.h | 1153 | JD DISPLAY DEFAULT | The default `displayMode` is now 2 (Borderless); it was 0 (Windowed). |
| GAME | game/LithoKraft/config/settings.json | 1153 | JD DISPLAY DEFAULT | `"displayMode": 0` becomes `2` (Borderless) on TheWorkstation. |
| TOOLS (outside tree) | hitch-logs/run-batch.ps1 | 1153 | BENCH | Benches now pass `--display borderless`; they previously ran windowed at 1280x720. |
| ENGINE | engine/src/render/software_renderer.cpp / software_renderer.h | 1156 | RENDER-THREADS | Band pool default changed from 8 to 4. Adds `SoftwareRenderer::setRenderThreads()` and `renderThreads()`: requests round down to a power of two, are clamped to hardware threads, are capped at 64, and are reconfigured between frames under the run mutex. |
| GAME | game/LithoKraft/src/app/main.cpp | 1156 | RENDER-THREADS | Adds the `--render-threads N` CLI flag (1..64). The CLI wins over settings; the help text is updated. |
| GAME | game/LithoKraft/src/game/include/lithokraft/game/settings.h, settings.cpp | 1156 | RENDER-THREADS | Adds settings key `renderThreads` (0 = engine default), which is loaded and saved. |
| GAME | game/LithoKraft/src/game/game.cpp, include/lithokraft/game/game.h | 1213 | ALT-TAB CLICK BUG | Root cause: a focus pause left `captured_` true, and `routeUiInput` drops every pointer event while captured, so pause-menu buttons were dead and nothing saw the click. Fix: `pointerToUi` now includes `paused_`; focus loss sets `captured_ = false`; a world click (`freePointerDown_`, i.e. a PointerDown no widget consumed) resumes and recaptures. |
| GAME | game/LithoKraft/src/game/include/lithokraft/game/settings.h | 1215 | DITHER OFF | `modelDither` default changed from true to false. This is the single option (cvar `render.model_dither`). The engine renderers already defaulted off, and the code is kept. |
| GAME | game/LithoKraft/src/game/game.cpp, controls_panel.cpp | 1218 | TPS LADDER | F7 steps 32/64/128/256/512 (kSettingsTpsModes) and wraps; it used to stop at 256 (kTickModes). There was no EUV flag gate in the code: the cap came from the 4-entry F7 table. The engine's kTickModes is untouched, so the hash is safe. |

**Layer split.** ENGINE files are under `engine/`; GAME files are under `game/LithoKraft/`. Two findings are MIXED:
- **GAME-H1:** the engine part is renderer.h, software_renderer.*, and gl33_renderer.cpp. The game part is game.cpp uploadEntities.
- **MCP-H1:** the engine part is iworldquery.h and server.cpp. The game part is world_query.cpp.

The ALT-TAB and LAUNCH-FOCUS fixes are also MIXED:
- **Engine:** window.*, glfw_input_backend.*
- **Game:** main.cpp, game.cpp/h

CHUNK-STALL is MIXED too: the engine part is sim_thread.*, and the game part is game.cpp.

No `.lnk` shortcuts pass `--display maximized`. `engine/tools/run-bench-suite.ps1` deliberately sweeps all display modes, so it was left unchanged.

## Afternoon session (after 12:22), 2026-10-15

**JD ruling D-1 (13:32 ET):** the dev key is accepted as the interim beta key. No code changed. The Release fatal-exit check is untouched, and the key was never read or printed.

**Correction (pixel identity):** the earlier "pixel-identical" claims came from `biome_capture`. That tool calls `renderMesh` directly, so it only covers the direct-mesh path, not the multithreaded `renderFrame` chunk pass. The frame path is now covered by the engine_render thread-count test below.

| Layer | File | Time(s) | Finding ID | What changed |
|---|---|---|---|---|
| GAME | game/LithoKraft/src/game/game.cpp | 1240 | SETTINGS-RESET | config.lth no longer overrides settings.json at boot. The stale `client.display_mode "0"` / `render.model_dither "1"` in config.lth was resetting the display to windowed. |
| GAME | game/.../game.cpp, src/app/main.cpp | 1245 | BENCH-WALK | Adds `--bench-path walk`: a ground-level real-play bench, about 1,400–1,450 meshes in the wafer dimension. It is the primary FPS figure. |
| TOOLS | hitch-logs/run-batch.ps1 (outside tree) | 1250 | BENCH-WALK | The batch now includes walk runs, all borderless. |
| ENGINE | engine/src/render/software_renderer.cpp/.h | 1255 | RND-BIN | Per-mesh 32-row strip binning (CSR). Pixels identical. |
| ENGINE | engine/src/render/software_renderer.cpp | 1305 (REVERTED) | RND-SPAN | Analytic row-span clip: no gain, and edge pixels changed. Reverted. A second attempt at 1315 was also reverted. |
| ENGINE | engine/src/render/software_renderer.cpp | 1325, 1345 | RND-CULL | Back-face culling in the chunk pass, matching GL (CCW front). Verified on real walk frames: no holes. Walk at 4 threads: render 64.8 → 54.6 ms. |
| ENGINE | engine/src/render/software_test.cpp | 1335 | RND-TEST | New renderFrame thread-count test: 48 meshes at 1/2/4/8 threads, bit-identical (223,223 lit pixels). Also a ground-trace fast-path equivalence test: 0 mismatches. |
| ENGINE | engine/src/render/software_renderer.cpp | 1312, 1340 | RND-AIDS | Two debug aids, both off unless their env var is set. `LL_SW_PROF=1` prints per-frame phase ms and pixel/triangle counters. `LL_SW_SHOT=<file.ppm>` (with `LL_SW_SHOT_FRAME=<n>`, default 300) dumps frame n once. |
| ENGINE | engine/src/render/software_renderer.cpp | 1320 | RND-PULSE | The ground-pulse emissive is memoized per chunk; it is a pure function of the chunk, so pixels are identical. |
| ENGINE | engine/src/render/include/litholattice/render/ground_traces.h, software_renderer.cpp | 1335 | RND-TRACEFX | `groundTraceEmissiveFast`: away from a line (distance > 0.081), both line sets weigh exactly 0, so the packet math is skipped. Bit-identical to the reference (test: 0 mismatches). Walk at 4 threads: render about 48.6 → 42.5 ms. |
| ENGINE | engine/src/render/software_renderer.cpp | 1350 (REVERTED) | RND-AVX2-ROWS | An 8-row AVX2 coverage loop: bit-identical, but no measurable gain in A/B tests. Reverted; kept as `.reverted-avx2-2026-10-15-1355`. |
| ENGINE | engine/src/world/include/litholattice/world/block.h | 1335 | HSMESH-STABLE | `ContentRegistry::StableDefScope`: while meshing, `blockDef()` reads a thread-local copy of the defs instead of taking a shared lock per voxel. The copy is refreshed when the epoch, block count or storage changes. Idea: JD. Refinement: Cortana. |
| ENGINE | engine/src/render/mesher.cpp, include/.../mesher.h | 1340 | HSMESH-VERIFY | Meshing uses the stable copy. Verify mode (on by default in Debug, `--verify-mesh` in Release) meshes twice, once stable and once through the locked live registry, and compares FNV-1a checksums. |
| GAME | game/LithoKraft/src/app/main.cpp | 1345 | HSMESH-VERIFY | Adds the `--verify-mesh` flag and prints a summary at exit. |
| GAME | game/LithoKraft/src/app/perf_headless.cpp | 1320 | HSMESH | Mesh-rate runs now use 1/2/4/8/all threads. |
| MIXED | engine renderer.h, mesh_input.h, sim_thread.cpp/.h; game game.cpp/.h | 1350 | HSMESH-PUBLISH | Publish at the phase boundary. Each `flushDirty()` tags its snapshots with a batch id and size. The game holds a batch until it is complete, then uploads it in one frame (batches of up to 64 sub-chunks; bigger ones use the budgeted path). |
| GAME | game/LithoKraft/assets/packs/pack_{16,32,64}/textures/** | 1340 | TEX-ROUTEIN | CloudAtlas refresh128 v3b set routed in: wafer main set plus r2 copper swap plus r2 edge-extended trace and torch pieces. 615 existing files overwritten; list in hitch-logs/texture-routein-2026-10-15.txt. Backup: assets/packs.orig-2026-10-15-1340.zip. New names (ores, alumitrace, seedor, trace_power) are not registered yet. |
| ENGINE | engine/src/render/include/litholattice/render/trace_ramps.h (new), software_renderer.cpp | 1345, 1350 | TRACE-BOX | Software per-pixel path: Cutout and Trace strip quads alpha-test at 0.5 (discard, no depth write). This fixes the black box: alpha-0 texels were drawn opaque. Strip quads are tinted through the r3 power ramps (off = #5A5556 red / #535659 blue) per CloudAtlas SHADER_NOTE. Texture arrays have no atlas, so gutters don't apply; the r2 edge-extended RGB covers filtering. |
| GAME | assets/packs/pack_{32,64}/textures/blocks/pulsor_top_firing.png, tone_block_front_playing.png | 1400 | TEX-ROUTEIN | Restored the original animated strips. The refresh128 versions are single frames, and their .png.json sidecars expect 6 / 4 frames; this had failed 2 lithokraft_selftest checks. |
| ENGINE | engine/src/procgen/litho_stamp_feature_selftest.cpp | 1405 | ELFIE-§7 | wafer.json lookup no longer depends on the working directory: it checks LITHO_SOURCE_ROOT and walks up every parent of the cwd. Fixes engine_litho_formula, which was a cwd bug, not an environment one. |
| GAME | game/LithoKraft/src/app/interchange_capture.cpp | 1405 | ELFIE-§7 | /home/hatch paths at lines 126/177/178 replaced by interchangeSourceRoot(): hatch first, then LITHO_SOURCE_ROOT, then a walk up from the cwd. |
| GAME | game/LithoKraft/src/game/sprite_host_selftest.cpp | 1405 | ELFIE-§7 | userFilesDir(): added a LITHO_USER_FILES override and %USERPROFILE%\workspace\user\files. Existing fallbacks unchanged. |
| ENGINE | engine/src/render/mesher.cpp | 1410 | ELFIE-§2.1 | jobDefs reserves kMeshPadVolume up front, so the reference jobDef() returns can never dangle. |
| ENGINE | engine/src/render/software_renderer.cpp | 1415 | ELFIE-§1.4 | SwBandPool: a nested run() runs inline instead of deadlocking on runM_. Band exceptions are caught, every band still decrements remaining_, and the first exception is rethrown after all bands finish. |
| ENGINE | engine/src/render/software_renderer.cpp | 1420 | ELFIE-§1.2 | The flat/sprite path (renderFlatQuads) clips near-plane-crossing quads (homogeneous w >= 1/128) instead of dropping them. Quads fully in front are unchanged. |

**Audio segfault (05:11, Release):** the 05:11 ctest log shows engine_audio crashing with SegFault at 0.68 s, before any output. Root cause is a stack overflow: AudioEngine holds a 96,000-float tap ring (about 384 KB) plus synths and queues, and the selftest builds it on the stack. In Release, inlining merges several test frames that each hold one, which overran Windows' default 1 MB stack. Debug doesn't inline, which is why only Release crashed. The 0504 `/STACK:8388608` link option fixes it (dumpbin on the current Release litholattice_tests.exe shows an 0x800000 reserve). The 0511 run used a binary linked before that change. Re-verified at 14:00: engine_audio Release passes 20/20 serially and in a full -j8 run, and lithokraft_selftest passes in Release. The full audio thread audit (miniaudio device lifetime, the SPSC queue, the tap ring) is in progress.
| ENGINE | engine/src/audio/include/litholattice/audio/audio_engine.h | 1450 | AUDIO-T1 | The 384 KB tap ring is heap-allocated at construction (was inline, making the object ~400 KB on the stack). Root cause of the 05:11 Release SegFault. render() still never allocates. |
| ENGINE | engine/src/audio/streaming.cpp | 1450 | AUDIO-T2/T3 | workerId_ is set by play() on the game thread (it raced with readers). `c.last` is captured before the move. |
| ENGINE | engine/src/render/software_simd.cpp (new), software_simd_avx2.cpp (new), include/.../software_simd.h (new), CMakeLists.txt, software_renderer.cpp | 1455 | RND-AVX2-DISPATCH (Elfie R3§6) | No translation unit is compiled with /arch:AVX2 any more, so the binary runs on any x86-64 CPU. AVX2 kernels (colour and depth clear fills) use per-function target attributes and are picked at runtime by CPUID+XGETBV. `LL_SW_FORCE_SCALAR=1` forces the baseline path. `-DLL_SW_AVX2=OFF` drops the AVX2 TU. The active path is logged once. Proof: biome_capture PNG SHA-256 identical on both paths (7/7); scenario hash aa82e7afb1eff9d3 on both; walk at 4 threads: 21.4 FPS baseline vs 19.6 avx2 (noise; no regression from dropping the TU-wide flag). |
| GAME | assets/packs/pack_{16,32,64}/textures/** (additions) | 1550 | TEX-ROUTEIN | New-name textures added, not yet referenced: 14 ores plus raw icons, alumitrace(+emissive), copper_ore, seedor_purple/orange, trace_power s00..s64 tiles (r3 off tiles), r3 palettes red/blue_trace_ramp. Registering the blocks is deferred: new block ids touch saves, worldgen palettes and possibly the content CRC, so it isn't low-risk this close to handoff. |
| DOC | R:\repos\LithoLattice-Workflow\Thread-Ownership-Audit-2026-10-15-1510.md | 1510 | THREAD-AUDIT | Engine-wide thread ownership, handoffs and locks. Fixed T-1..T-6; flagged F-1..F-8 (not changed). |

**Trace black-box check (visual):** a software-renderer frame of the circuits bench scene (frame 60, LL_SW_SHOT) shows the Trace pieces tinted, with no black boxes. File: hitch-logs/trace-fix-circuits-crop.png.

### 14:00–14:30 ET (Cortana 2026-10-15 1600/1610/1625)
| Tag | File(s) | Time | ID | Change |
|---|---|---|---|---|
| MIXED (JD idea) | engine/src/render/mesher.cpp, include/.../trace_ramps.h (traceMixPulseColor), software_test.cpp | 1600/1625 | PULSE-TRACEMIX | The ground pulse colour follows the mesh's red/blue Trace mix: all blue gives #2FB4FF, all red gives #FF514B, a mix gives the count-weighted mean, rescaled to the biome colour's peak; a mesh with no Trace keeps the biome colour. It is computed once per (re)mesh from the StripTint0/1 quad counts, so it updates only when that sub-chunk's blocks change. It rides pulseP1, so the GL33, VK and software backends all get it. Render-only. Hash aa82e7afb1eff9d3 unchanged; biome_capture 7/7 identical to the prior build (those scenes contain no Trace). Unit test: none/red/blue/mix all pass. Note: the count is per 16^3 sub-chunk mesh (the pulse's existing granularity), not per column. |
| ENGINE | frame_pacer.h | 1610 | FPS-PACE | The pacer sleeps to 2 ms before the deadline, then yield-spins on steady_clock (needs no timeBeginPeriod). |
| ENGINE | app/main.cpp, settings.h/.cpp, settings_cvars.cpp, app/selftest.cpp | 1610 | VSYNC-CAP | Software play presents with swap interval 1 (vblank) by default; `--no-vsync` opts out. `--fps-cap 30\|60\|off\|auto` (settings key fpsCap / cvar render.fps_cap: -1 auto [new default], 0 off, N). Auto measures avg render over frames 61..180 and caps at 60 if ≤14 ms, else 30 (logged "fps cap: auto -> N"). `--bench` forces vsync off and the cap off. The pacer never feeds the sim. Selftest expectations were updated for the -1 default (1238 PASS / 0 FAIL). |
| ENGINE (+GAME accessors) | software_renderer.h/.cpp (drawStatusStrip, meshesDrawnLastFrame), game.h (creativeMode, debugOverlayToggled, meshQueueDepth), main.cpp | 1610 | DEBUG-OVERLAY | **Debug Overlay**: a one-line strip across the top showing FPS, frame ms (current plus 1% low over 240 frames), TPS/target, dropped ticks, render threads, meshes drawn, mesh queue depth, and the bench path plus elapsed time during a bench. It is drawn into the software framebuffer after renderFrame and after the screenshot grab, and only for that present: the 18 covered rows are saved and restored straight after present, so screenshots, LL_SW_SHOT, the MCP/Daemon capture (which uses its own renderer instance) and the next frame never contain it. Gate: Debug builds, `--bench` (forced on), or creative mode, toggled with F3 (the existing ToggleDebug). Survival can't show it (build/mode check, not just a hidden key). In survival only the minimal "FPS n TPS n" counter is allowed (settings key fpsCounter, default off). There is no survival mode yet, so Game::creativeMode() returns true; it is the single hook to flip when survival lands. Software backend only; on GPU backends the old F3 HUD panel still applies. |

**Regression r1555 (pre-1600 code, textures in):** Release and Debug builds OK. Hash aa82e7afb1eff9d3 in both Release and Debug. Debug ctest 46/48 (only the env-known systerian and storage). Benches at 512 TPS: walk 4t 21.1 FPS / p99 68.2; walk 8t 30.3 / 45.5; orbit 4t 22.4 / 56.1; orbit 8t 33.8 / 35.6; stream 4t 311 / p99 5.0.
**Post-1625:** Release ctest 46/48 (same 2 env failures). Hash unchanged. Selftest 1238/0. rq mix1625: walk 4t 19.3 / p99 78.4; walk 8t 27.9; orbit 4t 21.2; orbit 8t 31.0 (overlay drawn during the bench; within run-to-run noise of the r1555 numbers, recheck in the final regression).

### 14:40–15:25 ET
| Tag | File(s) | Time | ID | Change |
|---|---|---|---|---|
| ENGINE | app/main.cpp | 1440 | OVL-AB | `--no-overlay` (A/B and bench switch). A/B: 3 interleaved 20 s walk runs, 4 threads, 1080p borderless, 512 TPS. Overlay on: avg frame 41.19 ms, present 0.876 ms. Off: 41.64 ms, present 0.787 ms. The overlay costs about 0.09 ms/frame (≈0.2%), well under the 2% bar, so there is no text cache. |
| ENGINE | software_renderer.cpp/.h, software_test.cpp, settings.h, main.cpp (bench JSON) | 1450 | RT-AUTO (JD ruling 14:41) | The render-thread default is now **auto**: the largest power of two ≤ (logical cores − 2), clamped to 2..16 (12 logical → 8). renderThreads 0/absent = auto; `--render-threads N` still overrides. Bench JSON now has `"render_threads"`. This supersedes the 4-thread default. Test: 12→8, 8→4, 4→2, 2→2, 1→2, 18→16, 64→16 (all ok). |
| GAME | game.cpp | 1455 | LOG-EVICT | The "dirty/signal-bearing but no save directory; NOT evicted" warning is now logged once per column. It repeated 1,741 (64) and 1,949 (32) times in 20 s framebuffer benches. Eviction behaviour is unchanged. |
| TOOLS | engine/tests/CMakeLists.txt | 1500 | MC-02 | RESOURCE_LOCK litho_tempdir on engine_storage/assets/mods/world, so tests that share fixed %TEMP% paths never run concurrently. |
| TOOLS | engine/tools/build.ps1 | 1500 | MC-03 | vswhere `-latest -version [17.0,`. The generator follows the found version (17 → "Visual Studio 17 2022", 18 → "Visual Studio 18 2026"; otherwise CMake chooses). TheWorkstation resolves to BuildTools 18.10.12224.181. |
| TOOLS | engine/CMakeLists.txt | 1500 | MC-06 | GLFW pinned to commit 7b6aead9fb88b3623e3b3725ebb42670cbe4c579 (the 3.4 tag), no longer the shallow tag. Reconfigure and build OK. |
| ENGINE (doc) | world_simulation.h, software_renderer.h, audio_engine.h | 1505 | AUDIT F-1/F-3/F-6 | Contracts documented: post-tick callbacks under the block mutex must be non-blocking; SoftwareRenderer setters are owning-thread only, between frames; drainTap has a single caller (McpSensesAdapter, under audioMutex_), confirmed. |
| ENGINE (reverted) | input_thread.h/.cpp | 1445→1500 | AUDIT F-7 | Tried an ordered overflow that never drops release events. It broke the tested contract "queue overflow drops the newest events and counts them" (engine_input) and made that test take 11 s. Reverted to .orig-1445; the attempt is kept as `.reverted-F7-2026-10-15-1500`. It needs a design decision (always deliver key-up vs. the drop-newest contract). |
| GAME | inventory.h, world_save.cpp | 1510 | H-03 | Per-item stack cap `stackCapFor(id)` = min(ItemDef::maxStack, 512); ids with no ItemDef keep 512. Applied in addItem, placeStack and canPlaceStack. player.dat load clamps, with a warning. |
| GAME | game.cpp | 1515 | H-05 | The saved pose is validated: non-finite values, y outside [0, 768) or |x|/|z| > 2^24 fall back to the dimension spawn (warning). Pitch is clamped to ±π/2. |
| GAME | game.cpp | 1515 | H-02 | Using a silica seed now consumes one from the selected stack (inReach-gated, same as Place). Same documented residual as Place: a reachable use the sim rejects still consumes; exact accounting needs a sim→game receipt. |
| GAME | game.cpp | 1515 | H-01 | Pick Block requires `st.inReach`. Creating the block from nothing (addItem) is creative-only; survival only selects an existing hotbar stack. |
| GAME | game_commands.cpp, screenshot_selftest.cpp | 1515/1540 | H-07 | `map` is cheat-flagged as well as session-tier. With cheats off, a full-authority local player is refused; it runs once cheats are allowed. Selftest updated (refused without cheats, runs with cheats). |
| ENGINE | resource_pack.cpp | 1525 | CA-04 | `PackStack::find()` returns `checkedPath(rel)` (canonical, contained, regular file), closing the icon-loader TOCTOU. |
| ENGINE | resource_pack.cpp | 1525 | CA path validation | `isSafePackPath` rejects any backslash, Windows device names in any component (CON/PRN/AUX/NUL/COM1-9/LPT1-9, with or without an extension), and components ending in '.' or ' '. |
| ENGINE | mod_manifest.cpp, mod_loader.cpp | 1525 | CA-05/06 | mod.json is capped at 1 MiB during the read (both readers). The mod scan skips symlinked or junctioned folders, requires each mod dir to canonicalise under the canonical mods root, and requires mod.json to be a regular file. Each skip is recorded in the scan errors. |
| ENGINE | render/include/.../texture_dilate.h (new), gl_texture_array.cpp, vk_texture_array.cpp | 1530 | CA-01 | RGB is dilated into alpha=0 texels before GPU mip generation (GL glGenerateMipmap and VK blit chain). Alpha is untouched. The software renderer is not affected: biome_capture 7/7 identical to px-simd-scalar. The GL/VK visual result is not verified this session (self-closing software benches only). |
| ENGINE | engine/src/render/include/litholattice/render/renderer.h | 1628, 1705 | CA-03, GREEDY | `PackedVertex` carries a 16-bit texture layer: the high byte is the top 8 bits of the old 32-bit `slot` word (`slot:24, layerHi:8`), so the vertex stays 16 bytes. Adds `layerIndex()` and `setLayer()`. Adds `IRenderer::prefersGreedyMeshes()` (default false). |
| ENGINE | engine/src/render/include/litholattice/render/block_materials.h, block_materials.cpp, mesher.cpp (Look), software_renderer.cpp (varyings) | 1628 | CA-03 | `FaceMaterial::layer` and the mesher Look layer are uint16. The software vertex stage reads `layerIndex()`. |
| ENGINE | engine/src/render/gl33/gl33_renderer.cpp, engine/assets/shaders/chunk.vert, engine/assets/shaders/vk/chunk.vert, engine/src/render_vk/vk_pipelines.cpp | 1628 | CA-03 | New `aLayerHi` attribute (byte 15; GL location 3, VK location 4); the shaders build `vLayer = aInfo.w \| aLayerHi << 8`. |
| ENGINE | engine/src/assets/include/litholattice/assets/texture_registry.h | 1628 | CA-03 | `kMaxLayers` goes from 256 to 2048 (the common GL_MAX_ARRAY_TEXTURE_LAYERS). |
| GAME | game/LithoKraft/src/game/game.cpp | 1628, 1658, 1705 | CA-03, F-2, GREEDY | The Deceitor part layers are uint16 via `setLayer`. MCP senses read a per-frame atomic tick copy (`mcpWorldTick_`) instead of the sim triple buffer. Chunk meshing sets `ctx.greedy` from the renderer. |
| GAME | game/LithoKraft/src/game/include/lithokraft/game/game.h | 1658 | F-2 | `std::atomic<uint64_t> mcpWorldTick_`. |
| ENGINE | engine/src/sim/include/litholattice/sim/sim_thread.h | 1658 | F-2 | Debug-only single-reader check on `SimThread::stats()`: the first thread binds, and later calls assert they come from the same thread. |
| ENGINE | engine/src/audio/include/litholattice/audio/audio_engine.h | 1658 | F-4 | `musicVoiceCount()` and `effectsVoiceCount()` (fixed at construction, so they are thread-safe). `music()`/`effects()` are documented as TESTS ONLY. |
| GAME | game/LithoKraft/src/game/sound_director.cpp | 1658 | F-4 | Uses the voice-count accessors; there is no game caller of `music()` left. |
| ENGINE | engine/src/world/include/litholattice/world/block.h | 1658 | F-5 | `ContentRegistry::instanceId_` (process-unique, never reused) is part of the StableDefScope cache key. |
| ENGINE | engine/src/render/mesher.cpp, include/litholattice/render/mesher.h | 1705 | GREEDY | Greedy meshing. Full-cube Tex/Emissive/GroundPulse faces with no rotation, a full-tile UV, uniform AO and no slot merge per slice, up to 15x15 blocks. A 1x1 result is byte-identical to the unmerged face. Applies only when `MeshContext::greedy` is set (`LL_MESH_GREEDY=0/1` overrides). Trace, strip, readout, palette, cutout, glass and animated faces never merge. |
| ENGINE | engine/src/render/include/litholattice/render/software_shaders.h, software_renderer.h, software_renderer.cpp | 1705 | GREEDY | `swTile()`: u/v values in [0,1] are bit-exact as before, and larger values wrap per tile. The software backend returns `prefersGreedyMeshes() = true`. |
| ENGINE | engine/src/render/software_renderer.cpp, include/litholattice/render/software_renderer.h | 1715 | HIZ | Hierarchical-Z: an upper bound on depth per 8x8 tile, with lazy re-tightening of dirty tiles. Shaded triangles skip whole 8-row tile bands whose tiles across the bbox all lie in front of the nearest vertex (1e-4 relative margin). Reset on clear(); disabled after resize or SSAA depth re-fill until the next clear(). Tiles align with the 32-row band strips, so there is no cross-thread sharing. LL_SW_PROF prints hiz_rows_skipped. |
| ENGINE | engine/src/render/software_test.cpp, engine/tests/engine_tests.cpp | 1628, 1705, 1715 | TESTS | Tests for the CA-03 layer round trip, Hi-Z draw-order independence (with 3-tile UVs), and greedy floor merging (576 faces become 16 quads, same area, max u 240; an unmergeable cube is byte-identical). |
| — | — | — | CA-03 (deferred) | Texture layer index 8→16 bits is NOT done. PackedVertex is a packed 16-byte struct (u,v,faceAo,layer,value,mode are all 8 bits, plus a 32-bit slot), so widening means 18 bytes or stealing 8 bits of `slot`. Either touches the mesher, the software rasterizer, the GL/VK attribute layouts and shaders, and the mesh checksum. Layers in use: 199 of 256 (57 headroom; the 363 new unreferenced files don't load). Proposed: slot→24 bits plus layerHi 8 bits, in its own change with a perf A/B. |
| — | — | — | H-06 held, H-04 deferred | As instructed. |

**LCD screen torture (framebuffer scene, Release, software, 512 TPS, auto threads = 8, flyover, 20 s, borderless 1080p):**
- 64×64: 20 FPS, frame avg 51.8 ms, p99 60.7 ms, TPS 512.0/512, 0 dropped ticks, tick 0.81/1.09 ms (avg/p99), 9,675 blocks, 0 hitches.
- 32×32: 21 FPS, avg 46.1 ms, p99 56.5 ms, TPS 511.9–512.2, 0 dropped, tick 0.62/0.79 ms, 4,755 blocks.
- Headless framebuffer sim: 512 TPS achieved, tick 0.22 ms avg / 0.95 ms p99.
- Animation: frame PNGs `hitch-logs/lcd-64.png` and `lcd-32.png` show the lamp bands lit. The selftest proves the shifting (framebuffer: stage-3 cell changed 167 times / lamp flashed 167 times in 2,000 ticks; the incremental re-flood matches a full pass every tick for 1,500 ticks; the pattern wall 64×64 has 0 dark and 0 wrong-colour pixels).
- Determinism: hash aa82e7afb1eff9d3; component-sleep 0 differing ticks.
- Bugs found: the evict-warning spam (fixed); `blue_via`/`red_via` models reference unknown faces (side_bottom/mid/single/top; pre-existing, not fixed).

**Checks after 1540:** hash aa82e7afb1eff9d3. Release ctest 45/48 before the H-07 test update; screenshot selftest passes after it; the remaining failures are systerian and storage (environment). Selftest 1238/0 (at 1520). rq ca1535 (auto threads): walk 4t 21.2 FPS / p99 66.6; walk 8t 29.8 / 47.2; orbit 4t 22.9 / 61.9; orbit 8t 32.7 / 37.7.


### Final regression, batch final1530 (15:08 to 15:29 ET)
Batch `final1530` (started 15:08 ET, finished 15:29 ET), final code, Release and Debug builds OK:
- **Hash:** `aa82e7afb1eff9d3` in both Release and Debug.
- **Debug ctest:** 46/48. The 2 failures are environment-only: `engine_systerian` (its user files are outside the repo) and `engine_storage` (symlink privilege). **Release ctest** (15:35 run, plus the screenshot selftest re-run after the H-07 test update): 46/48, same two.
- **Release selftest:** 1238 PASS / 0 FAIL.
- **Benches** (Release, software, 1080p borderless, auto threads = 8 unless noted):

| Path | TPS | FPS | p50 / p99 (ms) | render (ms) | hitches |
|---|---|---|---|---|---|
| walk | 512 | 30.8 | 32.9 / 45.1 | 31.3 | 0 |
| walk | 32 | 31.7 | 31.4 / 44.5 | 30.4 | 2 |
| orbit | 512 | 33.3 | 29.8 / 36.0 | 28.6 | 2 |
| orbit | 32 | 35.6 | 28.1 / 34.7 | 26.9 | 1 |
| stream | 512 | 288.0 | 3.0 / 9.5 | 1.6 | 10 |
| walk, --render-threads 8 | 512 | 29.8 | 33.6 / 47.6 | 32.3 | 0 |
| orbit, --render-threads 8 | 512 | 34.4 | 29.2 / 35.9 | 27.9 | 1 |

The sim held 511.7–511.9 TPS (minimum) in every run. For comparison, at 12:22 walk on 4 threads was 14.9 FPS with a 94.6 ms p99. The 4-thread figures from earlier today (r1555): walk 21.1 FPS, orbit 22.4 FPS.
- **Pixels:** the 7 biome_capture PNGs are SHA-256 identical to the AVX2/scalar reference (px-simd-scalar).
- **LCD screen torture (framebuffer scene, 512 TPS, 8 threads):** at 64×64, 20 FPS, p99 60.7 ms, 0 dropped ticks; at 32×32, 21 FPS, p99 56.5 ms, 0 dropped.

### Batch 16:25 to 17:50 ET (CA-03, F-2/4/5/8, greedy meshing, Hi-Z), regression final1730
- **Hash:** `aa82e7afb1eff9d3` in Release and Debug. It did not move at any step (it was also checked after CA-03 and after greedy).
- **Debug ctest:** 46/48. **Release ctest:** 46/48. The only failures are the known environment ones, engine_systerian and engine_storage. The new tests pass: CA-03 round trip, greedy floor merge plus 1x1 identity, Hi-Z draw-order independence.
- **Release selftest:** 1238 PASS / 0 FAIL. Debug mesh-verify: 0 mismatches.
- **Pixels:** biome_capture (default, non-greedy meshes) gives 7/7 SHA-256 identical to px-simd-scalar after CA-03, swTile and Hi-Z. With `LL_MESH_GREEDY=1` the 6 biome PNGs change: spawn differs in 16 pixels and trace_forest in 13 (0.002%; max channel delta 68). These are isolated texel-boundary rounding flips, not cracks. The overview map is identical. Greedy plus Hi-Z equals greedy alone, 7/7.
- **Benches** (Release, software, 1080p borderless, auto threads = 8, 512 TPS unless noted). The 15:29 final1530 figure comes first, then this batch:

| Path | FPS before -> after | p99 ms before -> after | render ms before -> after |
|---|---|---|---|
| walk | 30.8 -> 45.5 | 45.1 -> 31.5 | 31.3 -> 20.8 |
| walk, 32 TPS | 31.7 -> 47.2 | 44.5 -> 30.4 | 30.4 -> 20.0 |
| orbit | 33.3 -> 45.0 | 36.0 -> 28.0 | 28.6 -> 21.0 |
| orbit, 32 TPS | 35.6 -> 48.8 | 34.7 -> 25.4 | 26.9 -> 19.4 |
| stream | 288 -> 424 | 9.5 -> 3.8 | 1.6 -> 1.3 |
| walk, rt8 | 29.8 -> 45.6 | 47.6 -> 31.0 | 32.3 -> 20.8 |
| orbit, rt8 | 34.4 -> 46.2 | 35.9 -> 26.1 | 27.9 -> 20.4 |

  Step by step (rq walk/orbit at 8 threads): before CA-03 the figures were about 30.8/33.3. After CA-03: 31.8/35.4 (neutral; CA-03 is a format change only). After greedy: 35.0/38.7. After Hi-Z: 46.7/45.7. The sim held at least 511.7 TPS in every run.

### Lithography blocks, 17:55 to 18:45 ET (JD approved 16:31): registration and render only
Backups for every edited source file: `.orig-1755`. The pack `resources.json` files: `.orig-1800`. The PNGs are new files. Source art: `lithography_r2/r3_2026-10-15.rar` (verified byte-identical to the box copy, 123 PNGs). Face map: r3 REGISTRATION-NOTES.md.

| Layer | File | Time(s) | Finding ID | What changed |
|---|---|---|---|---|
| GAME | game/LithoKraft/src/content/blocks.cpp, include/lithokraft/content/blocks.h | 1755 | LITHO-REG | 8 blocks appended, append-only, so no id moves and saves stay compatible: 74 `lithokraft:selector` SelectOR, 75 `connector` ConnectOR, 76 `laysor` Laysor (solid+transparent+directional, not opaque), 77 `emitor` Emitor, 78 `input_buffer` Input Buffer, 79 `fabor` FabOR, 80 `autofabor` AutoFabOR, 81 `vior` Vior (neutral arm run). All have no behavior and SimRole::None; the Etchor logic is the next pass. The EtchOR (57) and the red/blue Vias already existed and are not duplicated. Creative inventory: they appear automatically (the palette lists the whole registry). |
| GAME | game/LithoKraft/assets/packs/pack_16, pack_32, pack_64: resources.json, textures/blocks, textures/items, textures/ui, textures/ui/icons | 1800 | LITHO-ART | 34 block textures (r2: autofabor, buffer, connector, etchor x5, fabor, selector, vior blue/red/run; r3: emitor x4, laysor_crystal x3). The superseded r2 laysor_* art is skipped. Also the 2 r3 item textures, the `etchor_name_field.png` UI texture, 8 inventory icons, and per-face models for the 8 new blocks. The EtchOR model goes from `all: etchor` to per-face (front chip out, back wafer in, sides laser port, top blue in, bottom red in). vior_blue/red_* are registered as textures; the existing Vias keep their art. manifest.json was not regenerated (resources.json was edited directly). |
| ENGINE | engine/src/render/include/litholattice/render/block_appearance.h, engine/src/render/mesher.cpp | 1755 | LITHO-HOOK | New `IBlockAppearance::faceLookCell(cell, slot, active)` (default nullopt): a neighbourhood-aware face look. The mesher tries it before faceLookAt. It is stateless and safe on mesh workers. |
| GAME | game/LithoKraft/src/content/appearance.cpp, include/lithokraft/content/appearance.h | 1755 | LITHO-LOOK | The Laysor renders as kShadeAlphaBlend (blended, not cutout and not screen-door). The Emitor front aperture is kShadeEmissive. Beam as light: a Laysor reached through a run of Laysors by an Emitor front facing it gets value 1, a violet glow. The EtchOR side face the beam enters through turns emissive. No geometry is added. |
| ENGINE | engine/src/render/software_renderer.cpp, include/litholattice/render/software_renderer.h | 1755 | LITHO-BLEND | Blended chunk pass on the software per-pixel path. StoredMesh::hasBlend is set at upload. Inside each 32-row strip, opaque quads draw first (front to back), then the strip's AlphaBlend quads (far to near over meshes that have them). Blending is src*a + dst*(1-a) with the texel alpha: depth-tested, no depth write, no Hi-Z dirty mark. A strip belongs to one thread, so the read-modify-write is race-free. Meshes without blend quads take the unchanged path. |
| ENGINE | engine/assets/shaders/chunk_shading.glsl | 1755 | LITHO-GL | GL chunk mode 12 (AlphaBlend) falls back to the glass screen-door branch. There is no sorted GPU chunk blend pass yet (open item). |
| GAME | game/LithoKraft/src/content/content_selftest.cpp | 1755 | TESTS | The materials-block assert now chains to kSelectOR. New checks: ids 74-81 are dense and namespaced, have no behavior, and the Laysor is see-through. The Laysor face is AlphaBlend and the Emitor front is emissive (back plain). Selftest went from 1238 to 1240 PASS. |
| TOOLS | game/LithoKraft/src/app/biome_capture.cpp, hitch-logs/make-package.ps1 | 1755, 1845 | LITHO-SHOT | `biome_capture <dir> --litho` renders a lithography test scene (litho_blocks.png) and exits; the default run is unchanged (pixels 7/7). The package script also copies the screenshot. |

- **Hash:** `aa82e7afb1eff9d3`, unchanged after registration.
- **Release ctest:** 46/48. **Debug ctest:** 46/48. The only failures are the known environment ones, engine_systerian and engine_storage.
- **Release selftest:** 1240 PASS / 0 FAIL.
- **Pixels:** the 7 default biome_capture PNGs are SHA-256 identical to px-simd-scalar.
- **Short bench** (rq, Release software, 1080p): walk 47.6 FPS (rt8; p99 30.1 ms), orbit 48.7 FPS (rt8; p99 25.6 ms). For rt4: walk 33.5, orbit 33.9.
- **Screenshot:** `hitch-logs/litho_blocks_2026-10-15.png`. It shows the Emitor, then 3 lit blended Laysors, then the EtchOR; one unlit Laysor; and the SelectOR, ConnectOR, Input Buffer, FabOR, AutoFabOR, Vior, blue Via and red Via. The floor shows through the Laysors.
- **Open:**
  - The beam trace reads only the mesh pad (+-1 block past the sub-chunk), so a beam that crosses a 16-block seam stops lighting there. The first test scene showed this; the shot keeps the beam row inside one sub-chunk.
  - Emitor power gating and all Etchor logic and wiring canon come next pass.
  - Laysor quads inside one mesh are not depth-sorted.
  - The GPU uses screen-door, not true blending.
  - The flat (non-per-pixel) software path draws the Laysor opaque.
  - The EtchOR name field is a stub: texture registered, flow still the `etch <name>` console command.
  - The EtchOR lit port faces away from the shot camera, so it is not visually confirmed.

### Etchor logic: mask-and-expose machine, 18:50 to 19:25 ET (after the 1845 package)
Backups: `.orig-1900` for every edited file. The new files are lithography.{h,cpp}, litho_scene.cpp and docs/Lithography-Machine-2026-10-15.md.

| Layer | File | Time(s) | Finding ID | What changed |
|---|---|---|---|---|
| GAME | game/src/game/lithography.h, lithography.cpp (new), game/src/game/CMakeLists.txt | 1900 | LITHO-LOGIC | Machine scan in JD's canon. EtchOR centre. Top: 3 ConnectORs 2 above, blue Via between. Bottom: 5 ConnectORs 2 below, red Via between. Back Input Buffer (front facing the EtchOR) holds the wafers in its value bits, 0..64. The front cell must be air (chip exit). Lasers enter from the sides: a lit Emitor facing inward through one or more Laysors to an outer top ConnectOR. ConnectORs are fed by strongly powered non-structure neighbours; the middle top one takes lit Trace dust on top. Expose = a laser plus at least one fed top bit. Mask = the 5 bottom bits. Code = top | bottom<<3. Pure reads through LithoReader, called between sim ticks; nothing touches the signal tick. |
| GAME | game/src/game/game.cpp, include/lithokraft/game/game.h, game_commands.h, game_commands.cpp | 1900 | LITHO-CMD | `etch <name>` within 6 blocks of an EtchOR goes through the machine: it refuses with the diagnosis unless formed, exposed, holding a wafer and clear in front; otherwise it takes one wafer from the back buffer and puts the named ChipOR into the inventory. With no EtchOR near, the B6 inventory flow is unchanged. New `etchscan` (diagnosis) and `etchload [n]` (wafers from the inventory into the back buffer). Game::lithoReader is for the sim thread between ticks only. |
| GAME | game/src/content/blocks.cpp, signal.cpp, appearance.cpp | 1900 | LITHO-EMITOR | Emitor power toggling: the Emitor is now SimRole::Lamp (lit on adjacent power, bit 0). The lamp writer keeps a DIRECTIONAL plain lamp's facing bits (fast and slow paths). Non-directional lamps only hold 0/1, so their states are unchanged and the hash held. The aperture glows and the beam fires only while powered. |
| GAME | game/src/content/appearance.cpp | 1900 | ELFIE-LOW-2 | The EtchOR beam-emissive port applies to the Left/Right side slots only (Elfie's 1845 review). |
| ENGINE | engine/assets/shaders/chunk_shading.glsl | 1900 | ELFIE-LOW-1 | Named constant `MODE_ALPHABLEND = 12` replaces the bare `vMode == 12`. |
| GAME | game/src/content/litho_scene.cpp (new), circuit_scene.h, CMakeLists.txt, game.cpp, app/main.cpp | 1900 | LITHO-SCENE | `--bench-scene litho` builds an UNWIRED machine 10 blocks south of the spawn, EtchOR facing you: all blocks and feeders, 8 wafers, a torch row to wire from, no Trace. `--bench-scene litho-wired` adds a wired reference copy 16 blocks east (mask 10101, both lasers lit, middle fed by Trace). |
| GAME | game/src/app/main.cpp | 1925 | BUG-FOCUS | Bug: test/game windows stole cursor and focus when launched. Cause: main calls `raiseAndFocus()` on first show (glfwFocusWindow, plus a topmost toggle to beat the Windows foreground lock) and captures the cursor even in bench runs, which also made the window topmost. Fix: in `--bench` runs, no raiseAndFocus, no cursor capture and no topmost. Normal play is unchanged. |
| GAME | game/src/game/wiring_selftest.cpp, content/content_selftest.cpp | 1900 | TESTS | 8 new checks on the real SignalSim, all pass: wired machine formed, lasers L+R, expose 111, mask 10101, code 175, 8 wafers; unwired machine dark and refusing; missing blue Vior names the reason; nearest-EtchOR radius; buffer count clamping; powered Emitors lit with facing kept. The Emitor is checked as a Lamp whose aperture lights only when on. |
| DOCS | docs/Lithography-Machine-2026-10-15.md | 1900 | LITHO-DOC | Canon, front/side/back ASCII diagrams, test scenes, open items. The wax/coolant/no-shrink upgrade is recorded as APPROVED, NOT IMPLEMENTED (waiting until the rest of the mechanics are worked out); the failure behaviour without wax or coolant is an open question for JD. |

- **Hash:** `aa82e7afb1eff9d3` in Release and Debug, unchanged.
- **Release ctest:** 46/48. **Debug ctest:** 46/48. The only failures are the known environment ones.
- **Release selftest:** 1248 PASS / 0 FAIL.
- **Pixels:** 7/7 identical to px-simd-scalar.
- **Bench** (software, 1080p, 512 TPS):
  - rt8: walk 44.9 FPS (p99 30.6 ms), orbit 48.0 FPS (p99 24.7 ms).
  - rt4: orbit 33.9; walk 27.4, measured while JD was using the machine, so noisy.
- **Smoke:** `--bench-scene litho` and `litho-wired` both build and exit cleanly.
- **Debug build caveat:** it predates the 1925 main.cpp edits (scene-name validation and the focus fix). The Release build has them.
- **Open:**
  - The chip goes into the inventory; there are no dropped items at the front yet.
  - The chip code is not stored on the item (that would be a save-format change; JD's call).
  - The SelectOR's role in the machine is not defined yet.
  - Buffers are loaded through `etchload` (no automatic feed).
  - The beam glow does not cross sub-chunk seams; crossing them needs beam data in the mesh input, so it is not cheap and was not done. The logic itself reads 16 blocks.
  - Focus: a new Win32 window may still activate when it is created. A further option is GLFW_FOCUS_ON_SHOW / GLFW_FOCUSED = false for bench runs (engine window hint), not done.
## 2026-10-15 20:01-21:10 — 3D voxel foliage (GAME/TOOLS), hash pins, review, overnight tree  // Cortana 2026-10-15 2110

### MORNING SUMMARY (for JD)
- Main tree `LithoLattice-LithoKraft-Full-Source-2026-10-15` + build\Release (20:43 build): 5 Tron-style foliage kinds, ids 82-86. Hash **6976b32954d906cd**. Left as is.
- Overnight tree `LithoLattice-LithoKraft-Full-Source-2026-10-15-overnight` (own build\, merge candidate): the review fixes, plus kind 6 Routing Moss (id 87, dead_zones). Hash **7794dcfd2b943361**, stable over 2 runs.
- Review: hitch-logs\Code-Review-Since-1845-2026-10-15-2050.md (severity ranked).
- Screenshots: hitch-logs\foliage-overnight-2026-10-15\foliage_3d.png (+ icon_foliage_*.png); main tree shot: hitch-logs\foliage-2026-10-15\foliage_3d.png.
- Open: foliage costs ~6 ms/frame in the software renderer (see Bench). Needs foliage LOD / quad reduction. Not done.

### Hash history (Release; each value stable over 2+ runs)
- aa82e7afb1eff9d3 (19:24, Etchor) -> 46d112e955ed5764 (20:15). Cause: the foliage_3d worldgen feature. Proof: blocks 82-86 with the
  pre-foliage wafer.json reproduced aa82... (20:25). wafer.json is embedded at build time. **JD sign-off 20:34**: pinned.
- Tron model reshape (shape only, 20:36): stayed 46d112e955ed5764. No model-to-sim coupling.
- foliage_3d moved before the structures (20:40) to fix lithokraft_scout_selftest (never-overwrite): 6976b32954d906cd (content change, recorded per JD 20:34 policy).
- Overnight + Routing Moss in dead_zones: 7794dcfd2b943361 (content change, recorded).
- `--renderer software` vs `--renderer vulkan` with --handshake-scenario give the same hash. The scenario is headless, so this only proves the flag doesn't leak.
- The 38 control/input/event/timing source files are byte-identical to the 1845 handoff.
- No hash constant exists in the source (the pin lives in logs/CHANGES only).

### Foliage design (render-side instances + deterministic worldgen)
- [GAME] content/foliage.{h,cpp}: models authored as level-4 (16^3/block) SparseOctree via model::ModelBuilder,
  meshed once with the engine micro_mesher (meshOctreeLOD), crystal core via litho_formula voxelizeLithoShape.
  One shared model + quad list per kind (magic static). An instance = its block cell only; the variant (quarter turn)
  comes from a position hash. Assets: game/LithoKraft/assets/models/foliage/foliage_<kind>_<i>.lthm (loaded first,
  generator fallback).
- [GAME] blocks 82-86 (cattail 2 tall, bush = routing-mound, fern = H-tree, reeds = bus lines, crystal_shrub = bond-wire loops
  + via array), Shape::Custom, non-solid, no sim role. appearance.cpp meshCustom branch; glow material (41) drawn emissive.
- [GAME] wafer.json feature `foliage_3d` (groundcover, after shrubs_dry_tuft): cattail capacitor_groves 0.03 (no water in game:
  stand-in wetland), bush trace_forest 0.04, fern trace_forest 0.03, reeds dicing_plains 0.03, crystal resonator_spires 0.02,
  (overnight) moss dead_zones 0.04.
- [GAME] Placeholder textures foliage_m32-m42 in all 3 packs (CloudAtlas to replace) + icons + resources.json model entries.
- [TOOLS] biome_capture `--foliage` (render icons + scene) / `--foliage-write-assets` (overnight: also rewrites .lthm).
- Memory: shared octrees 5-25 KB per kind (87.5 KB total for 6), quad lists 1.2-6.1 KB per kind. Per instance: 0 extra bytes (the cell).
- In-game: worldgen biomes above, or the creative palette ("Cattails", "Berry Bush", "Fern Clump", "Reed Tuft", "Crystal Shrub", "Routing Moss").

### Overnight tree fixes (review items)
- M1 appearance.cpp: foliage slot names built once (no per-cell string allocations).
- H2 (partial) appearance.cpp: bottom faces at the cell floor are culled over solid ground (the capture's 16384-quad trim no longer occurs).
- M2 content_selftest: shipped .lthm assets must equal the generator.
- M3 biome_capture: asset writes only with --foliage-write-assets; stats printed after all assets are written.
- L1 removed the unused shrubs.h include.
- Moss kind (id 87): foliage.h/.cpp, blocks, appearance range, selftest (6 dense ids), resources.json, wafer.json, icons.
- Backups: *.orig-2026-10-15-2100 / -2110 in the overnight tree.

### Verification
- Main tree 20:43: selftest 1252/0, ctest 46/48 (known env: engine_systerian, engine_storage).
- Overnight 21:05: selftest 1253/0 (incl. 36 litho/Etchor, saves, Trace, foliage x5), ctest 46/48 (same 2 env failures).
- Bench (software, rt8, 12 s, default res): pre-foliage 19:24 walk 44.9 / orbit 48.0 FPS.
  Overnight: foliage on 29.7 ms (walk) / 29.7 ms (orbit), foliage meshing disabled (A/B) 23.7 / 23.3 ms; final with moss 30.2 / 30.1 ms (p99 39.9 / 36.2).
  So foliage costs ~6 ms/frame (~25%); the rest of the gap vs 19:24 is attributed to machine load (not proven).
- Mouse capture: normal mode code-verified (only gated by benchSeconds<=0); not live-tested.
- Note: game/LithoKraft/saves/world/level.json appeared in the MAIN tree (a run with cwd = tree); excluded from the overnight copy.
## 2026-10-15 21:55 — Overnight tree merged into the main tree (JD approved 21:50)  // Cortana 2026-10-15 2155
- [MIXED] Copied from ...-overnight into ...-Full-Source-2026-10-15: the review fixes (M1, H2-partial, M2, M3, L1) and the Routing Moss kind (id 87).
  Files: appearance.cpp, blocks.cpp/.h, content_selftest.cpp, foliage.cpp/.h, biome_capture.cpp, 3x resources.json, wafer.json (moss
  entry), assets/models/foliage/foliage_moss_0.lthm, moss icons (3 packs). 11 overwritten files backed up as *.orig-2026-10-15-2150.
- Main build\Release rebuilt: hash **7794dcfd2b943361**, the same on 2 runs (matches the overnight tree). Selftest 1253/0. ctest 46/48 (known env:
  engine_systerian, engine_storage). The overnight tree is left in place.
## 2026-10-15 22:25: Controls + Repeater pass (JD 22:07 decisions D1-D9)
// Cortana 2026-10-15 2225: backups *.orig-2026-10-15-2215 (code) and -2225 (docs)
- [ENGINE] sim_thread.h kReach 8 -> 16 (engine max; the game gates per mode). The control-stream/EditCmd format is unchanged.
- [GAME] Reach: survival 5, creative 16 (D5); PickBlock uses the same gate.
- [GAME] Hold-to-repeat: break every 250 ms, place every 200 ms onto a new target only, in all modes (D3).
- [GAME] Sneak + right-click always places (D1). New action Secondary (LeftAlt): Alt + right-click sends AltUse to ANY component, which fixes the Thresholdor/radio/jammer settings bug (D2). Listed in the Controls panel.
- [GAME] C is now a crawl toggle (game-side; forced crawl unchanged) (D8).
- [GAME] Walk-bridging: when looking down with nothing in reach, place at the edge under the feet via kCmdBridgePlace (server-validated: air, solid neighbour, canPlace). New cvar game.bridging, default 1 (D9).
- [MIXED] Repeater delays are now 2/4/8/16 (D4). New encoding uses state bit 0x2000 + aux index. Legacy saves migrate lazily to the nearest delay (1-3->2, 4-6->4, 7-8->8). 16-bit delay line in history+timer (same save layout). Top-face click picks a button (kCmdRepeaterSet); side click cycles.
- [GAME] Repeater placeholder voxel model: slab, 4 buttons, active button glows kGlow, pips. Swappable: assets/models/repeater/repeater_<0-3>.lthm overrides it (for CloudAtlas). Block is non-opaque now.
- [GAME] Selftests: added fresh=2, cycle 2->4, delay 4 vs 2 = +2 ticks, delay 16 vs 2 = +14 ticks, and the legacy migration table.
- Deferred: D8 kneel (Z) needs a new control-stream player flag bit, so it is a control-format change; it needs JD's OK. D6 ramps/stairs are specced only. D7 liquids.
- Verify: Release build OK; selftest OK (0 failures); ctest 46/48 (known systerian/storage env failures).
- HASH (content-driven, D4 repeater): 7794dcfd2b943361 -> e015e51cf1559089, stable on 2 runs.

## 2026-10-15 22:45: Reach 8 + kneel by triple sneak tap (JD 22:36) + prayer/static/charge audit (JD 22:38)
// Cortana 2026-10-15 2245: code backups *.orig-2026-10-15-2240, docs -2245
- [GAME] Survival reach is now 8 (creative stays 16), for both reach gates in game.cpp.
- [ENGINE] Kneel pose (LocomotionPose::Kneel, appended so existing values keep their numbers): height 1.2, eye 1.0, sneak speed, no edge guard. LocomotionController::step derives it from the sneak bit's false->true edges: 3 taps within 0.6 s of step time latch kneel; the next tap, a jump, crawl or Fly releases it. NO control-format change; it is a pure function of the input sequence and dt. A sneaking body that can't fit now drops Sneak -> Kneel -> Crawl. Note: collision is whole-voxel, so with whole-block ceilings Kneel fits the same gaps as Sneak (1.5); the difference only shows on sub-block heights. The Z binding was not added (the triple tap replaces it). HUD shows "kneel".
- [ENGINE] engine_locomotion selftest: 2 taps do not kneel, 3 latch, next tap releases, slow taps don't kneel.
- Prayer NOT implemented (it needs new save fields and sim-side locomotion); see the spec section "Prayer, Static, Charge".
- Verify: Release OK; selftest ALL OK; ctest 46/48 (known systerian/storage). HASH e015e51cf1559089 x2: UNCHANGED (locomotion is not in the handshake hash).

## MORNING SUMMARY (overnight 2026-10-15 22:50 -> 23:20, JD delegated) // Cortana 2026-10-15 2320
Backups: *.orig-2026-10-15-2255 (prayer), -2310 (foliage merge), CHANGES -2320.
1. [GAME] Digital prayer, BUILT.
   - Kneel (triple sneak tap, derived in the locomotion body from the sneak bit, no control-format change) held for more than 64 ticks = 1 prayer.
   - Reward: max(1, 2% of the static cost of the next level), via GamePlayerAdapter::grantStatic. Max 3/day, one per kneel. Logs "prayer: +N static".
   - Day = 24000 ticks. The world tick is NOT saved (it restarts every session), so the day runs on a player-owned play clock persisted in player.dat. Otherwise reloading would reset the cap.
   - Runs in the per-sim-tick locomotion loop on the game thread (where the body and vitals already advance). It is tick-driven and deterministic, but it is not on the sim thread: locomotion itself isn't in the sim yet, and moving it there is a larger refactor (open).
   - [MIXED] player.dat v5 -> v6: appends u64 playTicks, u32 prayerDay, u8 prayersToday. v1-v5 still load (zeros). prayer.h is pure logic.
   - [GAME] The registered "kharge" stat range is now -64..+64 (the kharge.h morality meter); old 0..64 values load unchanged.
   - Selftests: 7 prayer tests + v6 round-trip + v5 backward load.
2. [LORE] /workspace/lithokraft-lore/Cortana-Wafer-Features-2026-10-15.md on the box: oxide fields, alignment marks, slip lines, probe scars (gameplay/look/placement/texture), credit Cortana, approved JD 18:47.
3. Vulkan push (TheWorkstation, RTX 3050 Ti, 1080p windowed, 10-12 s self-closing benches; logs hitch-logs\vulkan-push-2026-10-15\):
   | bench | before avg / p99 ms | after avg / p99 ms |
   | Vulkan walk | 1.25 / 3.48 | 1.27 / 3.41 |
   | Vulkan orbit | 1.37 / 4.22 | 1.32 / 3.63 |
   | Vulkan stream | 0.94 / 6.73 | (not re-run) |
   | Software walk | 33.1 / 41.5 | 33.7 / 43.9 |
   | Software orbit | 32.3 / 37.4 | 32.6 / 38.4 |
   - Vulkan is already ~750-800 FPS on the real Wafer: the 60+ target is met with a large margin. Software (~30 FPS) is the slow path.
   - [GAME] Foliage: lossless coplanar quad merge on the shared kind meshes. Only 1475 -> 1461 quads (1%, the micro-mesher already merges), so no measurable gain. Kept because it is lossless.
   - Real foliage LOD needs distance at mesh time (chunk meshes are distance-independent). Proposal: a level-3 mesh for chunks beyond 32 m, re-meshed on a ring change, or software-renderer culling of far Custom quads. Not done tonight.
   - Vulkan GAPS found in the log (features, not perf): "setPbrMaterials not implemented" and "uploadEntityMesh not implemented: entities invisible".
4. Verify: Release OK; selftest ALL OK; ctest 46/48 (known systerian/storage). HASH e015e51cf1559089 on 2 runs: unchanged (prayer, kneel and the mesh merge are outside the hash). Hash is headless, so it is renderer-independent.
Review notes (open): grantStatic adds onto the effective (base+boost) value, harmless until static boosts exist; the play clock only advances while playing (days = play-time days, by design); the merged-quad UVs assume position-derived UVs, so do a quick visual check of foliage.

## 2026-10-16 04:39-05:08 - JD morning batch: player.lat, saved world clock, 32-min day, mealtime prayer, foliage LOD, Vulkan entities  // Cortana 2026-10-16 0508
Backups: *.orig-2026-10-16-0456 (27 source files + CHANGES.md). Workers: Cortana only (0 of the 3 allowed), so there are no helper credits.
1. [GAME] The player save is now **player.lat** ("litho algorithmic type"), same binary format. readPlayer falls back to a legacy player.dat when the world has no player.lat footprint. The next save writes player.lat, which wins from then on. The legacy file is left untouched, so older builds still open the world. Comments, log text and tests are renamed. SAVE_FORMAT.md is updated.
2. [GAME] The world clock is **saved per world** in level.json v2 as "worldTick" (sim ticks ever run) and "worldTime", both decimal strings. Game restores it on load and writes it on every save and save-on-evict. Migration: a world with no saved clock and a v6 player takes playTicks as the clock (assuming 32 TPS). Otherwise the clock starts at day 0, 00:00. The 2026-10-15 play clock is removed.
3. [GAME] A day is **32 real minutes** (world_clock.h). TPS is selectable from 32 to 512, so each tick adds 512/TPS units of 1/512 s. That gives exactly 32 real minutes at every rate: 61,440 ticks/day at the default 32 TPS (2,560 per game hour), and 983,040 at 512. The 24 h clock (HH:MM) shows on the F3 TPS line.
4. [GAME] **Mealtime prayer**: the windows are 09:00-09:32, 13:00-13:32 and 17:00-17:32 game time. A prayer needs more than 64 consecutive kneel ticks inside a window. Ticks before the window opens don't count. Each window pays at most once per day (3/day max), and the reward is unchanged at 2% of the next level's cost, minimum 1. Each window lasts 32 game minutes = **42.67 real seconds = 1,365 ticks at 32 TPS**. JD's "32 seconds" was taken to mean the in-game :00-:32. player.lat v7 holds u32 prayerDay + u8 window bitmask. v6 saves load with no windows prayed. Tests rewritten: 12 prayer/clock checks, plus v7 round-trip, legacy v5/v6 .dat loads, .lat migration and the level.json clock round-trip.
5. [GAME/ENGINE] **Foliage LOD**: every kind has a cross-hatch variant (foliage::crossMesh): two crossed centre planes carrying the model's silhouette, greedy-merged. All 6 kinds together are 1461 detail quads -> 556 cross quads. The mesher emits both sets, tagged in the top 2 bits of PackedVertex::mode (kShadeLodNear 0x40 / kShadeLodFar 0x80; mode mask 0x3F). Each sub-chunk picks a set by its horizontal centre distance (> 32 blocks = far). On the GPU, GL33 and VK chunk.vert collapse the unused set. Software keeps a near set and a far set per mesh, and its far set skips foliage entirely. biome_capture uses the near set. Render-only.
6. [ENGINE] **Vulkan entities**: uploadEntityMesh/setEntityOrigin are implemented. Each frame slot streams its own grow-only host-visible buffer, drawn non-indexed through a new double-sided "entity" pipeline (chunk format) after the chunks. No 4096-vertex cap like GL33. The "entities invisible" warning is gone. PBR on Vulkan is still a follow-up and not cheap: it needs a chunk_pbr pipeline and a material table.
Verify (Release, after review fix): build OK; selftest 1287 PASS / 0 FAIL; ctest 46/48 (known engine_systerian, engine_storage). HASH **e015e51cf1559089** on 2 runs: UNCHANGED. The clock, prayer, file name and LOD are all outside the handshake hash; the LOD shape change did not move it.
Bench (RTX 3050 Ti, 1080p windowed, 10 s self-closing, --no-save; hitch-logs\lod-entities-2026-10-16\):
   | bench | before avg / p99 ms | after avg / p99 ms |
   | Vulkan walk | 1.12 / 3.13 | 1.33 / 3.78 |
   | Vulkan orbit | 1.20 / 4.03 | 1.35 / 1.92 |
   | Software walk | 26.7 / 31.7 | 16.3 / 20.3 |
   | Software orbit | 24.7 / 31.3 | 16.6 / 20.5 |
   Software is about 37 -> 61 FPS (far foliage skipped). Vulkan gave up about 0.2 ms (still ~750 FPS), from the extra LOD vertices plus the entity pass.
Review (one pass at the end) found 1 issue, fixed and re-verified: a save-on-evict before the first full save could rewrite level.json without the clock. The clock is now armed at load and before each eviction save.
Open / for JD: visual check of the far cross-hatch and of Vulkan entities (no VK validation layer installed; the benches have no item drops). The live save/reload smoke wasn't run because bench mode doesn't save and a normal run would take focus; selftests cover the round-trips. A pack-overridden chunk.vert would need the same 2-line LOD mask (no pack overrides it today).

## 2026-10-16 (Cortana): GUI overhaul, IVehicle/IGlider stubs, -or rename
Backups: `*.orig-2026-10-16-0517` (GUI + vehicles), `-0526` (-or rename), `-05xx` (assets/models/*.lthm, final game.cpp/UI.md).
### GUI: crisp, DPI-aware, aspect-correct
- theme.h / painter.h: `Theme::snapTextScale` / `pixelTextScale`. Glyphs are drawn only at integer texel scales (the title's 4.5x was stretching them).
- hud_ui_renderer.cpp / overlay_geometry.cpp: glyph origins, rect edges and clip rects snap to whole pixels.
- gl33/software_presenter.cpp: the blit is GL_NEAREST (was GL_LINEAR, which blurred the software renderer).
- GUI scale: Auto, 1x, 2x, 3x, 4x (was 0..2). Auto = clamp(floor(min(fbW/960, fbH/540)), 1, 4) on the physical-pixel framebuffer, so it is DPI-aware. A fixed scale is capped so a 640x360 layout fits (`GameUi::effectiveGuiScale`). settings / cvars clamp to 0..4.
- Bench-only screenshot tooling: `--ui-shot FILE --ui-view menu|settings|pause|hud`.
- Selftests: auto scale, fixed-scale cap, integer glyph scale. The HUD scale-2 test moved from 1600x1000 to 1920x1080 for the new policy.
- Docs: UI.md.
- Screenshots: hitch-logs/gui-2026-10-16/{vulkan,gl33,software}_{1280x720,1920x1080,2560x1080,1024x768}_{menu,settings,hud}.png
### IVehicle / IGlider (stubs, no content)
- New engine/src/sim/include/litholattice/sim/vehicle_interfaces.h: VehicleControls (pressed bits plus clamped axes only), VehicleState (1/256 fixed point), IVehicle (onMount/onDismount/onTick/thrustFor), IGlider : IVehicle (liftFor/glideRatio).
- C mirror: `LithoLatticeModVehicleDef` in mod_interface.h.
- Docs: SDK.md "Vehicles". Covers authority (the tick owns movement, there is no position-from-client path) and the note that player movement still runs on the game thread.
- wiring_selftest: determinism contract check. Nothing calls the interfaces, so the hash is unchanged.
### -or casing (JD rule)
- Every `XxxOR` became `Xxxor`, plurals included (ChipOR→Chipor, EtchOR→Etchor, ConnectOR→Connector, SelectOR→Selector, ...). This covers 413+ sites in display names, model names, identifiers, docs and comments.
- Registry IDs were already lowercase (`lithokraft:etchor`), so no load aliases were needed.
- assets/models: bashor, chopor, debuggor, heapor, pickor, seedor, slicor and turtlor .lthm were regenerated with author_models, because they embed the model name.
### Verify
Build OK. Selftest ALL OK (1294 pass). ctest 46/48 (baseline: engine_systerian, engine_storage). Hash e015e51cf1559089 on 2 runs, unchanged.

## 2026-10-16 (Cortana): Ginny's perf/determinism list, items a-c + SW tiers (proposal credit: Ginny)
Backups: `.orig-2026-10-16-0550` (a), `-0552` (b), `-0603` (c), `-0608` (span experiment, reverted). Benches: `hitch-logs/perf-2026-10-16/`.
- (a) FP pinned. `litholattice_fp_strict()` in engine/CMakeLists.txt sets /fp:precise (MSVC) and -ffp-contract=off (GCC/Clang). It is applied to the sim, world, entity, ai, projectile and procgen targets.
- (b) ai_world: the AI tick accumulator is an exact int64 remainder (`acc += simTicks*hz; while (acc >= worldTps)`), replacing the double.
- (c) Greedy meshing already existed (mesher.cpp, software only). It is now enabled for Vulkan and GL33 (`prefersGreedyMeshes()`). chunk_shading.glsl repeats the tile for merged UVs > 1 (`lithoWrapUV`), samples with textureGrad on raw-UV derivatives (no mip seams), and leaves faces with UV <= 1 unchanged. Foliage cross-hatch and cutouts never merge, so the LOD is unaffected. Greedy off/on screenshots look identical.
- SW tiers: tile binning (32-row band strips plus per-mesh CSR bins) and hierarchical Z (8x8 tiles) already exist. AVX2 exists only for clears. The span-setup experiment (analytic per-row span bounds) cut scanned px/frame 3.09M -> 0.97M but gave no FPS change, because shading dominates (690k shaded px/frame). It was reverted. AVX2 span shading is not done: the real cost is the per-pixel shader(), which would need an 8-wide shader rewrite.
- FPS (10 s benches, 1280x720): base -> after c. Software walk 60.6 -> 67.1, orbit 56.9 -> 64.2 (software was already greedy, so this is mostly run-to-run noise). Vulkan walk 758 -> 943, orbit 739 -> 866 (Vulkan noise is about ±10%).
- Verify: hash e015e51cf1559089 on 2 runs after every step. Selftest ALL OK; ctest 46/48 (baseline).

## 2026-10-16 (Cortana): Linux build check + AVX2 8-wide span shading (proposal credit: Ginny)
- Linux build: a source-only copy in /workspace/litho-linux on the box (Debian 13, GCC 14.2, Release, Xvfb). Built with 0 errors and needed no source fixes.
  - Handshake hash e015e51cf1559089 on 2 runs, the same as Windows.
  - Selftest ALL OK (1297 PASS). ctest 47/48: only engine_systerian fails; engine_storage passes on Linux.
- AVX2 span shading. New `swsimd::ShadeSpanBasic` and `shadeSpanBasicAvx2` (software_simd*.cpp/h), with a hook in rasterizeTriangleShaded.
  - It covers the common case only: basic shading or a PBR fallback, a plain texture, no dither, no ground FX, and no strip/cutout/blend mode. Everything else uses the scalar loop.
  - It is bit-exact by construction: the same sequential edge stepping, the same op order, no FMA, and truncating converts.
  - It needs AVX2 at runtime and is opt-in with `LL_SW_SIMD_SHADE=1`. Default is OFF because it measured no faster (slightly slower in orbit).
- Validation: biome_capture scalar vs AVX2 gives 15/15 images byte-identical on Windows and Linux (changed px 0, max error 0, mean error 0). A debug colour build showed the kernel shades about 22% of game frames and 20-28% of captures.
- Backups `.orig-2026-10-16-0643` and `-0645`. Diff images and stats are in hitch-logs/avx2-2026-10-16/.

## 2026-10-16 07:13 ET: Standards review and bug-fix run (approved by JD)
Backups: `*.orig-2026-10-16-0713` (28 files). The changes were first verified on Linux (/workspace/litho-linux), then ported after checking every Windows file still matched the base (SHA-256).
- Naming: interfaces renamed to the I-prefix form: ISoftwarePresenter, IAudioOutput, ISimSystem, IStdioTransport, IBtNode (18 files). SeedOR -> Seedor. alpha-docs: the leftover -OR spellings fixed (44 files, box-only).
- GUI: the F3 controls panel now sits above the hotbar (hud_small style). The menu tagline is on a dark box (menu_tagline style). The main menu is hidden while Settings is open and restored on close (dialog Close, closeSettings, and a menu rebuild).
- Bug fix: a rejected Place no longer uses up an item. The sim now reports each game Place edit back to the game (EditCmd.x = kPlaceReceipt, SimThread::drainPlaceReceipts), and Game::refundRejectedPlaces gives back the item a rejected place took.
- Clock: clockSynced_ is reset when a new SimThread is created, so the old sim's worldTick is never diffed against the new one's.
- Prayer: a clock that goes back a day marks that day as already prayed (no re-pray exploit). If level.json's clock is behind player.lat's prayerDay, the clock starts again at 00:00 of that day. Two new selftests.
- IVehicle stubs: VehicleControls::clamped() (unknown bits dropped, axes held to -127..127) and the engine mount gate tryMount/dismount (refuses rider 0 and a seat already taken). Nothing calls these yet.
- Verified on Windows: build OK; hash e015e51cf1559089 on both runs; selftest ALL OK (1296 PASS); ctest 46/48 (engine_systerian and engine_storage fail, same as baseline).
- Verified on Linux: hash e015e51cf1559089 on both runs; selftest ALL OK (1299 PASS); ctest 47/48 (engine_systerian only).
- Open: walk-bridging still uses up an item when rejected (the sim gives no result for it); player.lat stats and vitals are not validated; the naming questions for Master Chief (Lithoscript spelling, ComponentBehavior, procgen Node, TextEditBase, SignalPropagationPolicy).
