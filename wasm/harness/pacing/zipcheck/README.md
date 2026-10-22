# LithoKraft — Full Test Build (WASM, 2026-10-19, build 0.0.0.0.0.0.0.58)

Build identity: game release **Alpha 0.0.2.61**, engine rolling build
counter **0.0.0.0.0.0.0.58** — both are printed on each page's stats panel,
scraped live from the binary's own boot log.

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

## Radii (JD's web-build ruling, 2026-10-19)

Both pages pass **`--stream-radius 20`** by default: chunk render/stream
distance **20 chunks** around the player/camera. The sim (ticking) radius
stays **8 chunks** (`tickRadius: 8` in the bundled `config/settings.json`,
unchanged — determinism is untouched by either value). Override the
render radius per page load with **`?radius=N`** (4–32), e.g.
`gl.html?radius=16`; the stats panel's mode cell shows the radius in use.

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
- First load downloads ~12.8 MB (JS + WASM + asset package).

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

`index.html`, `gl.html`, `LithoKraft.js`, `LithoKraft.wasm`,
`LithoKraft.data` (the asset package: engine assets with ES 3.00 shaders,
the game's real packs/models/textures, scripts, config).

Full build/wiring notes and verification evidence: `wasm/FULL-TEST-REPORT.md`
in the source tree (not shipped in this bundle).
