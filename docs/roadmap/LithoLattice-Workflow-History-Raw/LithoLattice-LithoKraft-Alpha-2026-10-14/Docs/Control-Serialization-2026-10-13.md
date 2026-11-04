# Control-Event Serialization — Timing Handshake × Control System Coupling

**Date:** 2026-10-13 · **Workstream:** JD's design of 2026-10-13 12:44–12:46 EDT, implemented the same day
**Tree:** experiment copy `~/workspace/experiments/control-serialization/` (production untouched — verified byte-identical against `~/workspace/fix-program-2026-10-13/manifest-final.md5` at the end of the run)
**Status:** complete in the experiment tree, all gates green; production port instructions in §9.

JD's spec, verbatim essentials: *"Hook the control system up just like the timing handshake, and process it before and after the timing handshake. This means key press events and stuff like that is in line, in line and serialized with the timing handshake and very very responsive. But it doesn't need the orders of magnitude — it just needs the single power of two."* And the follow-up: this serialization also enables demo systems and high-fidelity recordings; the demo interface **IDemoRecord** is the next build on top of this layer. This report delivers the serialization layer IDemoRecord will consume — not IDemoRecord itself.

---

## 1. Design as built

### 1.1 The gap it closes

Before this change, SimThread quantized **edits only**: `edit()` pushed an `EditCmd` onto a handoff queue; the sim thread drained it once per loop iteration, stamped it with the then-current world tick, and applied it at that tick's head. Two sim-affecting inputs bypassed even that discipline:

- **The aim ray** lived in a triple-buffer slot read fresh at application time. A Break/Place resolved against whatever aim the frame thread had most recently written — so the *same* edit stream could resolve differently across frame rates, and no record of the aim existed.
- **The player chunk** (which defines the `TickArea` — which chunks tick at all) was a bare atomic the frame thread could rewrite *mid-burst*, between two world ticks of one catch-up burst.

And there was no sequence numbering and no record: nothing a demo system could have recorded.

### 1.2 The stream (`engine/src/sim/control_stream.h/.cpp`, new)

One serialized stream now carries every sim-affecting control event. Types, in the engine's I-interface spirit:

- **`ControlEvent`** — `{seq, tick, source, sourceId, kind, payload}`. Kinds: `Edit` (an `EditCmd`), `Ray` (a `RayInput`), `PlayerChunk` (cx/cz), `Locomotion` (wish direction + pose flags, mirroring `LocomotionInput`), `ControlFrame` (mirrors `mcp::IPlayerController::ControlFrame` field-for-field; the sim library does not depend on the mcp library, the adapter is game-side).
- **`ControlSource`** — `Local`, `McpPilot`, `Script`, `Replay`, `Test`. Recorded verbatim so a demo record shows every source that drove the world.
- **`ControlStream`** — the sequencer + pending buffer + accounting + recording tap, owned by SimThread, exposed as `SimThread::controlStream()` (the `handshakeAuthority()` pattern).

**The stamp domain** is the sim's own accounting: `tick` is the world-tick index (`worldTickN_`, the same counter keyed random streams and edit stamps already use — in handshake mode it advances only at committed barriers, in legacy mode it is the plain tick counter). **The gear is the world tick itself** — JD's single power of two (world TPS is already restricted to 32/64/128/256/512). No sub-tick rates, no domain ladder for control.

### 1.3 PRE / POST phases (the coupling, literally)

Per loop iteration, inside the sim thread's mutation window:

1. **PRE** — `ControlStream::prePhase(worldTickN_)`: drain every producer's submissions (the inbound queue) plus any pre-stamped injections; stamp live events `(seq = nextSeq_++, tick = worldTickN_)`, in drain order — the sim thread is the *single sequencing point*; merge into the pending buffer, kept sorted by `(tick, seq)`.
2. The handshake's **group-2 (clock-domain) phase** runs for the master step, as before.
3. **POST** — `SimThread::applyControlEvents(worldTickN_)` at the world-tick step head: extract every event with `tick <= worldTickN_` in `(tick, seq)` order and apply: Ray events move the **stamped aim**, PlayerChunk events move the **tick area**, Edit events run the (unchanged) edit application against the stamped aim — a re-cast follows any world-changing edit and any Ray event, exactly mirroring the old loop's cast discipline, now per-event so stream order *between* a ray move and an edit is honored exactly.
4. `IWorldSimulation::tick` runs — inside it, the handshake's **group-1 (Signal) phases** fire (`signal_world.cpp`).

So control is processed **before** the handshake cycle (PRE/stamp) and **after** its domain phase, before the tick commits (POST/apply) — JD's bracket, on both modes: in legacy mode the same PRE/POST bracket wraps the plain tick.

Application discipline is the handshake's language: **exhaustive, exactly-once, in stamp order**. Nothing is ever silently dropped — there is no code path that discards an event; an event extracted after its stamp tick (only possible for past-stamped injections, or extraction thresholds above the stamp) is applied and counted in `lateApplied`. Sequence/order regressions are counted in `orderViolations`. Both counters, plus `dropped` (0 by construction — the field exists so tests and IDemoRecord can *assert* it), are in `ControlStreamStats`.

**Paused path correction (one deliberate behavior tightening):** paused application used to pass a sentinel meaning "apply everything now." Under serialization, paused application is stamp-driven: live events stamped at drain carry the current tick and are due immediately — identical behavior for live input — while a *replayed* event stamped in the future now correctly waits for its tick instead of landing early. No existing flow could produce future stamps before this change, so nothing regressed; replay correctness required it.

### 1.4 Presentation vs sim (the alpha-presentation doctrine)

- **Presentation-only consumers keep reading raw:** the HUD look-at raycast still reads the live ray triple-buffer every iteration (latest wins, never serialized). Camera look smoothing and the locomotion body's game-thread stepping are unchanged.
- **Sim-affecting consumers read only stamped state:** edit resolution uses the stamped ray; the tick area uses the stamped player chunk. The player position that *produces* player-chunk updates is game-side (the locomotion body steps on the game thread against `SimThread::getBlock`, as before); what crosses into the sim — the chunk — is now a stream event.
- **Locomotion / ControlFrame events** have no sim-thread consumer by design (locomotion steps game-side; pilot frames merge into the game's InputSystem upstream of this boundary). They are first-class stream kinds *now* so the record format is complete: when IDemoRecord records a session it records these at their game-side tap, and the sim-boundary kinds (Edit/Ray/PlayerChunk) at this one. In this build they are produced by adapters/tests, carried, accounted, and extracted in order — never silently dropped.

---

## 2. The stream format (demo-record file format, v1)

Binary, little-endian, **fail-closed on parse** (bad magic, unsupported version, unknown kind/source/edit-kind, truncated or trailing bytes, or a checksum mismatch refuse the *whole* parse — no partial event list; the tree's storage doctrine).

**Header (48 bytes):** magic `LLCS` · u16 format version (=1) · u16 header length · u64 **world seed** · u8 **mode** (0 legacy / 1 handshake) · u16 **tick rate** (the single gear) · u16 handshake phases per cycle · u64 declared ticks · u64 declared events (both advisory; 0 = unknown/live).

**Record:** u64 seq · u64 tick · u8 kind · u8 source · u16 sourceId · payload by kind:

| Kind | Payload |
|---|---|
| Edit | u8 edit-kind, u8 facing, u16 block, i32 x/y/z, then the command block: u16 code, i32 x/y/z/a/b (38 bytes) |
| Ray | 6 × f32 (origin, direction) |
| PlayerChunk | i32 cx, i32 cz |
| Locomotion | f32 wishX, f32 wishZ, u8 flags, 3 pad |
| ControlFrame | f32 moveForward/moveStrafe/lookDx/lookDy, u8 flags, i8 hotbar slot, 2 pad |

Trailer: u64 FNV-1a checksum over all preceding bytes (the same hash family as the outcome hash). A one-line text form (`controlEventToText`) exists for inspection/logs: `seq=1 tick=120 src=mcp-pilot#2 kind=edit edit=use cell=(24,65,-30)`.

---

## 3. Determinism proof

New harness (`game/LithoKraft/src/app/control_stream_test.cpp`, `--control-test` / `--control-scenario`), driving the **real SimThread** (paused + stepped — the stream, not the wall clock, decides) on the §6.4 circuit-scene world (`buildCircuitScene(…, 120)`, wafer (4,4) r=8, 512 TPS, 2,048 world ticks). The scripted stream expresses the §6.4 inputs as stamped control events — including the two the old harness could not express through the sim at all:

- tick 0: `PlayerChunk(4,4)` (the fixture event every arm shares)
- tick 100: `Ray` (aim down at the first red trace) → `Edit Break`
- tick 120: `Edit Use` on the first repeater (explicit cell)
- tick 150: `Ray` again → `Edit Place` (the trace restored *through ray resolution*)

| Arm | Hash | Applied | Dropped | Violations |
|---|---|---|---|---|
| No-input baseline (fixture only) | `2a2d19a416a1826e` | 1 | 0 | 0 |
| Script, legacy | `5041cf1795c33a8c` | 6 | 0 | 0 |
| Script, legacy (repeat) | `5041cf1795c33a8c` | 6 | 0 | 0 |
| Script, handshake, 2 workers | `5041cf1795c33a8c` | 6 | 0 | 0 |
| Script, handshake, 4 workers | `5041cf1795c33a8c` | 6 | 0 | 0 |

The scripted world differs from the no-input baseline (inputs changed the world), the tick-150 Place verifiably restored the trace cell (`wireAfter == kRedTrace`), and the recording tap reproduced the exact stamped stream that drove each arm. Note the absolute values differ from the §6.4 harness's `faec0f…`: that harness bypasses SimThread (no random-tick machinery); this one drives the full SimThread pipeline. The contract — *same stream ⇒ same world, across runs and modes* — holds bit-exactly.

**The standing scenario is untouched:** `--handshake-scenario 2048` in both modes still prints `faec0f02700d98c6` — it has no control inputs, and this layer is invisible to it.

**Soak:** 20,000 stepped world ticks, a stamped event every 25 ticks cycling Command / Ray / Use(repeater) / PlayerChunk (800 events): all 800 applied exactly once, `dropped=0`, `orderViolations=0`, nothing left pending, and the recording tap's 800 events match the script verbatim end to end.

---

## 4. Responsiveness (input-to-application latency)

Harness: `--control-latency` — Command edits carrying a sequence index, the fixture simulation stamps application on the sim thread; 512 TPS (1 tick = 1.953 ms). The **old path** was measured with the identical harness against the unmodified SimThread (Phase A build) before the rewiring; the **new path** after. Patterns: *paced* (one submit every ~2 ms, n=300) and *burst* (64 submits at once).

| Path | Pattern | p50 | p99 | max |
|---|---|---|---|---|
| Old | paced | 0.99 ms (0.51 ticks) | 2.07 ms (1.06 ticks) | 3.73 ms (1.91 ticks) |
| Old | burst | 1.21 ms (0.62 ticks) | 1.22 ms | 1.22 ms (0.62 ticks) |
| **New** | paced | **0.99–1.05 ms (0.51–0.54 ticks)** | **1.94–2.57 ms (0.99–1.32 ticks)** | **1.95–3.03 ms (1.00–1.55 ticks)** |
| **New** | burst | **0.49–0.95 ms (0.25–0.49 ticks)** | **0.51–0.97 ms** | **0.51–0.97 ms (0.26–0.50 ticks)** |

(Ranges span two runs each. A third old-path paced run under load showed p99 4.80 ms / max 8.00 ms — 2-core VM jitter; the new path's runs were taken under comparable conditions.)

Reading: serialization costs **nothing measurable** — the distributions are the same family, because the loop structure is unchanged (the old path already drained once per iteration). What changed is the *bound's meaning*: application now happens at a named phase boundary with a sequence stamp, so worst-case latency is one tick plus one iteration by construction rather than by habit — and every application's position in the order is recorded. JD's "very very responsive" holds: median application lands within ~½ tick of submission; a 64-event burst fully applies within a single tick.

---

## 5. Adapter inventory — what routes through the stream now

| Source | Path | Stream crossing |
|---|---|---|
| Keyboard / mouse / gamepad (local player) | `InputSystem` (frame pump) → `Game::handleWorldActions` (game.cpp:1340–1360) → `SimThread::edit()` | **Edit** events (source Local) |
| Aim ray (local player) | `Game::update` → `SimThread::setRay()` (game.cpp:1689, called *before* `handleWorldActions` in the same frame — so a press and its aim are adjacent in stream order) | **Ray** events + the raw presentation slot (HUD) |
| Player position → tick area | `Game::update` → `SimThread::setPlayerChunk()` (game.cpp:1684) | **PlayerChunk** events |
| MCP pilots (`IPlayerController`) | `McpPlayerController::submitFrame` → translated to synthetic `InputEvent`s → `InputSystem::stageSyntheticEvents` → the same action handling as the keyboard | Crosses as **Edit/Ray/PlayerChunk** via the game's merged input, source recorded as Local — pilot *attribution* merges upstream of the sim boundary today; the **ControlFrame** kind exists in the stream/format for the per-pilot-attributed wiring IDemoRecord will want (documented gap, not a bypass: no pilot input reaches the sim except through the stream) |
| In-world Keyboard block typing | game.cpp:1917/1926/1937 → `edit(EditCmd::Kind::Command)` | **Edit** events |
| Console / script commands | game command handling → `EditCmd::Kind::Command` via `edit()`; hot-swap uses `runBetweenTicks` (not an input path) | **Edit** events |
| Tests / replay | `ControlStream::submitStamped()` | any kind, pre-stamped (source Replay/Test) |

**Nothing sim-affecting bypasses the stream.** The remaining raw reads are presentation-only, by doctrine: the HUD look-at raycast (live ray slot), camera look smoothing (`player_.updateLook`, frame-side), and the locomotion body's game-thread stepping (its sim-visible *effect* — the player chunk — crosses as an event). `SimThread::getBlock` reads are reads, not inputs.

---

## 6. Gates (experiment tree, Release, 2-core VM)

| Gate | Result |
|---|---|
| Build | exit 0 (warnings: 7, all pre-existing in untouched files — engine_tests unused variables, game.cpp missing-field-initializers; **zero in changed files**) |
| Game selftest, legacy | **978 PASS / 0 FAIL** — 958 standing + **20 added** (§7) |
| Game selftest, handshake | **978 PASS / 0 FAIL** on solo re-run (first pass: 977/1 — the single failure was `streaming: worker-thread decode… 440 Hz`, the documented audio load flake, under concurrent build load; not a control check) |
| `--handshake-test` | OK |
| `--handshake-scenario 2048` legacy + handshake | `faec0f02700d98c6` both — unchanged |
| `--control-test` | **OK** — 36 checks (20 unit + 11 determinism + 5 soak), all PASS |
| `--control-latency` | OK — §4 |
| ctest (serial) | see the ctest account below the table — **47/48 clean-serial best**, sole failure the embedded audio flake; no control-related failure in any run |
| Perf (see §8) | 512 TPS held in all 4 configs; tick avgs within contention noise of the status report |

Production tree: untouched — `md5sum -c` against the fix-program final manifest is clean.

**ctest account (corrected — an earlier draft of this report carried the status report's production-tree 48/48 figure in the row above; that was a reporting error, now fixed; the logs below are the experiment tree's own).** Run 1 (contended — overlapped this session's own direct gate runs on the 2-core box): **45/48** (`ctest.log`). Failures: `engine_systerian` — **locational, not a code failure**: it loads master SVGs via a path relative to the tree (`engine/src/systerian/../../../../user/files/…`) that assumes the tree sits at `~/workspace/<name>`; the experiment copy sits one level deeper. Proven by shimming the path (`~/workspace/experiments/user` → symlink to `~/workspace/user`) and re-running the single test: **PASS**. `engine_audio` and `lithokraft_selftest` — both the documented audio load-flake family: the selftest's failing line was its embedded `streaming: worker-thread decode… 440 Hz` check, all other 977 checks passing. Run 2 (clean serial, systerian path shimmed; `ctest-clean.log`): **47/48** — the sole failure again `lithokraft_selftest`'s embedded audio-streaming check (977 PASS lines alongside it), while the same binary run solo scored **978/0** repeatedly the same day in both modes (and again on a confirmation re-run after run 2, once load settled). The audio-streaming check is timing-sensitive under multi-crew compile load on this VM and is unrelated to this change (audio decode worker thread; it flaked identically on the unmodified production tree per the status report). No control-serialization check failed in any run.

## 7. Added selftest checks (the 20, all `[PASS] control: …` in `--selftest`)

Stamping/order: nothing extracts before its stamp tick · all extract at their stamp tick · live events stamped (seq 0,1,2 / tick) in submission order · accounting exact (0 late / 0 dropped / 0 violations). Injection: injected + live all extract · application order is (tick, seq), not arrival · live seq continues past injected seqs · past-stamp extractions accounted late, never dropped. Recording tap: reproduces the applied stream verbatim · takeRecord drains · recording off retains nothing. Format: writes · parses its own output · header round-trips · every kind round-trips field-for-field · truncated fails closed · flipped byte fails checksum · bad magic refused with valid checksum · unknown kind refused · line format carries seq/tick/kind/source.

## 8. Performance vs the status report

Method as the status report's: `./LithoKraft --perf-headless` circuits section at the 512 TPS target. **Contention note:** the model-dither crew was compiling on the same 2-core VM throughout this window (load average 2.3–3.9 on 2 cores; `cc1plus` observed live, exactly the condition the status report flagged for its own degraded run 1). A fully quiet box never occurred, so these numbers carry residual contention; the status report's quiet run 2 is the comparison column.

| Scene | Mode | This build (achieved / avg / p99) | Status report run 2 (achieved / avg / p99) |
|---|---|---|---|
| Default circuits (5,531) | legacy | run 1: 510 / 0.724 ms / 5.879 · run 2: **512.0 / 0.550 ms / 1.563** | 512.0 / 0.463 ms / 0.738 |
| Default circuits | handshake | **512.0 / 0.529 ms / 1.783** (single run, contended) | 512.3 / 0.556 ms / 1.786 |
| Wire-torture (1,445) | legacy | **512.3 / 0.394 ms / 1.476** (single run, contended) | 512.3 / 0.091 ms / 0.207 |
| Wire-torture | handshake | **512.3 / 0.250 ms / 0.426** (single run, contended) | 515.0 / 0.548 ms / 6.124 |

Reading: **achieved TPS is at parity in every configuration** — the headline gate holds. Tick averages sit within the contention envelope the status report itself documented (its contended legacy default run 1 averaged 1.454 ms — worse than any number above); the directly comparable handshake-default average (0.529 ms, taken *under* load) is at-or-better than the status report's quiet 0.556 ms, and wire-torture handshake improved on both avg and p99. The legacy-default run-2 average (+0.09 ms vs the quiet baseline, with load still ≈2.3–3.4) is attributed to that residual contention, not the change: the perf scenes submit no control events during measurement, so the only new per-iteration work is the PRE drain of two empty queues and a `hasDue` check per tick — nanoseconds against a ~0.5 ms tick. No regression beyond noise.

Structural note: with no control input in flight, the stream's per-iteration cost is two mutex lock/unlock pairs and an empty-vector swap (PRE) plus a `hasDue` check per world tick (POST) — nanoseconds against a ~0.5 ms tick; the perf scenes submit no edits at all during measurement.

## 9. The IDemoRecord consumption contract

IDemoRecord (next build, JD's name — the interface that *records*, producing the demo *record*) consumes exactly this layer:

1. **Record:** `sim.controlStream().setRecording(true)`; periodically `takeRecord(events)` — the applied events, stamps intact, in application order. Serialize segments with `writeControlStream(header, events, bytes)` — header from the session: world seed, mode, tick rate (the gear), handshake phases; declared ticks/events filled per segment or at close.
2. **Replay:** `parseControlStream(bytes)` (fail-closed), then `controlStream().submitStamped(events)` into a fresh SimThread over the same initial world; step or run — the world evolves bit-identically to the recorded session (proven in §3 for the SimThread-driven pipeline).
3. **Scope honesty:** a *complete* session record also needs the game-side kinds (Locomotion samples as the game consumes them, ControlFrames at the pilot adapter) tapped where they are consumed — the kinds and format already carry them; wiring those two taps is IDemoRecord's own first task, along with the initial-state snapshot (world save) a record must reference. The sim-boundary stream — everything that can change the world — is complete in this layer.
4. **Invariants IDemoRecord may assert at runtime:** `stats().dropped == 0`, `stats().orderViolations == 0`, `applied ==` events recorded.

## 10. Production port instructions

Copy these 13 files from `~/workspace/experiments/control-serialization/` to the same paths in `~/workspace/litholattice/`, then rebuild `build-linux` and re-run the gates in §6 (selftest counts become 978; scenario stays `faec0f…`).

**New (6):**
- `engine/src/sim/include/litholattice/sim/control_stream.h`
- `engine/src/sim/control_stream.cpp`
- `game/LithoKraft/src/app/control_stream_test.h`
- `game/LithoKraft/src/app/control_stream_test.cpp`
- `game/LithoKraft/src/app/control_latency.h`
- `game/LithoKraft/src/app/control_latency.cpp`

**Modified (7):**
- `engine/src/sim/CMakeLists.txt` — `control_stream.cpp` added to `litholattice_sim`.
- `engine/src/sim/include/litholattice/sim/sim_thread.h` — forward declarations; `setPlayerChunk`/`setRay`/`edit` moved to the .cpp as stream producers; `controlStream()` accessor; `applyEdits` → `applyControlEvents` + `applyOneEdit`; members: `edits_`/`editBuf_`/`drainBuf_`/`playerChunk_`/`stepRay_` replaced by `control_`/`controlDue_`/`stampedRay_`/`areaChunkKey_`.
- `engine/src/sim/sim_thread.cpp` — producers, PRE/POST wiring, paused-path stamp semantics (see §1.3).
- `game/LithoKraft/src/app/CMakeLists.txt` — the two harness .cpp files join the `lithokraft` target.
- `game/LithoKraft/src/app/main.cpp` — `--control-latency`, `--control-test`, `--control-scenario TICKS legacy|handshake|baseline` flags.
- `game/LithoKraft/src/app/selftest.cpp` — `fails += runControlSelfTest();` after the handshake section.
- `game/LithoKraft/src/app/handshake_test.h` / `handshake_test.cpp` — **two files**: exports `ControlScenarioWorld` / `buildControlScenarioWorld()` / `hashControlWorld()` wrappers around the existing §6.4 internals (no behavior change; `--handshake-test` still OK).

**Conflict risk:** low. The sim files were last touched by the fix program (P0a/P1) but no other crew is working in `engine/src/sim` concurrently (the model-dither crew works in render). `handshake_test.*` is touched only by the test crews. If the extensive-test crew regenerates anything, it writes reports, not these sources. The one semantic reviewers should double-check at port time is §1.3's paused-path tightening.

## 11. What is deliberately not here

- **IDemoRecord itself** — the interface, file lifecycle, playback UI: JD green-lit it as the *next* build; this layer is its foundation and contract (§9).
- **Per-pilot source attribution** for MCP-driven edits (merges upstream today, §5) — the ControlFrame kind is ready when that wiring is wanted.
- **Sub-tick control gears** — explicitly against spec: one power of two, the world tick.
- **Entity/mob input** — entities remain outside the deterministic core, as before.
