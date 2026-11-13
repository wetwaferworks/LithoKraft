# Engine Audio ctest Flakiness — Builder Report (2026-10-12)

**Status:** researched, not fixed (per JD: leave the code alone until the game runs).
**Scope:** `engine_audio` ctest entry = `litholattice_tests audio`
(`engine/tests/CMakeLists.txt:10`, no `RUN_SERIAL`, no timeout properties anywhere).

## 1. Observed behavior

- Full `ctest -R engine_`: 22/23; `engine_audio` flakes under parallel load, passes solo.
- **Reproduced here, 2026-10-12, on this 2-core VM:** 3 failures in 8 solo runs of
  `./build-linux/tests/litholattice_tests audio`. So it is not purely a parallel-ctest
  artifact — it fails solo whenever the box is busy (and this box is shared).
- The failing check, every time:
  `[FAIL] streaming: worker-thread decode, double-buffered handoff, 440 Hz intact, clean EOS`
  (`engine/src/audio/audio_selftest.cpp:624`).

## 2. Root-cause notes per test

### 2a. CONFIRMED — streaming consumer/producer race (`audio_selftest.cpp:579-626`)

The test (`// (5) Async streaming`):

```cpp
constexpr size_t kTotal = 48000;
AudioStreamer streamer(4096, 8);
streamer.play(/* decode lambda: 440 Hz sine, kTotal frames */);
size_t validTotal = 0;
for (int i = 0; i < 1000 && !streamer.finished(); ++i) {   // :604
  std::vector<float> chunk(8192 * 2, 0.f);                 // 65 KB alloc+zero per iter
  const size_t valid = streamer.render(chunk.data(), 8192);
  validTotal += valid;
  ...
}
check(allOnWorker && neverOnAudioThread && validTotal == kTotal && streamer.finished() && ...);
```

Why it breaks: `AudioStreamer::render` **never blocks** — it drains whatever chunks are
available and returns (`engine/src/audio/streaming.cpp:74-104`). The consumer has a fixed
budget of 1000 iterations (~10–30 ms of empty pulls). The worker must decode all 48000
frames (12 chunks) inside that window. On an oversubscribed 2-core box the worker thread
can go unscheduled for tens of ms → the consumer burns its 1000 iterations on empty pulls
→ `validTotal != kTotal`, `!streamer.finished()` → FAIL. The hot spin makes it worse: the
consumer burns a core spinning while the worker is the thread that needs the core.

The test assumes "the worker always makes 48000 frames of progress within ~10–30 ms of
consumer spinning." That assumption is false under load.

### 2b. CONFIRMED MECHANISM (not yet observed failing) — wall-clock shed assertion (`audio_selftest.cpp:655-662`)

```cpp
AudioEngine eng(kRate);
std::vector<float> st(2048 * 2, 0.f);
eng.render(st.data(), 2048, 2);                                        // :660
check(eng.shedLevel() == 0 && eng.lastAudioLoad() < 0.7,               // :661
      "degradation: no pressure -> level 0, behavior unchanged");
```

`AudioEngine::render` wraps the whole render in `steady_clock::now()` and feeds
`elapsed / budget` into the shedder (`engine/src/audio/audio_engine.cpp:99,133-137`;
`LoadShedder::update` stores it as `lastLoad_`, `degradation.h:54`). Budget for 2048
frames @ 48 kHz = 42.67 ms; the check demands load < 0.7, i.e. the render must complete
in under ~30 ms of *wall* time. One >30 ms deschedule — page fault, cold cache, another
ctest suite's thread burst — and this check fails, even though the DSP work is microseconds.

The `shedLevel() == 0` half is deterministic and safe: default `escalateAfter = 8`
(`degradation.h:37`), so a single slow callback can never escalate. Only the `< 0.7`
half is load-sensitive.

### 2c. ANALYZED — not a failure source: occlusion 120 ms sleep (`audio_selftest.cpp:961`)

```cpp
std::this_thread::sleep_for(std::chrono::milliseconds(120));   // "let the thread tick once"
check(oc.gain(q) > 0.99f, "occlusion: clear path keeps gain at 1.0");
```

The stub occlusion fn returns false (clear), so the thread lerps gain toward 1.0 from an
initial 1.0 (`occlusion_thread.cpp`: `smoothed = current + (target - current) * 0.5f`).
Gain is *exactly* 1.0 whether the thread ticks zero times or a hundred — it cannot dip
below 0.99 on this path. The sleep length is irrelevant to the outcome. **Do not chase
this one** if the failure signature mentions occlusion; the observed failures are all §2a.

### 2d. Not flaky, just slow: SPSC 200k spin (`audio_selftest.cpp:140-145`)

Producer/consumer spin on `tryPush`/`tryPop` with no timeout — logically cannot fail,
only gets slow under oversubscription. Leave it.

## 3. Proposed implementations (apply directly)

### Fix 1 — streaming test: deadline instead of iteration budget (the real fix)

`audio_selftest.cpp`, the `(5)` block (~:601-626):

```cpp
size_t validTotal = 0;
std::vector<float> chunk(8192 * 2, 0.f);              // hoisted: no per-iter alloc
const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
bool timedOut = false;
while (!streamer.finished()) {
  if (std::chrono::steady_clock::now() >= deadline) { timedOut = true; break; }
  const size_t valid = streamer.render(chunk.data(), 8192);
  validTotal += valid;
  for (size_t f = 0; f < valid && left.size() < kTotal; ++f) left.push_back(chunk[f * 2]);
  if (valid == 0) std::this_thread::sleep_for(std::chrono::milliseconds(1)); // yield to worker
}
```
and add `&& !timedOut` to the check at :624. Notes:
- The 1 ms backoff on empty pulls is load-bearing: it stops the consumer from
  starving the worker on 2-core machines (the current hot spin actively contributes
  to the flake).
- Even better (optional): give `AudioStreamer` a `waitForFinished(timeout)` backed by a
  `condition_variable` notified where `finished_` is set (`streaming.cpp:112`), and have
  the test block on it instead of polling. Larger diff; the deadline loop is sufficient.

### Fix 2 — shed check: stop asserting on wall-clock measurements

`audio_selftest.cpp:660-662` → split the deterministic half from the reporting half:

```cpp
eng.render(st.data(), 2048, 2);
check(eng.shedLevel() == 0,
      "degradation: no pressure -> level 0, behavior unchanged");
// Load *reporting* verified deterministically via injected durations
// (same pattern as the shedder block above):
eng.updateShed(std::chrono::nanoseconds(1'000'000), std::chrono::nanoseconds(10'000'000));
check(eng.lastAudioLoad() < 0.7,
      "degradation: lastAudioLoad follows injected durations");
```

This mirrors the deterministic `updateShed(ns, budget)` pattern the test already uses
for the shedder itself (:628-650). `shedLevel() == 0` after one render is provably stable
(`escalateAfter = 8`, `degradation.h:37`).

### Fix 3 — ctest hygiene: serialize the audio suite

`engine/tests/CMakeLists.txt`, after the `foreach` loop:

```cmake
set_tests_properties(engine_audio PROPERTIES RUN_SERIAL TRUE)
```

One line. Removes the parallel-ctest trigger (sibling suites' thread bursts). This is
defense in depth — it does **not** replace Fix 1, since §2a fails solo on busy boxes too.

### Fix 4 — optional, larger: virtual clock injection (recommend against for now)

Add an overridable `now()` hook to `AudioEngine` (default `steady_clock::now`) so tests
control callback timing end to end. Sketch only — Fix 2 covers the test without touching
production code, and the shedder itself is already fully deterministic by design
(`degradation.h`: "Deterministic and allocation-free"). Revisit only if new
time-dependent engine behavior needs testing.

## 4. What genuinely needs the live game

These cannot be diagnosed or validated headless; they are the reason JD parked this:

- **Shedder thresholds under real callback pressure.** The selftest feeds synthetic
  durations; whether warn 0.70 / critical 0.90 / overload 1.00 actually prevent dropouts
  (vs. false-triggering) needs a real miniaudio device callback under game load.
- **miniaudio device interaction.** The selftest header says it outright:
  "Never opens a device" (`audio_selftest.cpp:1`). Device open, format negotiation,
  callback jitter, underrun behavior — all untested until the game runs.
- **Occlusion thread against a real voxel world.** The test installs a stub returning
  `false`; real raycast cost per tick, the 32→16→8 Hz graceful-degradation ladder
  (`occlusion_thread.cpp`, `kMissThreshold = 64`), and gain smoothing audibility need
  the game.
- **Real decoders.** FLAC/Opus/Vorbis are header-parse stubs ("frames stubbed",
  :769-816); streaming against real compressed audio is untested.
- **The 120 ms occlusion sleep** at real 32 Hz under game thread load (see §2c — not a
  current failure, but its margin was never validated against a loaded schedule).

## 5. Suggested builder order

1. Apply Fix 1 + Fix 2 (test-only, zero production risk), re-run the audio suite in a
   loop (`for i in $(seq 1 20)`) to confirm the flake is gone.
2. Apply Fix 3.
3. When the game runs: validate §4 items, starting with device-callback behavior and
   shedder thresholds under real load.
