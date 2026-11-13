# The Timing Handshake — Pull-Tick Prototype Report

**2026-10-13 · Experiment, isolated tree — production source untouched**

**Attribution:** The **Timing Handshake** algorithm was designed by **David
"JD" Rittenhouse**, recovered from his teenage game-hacking work, and dictated
to Elfie on 2026-10-12/13. JD's summary of its core: synchronization is *"just
two synchronized QPC calls — one from the pull and one from the push."*

**Test bed:** [pull_tick_testbed.cpp](sandbox://workspace/experiments/pull-tick/pull_tick_testbed/pull_tick_testbed.cpp)
(single file, ~700 lines, builds with `g++ -O2 -std=c++17 -pthread`; modes:
`det phase freerun fail perf heavy`)
**Tree:** full source copy at `~/workspace/experiments/pull-tick/` (see §8 for
isolation proof).

---

## 0. Verdict up front

The Timing Handshake **preserves determinism exactly** — bit-identical
outcomes to the current sequential dispatcher across every configuration
tested (24/24) — *provided* the engine adopts phase-boundary state
visibility (snapshot reads, commit at the barrier), which is the real design
price. On this 2-core VM with lightweight stub components it is **not
faster** than the dispatch loop; it reaches parity only with heavy per-fire
work. Its wins are structural (cadence phases, genuinely free-running
non-critical systems, sync cost that scales with threads, not components),
not raw speed at current component costs. The missed-window policy is an
open question for JD: the only simple alternative (deadline-skip) was
**proven to break determinism**.

## 1. The design as built

**Status quo (baseline):** a tick executes; a for loop walks a dispatch list
of components and *pushes* the tick to each — modeled on `SimThread`'s master
loop over clock domains and on `ITickable::onTick(TickContext&)`
(`engine/src/sim/include/litholattice/sim/tick_interfaces.h`), with
deterministic per-(component, tick) splitmix64 work exactly like
`TickContext.rng`.

**The Timing Handshake (as dictated, including both designer clarifications):**

1. **Pull, don't push.** Each pull thread runs an **infinite while loop**
   over its components, constantly **polling** the main game thread with a
   can-tick query — JD's words: *"can I tick? is it time to tick?"* If the
   query returns true, the component ticks. (Clarification, 2026-10-13
   00:04 EDT; the test bed's loop is bounded by the test length only.)
2. **The main game thread is the authority.** It alone opens tick windows,
   in fixed order. In the prototype the query is
   `TimingHandshakeAuthority::canTick(phase)` — an atomic read of the
   authority-published phase index.
3. **Four phases per cycle: tick, tock, tick, tock** — and the phase count
   is **tunable** (clarification, 2026-10-13 00:07 EDT): nothing structural
   blocks raising it. Tested at 4, 8, and 16 phases/cycle (§3.1).
   Components carry a cadence class — every phase / half-cycle heads /
   cycle head — which is the clock-domain divider idea expressed as a
   phase schedule.
4. **Push/pull rendezvous.** After firing, each pull thread *pushes* its
   results and then **waits for the others**: no thread may pull phase N+1
   until every thread has pushed phase N and the authority has committed.
5. **Synchronization is the Timing Handshake itself:** two synchronized
   high-resolution-counter reads — the authority stamps each phase open
   (push side), the pull thread stamps its successful query (pull side) —
   and time agreement comes from both sides reading the **same counter**,
   not from lock-step messaging. Windows mapping: **QPC
   (QueryPerformanceCounter)**. Linux prototype equivalent:
   **`std::chrono::steady_clock`, i.e. `clock_gettime(CLOCK_MONOTONIC)`** —
   used for both stamps here, and §3.2 tests that pull-side reads never
   precede the authority's stamp for the same phase.
6. **Free-run systems.** Non-tick-critical components never query, never
   rendezvous — they "just kind of happen" on their own thread and cannot
   perturb the deterministic outcome (§3.4).

**Scope boundary (JD, 2026-10-13 00:09 EDT — hard rule):** the Timing
Handshake applies **only to the deterministic circuit systems — Trace and
Signal inside the game, and the core sim timing JD calls "our GPS."**
**Mobs/entities must NOT use it.** They need their own independent
ticking system. JD's reasoning, verbatim: *"otherwise they run at 512
and the bones break"* — entity/bone updates at the full 512 circuit rate
are wrong and destructive. The prototype's free-run pool (§3.4) models
non-critical *engine* systems only; it is **not** the mob system, and no
part of this prototype is. The independent mob ticking system is future
work, out of scope here.

**Interpretation choices I had to make (flagged for JD):**

- **Phase-boundary visibility.** During a phase, components read a
  *snapshot* of peer state taken at the last boundary and write to a
  pending buffer; the pending buffer commits atomically at the barrier.
  This is the same current/next double-buffer discipline the signal solver
  already uses, and it is what makes intra-phase execution order (and
  thread count) unable to change the outcome. Without it, a pull model
  cannot be deterministic. This is the design's real contract change.
- **"Fires at a deterministic time"** is implemented as a deterministic
  *phase slot* plus a canonical logical firing order (phase-major,
  id-minor). Physical completion order inside a phase deliberately varies
  run to run; the outcome does not (proven in §3.1).
- **Polling** is spin-with-yield, matching the dictation (tight polling
  loop). A production version would likely hybridize (short spin, then a
  futex/condition-variable wait) to stop burning cores — that deviates
  from the letter of the dictation, so the prototype does not do it.
- **Shard assignment** is static round-robin by component id, mirroring
  how clock domains partition work today.

## 2. Test load

Stub components model **circuit-class components only** — signal logic in
the Pulsor/CatchOR shape, per the scope boundary in §1 (mobs/entities are
excluded from the Timing Handshake entirely and are not modeled here).
Each fire does deterministic splitmix64 work (per-component cost
derived from seed + id + phase tick) and mixes two neighbours' snapshot
state into its own — so firing *order relative to the snapshot* matters
and any protocol leak shows up in the outcome hash. Outcome hash =
FNV-1a over final states in id order; log hash = FNV-1a over the canonical
firing log. Standard run: **10,000 phase ticks** (2,500 four-phase
cycles), sizes 64 / 512 / 4,096 components, pull threads 1 / 2 / 4. All
timings from a **2-core VM** (`nproc` = 2) — T=4 oversubscribes, and
pull timings vary ±~30% between runs; hashes never vary.

## 3. Results

### 3.1 Determinism — 24/24 PASS

Pull model vs sequential baseline, identical scripted workloads:

| Config | Outcome hash | Log hash | Fires |
|---|---|---|---|
| 64 comps, seed 1, T=1/2/4 | `a91a1dc24191e9e1` | `6e2bf5bf5699af43` | 377,500 |
| 64 comps, seed 42, T=1/2/4 | `9f83444fa8d62c16` | `6e2bf5bf5699af43` | 377,500 |
| 64 comps, seed 0xC0FFEE, T=1/2/4 | `961ad933f233f10e` | `6e2bf5bf5699af43` | 377,500 |
| 512 comps, seed 1, T=1/2/4 | `b05de851ef9d7608` | `ed5e3a3fcb15f15b` | 2,990,000 |
| 512 comps, seed 42, T=1/2/4 | `0f366a0db6de3c20` | `ed5e3a3fcb15f15b` | 2,990,000 |
| 512 comps, seed 12648430, T=1/2/4 | `1d315fca9114bba5` | `ed5e3a3fcb15f15b` | 2,990,000 |
| 4,096 comps, seed 7, T=1/4 | identical | identical | — |
| **8 phases/cycle**, 512, seed 42, T=1/4 | identical | identical | — |
| **16 phases/cycle**, 512, seed 42, T=1/4 | identical | identical | — |

Every pull configuration reproduces the sequential outcome and logical
firing sequence **exactly**, at every thread count and every phase count.
Determinism is a property of the protocol (snapshot + barrier commit), not
of the schedule.

### 3.2 Phase protocol — PASS

- Fires = **2,990,000**, exactly the analytic schedule count (no fire
  outside its cadence's phases; every scheduled fire happened).
- **Barrier-order violations: 0** — instrumented proof that no thread ever
  pulled phase N+1 before all threads pushed phase N (each pull asserts
  `committedPhases == N`).
- **QPC-agreement violations: 0** — in 10,000 phases × 4 threads, no
  pull-side counter read ever preceded the authority's stamp for that
  phase. The two synchronized counter reads agree, as designed.
- 10,000 authority phase stamps recorded, monotonic throughout.

### 3.3 Performance — wall-clock per 10,000 phase ticks (ms, 2-core VM)

Light work (~12 splitmix iters/fire):

| Components | Sequential | Pull T=1 | Pull T=2 | Pull T=4 |
|---|---|---|---|---|
| 64 | 34.3 | 70.9 | 71.7 | 86.6 |
| 512 | 261.5 | 404.1 | 362.7 | 335.0 |
| 4,096 | 2,134.8 | 3,056.9 | **1,835.1** | 2,756.2 |

Zero work (pure protocol cost — query, rendezvous, carry-over, commit):

| Components | Pull T=1 | Pull T=2 | Pull T=4 |
|---|---|---|---|
| 64 | 24.9 (2.5 µs/phase) | 44.7 | 71.4 |
| 512 | 130.8 | 116.4 (11.6 µs/phase) | 172.6 |
| 4,096 | 1,392.4 | 1,242.4 (124 µs/phase) | 1,382.4 |

Heavy work spot check (~1,024 iters/fire, 512 comps, 1,000 phase ticks):

| Sequential | Pull T=1 | Pull T=2 | Pull T=4 |
|---|---|---|---|
| 2,186.2 ms | 2,962.6 (0.74×) | 2,215.4 (0.99×) | 2,090.9 (1.05×) |

Reading the tables honestly:

- At light component costs the handshake is pure overhead: ~2.5 µs/phase
  floor at small scale, and it *loses* to the sequential loop almost
  everywhere on 2 cores. The 4,096-component zero-work row (~124 µs/phase)
  is dominated not by the handshake but by the double-buffer carry-over
  copies (4,096 state slots per phase) — a cost the sequential baseline
  also pays inside its light-work number, and one a real implementation
  would cut with dirty-slot commit lists.
- The single light-work win (4,096 comps, T=2, 1.16×) and the heavy-work
  trend (0.74× → 0.99× → 1.05× as threads grow) show the crossover shape:
  the model pays when per-fire work is large *and* real cores exist.
  LithoKraft's real tick-critical loads (signal propagation over chunk
  columns, chip domains at 512 Hz) are far heavier per fire than these
  stubs — but that win is **projected, not measured**, and this VM cannot
  measure it fairly.

### 3.4 Free-run — PASS

With a free-run thread hammering splitmix work alongside a 512-component
deterministic run: outcome and log hashes **unchanged**, and the free-run
thread completed **225,892,096 evaluations** (~2.3×10⁸/s, the serial-chain
latency bound) without a single query or barrier. (Methodology note: an
earlier counter read ~10¹¹ because the work's result never escaped and
the compiler legally deleted it; the measurement was fixed by making the
accumulated value escape into a sink, and the corrected figure is the one
reported.)

### 3.5 Failure modes

- **Straggler (barrier convoy):** one component busy-waiting 3 ms at
  phase 5,000 moved that phase from a 0.021 ms median to **3.014 ms** —
  the slowest component gates the entire phase, ~1:1. Timing changed;
  the outcome stayed bit-identical. Tail latency *is* the worst
  component, every phase, by construction.
- **Missed window / deadline-skip policy — TESTED, FAILS:** an
  experimental variant where the authority closes a phase after a 50 µs
  deadline and commits without late threads **diverged from the
  sequential outcome hash**. Skipping or deferring a fire changes the
  snapshot the next phase reads, and determinism is gone. The variant
  also could not keep arrival accounting phase-clean (late arrivals from
  a closed phase bleed into the next phase's count without extra
  per-phase tagging). **Conclusion: with this protocol, there is no cheap
  skip.** The prototype therefore ships the simplest policy — **wait**
  (the barrier holds) — and the missed-window policy is an **open design
  question for JD**, not a decision made here.
- **Never-returning component:** the shipped policy has no timeout, so
  one hung component deadlocks the phase clock — demonstrated by
  construction (straggler cost propagates without bound as cost → ∞).
  Any production version needs a watchdog story JD signs off on; a
  watchdog that *skips* re-opens the determinism problem above, and a
  watchdog that *kills the world tick* is a world-permanence decision.

## 4. Design assessment

**Does it preserve the 512 TPS determinism doctrine?** Yes — conditionally.
The doctrine survives intact *if* tick-critical components accept
phase-boundary visibility: read the snapshot, write pending, commit at
the barrier. The engine already thinks this way in the signal solver
(current/next buffers) and in `TickContext`'s deterministic RNG. The
friction is `ITickable` block behaviors that read *live* world state
mid-tick; those would need snapshot views or confinement to one shard.
512 TPS itself is untouched by the model: phases are logical fire points
under the master clock, and raising the phase count (tested to 16) is a
schedule change, not a rate change.

**Where it beats the dispatch loop**

- Sync cost scales with **thread count**, not component count — the
  per-phase handshake is O(T) atomics; the sequential loop is O(N) calls
  with zero parallelism.
- Cadence phases generalize clock domains cleanly: a 512 Hz chip domain
  and a 32 Hz world domain are just different cadence classes on one
  phase clock, instead of divider bookkeeping in one master loop.
- Free-run systems are *actually* free — no list membership, no
  dispatch cost, no accidental coupling to the deterministic outcome.
- Heavy, parallel-friendly loads (signal propagation, chip domains)
  get real multicore scaling once per-fire cost clears the ~µs/phase
  handshake floor.

**Where it loses**

- Light components: the handshake floor (~2.5 µs/phase minimum, plus
  carry-over) makes it strictly worse than a for loop. Much of the
  current dispatch list is light.
- Tail latency: every phase is as slow as its slowest tick-critical
  component. The sequential loop has the same total work but no
  rendezvous amplification of a single bad actor.
- Polling burns cores (by design, per the dictation). On a 2-core
  machine the pull threads measurably contend with the authority.
- Deadlock surface: one hung component stops the world clock. The
  current loop has the same theoretical hang, but a barrier makes it
  structural rather than incidental.
- Complexity: snapshot discipline, shard ownership, cadence
  registration, and the unresolved missed-window policy are all new
  invariants every future system must respect. JD called it "a big
  refactor" — the prototype agrees.

**What the real refactor would touch**

- `engine/src/sim/sim_thread.cpp` — the master loop becomes the
  Timing Handshake authority (publish windows, stamp phases, commit).
- `engine/src/sim/include/litholattice/sim/clock_domain.h` — domains
  re-express as cadence classes on the phase clock; the integer-divider
  logic retires.
- `engine/src/sim/det_sched.*` (DeterministicScheduler) — dispatch lists
  become shard registrations with cadence + tick-critical flags.
- `engine/src/sim/include/litholattice/sim/tick_interfaces.h` —
  `TickContext` gains snapshot/pending semantics; `ITickable` contract
  changes are the long pole.
- The tick debugger's freeze/step maps naturally onto the authority
  (it already gates the master clock), and its known multi-domain
  stepping defect would need re-verification against phase slots.
- **Mobs/entities: NOT touched.** Per JD's scope boundary (§1), entity
  ticking stays out of the Timing Handshake — *"otherwise they run at
  512 and the bones break."* A **separate, independent mob ticking
  system** is future work and is out of scope for this prototype and
  for the refactor described above; the refactor's blast radius is the
  circuit systems (Trace, Signal) and core sim timing only.

**Open questions handed back to JD**

1. Missed-window policy: wait-forever (current prototype), or is there a
   determinism-preserving deferral you have in mind? Skip is disproven.
2. Hung-component watchdog: stop the world, or something else? This is a
   world-permanence call, not an engineering one.
3. Polling purity: tight spin as dictated, or spin-then-wait hybrid for
   real hardware?
4. Within the circuit systems, which components are tick-critical
   (barrier) vs free-run? That census decides whether the refactor pays.
   (Mobs/entities are already decided: excluded — their independent
   ticking system is a separate design, for another night.)

## 5. Isolation verification

All work was done in the copy at `~/workspace/experiments/pull-tick/`.
The production tree `~/workspace/litholattice` was only ever read.
Manifest check (path + size + mtime for every file, md5 of the sorted
listing), taken before the experiment and after:

- Before: `cdd885cd19372d910ab619a2141e95ba`
- After: `cdd885cd19372d910ab619a2141e95ba` — **identical; production
  tree untouched.**

---

## Decisions on the open questions (2026-10-13 00:12 EDT)

JD delegated the calls on the proposed solutions to Elfie ("let you make the
calls on proposed solutions… partial credits to Elfie, Muse AI, and Meta
team") and confirmed at 12:12. Decided:

1. **Missed-window policy: WAIT.** A component that overruns its window
   extends the phase; the overrun is logged and counted, and repeat
   offenders are flagged for diagnostics. Deadline-skip is REJECTED — the
   prototype proved it diverges from the sequential outcome. Determinism
   is not negotiable for a performance costume.
2. **Hung-component watchdog: FREEZE.** If a phase exceeds a hard ceiling,
   the watchdog freezes the sim clock (tick_freeze semantics) and raises a
   fatal diagnostic naming the component. It never force-advances. A frozen
   world can be saved, inspected, and recovered; a force-advanced world is
   corruption. World permanence outranks frame rate.
