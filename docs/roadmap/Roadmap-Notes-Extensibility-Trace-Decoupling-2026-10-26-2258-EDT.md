# Roadmap Notes — Engine Extensibility & Trace Decoupling
**For: LithoLattice engine and LithoKraft as a whole (shared with Cortana)**
**Measured and written by Elfie, 2026-10-26 evening, at JD's direction.**

## What was measured, and how
At JD's challenge ("our engine is super extensible — try and pull out the trace system"), Elfie ran the decoupling litmus on the Litho Strike seed tree (a byte-identical copy of hub main `8726864`; the original trees were not touched; branch `elfie/trace-rip`, restored clean afterward). Method, per JD's diagnostic doctrine: isolate by areas, cut, and let the compiler and linker counts convict.

## The census (evidence)
- **The circuit system lives in the game layer, not the engine core.** cSignalSim (`content/signal.h`, 407 lines) and cSignalWorldSim (`content/signal_world.*`, 418 + 112 lines). The engine holds only generic machinery: the media tick interfaces (Trace wired / Signal wireless) and the trace JIT. (`core/f3_trace.h` is the F3 diagnostics lag-trace — unrelated.)
- **Cut 1** (module files and headers removed): the content components `passive_blocks.cpp`, `logic_components.cpp`, `input_components.cpp` include `signal.h` directly — circuits feed block behavior.
- **Cut 2** (headers present, implementations removed): **zero compile errors across the whole tree.** The link fails on **110 undefined references resolving to 22 distinct symbols**: 12 cSignalSim methods, 5 cSignalWorldSim methods + typeinfo/vtable, `runSignalSelfTest()`, and two co-located helpers (`sixbitChar`/`sixbitCode` — Systerian utilities that live in the signal files and need a new home).
- **Referencing objects: exactly 7** — `game.cpp`, `world_save.cpp`, `logic_components.cpp`, `handshake_test.cpp`, `perf_headless.cpp`, `selftest.cpp`, `wiring_selftest.cpp`.
- The undefined symbols name the interface themselves: `registerHandshake` / `tickHandshake` — the circuit sim plugs into the Timing Handshake through a clean socket.

**Verdict:** the engine's extensibility is real and measured. The circuit system's entire external surface is two classes and ~19 methods. Removing it is a transplant, not an excavation.

## The friction that remains: static casting
`game.cpp` reaches the circuit system through roughly ten `static_cast<cSignalWorldSim&>(sim_->simulation())` sites — the game names the *concrete* class (save/load of signal state, lamp hold ticks, test scenes) instead of speaking only to the simulation interface. Those casts are why extraction is a transplant today instead of an unplug.

## JD's direction (stated 2026-10-26 22:58 EDT)
1. **Make the engine and game even more extensible**: remove the static casting where systems are consumed — callers speak to the interface, never the concrete class.
2. **The Trace system's future form: a DLL compile that you either include or exclude.** Trace becomes a module in the same spirit as IMod content — present when a game wants circuits, absent when it doesn't (Litho Strike being the first absence), with no surgery either way.
3. Litho Strike's Day-1 Phase 1 (the plain no-circuit world sim behind the handshake interface) is the proving job for this direction.

## Registrar's proposed first moves (clay, for the crew)
- Route every `game.cpp` consumer through the simulation interface; where a call truly needs signal-specific state (saves, lamp holds), move it behind an interface the signal module itself implements.
- Rehome the sixbit helpers out of the signal files (they belong with Systerian).
- Keep the census as the regression test for all future decoupling work: re-run the cut, compare counts. The numbers should only shrink.

## Addendum — the DLL direction is general (JD, 2026-10-26 23:00 EDT)
JD, minutes after the filing above: the include-or-exclude DLL form is not Trace-only. **A lot of the systems will be DLLs you include or exclude** — the software rasterizer, the Vulkan renderer, the OpenGL renderer: all DLLs, separate from the executable that loads them up, **just like the Half-Life engine**. The executable is a host/loader; capability ships as modules. (Registrar's note: this extends the house's oldest philosophy — IMod was locked Half-Life-SDK style on 2026-10-10 — from mods to the engine's own systems.)
- And the DLLs will be **signed** (JD, 23:01 EDT) — the module system inherits the house's signing discipline (the same trust chain as the mod keys), so include/exclude is also verify/refuse.
