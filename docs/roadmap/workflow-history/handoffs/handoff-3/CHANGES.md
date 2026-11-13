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