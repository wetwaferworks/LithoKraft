# Addendum — build NN pacing pass (2026-10-19, Elfie)

JD's live play test of build 55 on itch (his Acer Nitro 5, real GPU) put the
GL page at frame 55.96 ms avg (max 117), FPS 18 at the standard opening
vista (seed 20261019, render radius 20, sim radius 8) while the sim ran
healthy (TPS 256.2/256, tick 0.606 ms, budget 16%, dropped 0). His first
suspect was anti-aliasing. This pass profiles the frame, answers the AA
question from the source, and ships the cheap wins behind URL switches.

## What anti-aliasing was actually doing

Nothing. The shipped GL context requests NO antialiasing:
engine/src/platform/window_emscripten.cpp created the WebGL2 context with
`attrs.antialias = EM_FALSE`, and the GL33 renderer draws straight into
the default framebuffer — there is no MSAA renderbuffer or resolve pass
anywhere in the GL path (no render.aa engagement; the software presenter's
context is a plain blit where AA does not apply). JD's suspicion was
reasonable — unexplained frame cost at fixed sim cost smells like a
resolve — but on this build AA was already off, so there was nothing to
save by turning it off. A `?aa=on|4` switch is added anyway (below) so the
cost of turning it ON is measured and the dial exists.

## Where the frame time goes (bench evidence)

Bench = this workstation's headless Chromium (SwiftShader software GL,
2 vCPU shared) — BENCH NUMBERS, not JD's GPU. Method note: the build's
own interactive perf line reads fps=0/frame=0 in play (a readout quirk
also present in build 55's shipped probe logs; the F3 overlay's separate
feed reads correctly — it is what JD's screenshot showed). Frame truth
here comes from CDP screencast compositor timestamps over fixed windows
after a fixed settle, at JD's playtest posture (stationary at the vista).

[NUMBERS TABLE: config | runs | presented fps (pooled median interval) |
 frame interval median/p95 | sim tick median | stream meshes at window]

Bench-mode postscript: the game's --bench mode on this bench is
streaming-bound to the point of pathology (multi-second upload stalls,
17 frames in 120 s at r20) and does not reproduce the interactive vista;
the interactive screencast method above supersedes it. In bench mode the
bucket split still identifies the transient cost: mesh/texture UPLOAD
bursts (avg 1.6 s/frame windows) — the initial stream, not steady state.

Top-level vs itch-style iframe (shipped build 55, same bench):
[TABLE: top median interval / fps vs iframe; rAF cadence notes;
 crossOriginIsolated + SAB held in the iframe; verdict for Archie]

## What changed (and why)

1. `?aa=off|on|4` on gl.html → window_emscripten.cpp reads the param on
   the browser main thread and sets the WebGL context's `antialias`
   attribute accordingly. WebGL's attribute is boolean (ANGLE's implicit
   MSAA is conventionally 4x), so on and 4 land in the same place.
   Default = off = the shipped behavior. The boot log states the result
   ("window: WebGL antialias …").
2. `?radius=N` (4..32, default 20) on both pages: passes --stream-radius
   N in both bench and interactive modes; the mode cell shows the radius
   in use. 16 and 12 are the interesting rungs.
3. GL33 renderer, classic chunk loop: the four per-mesh ground-effect
   uniform uploads (pulse P0/P1, trace P0/P1) are skipped when the value
   is bit-identical to the one already set — most meshes share them.
   Re-setting an identical uniform is a pure state no-op, so the GL
   state after every mesh is bit-exact what the old sequence left:
   pixels cannot move, the sim is untouched. The per-frame cull-decision
   cache also reserves its buckets once instead of rehashing mid-frame.
   (uOffset still uploads per mesh; VAO bind + draw per visible mesh
   remain — the structural fix for that is arena/instanced drawing,
   which WebGL2's missing base-vertex draw makes a bigger project,
   noted for Archie/Jenny rather than smuggled into a pacing pass.)

Untouched by design: sim code, the build-55 present fix
(emscripten_set_main_loop interactive path), the DOM input transport.

## Gates

[table: native selftest; demo-test cities ON hash (cb32c824f8b3ea98) /
 OFF (e015e51cf1559089) native + WASM + Node; the ONLY failure anywhere
 is the known stale in-test pin 5041cf1795c33a8c, unchanged]

Interactive verification from the shipped zip of THIS build, headless
Chromium, both pages: [menu presents / Enter→play / pointer lock /
 WASD echoes / TPS in play / screenshot audit — fill from probe output]

## Honest limits

- All frame numbers above are SwiftShader-on-2-vCPU bench numbers.
  SwiftShader rasterizes on the same two cores the sim and compositor
  need, so absolute values (seconds per frame at r20) say nothing about
  JD's Nitro 5; the CONFIG RANKING (radius response, AA cost, iframe
  delta) is the portable signal. JD's live re-test is the arbiter.
- The dedup win is real but bounded: it removes up to 4 of ~7 proxied
  calls per drawn mesh. If the vista cost on real hardware is raster/
  fill rather than submission, radius is the lever that moves it, and
  the arena question is the structural one.
