# Audio + Threading Implementation Report — 2026-10-11

Two-phase program (JD approved all; audio first, then threading). Implemented by three sequential workers on `~/workspace/litholattice/engine`. Final verification by coordinator: full build clean (only warning is the pre-existing `world/block.h:90` narrowing), `ctest` 10/13 — the 3 failures are exactly the grandfathered pre-existing set (engine_render golden hash, engine_sim block-names, lithokraft_selftest SIGABRT). **No new failures.**

## Phase A — Audio-path optimizations (all DONE, 44/44 audio tests)

1. **Voice virtualization** — `Voice::advance()` advances envelope/phase/noise/sweep without mixing; `Synth::setVirtualized()` + `AudioCommand::Virtualize`; allocator steals oldest *virtualized* before oldest audible; `noteOn` clears virtualization. Bit-identical resume proven via memcmp.
2. **SIMD mixing** — `simd_mix.h`: SSE2/NEON with scalar fallback, wired into `Synth::mixInto` and `AudioEngine::render`. Bit-exact vs scalar proven.
3. **Occlusion bake cadence** — `occlusion_cadence.h`: re-bake every N ticks OR on listener move >X blocks; defaults reproduce old behavior; existing 24/24 spatial tests untouched.
4. **Effect bypass** — `effects.h/.cpp`: Schroeder reverb as effects-bus insert, bypassed by default; `kEffectBypassDistance = 48.0f` with documented audibility reasoning; `effectBypassed()` policy.
5. **Async streaming** — `streaming.h/.cpp`: `AudioStreamer`, decode only on worker thread, chunks via `HandoffQueue`, 4096-frame chunks, 8-chunk readahead; audio thread never allocates/blocks/decodes.
6. **Graceful degradation** — `degradation.h`: `DegradationPolicy` (0.70/0.90/1.00, escalate after 8, recover after 64) + `LoadShedder`; L1 virtualizes quiet release tails, L2 halves voices, L3 half-rate render (aliasing, not dropout). Default = current behavior under no pressure.

Bugs caught: `<emmintrin.h>` include placement; a wrong test expectation (code was right); ASan caught a heap-buffer-overflow in a test (library ASan-clean).

## Phase B part 1 — Threading/job system (all DONE)

JD's note ("take your time, be diligent, don't cut corners") was relayed mid-flight; worker ran extra review passes and stress iterations. **Two real concurrency bugs found and fixed**: (1) Chase-Lev `popBottom` moved-then-CAS raced with thieves → `bad_function_call`; fixed to copy-then-resolve. (2) Violated single-owner invariant — `submit()` from any thread raced worker `popBottom` → lost jobs / torn reads / segfault; fixed with per-deque owner mutex, steal stays lock-free.

- **Work-stealing job system** (`core/chase_lev.h`, `core/job_system.h`): per-thread Chase-Lev deques, named threads (`litho-wN`, `litho-ioN`, `litho-input`), High/Normal/Low priorities, affinity via `sched_setaffinity` (Linux; no-op elsewhere). `JobPool` kept as-is for simple uses (documented).
- **Deterministic composition**: `det_sched` gained `runTick`/`runTicks` overloads taking `JobSystem&`; stages run behind `JobFence`; 64-tick keyed-RNG world hashes bit-identical between JobPool and JobSystem on 1/2/4/8 workers, debug flag on and off.
- **Hardening**: lock-free deques/SPSC/atomics; documented lock order (L0 lock-free → L1 leaf → L2 control → L3 debug leaf); no locks across job boundaries; `waitIdle()`/`JobFence::wait()` assert-fail from job threads; blocking work → `submitIo()`; RAII threads, no detached threads, disciplined shutdown.
- **Debug logging** (JD requirement): `LITHOLATTICE_THREAD_DEBUG` CMake option, off by default — job lifecycle (submit/steal/start/finish, >50ms slow-job tripwire), lock events, wake/sleep, queue depths, affinity. Disabled → macros expand to `((void)0)`; verified via `nm`: no debug symbols in binary. Tests pass both ways.
- **Input thread** (JD requirement): `InputPump` (`input/input_thread.h/.cpp`) owns `litho-input`, pinned to realtime mask; events flow through one lock-free `SpscQueue<InputEvent,1024>` (input → tick thread); tick drains once per frame, fans out to engine + UI; overflow drops newest with count; manual mode for GLFW main-thread constraint. **Determinism boundary: the drain** — live input non-deterministic, sim sees a fixed event list per tick; `ReplayInputBackend` bypasses the pump and stays the deterministic path. Input never stalls behind sim/render/I/O.
- **Stress tests**: 13 jobsystem/chase-lev + 3 input-pump checks; standalone ASan+UBSan harness (4 clean runs): steal storms, 100× shutdown race with 3000 in-flight jobs, fence rounds, det-sched 1–16 workers, 20000-event pump run; clean with debug flag on too.
- **TSan: not available** — compiles but the runtime can't start in this VM (`FATAL: ThreadSanitizer: unexpected memory mapping`, even for trivial binaries). Compensated with ASan+UBSan, 4× repetitions, line-by-line review of deque/shutdown paths.

## Phase B part 2 — File robustness (all DONE, 70/70 storage tests)

- **Crash-safe file contract** (`storage/file_contract.h/.cpp`): `writeFileAtomic` (temp + fsync + parent-dir fsync + atomic rename; target untouched on failure; temp cleaned); `FileWriteOptions{fsync, keepBackups}` with rotation only on successful commit; `frameRecord`/`unframeRecord` CRC32 framing (`LFRM` magic); `Quarantine` (log-once + count + entries); `readValidatedRecord` distinguishes absent/I-O-failure/corrupt. `RegionFile::flush` migrated onto it; AnvilRegion documented as already conforming.
- **Async I/O** (`storage/async_io.h/.cpp`): `IoScheduler(JobSystem&)` routes via `submitIo()`; `IoFence` mirrors `JobFence`; documented completion contract (callbacks on I/O thread, short/non-blocking, per-path FIFO strands, failures never throw). `WorldStore` gained optional `JobSystem*` backend with strict FIFO preserved.
- **Load-time validation**: unreadable regions quarantined (logged + `quarantinedCount()`), load continues with empty region, damaged file never overwritten.
- **Mod sandboxing** (`storage/mod_sandbox.h`): `ModSandbox(root).resolve()` rejects absolute/drive-letter paths, `..` escapes, NUL bytes, empty paths, symlink escapes (strict prefix containment), symlink loops; TOCTOU boundary documented honestly.

Tests: 44 new checks — torn-write recovery, backup rotation, CRC quarantine, async ordering/completion-on-I/O-thread, throwing callbacks, JobSystem-backed WorldStore round-trip, traversal attacks (nested `..`, absolute, drive-letter, NUL, symlink-out, prefix-sibling, loops).

## Verification (coordinator, first-hand)
- `cmake --build . --target litholattice_tests` — clean; only warning is pre-existing `world/block.h:90` narrowing.
- `./tests/litholattice_tests audio` — 44/44 PASS. `storage` — 70/70 PASS.
- `ctest` — 10/13; failures exactly the 3 grandfathered pre-existing ones. No new failures.
- `sim/trace_jit_basic.cpp` untouched throughout.

## Open questions (consolidated, not guessed)
**Audio:** cadence ownership per listener (engine provides, game owns); streaming codec/asset pipeline for `DecodeFn`; game must map synth voices → world distances for bypass; game should reduce NoteOn rate when `shedLevel() > 0`.
**Threading:** audio-thread pinning wiring is a one-liner left for the audio module; `JobPool` kept (not wrapped) by design; no `IJobExecutor` interface (concrete overloads keep determinism reasoning direct).
**Files:** multi-record atomicity across regions still needs a journal or accepted per-record atomicity (pending save-scheduler design); whether mod opens need `O_NOFOLLOW` + fd re-validation; backup depth for future world-level metadata.
**Naming:** new code follows `litholattice::` / `k`-prefix / existing subsystem names; `-OR` is a game-component convention, not applied to engine infra (documented); no existing violations spotted in touched modules; no silent renames made.
