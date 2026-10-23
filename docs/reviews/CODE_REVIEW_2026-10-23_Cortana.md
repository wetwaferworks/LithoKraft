# Code Review 2026-10-23 (Cortana)

Review and fixes by Cortana; seed list by Elfie; chaired by JaydeeWetwork

## Scope
I reviewed the whole tree on main: engine/ (LithoLattice), game/LithoKraft, wasm/ and ILithoGui/ (read only). I only read host/lithocef, the CEF layer, which is Elfie's lane. I did not open the Release signing check, any key material, seat tokens or _private folders. The robotics safety bar applied throughout. I also ran sweeps for races and atomics, off-by-ones and underflow, nondeterminism (rand), unchecked decode, and the sqrt sites.
Every build ran in a scratch copy (C:\Users\jdwetwork\review-scratch-2026-10-23\tree) using VS 18 2026 CMake, x64 Release. Nothing is committed. Each edited file has a *.orig-2026-10-23-review backup next to it.

## Build / test results
| | Build | ctest |
|---|---|---|
| Before | FAILED: C3861 glVertexAttribDivisor at gl33_renderer.cpp:958, plus the counter collision (see R-CNT-1) | nothing runnable |
| After (loader fix) | OK | 46/48 passed (-j4). An earlier -j8 run: engine_sim hit the 600 s timeout but passed standalone in under 90 s |
| After (all fixes) | OK | 46/48 passed. Procgen determinism hashes unchanged |

The 2 remaining failures:
- **engine_systerian**: needs ../../../../user/files/cistercian, which isn't in the tree. Environmental.
- **engine_storage**: the symlink sandbox tests fail on Windows without Developer Mode or admin. This is a test-harness bug: the tests should skip when a symlink can't be created.

## Issues
| ID | Sev | File:line | Description | Status | Fix |
|---|---|---|---|---|---|
| R-BLD-1 | P0 | engine/src/render/gl33/gl_loader.h | VertexAttribDivisor was missing from the loader X-table, so the native build broke. Elfie's loader change apparently wasn't synced | Fixed | Added the X entry and the #define |
| R-CNT-1 | P1 | game/LithoKraft/CMakeLists.txt:15 | The game's 4-wheel counter writes build_counter.txt. On Windows that is the same file as JD's 8-wheel BUILD_COUNTER.txt, so every Windows build overwrites it and then the JD counter fails (seed P2-6) | Reported | Needs JD's decision on the counter name. No counter written by me |
| R-DET-1 | P1 | game/LithoKraft/src/game/game.cpp ~2650 | The item-drop toss used rand(), which is nondeterministic | Fixed | splitmix64 hash of the event position and value. vy stays 2.5 |
| R-PG-1 | P1 | engine/src/procgen/stages.cpp:188-203, 240 | biomeWeight is laid out [i*biomeCount+b], but these sites indexed it with stride min(biomeCount, perBiome.size()). If the biome table is larger than the stage table, the wrong weights are read | Fixed | Stride is now biomeCount. Hashes unchanged in the tests |
| R-RACE-1 | P1 | game/LithoKraft/src/app/main.cpp:1665 | On Windows the MCP thread is detached, then mcpServer.reset() runs. That is a use-after-free if the thread is still blocked in its stdin read | Reported | CancelSynchronousIo + join |
| R-NET-1 | P2 | engine/src/net/protocol.cpp | Duplicate entity ids make the gap underflow, so the encoder silently emits a packet the decoder rejects | Reported | Reject or dedupe in the encoder |
| R-NET-2 | P2 | engine/src/net/protocol.cpp | When truncated, the removals block can push output past maxBytes | Reported | Budget the removals block |
| R-NET-3 | P3 | engine/src/net/protocol.cpp | decodeHeader doesn't validate the PayloadType enum | Reported | Range-check it |
| R-SIM-1 | P2 | engine_sim / timing_handshake.cpp:571 | Wall-clock watchdog plus a ctest -j8 timeout: the run is timing-sensitive under load | Reported | Investigate whether this is a hang or contention |
| R-TST-1 | P3 | engine_storage selftest | Symlink tests fail where they should skip | Reported | Skip when symlink creation is denied |
| R-LOG-1 | P3 | gl33_renderer.cpp:73 | Stale "(base-vertex draws)" log line (seed P2-5) | Fixed | Now reads "rebased-index draws" |
| R-DOC-1 | P3 | anim iksolve.cpp:16 | Stale comment about core normalize() | Reported | - |

## Seed items (Elfie)
- **P0-1 (Reported).** Suspects:
  - (a) chunk.vert and chunk.frag (and the wasm/stage copies) have no LK_MEGADRAW branch, so the mega program is the classic shader and uOffset is never set.
  - (b) multiDrawElementsInstancedWEBGL is called with instanceCounts=1 and no baseInstance, so every sub-draw reads instance record 0. It needs gl_DrawID or the base_vertex_base_instance extension.
- **P0-2:** MEGADBG is confirmed absent.
- **P1-3 (Deferred).** --stream-radius and --radius already give 10/8. The diamond footprint needs a new area shape (sim_thread.h:78) that changes determinism hashes, plus streamer ordering (chunk_streamer.cpp:216). It can't be tested on the phone from here.
- **P1-4:** noted.
- **P2-5:** fixed.
- **P2-6:** reported as R-CNT-1.
- **P2-7/8:** noted.

## Safety notes (robotics)
- R-DET-1 and R-PG-1 were nondeterminism and silent wrong-data paths. Both are fixed.
- R-RACE-1 is a shutdown use-after-free, and R-SIM-1 is timing-sensitive behaviour under load. Both should be resolved before the engine drives any real actuator.
- On the encoder side of the net protocol (R-NET-1/2), failures are silent. Fail loudly instead.
- camera.cpp:18 uses sqrtFast. Confirm camera pose never feeds picking or robot plans.
- texture_generator.cpp:325 uses sqrtFast in baked content. Check whether that counts as deterministic content.
- Percentile helpers cast p*n to size_t, which is UB on NaN. Add a guard.

## Sqrt audit
Audit by Elfie under Jenny's rules; brought by Archie
- cull_prototypes.h:168/252: these are normalizations, marked EXACT and pinned by selftest. No change.
- micro_mesher.h:155: a squared form was reverted before, and the linear contract is pinned. Not landed.
- math.h normalize callers: all are presentation code and show no measurable gain. Not landed.
- software_renderer sqrtFast sites 1052/2065: presentation only. Confirmed OK.
- PBR EXACT sites: untouched.
- Result: **0 conversions landed**, so no bench numbers were needed and every hash is unchanged.

## Disclosure
My first build ran in the real tree. The game's counter wrote "0.0.1.1" into BUILD_COUNTER.txt (case collision). I restored it to the HEAD value 0.0.0.0.0.0.1.16, with an approved retry, and git shows it clean. engine/build-review/ is a build dir I created during that attempt. I left it in place rather than delete files.

## Open items for JD
- Decide on R-CNT-1, the counter naming.
- Fix R-RACE-1.
- Look into R-SIM-1.
- Elfie: P0-1 shader sync / gl_DrawID, and P1-3.
- R-NET-1/2/3.
- engine_storage skip.
- systerian test data.
- Remove engine/build-review/ if unwanted.
- Pre-existing uncommitted changes from others (gl_web_shims, lk_webio.js, lithocef, wasm/full) are not mine.


## Fix run 2 (2026-10-23, JD-approved 2:21 PM ET)

The changes are uncommitted on main. Each edited file has a `.orig-2026-10-23-fix2` backup. Verification was a Windows Release build of the scratch copy, re-synced after Elfie's landings 2 and 3, followed by `ctest -j8`: **48/48 passed**. A later run passed 48/48 again, and the sim, net, storage and mcp tests had 0 failures in 4 more -j8 repeats.

| ID | File(s) | Change | Test proof |
|---|---|---|---|
| CNT-1 | BUILD_COUNTER.txt/.log, tools/build_counter.py, engine/CMakeLists.txt | Engine counter set to Litho Engine 0.0.0.0.0.0.1.19. Builds pass `--no-bump` (opt in with LL_ENGINE_COUNTER_BUMP). The parser accepts a "Litho Engine " prefix. | After the scratch build, BUILD_COUNTER.txt and its log were unchanged. The real tree still reads 0.0.0.0.0.0.1.19. |
| CNT-2 | game/LithoKraft/CMakeLists.txt, cmake/bump_build_counter.cmake, build_version.h, LITHOKRAFT_BUILD_COUNTER.txt, LITHOKRAFT_VERSION.txt | The game has its own rolling decimal counter and a separate public label. The build fails if the counter name matches build_counter.txt ignoring case. Documented in docs/versioning/COUNTERS.md. | The build printed "Alpha 0.0.1 build 1" and the game selftest passes. |
| R-RACE-1 | mcp/server.h, mcp/server.cpp, app/main.cpp, mcp_selftest.cpp | MCP shutdown: set the stop flag, call CancelSynchronousIo on the blocked `_read`, join, then free. The old code detached and freed. If the join times out after 2 s, it logs, detaches and leaks the server on purpose, so there is no use-after-free. | The mcp test "blocked stdin reader cancelled + joined before free" passes x8/8, and the stop flag resets. |
| R-NET-1 | net/protocol.h/.cpp, net/snapshot.h/.cpp | encodeSnapshot refuses duplicate ids in `current` and a baseline that isn't strictly ascending, with an explicit error. The snapshotter counts it, logs it and forces a full snapshot. | engine_net R-NET-1 x2 PASS |
| R-NET-2 | net/protocol.cpp, net/snapshot.cpp | Removals are packed only up to maxBytes. The rest are deferred and kept in the post-image baseline, and `truncated` is set. | R-NET-2 PASS (a 3000-removal snapshot packs to 195 bytes, under the 200-byte limit, and decodes) |
| R-NET-3 | net/protocol.h/.cpp | decodeHeader rejects payload types other than 1..5 or >=256 (mod), and reports which type it got. | R-NET-3 PASS (rejects 0, 6 and 255; accepts 1, 5, 256 and 4000) |
| R-SIM-1 | engine/tests/CMakeLists.txt | The run 1 timeout did not reproduce in 6 -j8 runs (about 1.7 s each), so it was contention, not a hang. Added a 300 s TIMEOUT guard so a real hang fails fast. | ctest json shows TIMEOUT 300; engine_sim passes |
| R-TST-1 | engine/tests/engine_tests.cpp | Symlink checks are skipped and print [SKIP] when the host can't create symlinks (Developer Mode is off). | engine_storage passes and prints 3 [SKIP] lines |
| R-TST-2 | systerian/CMakeLists.txt, systerian/testdata/cistercian/ | Vendored the 39 reference masters into the repo, so the test no longer depends on an outside path. | engine_systerian: "selftest: OK (0 failures)" |
| SQRT-1 | texture/texture_generator.cpp:325-326 | sqrtFast replaced by std::sqrt, because baked textures are deterministic content (Jenny's rules). | texture tests pass; no golden hash changed |
| SQRT-2 | camera/camera.cpp:18 | `length()` now uses std::sqrt. It only feeds the FOV kick, but the effective FOV reaches MCP senses (agent and robot vision). It runs once per frame. | camera tests pass |
| DOC-1 | anim/iksolve.cpp:16 | Stale comment fixed. core normalize() now has its own +Y guard; the local copy stays for its 1e-9 threshold. | comment only |
| P0-1 | gl33 mega-draw | Report only, as JD asked. The suspects from run 1 still stand. | n/a |

The sqrt audit was done by Elfie under Jenny's rules and brought by Archie.

**Determinism hashes:** the goldens pass. No golden moved from fix run 2. Elfie's diamond change defaults to square, so the default-config hashes did not move. The diamond footprint has no golden of its own yet.

### Review of Elfie's diamond and cvar landings (2 and 3): findings only, not changed (her lane)
- **D-1, off-by-one:** every site uses an inclusive `|dx|+|dz| <= r`: random_ticks.cpp:36, sim_thread TickArea, game.cpp:1050 count, chunk_streamer.cpp:178/193, and eviction at :223 against `r + hysteresis`. They agree. Eviction ordering is deterministic (farthest first, then key).
- **D-2, coverage gap:** the streamer keeps `max(tick, stream)` resident, but in its own shape. With `render.stream_shape=diamond` and `sim.area_shape=square` or `round`, the corners of the tick area, up to (r, r) at Manhattan distance 2r, are not resident. Suggest the streamer use the union of the two footprints, or force a square stream whenever the sim area isn't diamond.
- **D-3, validation:** `sim.area_shape` and `render.stream_shape` quietly fall back to "square" on any unknown value, including a typo like "Diamond". Suggest rejecting or warning on unknown values and matching case-insensitively. `render.stream_radius` is clamped to 0..64, which is fine; junk input parses to 0, which means auto.
- **D-4, thread safety:** all three cvars apply only on world load. The values are copied into the SimThread config and ChunkStreamer when they are built (`diamond_`, `radius_`), so the sim thread never reads GameSettings live, and there is no race with console writes. This holds only while they stay load-time settings; a live setter would need an atomic or a handoff at tick boundaries.
- **D-5, replay safety:** the shape is deterministic for a given setting, but neither `sim.area_shape` nor `sim.tickradius` is recorded in the save or replay metadata (grep found nothing). A replay run with a different setting would diverge silently. Suggest stamping both into the world or replay header and checking them on load.

### Unresolved or not done
- No process-level LithoKraft `--mcp-stdio` start/stop run; it needs a GL window. Proof is the in-process R-RACE-1 loop, which runs the real transport on a blocked pipe. ASan and debug-heap runs were not done.
- P0-1 mega-draw: report only.
## Fix run 3 (2026-10-23, uncommitted)

Approved by JD: diamond for both the sim area and the stream area, plus the remaining items.

- **T1 Diamond default:** diamondSimArea and diamondStreamArea now default to true (game.h, settings.h, config/settings.json). The stream radius is `minStreamRadiusCovering(...)`, so the sim area always sits inside the stream area. Both old `max(tick, stream)` sites in game.cpp now use it.
- **T2 Parser:** the new `sim/area_shape.h` is case-insensitive. Unknown values log a warning and fall back to diamond. It covers the cvars, `--sim-area`/`--stream-area` and LL_SIM_AREA/LL_STREAM_AREA. Tests are in engine_tests and selftest.
- **T3 Persistence:**
  - level.json is now v3 with simAreaShape, streamAreaShape and tickRadius. v1/v2 saves keep the configured area. The saved area beats CLI and settings, like the seed, and a mismatch logs a warning.
  - The demo preamble's reserved u16 is now the area word: bit15 = recorded, bits 0-1 sim, bits 2-3 stream, bits 4-13 tick radius. Old demos have 0 and replay with the configured area. New demos replay with the recorded area.
  - Tests are in demo_record_test.
- **T4 Golden:** the diamond area golden is `0xd34ce4e17eae47ac` (9x9 world, r3, 16 ticks, 3200 cells).
- **T5:** engine/build-review was already gone.
- **T7:** mega-draw P0-1 was NOT touched.
- **T8:** Release ctest -j8 passed 48/48 in four repeats plus one -V run.
- **GL 3.3 (Intel UHD):** a 20 s full-game start/stop with save gave a clean exit, and level.json v3 holds diamond/diamond/8. On a restart with `--sim-area SQUARE`, it warned and kept the saved diamond area.
- **ASan (RelWithDebInfo, /fsanitize=address, out of tree):** 42/48 passed. New findings, all in test fixtures, NOT fixed:
  - **ASAN-1** stack-use-after-scope: `ui_selftest.cpp:531,560` `Painter p(r, Theme::defaults());` binds a temporary Theme. This fails engine_ui and lithokraft_selftest. Fix: `const Theme th = Theme::defaults(); Painter p(r, th);`.
  - **ASAN-2** stack-use-after-scope: mods_selftest testExtensionLoaderDepthCap (~l.655). `ExtensionLoader::validateFacet` reads a facet that has gone out of scope. This fails engine_mods.
  - ASan timing and path failures only: engine_anim (riggen 308 ms over the 200 ms bound), engine_index (5 s bound), and engine_litho_formula (wafer.json is not found from an out-of-tree dir).
  - Under ASan, `--demo-test` passes all fix-3 area checks.
- **Pre-existing, not fixed:** the `--demo-test` constant `5041cf1795c33a8c` is stale (see wasm/FULL-TEST-REPORT.md).
- **Cosmetic, not fixed:** worldTick reads 0 after a 20 s GL run, and the area log says "(from settings.json)" even when a CLI flag set it.
- **Counter:** still `0.0.0.0.0.0.1.19`.

## Fix run 3b (2026-10-23, uncommitted)

- **ASAN-1 fixed:** `ui_selftest.cpp` now binds a named `Theme` before constructing `Painter` (2 sites).
- **ASAN-2 fixed:** FakeParent is self-referential (`facet_` points at its own `childApi_`), and moving it into a `std::vector` left dangling pointers. FakeParent's copy constructor and copy assignment are now deleted, and the depth-cap tests hold `std::unique_ptr<FakeParent>`.
- **ASan-only timing and paths:**
  - The riggen (200 ms) and index (5 s) limits are scaled x10 when `__SANITIZE_ADDRESS__` is defined.
  - Every `engine_*` ctest now gets `LITHO_SOURCE_ROOT` (engine/tests/CMakeLists.txt), so out-of-tree builds find wafer.json.
- **Area log:** the line now names the real source of each value: `--sim-area`/`--stream-area`, `LL_*`, or settings.json. The stream area is logged too. When a saved world overrides the area, a second line says "(from the saved world's level.json)". The CLI now wins over the env vars, like `--radius`.
- **worldTick 0 after 20 s:** not a bug. The run sits on the launch menu with the sim paused (`menu: ... sim paused`, tps=0.0/256), so the world clock does not advance.
- **.gitignore:** `*.orig*` added. It does not untrack the `.orig` files that are already tracked in git.
- **Streaming order (JD t2279u):**
  - Nothing in the pipeline was reversed. Requests were already sorted nearest-first, but by Euclidean distance, with ties broken by key. Job pool, ready queue and upload queue are all FIFO.
  - The world-load mesh pass submitted columns in `unordered_map` order, which is arbitrary.
  - Now: requests rank by `streamRequestPriority`, which is |dx|+|dz| on diamond streams (then dx^2+dz^2), with ties broken by key. The load-time mesh pass is sorted the same way around the spawn chunk.
  - Four selftest checks were added. A visual confirmation is still pending.
- **Elfie P1 (PBR on WebGL):** on `__EMSCRIPTEN__`, chunk_pbr is declined before compiling, with one info line, and the ERROR line is native-only. **Proposal (not done):** add an ES material-table route by replacing `samplerBuffer`/`glTexBuffer` with a 2D RGBA32F texture (`texelFetch` with an (id % W, id / W) lookup), so PBR can run on WebGL2.
- **Elfie P2 (AngelScript -7):** this is WASM-only; the native boot log shows "AngelScript 2.38.0 ready". WASM AngelScript is built with AS_MAX_PORTABILITY, which has no native calling conventions.
  - `log` is now bound with `asCALL_GENERIC` when `asGetLibraryOptions()` reports AS_MAX_PORTABILITY.
  - **Follow-up:** the other native bindings (script_console, game.cpp, ui_script_bindings) will fail the same way on web and need generic wrappers.
  - This has not been run on web.
- **Elfie P2/P3 (signal overrun):** the 3 overruns were from `signal.components` (id 1) under ASan `--demo-test`. Diamond membership is O(1): one bbox test plus |dx|+|dz|. Diamond also ticks fewer columns (145 vs 289 square at r8), so it lowers the cost, not raises it. The overruns look like cold-start or ASan cost. No change.
- **Elfie P3:**
  - (i) Missing `glTexBuffer`/`glDrawElementsBaseVertex` on web are now info-level stdout lines in both the loader and the shims.
  - (ii) `side_top/mid/bottom/single` in blue_via/red_via are connected-side variants (MODDING.md "Connected via sides"), not stale names. No geometry is lost: `side` covers every side face. The loader now accepts the keys quietly, which took the warnings from 8 to 0 in the GL boot log. Content is unchanged.
  - (iii) The rolling-counter log line is now plain; the wheel ink goes to a real terminal only.
- **P0 mega-draw:** the diagnostic was dropped per JD; nothing was touched.
- **Windows past glVertexAttribDivisor:** yes. A GL 3.3 boot on Intel UHD loads all entry points and runs and saves cleanly.
- **5041cf pin:** unchanged (it belongs to the PCH work).
- **Tests:** see the fix-run-3b report. Release ctest -j8 passed 48/48 x3 after the final edits. ASan passed 48/48 after the ASAN fixes.
- **Counter:** still `0.0.0.0.0.0.1.19`.

## Fix run 3c (2026-10-23)

- **Generic AngelScript bindings (Elfie P2 follow-up):** new `engine/src/script/include/litholattice/script/script_generic.h` builds `asCALL_GENERIC` wrappers from templates (`LL_SCRIPT_FN_OBJLAST(f)`, `LL_SCRIPT_FN_CDECL(f)`, which give a `ScriptEngine::ScriptFn`). `ScriptEngine::bindFunction(decl, fn, obj)` picks native or generic. Generic is used when the library reports AS_MAX_PORTABILITY, or when `forceGenericCalls(true)` is set, and one log line says so. script_console, game.cpp (currentDimension) and ui_script_bindings (10 functions) now go through it. The selftest stub `stubDimension` stays native because it is test-only. Two new engine_tests checks force the generic path and PASS: "log + uint64 cdecl binding run through asCALL_GENERIC" and "OBJLAST string/bool/float/int binding matches the native result". This has not been run on web.
- **Via connected sides (render issue):** `appearance.cpp` faceMaterial chose the legacy coloured `blue_via_side_*` textures by name, so the model's tinted neutral `side_top/mid/bottom/single` faces were never drawn. The model's connected-side variant now comes first and the name lookup is only a fallback. Files: block_model.h (`connectedSideFromName`, `FaceLayout::setConnected`), resource_loader.cpp, block_materials.h/.cpp (`connectedSide`), appearance.h/.cpp (`viaSideVariant`). Rule for a vertical run: bottom (via above only), mid, top (via below only), single (alone). Two "vias:" selftest checks PASS. **Remaining:** the BinaryQuad/PBR mesher does not call faceMaterial, so it still draws the plain `side` face there. No geometry is lost.
- **ES 3.00 PBR material table:** still a **proposal**, not done, because it is not a cheap change. It would replace `samplerBuffer`/`glTexBuffer` with an RGBA32F 2D texture read through `texelFetch(ivec2(id % W, id / W))`, plus a native/ES shader variant and the upload path.

### B8: intermittent engine_sim hang (fixed)

- **Severity: CRITICAL (safety).** The failure is silent: a submitted job never dispatches while input and the rest of the engine keep running. On a robot, a time-critical action (for example, moving an arm to catch a falling object) would simply never happen.
- **Symptom:** `litholattice_tests.exe sim` (and once `core`) sometimes hung forever at about 0 % CPU. Pre-fix Release hung at run 13. An earlier Release ctest round sat in engine_core for 28+ minutes until it was killed.
- **Location (real tree):** `engine/src/core/include/litholattice/core/job_system.h`
  - `JobSystem::submit` (line 195): the empty lock at line 219.
  - `JobSystem::shutdown` (line 352): the empty locks at lines 358-359, before the `notify_all` at lines 360-361 and the joins at lines 362-363.
  - The worker wait is in `JobSystem::workerLoop`, `cv_.wait(l, pred)` at line 454.
- **Root cause:** a condition-variable lost wakeup. `submit` (bumping `workGen_`) and `shutdown` (setting `stop_`) changed the predicate state and notified without taking `m_`, the mutex the workers wait on. The race goes like this:
  1. A worker checks the predicate under `m_` and sees nothing to do.
  2. Before that worker blocks, the other thread changes the state and calls notify.
  3. The worker then blocks and never wakes.
  At shutdown, the main thread then waits forever in `join()`.
- **Fix:** take and release `m_` (and `ioM_` for the io threads) between the state change and the notify. This orders the notify after any predicate check that is in progress. No timeout, retry or skip was added.
- **Stack proof** (flag-ON RelWithDebInfo build with the pre-fix header, hang at run 165 of the loop; captured by `stackdump.exe`, which uses DbgHelp StackWalk64):
  - main thread: `_Thrd_join` <- `JobSystem::~JobSystem` (job_system.h:188) <- `testDetSched` lambda (engine_tests.cpp:4439/4455).
  - worker thread: `SleepConditionVariableSRW` <- `_Cnd_wait` <- `JobSystem::workerLoop` (job_system.h:451, inside `cv_.wait`).
  - The line numbers are from that diagnostic build of the pre-fix header. This is exactly the shutdown missed-wakeup window. The 10 s waitIdle/fence watchdog never fired because the hang was in `join`, not in a fence. Full stacks are in `review-scratch-2026-10-23\b8-stacks.txt`.
- **Timing handshake (JD's suspect): not confirmed.** Every wait in `timing_handshake.cpp` is `wait_for(1ms)` with a predicate plus the FREEZE watchdog, and sim_thread's `wakeCv_` bumps its epoch under `wakeM_`. A handshake stall would spin and trip FREEZE. The hung process was idle, and the stacks show no handshake frames.
- **Loop proof after the fix:**
  - Release engine_sim: 130 + 130 = 260 runs, with 0 hangs and 0 failures.
  - Release engine_core: 80 runs, with 0 hangs.
  - ASan: ctest passed 48/48, and an engine_sim loop of 200 runs had 0 hangs and 0 failures.
- **PCH:** `R:\repos\PersonalControlHarness\engine\src\core\include\litholattice\core\job_system.h` is identical to the pre-fix file. Its submit increment is at line 213 and its shutdown at line 334, and the same two patches apply there. The PCH repo was not changed.
- **Diagnostic flag and macros** (these extend the existing `LITHO_THREAD_LOG`; nothing parallel was added):
  - CMake option `LITHOLATTICE_THREAD_DEBUG` (OFF by default; defined PUBLIC in engine/src/core/CMakeLists.txt). Turn it on with `-DLITHOLATTICE_THREAD_DEBUG=ON`.
  - `LITHO_THREAD_LOG(tag, fmt, ...)` writes into a lock-free ring per thread (256 lines of 200 chars, 256 thread slots).
  - `LITHO_THREAD_DUMP(why)` prints every ring to stderr.
  - With the flag OFF, both macros expand to `((void)0)` and their arguments are not evaluated.
  - In flag-ON builds only: `JobSystem::waitIdle` and `JobFence::wait` dump after 10 s and then keep waiting. submit and sleep log lines carry the work generation.
  - Setting `LITHO_THREAD_LOG_STDERR=1` also echoes each line live.
  - Files: thread_debug.h/.cpp, job_system.h.
  - After the instrumentation, the flag-OFF Release build passes ctest 48/48 x3 (44 s).

- **Follow-up (proposed, not implemented):** a deadline/liveness watchdog for critical jobs. A job submitted with a deadline that is not picked up by a worker within it raises a critical log (and a counter), so a stalled dispatch is never silent again.
- **Safety watchlist:** recorded as entry #1 in [docs/safety/SAFETY_WATCHLIST.md](../safety/SAFETY_WATCHLIST.md).

### R-PG-2: dungeon feature is a silent no-op (made loud, not implemented)

- **Where:**
  - `DungeonSlot::place` is empty (engine/src/procgen/include/litholattice/procgen/stages.h:499).
  - `loadFeature` creates it for `"type": "dungeon"` (engine/src/procgen/generator_loader.cpp:757-763).
  - `game/LithoKraft/assets/worldgen/organic.json:154` requests one.
- **Design check:** there is no layout design. PROCGEN.md says "placeholder". DESIGN-BRIEF-full.md only names "chip dungeons" (Syber-Squad themed, Void/Wafer, Pulsar) and has no rooms, spawning or loot. So nothing was invented.
- **Change:** the loader now calls `logError` ("dungeon generation is not implemented (issue R-PG-2); this feature places nothing") each time a generator requests a dungeon. The slot still loads, so existing worlds keep working. A procgen selftest check confirms the error is logged and PASSES. PROCGEN.md and the stages.h comment were updated.
- **A real implementation needs:**
  - a design for the layout (room/corridor grammar or blueprint set, sizes, and chip theming per dimension);
  - placement rules (depth band, spacing and biome or density keys, deterministic per-region seeding, and chunk-border handling the way structures do it);
  - blocks for its contents (spawner, loot container and their tables);
  - JSON schema plus loader validation;
  - determinism and selftest coverage.

### Found while testing (not changed)

- **R-TST-3:** in `procgen_selftest.cpp`, the "ruins: unknown biome" and "ruins: missing crust" rejection checks join several `R"(...)"` pieces, but only the last piece closes with `)"`. The first raw literal therefore runs across lines and contains a stray `"` and `R"(`. The JSON fails to parse ("expected ':'"), so both checks pass for the wrong reason. Each piece should close its own raw literal; the new R-PG-2 check does this.

### Tests (3c)

- Release ctest -j8: 48/48 x3 after the final edits.
- ASan ctest 48/48 and the 200-run engine_sim loop were run after the B8 fix, before the flag-gated instrumentation and R-PG-2 edits. The instrumentation compiles out when the flag is OFF.
- GL boot OK: counter `0.0.0.0.0.0.1.19`, AngelScript ready, hello script ran.
