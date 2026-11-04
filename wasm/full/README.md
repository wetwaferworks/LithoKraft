# LithoKraft — Full Test Build (WASM, 2026-10-20, engine build 0.0.0.0.0.0.1.9 / build 73, game Alpha 0.0.3.13)

## Bug-fix run (2026-10-20 evening — the current layer on top of the rip-out)

This bundle is the thin layer after the code review's fix run
(review: `Code-Review-Thin-Layer-2026-10-20-1627-EDT.md`; run summary:
`Fix-Run-Summary-*.md` beside the shipped zip). What changed:

- **Device fix (the v1.3 phone failure).** Shell v1.3 died on JD's
  moto g at `lkboot.js` with `ReferenceError: SharedArrayBuffer is not
  defined`: the Android shell's intercepted `https://app.local` origin
  never received cross-origin isolation in WebView, headers
  notwithstanding. Two fixes landed: (1) the layer no longer depends
  on the standalone `SharedArrayBuffer` constructor — the sync service
  RPC slot now lives **inside the shared wasm memory** (which worked
  on the device), and shared-memory creation is the one hard boot
  requirement, stage-named if it fails; (2) shell v1.4 serves the
  bundle from an in-app **http://127.0.0.1** server, a real origin
  whose responses get real isolation processing.
- **Loud boot.** Boot is stage-tracked; any failure prints
  `BOOT FAILED at stage "<stage>": <name>: <message>` into the page
  log and status line, plus a sticky error slot above the log.
  Boot also prints an environment line (`crossOriginIsolated=…,
  SharedArrayBuffer=…`) every time.
- **H1** — the GL stream's sync classifier missed 11 pointer-reading
  imports (they streamed async against reusable memory). Classification
  is now an explicit pointer-reader set + a boot-time audit +
  a harness census probe (`wasm/harness/probeglclass.js`).
- **H2** — the GL ring's async claim was a load→store race (two
  producers could overwrite each other's records; the stress probe
  tore 433+ records on the old protocol). Claims are now CAS tickets,
  published in claim order through a commit word the executor never
  reads past (`wasm/harness/probeh2.js`: 6,000/6,000 intact).
- **H3** — the buffer-binding cache was keyed by target alone;
  `ELEMENT_ARRAY_BUFFER` (0x8893) is per-VAO, so a VAO switch + re-bind
  was wrongly suppressed. Keyed by (VAO, target) now
  (`wasm/harness/probevao.js`). Found during verification, in the same
  constant family: the pixel pack/unpack offset paths used binding
  pnames (0x8894/0x8895) where targets (0x88EB/0x88EC) belong — also
  corrected.
- **Mediums M1–M8** (uniform-array hash keys, getdents64 encoded
  lengths, mailbox catch parity, pthread pool cap 8 + EAGAIN,
  main-worker death is terminal, no set-current on context create,
  confessed-stub registry with a real `WaitSync`, ReadPixels pack
  offset) and Lows L1/L2/L7/L10 — per-finding dispositions are in the
  run summary.
- **Bit-packing ledger:** GL ring records 16 B header + 16 B slots →
  8 B header + 8 B slots (typical draw record 64 B → 32 B); the asset
  pack header went from LKP1/JSON (229,855 B) to **LKP2 binary
  (63,523 B)**, still deterministic; the loader validates every range
  and rejects non-LKP2 packs loudly.
- **Carmack rsqrt** (`0x5f3759df` + one Newton step) is now in
  `core/math.h` (`rsqrtFast()`/`sqrtFast()`/`normalizeFast()`) and
  adopted where the engine's own gates earn it: camera/view math,
  the software renderer's per-pixel falloff + dither distance,
  sprite presentation, texture-generation voronoi, and the game
  camera controller. The native lib selftests pin numeric invariants
  at several of the review's other candidate sites (PBR cross-
  implementation equality, Snell refraction geometry, light-ordering,
  IK reach, LOD band contract, audio falloff) — the approximation was
  reverted at exactly those sites and they stay exact, as do ALL SIM /
  procgen / gameplay / net sqrt sites (never touched).
- **Console scroll:** the log pane follows the tail only while you
  are at the bottom; scroll up and new lines hold your place (JD's
  field report). Layer errors also pin into the sticky slot.
- **Counter:** every build in this run went through the normal path
  and ticked the append-only ledger (no bypass): the run's builds
  rolled the engine counter 67 → **73 (0.0.0.0.0.0.1.9)** and the game
  counter Alpha 0.0.3.5 → **0.0.3.13** — the intermediate rolls were
  the native gate builds and the rebuilds the gate failures required
  (see the run summary's ledger table). The shipped binary reports
  build **73 / Alpha 0.0.3.13**, printed on each page's stats panel,
  scraped from the boot log.

## Thin layer (2026-10-20 rip-out — the serving layer this bundle fixes)

On JD's ruling, **Emscripten is removed from this layer completely.**
The pages no longer load the generated Emscripten glue (the old
`LithoKraft.js`, 438,525 B, is gone from this bundle). They load
**`lkboot.js`** — a hand-written thin loader (ours) that implements the
WASM binary's declared import surface itself: boot/instantiate, the
in-memory filesystem (fed by our own `LKP1` pack, built by
`wasm/lkpack.py`), the syscall/WASI fd layer, a pthread worker pool
(the same file is the worker bootstrap), the mailbox proxy protocol
for main-thread services, the page services the binary imports by name
(input, canvas, frame loop, WebGL) under our own `lk_*` / `lk_gl*`
names, and a **state-caching WebGL2 wrapper**
(redundant state calls are dropped before they cross to the GPU;
draws are never cached).

The binary is re-linked from the same objects with
`-sPROXY_TO_PTHREAD=1`, `-sPTHREAD_POOL_SIZE=8`,
`-sOFFSCREENCANVAS_SUPPORT=1` and the `--preload-file` mappings
**removed** (exact recipe: `wasm/build-thin.sh`). Consequences:

- The canvas never leaves the page: there is **no OffscreenCanvas
  transfer hop anywhere** (the hop that never fired on Android
  WebView 155, which is why the old build booted to a black canvas on
  the phone while playing fine on desktop). Engine `main()` runs on a
  dedicated main worker (it cannot run on the browser main thread —
  it rendezvous-waits on threads whose main-thread services only the
  page can perform); the page owns the DOM, the canvas and the real
  WebGL2 context, services the binary's mailbox, and drives frames
  with its own rAF loop. The engine's GL calls travel a shared-memory
  command stream to the page, where our wrapper executes them; the
  page compositor then presents directly. Cost: up to one rAF of
  pipeline latency.
- `lkio_present` (our thin present verb) is the real present path on
  the software page, as originally designed.
- The `?diag=1` instrumentation stays (default off); its transfer
  counters now read 0 by design.
- No CMake configure is run for this build, so the engine build
  counter does not tick; the binary reports the same identity as the
  objects it was linked from.

The pre-rip build is preserved for analysis in
`~/workspace/emscripten-analysis/specimen/` (hash-listed manifest).

Build identity: game release **Alpha 0.0.3.4**, engine rolling build
counter **0.0.0.0.0.0.1.2** — both are printed on each page's stats panel,
scraped live from the binary's own boot log.

**This build is the drain-cadence fix** (the run was dispatched against
build 62; the rolling counter ticks on every build — gate builds
consumed 63–65, and the counter rolled to 0.0.0.0.0.0.1.0 at build 64 —
the shipped binary is the final build, 66, per the tree's standing
convention). On JD's ruling (2026-10-20) the web seam's three legs were
kept and the input drain was fixed first: the memory-mapped ring is now
drained at **step cadence** — a dedicated backend thread drains it on
the 256 Hz step clock, independent of presented frames — and every ring
record carries its **event-time timestamp** (writer-side
`performance.now()`), so inputs take effect ordered and stamped by
when they happened, not by when a frame happened to drain (page log:
`input: mapped ring drain at STEP cadence`). The same pass guards
pointer-lock re-acquisition after an exit (no more uncaught
SecurityError on an immediate re-lock). The thin-import surface was
also simplified to plain thin imports on JD's ruling the same night —
the earlier WASI-style fd shaping was dropped (it carried no runtime
cost; the drop is a simplification, not a perf fix). Scenario hashes,
selftest, and the Node smoke profile are unchanged. Full record: the
build-66 addendum in `wasm/FULL-TEST-REPORT.md`.

**Build 62 is the web-seam + bit-ops pass** (dispatched as "build 59";
gate builds consumed 59–61, shipped binary 62). It lands the approved
web-seam design:
(1) **memory-mapped input** — on cross-origin-isolated pages the DOM
listeners write binary input records into a shared-memory ring
(`input: memory-mapped ring LIVE` in the page log) and the game drains
it at frame boundaries; the previous string-queue transport remains as
the automatic fallback. (2) **Thin imports** — the software page's
present now runs through one hand-written `--js-library` verb
(`lkio_present`, a plain thin import) instead of ~15 stock GL crossings;
**`?lkthin=off`** restores the classic path. (3) A **bit-ops sweep**:
the tree's hot bit expressions (SplitMix64 mixers, FNV-1a steps,
palette pack/unpack, StoredQuad AO fields) now spell through one
shared, differential-tested header — values bit-identical, scenario
hashes unchanged. Full record: the build-62 addendum in
`wasm/FULL-TEST-REPORT.md`.

**Build 58 is the pacing pass** (after JD's itch play test of build 55:
GL page at 55.96 ms/frame on his GPU with a healthy sim). It adds two
URL dials — **`?radius=N`** (render/stream radius, both pages) and
**`?aa=on|4|off`** (WebGL antialias request, GL page) — and removes
redundant per-mesh uniform uploads in the GL chunk renderer. The AA
question is answered in the Pacing-Pass addendum in
`wasm/FULL-TEST-REPORT.md`: the shipped context requested **no**
antialiasing, so AA was already off; the dial exists so its cost can
be measured (`?aa=on` ≈ +50% frame time on the software-GL bench).

**Build 55 fixes the black-canvas defect of build 49** (found in JD's
itch playtest, 2026-10-19): interactive boot booted, simulated and
answered input but never presented a frame. The frame loop is now
driven through the browser's per-frame scheduler, so the launch menu
and the world render live on the page. Bench mode (`?bench=1`) is
unchanged. Root cause and proof: the build-55 addendum in
`wasm/FULL-TEST-REPORT.md`.

The full LithoLattice engine + LithoKraft game, compiled to WebAssembly,
running the real game: real worldgen (seed **20261019**), the real texture
packs (372 textures), the real wafer world.

## The two pages (the A/B)

- **`index.html`** — the **software rasterizer** path (CPU renderer +
  WebGL presenter blit).
- **`gl.html`** — the **GL33 renderer on WebGL2** (GLSL ES 3.00 shader
  translations; `--gl-arena off`, see below).

Same build, same world seed, same bench on both pages, cross-linked at the
top of each page. That is the point of the pair: if both paths agree, a
fault is above the renderer; if they differ, it is inside one renderer.

Each page boots the game **interactively by default**: the launch menu
appears, **Enter** starts play, a second click grabs the mouse
(pointer lock; Esc releases), WASD/Space play, F3 shows the live tick
overlay. Keyboard and mouse are fully wired (HTML5 input backend);
if pointer lock is refused, a drag-look fallback keeps mouse look
working. Add **`?bench=1`** to a page URL to restore the original
4-second orbit benchmark boot (self-closing), and **`?debuginput=1`**
to echo every input event into the on-page log. A stats panel under
each page shows the build identity, renderer, seed, page cadence,
input/pointer-lock state, and (in bench mode) the bench frame and
sim-tick numbers — no console needed.

## Radii (JD's web-build ruling, 2026-10-19; default revised by Build 79, 2026-10-21)

Both pages pass **`--stream-radius 16`** by default: chunk render/stream
distance **16 chunks** around the player/camera — JD's floor (render 16 /
sim 8, 2026-10-17). Build 79 moved the default down from 20 after the
build-78 device run traced the phone flicker to the radius-20 resident
set (1,681 columns) never finishing its fill on modest hardware; the
radius-16 set is 1,089 columns. The sim (ticking) radius stays
**8 chunks** (`tickRadius: 8` in the bundled `config/settings.json`,
unchanged — determinism is untouched by either value). Override the
render radius per page load with **`?radius=N`** (4–32), e.g.
`gl.html?radius=20` restores the old default on hardware that wants it;
the stats panel's mode cell shows the radius in use.

## Pacing switches (build 58)

- **`?aa=off|on|4`** (GL page only): sets the WebGL context's
  `antialias` attribute at creation. Default **off** — the shipped
  behavior (the engine requested no MSAA; there is no MSAA resolve pass
  in the GL path). WebGL's attribute is boolean, so `on` and `4` land in
  the same place (ANGLE's implicit MSAA is conventionally 4×). The boot
  log states the outcome (`window: WebGL antialias …`). Combine freely:
  `gl.html?radius=12&aa=on`.

## Requirements

- A browser with **WebGL2** and **SharedArrayBuffer** (the build is
  multithreaded). On itch.io that means the project's
  **"SharedArrayBuffer support"** option must be enabled so the game is
  served cross-origin-isolated; locally, serve with COOP/COEP headers
  (the tree's `wasm/serve.py` does).
- First load downloads ~13.0 MB (loader + WASM + asset package).

## Known web-path differences (all logged on the page, nothing silent)

- GL path: the PBR/BinaryQuad shader uses `samplerBuffer` (GLSL ES 3.10),
  which WebGL2 cannot compile — the PBR pass is disabled and the classic
  chunk path renders. Everything else compiles from the pack.
- GL path: chunk meshes use per-mesh VAOs (`--gl-arena off`) because
  WebGL2 has no base-vertex draw; same meshes, same pixels.
- Software path: SwiftShader-class WebGL lacks the BGRA upload extension,
  so the presenter falls back to a CPU-swizzle RGBA upload (one log line).
- AngelScript's `log()` binding fails to register on WASM
  (`asNOT_SUPPORTED`); scripts otherwise load.

## Files

`index.html`, `gl.html`, `lkboot.js` (the hand-written thin loader),
`LithoKraft.wasm`,
`LithoKraft.data` (the asset package in our LKP1 format: engine assets
with ES 3.00 shaders, the game's real packs/models/textures, scripts,
config), `touch.js` (touch overlay).

Full build/wiring notes and verification evidence: `wasm/FULL-TEST-REPORT.md`
in the source tree (not shipped in this bundle).
