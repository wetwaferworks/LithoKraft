# LithoLattice / LithoKraft Source Review, Pass 3 (READ-ONLY)
Date: 2026-10-15 (EDT). This pass builds on pass 1 (0902) and pass 2 (0909).
No source in the R:\ tree was changed. For diagnosis only, I compiled throwaway copies on the box under /tmp/ct. Nothing went back to the user's tree.
Naming rule: names ending in -or are written lowercase.

## 1. Console Debug failures: PINNED (root cause CONFIRMED)
- **Root cause:** a dangling pointer in `ConsoleRegistry::coerce`, at engine/src/console/concmd.cpp:363 (Int) and :373 (Float).
  - Each line calls `std::strtol(std::string(raw).c_str(), &end, 10)`, and strtod does the same on :373.
  - The temporary `std::string` is destroyed at the end of that full expression.
  - So `end` points into freed storage, and `*end != '\0'` on the next line (365 / 375) reads it after its lifetime ends.
  - In Release the stack bytes happen to still hold '\0', so the check passes.
  - MSVC Debug fills dead stack and heap memory with patterns, so `*end` is non-zero. Every typed Int/Float cvar set is then rejected.
  - That breaks the cvars, toggle, incrementvar, ++/--, nan/inf and the dependent exec/unterminated checks.
- **Proof:**
  - I rebuilt the console selftest on the box with `-O0 -fsanitize=address,undefined -D_GLIBCXX_DEBUG`.
  - ASan reports `stack-use-after-scope ... concmd.cpp:365 in ConsoleRegistry::coerce` and aborts.
  - A scratch copy with only those two lines changed passes the whole suite: **100 PASS / 0 FAIL**.
- **Fix:**
  - `const std::string tmp(raw); const long v = std::strtol(tmp.c_str(), &end, 10);`
  - Same for strtod.
  - A tree-wide grep found only these 2 instances.
- **Severity:** HIGH. This is undefined behaviour in a release build, and it currently works only by luck. It could break silently on another compiler, optimisation level or allocator.
- **Determinism:** no (console only). It does gate settings cvars such as render.fps_cap.

## 2. 1238 vs 1239 check count: PARTIALLY PINNED
- I extracted and counted the Windows `--selftest` output from the ctest log (#40, lines 1178-2628).
  - It shows exactly 1238 `[PASS]/[FAIL]` markers.
  - None is hidden by log interleaving. The interleaved lines at 912, 1091 and 1128 still carry their marker.
- **-3:** the "sig: symlink fixture created" failure skips 3 dependent checks (assets_selftest.cpp:385, 389, 395). Confirmed: none of those strings appear in the Windows output.
- **+1:** the Vulkan probe (app/selftest.cpp:1124-1131).
  - Windows ran it as a real check: "vulkan: headless instance + physical devices (... RTX 3050 Ti ...)".
  - On a GPU-less Linux box that line is a `[SKIP]` printf, not a check.
  - If the 1239 Linux reference ran without a GPU, Windows gets +1. That condition is UNVERIFIED: I don't have the Linux 1239 log. The Linux logs on the box are from 2026-10-09 and from an older, smaller tree.
- **+1 remaining:** unattributed.
  - There are no compile-time platform guards in the suites `runSelfTests` calls.
  - Correction to pass 2: my "data-dependent loop" theory doesn't hold. The repeated checks are fixed-count. Examples:
    - signal_selftest.cpp:617 runs 4 times on both platforms.
    - struct_registry_selftest.cpp:284 runs 3 times.
    - The diversity suite runs twice; see L-new-1.
  - To close it: diff the Linux `--selftest` output line-for-line against the Windows list (saved at C:\Users\jdwetwork\GrokBot-Transfer\t40-checks.txt).
- **Fix:** count SKIPs explicitly and print `N pass / M fail / K skip`, so the two platforms can be compared directly.

## 3. New findings (pass 3)
### HIGH
- **P3-H1: console coerce dangling pointer.** concmd.cpp:363,365,373,375. See section 1.
- **P3-H2: a region in-place rewrite isn't crash-safe** (engine/src/storage/anvil_region.cpp, `stage`, roughly lines 368-378).
  - When the new record fits the old run, `sector = sectors_[sl]` and the payload is written OVER the record the index currently points at, before `commit`.
  - A crash or power loss mid-write leaves the indexed record torn. CRC catches it, but the chunk's previous version is gone, and the column loads as Unreadable or lost.
  - The sidecar path (`fopen(sp,"wb")`, same function) truncates the live sidecar the same way.
  - This contradicts the file's own "payload-first" commit protocol.
  - **Fix:** always stage into a free run (copy-on-write), and free the old run only after the index commit. For sidecars, write to `.tmp` and rename.
  - **Determinism:** no (data integrity).
### MEDIUM
- **P3-M1: shared `exporting_` across dimensions** (world_save.cpp:305-307, 376-378).
  - `exporting_` is a single WorldSave member, but each dimension's WorldStore has its own I/O queue.
  - Saving two dimensions back-to-back with wait=false can let dim B's enqueue overwrite or clear dim A's SGNL export mid-encode, so component memory is saved into the wrong dimension or dropped.
  - **Fix:** key `exporting_` by dimension, or capture `state` in each save job.
  - **Determinism:** it can affect save/load replay.
- **P3-M2: synchronous save work on the caller thread.** saveDimension deep-copies every non-air SubChunk and re-reads blocks.names (readStableRemap) on every save (world_save.cpp:258, 265-283). Autosave stalls scale with the size of the loaded world. **Fix:** copy only dirty columns, and cache the remap check.
- **P3-M3: file_contract.cpp doesn't sync to disk on Windows** (the pass-2 M14 finding). anvil_region.cpp:70-72 already uses `_commit()`, so the two storage paths are inconsistent. **Fix:** mirror the anvil approach.
- **P3-M4: GL33 entity mesh silently truncated at 4096 vertices** (gl33_renderer.cpp:450,460). Entities past about 1024 quads vanish with no log. **Fix:** grow the buffer the way the sprite path at 170-177 already does.
- **P3-M5: narrow paths in the mod loader too.** mod_loader.cpp:130 uses `std::ifstream(path std::string)`. This is the same class as M15. In addition, `is_directory` (242) follows Windows junctions and symlinks into folders outside the mods directory (pairs with N-H2).
### LOW
- **L-new-1: diversity suite runs twice.** `runDiversitySelfTest()` is called at both procgen_selftest.cpp:3099 and :3104. The second call adds its failures again (double-counted) and costs runtime.
- **L-new-2: stdout and logger interleave mid-line in selftest output** (log lines 912, 1091, 1128). This makes log-based counting fragile. **Fix:** route both through one locked sink.
- **L-new-3: script hook budget too long for a tick.** The hook budget is `hookMs = 500` (script_engine.h:144). Hooks run only at load/editor time (game.cpp:659-660), so this is OK today. If an onTick hook is ever wired onto the sim thread, a single hook could stall the 512 TPS handshake for 500 ms before throttling.
- **L-new-4: stoul truncation** (game.cpp:487-491). On Linux, an out-of-range unsigned long is narrowed by static_cast to uint32 with no range check.

## 4. Final consolidated top 10
1. **Alt-tab click swallow and mouse re-capture** (pass 1; game.cpp:1776, focus edges, cached `captured_`).
2. **Console coerce use-after-scope** (P3-H1). A two-line fix, and it closes the Debug-only test failures.
3. **Region in-place rewrite crash safety** (P3-H2). Copy-on-write sectors plus a tmp+rename sidecar.
4. **Mesher per-voxel shared_mutex** (pass 1). Use a LUT snapshot.
5. **Software rasterizer:** front-to-back sort (N-H3), remove the per-pixel divides, and add near-plane clipping (H2).
6. **Windows sandbox hardening** (N-H2, P3-M5): reparse-point / final-path checks in sandboxed_io and the mod loader.
7. **Deterministic procgen math** (N-H1). There are 59 libm transcendental calls in worldgen.
8. **Ed25519 static-init race** (M8) and the **Vulkan submit-failure fence hang** (M10).
9. **Determinism and save hygiene:** the `rand()` item-drop leak (M11), the cross-dimension `exporting_` race (P3-M1), Windows durability (M14/P3-M3) and Unicode paths (M15).
10. **Windows test fixes:**
    - HighResTimerScope in the input thread.
    - SKIP symlink tests without the privilege.
    - systerian / wafer.json out-of-tree paths.
    - Explicit SKIP counting (fixes the 1238/1239 confusion).
    - Remove the duplicate diversity run.

## 5. Final coverage (honest)
- **Fully read, line by line, across 3 passes: about 6,400 lines.**
  - Pass 1: about 3,400 lines, covering:
    - the sim / handshake / threading core
    - mesher hot paths
    - the input/focus/cursor path
    - software renderer 1-340
  - Pass 2: about 2,500 lines, covering:
    - SHA-256/512
    - Ed25519
    - software renderer 340-1125
    - vk_renderer 140-519
    - sandboxed_io
    - input_thread
    - signal.cpp 120-439
  - Pass 3: about 500 lines, covering:
    - concmd.cpp coerce, setCVar and dispatch
    - the console_selftest failing blocks
    - anvil_region.cpp open/alloc/stage/commit (240-420)
    - world_save.cpp 176-465
    - gl33 440-500
    - script_engine.cpp run() (420-470)
    - selftest.cpp vulkan/save/evict blocks
- **Targeted reads plus pattern scans only (NOT line by line):**

  | Area | Lines |
  |---|---|
  | procgen | 7,029 |
  | GL33 remainder | about 830 |
  | vk renderer remainder | about 1,650 |
  | game.cpp remainder | about 1,900 |
  | world_save remainder | about 460 |
  | anvil_region remainder | about 420 |
  | mods | 1,647 |
  | script | about 830 |
  | UI | 2,980 |
  | console remainder | about 2,300 |
  | MCP | 4,543 |

  - Scan methods: libm, rand, fopen/path, catch(...), threads, GL buffer calls, budgets, and the MCP token compare (constant-time, OK).
- **Bottom line:** the request asked for all of the remaining areas to be read fully, and this pass did NOT do that. About 24,500 lines remain scanned rather than read. The pinned root causes above are verified, either by sanitizer run or by direct log evidence. A complete read of the remaining areas needs further dedicated passes, roughly 4-5 more at this depth. Suggested order: procgen, MCP, UI, console, game.cpp.
