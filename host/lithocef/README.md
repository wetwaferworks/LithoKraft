# IHwLayer — the hardware layer (formerly LithoCEF)

**Renamed and split 2026-10-25 (JD).** The layer is no longer just the CEF
host: it is *the* hardware layer — it sits outside the engine, boots the
engine, plugs into the hardware, and provides hardware resources while the
engine never learns what it runs on. Interface set (JD, 2026-10-25 01:54 EDT):

- **IHwLayer** (`IHwLayer.h`) — the base contract every tenant sits behind.
- **IHwCef** (`IHwCef.h`) — the CEF host tenant; the current working path.
- **IHwWebkit** — WebKit / Safari view tenant, stub only (phase 2).
- **IHwGpu** — GPU capability interface, the seat for hardware draw paths (phases 2–3).

`ILithoCEF.h` remains as a compatibility header mapping the old names onto
IHwLayer. The directory stays `host/lithocef` and the engine option stays
`LL_WITH_LITHOCEF` (renaming them would touch engine/CMakeLists.txt, outside
this layer). Targets: `lithohw_stub`, `lithohw_thread_handshake`,
`hwlayer_smoke`, `hwlayer_boot_smoke`, `thread_handshake_smoke`.

Doctrine (JD's, locked): proprietary and closed — `host/` stays out of the
public MIT drop; the Thread Handshake lives here, semantics frozen; the
Timing Handshake is *not* part of this layer; no Emscripten here (no linking,
no includes); loud boot errors stay loud; the scenario hash does not move.

The constitution below is carried verbatim from LithoCEF; read "LithoCEF
shell" as the IHwLayer base and `lithocef_*` as `lithohw_*` / `hwlayer_*`.

---
# LithoCEF — the Litho CEF layer

JD's law of the shape (verbatim, 2026-10-20 08:21 EDT):

> "The CEF layer will sit OUTSIDE of the engine and just boot the engine.
> And the CEF layer plugs into the hardware layer."

## Layering

```
hardware layer    display, GPU, input devices, OS windowing
LithoCEF shell    host process; plugs into the hardware layer;
                  boots the engine; owns surface, input feed, pacing
the engine        LithoLattice / LithoKraft, booted inside the shell,
                  untouched by how it got there
```

The engine never learns what it is running on. The shell absorbs the
hardware so the core doesn't have to. This is the web build's shape,
named: the itch page boots the WASM engine today, owns the canvas, the
input ring, and the GL context, and presents through the hand-written
thin imports in `engine/src/render/gl33/lk_webio.js`. LithoCEF is that
shell, grown up and built from our own Chromium (roadmap, far end: a
custom Chromium package for Wet Wafer Works hardware, headsets first —
see the run summary of 2026-10-20).

## The interface: `ILithoCEF`

`ILithoCEF.h` is the whole contract, and it is deliberately plain C —
no C++ types, no Chromium/CEF types, no OS headers — so nothing
browser-flavored leaks into engine headers and the table can cross a
DLL border unchanged. A backend exports one symbol,
`lithocef_get_api()`, returning a versioned function table:

- **Lifecycle** — `create` / `boot_engine` / `shutdown` / `destroy`.
  The shell boots the engine (payload URI + forwarded args); the
  engine's own startup stays the engine's business.
- **Surface provision** — `acquire_surface` / `release_surface` hand
  the engine a surface descriptor: a native window + GL context the
  shell owns, an off-screen GL texture for compositing, or NONE
  (headless / sealed sim runs). Handles are platform-opaque `void*`;
  the engine carries them, never dereferences them.
- **Input injection** — the engine registers an `LithoCefInputSink`;
  the shell translates hardware input into events and pushes them
  through it. The engine-side consumer is the memory-mapped input ring
  proven by the web build (build 62/66): the shell *feeds* the ring, it
  never reaches past it. Events carry writer-side monotonic timestamps
  (event time, never drain time — the drain-fix lesson).
- **Present / pacing hooks** — `present()` hands the finished frame to
  the hardware layer; `refresh_millihertz()` reports the display
  cadence the shell paces against. The shell paces *presentation* only;
  the engine's step clock (Timing Handshake, 256 TPS) is never the
  shell's to touch. Counters (`frames_presented`, `dropped_inputs`)
  and `last_error` keep the bench honest.

Determinism note: the shell is outside the sim by construction. It can
starve or flood presentation; it cannot move a scenario hash.

## The Thread Handshake (`thread_handshake.h`)

JD's thread architecture for this layer, designed by him on
2026-10-22 (in his words: "a whole new architecture I just designed";
he has not seen it in engine systems or the diagram books):

- **Top: the thread pool.** At hardware init the hardware layer takes
  its thread census and the pool is sized from the machine present —
  no hard-coded counts (modest hardware, modest pool).
- **Bottom: the thread dispatch pool**, facing the engine and other
  systems. As soon as the pool is available it gives the dispatch
  pool **one thread** — the listener/dispatcher — "to assign other
  threads where needed." All assignment flows through that one
  thread, in submission order, so grants can never race.
- **"Thread needed" calls.** Consumers (the engine, a renderer, any
  system) call into the dispatch layer and request as many threads
  as they need for their kind of work and the hardware's scale. The
  dispatch pool can call the top pool for more. Grants are
  all-or-nothing (a partial grant would starve two big consumers
  half-armed). JD considered and rejected an automatic one-thread
  grant to whatever renderer is running — demand decides.
- **Not the Timing Handshake.** The Timing Handshake governs *when*
  the sim steps; the Thread Handshake governs *who does the work*.
  Two handshakes, two layers.

`lithocef_thread_handshake` implements it (pure `std::thread`, no
engine dependency); `thread_handshake_smoke` gates it by counts:
census sizing, the dispatcher's seat (capacity = census − 1),
per-worker job invocation, all-or-nothing in-order grants, door
refusals for impossible requests, cancel-on-destroy, and the
one-thread machine (dispatcher doubles as the only worker).

## Backends

- **Stub (`lithocef_stub`) — this run.** Reference implementation with
  no CEF dependency and no hardware layer: inert NONE surface (honest
  null handles), recorded boot payload, counted frames, sink-forwarded
  input via the test hooks in `lithocef_stub.h`. `lithocef_smoke`
  exercises the whole contract and is registered with ctest when the
  option is on; `lithocef_boot_smoke` covers the lifecycle beyond it —
  re-boot after shutdown, a finite input ring whose capacity drops are
  counted, the Thread Handshake serving the engine while booted, and
  `frames_presented` pinned as a shell-lifetime counter.
- **CEF (next).** A backend linking the Chromium Embedded Framework's
  `libcef_dll_wrapper`, creating a real browser/window surface plugged
  into the host OS, booting the shipped web build's page as its first
  payload. CEF types stay inside that backend's translation units —
  `ILithoCEF.h` consumers never see them.

## Building

Default build: **nothing changes**. The host area is only added when
the engine is configured with:

```
cmake -S engine -B <build-dir> -DLL_WITH_LITHOCEF=ON
```

That builds `lithocef_stub` and `lithocef_smoke` alongside (never
inside) the engine and game targets. With the option OFF (the default)
the build is behaviorally identical to before this layer existed: no
new targets, no sim changes, no hash movement.
