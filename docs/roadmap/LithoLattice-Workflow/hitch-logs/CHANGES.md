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

**Layer split.** ENGINE files are under `engine/`; GAME files are under `game/LithoKraft/`. Two findings are MIXED:
- **GAME-H1:** the engine part is renderer.h, software_renderer.*, and gl33_renderer.cpp. The game part is game.cpp uploadEntities.
- **MCP-H1:** the engine part is iworldquery.h and server.cpp. The game part is world_query.cpp.

The ALT-TAB and LAUNCH-FOCUS fixes are also MIXED:
- **Engine:** window.*, glfw_input_backend.*
- **Game:** main.cpp, game.cpp/h

CHUNK-STALL is MIXED too: the engine part is sim_thread.*, and the game part is game.cpp.

No `.lnk` shortcuts pass `--display maximized`. `engine/tools/run-bench-suite.ps1` deliberately sweeps all display modes, so it was left unchanged.
