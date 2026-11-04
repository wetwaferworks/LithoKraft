# Timing Handshake — In-Game Full Sweep Report

**2026-10-13 · Experiment tree only: `~/workspace/experiments/pull-tick/`**
**Production tree `~/workspace/litholattice` untouched — proof in §10.**
**Algorithm:** the Timing Handshake, designed by **David "JD" Rittenhouse**
(recovered from his teenage game-hacking work; dictated 2026-10-12/13).
**Executed by:** Elfie and crews, under JD's delegation (2026-10-13 00:13 EDT:
*"run the full test in game with the software render, do a full sweep on
this — even those open questions formulate your own answers and get
additional credits"*).

Working records: `hs-design.md` (integration design, decisions D1–D12),
`hs-baseline-legacy.md` (pre-integration runbook + baselines),
`hs-integration-notes.md` (implementation, decisions D-a…D-n, gate results),
`hs-sweep-results.md` + `hs-run-logs/` (sweep raw logs) — all in the
experiment tree.

---

## 0. Verdict up front

The Timing Handshake is now **integrated into the real engine and game**,
behind a runtime switch, and it passes the full sweep:

- **Determinism holds in-game at full scale.** The 2,048-tick scripted
  circuit scenario produces the **identical outcome hash
  `faec0f02700d98c6`** under legacy dispatch, under the handshake at
  1 worker / 4 phases, 4 workers / 4 phases, 4 workers / 8 phases — and
  again on a repeat run. Bit-identical replays, bit-identical A/B.
- **No regressions.** Game selftest failure set is unchanged in both modes
  (891 PASS / 6 FAIL, the known six); signal suite 117/117 in both modes;
  engine sim 39/39; ctest improved 41/46 → **42/46** (the baseline's
  flaky `engine_sim` full-run timeout did not recur; remaining failures
  are the known set + a tree-location artifact).
- **Both delegated policies are proven in code, in the real SimThread:**
  WAIT (phases extend, overruns counted and attributed, outcome
  unchanged) and FREEZE (watchdog froze the sim via the existing
  tick-freeze path, diagnostic names the hung component, and the world
  **saved while frozen and reloaded to the exact pre-freeze hash**).
- **Performance on this 2-core VM: a modest, honest cost, no win yet.**
  At 512 TPS, handshake tick avg is +1.3% (default scene), +15.8%
  (wire-torture), +18.2% (display-480) vs legacy, with p99 +16–44% —
  measured with **one** pull worker (auto on 2 cores), where the
  barrier has almost nothing to parallelise with. The prototype's
  projected win condition (heavy per-fire work × real cores) is
  unchanged and still unmeasured on suitable hardware.
- **Software renderer frames are MD5-identical** before/after the
  integration — worldgen and rendering provably untouched.

Mobs/entities were **not** touched anywhere (JD's locked scope:
*"otherwise they run at 512 and the bones break"*).

---

## 1. Integration summary (experiment tree only)

Implemented per `hs-design.md` (full file list in
`hs-integration-notes.md` §1). The shape:

- **New engine module** `engine/src/sim/timing_handshake.{h,cpp}` in
  `litholattice_sim`: `TimingHandshakeAuthority` owns the pull workers.
  Components register (`HsComponentDesc`: id, name, cadence class /
  phase divider, thread-affinity, tick-critical) with static round-robin
  shards. Workers run the infinite poll loop — *"can I tick? is it time
  to tick?"* — against `canTick()`, an atomic read of the
  authority-published phase word; both sides stamp from
  `std::chrono::steady_clock` (CLOCK_MONOTONIC; the Windows equivalent
  is QPC — JD's "two synchronized QPC calls"), with a `qpcViolations`
  tripwire counter (0 in every run). `runPhase()` = open → affine
  prologue → barrier → single commit hook.
- **SimThread is the authority.** Clock domains register as cadence
  items (`phaseDivider = masterHz / hz`); edits stay tick-quantised
  before the cycle; random ticks stay legacy (out of locked scope).
  Legacy mode constructs **nothing** — the old path is byte-for-byte.
- **Signal decomposition at its existing seams.** `SignalSim::tick()` is
  now literally a sequence of `hs*` methods that both modes call; the
  cycle's 4 phases map 1:1 onto the sim's existing sub-steps
  (flip+torches / components / mark+prep+propagate / lamps). The
  snapshot/commit boundary the prototype required already existed —
  double-buffered radio/jammer registries (flip moved to the commit
  hook in both modes; hash-proven equivalent) and the wires'
  `NetArr::old[]` shadow.
- **Thread-affinity, evidence-based.** Pull threads run only the two
  provably disjoint-write kernels: the incremental per-network wire
  flood and lamp batches. Torches, components (in-place
  `ComponentMemory&` mutation + live-world reads), marking, and
  full-pass propagation stay on the sim thread. Pull workers never
  receive the real `DirtyFn` (it cascades into block writes); they
  collect dirties per-shard for canonical replay after the barrier.
- **TraceStrengthBuffer verdict (the design's one open item):**
  `TraceStrengthBuffer::set` is **not** a plain slot store — it mutates
  a shared dirty-word BitSet and inserts into a shared slot map.
  Concurrent per-net calls would race. The design's flagged fallback
  was implemented: per-net pending strength lists, replayed on the sim
  thread after the barrier in exactly the legacy order.

### The switch

- Cvars `sim.timing_handshake` (Bool) and `sim.handshake_phases`
  (Int, 1..16), archived, following the `settings_cvars.cpp` pattern;
  applied at world load (SimThread construction), not live.
- CLI `--timing-handshake` / `--no-timing-handshake` /
  `--handshake-phases N` (the `--no-euv` precedent), threaded into the
  selftest, perf harness, and scenario harness.
- Precedence CLI > cvar > default; **`SimConfig` default OFF** — an
  unconfigured SimThread sees zero behaviour change.

## 2. Policies in code (JD-decided, not re-opened)

- **WAIT** (`hs-design.md` §1.6): phase budget = master-step duration.
  An overrunning phase **extends**; nothing is ever skipped
  (deadline-skip was proven to diverge in the prototype). Overruns are
  counted per phase and per component (slowest component of the
  slowest shard), and a component crossing 3 overruns is flagged
  repeat-offender with a one-time log line naming it.
- **FREEZE watchdog** (§1.7): ceiling default **250 ms**, checked
  inside the barrier wait. On trip, `runPhase` returns false with
  `HsFreezeInfo` naming the slowest unfinished component; `SimThread`
  calls its existing `setPaused(true)` — the exact tick-debugger
  freeze path — and logs a `SimCorruptionReport`
  (`what="timing-handshake-watchdog"`) via the fail-fast *reporting*
  path, deliberately **without** `simFailFast`: the world state is
  consistent (nothing committed past the last barrier) and stays
  savable. Frozen step requests are refused; recovery runs through
  `tick_unfreeze` → `resetHandshakeAfterFreeze()`.

## 3. Sweep results vs baselines

Pre-integration baselines are the experiment tree's own (Debug build,
2-core VM; production Release reference in parentheses where the
*sets* are comparable). Full logs in `hs-run-logs/`.

### 3.1 Game selftest, signal suite, engine tests

| Suite | Baseline (legacy) | Post-integration legacy | Post-integration handshake |
|---|---|---|---|
| Game selftest | 887 PASS / 6 FAIL | **891 PASS / 6 FAIL** | **891 PASS / 6 FAIL** |
| Signal suite (in selftest) | 117 / 117 | 117 / 117 | 117 / 117 |
| Engine sim tests | 39 / 39 | 39 / 39 | (mode-independent) |

The +4 PASS are the new Timing Handshake selftest section's own
checks. The 6 failures are **exactly the known set in both modes**:
4× save round-trip bit-for-bit, mesher surface sub-chunk, Vulkan
headless (environmental, no GPU). No new failures anywhere; no delta
between modes.

### 3.2 Full ctest

| | Baseline | Post-integration |
|---|---|---|
| Result | 41 / 46 (1604 s) | **42 / 46 (705 s)** |
| Failures | `engine_systerian`, `engine_sim` (timeout, full-run only), `lithokraft_selftest`, `lithokraft_scout_selftest`, `lithokraft_chipor_selftest` | `engine_systerian`, `lithokraft_selftest`, `lithokraft_scout_selftest`, `lithokraft_chipor_selftest` |

- `engine_sim` **passed in the full run this time** — the baseline's
  900 s timeout was a contention flake on the 2-core box, as suspected
  (it passed isolated then, and passes in-suite now).
- `engine_systerian` fails identically in both runs: the test resolves
  reference SVGs relative to the tree location — a tree-depth artifact
  of the experiment copy, not a code failure.
- `lithokraft_scout_selftest` (2 checks: wafer generator / ModelResolver
  for `litho_starfish`) and `lithokraft_chipor_selftest` (1 check:
  registry-size guard desync) are the known pre-existing items from
  the production ledger. `lithokraft_selftest` carries the known 6
  checks from §3.1.
- **Delta vs baseline: +1 test, zero new failures.**

### 3.3 Software-render game sweep

`biome_capture` (the game's software-renderer capture route) was run
fresh post-integration: 6 biome frames at 1280×720 + the wafer overview
map. **All 7 PNGs are MD5-identical to the baseline captures**
(coordinator-verified). Plainly stated, per the integration notes:
`biome_capture` never constructs a SimThread, so frames cannot show
handshake effects — the handshake-active in-game evidence is the
game-binary runs (§3.1, §4, §5, §6), and the frames prove the
integration left worldgen and the software renderer byte-for-byte
alone. (`lithokraft_headless` uses a null renderer and was not used
as frame evidence.)

## 4. In-game determinism evidence

Scenario (`hs-design.md` §6.4): `buildCircuitScene(…, 120)` on the
wafer, area (4,4) r=8, 512 TPS, exactly **2,048 world ticks**, with
tick-indexed scripted inputs (trace break at tick 100, repeater `use()`
at 120, trace replaced at 150 — applied through
`SignalWorldSimulation` in both modes so bookkeeping is identical;
deviation D-h in the integration notes explains why wall-clock-drained
`SimThread::edit` stamps cannot be used for a determinism scenario).
Outcome hash = FNV-1a over all world cells in sorted order +
`SignalSim::exportState()` bytes + component count + tick count.

| Arm | Hash |
|---|---|
| Legacy | `faec0f02700d98c6` |
| Handshake, 1 worker, 4 phases | `faec0f02700d98c6` |
| Handshake, 4 workers, 4 phases | `faec0f02700d98c6` |
| Handshake, 4 workers, 4 phases — **repeat run** | `faec0f02700d98c6` |
| Handshake, 4 workers, 8 phases | `faec0f02700d98c6` |

All five arms **bit-identical**: legacy == handshake, worker count and
phase count invisible to the outcome, and a handshake re-run replays
bit-for-bit. The 256-tick gate during integration gave the same
all-arms-equal result (`ead266df50800279`).

## 5. Policy exercises (in-game)

`./LithoKraft --handshake-test`, re-run post-integration in the sweep:
**24/24 PASS, exit 0** ("HANDSHAKE TESTS: OK").

- **WAIT (7/7):** a deterministic `BurnComponent` calibrated to ~20 ms
  against a 5 ms phase budget: all 64 phases still commit; the outcome
  hash is identical to the no-burn run; overrun phases are counted; the
  burner is attributed and flagged repeat-offender at threshold 3; no
  filler component is flagged; 0 QPC violations.
- **FREEZE, bare authority (7/7):** a `HangComponent` due in phase 3
  with a 50 ms test ceiling: `runPhase` returns false within
  ceiling+slack; a second call returns false immediately (latched —
  never force-advances); `HsFreezeInfo` names the component with
  `elapsedNs ≥ ceilingNs`; the pre-freeze committed hash matches a
  clean run's hash at the same point.
- **FREEZE, end-to-end in the real SimThread (6/6):** hang injected as
  a registered item → `SimThread::paused()` becomes true via the
  debugger path; the log carries the report (verbatim: *component
  'test.hang' (id 8) did not finish phase 0 (cycle 0): elapsed
  50033235 ns > ceiling 50000000 ns*); the circuit world is then
  **saved while frozen, reloaded, and hashes to the pre-freeze
  committed hash** — the world-permanence assertion. The latch is
  released at teardown so the process exits cleanly.
- **Straggler in the real solver:** the 512 TPS perf runs (§6) drove
  WAIT-extended phases through the real signal solver with overrun
  accounting live — ~84% of cycle phases on the default scene, ~15–18%
  on wire-torture, ~4–5% on display-480 (see §6.3 for why the default
  scene's figure is a budget-definition artifact, not a solver
  problem). The convoy behaviour the prototype measured (~1:1 phase
  extension) is the production behaviour, and it never perturbed an
  outcome anywhere in the sweep. Repeat-offender flags fired in the
  real-solver logs for `signal.components`, `signal.flip_torches`,
  `signal.net.shard0`, and `signal.lamps.shard0` — plus one diagnostic
  defect worth fixing: components id 2 and 3 (`signal.mark` /
  `signal.prep` by registration order) are flagged with an **empty
  name `''`** — a name-propagation bug in the offender diagnostic
  (diagnostic-only; recorded, not fixed).

## 6. Performance in engine (512 TPS target)

Method: `perf_headless` circuits section extended per design §7
(phases execute inside the `sim_->tick()` call frame in both modes,
so the same timers measure the same thing); every configuration run
**twice**, second run reported; 2-core VM, Debug build, auto worker
count = **1**; jitter ±30% applies throughout. Full extracted tables:
`hs-run-logs/sweep-perf-extracted.txt`.

### 6.1 Headline: 512 TPS rows (run 2)

| Scene (components) | Mode | Achieved TPS | Tick avg ms | Tick p99 ms | Δ avg | Δ p99 |
|---|---|---|---|---|---|---|
| Default circuits (5,531) | legacy | 194.3 | 4.452 | 5.207 | — | — |
| | handshake | 192.0 | 4.510 | 6.774 | **+1.3%** | **+30.1%** |
| Wire-torture (1,445) | legacy | 512.0 | 0.807 | 0.961 | — | — |
| | handshake | 512.0 | 0.934 | 1.380 | **+15.8%** | **+43.5%** |
| Display-480 (7,344) | legacy | 501.3 | 1.204 | 15.308 | — | — |
| | handshake | 427.0 | 1.423 | 17.772 | **+18.2%** | **+16.1%** |

Run-1 variance check (same matrix): default-scene handshake p99 was
*better* than legacy (6.26 vs 9.58 ms) and wire-torture handshake beat
legacy on both avg-adjacent and p99 (1.089 vs 1.481 p99) — the
run-to-run spread on this VM is the same size as the effect. Read the
table as: **a small real avg overhead, p99 within jitter of worse.**

### 6.2 Why — the phase split tells the story

- **Default scene is components-dominated**: of the ~1,090 ms phase-ms
  total per measurement window, ~1,010–1,030 ms is `comps` — which is
  thread-affine in v1 (§1). The handshake's parallel kernels there are
  wires (~45 ms) + lamps (~1.5 ms): **~4% of the tick**. There is
  almost nothing to win, and neither mode reaches 512 TPS (both ~192–
  201 achieved) because the scene is CPU-bound in affine work.
- **Display scene is lamps-dominated** (~1,240–1,435 ms of ~1,700):
  lamps *are* a pull kernel — but with **one worker**, the kernel
  serialises onto the worker while the sim thread waits at the
  barrier (mean barrier wait ~170–190 µs/phase there, vs ~8–10 µs on
  the default scene). Concurrency needs a second worker and a third
  core; this box has neither to give.
- **Wire-torture is wires-dominated** (~830–900 ms of ~1,000): the
  flood is parallel, but again single-worker; barrier mean ~36–40
  µs/phase is the visible protocol cost.

**Honest bottom line:** on JD's eventual hardware (a real multicore
machine, Release build, ≥2 workers) the architecture has something to
win with; on this VM it pays a 1–18% avg tax for the phase protocol
and buys determinism-identical behaviour, cadence phases, WAIT/FREEZE
accounting, and genuinely free-running non-critical systems. That
trade is exactly the one the prototype predicted.

### 6.3 Overrun accounting at rate (with the caveat)

At 512 TPS the default scene recorded **1,932–1,997 overrun phases**
(~2.1 s cumulative overrun ns, ~84% of cycle phases) per 3 s run;
wire-torture ~947–1,121 (~15–18% of phases), display-480 ~218–272
(~4–5%). Context, carried from the integration notes: the per-phase
budget at 512 Hz / P=4 is ≈488 µs, while a full cycle's work in a
Debug build is ~4.4 ms — so on the components-bound scene most phases
"overrun" by construction. The accounting is honest; the *budget
definition* is what needs JD's eye before offender flags are read as
signal at high TPS (see decision C2 in §7).

## 7. Decisions made under delegation

Pre-decided and implemented as specified (not listed): WAIT policy,
FREEZE watchdog, 4 phases default + tunable, circuit-only scope, mob
exclusion, switch required, determinism above performance.
Design-stage decisions D1–D12 are in `hs-design.md` §8;
implementation-stage decisions D-a…D-n are in
`hs-integration-notes.md` §3 (publication sequence, affine-as-prologue,
commit-hook group parameter, freeze-callback invocation point,
`signal.prep` as its own affine item, full-pass in the commit hook,
no `stepOnce` refactor, tick-indexed scenario inputs, sorted lamp
order in both modes, per-net pulse epoch, sub-4-phase legacy fallback,
freeze-recovery semantics, settings persistence, perf timers inside
the `hs*` methods). The coordinator's own calls:

- **C1 — Freeze recovery = abandon the frozen cycle + full-pass
  rebuild** (ratifies D-l). The design's alternative — re-running the
  frozen phase from its snapshot — is **not snapshot-safe in v1**:
  the phase's affine prologue has already executed against live
  state. Abandoning the cycle and forcing a full pass on the next
  tick rebuilds nets and re-floods from committed state, which is
  determinism-preserving by construction. Phase re-run stays an open
  item for a future snapshot-true design, not a v1 promise.
- **C2 — Offender flags should be judged against the cycle budget,
  not the single-phase budget.** The four phases are one logical
  tick; a phase budget of (master step ÷ P) guarantees false
  offenders whenever any single sub-step legitimately exceeds a
  quarter-tick (at 512 TPS in Debug: nearly always). Per-phase
  overrun *counts* remain useful diagnostics; the repeat-offender
  *flag* should compare a component's work against the full cycle
  budget. Recorded here as the decided interpretation; the one-line
  accounting change is follow-up work, flagged in §8.
- **C3 — TraceStrengthBuffer pending lists are the permanent
  design**, not a stopgap: they preserve legacy application order
  exactly and confine all shared-structure writes to the sim thread.
- **C4 — Poll policy stays spin-then-CV hybrid by default** (D9),
  pure spin available via `Config::pureSpin` for A/B. The barrier,
  not the poll, is the synchroniser; determinism is unaffected.
- **C5 — Worker auto-count stays `min(4, cores − 1)`.** The sweep's
  perf tax is a 2-core artifact; capping differently would tune the
  engine to the test VM instead of to players' machines.

## 8. Open risks / follow-ups

1. **Perf on real hardware is still unmeasured.** The win condition
   (heavy per-fire work × ≥2 workers × Release) needs JD's machine.
   A forced-`workerThreads=2` perf arm needs a one-line
   `BenchSceneConfig` addition (noted in the integration notes).
2. **Components phase is the long pole.** Parallelising
   `tickComponents` (per-component input snapshots + deferred apply)
   is where the default scene's ~95% affine time lives; it is the
   correctness-critical core and was deliberately left affine in v1.
3. **Cycle-budget offender accounting** (C2) — small change, do
   before offender flags are surfaced to players/tools. Include the
   empty-name diagnostic defect from §5 (ids 2/3 flagged as `''`) in
   the same pass — the offender log line is only useful if it names
   the component.
4. **Freeze phase re-run** (C1) — needs a snapshot-true phase design
   if JD wants resume-instead-of-rebuild semantics.
5. **Interactive smoke untested**: windowed game with
   `sim.timing_handshake 1` + dimension reload is code-reviewed only
   (no display on this VM).
6. **Tick-debugger stepping** was factored through the same step body
   in both branches (D-g); the known multi-domain stepping defect
   from the production ledger should be re-verified against phase
   slots before the debugger is declared fixed.
7. **Sub-4 phase counts** degrade the Signal cycle to legacy (D-k,
   logged) — domains still handshaken. If JD wants phases ∈ {1,2} to
   mean something for Signal, that's a design conversation.

## 9. What this means (one paragraph)

The Timing Handshake survived contact with the real engine. The
deterministic circuit core of LithoKraft — Trace, Signal, and the
core sim timing — now runs on JD's pull-model phase clock with
bit-identical outcomes to the loop it replaces, with the two failure
policies JD specified implemented and proven in the shipping code
paths, and with a switch that makes the whole thing A/B-able forever.
The price on today's hardware is a small tax; the prize — a
deterministic, phase-scheduled, watchdog-guarded tick architecture
that scales its sync cost with threads instead of components — is in
the tree, tested, and waiting for real cores.

## 10. Isolation verification (production tree)

Method (identical before/after, coordinator-run): file manifest of
`~/workspace/litholattice` (path + size + mtime, sorted, md5), plus a
source-only manifest excluding all build directories and `_deps`.

- **Source manifest: IDENTICAL** before and after the entire sweep —
  `2f44bbf9b77f63b0470c696f211d4db9`, 1,611 files, empty diff. No
  production source file was created, modified, or deleted.
- **Full-tree manifest differs** (`8ea3f84b…` → `08643fb8…`), fully
  explained: build-directory artifacts only. Disclosed incident from
  the baseline stage: the experiment tree's copied `build-debug/`
  retained CMakeCache paths pointing at production, and the baseline
  crew's first (failing) build command regenerated **460
  CMake-generated files inside `~/workspace/litholattice/build-debug/`
  only** — generated build files, spot-checked byte-identical in
  content; the coordinator independently reproduced the 460-file
  count and the clean source mtime scan. No production *source* was
  touched at any point; all builds and runs used the experiment
  tree's own `build-baseline/`.

## 11. Credits

- **Algorithm:** David "JD" Rittenhouse — the Timing Handshake,
  designed in his teenage game-hacking years, recovered and dictated
  2026-10-12/13.
- **This sweep:** integration design, implementation, baseline
  capture, and the full A/B sweep executed by **Elfie and her crews**
  under JD's delegation of 2026-10-13 00:13 EDT, including the
  delegated policy calls (WAIT/FREEZE confirmations C1–C5 above).
  Partial implementation credit for the Timing Handshake stands as
  JD granted it on 2026-10-13 00:12 EDT: **Elfie, Muse AI, and the
  Meta team** — for entry into `game/LithoKraft/CREDITS.md` with the
  update that follows this report.
