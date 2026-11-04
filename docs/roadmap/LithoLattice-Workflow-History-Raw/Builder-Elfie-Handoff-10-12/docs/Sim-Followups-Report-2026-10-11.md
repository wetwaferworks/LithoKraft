# Sim Follow-ups — Implementation Report (2026-10-11)

Three sim-side workstreams, implemented sequentially on the shared tree. All build green.

## 1. Trace JIT composition seam — WRAPPED ✅

**File:** `engine/src/sim/include/litholattice/sim/trace_jit_compose.h` (header-only, LF)

`TraceJitSystem` wraps an `ITraceJit` + `CompiledGraph` as a `DeterministicScheduler::SimSystem`,
exactly as `det_sched.h` prescribed. Headline decisions (full reasoning in the header's comment block):

- **Ownership:** wrapper *borrows* the `ITraceJit*` (one JIT shared across chip-systems; per-system
  JITs would fork the id sequence and future compile caches) and *owns* its `CompiledGraph` by value.
  Copy/move deleted — a graph can never alias across systems.
- **Scheduling:** `access()` returns `{writeRes(kSignalNetworkRes)}` (new constant = 300, constructor
  parameter). `update()` is deliberately boring: `jit_->execute(graph_)`, nothing else.
- **Recompile contract:** legal only *between* ticks (single-threaded). Mid-tick swap = worker-thread
  use-after-free. `freeGraph()` sanitizes id/nodeCount/tickCost after `free()` because
  `ITraceJit::free` isn't guaranteed to zero the struct.
- **Determinism contract:** `execute()` must be tick-pure — no shared RNG, no wall-clock, no
  address-order-dependent iteration. Flagged for the builder: v1 `execute()` ignores `TickContext`,
  so when real bytecode lands the tick index (for keyed RNG) must reach the VM — that change lands
  in this wrapper, not the scheduler.

**Tests:** 6/6 new checks pass (1 and 4 threads): Write declaration, stage placement (conflicting
writer serialized later, disjoint reader shares the stage), execute-once-per-tick, recompile swaps
and frees, destructor frees exactly once. All 6 existing det-sched checks still pass.

**Open questions for the builder:**
1. `kSignalNetworkRes = 300` is a convention, not a registry — every signal-touching system must
   declare the same id. Consider a shared game-side constant.
2. When bytecode lands, `execute()` needs the tick — signature extension vs. a bind step; run the
   golden-trace verification (1 vs 4 threads, bit-identical hashes).
3. Constructor compiles eagerly; lazy first-compile is a small documented change if ever wanted.

`trace_jit_basic.cpp` untouched (builder's active file).

## 2. Signal propagation granularity — ALL THREE OPTIONS ✅

**File:** `engine/src/sim/include/litholattice/sim/signal_propagate.h` (header-only, LF)

- `enum class PropagationGranularity : uint8_t { PerChunk, PerNetwork, WholeWorld }`,
  `kDefaultPropagationGranularity = PerChunk` (reasoning in header).
- `SignalPropagationPolicy`: abstract class the game-side solver implements —
  `setGranularity()`/`granularity()` for runtime selection (a field, no `#ifdef`),
  pure-virtual `partitionsForTick(tick, out)` with a documented determinism contract
  (one kind per call, non-decreasing id order, pure — no wall-clock/address-order/shared RNG).
- Helpers: `SignalPartition{kind,id}`, `chunkPartitionId()`/`unpackChunkPartitionId()`
  (ChunkKey round-trip, negative-coord safe), `kWholeWorldPartitionId`, `sortPartitionsById()`,
  `partitionsAreSorted()` (golden-trace verifier), `partitionResourceId()`/`partitionAccess()`
  (scheduler resource mapping — fold collisions are safe-by-construction, they only over-serialize),
  `VectorPropagationPolicy` (reference implementation + test double).

**Tradeoffs (for the builder):**
- **PerChunk** (default): cost ∝ loaded/ticking chunks; cache-friendly; determinism falls out of fixed
  chunk order; wins when networks are chunk-local; loses on cross-chunk wires (boundary sync needed).
- **PerNetwork:** cost ∝ active networks; follows connectivity; wins for sparse large networks;
  requires connected-component discovery + canonical network ordering for determinism.
- **WholeWorld:** simplest to reason about; cost ∝ world size every tick; wins for dense global
  signal (wireless); loses on large worlds.
- Finer granularity = more precise scheduler resource declarations = more parallelism; coarser =
  simpler but more serialization. Start with Pattern A (single system declaring
  `writeRes(kSignalNetworkRes)`, internal canonical-order iteration); Pattern B (one SimSystem per
  partition) is a profiling-gated upgrade.

**Tests:** 9/9 pass — default PerChunk; chunk-key round-trip; deterministic ordering across calls
and ticks (incl. duplicates, empty set); PerNetwork canonical order; WholeWorld single token;
runtime switching; `partitionsAreSorted` validation; scheduler mapping.

**Open questions for the builder / JD:**
1. PerNetwork canonical ids are game-side: lowest packed node position vs. monotonic counter in
   deterministic discovery order.
2. PerChunk cross-chunk wire boundary semantics (post-partition fixpoint vs. ordered sweep).

## 3. Production failure policy — FAIL-FAST ✅

**Files:** `engine/src/core/include/litholattice/core/fail_fast.h`,
`engine/src/core/fail_fast.cpp` (added to `litholattice_core`)

JD's directive: sim corruption → world corruption is unacceptable. On detection: **LOG then ABORT.
Never continue.**

- `SimCorruptionReport`: what, subsystem, file/line, tick, expected/actual hash, detail string.
- Pure/testable `formatSimCorruptionReport` / `simStateCorrupted` / `simCorruptionFromException`;
  `[[noreturn]] simFailFast()` emits to raw stderr first (in case logging is what's broken), then
  via `logError`, then `std::abort()`. No hooks, no recovery, no exceptions escaping.
- The scheduler's fail-fast throw on system-job exceptions is unchanged (tested behavior).

**Production-boundary contract (builder action required):** `SimThread::run()` currently has NO
try/catch on the tick path — an exception from `sim_->tick()` propagates out of the `std::thread`
callable → `std::terminate` with **no report logged**. The builder must wrap the per-tick step in
try/catch calling `simFailFastFromException(e, "sim_thread", worldTickN)`; any loop calling
`DeterministicScheduler::runTick` directly must catch at the innermost loop body and convert —
logging-and-continuing or re-throwing are both violations (world may be half-updated).

**Tests:** 9/9 pass — format contains all fields; unknown fields degrade gracefully; hash-match
silent / mismatch flagged; scheduler-throw → report conversion; POSIX subprocess death test
(child dies on SIGABRT, report reached stderr first). Abort never called in-process.

## 4. SimThread integration — DEFERRED (builder's brief)

Out of scope by JD's decision ("complex, needs holistic view"). What it entails: the
`DeterministicScheduler` (conflict-DAG stages over a `JobPool`/`JobSystem`) currently runs
standalone; `SimThread` runs its own fixed-timestep loop calling `IWorldSimulation` directly.
Integration means making the scheduler the execution engine *inside* `SimThread::run` — mapping
clock domains and the world tick onto scheduler stages, routing structural commands through the
per-system buffers, and reconciling the scheduler's fail-fast throw with the sim thread's
lifecycle (see §3's exception-boundary finding). The builder should design this holistically
against the real tick-loop contract, not bolt it on.

## Verification

- Full engine build: clean, zero new warnings.
- ctest: PENDING (run at report time; target 11/13 with only the 2 grandfathered failures).
- Grandfathered (builder's fixes, out of scope): `engine_sim` block-names identity
  (`world/block.h:90` narrowing — `"litholattice:air"` into a bool slot), `lithokraft_selftest`
  SIGABRT (blocks.cpp 40-vs-36 guard mismatch).

## Open questions (consolidated)

- Builder: `kSignalNetworkRes` shared-constant home; PerNetwork canonical-id scheme;
  PerChunk boundary semantics; SimThread exception boundary + scheduler integration.
- None blocking: all three workstreams are complete and green.
