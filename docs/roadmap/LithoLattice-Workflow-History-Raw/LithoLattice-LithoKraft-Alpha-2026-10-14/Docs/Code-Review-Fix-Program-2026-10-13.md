# Code Review Fix Program — 2026-10-13

**Directive:** JD, 2026-10-13 05:18 EDT — "Begin with the fixes of all the
issues after the review. Trust your own judgment and autonomy."
Findings source: `Code-Review-Full-Production-2026-10-13.md` (159 findings:
4 critical · 25 high · 53 medium · 73 low · 4 info).
Target: production tree `~/workspace/litholattice`.
Coordinator: Elfie. Judgment calls are documented per pack, C1–C5 style,
for JD's review.

**Safety:** full pre-program snapshot at
`~/workspace/backups/litholattice-pre-fixprogram-2026-10-13/`
(1,611 files, md5 manifest verified byte-identical against production
before the first edit). Timing Handshake stays **default OFF** for the
whole program — the fixes make it safe to enable; flipping the default
remains JD's call.

## Starting baselines (verified 2026-10-13, pre-program)

| Check | Result |
|---|---|
| Build (build-linux, Make, -j2) | exit 0 |
| `--handshake-test` | OK (24/24) |
| `--selftest` (legacy) | 891 PASS / 6 FAIL on a clean run (known 6: 4× save round-trip comparator, mesher surface sub-chunk, Vulkan environmental). Load flakes observed at baseline too: 2 game-ui script-binding checks + audio-streaming check flake under load (review §0 documents the same class; one baseline run read 889/8 for that reason). |
| `--selftest` (handshake mode) | 891 PASS / 6 FAIL (same known set) |
| Signal suite (subset of selftest) | all signal checks pass (117/117 per sweep accounting) |
| Determinism scenario (2048 ticks) | `faec0f02700d98c6` — legacy 1w/4p, handshake 1w/4p, 4w/4p, 4w/8p — all arms identical |
| ctest | 43/46 (merge verification; known fails: lithokraft_selftest, scout, chipor) |

## Pack log

### P0a — Handshake lifecycle pack ✅ LANDED

**Findings closed:** C-1 (phase re-execution on stop/start), C-2 (stop()
detach → UAF), C-3 (no worker exception boundary), CH-H1 (barrier
defeasible after freeze-reset), CH-H2 (non-atomic publication tuple),
plus A-L7 (stop() global deadline + ceiling×2 overflow — per-worker
deadline, saturating multiply). Detach is gone entirely; the shipped
freeze test's 300 ms sleep workaround is deleted and replaced by
protocol checks.

**Judgment calls (for JD's review):**
- **C2 design = bounded-join + loud failure + poisoned authority**
  (review's option (b)), not shared ownership — shared ownership cannot
  honestly protect the real UAF surface (WorkFn captures of
  SignalWorldSimulation/SimThread). Semantics: per-worker bounded join;
  on timeout `stop()` fails loudly, the authority is poisoned
  (`workersOutstanding()`), `start()` refuses at both authority and
  SimThread level, `~SimThread` deliberately leaks a poisoned authority
  (logged), `stop()` is retryable and clears the poison once the worker
  returns; `~TimingHandshakeAuthority` aborts loudly if destroyed
  poisoned. `SimThread::stop()` now returns `bool`.
- C3: worker records the first failure and still arrives at the barrier;
  `runPhase` converts via `simFailFastFromException` before commit.
  Verified with a standalone throwing-component repro (corruption report
  + abort, no silent `std::terminate`).
- CH-H1: generation bump + per-item and under-`m_` arrival revalidation
  (chosen over parking-wait, which would hang unfreeze forever on a
  truly hung worker). CH-H2: seqlock-revalidated single snapshot.
- New tests: 200-cycle stop/start soak with save-pattern interleaving +
  per-cycle C1 regression (fires == published phases exactly), and a
  stuck-worker stop/poison/retry suite.

**Files changed (5):** `engine/src/sim/timing_handshake.{h,cpp}`,
`engine/src/sim/sim_thread.{h,cpp}` (stop() failure surface only),
`game/LithoKraft/src/app/handshake_test.cpp`. Full log:
`~/workspace/fix-program-2026-10-13/p0a-log.md`.

**Verification observed:** build exit 0; `--handshake-test` OK 5/5 runs
+ 1 under CPU load (WAIT/CH-H4 attribution did not flake this time —
that fix belongs to P1 regardless); selftest legacy AND handshake modes
both exactly the known 6 failures; scenario hash `faec0f02700d98c6` on
all four arms; `ctest -R engine_core|engine_sim` 2/2; ASan standalone
soak+stuck driver clean. Handshake still default OFF.

**Carried forward:** Game-side save/dimension-switch callers still
ignore `SimThread::stop()`'s new bool return — the surface is in, the
callers wire in P2 (game.cpp is that pack's file). Until then a poisoned
stop fails loudly at the engine layer and `start()` refuses, so nothing
corrupts silently; the game just doesn't yet surface the refusal.

### P0b — Permanence pack ✅ LANDED

**Findings closed:** C-4 (CRITICAL), SV-H1, SV-H3, SV-H2, plus C-L6
freebie (loadDimension return count).

- **C-4:** `writeBlockNames` → `writeFileAtomic` (fsync file+dir,
  backup depth 8); reads run `recoverCrashedWrite` behind a fail-closed
  magic/version/CRC validator (`blockNamesIntact`). Fail-closed
  semantics: present-but-corrupt table + region data ⇒ `loadDimension`
  returns **−1** (new documented surface), `loadColumn` refuses saved
  columns, `saveDimension` refuses before writing anything
  (`lastSaveOk()/lastSaveError()`), and `Game::saveWorld` logs FAILED,
  returns false, and skips player.dat. No-table-at-all (no footprint of
  any kind) stays the legacy-identity path.
- **SV-H1:** `RegionFile::flush` commits `pending_` into `records_` only
  after a successful write (kept + retryable on failure);
  `WorldStore::flush()` returns `bool`; `WorldSave::flush()` returns
  bool; `saveWorld` never logs "on disk" for a failed save.
- **SV-H3:** `writePlayer` via `writeFileAtomic`; `readPlayer` recovers
  via magic+CRC validator, promoting the newest intact generation.
  v1/v2/v3 reads and the CRC gate unchanged.
- **SV-H2:** `level.json` implemented —
  `{"saveFormatVersion":1,"seed":"<decimal>"}` (seed as a **string**:
  engine Json numbers are doubles and would round seeds > 2⁵³), written
  in `saveDimension` through the contract; `main.cpp` reads it before
  Game construction and overrides `cfg.seed`; an explicit disagreeing
  `--seed` logs a loud ERROR and the persisted seed wins. Wafer seed-0
  pin untouched. SAVE_FORMAT.md level.json line now reads [v1].

**Judgment calls:** async `flushAsync` cannot honestly return an
outcome (it is a job), so it publishes `lastFlushOk()`/
`lastFlushFailureCount()` and `flush()` is the reporting surface;
fail-closed load surface is `loadDimension == -1` (distinct from 0 =
fresh/legacy world); seed persisted as a decimal string.

**Tests added (all pass):** torn blocks.names heals from backup;
corrupt table + no intact generation + regions ⇒ load AND save refuse,
table untouched; torn player.dat heals from backup; seed round-trip at
a > 2⁵³ uint64 value; flush-failure injection (a directory placed at
the region `.tmp` path — works as root, where chmod would not)
surfaces as failure and pending records land on retry.

**Files changed (10):** storage `block_names.{cpp,h}`, `storage.{cpp,h}`,
game `world_save.{cpp,h}`, `game.cpp` (save surface), `main.cpp` (seed
wiring), `selftest.cpp` (5 new checks), `engine/docs/SAVE_FORMAT.md`.
Full log: `~/workspace/fix-program-2026-10-13/p0b-log.md`.

**Verification observed:** build exit 0; selftest **896/6 in both
modes** (891 baseline + 5 new checks; failure set still exactly the
known 6); `--handshake-test` OK; scenario `faec0f02700d98c6` ×4 arms;
`ctest -R storage|engine_sim` 2/2; **full ctest 43/46** — fails only
lithokraft_selftest, scout, chipor (the three known test-bug suites,
fixed in P2); engine_audio passed in the full run.

### P1 (signal + WAIT) — ✅ LANDED

**P1-Signal — findings closed:** SW-H1, SW-H2, SW-H3, C-M4, C-M5,
B-M3, B-L7.
- SW-H1: new `SignalSim::scanColumn` (per-column loop factored out of
  `scan()`); `loadColumn` surfaces each column's SGNL payload;
  the streamer delivers `StreamedColumn{column, signal, fromDisk}`;
  `pumpStreaming` (sim stopped) scanColumns + importStates every
  inserted disk column. Worldgen verified to place only SimRole::None
  blocks, so generated columns correctly skip the scan. Dimension-load
  path unchanged.
- SW-H2: strength clamped **to** 64 (never zeroed) at every layer —
  `importState` out/out2, both `seed()` paths, pulse application
  points, component-input sampling, all pass-through behaviours, and
  the `tickComponents` output store.
- SW-H3: RAII `JobGuard` decrements in-flight on all exits incl.
  exception unwinding; streamer destructor wait bounded at 5 s with a
  loud log.
- C-M4/M5: shared `imported_` sink replaced by a per-call armed sink
  with load serialisation; streaming remap build logs the
  unknown-block list once, matching loadDimension.
- B-M3: tri-state `LoadedColumn{Loaded/NotSaved/Unreadable}`; the
  session latch is deleted; Unreadable is fail-closed (never generated
  over, never marked handled). B-L7: failed keys un-request with
  exponential backoff (1 s → 30 s cap, no permanent give-up).
- **Gate test:** 10 new `p1-stream` selftest checks — a circuit
  (torch→trace→Memor→trace→lamp) in column (20,20), outside the
  pre-bake square, saved and reloaded through the streaming seam plus a
  real ChunkStreamer/JobPool pass: component memory restores
  bit-for-bit, the lamp relights, a wire value of 100 clamps to 64, a
  corrupt table yields Unreadable. Coverage honesty (logged): the test
  drives the WorldSave+SignalSim+streamer seams — the same calls
  pumpStreaming makes — but does not construct a full Game/SimThread.
- Files: 9 (signal.{cpp,h}, logic_components.cpp, world_save.{cpp,h},
  chunk_streamer.{cpp,h}, game.cpp pumpStreaming only, selftest.cpp).

**P1-WAIT — findings closed:** CH-H4, A-M2, plus the sweep §8.3
empty-name offender diagnostic.
- CH-H4: attribution rewritten — phase wall-time is now a per-group
  *diagnostic only* and charges nobody; a component earns an overrun
  only when the sum of **its own** runItem durations within one cycle
  exceeds that group's **cycle budget** (sweep decision C2 implemented);
  charging is eager with accumulator reset. A bystander cannot be
  flagged: only intervals around its own function enter its sum.
- A-M2: per-group budgets plumbed (group 1 = world-tick period / P;
  group 2 = master-step period, no double division); `HsStats` split
  per group (`perGroup[3]`, totals preserved for existing readers).
- Empty names: root cause was SSO-length names whose `c_str()` pointed
  into a reallocating caller vector; the authority now copies/owns
  every registered name at registration (stable deque storage) and the
  offender log prints that copy; empty registrations get a synthesized
  `unnamed-<id>`.
- Verification: `--handshake-test` **×10: 10/10 OK (50/50 checks)** —
  5 unloaded + 5 under 4×`yes` CPU saturation (load avg 7.24 on 2
  cores); fillers accrued zero overruns in every run. One test
  calibration judgment documented (burn calibrated by min-of-5 samples;
  burner raised to ~40 ms = 2× the new cycle budget).

**Combined central verification (coordinator-run):** build exit 0;
selftest **906/6 in both modes** on a quiet box (896 + 10 new stream
checks; failure set exactly the known 6 — the 2 game-ui script-binding
checks flaked once in a loaded parallel run and pass on re-run, the
documented baseline flake class); `--handshake-test` OK; scenario
`faec0f02700d98c6` ×4 arms. Full logs:
`~/workspace/fix-program-2026-10-13/p1-signal-log.md`,
`p1-wait-log.md`.

**P1-Render — findings closed:** R-H1, R-H2, R-H3, R-H4, R-H5, D-M1,
D-M6, D-L6.
- **R-H1 judgment call: LAND, not de-claim.** Ground traces landed
  end-to-end: GLSL mirror `groundTraceColor()` in chunk_shading.glsl
  (GL33 uniforms now resolve to real locations); Vulkan Frame-block/
  push wiring + `writeUniforms` + Mesh trace storage; new
  `render.ground_traces` cvar (ground_pulse pattern); `frameParams`
  sets time/on from the sim clock; `MeshContext::traceParams` finally
  fed via `WaferPulseCache::traceParams()`; the software backend
  consumes `groundTraceEmissive()` in its per-pixel path. Bonus defect
  found and fixed en route: the VK Frame block's pulse/trace tail was
  std140-misaligned (shipped VK pulse uniforms read zeros) — fixed
  with vec4 fields + name-mapping defines + offset static_asserts.
  Default safety is by construction: zero params ⇒ exactly zero
  contribution on every backend (engine-test pinned), and
  **biome_capture MD5s are identical before/after on all 7 PNGs**.
  Caveat: the VK trace visual is compile-verified only (no GPU on
  this VM).
- **R-H2:** SoftwareRenderer now genuinely implements
  `supportsPbrPath` (true), `setPbrMaterials`, `uploadPbrChunkMesh`,
  `setAmbientMode`, `setFrustumMarginDeg`, `setCullProtoConfig/Stats`;
  the PBR pass shades with real per-vertex material IDs. The classic
  path's 0xFFFF is documented as the JD-locked PackedVertex design,
  not a seam. The one un-mappable piece (transparency-tier ladder)
  log-warns once instead of silently no-oping.
- **R-H3 judgment call: FULL rasterizer.** CPU HUD rasterizer
  mirroring GL33's hud program (buildHudVertices triangles, font-atlas
  coverage, src-over in paint order) plus depth-tested highlight
  outline and on-top crosshair — software mode (the default) now shows
  menus, hotbar, crosshair, and block highlight. Feasibility was
  demonstrable, so the gate-only fallback was rejected as dishonest.
- **R-H4/D-M6:** Vulkan log-once warnings for dropped sprites and PBR
  uploads/materials; HUD overlay truncation log-warns once with
  counts. **D-M1:** software renderFrame frustum-culls via
  CullProtoState with the stored margin (GL/VK parity); drawn stats
  count only submitted meshes.
- Verification (child + coordinator re-run): build exit 0; selftest
  906/6 both modes; handshake-test OK; scenario `faec0f02700d98c6` ×4;
  `ctest -R engine_render|engine_sprite|engine_texture` 3/3; 11 new
  engine_render honesty checks pass; `lithokraft_headless` OK. (The
  child's full-ctest run read 42/46 with engine_audio failing under
  load and passing on serial re-run — the documented flake; the
  coordinator's P0b gate run read 43/46.)
- Full log: `~/workspace/fix-program-2026-10-13/p1-render-log.md`.

### P1b — Console/mod security pack ✅ LANDED

**Findings closed:** N-H1, T-H1, T-M1 (E-M1), F-M6.
- **N-H1:** aliases are now `struct Alias { body, callerMask }`,
  mirroring the keybind fix — the definer's mask is stamped at
  definition and re-installed at expansion, composed with the
  invoker's mask. Composition rule (documented): most-restrictive
  wins (bitwise AND, `kCallerFullAuthority` as identity), re-composed
  at each nesting level so authority only ever narrows; a `wait`
  inside a restricted alias captures the composed mask so suspension
  can't launder it either (regression-tested). **Bonus defect found
  and fixed:** the `alias` builtin folded the alias name into its own
  body (`joinRest(argv)` instead of argv[2..]), so every
  builtin-defined alias recursed to the cap and never ran — the very
  channel the laundering repro runs through.
- **T-H1:** canonical mod hash is now length-prefixed
  (`u64le(count) ‖ [u64le(len(rel)) ‖ rel ‖ u64le(len(bytes)) ‖
  bytes]*`); `signing.h` spec rewritten with framing history and an
  explicit re-issue note (no signatures exist in the wild). The
  Appendix E transplant pair now hashes differently, A's signature
  transplanted onto B is rejected, and normal sign/verify round-trips
  with an ephemeral keypair.
- **T-M1:** parser layer validates range shape (`isValidVersionRange`:
  `*`, bare SemVer, or `>=`/`=`/`^`/`~` + SemVer; empty/short ranges
  are manifest errors, never exceptions); containment layer wraps
  each mod's parse/verify and the whole `scanMods` in try/catch —
  one hostile mod folder can no longer abort boot.
- **F-M6:** new `CommandFlags::session` tier — runs only under full
  authority, independent of cheats (cheat gate untouched); flagged:
  `quit`, `save`, `map`, `debug.defrag`. Screenshot paths confined
  via `resolveScreenshotPath()` to `<config dir>/screenshots`
  (absolute/`..` refused → confined auto-name fallback; resolved path
  always printed). `genmap` deliberately left unconfined (already
  cheat-gated) — documented.
- Verification (child + coordinator re-run): build exit 0; selftest
  **911/6 in both modes** (906 + 5 new security checks; known 6
  unchanged); affected ctest group 8/8; **full ctest 43/46** with
  fails exactly the known set; handshake-test OK; scenario
  `faec0f02700d98c6` ×4. No key material generated or installed;
  test-key refusal and {3}-only offers untouched.
- Full log: `~/workspace/fix-program-2026-10-13/p1b-log.md`.

### P2a — Sim core pack ✅ LANDED

**Findings closed:** CH-H5, CH-H6, CH-H8, A-M1, A-M3, A-M5, A-M6,
A-M7, plus A-L2 (fail_fast.h contract text).
- **CH-H5:** one shared `stepMasterOnce` body (domains/group-2 phase,
  edits, tick, `randomTick` **and** `runRandomTicks`, window/stat
  accounting) now serves both live and stepped branches;
  `stepTicks(n)` advances until exactly n **world** ticks complete
  (verified at worldDiv 16: 3→3, 1→+1, 16→20). The sim_thread.h
  header claim is now true. (The game binding's unconditional "stepped
  N ticks" success report lives in game.cpp — carried to P2b's pause
  machine, which must report the ticks actually advanced.)
- **CH-H8:** `masterStep`/`worldTick`/`dropped`/accumulator/window
  state hoisted to members initialised once at construction; only
  wall-clock pacing re-bases per `run()`. New test proves tick indices
  run consecutively across a stop/start — no keyed-stream replay after
  saves/stream pumps anymore.
- **CH-H6:** the whole loop body runs under `tickGuard` (paused
  applyEdits, publishEvents, activeCount/inspect, castRay fallback);
  `fail_fast.h` now documents the landed boundaries incl. the worker
  boundary from P0a.
- **A-M5:** a group-2 frozen step no longer advances `masterStep`
  (it re-runs on resume); `freezeReported_` resets when the latch is
  observed cleared, so a second distinct freeze reports.
- **Judgment calls:** A-M1 — implemented an explicit `kAllGroups`
  sentinel (caller group 0 now fires only group-0 components;
  production calls only groups 1/2, so this is safe); A-M3 — unified
  upward: `pollPhase()` is the single poll implementation, workerLoop
  uses it, `canTick()` is implemented on it; A-M6 — `statsSnapshot()`
  under a new stats mutex, perf_headless migrated; A-M7 — full CV
  parking for the paused loop with epoch-bumped wake notifications
  (unpause/step/edit/ray/rate/domain/remesh/stop), `waitLoops` gets a
  50 µs backoff instead of a yield spin.
- New engine tests: exact step advancement, stepped runRandomTicks
  picks, stepped-vs-live state equivalence, resume-after-step,
  stop/start continuity, group semantics, stats snapshot, poll
  negatives — all pass ×3.
- Verification: build exit 0; selftest 911/6 both modes; scenario
  `faec0f02700d98c6` ×4; full ctest 43/46 (known 3). `--handshake-test`
  OK in 11/13 executions; the 2 failures occurred when a duplicated
  verify command ran two suites concurrently on the 2-core box (the
  documented WAIT load-sensitivity); all sequential and manual
  under-load runs pass, and the coordinator's quiet-box re-run is OK.
- Full log: `~/workspace/fix-program-2026-10-13/p2a-log.md`.

### P2b-1 — Inventory unification ✅ LANDED

**Findings closed:** G-H1, G-H2, C-M2. The pre-authorized judgment
call (42-slot Inventory is canonical) is implemented:
- **One store:** `Game::hotbar_` is deleted. HUD, selection,
  place/break/pick/drop, the palette, the MCP snapshot, and saving all
  operate on `player_.inventory`; Game keeps only the selection index.
- **Bridge (no new registrations needed):** a stack whose itemId is a
  live BlockId is its own item form and places itself; registered
  items defer to the ContentRegistry `placesBlock` (only ChipOR
  places); tools/Fragment/seed place nothing. Every block already has
  an item form by the id convention, so the "blocks without item
  forms" case does not exist and no fallback store was needed. The
  Fragment and tools now render in the HUD with their real item icons
  — the Fragment is visible in hotbar slot 0 ("The Fragment").
- **player.dat v4:** full 50-slot inventory block (string id + count
  per slot, canonical flat order, empty slots explicit) written on
  the P0b contract; the hotbar is stored as string ids and
  alias-remapped on load (C-M2 closed in the same bump); unknown ids
  drop only their own stack with a log line; v1–v3 files load with
  the old raw-id behavior + empty inventory (Game migrates legacy
  hotbars to full stacks). The boot re-grant comment at game.cpp is
  fixed and now true — no duplicate Fragment across restarts, pinned
  at WorldSave level and at real-Game relaunch level.
- **Palette honesty:** the dialog is retitled "Block palette
  (creative)" with an explanatory note; clicks grant a full stack
  (512) into the real inventory hotbar. Placement consumes from the
  stack, gated on reach so void clicks don't eat blocks (a residual
  sim-rejection wart is documented in the pack log).
- Nine further judgment calls documented in the log §5 (kit layout
  with Switch displaced to main slot 0, PickBlock collects 1, MCP
  hotbar kept as a derived BlockId view, …).
- Verification (child + coordinator re-run): build exit 0; items /
  fragment / headless selftests pass; game_ui checks pass inside the
  main selftest; main selftest **921/6 in both modes** (+10 new
  checks, fail set exactly the known 6); `--handshake-test` OK;
  scenario `faec0f02700d98c6` ×4; full ctest 43/46 (known 3). One
  audio-streaming check flaked once under load and passed on re-run —
  the documented flake class; nothing in this pack touches audio.
- Full log: `~/workspace/fix-program-2026-10-13/p2b1-log.md`.

### P2b-2 — Game correctness + test fixes ✅ LANDED

**Findings closed:** CH-H7, G-M2, G-M1, CAM-1 (E-H1), CAM-3, PLY-1,
PLY-4, ENT-3, G-L5, F-L7, and the test bugs SV-M1 (comparator),
B-L8/D-M9 (mesher check), G-M3 (chipor guard), G-M4 (scout resolver),
G-L6 (partial), G-M7 (temp-dir collisions), plus the P0a stop()-return
wiring carry.
- **Pause machine:** one `Game::PauseState` machine
  (Playing / MenuPaused / TickFrozen / HsFrozen) owns `paused_` and
  the single `sim_->setPaused()` computation. All tick bindings route
  through it; `tick_unfreeze` refuses honestly while a menu is up;
  entering Playing from any frozen state calls
  `resetHandshakeAfterFreeze()` *before* unpausing (the Esc-resume
  re-freeze loop is dead); `update()` mirrors the `hsFrozen` stats bit
  into HsFrozen; `tick_step` reports the actual worldTick delta.
  `SimThread::stop()` failures now abort saveWorld (logError + false,
  nothing written), streaming batches (stop before drain — the batch
  stays queued), and dimension switches, honestly.
- **Spawn:** wafer `P+12 @ z+40` → `P+1 @ z+2` — the old z was off the
  platform entirely (documented judgment call; measured spawn y=266
  on platform top 265); organic `h+12` → `h+1`; respawn inherits the
  fixed point.
- **Camera:** FP `viewPose()` and the `setPose` glue both use
  `playerEyePos` with the locomotion eye height (1.62/1.27/0.4) —
  humans and MCP pilots now see from the same height; stale header
  comment rewritten. CAM-3: Camera syncs cfg distances live until the
  user zooms.
- **Vitals:** `tickVitals` normalises via a 32 Hz sub-accumulator
  (identity at 32 TPS); tests pin equal drain/refill at 32 vs 512 TPS.
  Entities tick from worldTick deltas (64-tick cap) and freeze with
  the world (ENT-3).
- **Test fixes:** `sameCells` compares per-voxel `at()`+state
  (palette `operator==` untouched) — all 4 save checks pass; mesher
  check meshes column (2,2)'s own surface sub-chunk (sy 16 → 2,228
  verts, matching the review probe); chipor guard is registry-derived;
  scout passes the ModelResolver at both sites; spawn check gained an
  upper bound and the DEBUG dump is gone; the Vulkan probe reports
  **[SKIP]** on device-less boxes; selftest temp dirs are unique per
  run.
- **End state (coordinator-verified):** build exit 0; **selftest
  930 PASS / 0 FAIL in both modes**; `--handshake-test` OK; scenario
  `faec0f02700d98c6` ×4; **ctest 46/46 (100%)** in the coordinator's
  gate run (the child's run read 45/46 with only the audio-streaming
  load flake inside lithokraft_selftest, which passes solo — the
  flake class documented since baseline); biome_capture 7/7 frames
  byte-identical (it builds its own eye pose; the FP fix cannot
  affect it — proof table in the log).
- Full log: `~/workspace/fix-program-2026-10-13/p2b2-log.md`.

### P2c — Wiring completions ✅ LANDED

**Findings closed:** ENT-1 (E-H2), F-M9, CAM-2; G-M5 closed as a
documented remainder; G-L7 landed en route.
- **Projectiles are real now.** `litholattice_projectile` is a static
  library: `ProjectileManager` spawn/despawn/tick, swept movement
  substepped at ≤0.5 blocks (a 30 blocks/tick projectile cannot
  tunnel a 1-block wall — test-pinned), pierce iff
  `penetration > density` else stop-at-contact despawn, entity seam
  with owner exclusion, boomerang rules, and `loadChunksAhead`
  through an injectable chunk-request seam with in-manager dedup.
  Game wiring fit cleanly (voxel seam via `SimThread::getBlock` +
  registry density; chunk seam via a new
  `ChunkStreamer::requestColumn`), ticked once per elapsed world tick
  inside `updateEntities()` so it freezes with the world. No spawner
  was invented and Ping is untouched. Honest caveat logged: no game
  block sets `density` yet (all 0). New ctest `engine_projectile`:
  25/25.
- **MCP stdio is wired.** `--mcp-stdio` in main.cpp (default OFF ⇒ no
  server, no thread): `McpServer` built from a new
  `Game::mcpBackends()` (controller/player/vision/audio real; other
  seams null = honestly "not wired"), poll-based POSIX stdio
  transport writing to a saved stdout dup while fd 1 redirects to
  stderr (log output would corrupt JSON-RPC), stop+join before Game
  destruction. The assumed `mcp grant|revoke|agent` console commands
  were added in Game, cheat-flagged so restricted callers cannot
  self-grant. **Live-binary smoke was infeasible for a pre-existing
  reason: the Release binary refuses to boot on the retired test
  signing key** — the guard working as designed; the substitute
  `lithokraft_mcp_wiring_selftest` (real headless Game + real server,
  JSON-RPC round trip, deny→grant→snapshot→revoke) passes 11/11. The
  pack log recommends JD pipe one `initialize` line through a bootable
  (key-installed or Debug) build to close the last inch.
- **Dimensions (G-M5): documented remainder, nothing fabricated.** The
  loader vocabulary cannot express the Cosmic (the Empty, satellites,
  traversal, Underwafer gating), the Underwafer (Sear/hypervisor
  content, interface), or the Void (unbounded, infohazards); an
  empty-air Void registration was considered and rejected as a
  falling-forever trap. Registration stays at 2; the per-dimension
  needs-list is written into `dimensions.h`/`createDimensions()` for
  JD. En route: display name "Organic World" → "Organic" (locked
  one-name convention; id/alias unchanged — G-L7).
- **CAM-2 wired:** ChunkStreamer refactored to a shared request pump
  (`update`/`updateAround`/`requestColumn`, same dedup/backoff/
  32-in-flight cap); `GameCameraController::chunkLoadRequests()`
  exposes non-FP view + pilot camera centers/radii; `pumpStreaming()`
  issues player requests first (structural priority) and clamps
  secondary radii to the player's. Docs corrected: the radius is the
  `camera.chunk_radius` cvar — there is still **no hardware scaling**,
  and the docs now say so.
- Verification (child + coordinator): build exit 0; selftest 930/0
  both modes (two coordinator-side reruns caught the documented
  audio-streaming / script-editor load flakes once each; quiet-box
  runs are 930/0); handshake-test OK; scenario `faec0f02700d98c6` ×4;
  **full ctest 47/47** (46 + the new engine_projectile).
- Full log: `~/workspace/fix-program-2026-10-13/p2c-log.md`.


### P3-1 — Engine core + script/registry tail ✅ LANDED

**Findings closed (18/18, none deferred):** A-L1, A-L3, A-L4, A-L5,
A-L6, A-L8, A-L9, A-M4, F-M1, F-M2, F-M3, F-M5, F-M7, F-L8, F-L10,
F-L11, F-L1-net, F-L2-net.
- **Core/sim:** tick-rate validation accepts only kTickModes ∪ {512}
  and enforces `hz ≤ kMaxDomainHz` fail-fast (A-L1); JSON parse now
  rejects trailing input and non-RFC-8259 numbers — NaN/Infinity/hex
  floats can no longer enter (A-L3); TickArena overflow/moved-from/
  fromHandle holes closed in all builds (A-L4); TripleBuffer lifetime,
  log.h "one record per call", and the QPC tripwire (clock
  monotonicity, not two-source agreement) documented honestly
  (A-L5/L6/L8); `pureSpin` wired via `SimConfig::handshakePureSpin`,
  `setCommitFn` overwrite warns, phaseDivider sentinel 0 = unset so
  explicit 1 wins, HalfCycle header corrected to integer division
  (A-L9); Chase-Lev copy-before-commit + `catch(...)` in workerLoop
  (A-M4).
- **Script/registry/mods/net:** script quota is a single counter
  (`memoryBytes_` deleted; F-M1); AllocHeader holds a
  `shared_ptr<QuotaAccount>` — freeShim never derefs an engine
  (F-M2); registry mutators debug-assert one thread + `itemDef`
  returns by value under shared lock (F-M3); BlockId cap fails
  closed with a logged refusal at 65536 (F-M5); mod ids validated,
  duplicate ids rejected, grants bound to (id, version) — a version
  bump re-prompts by design now (F-M7); sdk double→uint32
  range-checked (F-L8); `ScriptEngine::run` exception-guarded with
  context restore (F-L10); addBlock stable-id collision is a logged
  refusal, not a throw (F-L11); replay window is verify-then-commit
  (peek/commit split; a forged far-future counter can no longer kill
  the session — F-L1-net); `getUVarint64` rejects non-minimal
  encodings (F-L2-net).
- **Judgment calls (documented in the log):** A-M4 — `inFlight_` is
  deliberately NOT decremented on the findJob-throw path: with
  copy-before-commit the job is preserved and still counted, so the
  review's literal wording (written against the old lossy deque)
  would double-release and let `waitIdle` return early; workerLoop
  retries the scan after backoff instead. F-M7 intentionally breaks
  the old "version bump never re-prompts" flow (that was the
  vulnerability); the mods selftest was updated to the new policy
  with a new check pinning the surviving within-version new-bit
  diff. HalfCycle header-corrected rather than behavior-changed.
- **Out-of-file-set fix (flagged by the child, coordinator accepts):**
  one character in `engine/src/assets/assets_selftest.cpp:274` — its
  provenance fixture built JSON with a stray trailing `)` that only
  ever parsed because of the A-L3 defect; the fixture now builds the
  balanced JSON it always intended. No assertion changed.
- Sub-notes for future sweeps (not deferred items): `getUVarint`
  (32-bit) has the same non-minimal acceptance; `parseModJson`'s
  `permissionSet` cast has the same unchecked double→uint32 pattern
  as the fixed sdk cast.
- Verification (child; coordinator central re-run recorded in the
  P3 wave-1 gate below): build exit 0; engine tests all-modules OK
  (12 new P3-1 checks); selftest **942/0 both modes**;
  `--handshake-test` OK; scenario `faec0f02700d98c6` ×4; ctest 47/47.
  Two transient failures (audio-streaming selftest check,
  engine_audio) occurred only while a parallel workstream's
  `ctest -j2` oversubscribed the 2-core box — documented flake class;
  both pass solo/clean.
- Full log: `~/workspace/fix-program-2026-10-13/p3-1-log.md`.

### P3-2 — World/save tail ✅ LANDED (1 item deferred with blocker)

**Findings closed (14):** B-M2, B-M4, B-M5 (short-term half), B-L1,
B-L2, B-L3, B-L4, B-L5, B-L6, C-M3, C-L1, C-L2, C-L3, C-L4,
C-L5 + C-I2.
- **Streaming/save:** streamed insertion is budgeted at 4 columns
  per pump via `takeReadyUpTo` — a 32-column burst no longer lands
  in one sim-stopped window (B-M2); TraceStrengthBuffer is keyed by
  world packPos end-to-end with a `kNoSlot` sentinel and full-pass
  changed-cell parity with the incremental path (still no production
  consumer — B-M4); component iteration order sorted for snapshot
  determinism (B-L1); packPos range constants documented + streamer
  refusal outside them, Void named at-risk (B-L2); RegionFile::open
  refuses files > 64 MiB (C-M3); readPlayer checks `b.done()` with
  a 1 MiB cap and count guards (C-L1); crashed-write `.torn` rotates
  to the first free suffix instead of overwriting the forensic copy
  (C-L2); AnvilRegion quarantines bad headers with a count and is
  marked experimental (C-L3); a no-registry W64 BLKS save now fails
  loud instead of truncating (C-L4); SAVE_FORMAT.md updated to the
  v4 reality, with CRC32 documented as corruption detection, not
  authenticity (C-L5 + C-I2).
- **Worldgen honesty:** nonzero scalar density without a `biomes`
  object now fills 0 + logs loud instead of silently misbehaving
  (wafer.json verified unaffected — B-L3); the tautological river
  test was replaced with a real cross-border replica test (B-L4);
  LightEngine clamps and is marked staged/unwired (B-L5);
  `basesInRect` documented as returning *candidate* bases (B-L6);
  B-M5's short-term half aligns the header claims with reality.
- **DEFERRED — B-M1 (no column eviction), JD decision item.** No
  safe eviction exists in the current seams: ChunkMap has no erase
  API, SignalSim has no per-column un-index, there is no
  mesh-retire seam, and no per-column dirty tracking. Nothing
  behavior-changing was bounded in its place (no caps, no silent
  drops). Quantified growth from the log: ~85–145 KiB per column;
  ~25–42 MB per 289-column area; ~0.85–1.45 GB per 10k explored
  columns, never released. The log holds a 5-step eviction design
  for JD's go/no-go. **B-M5 long-term half also deferred:** the
  world-position probe fix changes generated terrain at chunk
  borders — a worldgen behavior change needing its own workstream,
  not a tail fix.
- Verification (child; coordinator central re-run in the P3 wave-1
  gate below): build exit 0; selftest **942/0 both modes** (+12 new
  checks); `--handshake-test` OK; scenario `faec0f02700d98c6` ×4;
  ctest 47/47. Flakes observed were the documented load classes
  only, clean on standalone reruns.
- Full log: `~/workspace/fix-program-2026-10-13/p3-2-log.md`.

### P3 wave-1 gate (P3-1 + P3-2 combined) — coordinator-verified ✅

Central verification on the combined tree (`verify.sh p3wave1 full`,
quiet box): build exit 0; selftest **942 PASS / 0 FAIL in both modes**;
`--handshake-test` OK; scenario **faec0f02700d98c6 on all four arms**
(re-confirmed with a direct full-hash run — the verify.sh grep prints
only the trailing 8 hex chars, a harness display artifact, identical
across arms); ctest under `-j2` read 46/47 with only `engine_audio`
failing — the documented load-sensitive flake class; the coordinator
re-ran `engine_audio` solo on the same tree and it **passes (47/47
solo-verified)**. Manifest
`manifest-after-p3-wave1.md5`: exactly the 47 files the two pack logs
declare, no adds/removes — the parallel packs stayed inside their
file sets (plus P3-1's one flagged fixture character).

### P3-3 — Render/assets tail ✅ LANDED

**Findings closed (20):** D-M2, D-M5, D-M7, D-M8, D-L1, D-L2, D-L3,
D-L5, D-L7, D-L8, E-M2, E-M3, E-M4, E-L1, E-L2, E-L3, E-L4, E-L5,
E-L6, E-L7, E-L8 — plus bonus **E-M5** (SVG float→int UB; an
Appendix E medium assigned to neither pack, fixed in-set in a file
already being edited). **E-L9 closed-as-documented** (ed25519
small-order documented only, no crypto behavior change — as
directed). **2 conservative defaults:**
- **D-M3 (`render.fps_cap`)**: the engine half is real — new
  header-only `render/frame_pacer.h`, a deadline pacer, default OFF,
  clamp 0..1000, no catch-up debt, 5 checks passing. The
  cvar/Settings/main-loop wiring lives in game files outside this
  pack's file set; the exact wiring recipe is written in the header
  as a follow-up for the game owner. Not silently dropped — flagged
  here and in the header.
- **D-M4 (mesher defs epoch)**: conservative form landed — the
  mesher captures `definitionEpoch()` before/after a pass, re-meshes
  once on a mid-pass refresh, logs loud on a second, and the
  contract is documented in `mesher.h`. A full epoch mechanism was
  assessed as beyond tail scope.
- Highlights: software sprite pass corrected (atlas-rect averages
  cached per atlas version/page/rect; AlphaBlend/Additive blend in
  producer order with true average color, depth-test without
  depth-write; cutout path byte-identical; 3 pixel-pinned checks);
  GL sprite VBO grow-only; transparency-tier claims in both headers
  now state exactly GL33 + BinaryQuad availability; FXAA genuinely
  edge-gated; .ltxr caps (256 nodes / 64 stops / 1 MiB) + numeric
  bands verified against all shipped recipes; file-size caps
  enforced *during* reads (Image 64 MiB, pack slurp 16 MiB);
  verify-time symlink refusal in both signed-mod verify paths
  (unsigned dev flow untouched, test-pinned); SvgRasterCache
  LRU-128 + mutex + canonical key; provenance extended to
  animations/palettes and documented in RESOURCE_PACKS.md, converter
  handles .svg without inventing provenance stamps. E-L2 judgment
  call: O_NOFOLLOW + fchmod 0600, deliberately **no O_EXCL** to
  preserve keygen re-issue.
- Verification (child): build exit 0; `--handshake-test` OK;
  scenario `faec0f02700d98c6` ×4; **biome_capture 7/7 byte-identical**;
  in-area ctests 5/5. Its full-ctest run read 46/48 under concurrent
  P3-4 builds: `engine_audio` (documented load flake, passes solo)
  and one `lithokraft_selftest` check in game block registration —
  P3-4's area, mid-edit at the time; reconciled at the wave-2 gate
  below (it was P3-4's own mid-pack test regression, caught and
  fixed by P3-4 itself).
- Full log: `~/workspace/fix-program-2026-10-13/p3-3-log.md`.

### P3-4 — Console/net-docs/game/entity tail ✅ LANDED

**All 26 items addressed.** Closed in code: F-L3 (MCP net://version
→ `net::kProtocolVersion`, pinned == 3), F-L5 (unterminated `/*`
prints an error), F-L6 (bumpCVar fails closed on non-finite
delta/current/result), F-L9 (MCP HTTP Origin/Host allowlist before
auth → 403, pinned ×3), H-ENT-4 (FallingBlock 60 s / y<−1024
despawn), H-ENT-5 (persistent sorted entity ids + behaviour
exception boundary; new `engine_entity` ctest suite), H-ENT-6
(moveCollide substep ≤0.5 blocks, frame-dt proven bit-identical),
H-AI-1 (self-despawn regression test — code was already fixed, now
pinned), H-AUD-1 (`gain()` genuinely lock-free via published
snapshot — the header claim is now true, not just corrected),
H-AUD-3 (setInline(false) restarts at Full), H-AUD-5
(perceived-volume sort), H-AUD-6 (future-tick underflow guard),
H-DEF-1 (nullptr refused), H-ANIM-1 residual (two-bone rig
finiteness → no solution, matching CCD/FABRIK).
- **Docs/comments only:** F-M8 (scaling.h/version.h STATUS:
  multiplayer not implemented, anti-cheat unimplemented — stated,
  no behavior change), F-L4 (handshake guidance documents the
  current {3}-only fail-closed offer; nothing installed, no v1
  codec — JD's decision preserved), F-L12 (mod_manifest canonical;
  mod_loader marked staged/unwired), G-L1 (append warning), G-L2
  (kFusor debris cleaned, behaviour-neutral — Fusor's role is
  undefined in design), G-L3 (no-op patch removed; false "game
  cannot start" docs corrected in all 3 files), G-L4 (README
  accuracy: 8-slot hotbar, Fragment-first start kit, gravity,
  Thresholdor/Pulsor/Memor, "Organic"), CAM-4 (FP-bypass scope),
  H-AI-2 (guard comment).
- **Documented remainders by design:** F-M4 — namespace format
  validation implemented in registerBlock (malformed ids refused,
  test-pinned), but per-registrant ownership *enforcement* cannot
  be real today (no caller identity exists; the loader is unwired)
  — the contract and the exact future requirement are documented at
  registerBlock/blocks.h rather than faked. G-M6 — Fragment UI
  needs-list written to `game/LithoKraft/docs/FRAGMENT-UI-NEEDS.md`
  (7 missing pieces + 4 decisions for JD); no UI half-built. PLY-6
  note-only, recorded in the log.
- **Honesty flags (worker's own, preserved):** (1) P3-4 caught and
  fixed one regression of its own mid-pack — its first F-M4 test
  registered a `testmod:widget` block into the shared registry,
  breaking the save selftest's unique-names check (the failure
  P3-3's run observed); the test now uses first-wins re-registration
  with no append, and all final numbers are post-fix. (2)
  `verify.sh`'s grep|tail summary lines can mix section counts —
  its mid-pack summary claimed "944/0" while its own logs contained
  that FAIL. The coordinator's final gate below therefore derives
  counts directly from the raw logs, not the script's summary.
- Verification (child, solo quiet-box, counts re-derived from fresh
  runs): build exit 0; selftest **958 PASS / 0 FAIL both modes**;
  `--handshake-test` OK; scenario `faec0f02700d98c6` ×4; **ctest
  48/48** (47 + new `engine_entity`).
- Full log: `~/workspace/fix-program-2026-10-13/p3-4-log.md`.

### P3 wave-2 gate + FINAL CLOSE-OUT ✅ PROGRAM COMPLETE

Central verification on the combined final tree (`verify.sh final
full`; counts derived directly from the raw logs, per P3-4's note
about the script's summary lines): build exit 0; selftest **958
PASS / 0 FAIL in both modes** (grep-counted from
`logs/final-selftest-*.log`); `--handshake-test` OK; scenario
**faec0f02700d98c6 on all four arms**; ctest — the `-j2` run read
47/48 with only `engine_audio` failing (the documented
load-sensitive flake), and the coordinator's **serial ctest run
reads 48/48 (100%)** on the same tree. The P3-3-flagged
block-registration failure is reconciled: it was P3-4's own
mid-pack F-M4 test regression, caught and fixed by P3-4; the check
passes in the final tree. Final manifest `manifest-final.md5`:
1,618 files (+3 creations vs wave 1 — `entity_selftest.cpp`,
`frame_pacer.h`, `FRAGMENT-UI-NEEDS.md`, exactly the files the
wave-2 logs declare), 68 changed, 0 removed.

## End-state baselines vs starting baselines

| Check | Program start (2026-10-13) | Program end |
|---|---|---|
| Build | exit 0 | exit 0 |
| Game selftest (legacy) | 891 PASS / 6 FAIL | **958 PASS / 0 FAIL** |
| Game selftest (handshake mode) | 891 PASS / 6 FAIL | **958 PASS / 0 FAIL** |
| Signal suite | 117/117 | all signal checks PASS (inside selftest, 0 FAIL) |
| `--handshake-test` | OK (24/24) | OK |
| Determinism scenario (2048 ticks, 4 arms) | `faec0f02700d98c6` | **`faec0f02700d98c6` — unchanged** |
| ctest | 43/46 | **48/48** (serial; new suites: engine_projectile, engine_entity, lithokraft_mcp_wiring_selftest) |
| Timing Handshake default | OFF | **OFF — unchanged (JD's call)** |

## Program summary

- **159 review findings** (4 critical · 25 high · 53 medium · 73
  low · 4 info) processed across 14 packs: P0a, P0b, P1-signal,
  P1-WAIT, P1-render, P1b, P2a, P2b-1, P2b-2, P2c, P3-1, P3-2,
  P3-3, P3-4. **No pack was reverted**; the revert rule never
  triggered. All 4 criticals and all 25 highs are closed. Every
  pack gate held the determinism hash bit-identical.
- The 6 starting selftest failures are all resolved — 4 were
  test-comparator bugs, chipor/scout/mesher were test bugs, and
  the Vulkan probe now reports [SKIP] on device-less boxes. The
  selftest grew 891 → 958 checks as packs added pinning tests.

## Deferred / JD-decision items (nothing silently dropped)

1. **B-M1 column eviction** — deferred with blocker (no ChunkMap
   erase, no SignalSim per-column un-index, no mesh-retire seam,
   no per-column dirty tracking). Quantified growth + 5-step
   design in `p3-2-log.md`. JD go/no-go.
2. **B-M5 long-term half** — world-position probe fix changes
   generated terrain at chunk borders; needs its own workstream.
3. **D-M3 remainder** — `render.fps_cap` game-side wiring (cvar +
   Settings + main-loop pacer drive); recipe written in
   `frame_pacer.h`. Engine half landed, default OFF.
4. **F-M4 ownership enforcement** — format validation landed;
   per-registrant enforcement awaits a caller identity in the
   (unwired) mod loader; contract documented at registerBlock.
5. **G-M6 Fragment UI** — needs-list at
   `game/LithoKraft/docs/FRAGMENT-UI-NEEDS.md` (7 missing pieces +
   4 decisions for JD). No UI half-built.
6. **Dimension content** (Cosmic / Underwafer / Void) — needs-list
   in `dimensions.h` from P2c; loader vocabulary cannot express
   them yet.
7. **MCP live stdio smoke** — wiring verified via
   `lithokraft_mcp_wiring_selftest` 11/11; a live-binary smoke
   needs JD's real signing keypair installed (or a Debug build),
   because Release correctly refuses to boot on the retired test
   key.
8. **H2 interop** — current offer is {3}-only, fail-closed, now
   documented accurately (F-L4). Installing {1,2,3} + writing the
   missing v1 codec remains JD's product decision; NOT done.
9. **Release signing key** — JD's offline action; the refusal
   stays intact. T-H1 mod-hash framing means existing mod
   signatures must be re-issued when signing begins (none in the
   wild).
10. **Block density values** — projectile pierce is implemented
    but no block sets `density` yet (all 0); a content call.
11. **Timing Handshake production enable** — lifecycle-safe after
    P0a/P1-WAIT/P2a, still DEFAULT OFF. Flipping it is JD's call.
12. Future-sweep sub-notes from P3-1: `getUVarint` (32-bit)
    non-minimal acceptance; `parseModJson` `permissionSet`
    unchecked double→uint32 cast (same pattern as the fixed
    F-L8 sdk cast).

*Program run under JD's 2026-10-13 05:18 EDT directive. Every
judgment call is documented in the per-pack sections above and in
the pack logs under `~/workspace/fix-program-2026-10-13/`.*
