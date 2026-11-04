# Timing Handshake — Production Merge Report

**2026-10-13 · JD directive 04:35 EDT: "Go ahead and begin with the merge."**
Source records: [Timing-Handshake-InGame-Sweep-2026-10-13.md](sandbox://workspace/user/files/Timing-Handshake-InGame-Sweep-2026-10-13.md) + experiment tree working files
(`~/workspace/experiments/pull-tick/hs-design.md`, `hs-integration-notes.md`,
`hs-sweep-results.md`).

---

## 1. Snapshot (taken before anything was touched)

- **Location:** `~/workspace/backups/litholattice-pre-handshake-merge-2026-10-13/`
  (rsync, 29 MB, 1,611 files; excludes build dirs and `_deps`).
- **Snapshot integrity:** source manifest of the snapshot is byte-identical to
  the pre-merge production manifest (`SNAPSHOT_MANIFEST_IDENTICAL` via
  sorted path+size+mtime diff).
- **Pre-merge production manifest:** 1,611 files,
  md5 `5ae64504c598eab9c538cadecb098e5e` (stored in the snapshot as
  `MANIFEST-premerge.txt`). Note: differs from the sweep-start manifest
  (`2f44bbf9b77f63b0470c696f211d4db9`) in exactly one file,
  `game/LithoKraft/CREDITS.md` — the parent's own credits edit at ~04:16
  EDT, not part of this merge.

## 2. Drift analysis (production vs experiment base, per file)

Production source was otherwise **byte- and mtime-identical** to the
experiment copy base. For each of the 19 modified files the P→E diff was
inspected:
- Keyword scan of every removed line for non-integration content
  (camera, locomotion, sprite, present, software, velocity, vitals) —
  **zero hits**.
- Removed lines in the small files are exactly the integration's edits
  (CMakeLists add lines, `main.cpp` selftest call signature,
  `settings.cpp` sync lines, etc.).
- `sim_thread.cpp` removed lines (17) = the two domain-step loops the
  integration rewired inline in both run branches (D-g).
- `signal.h` removed (1) = `takePhaseMs()` replaced by the atomic
  accumulator version (D-n). `signal_world.cpp` had **zero** removed
  lines (pure additions). `perf_headless.cpp` removed (5) = extended
  perf output lines (D-n/§7 plumbing). `signal.cpp` removed (92) = the
  original monolithic tick body, decomposed into the `hs*` methods per
  §3.2 (all lines verified signal-domain internals).
- **Line endings:** `game/LithoKraft/src/app/selftest.h` is CRLF in
  production but LF in the experiment copy (the experiment diff showed
  all 9 lines changed for that reason; the only file with an ending
  mismatch among all 23). It was **hand-edited** (Python byte-level,
  signature line only) to preserve CRLF — verified CRLF-intact and
  content-identical to the experiment version modulo endings.

**Drift verdict: no production drift in any touched file. Faithful port
was safe and complete.** Cross-check after porting: diff of pre-merge
snapshot vs experiment tree (excluding build dirs, _deps, hs-*
records, testbed) shows differences ONLY in the 23 integration files
plus `CREDITS.md` (parent-owned, deliberately not ported) — no other
file changed.

## 3. File-level changelog (production)

**Created (4, copied verbatim):**
- `engine/src/sim/include/litholattice/sim/timing_handshake.h`
- `engine/src/sim/timing_handshake.cpp`
- `game/LithoKraft/src/app/handshake_test.h`
- `game/LithoKraft/src/app/handshake_test.cpp`

**Modified (19), all integration-attributable:**
- `engine/src/sim/CMakeLists.txt` — `timing_handshake.cpp` added to
  `litholattice_sim`.
- `engine/src/sim/include/litholattice/sim/world_simulation.h` —
  `IWorldSimulation::registerHandshake` (default no-op).
- `engine/src/sim/include/litholattice/sim/sim_thread.h` —
  `SimConfig::{timingHandshake=false, handshakePhasesPerCycle=4,
  handshakeWorkerThreads=0, handshakeWatchdogMs=0}`,
  `SimStats::{hsOverrunPhases, hsOffenders, hsFrozen}`,
  `handshakeAuthority()`, `handshakeFrozen()`,
  `resetHandshakeAfterFreeze()`.
- `engine/src/sim/sim_thread.cpp` — authority construction +
  registration, domain cadence items, `start()`/`stop()` wiring,
  domain phases in both run branches, freeze handling, frozen step
  refusal, stats roll-up.
- `game/LithoKraft/src/content/include/lithokraft/content/signal.h` —
  hs* public surface, per-net `NetWork` records, `pulseGen`, phase
  timing accumulators.
- `game/LithoKraft/src/content/signal.cpp` — `tick()` decomposed into
  `hsBeginCycle/hsFlipAndTorches/hsComponents/hsMark/
  hsPrepIncremental+hsPropagateNet×marked+hsApplyStrengthPending
  /hsLampsBatch(all)/hsFinishCycle/hsCommit`; thread-safety discipline
  (pull workers never touch the real `DirtyFn`; per-shard dirty
  collectors; per-net pending strength lists replayed on the sim
  thread — D2/D3 outcomes).
- `game/LithoKraft/src/content/include/lithokraft/content/signal_world.h`
  + `signal_world.cpp` — `registerHandshake`, handshake `tick()`
  driving P `runPhase` calls, commit handler.
- `game/LithoKraft/src/game/include/lithokraft/game/settings.h`,
  `settings.cpp`, `settings_cvars.cpp` —
  `GameSettings::{timingHandshake, handshakePhases}`; cvars
  `sim.timing_handshake` (Bool, default **OFF**) /
  `sim.handshake_phases` (Int, 1..16); archived; `syncFrom`; settings
  file persistence.
- `game/LithoKraft/src/game/include/lithokraft/game/game.h` —
  `GameConfig::{timingHandshake, handshakePhases}`.
- `game/LithoKraft/src/game/game.cpp` — `SimConfig` filled at world
  load (+ log line); apply-on-change logs "applies on world load";
  `tick_unfreeze` calls `resetHandshakeAfterFreeze()` when frozen.
- `game/LithoKraft/src/app/main.cpp` — `--timing-handshake` /
  `--no-timing-handshake` / `--handshake-phases N` /
  `--handshake-test` / `--handshake-scenario`; CLI > settings >
  default-off; flags threaded into perf and selftest.
- `game/LithoKraft/src/app/perf_headless.h` / `perf_headless.cpp` —
  phase-timer plumbing (§7).
- `game/LithoKraft/src/app/selftest.h` / `selftest.cpp` /
  `app/CMakeLists.txt` — selftest takes the handshake flag;
  `runHandshakeSelfTest()` section (+4 checks); `handshake_test.cpp`
  linked into the app.
- `game/LithoKraft/src/app/selftest.h` — signature-only change,
  CRLF preserved.

**Explicitly NOT ported:** build dirs, `_deps`, `pull_tick_testbed`
(standalone harness), `hs-*.md` working records, and the C2
cycle-budget accounting change + empty-name diagnostic fix
(JD-sequenced as LATER optimization work).

**Default OFF:** `SimConfig` default is false and `main.cpp` precedence
is CLI > cvar > default; legacy mode constructs nothing — an
unconfigured build behaves as before (verified below).

## 4. Build safety

- Production `build-linux` cache paths verified BEFORE the first
  build: `CMAKE_CACHEFILE_DIR` = production `build-linux` itself,
  `CMAKE_HOME_DIRECTORY` = production `engine/` — no cross-tree
  writes possible (the sweep incident pattern cannot recur here).
- `cmake --build ~/workspace/litholattice/build-linux -j2` →
  **BUILD_EXIT=0** (Release), zero errors in the build log
  (145 s wall).

## 5. Verification (merged production tree, build-linux, Release)

Run order below; binary `./LithoKraft` from `build-linux/`.

| Check | Result |
|---|---|
| `--handshake-test` | **24 PASS / 0 FAIL, exit 0, "HANDSHAKE TESTS: OK"** (WAIT 7/7 incl. 20 ms burn vs 5 ms budget; FREEZE 7/7 incl. latch; FREEZE e2e 6/6 incl. frozen-world save/reload to pre-freeze hash; 256-tick scenario summary) |
| `--selftest` (legacy default) | **891 PASS / 6 FAIL, exit 1** |
| `--selftest --timing-handshake` | **891 PASS / 6 FAIL, exit 1** |
| Failure-set A/B | **identical** (compared with log timestamps stripped) — known 6: 4× save round-trip, mesher surface sub-chunk, Vulkan headless (environmental) |
| Determinism scenario 2048 ticks | legacy `faec0f02700d98c6` · hs(w1,p4) `faec0f02700d98c6` · hs(w4,p4) `faec0f02700d98c6` · hs(w4,p8) `faec0f02700d98c6` · hs(w4,p4) repeat `faec0f02700d98c6` — **all five arms bit-identical, and identical to the experiment-tree hash** |
| Signal suite (selftest section, lines 1–149 of each log) | **117 PASS / 0 FAIL in both modes** |
| `./tests/litholattice_tests sim` | **39 PASS** ("ENGINE TESTS OK") |
| `ctest` (46 tests) | **43/46 passed** (69.7 s). Failures: `lithokraft_selftest` (carries the known 6 checks), `lithokraft_scout_selftest`, `lithokraft_chipor_selftest` — all known pre-existing. `engine_systerian` **passes** in production (its experiment-tree failure was a tree-location SVG-path artifact). Zero new failures |

Expected vs observed: production pre-merge baseline was 887/6
(selftest); +4 handshake-section checks → **891/6**, exactly observed,
both modes. No regressions.

## 6. Post-merge production manifest

1,615 files, md5 `3cdcd91920f2583386dd649c7b1942ad`
(`~/workspace/merge-run-logs-2026-10-13/manifest-postmerge.txt`).
Delta vs pre-merge: the 23 ported files, plus
`game/LithoKraft/CREDITS.md` — the parent's own credits edits (the
second at 08:43:59 UTC, during this verification), which are not part
of this merge. No test-run artifacts landed in the source tree
(verification runs wrote only into `build-linux/`).

**Process disclosure:** `/tmp` was wiped mid-task, destroying the
first round of run logs and the first ctest attempt's output. Every
verification above was therefore **re-run** with logs kept durably in
`~/workspace/merge-run-logs-2026-10-13/` (build.log excepted — the
build was not repeated; its exit-0 result and the binaries it produced
are what the re-run tests exercised). All numbers in §5 are from the
durable logs.

## 7. Rollback

If anything later goes wrong: restore
`~/workspace/backups/litholattice-pre-handshake-merge-2026-10-13/`
over `~/workspace/litholattice` (excluding the build dirs), which
returns the tree to the pre-merge manifest
`5ae64504c598eab9c538cadecb098e5e`.
