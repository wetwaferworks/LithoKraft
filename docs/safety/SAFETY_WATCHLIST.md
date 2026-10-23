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

## 2. Completion waits with no deadline

- **Status:** Documented (review R-SAFE-2). Not fixed.
- **Pattern:** a caller blocks on "all my jobs are done" (a fence, waitIdle, batch wait, flush or join) with no deadline.
- **Why it is dangerous:** one stuck or never-dispatched job freezes the caller silently, and the rest of the system looks alive.
- **Rule:** every cross-thread completion wait in a safety path either has a deadline that raises a critical log, or is covered by the liveness watchdog.
- **Test:** inject a job that never completes and require a critical log within the deadline.

## 3. Unbounded retry on a persistent failure

- **Status:** Documented (R-SAFE-1). Not fixed.
- **Pattern:** "log, back off, retry" with no cap.
- **Why it is dangerous:** if the failure is deterministic, the work never happens and the thread livelocks, and the only signal is a log flood.
- **Rule:** cap retries, then escalate (critical log, fail the job so its waiters complete).
- **Test:** inject a failure that always repeats and require a bounded outcome.

## 4. Silent containment

- **Status:** Documented (R-SAFE-5/6/7). Not fixed.
- **Pattern:** a `catch (...)` that removes an agent, drops a command or uses a default value without logging.
- **Why it is dangerous:** containment is right, but silence hides it. A robot behaviour that throws simply stops existing.
- **Rule:** every containment logs at least once, naming the component, and counts the event.
- **Test:** throw from each contained boundary and require the log line.

## 5. Waiting for global quiescence instead of your own result

- **Status:** Documented (R-SAFE-4). Not fixed.
- **Pattern:** a synchronous call waits for "the whole queue is empty" rather than for its own request.
- **Why it is dangerous:** steady background work starves it for an unbounded time, and a call from inside the pool it waits on deadlocks.
- **Rule:** wait on a per-request completion, and never block a pool thread on that same pool.
- **Test:** run a synchronous call under continuous background posting and require it to finish within a bound.

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
