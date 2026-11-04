# Timing Handshake Promotion — 2026-10-14

**Directive (JD, 2026-10-14 04:27 EDT):** "I want to promote the Timing Handshake and coupling of the event system as the standard when it passes over to me and Cortana on the workstation. Go ahead and begin with that."

**Result: DONE.** The Timing Handshake (Time Serialization) is now the **default** sim-timing mode of the production tree, with the coupled control/event serialization bracket as the standard event path riding with it. Legacy mode is unchanged and remains available as an explicit opt-out (`--no-timing-handshake`, cvar `sim.timing_handshake 0`, or `"timingHandshake": false` in settings.json). The pre-promotion snapshot at `~/workspace/backups/litholattice-pre-handshake-default-2026-10-14/` was not disturbed; a snapshot diff confirms the change set is exactly the 12 files listed in §2 — nothing else moved.

---

## 1. Inventory — everywhere the default lived (before the flip)

The mode default was **duplicated across five sites** (finding F1, §5). All were inventoried before any edit:

| # | Site | Role |
|---|------|------|
| 1 | `engine/src/sim/include/litholattice/sim/sim_thread.h:72` — `SimConfig::timingHandshake = false`, with a comment asserting "default OFF" (L70–71) | Engine struct default; an unconfigured `SimThread` ran legacy |
| 2 | `game/LithoKraft/src/game/include/lithokraft/game/settings.h:73` — `GameSettings::timingHandshake = false` | The runtime driver: persisted settings, cvar bridge (`sim.timing_handshake` via `settings_cvars.cpp`) |
| 3 | `game/LithoKraft/src/game/include/lithokraft/game/game.h:74` — `GameConfig::timingHandshake = false` | `main.cpp`'s `cfg` type; CLI/settings flow into it, then into `SimConfig` at world load (`game.cpp:733`) |
| 4 | `game/LithoKraft/src/app/perf_headless.h:10` — `PerfBench::timingHandshake = false` | Bench struct default (in practice overridden by `main.cpp:678` from `cfg`) |
| 5 | `game/LithoKraft/src/app/selftest.h:8` — `runSelfTests(..., bool timingHandshake = false)` default argument | No caller relies on it (`main.cpp:682` always passes `cfg.timingHandshake`) |

**Verified non-defaults (checked, no change needed):**
- **CLI:** `--timing-handshake` / `--no-timing-handshake` both already exist (`main.cpp:559–560`); with neither flag, the value comes from settings (`main.cpp:648`). No default is baked into arg parsing.
- **Shipped config:** `game/LithoKraft/config/settings.json` carries **no** `timingHandshake` key, so the struct default rules; no persisted `settings.json` in the build tree or user dirs pins `false`.
- **Scenario/test runners:** `--handshake-scenario`, `--control-scenario`, `--handshake-test`, `--control-test`, `--demo-test` all take or set mode **explicitly** (`handshake_test.cpp`, `control_stream_test.cpp`, `demo_record_test.cpp` set `cfg.timingHandshake` per arm). No hidden default assumptions.
- **Selftest pins:** a full-tree search found **no pin asserting the OFF default** — no behavioural pin needed changing (see §4).
- **CTest:** `lithokraft_selftest` invokes bare `--selftest`; the selftest A/B-compares both modes in-process (`selftest.cpp:1321–1322`), so it is mode-agnostic.
- **MCP:** the "handshake" hits in `engine/src/mcp/` are the JSON-RPC protocol handshake — unrelated.
- **Engine internals:** `TimingHandshakeAuthority::Config` defaults (phases, watchdog) are mechanism constants, not the mode switch; untouched.

**Comment/doc strings in-tree that stated the default or "experiment" status** (updated, §2): `sim_thread.h` header comment and section banner, `timing_handshake.h` file header, `world_simulation.h`, `sim_thread.cpp`, `settings.h`, `game.h` (×2), `settings_cvars.cpp` cvar comment, `perf_headless.h`, and the section banners in `signal.h`, `signal_world.h`, `signal_world.cpp`. No in-tree README/`.md` states the handshake default (the only `.md` hit is the CREDITS entry, which is historical and correct as written). Out-of-tree docs are listed in §6.

## 2. The flip — 12 files

Value flips (5):
- `engine/src/sim/include/litholattice/sim/sim_thread.h` — `SimConfig::timingHandshake = true`; comment rewritten: DEFAULT ON since 2026-10-14 (JD's promotion), legacy = explicit opt-out.
- `game/LithoKraft/src/game/include/lithokraft/game/settings.h` — `GameSettings::timingHandshake = true`; doc comment annotated "(default ON since 2026-10-14, JD's promotion)".
- `game/LithoKraft/src/game/include/lithokraft/game/game.h` — `GameConfig::timingHandshake = true`.
- `game/LithoKraft/src/app/perf_headless.h` — `PerfBench::timingHandshake = true`; comment notes the default follows the standard and `--no-timing-handshake` selects legacy.
- `game/LithoKraft/src/app/selftest.h` — `runSelfTests` default argument `= true` (consistency only; no caller depends on it).

Comment/doc-string updates only (7): `timing_handshake.h`, `world_simulation.h`, `sim_thread.cpp`, `settings_cvars.cpp`, `signal.h`, `signal_world.h`, `signal_world.cpp` — "(experiment)" status markers removed/reworded to reflect the promotion; no code semantics touched.

No other behaviour changes. Legacy code paths are byte-for-byte what they were; they are simply no longer the unconfigured default.

## 3. Event coupling — verified from code, exercised by gates

Verified in `engine/src/sim/sim_thread.cpp` (not from memory):
- `ControlStream` is constructed unconditionally in the `SimThread` constructor (L70).
- **PRE phase:** `control_->prePhase(worldTickN_)` runs at the head of every sim iteration (L~562), draining every control source once and stamping each event `(seq, worldTick)`.
- **POST phase:** `applyControlEvents(worldTickN_)` runs inside the master step after the domain phase — in handshake mode that is after the handshake workgroup phase commit; the same bracket wraps the legacy step (L505). Matured events apply exactly once, in (tick, seq) order.
- The IDemoRecord tap sits inside `applyControlEvents` (Locomotion/ControlFrame kinds feed the record), so recording/replay engage identically under the new default — proven by `--demo-test` below: recorded = legacy replay = handshake replay, all at the standing hash.
- Engagement under the new default is observable in the gates themselves: the no-flag `--selftest` run's log carries the line "(selftest invoked with --timing-handshake: …)" even though no flag was passed, and the no-flag `--perf-headless` run tags every TPS line `[handshake]`.

## 4. Gates — coordinator-run, settled tree

| Gate | Result |
|---|---|
| Full build | **exit 0** (see environment note below) |
| Selftest, no flags (now handshake default) | **1191 PASS / 0 FAIL**, `ALL SELFTESTS: OK`, exit 0 |
| Selftest, `--no-timing-handshake` (legacy opt-out) | **1191 PASS / 0 FAIL**, `ALL SELFTESTS: OK`, exit 0 |
| `--handshake-test` | **HANDSHAKE TESTS: OK**, exit 0 |
| `--control-test` | **CONTROL TESTS: OK**, exit 0 |
| Scenario `--handshake-scenario 2048`, legacy | `hash=aa82e7afb1eff9d3` |
| Scenario 2048, handshake 2 workers / 4 phases | `hash=aa82e7afb1eff9d3` |
| Scenario 2048, handshake 4 workers / 4 phases | `hash=aa82e7afb1eff9d3` |
| `--demo-test` | 43 PASS / 1 FAIL — see flag below; **all demo arms agree at `aa82e7afb1eff9d3`** (recorded = unrecorded = legacy replay = handshake replay) |
| ctest serial (`-j1`) | **48/48 passed**, 100%, 79.8 s, exit 0 |
| Perf, no flags (now-default handshake), `--perf-headless` | **512 TPS achieved** — tick avg **0.522 ms**, p99 **1.075 ms** (pre-promotion handshake battery: 0.578 / 1.993); all five ladder rates (32–512) tagged `[handshake]` |
| Perf, legacy opt-out, `--perf-headless --no-timing-handshake` | 512 TPS achieved — avg 0.467 ms, p99 0.704 ms (prior legacy battery: 0.472 / 0.660) |

**Pin accounting:** the 1191/0 base is preserved exactly in both arms; **zero pins were changed**. No pin asserted the old default, so none needed re-pinning; the only default-shaped edit outside the five value sites is the `runSelfTests` default argument (§2), listed there individually.

**Flag (pre-existing, NOT caused by this promotion):** the single `--demo-test` FAIL is the standing stale absolute-hash pin — the check expects the pre-wiring scripted hash `5041cf1795c33a8c` while every arm actually produces `aa82e7afb1eff9d3`. This is the identical 43/44 state the spawn-planting crew reported; the repin decision is already on JD's desk and was deliberately **not** taken here (out of this directive's scope). Determinism itself is intact: the scenario hash did not move on any arm.

**Environment note (build):** this VM had lost the system `libxcursor-dev` package since the tree was first built (GLFW's dependency files reference `/usr/include/X11/Xcursor/Xcursor.h`, which was absent, and the first `apt-get` attempt failed while the mirror was mid-sync). The full build was completed at exit 0 using the include/lib paths of the experiment sysroot that already exists on disk (`~/workspace/experiments/model-dither/sysroot/`), passed via `C_INCLUDE_PATH`/`CPLUS_INCLUDE_PATH`/`LIBRARY_PATH` for the build invocation only — no tree or cache changes. Postscript: the backgrounded `apt-get install -y libxcursor-dev` subsequently completed successfully, so `/usr/include/X11/Xcursor/Xcursor.h` is present again and future builds on this VM no longer need the sysroot paths. On the workstation this was always a non-issue.

## 5. Findings

- **F1 — the mode default was duplicated in five places** (inventory §1). They were flipped consistently, but nothing enforces that they stay in agreement: a future edit to one site silently forks the default. Not refactored here (outside the flip's scope, per discipline). Recommendation for a later pass: derive `GameConfig`/`PerfBench` defaults from `GameSettings`, and `SimConfig`'s from a single named constant — JD/Cortana's call.
- **F2 — usage text:** the `LithoKraft` usage string does not mention `--timing-handshake` / `--no-timing-handshake` at all. Harmless, but the opt-out deserves a line now that it's the non-default path. Left untouched (scope); noted for the handover.

## 6. Out-of-tree docs that still say "default OFF" (left alone, for Elfie)

Dated historical reports (true when written; listed for completeness): `Code-Review-Full-Production-2026-10-13.md`, `Code-Review-Fix-Program-2026-10-13.md`, `Fix-Run-Report-2026-10-11.md`, `Status-Report-Extensive-Test-Run-2026-10-13.md`, `Brief-The-Days-Yield-2026-10-13.md`, `Ascii-Systerian-Renderer-2026-10-13.md`, `Worldgen-Completeness-2026-10-13.md`, `Wiring-Team-A-2026-10-13.md`, `Wiring-Team-B-2026-10-13.md`, `Trace-Edges-CrossStamp-2026-10-13.md`.

Living documents a reader could mistake for current state: `Time-Serialization-Workbench-2026-10-13.md` (the kickoff/workbench sheet), `Time-Serialization-Control-Coupling-Addendum-2026-10-13.md`, `Brief-Whats-Missing-2026-10-13.md`, `Joint-Review-Decisions-2026-10-13.md`, and `Cortana-Return-Brief-2026-10-15.md` — the return brief in particular should gain a line about the promotion before Thursday's session if it hasn't already.

## 7. Handover note — what JD & Cortana will observe on the workstation

- **World load log:** every world load now prints `timing handshake: ON (phases/cycle 4; applies at this world load)` (game.cpp). Previously that line appeared only with `--timing-handshake` or a settings opt-in. In legacy opt-out, the line is simply absent, as before.
- **Console/settings:** cvar `sim.timing_handshake` now reads `1` on a fresh profile; a settings save will persist `"timingHandshake": true`. Changing it in settings logs "settings: timing handshake -> on/off … (applies on world load)" — still never live-swapped, by design.
- **Selftest/perf invocations with no mode flag now run the handshake**: `--selftest` prints the "(selftest invoked with --timing-handshake…)" note, and `--perf-headless` tags its TPS lines `[handshake]`.
- **Opting out to legacy (any one of):** launch with `--no-timing-handshake`; or set `sim.timing_handshake 0` in the console/settings; or put `"timingHandshake": false` in `config/settings.json`. Nothing about legacy was removed or altered.
- **Getting the old default back permanently:** the canonical sites are `GameSettings::timingHandshake` (`game/LithoKraft/src/game/include/lithokraft/game/settings.h`) for the shipped game and `SimConfig::timingHandshake` (`engine/src/sim/include/litholattice/sim/sim_thread.h`) for engine-level consumers — flip those (plus the `GameConfig`/`PerfBench` mirrors, §1) back to `false`. Better, per F1: do it in the Thursday session and consider the single-source cleanup then.
- **Nothing else changes:** same scenario hash (`aa82e7afb1eff9d3`), same 1191-check selftest, same saves/demo formats (LLCS/LLDM untouched). The promotion changed which path is the standard, not what either path computes.

---

*Gates run 2026-10-14 on the settled production tree (post spawn-garden planting). Logs: build + gate outputs retained by the coordinator; key lines quoted verbatim above.*
