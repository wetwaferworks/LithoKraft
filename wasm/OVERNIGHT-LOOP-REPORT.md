# Overnight Loop Report — 2026-10-18 → 19 (receive tree, custody 0.0.1.61)

**Orders (JD, 23:07–23:09 EDT):** the usual loop — bit-packing look → review →
bug fix → verify (gates). Conservation mode: the tree is already heavily
optimized (shake + 2026-10-17 overnight run); break nothing; reversible steps
only; gates before and after any touch; a clean pass is reported clean. Plus:
bring the WebAssembly work to a clean checkpoint (no feature push).

**Discipline held:** one editor in the tree; every edited file backed up as
`<file>.orig-2026-10-18-wasm`; JD-gated items (F1's three paths, joint F5+F8,
SIMD promotion, demo-test repin, encodeMatBytes scheduling) untouched.

---

## 0. Baseline gates — untouched tree, BEFORE any edit

Run from `tree/build-linux` with the standing binary (counter `0.0.0.0.0.0.0.33`):

| Gate | Result |
|---|---|
| `--demo-verify ../demo_record_proof.lldemo` | **COMPLETE** (6 events, 1 segment) |
| `--demo-test` (cities ON) | hash **cb32c824f8b3ea98** on recorded / no-recorder / replay-legacy / replay-handshake-2w arms |
| `--demo-test --cities off` | hash **e015e51cf1559089** on recorded + replay arms |
| `--selftest` | **ALL SELFTESTS: OK** |

The single FAIL line in `--demo-test` is the known stale pin (expected
`5041cf1795c33a8c` vs canonical `cb32c824f8b3ea98`) — the repin awaiting JD's
signature, present in the baseline exactly as documented yesterday. Not a
regression; not touched tonight.

---

## 1. WASM checkpoint

Toolchain: Emscripten **6.0.12** (`~/workspace/emsdk`), configured into the new
`tree/build-wasm/` with vendored deps (FetchContent pointed at `tree/_depsrc/`;
stb/miniaudio headers seeded from `build-linux/_deps`, byte-identical pinned
downloads). Build type Release, engine tests OFF.

### Attempt 1 — vanilla configure (no tree edits): FAILS, as recon predicted

GLFW's CMake hunts for X11 under the Emscripten toolchain and dies:
`Could NOT find X11 (missing: X11_X11_LIB)` at `_depsrc/glfw-src/src/CMakeLists.txt:181`.
Conclusion (already in WASM-READINESS.md §5): GLFW cannot be the browser route.

### Wiring added (all reversible, all backed up)

New files (additive only):
- `engine/src/platform/window_emscripten.cpp` — browser `Window`: canvas
  sizing via Emscripten HTML5, null input backend, page-session clipboard,
  `glProcAddress` → `emscripten_webgl_get_proc_address`, Vulkan honestly
  unavailable. Its header comment states the stub's limits (no HTML5 input
  events yet, no main-loop restructure yet).
- `game/LithoKraft/src/app/vulkan_stub_emscripten.cpp` — `createVulkanRenderer`
  / `probeVulkan` answering exactly as on a Vulkan-less machine (selftest
  reports SKIP, not FAIL); the app's fallback path is unmodified.

CMake edits (build wiring only; `.orig-2026-10-18-wasm` beside each):
- `engine/CMakeLists.txt` — GLFW FetchContent, glslang FetchContent, and
  `add_subdirectory(src/render_vk)` each wrapped in `if(NOT EMSCRIPTEN)`.
- `engine/src/platform/CMakeLists.txt` — `if(EMSCRIPTEN)` selects
  `window_emscripten.cpp` (links input+core only); `else()` keeps the native
  GLFW sources/link/defines verbatim.
- `game/LithoKraft/src/app/CMakeLists.txt` — `litholattice_render_vk` link
  gated to non-Emscripten; the stub source joins the exe under EMSCRIPTEN.

### Attempt 2 — guarded configure: SUCCEEDS

`emcmake cmake` completes (AngelScript 2.38 configures; generation clean).

### Compile waves

- **Wave 1:** 2 errors total before the platform dependency wall stopped the
  rest (~36% of the closure attempted):
  1. `engine/src/mcp/sockets.cpp:203` — `unknown type name 'timeval'`: glibc's
     `sys/socket.h` provides it transitively; Emscripten's musl one doesn't.
  2. `window_emscripten.cpp` — `emscripten_get_proc_address` is not the name
     in this SDK; the WebGL variant is.
- **Fixes (both in the sanctioned classes):** the shim now uses
  `emscripten_webgl_get_proc_address` (`<emscripten/html5_webgl.h>`);
  `sockets.cpp` gained a purely additive `#if defined(__EMSCRIPTEN__)`
  `#include <sys/time.h>` block (backup made; native preprocessing unchanged).
- **Wave 2 (after the two fixes): the entire `lithokraft` closure compiles
  and links.** Remaining error surface on the way, each diagnosed from the
  log, none in simulation/gameplay code:
  3. `main.cpp` / `selftest.cpp` / the stub: `litholattice/render_vk/vulkan_renderer.h`
     not found — excluding the render_vk target also removed its PUBLIC
     include path, while the app still includes the header for the factory
     declarations. Fixed in build wiring only: the app target lends itself
     the include dir under EMSCRIPTEN; the stub supplies the symbols.
  - **Artifact:** `build-wasm/LithoKraft.js` (121,524 B) +
    `build-wasm/LithoKraft.wasm` (**4,844,285 B**), `[100%] Built target
    lithokraft`, zero errors. Every engine module in the game's closure
    compiled under Emscripten 6.0.12 — core, world, sim, storage, render
    (GL33 + software presenter included), AngelScript, miniaudio device,
    net, MCP, the lot. Mid-run, an environment refresh wiped /tmp and one
    background build job; all objects under `build-wasm/` survived and the
    build resumed incrementally — no work lost, noted for honesty.

### Runtime smoke (node, the CMake cross-compiling emulator — no browser)

Exactly what was verified, no more:
- `node LithoKraft.js --selftest`: the binary instantiates and runs.
  **122 [PASS], 0 [FAIL]** covering the full signal-sim self-test and the
  content self-test (signal rules, radio, TRACE isolation/decay/round-trip),
  then **`Aborted()`** at the first line of the Audio self-test.
  Cause, evidenced: `runAudioSelfTest()` prints its header
  (`audio_selftest.cpp:70`) and constructs a `std::thread` at line 140; this
  build has pthreads disabled, and thread construction aborts. Not an audio
  bug — the threading wall, hit exactly where expected.
- `node LithoKraft.js --demo-test`: **31/31 stream-level demo checks PASS**
  (record round-trip, non-perturbation, fail-closed/recovery matrix,
  analysis queries — the demo/disc machinery itself, running in wasm), then
  the SimThread scenario section logs two worldgen errors (`unknown model
  'starfish'`, `'blossom'`) and aborts at the same thread wall.
- **Two runtime blockers, precisely bounded:** (1) **threads** — the whole
  threaded surface (SimThread, audio streaming, job system) needs
  `-sUSE_PTHREADS=1` + SharedArrayBuffer; `serve.py` already sends the
  COOP/COEP headers that unlock it in a browser. (2) **assets** — nothing is
  embedded/preloaded into the wasm FS, so file-backed models are missing
  (the worldgen errors above); the remedy is `--preload-file`/`--embed-file`
  packaging or the storage-layer design (readiness §2), a packaging step,
  not a code fix.
- **Not verified (stated plainly):** no frame has been rendered to a canvas.
  The windowed path additionally needs the `emscripten_set_main_loop`
  restructure and the GLSL 330 → ES 3.00 shader question answered — both
  already on the readiness roadmap (steps 2 and 4). The "frame on canvas"
  milestone is therefore **not** claimed; what stands is: full compile +
  link, and the game's logic executing correctly in wasm up to the thread
  wall.

---

## 2. Bit-packing look (read-only)

Verified against the 2026-10-17 audit (`audit-bitpacking.md`) with a native
`sizeof` probe compiled in /tmp against this tree's headers — every landed
claim still exact:

| Struct | Measured | Audit claim | Verdict |
|---|---|---|---|
| `SubChunk` | 8,352 B | 8,352 (post-F2) | exact |
| `ChunkColumn` | 424 B | 424 (post-F5 partial) | exact |
| `VoxelPalette` | 64 B | 64 (post-F4) | exact |
| `CellState` | 2 B | 2 (all 16 bits used) | exact |
| `MeshInput` | 29,720 B | ≈29,712 (F6 evidence) | +8 B since audit; F6 math unchanged in substance |
| `StoredQuad` | 38 B | 38 (`static_assert` in header) | exact |

Remaining audit items are all either JD-gated (F1, joint F5+F8, Class C),
GPU/transient/disk-side (F9–F16), or the two small residency items
**F6** (MeshInput snapshot pool, ≈0.9 MB of a 1.9 MB transient pool) and
**F7** (per-column pulse/trace table, ≈0.54 MB at r32) — both Class B, both
unlanded. Under conservation orders neither was attempted tonight: F6 rewrites
the mesher's snapshot format (the mesher's whole input — a packing slip is a
wrong mesh), and F7 touches every backend's mesh store; both deserve JD's
daylight go-ahead and their full gate batteries, not an overnight slot.
Nothing else in the hot paths measured wider than the audit recorded. **The
pass is otherwise clean — consistent with JD's calibration.**

_Observation (no action):_ `StoredQuad` carries the audit's known follow-on
(37 bits of corner data in 8 B/corners → a 26 B/quad form exists) — a future
phase's item, render-side, JD-gated by the same logic as F7.

_Post-audit code spot-check:_ the shake's one new storage structure, TILE's
`TextureDecodeCache` (`engine/src/assets`, bounded LRU: 512 entries /
64 MB decoded cap), was read for the same lens — keys are (path, size, mtime)
per *file*, values are the decoded images themselves. Honest shape for a file
cache; no packing finding.

---

## 3. Review pass

**WASM additions (self-review, read cold after writing):**
- `window_emscripten.cpp` implements every `Window` symbol the native file
  exports (checked against `window.cpp` line by line, including
  `displayModeName`/`parseDisplayMode`, which are byte-identical copies of the
  pure string logic). Deviations from native, all deliberate and commented:
  `create()` never returns nullptr (no window-creation failure exists in a
  tab); clipboard is page-session only; focus is always true; Alt+Enter
  display toggle is inert. The one structural gap is named in its header: the
  app's native `while (!shouldClose())` loop never yields to the browser —
  the `emscripten_set_main_loop` restructure is the known next step before
  the windowed exe can actually run in a page (the headless target is the
  runtime-smoke path until then).
- The 3 clang warnings in the shim TU are `-Wunused-private-field` notes on
  header members (`winX_`, `winY_`, `toggleHeld_`) that the *native* TU uses
  and the stub legitimately doesn't. Not defects; no cosmetic fix applied.
- `vulkan_stub_emscripten.cpp`: verified against `selftest.cpp`'s probe
  logic — `loaderFound=false` takes the `[SKIP]` branch with the stub's
  summary string, exactly the environmental-absence behavior the native code
  designed for Vulkan-less machines.
- CMake guards: native branches keep the original lines verbatim (diffed
  against the `.orig-2026-10-18-wasm` backups); nothing native-visible is
  inside an `EMSCRIPTEN` block, and vice versa. The native rebuild in §4 is
  the empirical proof of that claim, not just the diff.

**Tree state:** tonight's complete touch list (find by mtime): the 3
CMakeLists + backups, `sockets.cpp` + backup, the 2 new source files, this
report — and the build counters. Both counters rolled 33 → 35: the
`jd_build_counter` target runs on **every** build by JD's law, wasm builds
included; display-only, never in the determinism hash, disclosed here rather
than discovered later. Production tree (`~/workspace/litholattice`) untouched.
No other agent edited this tree tonight.

**Hot-path review:** the bit-packing section (§2) doubles as this pass: the
audit's landed claims re-measured exact, its unlanded items re-confirmed as
JD-gated or Class B/C. No new wide-storage finding surfaced in the hot paths
probed. Review #3's docket (R1-F1 High first) remains JD's to prioritize —
not touched under conservation orders.

## 4. Fixes applied & verification gates (after)

**Fixes applied tonight (complete list):**
1. `engine/src/mcp/sockets.cpp` — additive `#if defined(__EMSCRIPTEN__)`
   `#include <sys/time.h>` (musl lacks glibc's transitive `timeval`). Diff:
   6 lines, all inside the guard. Native preprocessing unchanged.
2. My own new files: `emscripten_webgl_get_proc_address` correction in the
   shim; the render_vk include-path lend in the app CMake (both §1).
3. Environment repair (owed from 2026-10-18, outside the tree): seven X11
   dev headers missing from `/usr/include/X11` (Xcursor + Xrandr, Xinerama,
   panoramiXext, XInput, XInput2, Xfixes) restored from the surviving
   sysroot copies in `~/workspace/experiments/model-dither/sysroot` —
   missing files only, nothing overwritten. First native rebuild attempt
   failed on exactly this (`X11/Xcursor/Xcursor.h: No such file`), not on
   any tree change.

**Nothing else was "fixed," because nothing else was found broken.** The
encodeMatBytes disk-codec sentinel remained report-only (JD's scheduling
decision; unreachable today; conservation orders).

**After-gates (native `build-linux`, rebuilt with tonight's edits in place):**

| Gate | Baseline | After | Verdict |
|---|---|---|---|
| native build `lithokraft` | (standing binary) | **[100%] Built target**, 0 errors | wiring is native-neutral, empirically |
| `--demo-verify` proof file | COMPLETE | COMPLETE | held |
| `--demo-test` cities ON | cb32c824f8b3ea98, all arms | cb32c824f8b3ea98, all arms | **held** |
| `--demo-test --cities off` | e015e51cf1559089 | e015e51cf1559089 | **held** |
| demo-test [FAIL] lines | 1 (stale pin only) | 1 (stale pin only, identical text) | held |
| `--selftest` | ALL SELFTESTS: OK | ALL SELFTESTS: OK | held |
| `ctest` suite | 47/48 (sole fail: engine_systerian, environmental) | 47/48, same sole fail | held |

Candor: under `ctest -j2`, `engine_audio` failed once in the parallel run —
the load-sensitive streaming check the 2026-10-17 candor record documents
verbatim (flakes under self-inflicted load, passes on re-run). Solo re-run:
**100% passed**. Tonight's edits touch no audio code. Build counter rolled
to `0.0.0.0.0.0.0.38` across the night's builds (JD's every-build law;
display only).

## 5. Ordered next steps

**WASM (daylight, in order):**
1. Threading pass: rebuild with `-sUSE_PTHREADS=1` and re-run the node
   smokes — the audio wall and the SimThread scenario (incl. the in-wasm
   scenario hash arms cb32/e015) are the immediate prizes; then serve the
   pthreads build via `serve.py` (headers already correct) for a real
   browser check.
2. Asset packaging: `--preload-file` the engine/game asset dirs (models
   first — 'starfish'/'blossom' are the named missing) and re-run
   `--demo-test`; worldgen should go quiet.
3. Frame-on-canvas proper: `emscripten_set_main_loop` restructure of the
   app loop + a canvas presenter behind `ISoftwarePresenter`, then the
   GLSL 330 → ES 3.00 question for the GL33 path (readiness §6 steps 2/4).
4. Only after 1–3: the persistence design (storage → IDBFS + export/import)
   — JD decision item, readiness §2.

**Engine/optimization queue (unchanged, JD's calls):** F6 and F7 (the only
unlanded Class-B residency items; §2), the joint F5+F8 refactor, F1's three
paths, SIMD promotion, TEXRES default, Review #3's docket (R1-F1 first),
the demo-test repin signature, and the encodeMatBytes sentinel scheduling.

**Housekeeping:** the seven restored X11 headers are file copies, not
package state — a proper `apt` reinstall of the -dev packages remains the
durable fix when the box next has a maintenance window.

---

*Loop closed 2026-10-19, pre-dawn. Baseline and after-gates are the same
table with the same numbers; the wasm tree went from "no EMSCRIPTEN branch
anywhere" to a linked 4.8 MB module whose logic runs — and the hashes never
moved.*
