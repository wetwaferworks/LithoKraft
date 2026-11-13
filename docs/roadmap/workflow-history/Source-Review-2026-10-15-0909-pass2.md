# LithoLattice / LithoKraft Source Review, Pass 2 (READ-ONLY)
Date: 2026-10-15 (EDT). Builds on pass 1 (Source-Review-2026-10-15-0902.md). No code was changed.
Naming: names ending in -or are written lowercase (Observor, Deceitor), as the house rule requires.

## Summary
- Priorities 1-4 progress:
  - Crypto (SHA-256, SHA-512, Ed25519) is fully read.
  - Software renderer lines 340-1125 are fully read.
  - The Vulkan core frame path (vk_renderer.cpp 140-519) is fully read.
  - GL33 and procgen were only SCANNED, not read line by line (see Coverage).
- All five open items are root-caused, except console, which is narrowed but not pinned.
- New HIGH findings:
  - N-H1: procgen uses libm transcendental functions, which breaks cross-platform world determinism.
  - N-H2: the Windows mod sandbox has no symlink/O_NOFOLLOW hardening.
  - N-H3: the software renderer has no front-to-back sort, so overdraw is fully shaded.
- No new CRITICAL findings.

## Open items: root causes
1. **engine_console Debug-only failures** (console_selftest.cpp:240-264 cvars block, 581-672 toggle block). NARROWED, NOT PINNED.
   - Both blocks set typed cvars through executeLine("name value"), read them back with findCVar()->value, and both register a Float cvar.
   - The suspect path is coerce (concmd.cpp:350-392: strtol/strtod with errno, plus "%.9g" formatting).
   - Each block shares one `ok` variable, so one bad check cascades into the later checks in that block.
   - Fix/next step: give each assert its own diagnostic (expected vs actual), then rerun the Debug binary.
   - Determinism: no.
2. **Input queue-overflow test** (input_selftest.cpp:94-105).
   - The pump calls sleep_for(2ms) (input_thread.h:73 pollInterval, input_thread.cpp threadLoop) without HighResTimerScope. On Windows each sleep lasts about 15.6 ms.
   - At 7 events per poll, 3000 events take about 429 polls, roughly 6.7 s. The test waits only 2.5 s, so dropped() comes out lower than 3000-1024.
   - Fix: hold HighResTimerScope in the input thread, or have the test wait until the backend is drained.
   - Determinism: no.
3. **Symlink tests.**
   - Windows create_symlink needs Developer Mode or admin rights. This affects the storage sandbox tests and assets_selftest.cpp:378-379.
   - Fix: SKIP on ERROR_PRIVILEGE_NOT_HELD. See also N-H2.
4. **1238 vs 1239 selftest count.**
   - The symlink fixture failure skips 3 dependent checks (assets_selftest.cpp:380-396), giving -3.
   - The remaining +2 come from data-dependent check loops and are not exactly attributed. No compile-time platform skip was found.
   - Fix: make check counts independent of the data, or log skipped checks explicitly.
5. **Mob densities all 0.**
   - There is no natural mob spawner anywhere in the source. Mobs appear only via the `deceitor spawn` console cheat, and that needs ai.enabled, which defaults to OFF (ai_host.h:36,138; game_commands.cpp:134-139).
   - The density values in blocks.cpp are marked JD-DECISION.
   - This is a missing feature, not a bug.
6. **engine_systerian failure (new).**
   - It resolves engine/src/systerian/../../../../user/files/cistercian/*.svg, a path outside the repo, so 39 files are missing.
   - Fix: point it at a repo-relative fixture directory, or SKIP when the directory is absent.

## New findings (ranked)
### HIGH
- **N-H1: procgen libm determinism.**
  - 59 std::sin/cos/atan2/pow/exp calls in engine/src/procgen feed voxel placement, e.g. trees.cpp:53,113,249 and shrubs.cpp:97,228,308.
  - MSVC and glibc libm differ in the last ulp, so the same seed can produce different trees and shrubs on Windows and Linux.
  - Fix: use integer or fixed-point math, or a vendored deterministic sin/cos/atan2 (polynomial tables); keep /fp:precise.
  - Determinism: YES. The world is cross-platform; scenario hash aa82e7afb1eff9d3 is unaffected as far as observed.
- **N-H2: Windows mod sandbox.**
  - sandboxed_io.cpp:123-137 has no O_NOFOLLOW or final-path check on non-Linux, and is_regular_file follows symlinks.
  - The exec hardening at concmd.cpp:104 and :967 is Linux-only, as are the file_contract guards.
  - Fix: open with CreateFileW and FILE_FLAG_OPEN_REPARSE_POINT, then check GetFinalPathNameByHandleW against the sandbox root.
  - Determinism: no.
- **N-H3: software renderer draw order.**
  - Meshes are drawn in std::map key order (software_renderer.h:239-240) with no front-to-back sort, so every overdrawn fragment is shaded.
  - Fix: sort visible chunks by camera distance each frame.
  - Determinism: no.
### MEDIUM
- **M8: Ed25519 static init race.** curve_d() and base_point() use a non-thread-safe `static bool init`, which is a data race under concurrent verifies. Fix: use a magic-static (lambda initializer).
- **M9: Ed25519 performance.** It uses affine coordinates with 2 field inversions per point_add, so verify is very slow. Fix: use extended coordinates.
- **M10: Vulkan submit failure hang** (vk_renderer.cpp:201-214). If vkQueueSubmit fails after vkResetFences, the fence never signals and the next waitSlot(UINT64_MAX) hangs forever. Fix: on failure, re-signal the fence with an empty submit, or recreate it.
- **M11: rand() in item-drop velocity** (game.cpp:2009-2011). It is non-deterministic and not thread-safe, and breaks replay/demo. Fix: use a seeded sim RNG. Determinism: YES (replay).
- **M12: signal per-tick work.** signal.cpp:402 copies the mountPowered_ set every tick, and collectMountPowered runs twice per tick (375, 403). Fix: cache it once per tick. Determinism: no (the iteration is order-independent).
- **M13: SSAA2x reallocation.** After resolve, fb_ is set to the target size, so fb and depth are reallocated every frame (software_renderer.cpp:1038, 1088-1093). Fix: keep separate render and present buffers.
- **M14: saves are not durable on Windows.** file_contract.cpp fsyncFile only flushes on non-Linux, so data is not on disk. Fix: _commit(_fileno(f)) or FlushFileBuffers.
- **M15: narrow-path fopen.** 18 fopen(path.string().c_str()) sites (e.g. anvil_region.cpp:249,254,349,406,529) fail or throw on non-ASCII Windows paths, e.g. a user profile folder with non-ASCII characters. Fix: _wfopen or std::ofstream(path).
- **M16: MCP Windows path untested.** http_server.cpp:12,130 is marked "UNVERIFIED Windows branch", and the live HTTP tests are POSIX-only. The constantTimeEqual token compare (167-173, used at 425) is fine.
### LOW
- Ed25519 is not constant-time when signing (offline tool only).
- point_decompress accepts non-canonical x=0 with the sign bit set.
- verify fails when S=0.
- fe_reduce has a theoretical dropped carry.
- SHA final() is not re-entrant.
- swMeshKey drops the top 8 bits of cx (software_renderer.cpp:481-484). GL33:321 uses a different key scheme.
- uploadChunkMesh copies vertices instead of moving them (:496).
- applyFXAA legacy path is dead code.
- vk recreateSwapchain clears renderDone_ right after a device wait (present-wait hazard).
- signal.cpp:163-165 comment is stale ("sim is stopped", now "held").

## -or naming sweep
- Capital "-OR" appears in 31 files and 59 string literals: ChipOR, EtchOR, ChopOR, PortOR, LithographOR, SlicOR, BashOR, SeedOR, HeapOR, PickOR, DebuggOR, TurtlOR, ReadOR, SelectOR(s), ConnectOR, FabOR.
- Mixed forms in the same enum: blocks.h:132 has SignalTransmittor, SignalReceivor and Overclockor alongside LithographOR and ChipOR.
- Mixed forms for the same thing: Portor in comments (logic_components.cpp:339-348) vs class PortOR.
- User-visible strings: blocks.cpp:76,84,108,113,139; items.cpp:13,40-49; game_commands.cpp:106; game.cpp:171; mcp/prompts.cpp:74.
- Stable ids are already lowercase (lithokraft:chipor), so renaming is save-safe.

## Updated top 10 fixes
1. Alt-tab click swallow and re-capture (pass 1; game.cpp:1776 plus focus edges).
2. Mesher: per-voxel shared_mutex. Use a LUT snapshot (pass 1).
3. Software rasterizer: front-to-back sort (N-H3) plus per-pixel divide removal.
4. Near-plane clipping in the software renderer (H2).
5. Windows sandbox hardening (N-H2).
6. Deterministic procgen math (N-H1).
7. Ed25519 static init race (M8) and Vulkan submit hang (M10).
8. rand() determinism leak (M11) and blockMutex burst (pass 1).
9. Windows durability and Unicode paths (M14, M15).
10. Windows test fixes: input timer, symlink SKIP, systerian path, console diagnostics.

## Coverage (honest)
- FULLY READ, about 5,900 lines total:
  - pass 1: about 3,400 lines
  - sha256: 115; sha512: 120; ed25519: 500
  - software_renderer 340-1125: about 786
  - vk_renderer 140-519: about 380
  - sandboxed_io: 206
  - input_thread: 79
  - signal.cpp 120-439: about 320
  - file_contract top: 70
  - related test sections
- PATTERN-SCANNED ONLY:
  - procgen (about 8k lines)
  - GL33
  - vk_renderer remainder
  - MCP http_server
  - audio (miniaudio callback, clean)
  - storage/anvil_region
  - determinism sweep across the tree
- NOT REVIEWED: the rest of game.cpp, world_save.cpp, mods loader, script, UI, console beyond the failing paths.
- A full line-by-line read of the whole codebase is NOT complete. A pass 3 is needed for the items above.
