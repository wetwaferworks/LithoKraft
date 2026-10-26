# Thread Handshake — Implementation Notes

**Status:** pre-implementation notes, 2026-10-26. Companion to Elfie's design overview, "The Thread Handshake — Design Overview".
**Build:** Wednesday 2026-10-28 after the usage reset, in a separate test tree first.
**Credits:** Design: JD (dictation, vision, direction). Documentation: Elfie. Implementation: Cortana (planned).

## 1. Two kinds of threads (JD, 2026-10-26 6:41 PM ET)

**Dedicated threads**
- At engine boot, the Thread Handshake assigns specific threads to specific hardware tasks. Initial assignments:
  - Graphics / software rasterizer (the render workers)
  - Timing Handshake (the engine's sim clock)
- These spawn globally when the engine boots and despawn when it powers down. They are never returned mid-run.
- Each one still gets a full ThreadRecord. Its spawn is stamped at boot and its return at shutdown, with the reason "engine power-down".

**Pooled threads**
- Pooled threads carry demand-driven jobs: chunk meshing, background jobs and other "thread needed" calls.
- They go through the dispatch pool's POOLED → … → RECLAIMED cycle.

## 2. Spawn means lending, not OS creation

- Threads are created once (at boot, from the hardware census) and stay alive in the pool.
- In the protocol, "spawn" and "despawn" are stamped hand-offs between the pool and a job. They do not create or destroy OS threads.
- Reason: creating an OS thread costs tens of microseconds. At 512 TPS, a tick lasts about 1.95 ms, so doing that per job would eat the budget.
- The record is the same either way.

## 3. Ledger stamped in ticks, written in stream order

- `spawnTick` and `returnTick` are Timing Handshake tick numbers, never wall-clock time. Wall-clock times can go in a separate debug field that is excluded from hashes.
- Ledger entries are committed in the stream's order: (tick, then sequence number assigned at dispatch). The order in which threads happen to finish does not matter.
- Workers may finish in any order. Their results are merged in that fixed order before the tick's hash receipt.
- This is what lets the fairness gate pass: the hashes are identical at 2 or 16 threads. Without it, timing noise alone would change the ledger.

## 4. Fairness gate and tests

- Run the scenario-hash anchors with the pool forced to 2, 4, 8 and 16 threads. All must match: cb32c824f8b3ea98 (cities on) and e015e51cf1559089 (cities off).
- Test the dedicated-thread lifecycle: boot spawns them, shutdown returns them, and nothing leaks.
- Measure ledger overhead per tick, and keep it off the hot path.

## 5. Existing pieces to fold in

- Item 4's adaptive background cap (`01b5a12`): it caps background jobs at 1 thread while moving and at 2–3 while standing still. That policy moves into the dispatch pool.
- `LL_SW_RENDER_THREADS` and Archie's item 3 thread-count question: the dedicated render-thread count is decided by the handshake from the hardware census.
- Respect the low-end tiers: on 4 or fewer cores, dedicated threads come first and the pool shrinks.
