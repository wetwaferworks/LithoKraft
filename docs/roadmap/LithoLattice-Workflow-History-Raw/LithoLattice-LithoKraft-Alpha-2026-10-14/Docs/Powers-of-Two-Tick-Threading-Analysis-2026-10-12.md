# Powers-of-Two Tick Threading — Analysis & Design

**Project:** LithoKraft on the LithoLattice engine (private tree, `~/workspace/litholattice`)
**Date:** 2026-10-12
**Author:** Elfie (TICK-ARCHITECTURE analysis pass)
**Status:** Analysis + design only. No production code was written for this document. Nothing in this document is a decision until JD says so.

---

## How to read this document

Every substantive claim carries one of these labels:

- **TRACED** — read directly from the code, with a file anchor. Line numbers are as of the tree on 2026-10-12 and will drift; function names are the durable anchor.
- **MEASURED** — from the perf runs of 2026-10-12 on Elfie's VM (Release build, `perf_headless` and app benches). These are VM numbers, not workstation absolutes.
- **DERIVED** — arithmetic computed from MEASURED numbers (e.g. Amdahl serial fractions). Only as strong as the measurements underneath.
- **HYPOTHESIS** — a code-grounded candidate explanation that the current evidence does not prove. Each one names the instrumentation that would settle it.
- **PROPOSED** — design recommendation. Not existing behaviour, not decided.

JD's directive behind this work: *"start analyzing the code, take your time, do your diligence, design some robust testing frameworks — we're going to nail down something new. They had subtick — what about powers of two tick threading for the timing."*

The concept under analysis: the sim's timing scales along powers of two on **two axes at once** —

1. **Thread/lane counts:** 2 → 4 → 8 sim-timing lanes.
2. **Rates:** tick rates are already powers of two in this engine (32/64/128/256/512 TPS, unlock path to 8192), and subsystems can tick at harmonic gears of a base rate (rate/2, /4, /8, …).

The target includes machines beyond 512 TPS — colleges and enterprises running real computing simulations inside LithoKraft. The plan of record is to build and validate the **two-thread version first** — one authoritative tick thread plus one sub-tick thread — then scale toward four.

---

## 0. Executive summary

**The current architecture in one paragraph (TRACED).** One `SimThread` owns the active dimension's `ChunkMap` and is the sole mutator of authoritative world state. It runs a fixed-timestep accumulator over a *master clock* whose rate is the maximum of the world TPS and every enabled clock domain; the world ticks on an integer divider of that master rate. Cross-thread traffic is narrow and deliberate: stats and the look ray travel through lock-free triple buffers, edits and sim events through mutex-guarded handoff queues, and mesh work leaves the sim thread as immutable 18³ snapshots (`MeshInput`) for a worker pool. The game/render thread never touches the live map except through `getBlock()` under a mutex the sim holds across its whole mutation window, and through full stop-the-thread operations for saving and streaming insertion.

**Findings that matter most:**

1. **The two-thread design JD wants is mostly a formalization of what the code already wants to be (TRACED).** The ownership discipline (single writer, immutable snapshots out, tick-stamped edits in) is already the house pattern — in the mesher, in `AiWorld`'s parallel path search, and in the `DeterministicScheduler` library. What is missing is a published *clock* and a published *tick-boundary snapshot* for dynamic state. There is no general interpolation snapshot today; `SimStats` is a HUD/status struct, not a state sample.
2. **The sim's own catch-up cap is 16 master steps per loop iteration, with a 100 ms retained-lag ceiling (TRACED)** — not the 64-tick cap people cite, which belongs to game-side locomotion. Dropped time is only the excess above 100 ms.
3. **With EUV enabled (the default), the sim loop runs at 512 Hz no matter what the world TPS is (TRACED).** At 32 TPS world rate the loop still wakes, drains, raycasts, publishes stats and events 512 times a second; the world ticks every 16th master step. This is a concrete, code-level contributor to "TPS won't hold steady even at 32" on the live game — separate from the perf-harness anomaly below.
4. **The 128 TPS perf anomaly is not causally proven (MEASURED + HYPOTHESIS).** The inversion (128 TPS slower per-tick than 512 TPS) cannot be explained by any rate-dependent branch in the ticked code — there is none. The measurement boundary itself is suspect: the harness times only `IWorldSimulation::tick`, excluding the serial remesh-snapshot flush whose cadence resonates with tick rate, and excluding scheduler wake/preemption effects that a wall-clock wrapper absorbs. Section 2.2 ranks the hypotheses and specifies the phase instrumentation that settles it.
5. **Tick identity resets on every stop/start (TRACED, defect-class).** `worldTick`, `masterStep` and the dropped counter are locals of `SimThread::run()`; saving and streaming insertion both stop/start the thread, so world tick returns to 0 in normal play. Random ticks are keyed by tick index, so their streams restart too. Any SimClock design must decide tick persistence explicitly.
6. **Worldgen/mesh scaling is bounded by serial structure, not by worker count (MEASURED + DERIVED + TRACED).** 1→4 threads buys 1.26× (generation) and 1.39× (meshing) — effective serial fractions ≈72% and ≈62%. The sources are named in §2.4: a single global job queue, a single result handoff mutex, serial snapshot capture on the owning thread, serial drain/insert, and per-batch tail effects. Adding threads to this shape will not fix it; changing the shape will.
7. **Determinism today is a set of scoped guarantees, not one end-to-end replay proof (TRACED).** SignalSim given the same tick-stamped edit stream, worldgen at any thread count, the deterministic scheduler at 1/2/4/8 workers, input replay — each is pinned separately. The full game (locomotion intent sampled per frame, item entities on wall `dt`, an unseeded `rand()` in item-drop toss) is *not* bit-exact end-to-end today. The constitution in §4 states what any multi-thread design must preserve, and the TickTrace gate in §7.1 is designed to turn the scoped guarantees into one gate.
8. **Beyond 512 TPS is a composition, not a knob (PROPOSED).** At 8192 TPS the tick period is 122 µs; the current per-loop fixed costs (mutex drain, 64-block raycast, stats publish, yield-spin wait) cannot be paid per master step. The credible enterprise story is: higher base rate for the domains that need it + harmonic gears for everything that doesn't + deterministic parallel pure phases + parallel snapshot consumers. §6.9 develops this.

---

## 1. The current architecture, as traced

### 1.1 Ownership and the main-loop ↔ SimThread contract

**TRACED — Sole ownership.** `SimThread` owns the active dimension's `ChunkMap` while running; `IWorldSimulation` is documented "used on the sim thread only" (`engine/src/sim/include/litholattice/sim/sim_thread.h`; `engine/src/sim/include/litholattice/sim/world_simulation.h`). LithoKraft's concrete simulation is `SignalWorldSimulation`, whose `tick()` delegates to `SignalSim::tick` (`game/LithoKraft/src/content/signal_world.cpp`).

**TRACED — Construction.** `Game::loadActive()` builds the `SimThread` with `tickRate = cfg_.tps`, `radiusChunks = cfg_.tickRadius`, the dimension epoch, the dimension's random-ticks-per-section, and exactly one clock domain: `{"EUV", kMaxDomainHz, cfg_.euv}` (`game/LithoKraft/src/game/game.cpp` ≈596–601). EUV defaults **on** (`game/LithoKraft/src/game/include/lithokraft/game/settings.h`: `bool euv = true`).

**TRACED — Cross-thread channels.** The complete contract surface:

| Channel | Mechanism | Direction | Blocking behaviour |
|---|---|---|---|
| `SimStats` | `TripleBuffer<SimStats>` | sim → game/render | Non-blocking both sides; reader may see a stale-but-coherent sample |
| Look ray | `TripleBuffer<RayInput>` | game → sim | Non-blocking |
| Player edits | `HandoffQueue<EditCmd>` (one mutex) | game → sim | Brief mutex on push/drain |
| Sim events | `HandoffQueue<SimEvent>` (one mutex) | sim → game | Brief mutex on push/drain |
| Mesh snapshots | `std::shared_ptr<MeshInput>` via submit callback → `JobPool` | sim → workers → game | Immutable after capture; workers never touch the live map |
| Single-block reads | `SimThread::getBlock()` under `blockMutex_` | any → sim's map | **May block up to one full catch-up burst** (see §1.4) |
| Between-tick callbacks | `SimThread::runBetweenTicks()` | game → sim | Pauses sim, waits for the mutex, runs callback under it |

Anchors: `sim_thread.h` (channel members, `getBlock` comment ≈116–124, `runBetweenTicks` ≈95–101); `engine/src/core/include/litholattice/core/threading.h` (`TripleBuffer` ≈16–40, `HandoffQueue` ≈142–158).

**TRACED — What the stats channel is not.** `SimStats` carries `worldTick`, achieved/target TPS, tick-ms averages over a 1-second window, dropped count, active component count, random-tick count, per-domain achieved rates, and the look-ray hit (`sim_thread.h` ≈59–76). It is a *status* publication. It does **not** carry entity transforms, block-state versions, or event batches — there is no tick-boundary world/dynamic snapshot for interpolation anywhere in the current design. (§6.4 designs one.)

**TRACED — The app loop.** `game/LithoKraft/src/app/main.cpp` (≈601–660): poll display toggle → `input.update(window->input())` → frame `dt` from `steady_clock`, clamped to 0.1 s → `game.update(dt, w, h)` → `game.uploadEntities()` → build `FrameParams` (with `timeSec` = seconds since app start) → `renderer->renderFrame(fp)` → complete pending screenshot → software present if applicable → `window->swap()`. No explicit frame-rate limiter exists in this loop beyond present/vsync behaviour; JD's ~300 FPS live report is consistent with an unclamped loop.

**TRACED — Game::update's dependence on worldTick deltas.** `Game::update()` (`game.cpp` ≈1127–1345) reads `sim_->stats().worldTick` independently for four consumers:

- **Lithoscript `wait` pump** — loops `consoleRegistry_.tick()` once per elapsed world tick; **no catch-up cap traced**; resyncs downward when the counter resets (≈1132–1136).
- **Locomotion** — samples one frame of input intent, then steps the body once per elapsed world tick with `dt = 1/targetTps`; **catch-up capped at 64 ticks/frame**; solidity callback calls `sim_->getBlock()` per query (≈1184–1204).
- **Vitals** — advances by the tick delta in one batched call (≈1224–1233).
- **Sprites** — advances by the tick delta, cast to `uint32_t` (≈1239–1245).

So a sim burst becomes a game-thread burst: the frame that notices 64 new world ticks also pays up to 64 locomotion steps (each with repeated `getBlock()` mutex acquisitions), a script pump of the same size, and batched vitals/sprite work.

### 1.2 The scheduler: master clock, accumulator, catch-up

**TRACED — Master clock.** In `SimThread::run()` (`engine/src/sim/sim_thread.cpp`, `run()` from ≈237; reconfigure lambda ≈271–277):

- `worldTps = requestedTps_`; `masterHz` starts at `worldTps` and rises to the max of every *enabled* domain's rate.
- `worldDiv = masterHz / worldTps` (integer; all rates are powers of two, so this is exact).
- `step = 1'000'000'000 / masterHz` nanoseconds (integer division; exact for rates ≤ 512, truncated above — see §5).
- On each master step, domain `d` steps when `masterStep % (masterHz / d.hz) == 0`; the world ticks when `masterStep % worldDiv == 0`.

**TRACED — Wait policy.** `waitUntil()` (≈228–235): sleep until ~1.5 ms remain, then `std::this_thread::yield()`-spin. A `HighResTimerScope` (1 ms timer resolution on Windows, no-op elsewhere) is held for the thread's lifetime. The wait target is `last + (step − acc)`, i.e. relative to the current iteration's start, not an absolute epoch schedule.

**TRACED — Accumulator and caps (the sim's own policy).** Per loop iteration (≈301–311, ≈337):

- `acc += now − last`.
- Retained lag is capped at **100 ms**: excess is added to `dropped` and `acc` is clamped to 100 ms.
- The unpaused loop executes while `acc >= step` **and `steps < 16`** — the catch-up cap is **16 master steps per loop iteration**. (At world==master rate that is 16 world ticks; with EUV on at 32 TPS world rate it is 16 master steps = 1 world tick.)
- If backlog remains after 16 steps, the wait target is already in the past, so the next iteration runs immediately and continues the burst. The cap bounds one mutex hold, not the total catch-up rate.
- `dropped` counts only accumulator time beyond the 100 ms ceiling, in **master steps** (see defect F2, §1.5).

**TRACED — Paused behaviour.** While paused: `acc` is zeroed every iteration, edits are applied immediately via `applyEdits(ray, kApplyAllEdits)`, and unpausing does **not** catch up the paused interval (≈320–348). `stepTicks(n)` (tick debugger) forces pause, then consumes requested steps inside the loop: each requested step advances `masterStep`, stepping domains on their dividers and the world on `worldDiv`; `ticksStepped_` counts world ticks only.

**TRACED — Edit quantization.** Once per loop iteration the edit queue is drained; **every drained edit is stamped with the current `worldTickN`** and appended to `editBuf_` (≈301–306). During a world tick, `applyEdits(ray, worldTickN)` applies edits whose stamp is due, in buffer order. The header states the intent plainly: edit application is a pure function of tick index (`sim_thread.h` ≈36–44). The corollary: *live* input timing is not deterministic — which tick an edit lands on depends on drain timing — but a recorded stream of tick-stamped edits replays exactly.

### 1.3 Per-world-tick pipeline and measurement boundaries

**TRACED — Order inside one unpaused world tick** (`sim_thread.cpp` ≈349–362):

1. `applyEdits(ray, worldTickN)` (due player edits; also performs the look raycast if any edit is due).
2. `sim_->tick(chunks_, area, dirtyFn)` — for LithoKraft, `SignalSim::tick`.
3. `sim_->randomTick(...)` — the game hook; `SignalWorldSimulation` does not override it (default no-op).
4. Engine `runRandomTicks(...)` — keyed per (tick, section); see §4.
5. `++worldTickN`.

If no tick ran in an iteration and no edit was applied, the loop still performs a full look raycast (`castRay`, 64-block range) to keep the HUD highlight live (≈376).

**TRACED — Per-loop fixed work.** Every loop iteration, tick or not: reconfigure check (atomics), edit drain (mutex), the raycast above, `flushDirty()` check, `publishEvents()`, a full `SimStats` write + triple-buffer publish, and the wait. With EUV on, this fixed work runs at 512 Hz at *every* world TPS (F6, §1.5).

**TRACED — Two different "tick ms" numbers exist and must not be conflated.**

- `SimStats::tickMs` (sim thread): the timer starts **after** clock-domain stepping (`t1`), so it includes edit application + `IWorldSimulation::tick` + the random-tick hook + engine random ticks, but **excludes domain stepping**. It is averaged over a 1-second window. `budgetPct` uses a second accumulator (`windowAllNs`) that wraps the whole master-step body (domains + world work) but still **excludes** flush, stats publication, event publication, and the no-tick raycast (§1.5, F5).
- `perf_headless`'s `TimedSimulation` (`game/LithoKraft/src/app/perf_headless.cpp` ≈56–80): wraps **only** `IWorldSimulation::tick`. Tonight's circuit "tick avg/p99" is therefore `SignalSim::tick` alone — not edits, not random ticks, not domains, not flush, not scheduling latency.

**TRACED — Dirty remesh flushing is serial sim-thread work.** Dirty sub-chunks collect during ticks; after the mutex is released, `flushDirty()` may run (≈379–383): signal-driven flushes are coalesced to at most ~120/s (`kRemeshInterval = 8333 µs`, ≈265); player edits and `remeshAll` force an immediate pass. `flushDirty()` itself (`sim_thread.cpp` ≈111–135) iterates the dirty set **on the sim thread**, snapshots each sub-chunk into a pooled `MeshInput` (18³, ≈24 KB; pool capped at 64 buffers), and submits to the worker pool. Meshing is parallel; **snapshot capture is a serial phase on the tick thread**, invisible to both tick-ms metrics above.

### 1.4 Who blocks whom: convoys and worst-case waits

**TRACED — `getBlock()` convoys.** The sim thread holds `blockMutex_` across the edit drain **and every tick of the loop iteration** (≈295–376). A game-thread `getBlock()` therefore waits up to one full catch-up burst: worst case 16 master steps of tick work plus edit application. Locomotion's solidity callback calls `getBlock()` repeatedly during its own up-to-64-step catch-up, so after a hitch the game thread and sim thread can alternate bursts against each other. The header is honest about this: "fine for a handful of queries per frame, not for bulk scans" (`sim_thread.h` ≈116–124).

**TRACED — `runBetweenTicks()` convoys.** It sets pause, waits for `blockMutex_`, runs the callback under the mutex, restores pause (`sim_thread.cpp` ≈56–68). Users: hot-swap definition refresh (`game.cpp` ≈188), `surfaceYAt` (≈704), and the camera spring-arm raycast (≈715). Worst-case wait is again one in-flight burst. Caveat F4 (§1.5): `flushDirty()` runs *outside* the mutex, so the "between ticks" guarantee covers the mutation window, not an in-flight snapshot pass.

**TRACED — Stop-the-world operations on the frame path.**

- `Game::pumpStreaming()` (`game.cpp` ≈674–697): when generated columns are ready, it calls `sim_->stop()` (joins the sim thread), inserts columns on the game thread, snapshots/submits their initial meshes, then `sim_->start()`. Every streaming insertion batch is a frame hitch by construction and pauses simulation.
- `Game::saveWorld()` (`game.cpp` ≈647–665): same stop/start pattern around the consistent snapshot copy; the measured all-changed pause at 4,096 columns is 354 ms (MEASURED, §2.1).
- `stop()` itself waits for the thread to notice `running_ == false`: up to one wait interval plus one in-flight burst.

**TRACED — Upload budgeting bounds one convoy, creates another queue.** `Game::uploadMeshes()` (`game.cpp` ≈1395–1455) coalesces to the newest mesh per sub-chunk and uploads at most **64 sub-chunks and 3 MiB per frame**. This bounds per-frame upload cost (it was written against an older 49 ms hitch), but under sustained circuit churn the pending queue is a backlog that drains at 64/frame while snapshots keep arriving at up to ~120 flushes/s.

**TRACED — Pool sharing.** One `JobPool` (sized `max(2, hardware_concurrency) − 2` in `main.cpp` ≈566) serves worldgen streaming, mesh jobs, and everything else the game submits. `ChunkStreamer` caps its own in-flight jobs at 32 "to leave pool capacity for meshing" (`game/LithoKraft/src/game/chunk_streamer.cpp` ≈30–32). There is no priority separation between latency-sensitive remesh jobs and bulk generation jobs in this pool. (A work-stealing `JobSystem` with priorities exists in the engine — §2.4 — but the live game path does not use it.)

### 1.5 Contract defects and ambiguities found while tracing

These are stated as findings, not fixes; several feed directly into the design (§6) and the test gates (§7).

- **F1 — Tick identity resets on stop/start (TRACED).** `worldTickN`, `masterStep` and `dropped` are locals of `run()` (`sim_thread.cpp`:241). `saveWorld()` and `pumpStreaming()` stop/start the same `SimThread` in normal play, so `worldTick` returns to 0 each time. Consequences: (a) Game consumers all carry resync-downward logic whose comments attribute resets to "world reloaded" — streaming inserts and autosaves cause them too; (b) engine random ticks are keyed by `(tickIndex, section)`, so after a restart the same sections replay the same random streams from tick 0; (c) clock-domain phase alignment (`masterStep % divider`) restarts. Any SimClock must define whether tick is persistent per dimension-load, per session, or per thread-run — today it is per thread-run, implicitly.
- **F2 — `droppedTicks` counts master steps, not world ticks (TRACED).** The increment divides excess lag by the *master* step (`sim_thread.cpp`:311). With EUV on at 32 TPS world rate, one dropped world tick is reported as 16 dropped "ticks" in the HUD (`game.cpp` ≈1572). The metric needs splitting (`droppedMasterSteps` / `droppedWorldTicks`) before it can gate anything.
- **F3 — Debugger steps are not live ticks (TRACED).** The paused `stepTicks` path steps domains and calls `sim_->tick` + `sim_->randomTick`, but never calls engine `runRandomTicks` (compare `sim_thread.cpp` ≈326–348 vs ≈354–360). A stepped world and a live world diverge in random-tick effects. This matters for any replay/debug tooling built on the stepper.
- **F4 — `runBetweenTicks` does not cover `flushDirty` (TRACED).** The flush runs after `blockMutex_` is released (≈376–383). Read-only callbacks (surface query, camera raycast) are unaffected in practice, but a callback that mutates data a snapshot reads (e.g. definition hot-swap while a flush resolves the registry) is outside the mutex protocol the header describes.
- **F5 — Published budget metrics understate loop cost (TRACED).** `tickMs` excludes domain stepping; `budgetPct` excludes flush, event/stats publication, and no-tick raycasts (§1.3). At high master rates the excluded work is exactly the work that grows. PaceLab (§7.2) must instrument phases directly rather than trust these two numbers.
- **F6 — EUV-on forces a 512 Hz loop at every world rate (TRACED).** Default-on EUV (`settings.h`:47; `game.cpp`:599) makes `masterHz = 512` even at 32 TPS world rate. The domain's own `step()` is a stub LCG (`engine/src/sim/include/litholattice/sim/clock_domain.h` ≈8–21) — the cost is not the domain logic, it is the 512 Hz loop fixed work around it: edit drain, raycast, stats publish, event publish, flush check, and a yield-spin wait that never sleeps deeply at that rate.

---

## 2. The performance evidence

### 2.1 Measured baseline (2026-10-12, Elfie's VM)

**MEASURED — Stress circuit** (5,531 indexed components; `perf_headless` circuits section; metric = `SignalSim::tick` only, per §1.3):

| World TPS | Tick budget | Tick avg | Tick p99 | Notes |
|---|---|---|---|---|
| 512 | 1.953 ms | 0.654 ms | 2.835 ms | Holds 512 TPS; avg = 33.5% of budget, but **p99 exceeds budget** |
| 256 | 3.906 ms | 0.725 ms | 5.603 ms | p99 exceeds budget |
| 128 | 7.813 ms | 3.422 ms | 26.640 ms | ≈395 ticks in the nominal 3 s window (≈132 TPS achieved) |

**MEASURED — Random ticks** (engine `runRandomTicks` timed directly, 8 picks/section):

| Radius | Picks/tick | Avg | p99 |
|---|---|---|---|
| 8 chunks | 7,016 | 0.834 ms | 5.845 ms |
| 12 chunks | 15,104 | 2.051 ms | 24.905 ms |

At radius 12 the p99 alone is ~80% of a 32 TPS tick budget before SignalSim runs at all.

**MEASURED — Thread scaling** (`perf_headless`, 256 columns / all sub-chunks):

| Threads | Generation | Meshing |
|---|---|---|
| 1 | 494.6 columns/s | 916.9 sub-chunks/s |
| 4 | 625.4 columns/s | 1,276.8 sub-chunks/s |

**MEASURED — Save/load:** autosave pause with ~2% columns changed ≈0.5–1.3 ms; pause with **all columns changed at 4,096 columns = 353.6 ms**; full save ≈5,210 ms; load ≈5,560 ms (4,096 columns).

**MEASURED — AI batch** (`AiWorld`, harness-only; not wired into the live game loop as traced): 64 mobs — 20.05 ms avg / 68.58 ms p99 per AI tick; 128 mobs — 24.71 ms avg / 79.72 ms p99. (AI ticks are 8–16 Hz by design, so this is a budget question, not a per-world-tick cost — but at 64 mobs the p99 is ~7 AI-tick budgets at 10 Hz if it were ever called synchronously.)

**MEASURED — JD's live workstation report (for contrast):** ~300 FPS with heavy hitching; TPS unstable even at 32. Targets JD set: stable 30/60 FPS, 120 FPS interesting for double-buffered vsync; the enemy is variance, not averages. The VM numbers above cannot confirm or refute the workstation numbers; §2.3 explains the workstation shape from the code.

### 2.2 The 128 TPS anomaly: status and hypotheses

**Status: unresolved.** No rate-dependent branch exists in the ticked code that would make a 128 TPS tick do more logical work than a 512 TPS tick: `SignalSim::tick` executes the same phases over the same scene per world tick at any rate (§4.1), and the harness configures no clock domains, so `masterHz == worldTps` in these runs. Any honest explanation must therefore come from *timing structure around* the tick, not from tick semantics. Ranked hypotheses:

- **H1 — Measurement boundary + flush cadence resonance (HYPOTHESIS, code-grounded).** `TimedSimulation` times only `IWorldSimulation::tick`. The serial `flushDirty()` snapshot pass (§1.3) runs on the same thread *between* ticks, capped at ~120 passes/s. At 512 TPS a flush pass happens at most every ~4th tick and coalesces several ticks of dirty marks; at 128 TPS the 7.81 ms tick period sits just under the 8.33 ms flush interval, so nearly every tick is followed by a full snapshot pass over the circuit's dirty sub-chunks. That pass is excluded from the metric, but it evicts cache, allocates/recycles 24 KB snapshot buffers, and delays the next tick's start — and a wall-clock wrapper around the *next* tick absorbs any preemption or migration that delay invites. This predicts: tick-time histograms should correlate with "ticks since last flush," and phase instrumentation (flush duration per loop) should show the missing time. It does **not** by itself prove a 26 ms p99.
- **H2 — Accumulator bunching after late wakes (HYPOTHESIS, code-grounded).** `waitUntil()` sleeps then yield-spins; any wake lateness beyond one step is repaid as back-to-back ticks (up to 16) under one mutex hold (§1.2). Bunched ticks run with different cache/branch state than evenly spaced ticks, and the *first* tick after a sleep pays cold-start costs the wrapper attributes to the tick. At 512 TPS the thread rarely sleeps deeply (period 1.95 ms vs the 1.5 ms spin threshold — it is effectively always hot); at 128 TPS it sleeps ~6 ms per cycle and is exposed to OS/VM wake jitter every tick. This predicts: per-tick time should correlate with the preceding inter-tick gap, which the current harness does not record.
- **H3 — Environment contention during the runs (HYPOTHESIS, evidence-adjacent).** The 128 TPS run produced ≈395 ticks in a nominal 3.000 s window — the window itself stretched by ~86 ms of sim time, on a VM where other build/perf work was active in the same session. A vCPU preemption *inside* a timed tick inflates that tick's measured duration directly, and at 128 TPS the run has only ~384 samples, so p99 is ~4 ticks. This predicts: re-running the circuit section alone on an idle machine should compress or move the anomaly. It has not been re-run in isolation; until it is, H3 cannot be excluded and the anomaly must not be "explained" by code alone.
- **H4 — Scene phase sampling (HYPOTHESIS, weak).** Circuit work per tick is phase-dependent (clock grids fire large re-floods on specific ticks). A 3 s window covers ~1,536 ticks at 512 TPS but only ~395 at 128 TPS, so burst phases are weighted differently. This can move an average; the size of the inversion makes it an unlikely sole cause.

**What settles it (PROPOSED, feeds §7.2):** per-phase timestamps inside `SimThread::run()` (domains / edits / sim tick / random ticks / raycast / flush / publish / wait), recorded per loop iteration with the preceding gap and the steps-run count, exported by the circuit bench. With those six numbers per iteration, H1–H4 separate immediately: H1 shows flush time preceding slow ticks, H2 shows gap→tick correlation, H3 shows uniform phase inflation on scattered iterations, H4 shows slow ticks at fixed tick indices.

**A separate, fully explained low-rate problem (TRACED):** the *live game* at low world TPS runs its sim loop at 512 Hz because EUV is on by default (F6). That is not the perf anomaly (the harness has no EUV domain), but it is a real per-second cost and wake-rate multiplier on JD's machine at 32 TPS, and it makes the sim thread compete with the render thread 16× more often than the world rate suggests.

### 2.3 Why the live game hitches: convoy inventory

Mapping JD's report (300 FPS, heavy hitching, TPS won't hold at 32) onto traced code, the hitch sources in expected-impact order:

1. **Streaming stop/start (TRACED, §1.4).** Every ready-column batch joins and restarts the sim thread on the frame thread. During fast flight this can recur continually; each occurrence is a guaranteed frame spike plus a sim pause.
2. **Sim catch-up bursts ↔ locomotion catch-up (TRACED, §1.1/§1.4).** A late frame lets the sim burst up to its caps; the game then pays up to 64 locomotion steps with repeated `getBlock()` waits against the sim's next burst. The two catch-up policies (16 master steps/loop; 64 ticks/frame) were tuned independently and compose badly after a hitch.
3. **Autosave / save stop-the-world (TRACED + MEASURED).** 354 ms pause in the all-changed worst case at 4,096 columns; typical dirty saves are small (0.5–1.3 ms), but a save after heavy editing or streaming lands in the tail.
4. **Remesh snapshot serialization on the sim thread (TRACED).** Busy circuits dirty sub-chunks continuously; `flushDirty()` snapshots them serially at up to ~120 passes/s *on the tick thread*, competing directly with tick execution. This is the single largest structural threat to TPS stability under circuit load.
5. **Upload budget backlog (TRACED).** 64 sub-chunks / 3 MiB per frame upload cap vs up to ~120 snapshot passes/s of arrivals: under churn the queue grows and the world *looks* stale even when the sim is on time — perceived as lag.
6. **512 Hz loop fixed work at low world TPS (TRACED, F6)** and an **unclamped render loop** (§1.1): the frame thread spins at ~300 FPS doing per-frame work (including per-frame worldTick-delta checks and streaming scans) and competing for cores with a yield-spinning sim thread.

None of these require SignalSim itself to be slow — consistent with the harness holding 512 TPS on the circuit scene. The live problem is *composition*, which is precisely what a two-lane design with a published clock and snapshot discipline is meant to discipline (§6).

### 2.4 Worldgen/mesh scaling: the serial fraction and its sources

**DERIVED — Effective serial fractions (Amdahl).** From §2.1: generation speedup at 4 threads = 625.4/494.6 ≈ **1.264×** → effective serial fraction ≈ **72%**. Meshing speedup = 1276.8/916.9 ≈ **1.393×** → effective serial fraction ≈ **62%**. "Effective" is load-bearing: these numbers bundle true serial code, finite-batch tail effects, queueing, and memory bandwidth; they are the ceiling on what *more of the same threads* can buy, not a line-item audit.

**TRACED — Generation's serial sources.**

- **Single global job queue.** `JobPool` is one mutex + one condition variable + one FIFO `deque<std::function<void()>>` (`threading.h` ≈44–105). Every submit and every job pop serializes on that mutex; jobs are type-erased `std::function`s.
- **Single result handoff.** Each generation job pushes its column into one `HandoffQueue` — one mutex per push (`threading.h` ≈142–158).
- **Serial drain and insert.** `Dimension::load()` waits for the batch, drains the queue, and inserts columns into the `ChunkMap` **sequentially on the caller** (`engine/src/world/dimension.cpp` ≈6–25). The perf harness replicates this shape and likewise drains/inserts serially after the timed section — so the *live* serial cost is worse than the harness number, not better.
- **Serial within a column.** `GeneratorPipeline::generate()` is an ordered stage chain per column — climate → biome select → biome post-stages → terrain → surface rules → carvers → features (`engine/src/procgen/pipeline.cpp` ≈36–83) — and `ColumnGenerator::generate()` then converts the buffer into sub-chunks with a serial cell scan (`engine/src/world/column_generator.cpp` ≈44–68). Parallelism exists only *across* columns, so a 256-column batch has a longest-column tail the batch wait must absorb.
- **Per-feature keyed RNG is not a serial source** (worth stating): feature RNG is seeded per (chunk seed, feature-name hash), so features do not shift each other's streams (`pipeline.cpp`, `FeatureRegistry::placeAll`).

**TRACED — Meshing's serial sources.**

- **Snapshot capture is serial and excluded from the "mesh" timing.** The harness builds every `MeshInput` sequentially (`snapshotForMeshing` per sub-chunk) *before* the timed parallel section (`perf_headless.cpp` ≈143–151). In the live game the same capture is serial on whichever thread owns the map — the sim thread for remesh (§1.3), the game thread for initial/streaming loads. Snapshot capture reads 27 sub-chunks' borders per target (`engine/src/world/mesh_input.cpp`; `mesh_input.h`: an 18³ snapshot, "taken by the thread that owns the ChunkMap").
- The parallel remainder still shares the `JobPool` queue above, plus read-only registry/material lookups and raw memory bandwidth: each snapshot is ≈24 KB read to produce a vertex stream.

**TRACED — The engine already owns the alternative.** `JobSystem` (`engine/src/core/include/litholattice/core/job_system.h`) provides per-worker Chase-Lev work-stealing deques, priorities, dedicated I/O threads, `JobFence` barriers, and affinity policies — and the deterministic-scheduler tests already prove `JobSystem` and `JobPool` backends produce bit-identical results at 1/2/4/8 workers (§4.1). The live game uses `JobPool` everywhere traced. Any scaling work should evaluate routing the existing pure jobs (mesh, gen) through `JobSystem` before inventing new machinery — as a *separate* change from the tick-threading stages, gated by TickTrace (§7.1).

---

## 3. Clock census: everything that keeps private time today

A single `SimClock` should become the authority for **sim-derived time**. It must not — and in this design does not — replace physical clocks: the audio device sample clock, OS input timestamps, profiling timers, rate limiters, and storage/network timeouts measure the real world and stay real. The census below separates the two.

### 3.1 Sim-derived clocks (SimClock should own or feed these)

| Consumer | Time base today | Anchor | Notes |
|---|---|---|---|
| SimThread master step / world tick | Integer master steps; world divider | `sim_thread.cpp` run() | The authoritative clock already; unpublished except via stats |
| Clock domains (EUV) | Integer divider of master | `clock_domain.h`; `sim_thread.cpp` | Stub logic; rates are powers of two |
| SignalSim tick count, lamp hold | World ticks (`tickNo_`, hold in ticks) | `signal.cpp` tick() | Lamp hold is set from TPS at load (`cfg_.tps/30` for ≥256 TPS) — a rate-aware exception |
| Engine random ticks | Keyed by `(worldTick, section)` | `random_ticks.cpp` | Runs at world TPS only |
| Lithoscript `wait` | World-tick deltas | `game.cpp` ≈1132 | Freezes when paused; no catch-up cap |
| Locomotion | World-tick deltas × `1/targetTps` | `game.cpp` ≈1192–1204 | Correctly rate-normalized |
| Vitals (oxygen/stamina) | World-tick counts, constants calibrated "at 32/s" | `player_adapter.h` ≈67–71 | **Rate-sensitive:** at 512 TPS the same constants drain ~16× faster in wall time. Tick semantics or seconds semantics — a design decision (§6.8), not a bug to silently fix |
| Sprites (sim) | World-tick deltas; per-tick integration | `sprite_system.cpp` tick(); `game.cpp` ≈1239 | Presentation state, but advanced in tick units — real-time speed scales with TPS |
| Player abilities | "Engine ticks" (header assumes 512 TPS) | `engine/src/player/.../ability.h` ≈8–11 | Not currently ticked from the live game loop as traced; the 512 assumption is a latent trap |
| AI (`AiWorld`) | Own accumulator: `advance(simTicks, worldTps)` → AI ticks at 8–16 Hz (clamped), LOD thinks every 1/2/4 AI ticks | `ai_world.cpp` ≈46–124 | Uses a `double` accumulator — long-run drift vs an integer gear is a real (if small) determinism smell. Harness-only today |
| Ground pulse / ground traces (shader) | `worldTick / TPS` → sim seconds | `game.cpp` ≈1481–1486; `ground_pulse.h` | Pausing freezes it — the model SimClock consumer |
| MCP status / senses stamps | Reads `worldTick` from stats / tick callback | `mcp/server.cpp`; `mcp_senses_adapter.cpp` | Vision frame is stamped with the tick **at capture completion**, not at the state it rendered — a coherence defect the snapshot contract (§6.4) addresses |

### 3.2 Frame/wall clocks (presentation; SimClock informs, does not own)

| Consumer | Time base today | Anchor | Notes |
|---|---|---|---|
| App frame `dt` | `steady_clock`, clamped 0.1 s | `main.cpp` ≈604–608 | Drives camera, UI, entities, menu |
| Camera (look, blend, shake, spring-arm) | Frame `dt` | `camera.cpp`; `camera_controller.cpp` | Presentation only |
| Game-thread entities (items) | Frame `dt` | `game.cpp` updateEntities | Uses wall `dt`; item toss uses unseeded `rand()` (§4.2) |
| `FrameParams::timeSec` | Seconds since app start | `main.cpp` ≈646 | Feeds shader time **and texture animation** (below) |
| Texture animation | `uTime` (app elapsed seconds) ÷ `frameMs` in shader | `chunk_shading.glsl` ≈128; `texture_registry.h` (frameMs) | **Does not freeze when the sim pauses** — a visible clock-discipline leak |
| Sprite animation frame | Integer sprite age in ticks | `sprite.h` (ticksPerFrame) | Sim-derived; already disciplined |
| Title/status, perf lines | 0.5 s / 1 s wall windows | `main.cpp` | Diagnostics |
| Autosave interval | Wall seconds | `game.cpp` handleToggles | Policy clock; fine as wall time |
| IK / rig animation | Render-rate | rig/anim headers | Presentation only |

### 3.3 Physical clocks (must remain independent)

| Consumer | Clock | Anchor | Why it stays |
|---|---|---|---|
| Audio synthesis/render | Device sample count, 48,000 Hz nominal | `audio_engine.h` (kDefaultSampleRate); render() drains commands per callback | Sample production is a device contract. SimClock may *timestamp events*; it cannot drive samples |
| MCP audio tiers | Downsampled 48 kHz tap: 8000/16000/24000/48000 Hz | `mcp_senses.h` ≈44–47 | Physical media clock |
| Audio occlusion thread | Own thread at 32/16/8 Hz with deadline degradation | `occlusion_thread.cpp` ≈106–165 | Already a harmonic-gear precedent (§6.8); not game-integrated as traced |
| Input events | Backend clock (`glfwGetTime` live); `InputPump` restamps to `steady_clock` on receipt where used | `input_event.h`; `glfw_input_backend.cpp`; `input_thread.h` | The live app polls the backend directly per frame and does not route through `InputPump` as traced. Event *times* exist but are not used for tick assignment (§6.6) |
| Script/mod CPU budgets | Wall microseconds (deadlines, quotas) | `script_engine.cpp`; `extension_budget` | Safety mechanism — must stay wall |
| MCP rate limiting | Wall milliseconds | `mcp/rate_limit` | Abuse control — must stay wall |
| Save/perf/bench timing | Wall clocks | everywhere in benches | Measurement, by definition |

**Census conclusion (PROPOSED):** SimClock's job is to give every §3.1 consumer one published, rational, pause-aware time — `tick + alpha + rate + epoch` — and to let §3.2 consumers *sample* it coherently. Anything in §3.3 that "needs sim time" should receive a timestamp, never a replacement clock.

---

## 4. Determinism: the constitution

### 4.1 What is actually guaranteed today (scoped guarantees)

**TRACED — There is no single end-to-end replay artifact. There are five scoped guarantees, each pinned by its own tests:**

1. **SignalSim, given a fixed tick-stamped input stream.** One thread mutates the world in a fixed phase order (§4.3). Selftest precedent: the game selftest saves a world, restores it, ticks both copies 70 more ticks, and requires identical cells and identical exported signal state (`game/LithoKraft/src/app/selftest.cpp` ≈133–186). SignalSim also carries internal determinism defences: exported component state is sorted by packed position before serialization (`signal.cpp` ≈24–28); `buildNets()` sorts wire keys before assigning network IDs *because* IDs must not depend on unordered-set bucket order (`signal.cpp` ≈358–365); network reader lists are sorted and deduplicated (≈412); radio query tie-breaks are canonical (strongest, then nearest, then lowest packed position — `signal.h` ≈74–100); torches and components use compute-then-apply two-phase updates (`signal.cpp` tickTorches/tickComponents).
2. **Worldgen at any thread count.** Same seed → identical columns at 1, 4, and all threads (`selftest.cpp` ≈195–218). Mechanism: generation is a pure function of (seed, position) with per-feature keyed RNG (§2.4).
3. **DeterministicScheduler at 1/2/4/8 workers, on either pool backend.** `engine/tests/engine_tests.cpp` ≈3623–3742: stage construction from declared Read/Write conflicts; command buffers drain in (system, sequence) order; keyed RNG stable per (entity, tick, salt); 64-tick runs hash bit-identical on 1, 2, 4 threads (JobPool) and on 1, 2, 4, 8 workers (JobSystem). The scheduler is a **library** — the live SignalSim path does not run on it (SimThread calls `IWorldSimulation` directly).
4. **Input replay at the InputSystem level.** A recorded event stream replayed through `ReplayInputBackend` reproduces the same actions (`game/LithoKraft/src/game/input_selftest.cpp` ≈186–205). `InputPump`'s contract states the same for its manual mode: identical streams hash identically; live threaded pumping explicitly does **not** promise it (`input_thread.h` ≈35–38).
5. **Keyed/seeded subsystems individually.** Engine random ticks keyed by (tick, section) with a splitmix-style hash (`random_ticks.cpp`); sprite emitters with splitmix64 seeded RNG and fixed draw counts; `AiWorld` thinking in mob-ID order (`std::map`), path searches parallel but **results applied in ID order** (`ai_world.cpp` ≈52–124; header: "the outcome never depends on thread count or timing").

**Honesty note (TRACED):** the standing main-selftest baseline has known failures, including four save/restore bit-for-bit checks elsewhere in the suite. The scoped guarantees above are the ones with passing, targeted precedents; §7.1's gate is designed to stand on those fixtures and quarantine known-failing legacy checks by name rather than pretend the whole suite is green.

### 4.2 What is *not* guaranteed today

- **Full-game replay (TRACED).** Locomotion intent is sampled once per rendered frame and reused for every world tick that frame consumes (`game.cpp` ≈1184–1204): the same physical input assigns to different tick groupings at different frame rates. Its solidity queries read the live map at frame time, not the map as of each elapsed tick. Item entities integrate on wall-clock `dt`, and the item-drop toss in `Game::playSounds` uses unseeded `rand()`. None of this is inside the sim's determinism envelope — and none of it needs to be for the sim gate to be meaningful — but no design may claim "the game replays bit-exact" today.
- **Event order (TRACED, risk).** `SignalSim`'s component pass iterates `comps_`, an `unordered_map`; sound events are appended in that iteration order. Within one binary/libstdc++ the order is stable for a fixed insertion history, but it is not a canonical, code-level guarantee. Event *order* must be either canonicalized or explicitly included in the golden hash so a change is caught, not absorbed.
- **Tick-indexed streams across restarts (TRACED, F1).** Random-tick streams key on `worldTick`, which resets on stop/start — the stream is deterministic per run segment, not per world lifetime.

### 4.3 The constitution (invariants any design in §6 must obey)

1. **One authoritative mutator.** Exactly one thread may mutate authoritative world state, or mutation must be partitioned by a proof of disjointness with canonical application order (the DeterministicScheduler pattern). "Mostly one writer" is a violation.
2. **Time is integer.** Authoritative time advances in integer ticks/steps. Wall-clock values may *schedule* ticks; they may never *parameterize* a mutation.
3. **Inputs are data with tick identity.** Anything that affects authoritative state arrives as a recorded item stamped with (tick, phase, sequence). Live timing decides the stamp; after that, the stamp is the truth.
4. **RNG is keyed, never streamed.** Every random draw derives from (system key, entity/section key, tick, salt). Shared sequential streams whose draw order depends on scheduling are forbidden in authoritative code.
5. **Parallel work returns indexed results; application is canonical.** Workers compute into per-index slots; a single applier merges in a fixed order (mob-ID order in `AiWorld`, (system, seq) in the scheduler, sorted keys in `buildNets`).
6. **Iteration order that can reach an outcome must be canonical.** Unordered containers may hold state, but any order that leaks into events, dirty sets that affect outcomes, network IDs, or hashes must be sorted or set-reduced (max/sum with fixed reduction order).
7. **Snapshots are coherent or labelled incoherent.** A published sample names exactly one (epoch, tick). Mixing fields from different ticks into one sample is forbidden; a consumer that assembles from multiple sources must carry each source's stamp (the MCP vision stamp defect, §3.1, is the cautionary example).
8. **Epochs fence stale work.** Any worker result carries the epoch it was computed in; results from a dead epoch are dropped, never applied (the mesh epoch/seq discipline is the existing model).
9. **Budgets may drop presentation, never authoritative work.** A gear or budget that skips, coalesces, or reorders authoritative computation is a *semantics change* and must be gated and named as one.
10. **Floating point is guilty until proven innocent.** Authoritative FP reductions must have a fixed order; cross-thread FP results must be bit-compared in the gate, not tolerance-compared, wherever the existing code already achieves bit-equality.

### 4.4 Precise break risks for multi-thread designs

- A second thread reading the **live** ChunkMap "just for sampling" — there is no safe read of a mutating hash/palette structure without the mutex protocol; this is why snapshot *capture* stays on the owner (§6.4).
- Applying parallel results in completion order (the classic race-to-different-world).
- Rate-gearing a causally coupled system: SignalSim's torches → components → wires → lamps are one causal step; gearing any phase against the others changes circuit semantics (§6.8).
- Sub-tick input phase leaking into authoritative resolution before the replay format records it (§6.6).
- LOD/budget decisions based on thread availability or wall time (e.g. "skip AI thinks when busy") — outcomes become machine-dependent.
- Snapshot publication assembled field-by-field from a live, ticking source (torn samples).
- Worker results surviving an epoch change (dimension switch, save/load) and landing in the new world.
- Treating local-player prediction as authoritative and "correcting" the world to match it (§6.5).

---

## 5. The TPS machinery and what "beyond 512" demands

**TRACED — The rate ladder as shipped.**

- Engine world modes: `kTickModes = {32, 64, 128, 256}`, `kDefaultTps = 32` (`sim_thread.h`:18–19).
- Settings UI adds 512 (`kSettingsTpsModes = {32, 64, 128, 256, 512}`, `settings.h`); F7 cycles only 32–256 (`game.cpp` handleToggles).
- `kMaxDomainHz = 512` — "highest supported domain rate" (`clock_domain.h`:8–15). It is a *named support contract*, not an enforced ceiling: `SimThread` validates only "positive power of two" for both world rate and domain rates (`sim_thread.cpp` isPow2Rate / validateTickRate; constructor domain check).
- CLI: `--tps` normally accepts the five modes; **`--tps-unlock` accepts any power of two from 32 to 8192** for debug/bench (`main.cpp` ≈509–517).

**TRACED — What actually limits TPS today, in order of appearance as rate rises:**

1. **Per-world-tick authoritative work.** SignalSim's global phases (collect mount power twice per tick, torches, components over `comps_`, wake-set rebuild, wire flood, lamps — `signal.cpp` tick() ≈242–318), plus engine random ticks scanning every non-empty section in radius (§2.1: 2.05 ms avg at radius 12 — at 512 TPS that alone exceeds the whole tick budget). Component sleep exists (`setComponentSleep`) but the live game never enables it as traced; every component ticks every world tick.
2. **Per-loop fixed work at master rate (§1.3).** Edit-drain mutex, raycast, stats publish, event publish, flush check — paid per loop iteration, and loop iterations track `masterHz`, not world TPS.
3. **Serial snapshot flushing on the tick thread (§1.3).** Scales with dirty-sub-chunk count, not rate, but at high TPS it directly steals tick budget.
4. **The wait itself.** At ≥512 Hz the wait is a yield-spin for most of the period (spin threshold 1.5 ms vs period 1.95 ms at 512; at 8192 the 122 µs period is *entirely* inside the spin window and an order of magnitude below the 1 ms timer-resolution scope). A dedicated spinning core is a policy decision, not an accident to drift into (§9).
5. **Integer step truncation.** `step = 1e9 / masterHz` ns: exact through 512, truncated above (8192 → 122070 ns vs the ideal 122070.3125 ns — the scheduler would run ≈0.00026% fast; negligible for rate, but the design should carry rate as a rational, not accumulate truncation into long-running domain phases).

**PROPOSED — The "beyond 512" requirements list** (each maps to a design element in §6 or a gate in §7):

- Batch per-step overhead: stats/event publication at world-tick or geared cadence, not per master step; raycast only on ray change or world tick.
- A deadline-based (absolute epoch) scheduler option for high rates, replacing the relative `last + (step − acc)` target.
- SignalSim active-set discipline (component sleep or equivalent) proven equivalent by TickTrace before it is allowed to carry high-rate claims.
- Random ticks restructured as phase cohorts (§6.8) so their cost spreads across master steps instead of landing in one world tick.
- A declared support contract per rate: a rate is "supported" only for scenes that pass LadderBench at that rate (§7.4) — not because the CLI accepts it.

---

## 6. Design: Powers-of-Two Tick Threading (all PROPOSED unless labelled)

### 6.1 Principles

1. **Formalize, don't replace.** The design extends the existing ownership pattern (§1.1) rather than introducing a new threading model. Stage 1 changes *publication*, not *computation*.
2. **Two axes, one story.** Lanes (2→4→8) scale *who works*; gears (rate, rate/2, /4, /8…) scale *how often*. Both are powers of two, both are configuration, neither changes semantics without passing the determinism gate.
3. **"Lane" is a sim-timing role, not an OS-thread count for the whole process.** Render, audio, input, storage, and the existing gen/mesh worker pool already exist. Counting them again as "tick threads" would be dishonest arithmetic. A lane is a role with its own queue and scheduling policy; lanes 5–8 are a *pool configuration* (§6.7), not four more bespoke threads.
4. **Capture on the owner, prepare on the consumer.** Only the tick thread may read the live map. Everything other lanes touch is an immutable capture made at a tick boundary.

### 6.2 Stage 1: exactly two lanes

**Lane A — Authoritative tick thread** (evolves from today's `SimThread`, unchanged duties):

- Sole mutator of the ChunkMap and `IWorldSimulation` state; owns scheduling, edit application, clock domains, random ticks, and tick publication.
- **New duty, the only one:** at each completed world tick, publish a `TickPublication` (§6.4) — clock state plus a compact dynamic-state capture — into a triple-buffered mailbox. No rate, ordering, RNG, or subsystem behaviour changes.

**Lane B — Sub-tick thread** (new; read-only against authoritative state):

- Consumes only completed `TickPublication`s. Never calls `getBlock()` on the live map as its normal path (that would re-import the §1.4 convoy into the new lane); never mutates world, signal, entity-authority, or save state.
- Duties:
  - **Interpolation sampling:** maintain prev/cur dynamic-state pairs; answer `sampleAt(simTime)` for renderers and agents with (epoch, tick, alpha) stamps.
  - **Snapshot preparation:** fold tick publications into consumer-ready forms (agent observation packets, visibility pre-computation, MCP sample assembly) off the tick and frame threads.
  - **Local-player prediction support:** maintain the predicted local transform (§6.5) and publish prediction error at each reconciliation.
  - **Clock fan-out:** own the presentation-side `SimClock` sample (alpha computation from the published schedule), so every consumer reads the same time.
- Scheduling: blocks on publication arrival or the next interpolation deadline — it does not spin. When paused, alpha freezes at 0 and the lane idles.
- Failure discipline: if Lane B falls behind, consumers read the latest completed sample (latest-wins). Lane B can never back-pressure Lane A.

**What stage 1 deliberately does not do:** no DeterministicScheduler adoption in the live path, no JobSystem migration, no gear changes, no input-phase semantics, no SignalSim changes, no tick-persistence change (F1 is *documented and epoch-fenced*, not fixed — fixing it changes random-tick streams and needs JD's call, §9).

### 6.3 SimClock API (design sketch)

Published by Lane A (authoritative part) and completed by Lane B (presentation part). Rate is carried as an exact rational (`rateNum/rateDen` or numerator/denominator tick duration) so rates above 512 do not inherit the integer-step truncation of §5.

```cpp
// PROPOSED — engine/src/sim/include/litholattice/sim/sim_clock.h (design only)
struct TickPublication {              // written by Lane A at a tick boundary, triple-buffered
  uint64_t tick;                      // world tick index (per current F1 semantics)
  uint64_t masterStep;                // master steps completed
  uint32_t epoch;                     // dimension-load generation (fences stale work)
  uint32_t worldRateHz;               // current world rate
  uint32_t masterRateHz;              // current master rate
  int64_t  tickDurationNsNum, tickDurationNsDen;  // exact rational duration of one world tick
  int64_t  nextDeadlineSteadyNs;      // scheduled time of next tick (steady-clock ns)
  bool     paused;
  uint64_t droppedWorldTicks;         // split per F2 (master-step count kept separately)
  uint64_t droppedMasterSteps;
  // + compact dynamic capture (see 6.4)
};

struct SimSample {                    // produced by Lane B / read by any consumer
  uint64_t tick;                      // last completed tick
  uint32_t alphaNum, alphaDen;        // sub-tick phase in [0,1), exact rational
  uint32_t epoch;
  int64_t  ageNs;                     // wall age of the tick at sample time (diagnostics)
};

class SimClock {
 public:
  const TickPublication& latest() const;          // newest completed tick
  SimSample sample() const;                       // tick + alpha at read time
  // Exact conversions (integer/rational only; no float drift):
  int64_t  tickToSteadyNs(uint64_t tick) const;   // scheduled time of a tick this epoch
  uint32_t phaseForDivider(uint32_t divider, uint64_t tick) const;  // tick % divider, gear phase
  bool     due(uint64_t tick, uint32_t divider, uint32_t phase) const;
};
```

Rules: alpha is a **presentation/sample quantity only** — it must never be fed back into an authoritative computation (constitution §4.3.2). `phaseForDivider`/`due` are the single gear mechanism (§6.8); subsystems do not keep private accumulators (the `AiWorld` double accumulator is the anti-pattern this replaces).

### 6.4 Snapshot and interpolation contract

**What exists today (TRACED):** mesh snapshots (immutable, epoch/seq-fenced) and a stats struct. No dynamic-state snapshot. This contract fills exactly that gap and no more.

**`TickPublication` dynamic capture (PROPOSED)** — compact, fixed-cost, captured by Lane A at the tick boundary:

- Header: (epoch, tick, masterStep, rates, paused) per §6.3.
- Local player + dynamic entities: previous transform, current transform, velocity where authoritative, pose/mode flags. (For stage 1 the local player's body is stepped on the game thread — §1.1 — so the game thread contributes its per-tick body result into the publication ring as a *producer of its own state*, stamped with the world tick it consumed. The publication mechanism is shared; ownership of each field stays with its current owner.)
- Event batch for this tick (kind/value/position + deterministic sequence), copied at publication so Lane B and the game thread are not rival consumers of the existing event queue.
- Changed-cell / dirty-sub-chunk version list for invalidation (consumers re-snapshot blocks via the existing MeshInput path; blocks are never interpolated).
- Tick area and the config stamps a consumer needs to interpret the sample.

**Interpolation rules (PROPOSED):**

- Continuous quantities (entity/player transforms, camera-relevant poses) interpolate between completed ticks N and N+1 by alpha.
- Discrete state (blocks, circuits, inventory, events) switches at tick boundaries. Nothing geometric is interpolated for blocks.
- Consumers must tolerate: paused (alpha frozen), missing newest sample (hold last, report age), epoch change (drop all pairs, resync), and a maximum sample age beyond which they must declare staleness rather than extrapolate.
- Extrapolation is capped (proposed: ≤ 1 tick, never across epoch/pause/teleport discontinuities) and always labelled as prediction, never merged into authoritative state.
- Full-world snapshots remain what they are today — save/streaming operations. The per-tick publication is **not** a world snapshot and must never grow into one by accretion; block-region samples for agents are produced on request at tick boundaries through the existing immutable-snapshot mechanism.

### 6.5 Local-player prediction

- Prediction lives entirely on Lane B / presentation side. It extrapolates the local player's transform from the last authoritative pair plus recorded input intent (§6.6), and reconciles at each tick boundary: authoritative state wins; the correction is applied as a bounded presentation blend, and the error (distance/angle) is published as a PaceLab metric (§7.2).
- Collision for prediction uses a bounded immutable **collision window** captured by the owner at the tick boundary (same capture discipline as `MeshInput`). Prediction without a current window degrades to kinematic-only and must say so in its metrics — silently predicting through walls is worse than not predicting.
- Prediction errors never become world edits, never feed locomotion's authoritative step, and never persist into saves.

### 6.6 Input timestamping — the sub-tick lesson

**Current state (TRACED).** Input events already carry times (`InputEvent::time`, backend clock; `InputPump` restamps to `steady_clock` where used), but the live path polls per frame, samples *action state* once per frame, and SimThread stamps resulting edits with the tick current at drain time. No sub-tick phase survives into the authoritative path.

**Quantization stakes (DERIVED from rates):** one tick is 31.25 ms at 32 TPS, 15.625 ms at 64, 7.8125 ms at 128, 3.906 ms at 256, 1.953 ms at 512, 0.122 ms at 8192. Sub-tick phase is therefore a *first-order* input-fidelity question at 32–128 TPS (the shipped default range), a minor one at 512, and negligible for human input at 8192 (device polling and display latency dominate long before).

**PROPOSED progression:**

1. **Record (stage 2):** every input/edit carries `(tick, phase, sequence)` — phase as a fixed-point fraction of the tick from the SimClock sample at receipt, sequence as a per-tick ordinal. Phase is used for ordering diagnostics, prediction, and replay fidelity only.
2. **Order (stage 2):** within a tick, replay and tooling sort by (tick, phase, sequence) — never by arrival wall time.
3. **Resolve (later, only if JD wants it):** authoritative sub-tick resolution for *discrete* actions designed for it (e.g. break/place ordering races), processed by Lane A at phase quanta of tick/2, /4, /8 via the master clock — the master/divider machinery already generalizes to this. The moment phase affects an outcome, the replay format must record it (constitution §4.3.3) and TickTrace must gate it.

The lesson imported from sub-tick systems generally: **timestamp at capture, because phase cannot be reconstructed later.** The cheapest time to adopt this is before netplay and before enterprise consumers depend on input timing.

### 6.7 The scale ladder: lanes 3–8

Evaluated honestly — including what each lane *cannot* do:

| Lane | Role | Determinism discipline | Honest limit |
|---|---|---|---|
| 1 | Authoritative tick (exists) | Sole mutator | The serial bottleneck everything else routes around |
| 2 | Sub-tick sampler / interpolation / snapshot prep (§6.2) | Read-only | Bounded by publication cost on Lane A |
| 3 | **Deterministic post-tick compute**: pure work over the immutable tick publication — AI path-search candidates, visibility sets, agent observations, analytics | Indexed results; Lane A (or the game thread, for game-owned state) applies in canonical order — the `AiWorld` pattern | Only helps work that is genuinely pure over a snapshot; SignalSim's causal core is not |
| 4 | **World-services coordinator**: streaming prioritization, mesh-snapshot packaging requests (capture still on Lane A), save *preparation* from consistent captures, consumer fan-out | Coordinates; captures nothing from the live map itself | Save consistency still requires a tick-boundary capture or the existing stop pattern; this lane removes encoding/IO from the pause, not the capture |
| 5 | **Consumer lane A**: MCP/agent sample assembly for multiple pilots, software-render tiles for agent cameras | Consumes publications + mesh snapshots only | Vision cost scales with pilots; per-pilot gears (§6.8) are the control |
| 6 | **Consumer lane B / observability**: replay-trace hashing, metrics, recording, enterprise telemetry | Read-only; hashing is canonical-order | Competes for memory bandwidth with Lane A — must be budget-capped |
| 7 | **Pure-phase partition worker** (pool): DeterministicScheduler stages over immutable state — random-tick cohorts, AI planning, broadphase, non-causal queries | Declared Read/Write resources; command buffers drained canonically | Only as good as the disjointness proofs; per-chunk signal partitions need a boundary protocol (§6.8) |
| 8 | **Pure-phase partition worker / persistence prep**: second partition group + save encoding, asset/texture preparation | Same as 7; persistence works from captures | Diminishing returns are real: at some point bandwidth, not threads, is the wall (§2.4) |

**Candidate decompositions, judged:**

- **Parallel snapshot consumers — strongest early scaling.** Multiple renderers, agent cameras, and enterprise observers consuming one publication stream costs Lane A almost nothing per added consumer. This is where lanes 5–6 earn their keep.
- **Parallel post-tick pure phases — strong, precedented.** AI path search (already parallel + indexed-apply), visibility, analytics. Lane 3/7 territory. The DeterministicScheduler exists precisely to make this provable.
- **Spatial partitions — conditional.** Random-tick sections and per-chunk queries partition cleanly *if* effects are deferred through command buffers. Signal propagation partitions by chunk only with an ordered boundary-resolution pass; by network more naturally (the engine's `SignalPropagationPolicy` already specifies PerChunk/PerNetwork/WholeWorld grains with canonical ordering, defaulting to PerChunk — `engine/src/sim/include/litholattice/sim/signal_propagate.h`). Pattern A from that header (one system, internal canonical slicing) precedes Pattern B (per-partition systems) — and both wait for profiling to indict propagation specifically.
- **Gen/mesh workers — already exist; do not double-count.** They are the application worker pool, not sim-timing lanes. Their scaling problem is structural (§2.4), and fixing it (JobSystem routing, parallel snapshot packaging) is orthogonal work that the ladder may *use* but must not claim as a tick-threading result.

### 6.8 Harmonic rate gears

A **gear** is a subsystem cadence expressed as an integer divider of the base rate, with a fixed phase, driven from `SimClock::due(tick, divider, phase)` — no private accumulators, no wall time. Gears compose with clock domains: domains gear *up* from the world rate to the master rate; subsystem gears gear *down* from the world (or master) rate. Both are the same integer-divider idea the scheduler already implements.

| Subsystem | Gear candidate? | Form | Semantics risk |
|---|---|---|---|
| AI | **Yes — strongest** | Align 8–16 Hz to exact dividers of base (e.g. base 512 → AI at /32 = 16 Hz); integer accumulator replaces the double one | Low: AI already runs off-rate with LOD 1/2/4. Must prove trajectory equivalence in TickTrace at each (base, gear) pair claimed |
| Random ticks | **Yes, as phase cohorts — not as frequency cuts** | Split sections into 2/4/8 deterministic cohorts spread across master sub-phases of the *same* world tick; total picks and per-section keyed values unchanged | Cutting frequency (fewer picks/sec) or bursting picks changes growth/decay semantics — that is a gameplay change needing JD's explicit approval, not an optimization |
| MCP/agent sampling | **Yes** | Per-pilot dividers (vision/audio already rate-capped at 4 FPS / 8 chunks/s) | None authoritative; samples carry their (tick, epoch) stamp |
| Audio occlusion prep | **Yes** | Distance/importance gears over the existing 32/16/8 Hz precedent | Presentation only |
| Streaming prioritization | **Yes** | Recompute want-lists at /4 or /8 cadence; urgent under-player requests bypass | None authoritative |
| Remesh/light presentation | **Yes (already geared informally)** | Align the ~120 Hz coalesce to a master divider | Output-only; dirty *tracking* stays per-tick |
| Autosave preparation | **Yes, preparation only** | Gear dirty-column tracking + encoding prep; final consistency capture unchanged | World-permanence rules govern; no gear may weaken the consistency boundary |
| Animation (texture) | **Rebase, don't gear** | Move shader animation time from app-elapsed to SimClock sim-seconds so pause freezes it (§3.2) | Presentation only |
| Sprites (sim) | **No (not silently)** | Per-tick integration is its semantics | A gear changes motion; treat as game-design change if ever proposed |
| Vitals | **Not without a decision** | Constants are tick-count semantics calibrated at 32 TPS (§3.1) | Converting to seconds changes behaviour at every non-32 rate — JD's call (§9) |
| SignalSim phases | **No** | Torches/components/wires/lamps are one causal step | Gearing any phase against the others rewrites circuit semantics |
| Clock domains | **Already gears (upward)** | Master/divider machinery exists | Domain *logic* is still a stub; gears don't change that |

### 6.9 How the two axes compose — and where ">512 TPS" actually comes from

The powers-of-two story is one sentence: **one authoritative base rate, integer dividers in both directions, and lane counts that double the parallel pure work around a serial causal core.**

- A college running a computing-lab world at 2048 TPS does not need AI at 2048 Hz, random ticks bursting 2048×/s, or eight copies of the tick thread. It needs: a 2048 Hz base for the clock domains/circuits under study, AI geared to /128, sampling geared per observer, random ticks spread as cohorts, and lanes 3–8 absorbing observation, analysis, and persistence work that would otherwise steal the tick thread.
- Conversely, a higher base rate alone buys nothing once per-loop fixed costs dominate (§5): at 8192 TPS the budget is 122 µs and the current loop spends a meaningful fraction of that on bookkeeping. The enterprise story is therefore **both axes, plus the §5 batching work** — and LadderBench (§7.4), not the CLI, decides which (scene, rate, lanes, gears) combinations may be called supported.
- Multiple dimensions/shards — one tick authority per dimension with a shared epoch mapping — remain the honest answer for *world size* scaling, as distinct from *rate* scaling. Whether dimensions share one SimClock authority is JD's question (§9); the design does not presume it.

---

## 7. Testing framework design (all PROPOSED)

JD asked for robust testing frameworks designed *before* implementation. Four named frameworks, each with harness shape, metrics, and pass criteria defined up front. Everything labeled "reuse" exists in the tree today; everything else is new tooling to build alongside stage 1 — not after it.

### 7.1 TickTrace — the determinism golden gate

**Purpose.** Prove that threaded and geared variants produce bit-identical authoritative outcomes. This is the gate every stage in §10 must pass; nothing scales past it.

**Harness shape.**

- Fixed world seed + fixed initial world per scenario (reuse the circuit scene and a terrain scene from the existing benches).
- Fixed recorded input/edit stream: items stamped `(tick, phase, sequence)` (§6.6), replayed through the tick-stamp path — *not* through live frame sampling. **Reuse:** `ReplayInputBackend` / `InputSystem::processFrame` precedent; the save/restore/tick-compare precedent in the game selftest (§4.1.1).
- Configuration matrix per run: logical lanes {1 (baseline serial), 2, 4, 8} × base rates {32, 128, 512} × gear combinations under test (each gear on/off independently where legal).
- Hashing: canonical state bytes hashed at every tick (cheap scenes) or at power-of-two checkpoints (1, 2, 4, … 4096) for heavy scenes — component state (reuse `SignalSim::exportState`'s sorted serialization), world cells in canonical column order, entity state, **and an event-order hash**. **Reuse:** `DeterministicScheduler::hashBytes/hashU64` (FNV-1a) from `det_sched.h`.
- Every run emits a trace file: config, per-checkpoint hashes, first-divergence tick + subsystem if any.

**Pass criteria (defined now).**

- Identical hash at **every** checkpoint across all lane counts and all approved gear configurations. No tolerance, no "close enough."
- No missing, duplicated, or reordered authoritative events (event hash covers this).
- A divergence report names the first differing tick and the state region — a bare "hash mismatch" is a harness failure, not a test result.
- Known-failing legacy save fixtures are quarantined **by name** in the harness config (§4.1 honesty note); quarantine is visible in every report and can only shrink.

### 7.2 PaceLab — the pacing harness

**Purpose.** Measure whether TPS and frame pacing *hold* — distributions, not averages. Directly targets JD's stable-30/60 (120 for double-buffered vsync) goal and the variance enemy.

**Instrumentation first (the §2.2 lesson).** Per loop iteration, record: preceding gap, steps run, and phase durations — domains, edit drain/apply, `IWorldSimulation::tick` (with SignalSim sub-phases comps/mark/wires/lamps where available — `SignalSim` already accumulates a `PhaseMs` split), engine random ticks, raycast, flush/snapshot, event+stats publication, wait. Phase timing must be compile-time switchable and zero-cost when off.

**Metrics.**

- Tick execution: p50/p90/p99/p99.9/max per phase and total; scheduler latency (due → tick start; tick end → publication).
- Backlog: accumulator depth over time, catch-up iterations per loop, dropped world ticks and master steps (split per F2).
- Snapshot health: publication age at consumption, alpha error, prediction error (§6.5).
- Frame: input / update / upload / render / present / total, p50/p99/max; **hitch counts at >16.7 ms, >33.3 ms, and a severe threshold of >50 ms**, counted after warmup.
- Per-second lines compatible with the existing bench output, so tonight's numbers and future numbers diff cleanly.

**Reuse:** `perf_headless`'s `TimedSimulation` wrapper pattern and JSON emitter; the app bench's per-second lines and final summary; `MsSamples` (avg/percentile) from `bench_stream.cpp`.

**Pass bar (proposed defaults; JD tunes the numbers, the shape is the deliverable).**

- Achieved TPS within a declared tolerance (proposed ±1%) of target over the full run, per scenario.
- p99 authoritative tick execution under the tick budget for the target scene/rate.
- Zero severe hitches (>50 ms) in flyover after warmup; hitch counts at 16.7/33.3 ms reported even when passing.
- No unbounded backlog growth: accumulator depth and upload-queue depth return to baseline after induced disturbances.
- Dropped world ticks = 0 in standard scenarios; nonzero is a failure unless the scenario is explicitly an overload-recovery test.

### 7.3 Load Scenario Matrix

Each scenario specifies: seed, scene, duration, warmup, configuration, and which gate(s) it feeds. (G = TickTrace gate, P = PaceLab gate, X = exploratory only.)

| Scenario | Shape | Feeds |
|---|---|---|
| Idle spawn | Loaded world, no input, 60 s | P |
| Orbit | Camera orbit at fixed radius, 120 s | P |
| Streaming flyover | Bench-path flight across ungenerated terrain at set speed | P (hitch bar applies) |
| Circuit stress | 5,531-component scene at 32/64/128/256/512 TPS | G + P |
| EUV-on at low world TPS | World 32 TPS, EUV domain enabled (F6 shape) | P |
| Random-tick-heavy | Radius 12, all blocks opted in | G + P |
| Mob/AI density | 64 and 128 mobs, geared AI on/off | G + P |
| Save during play | Autosave + manual save mid-flight, incl. heavy-edit world | P + world-permanence checks |
| Streaming insertion during play | Continuous flight with insertion batches | P |
| Low-TPS input fidelity | 32/64/128 TPS with timestamped input stream | G (phase ordering) |
| Multi-consumer | 1/2/4 MCP pilots sampling simultaneously | P |
| Long soak | 10-minute stream soak (exists) + extended variant | P + memory-drift |

### 7.4 LadderBench — the scale protocol

**Matrix:** lanes {2, 4, 8} × base rates {512, 1024, 2048, 4096, 8192} on the circuit-stress and computing-lab scenes, gears at their §6.8 defaults.

**Required reporting per cell:** achieved TPS; tick p50/p99/max; scheduler publish latency; per-lane CPU; speedup and parallel efficiency vs the 2-lane cell; TickTrace result; hitch counts; memory. The first failing metric in the first failing cell is the headline — a ladder run that "mostly passed" reports its failure point, not its averages.

**Support rule (PROPOSED):** a (scene, rate) pair is *supported* only when its cell passes PaceLab's bar **and** TickTrace. The CLI accepting 8192 (§5) makes it *configurable*; LadderBench makes it *supported*. Marketing, docs, and enterprise claims may cite only supported cells.

### 7.5 What gets built where

| Need | Build on existing | New tool |
|---|---|---|
| Determinism gate | Selftest precedents; `det_sched` hash helpers; replay input backend | `TickTrace` runner (headless, config-matrix, trace files) |
| Pacing | `perf_headless` sections; app bench JSON/lines; `MsSamples` | Phase instrumentation in `SimThread` (switchable); `PaceLab` aggregator + hitch accounting |
| Scenarios | Circuit scenes, bench paths, stream soak | Scenario manifests (seed/config/duration/gates) shared by TickTrace + PaceLab |
| Scale ladder | `--tps-unlock` path | `LadderBench` driver sweeping lanes × rates over the manifests |

---

## 8. Risks

- **Snapshot bandwidth (PROPOSED-design risk).** If the tick publication grows beyond its compact brief (§6.4), Lane B becomes a memory-bandwidth tax on Lane A. Mitigation: fixed-size publication, changed-lists instead of state copies, PaceLab publishes publication cost per tick.
- **Prediction whiplash.** Aggressive extrapolation at low TPS (31 ms ticks) can produce visible correction snapping. Mitigation: conservative caps, error metrics in PaceLab, prediction is presentation-only and can be disabled per consumer.
- **Parallel packaging cost.** For small scenes, indexing/copying pure work for lanes 3/7 can cost more than doing it serially. Mitigation: lane activation thresholds by measured workload, and TickTrace/LadderBench run at small *and* large scenes.
- **Spatial partition overreach.** Global circuit networks and radio make "partition by chunk" a semantics project, not an optimization (§6.7). Mitigation: Pattern A before Pattern B; propagation is partitioned only on profiling evidence.
- **Busy-spin policy at high rates.** ≥512 Hz already yield-spins; high-rate enterprise profiles may want a dedicated core. That is a machine-policy decision with laptop/shared-host consequences (§9).
- **Gear drift into gameplay.** The easiest failure mode in this entire design: a "performance gear" that quietly changes growth rates, AI behaviour, or vital pacing. Mitigation: constitution §4.3.9 + TickTrace per gear + the §6.8 table's explicit no-go rows.
- **Metric self-deception.** Tonight's anomaly (§2.2) exists partly because two different "tick ms" numbers were compared as if they were one. Mitigation: PaceLab's phase schema is the single vocabulary; old metrics are mapped or retired, never mixed.

---

## 9. Open questions for JD (genuinely his to decide)

1. **Lane semantics:** does "thread count" in the product story mean dedicated sim-timing lanes (this document's definition), total worker threads, or a packaged performance tier name?
2. **Enterprise priority:** for beyond-512 machines, what matters most — maximum TPS on compact computing scenes, maximum world size (sharding), or maximum concurrent agent/observer consumers? The ladder optimizes differently for each.
3. **Determinism scope:** must bit-exactness hold across *every* lane count and gear (this document's gate design), or only within a declared replay/competition configuration?
4. **Sub-tick authority:** should input phase ever alter authoritative outcomes (§6.6 step 3), or remain presentation/prediction-only permanently?
5. **Tick persistence (F1):** should `worldTick` survive stop/start within a dimension load? Fixing it changes random-tick streams vs today — a visible behaviour change that needs an explicit yes.
6. **Vitals semantics (§6.8):** are oxygen/stamina rates defined in ticks (faster in wall time at high TPS, as today) or in seconds (constant across rates)?
7. **Spin policy:** is a dedicated busy-spinning core acceptable at the highest tiers, and on which machine classes?
8. **Dimension clocks:** do multiple dimensions share one SimClock authority, or run independent tick authorities under a shared epoch mapping?

---

## 10. Staged implementation plan (PROPOSED)

**Stage 1 — Two lanes, nothing else changes.**
SimClock publication from Lane A; Lane B (sub-tick) with interpolation sampling and snapshot preparation; compact tick publication per §6.4. No rate, ordering, RNG, gear, or subsystem changes. TickTrace and PaceLab are built *with* this stage and gate it. Exit criteria: TickTrace identical vs the 1-lane baseline on all §7.3 gate scenarios; PaceLab shows no TPS/pacing regression; publication cost per tick measured and within its budget.

**Stage 2 — Presentation and input metadata.**
Renderer/agent interpolation goes live on SimClock; local-player prediction + reconciliation (§6.5); input `(tick, phase, sequence)` recording (§6.6 steps 1–2; phase does not affect outcomes); texture animation rebased to sim time. Exit: prediction error and hitch metrics in PaceLab at or better than stage-1 baselines.

**Stage 3 — Four lanes.**
Lane 3 (deterministic post-tick compute; AI path planning is the model workload) and Lane 4 (world-services coordination; save *preparation* off the frame path). Exit: TickTrace across {1,2,4} lanes; flyover hitch bar met with streaming active.

**Stage 4 — Harmonic gears.**
AI gear alignment (integer accumulator), random-tick phase cohorts, per-pilot sampling gears, occlusion/streaming-priority gears, autosave-prep gear. Each gear lands individually, each behind TickTrace or an explicit "presentation-only" classification. Exit: gear matrix green in TickTrace; LadderBench re-run.

**Stage 5 — Eight lanes and high-rate profiles.**
DeterministicScheduler-backed pure-phase partitions (lanes 7–8); spatial work only where causality is proven; §5 batching (geared publication, deadline scheduling option); enterprise profiles validated cell-by-cell through LadderBench before any rate above 512 is called supported.

---

## Appendix A — Anchor index (as of 2026-10-12)

- Scheduler/contract: `engine/src/sim/sim_thread.cpp` (run ≈237; accumulator/caps ≈301–337; pipeline ≈349–362; flush ≈265, 379–383; waitUntil ≈228–235, 425); `engine/src/sim/include/litholattice/sim/sim_thread.h` (modes:18–19; EditCmd:36–44; SimStats:59–76; getBlock:116–124); `engine/src/sim/include/litholattice/sim/clock_domain.h`:8–21; `engine/src/sim/include/litholattice/sim/world_simulation.h`.
- Game wiring: `game/LithoKraft/src/game/game.cpp` (SimConfig ≈596–601; saveWorld ≈647–665; pumpStreaming ≈674–697; update ≈1127–1345; uploadMeshes ≈1395–1455; ground pulse ≈1481–1486); `game/LithoKraft/src/app/main.cpp` (loop ≈601–660; TPS validation ≈509–517); `settings.h`:45–47.
- Signal: `game/LithoKraft/src/content/signal.cpp` (exportState sort ≈24–28; tick ≈242–318; buildNets ≈358–465); `signal.h` (radio/jammer ≈59–141; containers ≈196–209); `signal_world.cpp`.
- Determinism precedents: `engine/src/sim/include/litholattice/sim/det_sched.h`; `engine/tests/engine_tests.cpp` ≈3623–3742; `game/LithoKraft/src/app/selftest.cpp` ≈133–218; `game/LithoKraft/src/game/input_selftest.cpp` ≈186–205; `engine/src/sim/include/litholattice/sim/signal_propagate.h`.
- Random/AI: `engine/src/sim/random_ticks.cpp`; `engine/src/sim/include/litholattice/sim/random_ticks.h`; `engine/src/ai/ai_world.cpp` ≈46–124; `engine/src/ai/include/litholattice/ai/ai_world.h`:1–4.
- Threading/jobs: `engine/src/core/include/litholattice/core/threading.h` (TripleBuffer ≈16–40; JobPool ≈44–105; HandoffQueue ≈142–158); `engine/src/core/include/litholattice/core/job_system.h`.
- Gen/mesh: `engine/src/world/dimension.cpp` ≈6–25; `engine/src/procgen/pipeline.cpp` ≈36–83; `engine/src/world/column_generator.cpp` ≈44–68; `engine/src/world/mesh_input.cpp`; `engine/src/world/include/litholattice/world/mesh_input.h`; `game/LithoKraft/src/game/chunk_streamer.cpp` ≈30–66; `game/LithoKraft/src/game/bench_stream.cpp`.
- Clocks: `engine/src/audio/include/litholattice/audio/audio_engine.h`; `engine/src/audio/occlusion_thread.cpp` ≈106–165; `engine/src/mcp/include/litholattice/mcp/mcp_senses.h` ≈44–56; `game/LithoKraft/src/game/mcp_senses_adapter.cpp`; `engine/src/sprite/sprite_system.cpp` tick(); `game/LithoKraft/src/game/include/lithokraft/game/player_adapter.h` ≈67–71; `engine/src/input/include/litholattice/input/input_thread.h` ≈22–38.
- Perf harness: `game/LithoKraft/src/app/perf_headless.cpp` (TimedSimulation ≈56–80; scaling ≈123–155; circuits ≈324–368).

## Appendix B — Terms

- **Lane:** a sim-timing role with its own queue/scheduling policy (§6.1.3). Not a whole-process thread count.
- **Master step:** one iteration of the master clock at `masterHz`; world ticks and domain steps are integer-divider children of it.
- **Gear:** a subsystem cadence expressed as an integer divider (with fixed phase) of a base rate (§6.8).
- **Tick publication:** the immutable (epoch, tick)-stamped bundle Lane A emits at a tick boundary (§6.4).
- **Alpha:** the sub-tick phase in [0, 1) between the last completed tick and the next; presentation-only.
- **Supported (rate/scene):** passing TickTrace + PaceLab at that configuration (§7.4) — as opposed to merely *configurable*.
