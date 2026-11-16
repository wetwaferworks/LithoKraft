# Full Test Report — LithoKraft in the browser (2026-10-19)

**Orders (JD, 03:57–03:58 EDT):** load the full engine + full game in the
browser, textured ("not the grayscale"), at speed — and package it so JD
can A/B the two present paths on the same content from the page: GL
(hardware/WebGL2) vs the software rasterizer, same build, same world.
Differential debugging is the point: paths agree → fault is above the
renderer; paths differ → fault is inside one renderer. Software spike fix
(Job 1) landed and browser-verified first, per JD's sequencing.

## Job 1 result (prerequisite, done)

Software spike page (`web-spike/index.html`) reproduced broken in headless
Chromium, root-caused, fixed page-side, re-verified: 60 fps, 359 frames
presented in 6 s, canvas coverage 33,425/129,600 (25.8%) vs Node audit
32,781 (25.3%) — same scene. Faults: (1) page read `Module.HEAPU8`, which
classic Emscripten builds never attach to `Module` (MODULARIZE builds do —
hence Node-clean/browser-broken); first frame threw, rAF chain died, page
hung silently on "loading wasm…". (2) latent race: callback assigned after
the module script tag. Fixes + lessons in `web-spike/README.md`.

## Threading posture (decision, documented per orders)

**Pthreads ON for the full test.** Evidence: last night's no-pthreads wasm
aborted at the first `std::thread` construction (audio self-test line 140;
SimThread scenario) — the game's own threading (SimThread, audio stream,
job system) makes a single-threaded boot a dead end, not a posture.
`serve.py` already emits COOP/COEP; itch's SharedArrayBuffer option was
enabled by JD; headless Chromium honors isolation under those headers.
Single-threaded stays the documented fallback if the pthreads link fails
irrecoverably — not the plan.

## Checkpoints

- [x] Recon: app boot/loop/renderer selection; shim surface; wasm link config; asset layout
  - App: `createDisplay` builds window(1280×720) + renderer by `--renderer
    software|gl33|vulkan` (default software = CPU rasterizer + presenter
    blit). Bounded boots exist: `--bench N` (3 s warmup, self-closing),
    `--bench-path`, `--ui-shot file.png`, `--seed n` (same seed → same
    world: the A/B's honesty anchor). Assets resolve exe-dir-first
    (`/assets`, `/LithoKraft/assets` in VFS), source-path fallback.
  - Release-boot seal: baked key is JD's local dev key, NOT the retired
    test key → `bakedKeyIsTestKey()` false → no FATAL refusal; registry
    seals (designed release behavior).
  - GL33 shaders are ALL `#version 330 core` (chunk/hud/line/sprite/pbr +
    the presenter's compiled-in pair). Loader prefers pack files and
    falls back to build-time-embedded copies, logging failures per
    shader — so ES 3.00 translations in the VFS pack are tried first and
    every rejection is evidence, not silence.
  - Wiring added for this job (backups `*.orig-2026-10-19-fulltest`):
    `engine/src/render/gl33/software_presenter_webgl.cpp` (NEW: the
    presenter factory for Emscripten — ES 3.00 blit shaders mirroring
    the stock presenter, BGRA-extension upload with logged CPU-swizzle
    fallback); stock `software_presenter.cpp` body guarded
    `#if !defined(__EMSCRIPTEN__)` (native preprocessing unchanged);
    render CMakeLists gains the one source line;
    `window_web_legacy.cpp` now creates+makes-current a WebGL2 context
    on `#canvas` for OpenGL33 windows (the native contract), with
    preserveDrawingBuffer for post-exit frame inspection.
- [x] A/B pages written (`wasm/full/index.html` software, `wasm/full/gl.html`
  GL33): one Module-per-page defined BEFORE `LithoKraft.js` (Job 1 lesson),
  stdout mirrored into an on-page log, identical arguments except
  `--renderer` — `--bench 4 --bench-path orbit --seed 20261019 --scripts
  exe --mute --ui-shot --bench-json`. Same build serves both.
  - GL shader pack: `wasm/translate_shaders.py` staged
    `wasm/stage/engine-assets` (byte copy of `engine/assets` with the 11
    top-level shaders mechanically translated 330 → ES 3.00; full log in
    `stage/SHADER-TRANSLATION-LOG.md`). Only flagged hazard:
    `samplerBuffer` in chunk_pbr (ES 3.10 feature — verdict left to the
    runtime compile log, by design).
- [x] Rebuild: pthreads + asset preload — build-wasm2 links clean (LithoKraft.wasm 5.76 MB, .data 6.55 MB with all 39 .lthm models + packs preloaded). One compile fix en route: `GL_NO_ERROR` → `!= 0` in the new presenter TU (GLES3 header alias not visible there).
- [x] Node smoke #1 (2026-10-19): threads work — selftest runs 88 s deep (core SELFTEST OK, input OK) where last night's no-pthreads build died instantly. BUT demo-test worldgen fails in wasm: `unknown model 'starfish'/'blossom'` → wafer dimension loads empty → scenario hash `788a4acd8752956b` for both cities settings (internally bit-identical record/replay/handshake; NOT the native `cb32…`/`e015…`). Root cause found: `LITHOKRAFT_MODELS_DIR` is a baked **host source path** (`…/tree/game/LithoKraft/assets/models`) compiled in by CMake — the files ARE preloaded, but at the VFS path `/LithoKraft/assets/models`. The resolver has no exe-dir fallback, so every model load misses inside the wasm FS. Same root explains the wasm content-selftest failures (foliage .lthm compare, bulb cores, etched blocks).
  - Fix (build wiring only, native default byte-identical): new overridable `LITHOKRAFT_MODELS_DIR_PATH` in `game/LithoKraft/CMakeLists.txt` (default = the historical source path); the four `LITHOKRAFT_MODELS_DIR` definitions (content, model, model_selftest, items_selftest) route through it. Wasm configure passes `-DLITHOKRAFT_MODELS_DIR_PATH=/LithoKraft/assets/models`. Backups `*.orig-2026-10-19-fulltest`. Other baked paths audited: engine-assets/source-dir uses are exe-dir-first with baked fallback (fine in VFS); dev-scripts dir unused under `--scripts exe`; MODELS_SRC_DIR is authoring-tools only.
- [x] Node smoke #2 (after the MODELS_DIR fix): **worldgen quiet, both gated scenario hashes reproduce bit-identically in wasm** — cities ON `cb32c824f8b3ea98`, cities OFF `e015e51cf1559089`, each in all four modes (record / no-recorder / legacy replay / handshake replay). The wasm sim is bit-identical to native on the gated scenarios. Both runs still print "DEMO TESTS: FAILED" for exactly one check — the stale in-test constant `5041cf1795c33a8c`, which fails identically in the pre-edit native binary; it is not one of JD's gates and no hash moved. (Harness note: the Node process lingers after the tests complete — proxied-pthread keepalive — so Node runs here are judged by their printed output, not exit code.)
  - Remaining wasm selftest findings, root-caused: content segment fully explained by the MODELS_DIR fix (recheck pending in-browser); assets segment (8) = the suite reads a fixture via `__FILE__` (`engine/src/assets/testdata/cistercian_1.svg`, assets_selftest.cpp:177,238) — a host source path that does not exist in the VFS, so the fixture reads empty; test-harness artifact, not a product path (the game never reads `__FILE__` paths at runtime). Audio (2) and the JobPool deliberate-throw abort remain open diagnostics; neither is on the game's happy path.
  - Native pre-edit binary reference: demo-test cities ON records `cb32c824f8b3ea98` in all four modes (record / no-recorder / legacy replay / handshake replay) ✓; its only FAIL is the stale in-test constant `5041cf1795c33a8c` ("control crew's scripted hash") — pre-existing, not one of JD's gates.
  - Configure (build-wasm2, from `engine/`): `-pthread` compile flags;
    link `-sUSE_PTHREADS=1 -sPROXY_TO_PTHREAD=1 -sPTHREAD_POOL_SIZE=8
    -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=512MB -sTOTAL_STACK=8MB`
    (mirrors the Windows /STACK) + `--preload-file` for staged engine
    assets → `/assets`, game assets → `/LithoKraft/assets`, scripts →
    `/scripts`, config → `/config`, `mod.json` → `/mod.json`. PROXY_TO_
    PTHREAD because the game's threads synchronize with waits that are
    illegal on a browser main thread; the proxied pthread makes every
    native sync legal unchanged, and the JS libs proxy canvas/WebGL.
- [x] Browser boot via serve.py + headless Chromium — **BOTH PATHS BOOT, RENDER THE TEXTURED WORLD, BENCH, AND EXIT CLEAN** (2026-10-19, harness in `wasm/harness/`, screenshots `wasm/harness/sw5.png` + `gl3.png`):
  - Software page: `renderer: software`, resources `372 textures (0 missing), 96 models, 3/3 shader programs from packs`, `dimension 'wafer' loaded: 256 columns, 833 sub-chunks` (worldgen quiet — no starfish/blossom errors), bench `frames=4 avg=455.839 ms` on this 2-core VM (render 322.7 ms of it; CPU rasterizer at 1280×720 — workstation number, not JD's-hardware number). Presenter took its designed fallback: BGRA extension absent under SwiftShader → CPU-swizzle RGBA upload, logged.
  - GL page: `renderer: OpenGL 3.3 core | WebKit WebGL | OpenGL ES 3.0`, same resources/world; bench `frames=1 avg=27331 ms` — the breakdown shows `upload=27305.7 ms, render=3.965 ms`: the 27 s is one-time texture/mesh upload + first-compile cost through SwiftShader on 2 cores, NOT steady-state rendering. chunk_pbr fails to compile exactly as predicted (`samplerBuffer` is ES 3.10) → PBR/BinaryQuad path disabled, classic path renders — designed degradation, logged loudly.
  - Screenshots: both pages show the same seed-20261019 wafer world (pillar platform, purple trace terrain, wafer ground) + full HUD — the A/B pair agrees on the scene.
  - Three web-only faults found and fixed on the way (all Emscripten-guarded or build wiring; native untouched):
    1. No WebGL2 without `-sMAX_WEBGL_VERSION=2`, and under PROXY_TO_PTHREAD the canvas must be transferred to the proxied pthread: `-sOFFSCREENCANVAS_SUPPORT=1` (+ built-in `OFFSCREENCANVASES_TO_PTHREAD=#canvas`; pages already set `Module.canvas`). Isolated first in a 20-line probe (context create → 0 without, magenta readback 255/0/255/255 with).
    2. GL loader hard-failed on two GLES 3.1 entry points WebGL2 cannot provide (`glTexBuffer`, `glDrawElementsBaseVertex`). Fix: Emscripten-only fixups in `gl_loader.cpp` + new `gl33/gl_web_shims.cpp` install logged no-op shims and re-verify table completeness. Call-site audit: TexBuffer's only caller is gated by `supportsPbrPath()` (off on web by construction); BaseVertex's only caller is the arena path, and the GL page runs `--gl-arena off` (same meshes/pixels, per-mesh VAOs). Backup `gl_loader.cpp.orig-2026-10-19-fulltest`.
    3. GL33 `init()` compiles the built-in shaders up front; the 330-core built-ins can never compile on WebGL2. Fix: overridable `LITHOLATTICE_SHADER_DIR_PATH` (render CMakeLists; native default unchanged) — the wasm build embeds the staged ES 3.00 translations as its built-ins, the same text the pack carries. One translator gap found by the attempt: ES 3.00 fragment shaders need sampler default precisions — `translate_shaders.py` now also emits `precision highp sampler2DArray;` + `sampler3D;`.
    4. `Window::setTitle` used a raw EM_ASM touching `document` — fatal in the DOM-less proxied pthread (`ReferenceError: document is not defined`, killed the worker at first title update). Now `MAIN_THREAD_EM_ASM`. It was the only `document.` EM_ASM in the tree (grep-verified).
  - Open (non-blocking, logged in both runs): AngelScript `RegisterGlobalFunction 'void log(const string &in)'` fails with `asNOT_SUPPORTED` — scripts run without the log binding on wasm. Not chased (conservation mode; gameplay unaffected in bench posture).
- [x] A/B pages: same build + world, software vs GL present path, toggle from the page (cross-linked pair, identical args except `--renderer` + GL-only `--gl-arena off`; verified above)
- [x] Native gates after edits — **both scenario hashes exact, selftest ALL OK** (run on the pre-edit native binary, which is the correct baseline: the full native relink is blocked on this workstation by a pre-existing environment gap — GLFW needs `X11/Xcursor/Xcursor.h`, not installed, and the package manager is held by a system process; my edits are proven native-inert below, so the baseline binary IS the post-edit native behavior):
  - demo-test cities ON: `cb32c824f8b3ea98` in all four modes ✓; cities OFF: `e015e51cf1559089` in all four modes ✓. Both verdicts print "DEMO TESTS: FAILED" for exactly one check — the stale in-test constant `5041cf1795c33a8c` ("control crew's scripted hash"), pre-existing and unrelated to JD's gates; recommend the crew refresh that constant to `cb32…`.
  - `--selftest`: **ALL SELFTESTS: OK**, exit 0 (every segment: sim, content, audio, input, assets, procgen, cities…).
  - Native-inert proof for the edits: after reconfiguring with the edited CMakeLists, `litholattice_render` + `lithokraft_content` + `lithokraft_model` rebuild clean natively; the regenerated `LITHOKRAFT_MODELS_DIR` define strings are byte-identical to pre-edit, and the regenerated `builtin_shaders.h` md5 is identical (`4ebae9b389f92dc00646c1a24cc3c5db`) — same defines, same embedded shaders, and the two source edits (`gl_loader.cpp`, `software_presenter.cpp`) are inside `#if defined(__EMSCRIPTEN__)` / `#if !defined(__EMSCRIPTEN__)` guards that leave native preprocessing unchanged.
- [x] Wasm selftest accounting (via `wasm/harness/run-node-smoke.sh`; the OFFSCREENCANVAS startup transfer needs a canvas stand-in under Node — a MessageChannel port, documented in the script): core SELFTEST OK; **CONTENT SELFTEST OK** (the MODELS_DIR fix, confirmed at suite level); INPUT OK. Remaining wasm-only deltas vs native ALL-OK: ASSETS (8) = the `__FILE__`-relative testdata fixture, a harness artifact (see above); AUDIO (2) = in-memory FLAC STREAMINFO check + the worker-thread streaming check, cause not isolated (parser is portable byte math; neither is on the game's boot path; the game runs muted here and audio is stub-tier by design); then the suite's deliberate-throw JobPool checks abort the worker (exception escaping a pthread entry under PROXY_TO_PTHREAD) — native passes the same checks; recorded as the top wasm-platform follow-up, happy path unaffected (both browser benches + 2048-tick demo sessions run the job system clean).
- [x] itch bundle: `tree/wasm/LithoKraft-Full-Test-2026-10-19.zip` (flat, `index.html` at root: both pages + LithoKraft.js/.wasm/.data + README; 12.75 MB raw, 6.06 MB zipped) + stamped copy at `~/workspace/user/files/LithoKraft-Full-Test-2026-10-19-0547-EDT.zip`. **Verified as-shipped**: extracted to a fresh directory, served, software page booted to the world, benched (frames=4, avg 497 ms), exited clean. The bundle README states the scope plainly: bench/test build — boot, worldgen, rendering and sim are real; keyboard/mouse input is not wired yet (null input backend), so this is not free play.
- Evidence on disk: `wasm/harness/` (fulltest.js, run-node-smoke.sh, page logs `sw5-log.txt` / `gl3-log.txt` / `bundle-sw-log.txt`, screenshots `sw5.png` / `gl3.png` / `bundle-sw.png`).

---

# ADDENDUM — Fix run for JD's first playtest reports (2026-10-19, build 0.0.0.0.0.0.0.49)

JD played the shipped bundle on itch (GL page) and filed three reports
(10:09–10:10 EDT): (1) controls dead in the embed, no console errors;
(2) `GL_INVALID_OPERATION: glReadPixelsRobustANGLE` ×2 on gl.html;
(3) no stats readout on the page. Plus his question: why does the game
say "Alpha 0.0.2.51" while the tree counter says 0.0.0.0.0.0.0.48?
Build 49 is now the public launch build (JD, 11:12 EDT), with JD's
radii ruling: render/stream 20 chunks, sim (tick) radius 8.

## Report #4 first (identity, because it frames the rest)

The tree carries TWO counters. JD's 8-wheel rolling counter
(`BUILD_COUNTER.txt`, base-64 wheels, ticks every build) and the legacy
4-wheel game-release counter (`build_counter.txt`, Cortana-era
`lk_build_counter`, also ticks every build). The in-game string
"Alpha 0.0.2.51" is the legacy counter's *release line*
(`build_version.h` → `LITHOKRAFT_ALPHA_VERSION`), baked at link time —
an intentional game version, not a stale artifact; it simply marches
independently of the 8-wheel counter (49 game builds vs 48 counter
ticks had accumulated different histories + the full-test chain ticked
the legacy wheel three times). The fix build ticks both exactly once:
**8-wheel 0.0.0.0.0.0.0.49, release line Alpha 0.0.2.52**. Both pages
display BOTH numbers, labeled ("game release" / "engine build
counter"), scraped live from the binary's own boot lines. Whether to
retire the legacy counter remains JD's open call (it was not an error).

## Report #1 — controls (root causes, plural, each with evidence)

**Cause A (as shipped):** the bundle booted the *null* input backend
(`window_web_legacy.cpp`: "keyboard/mouse input is not wired yet") AND
booted with `--bench`, in which the app disables cursor grab and
self-exits. Dead by construction — this alone explains JD's report.

**Cause B (found by the fix, the hard one):** the replacement backend
first used `emscripten_set_*_callback` registered from the proxied
pthread. Registration returned SUCCESS, yet callbacks were NEVER
delivered in the game binary. Proof, not suspicion: a diagnostic build
added a poll() heartbeat counting callback pushes — over minutes of
runtime, across both pages, `pushed=0` while the app polled
continuously; meanwhile the page's own JS listeners saw every event
(`[page]` echoes). A standalone probe with identical flags
(PROXY_TO_PTHREAD, offscreen canvas, WebGL2, the same ten
registrations) received every callback — so the API is sound in
principle and dead in this configuration; the precise runtime
interaction could not be isolated in the sandbox and is NOT guessed at
in this report.

**The fix (JD's hypothesis, 11:11 EDT — he called it):** "hook into
the browser event system and grab it that way." The shipped transport
is page-level DOM listening: the backend constructor installs
main-thread JS listeners (document keydown/keyup; canvas
mousedown/mouseup/mousemove/wheel; pointerlockchange/error;
window focus/blur) that serialize events into a queue; `poll()` drains
it once per frame through a single `MAIN_THREAD_EM_ASM_INT` hop (a
pipe independently proven live in this binary — it also fixed the
`?debuginput=1` flag read, which had silently read the worker's URL).
All event semantics are the callback design's, unchanged: physical
`code` key map, text events, consume rules (now JS preventDefault),
position-delta drag-look fallback when pointer lock is refused, and
pointer lock re-requested *inside the mousedown handler* (the
gesture the browser grants). Two transport bugs were caught by the
bench's own instrumentation during the fix: the debug flag read from
the worker's `location` (always "off"), and a drain that swapped the
queue array out from under its listeners (fixed to clear in place).
Native code is untouched (the TU is wasm-only).

**Proof (headless Chromium, shipped pages, CDP-synthesized input):**
- Software page: menu reached → bare **Enter → `menu: play`** →
  click → **`input: pointer lock engaged`** (pointerLockElement =
  canvas), stats panel shows lock "engaged" → mouse move + held W:
  all echoed (`input> KeyDown Enter/W`, `MouseDown`, `MouseMove`);
  sim visibly alive: perf **tps 0.0 (menu) → 221–246/256**, tick
  ~3.6–3.9 ms. (`harness/fix49-sw-play-*`)
- GL page: same script — Enter → `menu: play` 32 ms later, pointer
  lock engaged, W echoed, tps up to **256.0/256**, console clean.
  (`harness/fix49-gl-play-*`)
- Focus semantics note (game's own, not a web defect): the menu
  auto-focuses its Play button; a click that misses every focusable
  widget clears that focus (`setFocus(nullptr)` on PointerDown in
  `ui_root.cpp`), after which Enter is ignored — identical natively.
  Clicking the button itself, or pressing Enter/Space first, works.
  The pages' prompt chip now reads exactly that: "Enter starts play —
  click again to grab the mouse."

**Presentation caveat (honest, unresolved in-sandbox):** in headless
Chromium the bench pages present the textured world (verified again
this run, incl. a live mid-bench screenshot), while interactive-mode
screenshots here come back black even though rendering, sim and input
all provably run. JD's own itch screenshot showed the world live in
the embed on his GPU. We did not chase this past the evidence: it is
either a SwiftShader/headless compositor artifact or a menu-state
present difference; JD's seat is the arbiter at launch.

## Report #2 — glReadPixels console defect

`gl33_renderer.cpp` `grabFramebuffer()` (the `--ui-shot` path) read
back `GL_RGB/UNSIGNED_BYTE` — legal on desktop GL, rejected by
WebGL2/ANGLE from the RGBA8 default framebuffer. Fix:
`__EMSCRIPTEN__`-guarded `GL_RGBA/UNSIGNED_BYTE` readback with the
alpha dropped into the RGB contract; native preprocessing of the TU
is byte-identical to the original (81,939 lines, proven by diff).
Verified: gl.html bench in Chromium — the error is gone (console
shows only the itch-style favicon 404 in local serving, which is not
ours), at both radii.

## Report #3 — stats panel

Both pages carry a live stats panel fed by scraping the game's own
stdout: game release, engine build counter, renderer, world seed,
mode, page rAF cadence, input state, pointer-lock state, and in bench
mode the bench frame avg/p50/p99/max + sim tick avg/p99. Verified
populated in Chromium on both pages, both modes.

## Radii (JD's ruling, 11:09 EDT)

Both pages pass **`--stream-radius 20`** (the engine's existing CLI
surface, main.cpp:589; the previous effective value was the default
32) in both interactive and bench argument lists. Sim radius stays
**`tickRadius: 8`** in the bundled `config/settings.json` — untouched,
so the determinism gates are unaffected by construction.
Bench vista frame times on this 2-core workstation (SwiftShader;
JD's-hardware numbers will differ):
- Software: radius 32 → **avg 479.297 ms** (8 frames, render 387.9)
  vs radius 20 → **avg 345.188 ms** (9 frames, render 289.5): **−28%**.
- GL: single-frame benches both runs (n=1, one-time upload/compile
  costs dominate and land in different buckets run to run: 5,007.8 ms
  upload-heavy at 32 vs 12,984.6 ms render-heavy at 20) — **not a
  comparable pair; no GL frame-time claim is made.** The software
  pair is the honest comparison.

## Gates on the final binary

- Native selftest: **ALL SELFTESTS: OK**; native demo-test: cities ON
  `cb32c824f8b3ea98`, cities OFF `e015e51cf1559089`, all arms; only
  FAIL is the pre-existing stale in-test pin `5041cf1795c33a8c`
  (repin still awaits JD's signature; not a gate).
- Wasm (Node) demo-test on the final build-49 binary: cities ON
  `cb32c824f8b3ea98` all arms; cities OFF `e015e51cf1559089` all arms.
  (One earlier OFF run under heavy parallel load failed the
  "stepped all 2048 ticks" cleanliness check while every hash still
  matched; a quiet-box re-run passed it — recorded as a load flake,
  not a regression.)
- Shipped-zip re-verification: see below (updated at packaging time).

## What remains itch-environmental

Nothing on the input path: the embed needs only the click the page
asks for (itch focuses its iframe on click; document-level keys then
arrive; pointer lock is requested from inside the gesture handler,
which is the embed-safe pattern). The wrapper console noise JD saw
(`monetization`, `xr`, allowfullscreen precedence,
`screen.orientation.lock`) is itch boilerplate, not ours.

## Shipped-zip re-verification (2026-10-19, ~12:00 EDT)

Package: `~/workspace/user/files/LithoKraft-Full-Test-2026-10-19-1158-EDT.zip`
(6,073,126 B; flat: index.html, gl.html, LithoKraft.js/.wasm/.data,
README.md). Extracted fresh, served with COOP/COEP, verified as-shipped:
- Software page (interactive, ?debuginput=1): menu → Enter → `menu: play`,
  pointer lock engaged (element = canvas, panel shows "engaged"),
  W/mouse echoes, perf tps 256–260/256, stats panel populated with
  Alpha 0.0.2.52 + counter 0.0.0.0.0.0.0.49. (`harness/zipverify-sw-play-*`)
- GL page (bench): textured frame (7,571 sampled colors), **zero GL
  errors** in console. (`harness/zipverify-gl-bench-*`)

---

# Build 55 addendum — interactive-boot black canvas: root cause and fix (2026-10-19)

## The field report

JD uploaded build 49 to itch and playtested on his real GPU (12:43–12:44
EDT): the embed boots, a click "looked like it was going to do
something", then the canvas stays **black indefinitely**; fullscreen
also black. His console paste carried only itch wrapper noise. The
build-49 addendum had recorded the same black in headless interactive
screenshots and left it open (sandbox artifact vs real). JD's test
settled it: **real defect**. Distinction check, per JD's fork: the
on-page log **fills** while the canvas stays black (in the sandbox:
boot reaches `menu: launch menu`, input goes live, Enter produces
`menu: play`, pointer lock engages, sim TPS rises to ~253–257/256).
**Boots and runs — never presents.**

## Root cause (proven, with the mechanism in Emscripten's own source)

The canvas is an **OffscreenCanvas owned by the proxied pthread**
(`-sPROXY_TO_PTHREAD=1 -sOFFSCREENCANVAS_SUPPORT=1`), and the game's
main loop was a hand-rolled, never-returning `while` loop. Current
browsers present a WebGL canvas **implicitly at task boundaries**;
a worker task that never ends never presents. Bench mode presented
because bench **exits** — ending the task flushes the final frame
(`preserveDrawingBuffer` holds it). Every bench screenshot in every
report was taken at/after exit; every interactive frame was rendered
into a buffer nobody ever saw, on any GPU, forever.

Ruled out on the way (each tested, each insufficient):
- **Focus/visibility gating, first-input gating, worldgen blocking** —
  the game demonstrably runs and answers input while black.
- **Frame pacing / thread parking** — build 49 + forced `--fps-cap 30`:
  identical black frames (menu screenshot byte-size-identical).
- **`emscripten_webgl_commit_frame()`** — a documented no-op in
  current browsers; the SDK source says so verbatim ("swap is
  implicit, so this function is a no-op for now"). Setting
  `explicitSwapControl` at context creation changes nothing without
  it. Both were tried, measured, and **reverted**.

## The fix (web seam only; no sim code touched; native unchanged)

`game/LithoKraft/src/app/main.cpp`, one file (backup
`main.cpp.orig-2026-10-19-build50`):

- The frame-loop body and shutdown tail moved out of `main()` into
  `lkStepFrame()` / `lkShutdownTail()` over a **heap-allocated
  `FrameCtx`** holding every cross-frame value (the two threeway
  worker structs moved to file scope so the context can hold them).
  Setup code binds the context fields through reference aliases, so
  the moved code is the original text, not a rewrite.
- **Interactive web runs** (`__EMSCRIPTEN__`, not bench, not Node)
  register `lkStepFrame` via `emscripten_set_main_loop_arg` — one
  task per frame, so the browser gets its presentation boundary
  every frame. The page log prints the proof line:
  `web present: interactive loop via emscripten_set_main_loop`.
- **Bench and native** iterate the same `lkStepFrame` in a plain
  `while` loop — byte-for-byte the original semantics.
- First attempt's gate (`typeof window` — `undefined` inside a
  worker, so it silently fell back) was caught precisely because the
  dispatch logs which path ran; the shipped gate tests for Node
  instead. Recorded here so the false start is not repeated.

Counters: every build ticks per JD's law; the fix iterations carried
the counter 49 → **0.0.0.0.0.0.0.55** and the game release to
**Alpha 0.0.2.58**. The shipped binary, both page stats panels
(scraped live from its boot log), `BUILD_COUNTER.txt`, and the bundle
README all agree on those numbers. The mission name "build 50" is
the fix round; the shipped artifact is counter 55 because the
intermediate test builds ticked too — that is the law working, not
drift.

## Proof (from the shipped zip, fresh extraction, COOP/COEP server)

Package: `~/workspace/user/files/LithoKraft-Full-Test-2026-10-19-1409-EDT.zip`
(6,076,310 B; SHA-256
`2ba93d0da8538cbea416aa1dff0f00a453b0c3e3d789e546313b8255b1a332bf`;
flat 6 files, same shape as build 49).

- **GL page, interactive, same-page CDP screenshots** (new harness
  `wasm/harness/probe_present.js` — navigates once; the older
  `interactive.js` re-navigates for its shots, which is why its
  frames could never be trusted as presentation evidence):
  menu frame = the launch menu (LITHOKRAFT / Play / Settings / Exit)
  over the live wafer vista; play frame = first-person world + HUD +
  hotbar. Pixel audit: menu mean (62.6, 71.5, 93.6), ~99% non-black
  samples (black baseline: mean ≈ (3–7) and 53,412 B per menu shot;
  the presented menu shots run 367–411 KB).
  (`harness/zip55gl-menu.png`, `zip55gl-play.png`)
- **Input from the zip**: synthesized Enter → `menu: play -
  creative/testing`; click → `input: pointer lock engaged`; sim TPS
  **253.5–256.9/256** in play vs 0.0 at menu. (`harness/zip55gl-log.txt`)
- **Software page**: same probe — menu presents over the world, live
  DEBUG strip, play presents. (`harness/probe55sw-*.png`, staged build)
- **Bench regression (staged final binary)**: GL bench finished,
  frame audit 3,370 sampled colors / 61.1% non-black; software bench
  3,904 colors / 66.3% non-black; console shows only the known local
  favicon 404 — **zero GL errors**.
- **Gates on the final binary**: native selftest **ALL OK**; native
  demo-test cities ON `cb32c824f8b3ea98` ×4 arms, cities OFF
  `e015e51cf1559089` ×4 arms; WASM (Node) demo-test on the shipped
  binary: ON `cb32…` ×4, OFF `e015…` ×4. Sole FAIL everywhere is the
  pre-existing stale pin `5041cf1795c33a8c` (repin still awaits JD's
  signature; not a gate). Sandbox frame times remain SwiftShader
  numbers (seconds-scale hitches while streaming) — workstation
  evidence only, not JD-hardware numbers.

## Harness note for the next crew

`interactive.js`'s click-then-Enter stage can still time out on
`menu: play`: clicking empty menu space clears the menu's keyboard
focus (build-49 addendum, native-identical semantics), so the Enter
goes nowhere. The zip run above hit exactly that; the probe's
Enter-first sequence (the page's own documented order) is the
reliable input proof. Not a game defect.

---

## Successor verification (2026-10-19, ~14:41 EDT onward)

The build-50/55 crew's final handoff was lost to a runtime failure (a
background exec disappeared at ~14:40 EDT) **after** the work above was
complete: the addendum, the package, and the zip proofs were all on
disk by 14:19 EDT. A successor crew independently verified the
delivered artifact rather than rebuilding:

- Package `LithoKraft-Full-Test-2026-10-19-1409-EDT.zip` SHA-256
  re-computed: `2ba93d0da8538cbea416aa1dff0f00a453b0c3e3d789e546313b8255b1a332bf`
  — matches the addendum; flat 6 files; the extracted `.wasm` is
  md5-identical to `build-wasm2/LithoKraft.wasm`.
- Proof screenshots re-inspected directly: `zip55gl-menu.png` shows
  the launch menu presented over the live wafer vista (stats panel:
  Alpha 0.0.2.58, counter 0.0.0.0.0.0.0.55); `zip55gl-play.png` shows
  first-person play with HUD, pointer lock engaged. Log lines confirm
  `web present: interactive loop via emscripten_set_main_loop`,
  `menu: play`, TPS 253.5–256.9/256; console carries only the known
  favicon 404 (zero GL errors).
- Source: the fix is present in `game/LithoKraft/src/app/main.cpp`
  (`lkStepFrame`/`lkShutdownTail`/`emscripten_set_main_loop_arg`,
  dispatch at the `__EMSCRIPTEN__` seam; backup
  `main.cpp.orig-2026-10-19-build50`). `window_web_legacy.cpp` also
  carries a build50 backup, but its shipped diff is comment-only —
  the code fix is main.cpp alone, as the addendum states.
- Gate spot-check re-run independently (Node demo-test, shipped
  binary, cities ON): `cb32c824f8b3ea98` in all four arms,
  dropped=0, violations=0; the sole [FAIL] is the pre-existing stale
  pin `5041cf1795c33a8c` (repin still awaits JD's signature).
  Output: `wasm/harness/successor-node55-on.txt`.
- Counters: no successor tick — `BUILD_COUNTER.txt` stands at
  0.0.0.0.0.0.0.55 / Alpha 0.0.2.58, as shipped. The artifact is
  **adopted as-is**; JD's upload + personal test remain the final
  arbiter on his GPU/itch, as with build 49.

---

# Addendum — build 58 pacing pass (2026-10-19, Elfie)

JD's live play test of build 55 on itch (his Acer Nitro 5, real GPU) put
the GL page at frame 55.96 ms avg (max 117), FPS 18 at the standard
opening vista (seed 20261019, render radius 20, sim radius 8) while the
sim ran healthy (TPS 256.2/256, tick 0.606 ms, budget 16%, dropped 0).
His first suspect was anti-aliasing; he also asked whether the itch
embed itself costs time, and what translation layer could absolve the
desktop-vs-web draw-call discrepancy. This pass profiles first, then
ships the cheap wins behind URL switches. Build identity: engine
counter **0.0.0.0.0.0.0.58**, game **Alpha 0.0.2.61** (three ticks past
55: the pacing build, the native gate build, the final staged build).

## What anti-aliasing was actually doing

Nothing. The shipped GL context requests NO antialiasing:
`engine/src/platform/window_web_legacy.cpp` created the WebGL2 context
with `attrs.antialias = EM_FALSE`, and the GL33 renderer draws straight
into the default framebuffer — no MSAA renderbuffer, no resolve pass,
no `render.aa` engagement anywhere in the GL path (the software
presenter's context is a plain blit, where AA does not apply). The
suspicion was reasonable — unexplained frame cost at fixed sim cost
smells like a resolve — but AA was already off, so there was nothing to
save by turning it off. A `?aa=` dial was added anyway (below) so the
cost of turning it ON is measured: on this bench it is ≈ +46% frame
time (software resolve under SwiftShader; real GPUs pay far less).

## Where the frame time goes (bench evidence)

Bench = this workstation's headless Chromium (SwiftShader software GL,
2 shared vCPUs) — **bench numbers, not JD's GPU**. Method notes, all
earned the hard way:

- The build's own interactive perf line reads fps=0/frame=0 in play (a
  readout quirk also present in build 55's shipped probe logs); the F3
  overlay's separate feed reads correctly (it produced JD's itch
  numbers and the overlay samples below). Frame truth here comes from
  CDP screencast compositor timestamps over fixed 90 s windows after a
  fixed settle (240 s at r20), at JD's posture: stationary in play,
  pointer locked, stream state reported per run.
- `--bench` mode on this bench is streaming-bound to pathology
  (multi-second upload stalls; 17 frames in 120 s at r20) and does not
  reproduce the interactive vista. Its bucket split still names the
  transient cost: mesh/texture **upload** bursts (window averages:
  upload 1,108–1,637 ms vs render 319–1,192 ms, update 40–67 ms,
  present ≈ 0.003 ms; in the one fully-healthy bench second: update
  164.6 / upload 248.6 / render 7.7 ms). Presentation from the game
  thread is ≈ free (async compositor). Sim tick medians in play across
  every run below: **1.08–2.64 ms at TPS ≈ 256** — the sim is never
  the problem, on either build.

Presented-frame interval medians (ms; lower is better):

| config | build 55 (shipped) | build 58 |
|---|---|---|
| r20, AA off (default) | 13,020 (n=5 int., 1,398 meshes, tick 1.89) | **6,172 pooled** (n=26; runs 5,909 / 6,212; ~1,610 meshes; tick 1.26 / 1.81) |
| r20, AA on (?aa=on) | — (no dial) | 9,043 (n=8, 1,413 meshes, tick 1.08) |
| r16 | 10,393 (n=6, 1,173 meshes, tick 2.64) | 5,541 (n=15, 1,358 meshes, tick 1.16) |
| r12 | 5,251 (n=15, 1,310 meshes, tick 1.88) | 5,242 (n=15, 1,333 meshes, tick 1.51) |

Plus one long-settle build-55 sample: after 660 s the F3 overlay read
frame 8,436.95 ms at 2,457 meshes streamed. Reading the table honestly:
(a) build 58's r20 median is ≈ 2× better than build 55's at a HIGHER
streamed-mesh count — consistent with the uniform-upload dedup (below)
removing ~4 proxied calls per drawn mesh on a bench where proxy hops
are expensive; treat the exact factor as bench-grade, not a promise.
(b) Radius helps both builds, with the caveat that on this bench part
of the win is the stream finishing sooner (fewer upload bursts inside
the window), not just fewer draws; at r12 both builds converge
(~5.25 s) — a bench floor set by raster + contention, not by the game.
(c) AA on costs ≈ +46% here (9,043 vs 6,172 pooled).

## The embed question (JD's addendum: our code vs itch's hosting)

The same shipped gl.html ran (a) top-level from a local server and (b)
inside a cross-origin iframe approximating itch's player (separate
origin; COOP same-origin + COEP require-corp + CORP cross-origin on the
game origin; itch's allow-list on the frame). Inside the iframe the
game booted, entered play, and stayed **crossOriginIsolated with
SharedArrayBuffer available** — no threading/SAB degradation — and the
sim ran identically (TPS ~254–256, tick ~1.1–1.4 ms, dropped 0).
A clean frame-cadence A/B could NOT be honestly extracted on this
bench: the screencast channel that measures presented frames top-level
returned no frames for the out-of-process iframed page (a measurement
limit of headless OOPIF, not a game result), and the uncontrolled
player posture diverged between runs. What the evidence supports:
nothing in the embed blocked or degraded the game's threading or sim,
and JD's own itch test presented at 55.96 ms inside itch's real embed.
If a platform delta exists on his GPU, the clean test is his: the same
build opened via its direct game URL (top-level) vs inside the itch
page, F3 overlay in both — this bench cannot reproduce his compositor.

## Extension probe (JD's question: what restores the arena?)

WebGL2 in this bench's Chromium 155 (SwiftShader/ANGLE), one line each:

- `WEBGL_multi_draw` — **present** (`multiDrawArraysWEBGL` live).
- `WEBGL_multi_draw_instanced_base_vertex_base_instance` — **absent**.
- `EXT_draw_elements_base_vertex` / any base-vertex path — **absent**
  (neither EXT_ nor OES_ variant among the 30 extensions).
- WebGPU (`navigator.gpu`) — **present** (object exists; presence only).

Interpretation (probe only — nothing implemented): the desktop arena
model as-is — one merged buffer + base-vertex draw per chunk — cannot
be restored on WebGL2 here; there is no base-vertex path of any kind.
But `WEBGL_multi_draw` IS present, and its `multiDrawElementsWEBGL`
takes a per-draw firstIndex offset into one shared index buffer in a
single API call — a different consolidation that needs no base-vertex:
one arena VAO, per-chunk indices baked with their vertex offset at
upload, all chunk draws as one multi-draw. That is the lead for the
structural fix (Archie/Jenny territory), recorded here as evidence.
Caveat: this is the bench Chromium's list; JD's ANGLE-on-his-GPU list
should be confirmed with the same one-line check on his machine.

## What changed (and why)

1. **`?aa=off|on|4`** (GL page): `window_web_legacy.cpp` reads the
   param on the browser main thread (the worker's own `location` is
   the worker URL — the `?debuginput` trap) and sets the WebGL context
   `antialias` attribute. WebGL's attribute is boolean (ANGLE's
   implicit MSAA is conventionally 4×), so `on` and `4` land in the
   same place. Default = off = shipped behavior. Boot log states the
   outcome; verified live: `window: WebGL antialias off (?aa=off)` /
   `window: WebGL antialias requested (?aa=on)`.
2. **`?radius=N`** (4–32, default 20; both pages): passes
   `--stream-radius N` in bench and interactive arg lists; the stats
   mode cell shows `radius N · aa X` in use.
3. **GL33 renderer, classic chunk loop** (`gl33_renderer.cpp`): the
   four per-mesh ground-effect uniform uploads (pulse P0/P1, trace
   P0/P1) are skipped when bit-identical to the value already set —
   most meshes share them. Re-setting an identical uniform is a pure
   state no-op, so GL state after every mesh is bit-exact what the old
   sequence left: pixels cannot move, the sim is untouched. The
   per-frame cull-decision cache also reserves its buckets once
   instead of rehashing mid-frame. (uOffset still uploads per mesh;
   VAO bind + draw per visible mesh remain — the structural fix is
   the multi-draw arena above, deliberately not smuggled into a
   pacing pass.)

Untouched by design: sim code, the build-55 present fix
(`emscripten_set_main_loop` interactive path), the DOM input transport.
Backups: `.orig-2026-10-19-pacing` beside every edited source/page.

## Gates (build 58 binary, re-read from disk logs)

- Native (new binary): **selftest ALL OK**; demo-test cities ON hash
  **cb32c824f8b3ea98**, OFF **e015e51cf1559089**.
- WASM (Node smoke, final staged binary): demo ON **cb32c824f8b3ea98**
  (×4 mentions), OFF **e015e51cf1559089** (×4). The ONLY failing check
  in every arm is the known stale in-test pin
  `5041cf1795c33a8c` — unchanged, awaiting JD's repin signature.
- Node-smoke SELFTEST caveat, stated plainly: in this runner the
  shipped build-55 binary and build 58 produce a **byte-identical**
  failure profile (10 checks: 2 audio, 8 assets/SVG/provenance, then a
  trailing abort) — an environment property of the Node runner here,
  not a regression; the native selftest is fully green and the
  in-browser boot is verified below.

## Verification from the shipped zip

Zip extracted fresh, served with COOP/COEP, both pages exercised:
software page — menu presents (screenshot audit), `menu: play` after
Enter, perf lines live, only console item the known favicon 404.
GL page (Enter-first driver; the older `interactive.js` click-first
order timed out at stage 1 — the known flaky order from build 49,
reported rather than hidden): menu presents, Enter → play, click →
`input: pointer lock engaged`, `?debuginput=1` echoes the real key
(`input> KeyDown W (code KeyW)`), and holding W walked the player
into the Capacitor Can at spawn (overlay state `#42 257 254 28`,
look target "Capacitor Can") — screenshot
`wasm/harness/pacing/results/zip58gl-input2.png`. Stats panel reads
Alpha 0.0.2.61 / counter 0.0.0.0.0.0.0.58 / `radius 20 · aa off`.

## Honest limits

- Every frame number above is SwiftShader-on-2-vCPU: the sim thread
  alone wants ~half the bench at 256 TPS, and SwiftShader rasterizes
  on the same cores. Absolute values (seconds per frame) say nothing
  about JD's Nitro 5; the portable signals are the rankings (dedup
  direction, radius response, AA cost sign) and the source-level AA
  verdict. JD's live re-test is the arbiter.
- The dedup win is bounded: up to 4 of ~7 proxied calls per drawn
  mesh. If JD's 56 ms is raster/fill-bound on his GPU rather than
  submission-bound, radius is the lever that moves it today, and
  multi-draw arena is the structural answer (probe evidence above).

Artifact: `~/workspace/user/files/LithoKraft-Full-Test-2026-10-19-1954-EDT.zip`
(6,077,602 B, SHA-256
7eefdb077e28f0f8f817d28c192a56e62108d4b6efb809b9e8a557b2a0da3a39).
Counters after this pass: `BUILD_COUNTER.txt` 0.0.0.0.0.0.0.58 /
`build_counter.txt` 0.0.2.61 — the zip's binary is the last build;
no build of any kind followed it.

# Addendum — build 62 web-seam + bit-ops pass (2026-10-20, Elfie)

JD asked for the approved web-seam design (Web-Seam-Design-2026-10-20)
and a bit-ops sweep in one dispatch: implement → review → fix → verify,
twice. The run was dispatched against counter 58 with "tick to 59" as
the target; the rolling counter ticks on EVERY build
(`tools/build_counter.py`, ledger `BUILD_COUNTER.log`), and this run's
own gate builds consumed the ticks — 59 and 60 by the first native
gate builds (2026-10-20T04:56/04:57Z), 61 by a WASM `all`-target build
that failed on a pre-existing target (below), 62 by the final staged
build. Build identity: engine counter **0.0.0.0.0.0.0.62**, game
**Alpha 0.0.3.0** (the game counter's third wheel rolled 2 → 3 at 64).
The zip's binary is the 62 build; no build of any kind followed it.

## Work item A1 — memory-mapped input (landed)

`engine/src/platform/window_web_legacy.cpp`: a fixed shared-memory
input ring (512 records × 80 B, header with magic/write/read/dropped +
canvas geometry) lives in WASM linear memory. The existing DOM
listeners write binary records into it with Atomics when the page is
cross-origin isolated (`input: memory-mapped ring LIVE`); `poll()` —
inside the per-frame input pump — drains it at frame boundaries, so
input stays quantized to the step pipeline. The legacy string queue
remains byte-for-byte and carries automatically until the first ring
record lands, and permanently where SharedArrayBuffer is unavailable.
Both transports feed ONE conversion core (the old handlers refactored
into `handle*Vals` cores with string adapters), so event semantics —
key map, consume rules, echoes, timestamps at conversion — exist
exactly once. Full-ring drops are counted in the header and logged
once; an index-desync resync guard is present (unreachable under the
SPSC protocol). Sim code and native paths untouched.

## Work item A2 — thin imports, WASI-shaped (landed)

New `engine/src/render/gl33/lk_webio.js`, linked via `--js-library`
from `engine/src/render/CMakeLists.txt` (Emscripten only): one verb,
`lkio_present` — fd-first (fd 0 = canvas present target), then the GL
names created at init, the sampler uniform name, and the texel
pointer; the whole viewport/upload/error-check/program/uniform/draw
sequence runs in ONE host crossing against the stock GL registry.
Returns 0 on success, the GL error code (nothing drawn) on upload
failure, −1 when it cannot run. `software_presenter_webgl.cpp` engages
it by default; `?lkthin=off` restores the classic per-call path, and
any non-zero return falls through to the classic path, which keeps
owning the BGRA-demote policy. The same fd-shaped surface is the
documented seam a future WASI runtime implements unchanged (the
"third fusion" experiment, browser shims today).

## Work item B — bit-ops sweep (landed, differential-proven)

New shared header `engine/src/core/include/litholattice/core/bitops.h`
(header-only, constexpr, always-inline; visible tree-wide through
`litholattice_core` with no CMake change): masks, rotl/rotr (k=0
defined; WASM lowers to native `i64.rotl`), SplitMix64 mixer, FNV-1a
step, field extract/insert, and the two-word spanning pair. Adopted
call sites, each proven bit-exact against a verbatim copy of the
original expression in `tools/bitops_diff_test.cpp` (standalone, not
wired into ctest): `procgen::mix64`, `procgen::nameHash`,
`Rng::rotl`, `sim::detMix64`, `DeterministicScheduler::hashBytes`
(step spelling only — the loop is never re-associated), the voxel
palette's `readPacked`/`writePacked` (including the original's
unmasked high-word spill quirk; the hi word is still loaded/stored
only on spill), and the StoredQuad AO pack/expand in
`software_renderer.cpp`. Proof scale: 200k random mixer inputs, 14
palette widths × 4,000 writes with full-buffer byte comparison,
AO expand exhaustive over 256×256×4, 100k random extract/insert.
Result: **ALL OK**. Micro-bench (this workstation, 2 vCPU, g++ -O2,
medians of 7): mix64 7.50 → 7.42 ns/op, hashBytes 136,825 → 136,816
ns per 64 KiB, readPacked (w=16) 2.256 → 2.263 ns/op — parity, stated
plainly: the win is one tested home for the tree's bit craft and
identical lowerings everywhere, not a per-call speedup. No inline
assembly shipped: for these ops the portable forms already lower to
the single instructions asm would emit, so hand asm would be
maintenance debt, not speed (the header records this rule). Held:
`PackedMaterials::writeIndex` (its low-word store does NOT mask the
index — a different expression, adoption needs a caller-domain
proof); the FNV-1a loops' re-association (hash law); CRC-32 slicing
and the storage codecs (save-path scope, separate pass);
`interleave3` in `subdiv/micro_table.h` — its own header documents it
wrong for inputs ≥ 1024, and any fix moves hashes: **JD's call**.

## Gates (final binaries, re-read from disk logs)

- Native: **selftest ALL OK**; demo-test cities ON hash
  **cb32c824f8b3ea98**, OFF **e015e51cf1559089** (demo record proof
  verifies: legacy replay bit-identical, handshake replay
  bit-identical); ctest **47/48**, sole failure `engine_systerian`
  (environmental, missing reference SVGs — baseline).
- The ONLY failing check in every demo arm, native and Node, is the
  known stale in-test pin `5041cf1795c33a8c` — unchanged, awaiting
  JD's repin signature.
- WASM: configure+compile+link succeeds for the shipped target set
  (the 10 executables this build tree has always produced). One
  honest note: a full `all` build still fails at
  `litholattice_tests`, which links `litholattice_render_vk` — a
  target the browser configuration skips by design
  (`engine/CMakeLists.txt`: "neither is built for the browser").
  Pre-existing, untouched by this pass, and the shipped set never
  included it.
- Node smoke (final binary): selftest **295 PASS / 9 FAIL** — the 9
  are byte-identical to the pre-change baseline list (1 FLAC codec,
  4 litho SVG, 4 provenance; an environment property of this runner,
  as recorded for builds 55/58), followed by the known trailing abort
  from the deliberate-throw JobPool checks. Demo ON
  **cb32c824f8b3ea98** in all four arms, OFF **e015e51cf1559089** in
  all four arms, sole FAIL the stale pin in both.

## Verification from the shipped zip

Zip extracted fresh, served with COOP/COEP, driven by the tree's
headless Chromium (SwiftShader, 2 vCPU — bench numbers, not JD's GPU):

- Software page: `input: memory-mapped ring LIVE`, `thin-import
  present engaged` (SwiftShader lacks the BGRA extension, so the
  thin verb's RGBA-swizzle branch is the one exercised), menu →
  Enter → play presents the real wafer world (screenshot audit:
  `wasm/harness/probe59-sw-play.png`; stats panel reads Alpha 0.0.3.0
  / counter 0.0.0.0.0.0.0.62). Ring proof is direct: header indices
  before/during/after a W press read w=3 r=3 → w=4 r=4 (KeyDown) →
  w=5 r=5 (KeyUp), dropped=0, and the conversion echo `input>
  KeyDown W (code KeyW)` appears — the event travelled the ring and
  the C++ drain consumed it.
- Software page `?lkthin=off`: `thin-import present disabled by
  ?lkthin=off (classic path)`; boot, menu, play, ring input all
  verified as above.
- GL page: ring LIVE, menu → play; a first probe sampled the ring
  0.7 s after the W press during streaming-heavy seconds-long
  SwiftShader frames and caught the write index ahead of the read
  index with no echo yet — reported here rather than hidden; the
  follow-up probe with a 30 s window (`probe59-glwait.js`) shows the
  KeyDown drained and echoed within 1.5 s and KeyUp likewise
  (w=5 r=5). Frame-boundary drain latency on a software-GL bench,
  not a stall.

## Honest limits

- The WASI half of the third fusion is a surface, not a runtime: no
  WASI host was available in this environment to execute the verbs;
  the fd-shape is documented in `lk_webio.js` and the presenter for
  the later runtime test.
- Ring input is verified in Chromium/SwiftShader on this workstation;
  JD's live play test on his hardware remains the arbiter, as with
  every build.
- Review-pass fix worth naming: the ring's desync and overflow
  log-once flags were originally one shared flag (either event would
  have suppressed the other's log); split before shipping.

Artifact: `~/workspace/user/files/LithoKraft-Full-Test-2026-10-20-0129-EDT.zip`
(6,080,491 B, SHA-256
6458003be30bf3a3608982e3c7f8b1450634f21995f019cdeb907b3e00a66030).
Counters after this pass: `BUILD_COUNTER.txt` 0.0.0.0.0.0.0.62 /
`build_counter.txt` 0.0.3.0 — the zip's binary is the last build;
no build of any kind followed it.

# Addendum — build 66 drain-cadence fix + pointer-lock rider + WASI drop (2026-10-20)

Dispatched after JD's live test of build 62 on his own GPU (GL page
under 30 FPS, controls felt sluggish in his Chrome; his Edge control
test of the same build "works very well" — the sluggishness reads as
browser-side, not a seam regression). JD's ruling 2026-10-20 02:39:
keep all three fusion legs; fix the drain to step boundaries first.
Follow-on ruling 02:51: drop the WASI leg — simplify the thin-import
surface to plain thin imports. Counter history of the run: builds
63/64/65 were gate builds (63 also reproduced the pre-existing
`litholattice_tests` → `render_vk` link failure of the `all` target,
unchanged); the counter rolled to 0.0.0.0.0.0.1.0 at build 64; the
shipped binary is the final build, 66 (0.0.0.0.0.0.1.2 / Alpha 0.0.3.4).

## The cadence change (one file: engine/src/platform/window_web_legacy.cpp)

Root cause, restated precisely: build 62's ring was drained by
`poll()`, which the web main loop calls ONCE PER PRESENTED FRAME —
under SwiftShader GL frames of ~3.4 s (and under any real streaming
stall), an input record waited for the next frame no matter how fast
the sim stepped. Two halves landed:

1. **Step-cadence drain.** A dedicated drain thread (started in the
   backend constructor when the ring is live) consumes the ring on
   the 256 Hz web step clock — one drain pass per step period,
   resyncing after stalls instead of bursting; worst-case transport
   latency is one step period (~3.9 ms) plus conversion, independent
   of presented frames. A final drain runs at thread stop. If thread
   creation fails, `poll()` falls back to the build-62 per-frame
   drain verbatim. Boot log: `input: mapped ring drain at STEP
   cadence (256/s, dedicated thread; frames no longer gate input)`.
   Conversions from the drain thread and the legacy queue drain are
   serialized by one mutex (the queue's bridge hop stays outside it).
   Honest scope note: the game samples ONE LocomotionInput per
   presented frame and applies it across that frame's steps — that
   application layer is the shared InputSystem, identical on the
   native GLFW path; this fix removes the transport's frame-quantized
   wait and the timestamp fiction in front of it, which is what the
   seam owns.
2. **Event-time timestamps.** Ring records (format v2) carry the
   event time in words 16/17 (float64 `performance.now()`
   milliseconds, written by the JS listener at event time; 18/19 stay
   reserved-zero; old zero stamps fall back to conversion time).
   Conversion stamps each `InputEvent` with its own event time, so
   events distribute across steps by when they happened. The legacy
   string-queue path passes conversion time exactly as before —
   behavior unchanged. The sim consumes `InputEvent.time` only for
   ordering/recording (verified), so no hash can move.

## Pointer-lock rider (landed)

JD's build-62 console dump showed Chrome's uncaught
`SecurityError: Pointer lock cannot be acquired immediately after
the user has exited the lock.` The mousedown re-lock now (a) skips
inside the ~1.25 s post-exit cooldown (the page's pointerlockchange
handler stamps the exit), and (b) always swallows the returned
promise's rejection; `Window::setCursorCaptured` likewise suppresses
its frame-loop re-request for 1.3 s after a release/error. Bench: a
click → lock → exit → immediate re-click cycle on both pages
produced zero pageerrors and zero SecurityError console lines, and
the lock re-engaged.

## WASI drop (JD ruling 2026-10-20 02:51)

The thin-import surface is plain thin imports now: the fd-shaped
convention and its documentation are removed from `lk_webio.js`, the
presenter declaration, the render CMake comment, and this package's
README; `lkio_present`'s first argument is a plain target index
(0 = canvas). Functionally identical — the shaping carried no
runtime cost; the drop is a simplification ruling, not a perf fix.
(The only "wasi" string left in the shipped glue is inside
Emscripten's stock runtime internals, not the seam.)

## Gates (verbatim outcomes)

- Native selftest: `ALL SELFTESTS: OK`.
- Native demo: cities ON `hash=cb32c824f8b3ea98`, OFF
  `hash=e015e51cf1559089` in all arms; proof lines PASS (recorded
  session stepped 2048 ticks; every event applied exactly once, none
  dropped/misordered; tamper cases rejected); sole FAIL is the known
  stale pin `5041cf1795c33a8c` (awaits JD's repin signature).
- Native ctest: 47/48 — sole failure `engine_systerian` (missing
  reference SVGs, environmental). One flake logged: the first ctest
  run of the night also failed `lithokraft_selftest`; it passed on
  isolated rerun and again in the full pass-2 rerun (47/48).
- WASM: `lithokraft` target configures/compiles/links; the shipped
  set built. (`litholattice_tests` still fails pre-existing on the
  missing `litholattice/render_vk/sprite_vertices.h` — the browser
  configuration skips render_vk by design; disclosed since build 62.)
- Node smoke (shipped binary): selftest **295 PASS / 9 FAIL**,
  failure list byte-identical to the build-62 baseline (1 FLAC,
  4 litho SVG, 4 provenance). One flake logged: a first run showed
  294/10 with an audio "streaming" check flipped under build load;
  the quiet rerun reproduced 295/9 exactly. Demo ON/OFF hashes hold
  in all four arms each, sole FAIL the stale pin.
- Staged `LithoKraft.wasm` md5-identical between `build-wasm2/` and
  `wasm/full/` (`0ac5f6fc25b5d1764b5887888b0064a2`).

## Verification from the shipped zip

Zip extracted fresh, served with COOP/COEP, driven by the tree's
headless Chromium (SwiftShader, 2 vCPU — bench numbers, not JD's
GPU), probe `wasm/harness/probe63.js`:

- Both pages: `memory-mapped ring LIVE`, `mapped ring drain at STEP
  cadence`, menu → play, `?debuginput=1` echo `KeyDown W` seen,
  `dropped=0`.
- **Drain latency (the headline):** on the GL page at a presented
  rate of 0.29 fps (frames ~3.4 s apart), a dispatched keydown was
  written to the ring 9 ms after dispatch and **consumed 19 ms after
  dispatch** — versus build 62's ~1.5 s frame-gated echo in the same
  environment. The drain now tracks the step clock, not the frame
  clock; SwiftShader frame times no longer mask or gate it.
- Thin on/off (JD's 2026-10-20 02:40 measurement note): `lkio_present`
  exists only in the software page's presenter — the GL page never
  calls it, and `?lkthin=off` there is a no-op (0.29 vs 0.27 fps,
  drain 19 vs 24 ms — noise). On the software page, thin on vs off:
  20.73 vs 22.50 presented fps and equivalent drain — within bench
  noise on this workstation; **no evidence the thin imports
  contribute to sluggishness on either page.**
- Pointer-lock cycle (both pages): no uncaught errors; lock
  re-engages after the guarded window (see rider above).

## Package

`~/workspace/user/files/LithoKraft-Full-Test-2026-10-20-0306-EDT.zip`
(6,081,600 B, SHA-256
dd8d5a061a8d8e5b6398e8706ca7e527ab29de32e003890c3832880430e4ee49).
Counters after this pass: `BUILD_COUNTER.txt` 0.0.0.0.0.0.1.2 /
`build_counter.txt` 0.0.3.4 — the zip's binary is the last build;
no build of any kind followed it.

Context recorded, NOT worked in this run (JD 2026-10-20): his field
verdict praises the software renderer as exceptionally efficient;
the GL page improved after his Chrome reset (~38–48 FPS in flight,
TPS ~256) with hitching remaining on his rig; the registrar's read
of his pasted GL log attributes the worst hitch cluster to a
pointer-lock/focus-loss interval plus streamer border thrash
(repeated evict/re-add of the same far columns along chunk edges),
and notes the chunk_pbr shader failing on WebGL2 (`samplerBuffer`
reserved word; PBR path disabled, classic path carries).
