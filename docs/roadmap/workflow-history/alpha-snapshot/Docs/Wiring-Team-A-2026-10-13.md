# Wiring Team A — Sim & Systems: Final Report

**LithoLattice / LithoKraft — 2026-10-13**
**Scope:** Team A items A1–A6 from JD's green light (15:41 EDT) and `Brief-Whats-Missing-2026-10-13.md`. Team B (render/world) worked in parallel on the same production tree; shared files were edited hunk-wise under a serialized edit lock, never overwritten.
**Formation:** coordinator + exactly two workers. Worker 1: A1 → A4 → A6. Worker 2: A2 → A3 → A5.
**Safety:** full production snapshot taken **before any edit** — `~/workspace/backups/litholattice-pre-wiring-2026-10-13/` + `~/workspace/wiring-2026-10-13/manifest-pre-wiring.md5` (21,110 files; snapshot verified hash- and structure-identical). Per-item backups under `~/workspace/wiring-2026-10-13/backups-worker1/` and `backups-worker2/` enable item-wise revert. **No item required reverting.**

## Verdict summary

| Item | Result |
|---|---|
| A1 — Port control serialization + IDemoRecord | **WIRED** (18/18 files, gates green at port time) |
| A2 — Column eviction (B-M1) | **WIRED** (all seams; permanence proofs bit-exact) |
| A3 — Fragment no-decision subset (B1/B1a) | **WIRED (subset)** — B1 remains **PARTIALLY WIRED**; the 4 JD decisions are untouched |
| A4 — `render.fps_cap` game-side wiring (B7) | **WIRED** (recipe followed exactly) |
| A5 — Projectile density (B8) | **WIRED** (values flagged JD-DECISION, not approved) |
| A6 — AiWorld production path (B6) | **WIRED** (spawn/lifecycle content remains JD-scope) |

---

## A1 — Control serialization + IDemoRecord port: WIRED

Exactly the combined deduplicated 18-file manifest (IDemoRecord report §8): 10 new + 8 modified, no more, no less. Pre-copy, all 8 modified production files' MD5s equaled the pre-wiring snapshot and all 10 new files were absent; post-copy, all 18 MD5s match the experiment tree.

Gates at port time (production `build-linux`, tree as of ~20:00Z): build exit 0; demo selftest **31/31** and control selftest **20/20** PASS in both modes; `--demo-test` OK with all four world hashes **`5041cf1795c33a8c`**; `--handshake-test` OK; `--control-test` OK; `--handshake-scenario 2048` = **`faec0f02700d98c6`** in both modes; `--demo-verify` COMPLETE, exit 0.

## A2 — Column eviction: WIRED

All five B-M1 seams landed together (no partial eviction):

- `ChunkColumn::dirty`/`generation` + `markDirty()` on `ChunkMap::set`/`setState` mutation (fresh generated/disk-loaded columns start clean); `ChunkMap::erase`/`contains`.
- `SignalSim::unscanColumn` — drops all indexed sets for the column plus lamp holds/wakes, zeroes removed wires' trace-strength slots, clears the reach cache, forces a full pass/net rebuild; `exportColumnState` helper.
- `WorldSave::saveColumns` — save-on-evict through the existing store + per-column SGNL path with saveDimension's fail-closed discipline; SGNL export happens **strictly before** unscan.
- `Game::evictFarColumns()` at the end of `pumpStreaming()`, in its own sim-stopped window (a poisoned stop aborts the batch, columns stay resident). **Conservative rule:** a column drops without saving only when it is not dirty **and** the sim indexes nothing in it; absent/failed save ⇒ column stays resident, logged.
- Mesh retire: pending classic/PBR queues purged, upload sequence tombstones (cleared on re-insertion), empty meshes uploaded to renderer + MCP vision cache. `ChunkStreamer::forgetColumn` removes `requested_`/staged/retry entries so evicted columns can re-stream.

**Proofs (16 selftest checks, PASS in both modes):** generate→mutate→save-on-evict→unscan→erase→reload gives **bit-exact world-hash equality** and SGNL `exportState` equality; no ghost components after unscan + ticks; clean fast-path evict/reload equality; resident count bounded **1681→441** at radius 8 + hysteresis 2 with **≤4 evicted per pump**; no candidates inside the hysteresis band.

**JD-DECISION parameters (implemented proposals, not locks):** hysteresis **+2 chunks**, eviction budget **4/pump** — named constants `kEvictHysteresisChunks`/`kEvictBudgetPerPump` in `game.cpp`.

*Caveats:* mesh retire is verified at the seam level (no GL-context visual check exists on this VM); eviction is driven from `pumpStreaming` only and touches only the active dimension, matching insertion's discipline.

## A3 — Fragment subset: WIRED (B1 PARTIALLY WIRED — never claim more)

- **Production ownership:** `Game` owns a real `FragmentState` (member + `fragmentState()` accessors); `Game::renderFragmentText()` feeds `markdown_unlocked()` into the markdown renderer — the flag now comes from state in production. The stale "zero production callers" note in `fragment_state.h` is corrected.
- **B1a general per-item use seam (not a Fragment-only verb):** new `item_use.{h,cpp}` registry dispatched on the sim thread via `kCmdItemUse` in `signal_world`, routed from `Game::handleWorldActions`. Handlers exist only for already-defined behavior: **SeedOR → silica till**, **silica seed → plant on tilled soil**. The Fragment handler is an explicit **fail-closed, logged "decision-pending" stub**; every unregistered item dispatches to a log + `Failed` return — never a silent fake success.
- **Proofs:** 7 app-selftest item-use checks PASS (defined dispatch mutates the world; dead-silicon refusal; Fragment + undefined + unknown ids fail closed); headless harness `testFragmentOwnership` **5/5** against a real `Game` (fresh state renders plain paragraphs; after a state upgrade the same production render path parses markdown).

**The 4 JD decisions remain entirely open, verbatim from `FRAGMENT-UI-NEEDS.md` / brief C3 — none was touched:**

1. Open verb: dedicated key, "use selected item," or both?
2. Interim upgrade trigger until the Spark exists (console command / certificate-table interaction are the cheap candidates) — or wait for the Spark?
3. Persist `FragmentState` in player.dat (format bump) at the same time as the dialog, or dialog first with session-only state?
4. Gallery: in-book viewer in v1, or capture-only until later?

Consequently: no dialog/opener, no `upgradeFragment()` production caller, no Fragment persistence, no gallery viewer were built. The book still cannot be opened in game; B1 is **partially wired (subset)**.

## A4 — `render.fps_cap`: WIRED

The `frame_pacer.h` recipe followed exactly: `GameSettings::fpsCap` (default **0 = off**) with JSON load/save clamped 0..1000; `render.fps_cap` cvar (Int, clamp 0..1000, archived) + `syncFrom`; `main.cpp` owns a `FramePacer` — `setCapFps` at init, **live reconfigure** whenever settings change (cvar/UI → `Game::onSettingsChanged` → settings → loop), `beginFrame()` at loop top, `endFrameAndWait()` immediately after `window->swap()`.

**Proofs:** 13 "fps cap:" selftest checks, **13/13 PASS in both modes** — default off, JSON round-trip + load clamps (5000→1000, −5→0), pacer no-op when off, clamp 1000, 60 FPS interval via computed sleep 10–25 ms plus a bounded measured check, overrun = zero wait, live disable, cvar set 120/99999→1000/0 through the apply path.

## A5 — Projectile density: WIRED (values JD-DECISION)

Search first: **no JD-approved density table exists** anywhere in tree/docs/reports (only engine-selftest fake walls at 10/200). Per the task instruction, a minimum differentiated **material-class table** was built in `blocks.cpp` (`def()` gained a density parameter; all 65 rows carry values) and is marked in the code comment and here as **JD-DECISION (balance review) — not approved**: non-solids 0 (never consulted), glass 10, earth 20, trace tips 20 / branch 25 / trunk 30, silicon slab 35 / wafer grades 45 / dense 60, stone slab 40 / stone 50, lamps 40, machine/component cubes 45 (incl. Team B's EtchOR), capacitor/dead-memory/certificate 50, resonator crystal 55, structure block 60, ChipOR + viewport 70, ceramic 80.

**Proofs (6 checks PASS):** representative defs carry the table values; coverage invariant (solid ⇔ density > 0) over the whole registry; through the production ChunkMap voxel seam — penetration 30 pierces dirt (20) but stops at ceramic (80), penetration 10 stops at dirt, penetration 90 pierces ceramic. *Note:* `blockContentCrc` includes density, so content CRCs changed (expected; no check pinned the old values). No production code spawns projectiles yet, so pierce behavior is proven via the engine manager + production seam, not live gameplay.

## A6 — AiWorld production path: WIRED

- New game-side owner **AiHost** (`game/ai_host.{h,cpp}` + `ai_host_selftest.cpp`, SpriteHost pattern; `lithokraft_game` already links `litholattice_ai`). `Game::loadActive()` binds it to the active dimension's **live ChunkMap + block registry + JobPool** before `sim_->start()` (spawn-area 3×3 columns prewarmed per the WalkCache contract); dimension switch/destruction unbinds first.
- **Gate:** no pre-existing `ai.*` cvar existed, so one was added following the host-knob pattern: **`ai.enabled`** (Bool, default **0/OFF**, archived). Disabled = nothing constructed, advance is a no-op.
- **Driving:** `Game::update()` advances by elapsed sim world ticks via `advance(simTicks, worldTps)` — never `tick()` per sim tick (the ENT-2 fix direction; engine clamps AI to 8–16 Hz; freezes with the sim). The live player cell is fed as the observer each advance.
- **Proofs:** 12 "ai host:" smoke checks, **12/12 PASS in both modes** — cvar default off; bound+disabled constructs nothing; enable constructs over the fixture live map; `advance(32,32)` = 10 AI ticks; a spawned mob paths and moves; `advance(512,512)` adds 10 ticks (not 512); disable/unbind destroy; re-enable is fresh/empty.

**JD-SCOPE caveat:** no mob content/definitions and no spawn/lifecycle design were invented — the production AiWorld starts empty; what spawns mobs in the live game is JD's scoping call. Columns beyond the prewarmed spawn area compute lazily on first path use (WalkCache's documented contract, same as the perf harness).

---

## Final full battery — coordinator-run on the settled combined tree (Team A + Team B)

| Gate | Result |
|---|---|
| Build (`cmake --build build-linux -j2`) | **exit 0** |
| `--selftest` (legacy) | **1184 PASS / 0 FAIL**, exit 0, "ALL SELFTESTS: OK" |
| `--selftest --timing-handshake` | **1184 PASS / 0 FAIL**, exit 0 |
| `--handshake-test` | **OK** |
| `--control-test` | **OK** |
| `ctest` (serial) | **48/48 PASS**, exit 0 |
| `--perf-headless` circuits, 512 TPS target | **512 achieved in both modes** — legacy avg 0.472 ms / p99 0.660 ms; handshake avg 0.578 ms / p99 1.993 ms (5,531-component default scene, quiet box) |
| Scenario `--handshake-scenario 2048` | **`d609126cc60d71c7` — identical in legacy, handshake 2-worker, and handshake 4-worker arms** (see finding below) |
| `--demo-test` | 11/12 — sole FAIL is the absolute-hash pin (see finding below) |

### Final-battery finding — absolute hashes moved; determinism intact (cross-lane, not an A1 defect)

At A1's port-time gates (~20:00Z) the scenario hash was `faec0f02700d98c6` in both modes and the demo proof's four hashes were `5041cf1795c33a8c`. After Team B's content wiring landed (plant-binding blocks/worldgen and EtchOR/ChipOR content — Team B's own 20:45Z run first measured the new value), the scenario world's content changed: the final tree reads **`d609126cc60d71c7` in every arm** and the demo session's recorded/no-recorder/replay-legacy/replay-handshake hashes **all four agree at `d609126cc60d71c7`**. Every behavioral guarantee (cross-mode determinism, recording non-perturbation, bit-identical replay) holds at the new value. The single failing check is `demo_record_test.cpp`'s absolute pin expecting `5041cf1795c33a8c`. **Recommended follow-up (not done here):** repin that one expected constant to `d609126cc60d71c7` once Team B's content is declared final — editing the pin mid-flight, while the other team's content was still landing, would have chased a moving target.

## Defects found and fixed during the run

1. **A3 `item_use` deadlock (own code, fixed):** `registerDefaults()` re-entered `ensureDefaults()`' `std::call_once` on first dispatch — deadlocked the full selftest (independently reproduced by three runs across both teams). Fixed: defaults now insert directly into the map. All final gates ran after the fix.
2. **A3 `item_use.h` compile error (own code, fixed cross-lane):** unqualified `ChunkMap&` inside namespace `lithokraft`; Worker 1 applied the one-token fix (`litholattice::ChunkMap&`, backup kept) after ~9 minutes blocked; verified and kept by Worker 2.
3. **Build-dir artifact (not source):** an early build failed linking author tools against a stale `liblitholattice_procgen.a` symbol index after concurrent archive steps; fixed with `ranlib` on that one build-dir archive. Builds clean since.
4. **Coordination incident (disclosed):** Worker 2's A2 build once overwrote Worker 1's `team-a-building` marker after its wait window expired while Worker 1 was between build and tests (no test process was running; verified via `ps`). The coordinator's clean final battery above supersedes any mid-window numbers.

Foreign failures seen mid-run (hotbar icon coverage, player.dat v4 round-trip) were traced by diff to Team B's in-flight B6/B7 work and **resolved as Team B completed** — the settled tree's final battery is fully green on both counts.

## Hard exclusions — untouched

Timing Handshake default stays **OFF**; H2/net protocol stays **{3}-only, fail-closed**; signing keys untouched; mod ownership caller identity untouched; Fragment UI's 4 decisions untouched (listed verbatim in §A3).

## Changed files — Team A (44 files: 15 new, 29 modified; verified against the pre-wiring manifest)

Shared files (`selftest.cpp`, `game.cpp`, `game.h`, `main.cpp`, `settings_cvars.cpp`, `world_save.cpp`, `blocks.cpp`) also carry Team B's hunks, merged hunk-wise; the list below is Team A-attributed files only.

- **A1 (18):** `engine/src/sim/control_stream.cpp` (new), `engine/src/sim/demo_record.cpp` (new), `engine/src/sim/include/litholattice/sim/control_stream.h` (new), `engine/src/sim/include/litholattice/sim/idemo_record.h` (new), `engine/src/sim/include/litholattice/sim/sim_thread.h`, `engine/src/sim/sim_thread.cpp`, `engine/src/sim/CMakeLists.txt`, `game/LithoKraft/src/app/control_stream_test.{h,cpp}` (new), `game/LithoKraft/src/app/control_latency.{h,cpp}` (new), `game/LithoKraft/src/app/demo_record_test.{h,cpp}` (new), `game/LithoKraft/src/app/handshake_test.{h,cpp}`, `game/LithoKraft/src/app/CMakeLists.txt`, `game/LithoKraft/src/app/main.cpp`, `game/LithoKraft/src/app/selftest.cpp`
- **A2 (11):** `engine/src/world/include/litholattice/world/chunk.h`, `engine/src/world/include/litholattice/world/chunk_map.h`, `game/LithoKraft/src/content/include/lithokraft/content/signal.h`, `game/LithoKraft/src/content/signal.cpp`, `game/LithoKraft/src/game/include/lithokraft/game/chunk_streamer.h`, `game/LithoKraft/src/game/chunk_streamer.cpp`, `game/LithoKraft/src/game/include/lithokraft/game/world_save.h`, `game/LithoKraft/src/game/world_save.cpp`, `game/LithoKraft/src/game/include/lithokraft/game/game.h`, `game/LithoKraft/src/game/game.cpp`, `game/LithoKraft/src/app/selftest.cpp`
- **A3 (10):** `game/LithoKraft/src/content/include/lithokraft/content/item_use.h` (new), `game/LithoKraft/src/content/item_use.cpp` (new), `game/LithoKraft/src/content/include/lithokraft/content/signal_world.h`, `game/LithoKraft/src/content/signal_world.cpp`, `game/LithoKraft/src/content/CMakeLists.txt`, `game/LithoKraft/src/game/include/lithokraft/game/fragment_state.h`, `game/LithoKraft/src/game/include/lithokraft/game/game.h`, `game/LithoKraft/src/game/game.cpp`, `game/LithoKraft/src/app/selftest.cpp`, `game/LithoKraft/src/headless/harness_selftest.cpp`
- **A4 (6):** `game/LithoKraft/src/game/include/lithokraft/game/settings.h`, `game/LithoKraft/src/game/settings.cpp`, `game/LithoKraft/src/game/settings_cvars.cpp`, `game/LithoKraft/src/game/game.cpp`, `game/LithoKraft/src/app/main.cpp`, `game/LithoKraft/src/app/selftest.cpp`
- **A5 (3):** `game/LithoKraft/src/content/blocks.cpp`, `game/LithoKraft/src/game/game.cpp`, `game/LithoKraft/src/app/selftest.cpp`
- **A6 (9 + 1 cross-lane):** `game/LithoKraft/src/game/include/lithokraft/game/ai_host.h` (new), `game/LithoKraft/src/game/ai_host.cpp` (new), `game/LithoKraft/src/game/ai_host_selftest.cpp` (new), `game/LithoKraft/src/game/CMakeLists.txt`, `game/LithoKraft/src/game/include/lithokraft/game/settings_cvars.h`, `game/LithoKraft/src/game/settings_cvars.cpp`, `game/LithoKraft/src/game/include/lithokraft/game/game.h`, `game/LithoKraft/src/game/game.cpp`, `game/LithoKraft/src/app/selftest.cpp`; cross-lane one-token fix in `game/LithoKraft/src/content/include/lithokraft/content/item_use.h` (A3's file)

*Logs: `~/workspace/wiring-2026-10-13/team-a-worker1.md`, `team-a-worker2.md`, `final-*.log`. Snapshot: `~/workspace/backups/litholattice-pre-wiring-2026-10-13/`.*
