# Safety watchlist (LithoLattice)

A living list of failure patterns that real-world robots built on LithoLattice must
withstand. Example (JD): a safety robot lifting 100 lb columns. If one of its jobs
silently fails to run, a person can be hurt. Add an entry whenever a review finds a
pattern like this. Each entry names the pattern, why it is dangerous, how it was found,
the fix, the rule, how to test for it, and its status.

## 1. Lost wakeup: a job that is never dispatched

- **Status:** Documented. Fixed in the engine on 2026-10-23. The proposed deadline watchdog is not implemented yet.
- **Pattern:** a thread changes the state a condition variable waits on, then notifies
  without taking that condition variable's mutex. A waiter that has just checked the
  predicate (under the mutex) and found nothing to do, but has not yet blocked, misses
  the notify. It then sleeps forever even though work is queued or shutdown was requested.
- **Why it is dangerous:** it is silent. There is no crash, no error, and no CPU spike.
  Input, rendering and the rest of the engine keep running and look healthy, but the
  submitted job never runs. On a robot, a time-critical action (moving an arm to catch a
  falling object, stopping a lift) would simply never happen.
- **How it was found:** issue B8 in
  [CODE_REVIEW_2026-10-23_Cortana.md](../reviews/CODE_REVIEW_2026-10-23_Cortana.md).
  engine_sim hung intermittently at about 0 % CPU. A build with the
  `LITHOLATTICE_THREAD_DEBUG` flag on was looped until it hung (run 165), then an
  all-thread stack dump (DbgHelp, `stackdump.exe`) showed the main thread in `join()`
  inside `JobSystem::~JobSystem` and a worker asleep in `cv_.wait` in
  `JobSystem::workerLoop`. The tick timing handshake was investigated and ruled out.
- **Fix and where it lives:** `engine/src/core/include/litholattice/core/job_system.h`.
  - `JobSystem::submit()` takes an empty `std::lock_guard` on `m_` between the
    `workGen_` increment and `cv_.notify_one()`.
  - `JobSystem::shutdown()` does the same on `m_` and `ioM_` before the `notify_all` calls.
  - Both sites carry a comment explaining why the empty locks are required.
- **Rule to follow:** never notify a condition variable after changing its predicate
  without taking that condition variable's mutex in between. Either change the predicate
  under the lock, or take an empty lock before the notify. Otherwise use timed or
  non-blocking waits: the tick timing handshake uses `wait_for(1ms)` with a predicate and
  never blocks indefinitely on a mutex or condition variable, so it cannot hang this way.
- **How to test for it:**
  - Loop the affected tests hundreds of times (200+ runs, in Release and under ASan)
    and require 0 hangs.
  - Diagnose with a `-DLITHOLATTICE_THREAD_DEBUG=ON` build. Its watchdog dumps the
    per-thread `LITHO_THREAD_LOG` rings after 10 s in waitIdle/fence. When a hang is
    outside a fence (for example in a join), capture all thread stacks with a debugger
    or `stackdump.exe <pid>`.
- **Proposed follow-up (not implemented):** a deadline/liveness watchdog for critical
  jobs. A job submitted with a deadline that is not picked up by a worker within that
  deadline raises a critical log (and a counter), so a stalled dispatch can never be silent.

## Requirements

### R1. Black Box Mystery Solved (JD): a replayable, tamper-evident decision log

- **Status:** Proposed, not implemented.
- **Requirement:**
  - **Record everything:** every input and sensor sample is recorded with the tick number it was consumed on.
  - **Cover all in-engine AI (JD t2342u):** the log also covers NPC/mob AI, the AI framework, and any AI agents acting through the engine, not just robot sensor input. Every AI decision is logged with its inputs and its tick number, and it is replayable. Reason: the in-engine AI is meant to carry over to real robot bodies.
  - **Replay every decision:** deterministic replay of that record reproduces every decision tick by tick. "Why did the robot do this? Roll back the disc and find out."
  - **Make it tamper-evident:** entries are hash-chained, each one including the hash of the one before. Any edit, deletion or reordering breaks the chain, so the record can stand up as evidence in court.
- **Existing building blocks:**
  - In PCH (PersonalControlHarness): the rewind/fork testbeds (`rewind_fidelity`, `fork_budget` in `testbeds/tb_model_testbeds.py`) show tick-exact rewind and branching, and the drift check (`tools/check_upstream_drift.py`) shows content-hash verification.
  - In the engine: deterministic ticking and the demo record/replay path (`idemo_record.h`, `--demo-test`).
- **Still needed:**
  - a record format covering all inputs and sensor samples;
  - the hash chain and a verifier;
  - a replay that compares each decision against the log;
  - a guarantee that recording cannot drop samples silently (see entry 1).

### R2. Physical interaction limits

- **Status:** Proposed, not implemented.
- **Requirement:** actuator force and grip limits are enforced below the AI layer, in the actuator/control path that no AI decision can bypass. No AI decision can command more than a safe force near people. JD's example: shaking a human's hand gently.
- **Still needed:**
  - per-actuator force, torque and grip limit tables, with stricter limits when a person is near;
  - enforcement and clamping in the layer under the AI;
  - a critical log entry (recorded in the R1 decision log) whenever a command is clamped;
  - tests that try to exceed the limits.
