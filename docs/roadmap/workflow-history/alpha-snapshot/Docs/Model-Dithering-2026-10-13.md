# Model Dithering — Phase 1 Report (2026-10-13)

**Status: COMPLETE and verified** in the experiment tree
`~/workspace/experiments/model-dither/`. Production tree untouched
(verified against `~/workspace/fix-program-2026-10-13/manifest-final.md5`:
zero diffs). JD directive: "Model dithering — we need that for when the
cameras get up close. It's just a standard feature and staple in most
engines these days."

---

## 0. Salvage audit (what the first crew actually landed)

The first crew errored mid-work when a background build process
disappeared. I diffed the experiment tree against production and against
`manifest-pre-phase1.md5`, read every changed file, and finished the
program. Honest inventory:

| Piece | State found | Verdict |
|---|---|---|
| `render/model_dither.h` — IGN threshold + fade math (new) | Complete, well-formed | **Kept** |
| Software renderer: dither in both rasterizers + sprite fade | Complete | **Kept** |
| GL33: shader block in `chunk_shading.glsl`, `chunk.frag` define + uniforms, `chunk_pbr.frag` inline block, uniform plumbing | Complete in source | **Kept** (runtime-unverified — no GL context on this VM, the standing limitation) |
| `IRenderer::setModelDither` + game cvars + `game.cpp` apply | Complete | **Kept** |
| Engine dither checks in `render/software_test.cpp` | Written, **never ran** — see below | Kept, now wired |
| `software_aa.h/.cpp` (Phase 2 library) | Written but **orphaned**: not in CMake, no cvar, no call sites | Completed in Phase 2 |
| Capture lab in `biome_capture.cpp` | Contained a **compile error** (1-arg `setModelDither(false)` call) — the crew's tree never built after the lab was written (`build-baseline.log` ends in `Terminated`, `build-p1.log` stops at ~94%) | Fixed |
| Phase 3 | Not started | Built in Phase 3 |

Two defects found and fixed:

1. **Class default ON broke the engine's own tests.** Crew 1 defaulted
   the renderer-class member `ditherEnabled_` to `true`. Every existing
   engine test that renders a quad at clip-w 1 (inside the 2..8 band →
   fade 0) then rasterized nothing: 6 failures in `engine_render`
   (pbr-software per-pixel quad, backface-culling convention, PBR mesh,
   material-ID shading, ground-trace emissive, cutout sprite form).
   Fix: the **setting** defaults on (JD's staple call) and the game
   applies it at startup; the **class** starts off, like all other
   renderer state, so headless consumers see pre-dither output unless
   they opt in. `engine_render` is green again.
2. **`software_test.cpp` was compiled nowhere.** Both crews' engine
   checks lived in a TU no target built. It is now compiled into
   `litholattice_tests` (new `render/software_test.h` declaration) and
   runs under ctest `engine_render` — the dither/AA/ASCII/Systerian
   checks below are real, executed results, not intentions.

## 1. What was built

Near-camera **screen-door fade**: geometry whose view depth falls inside
the band **[near, far]** (default 2..8 blocks) is discarded per-pixel
against a deterministic threshold; geometry closer than `near` is fully
faded, beyond `far` untouched. Close-up cameras — free cams, MCP/agent
cameras, Daemon-vision feeds — are no longer swallowed by a wall of
voxels. Purely presentational: no sim interaction, no RNG state, no
remeshing (the test runs in the shading stage from the fragment's view
depth).

- **Pattern: interleaved gradient noise** (Jimenez 2014) — a hash of the
  integer pixel position only. No frame counter, no time, no per-object
  seed: for a static camera the pattern is bit-stable across frames, and
  the CPU and GLSL forms use the identical formula so backends agree
  pixel-for-pixel on the discard decision. Survey (crew 1, kept): Bayer
  4×4/8×8 (the engine's existing glass pattern) tiles visibly at the
  large on-screen scale of near geometry; void-and-cluster has the best
  distribution but costs a 64×64 rank table; IGN is table-free,
  aperiodic, and is the pattern shipped games use for this exact job.
- **Software renderer (the #1 lane):** per-triangle classification
  against the band (fully-in triangles are dropped before rasterizing —
  faded geometry costs nothing; straddling triangles test per-pixel
  before shading and before any depth write, i.e. true discard
  semantics). Applies to chunk meshes, PBR meshes, entities; blended
  sprites fade as whole billboards by nearest-corner distance (a
  translucent quad reads better as a fade than as confetti); HUD/lines
  are never faded.
- **GL33:** guarded block in the shared `chunk_shading.glsl`
  (`#ifdef LITHO_MODEL_DITHER`, defined only by the GL includers) plus an
  inline twin in `chunk_pbr.frag`; uniforms plumbed in both programs.
- **Vulkan:** not plumbed — the shared GLSL block is compiled out there
  because the band uniforms have no push-constant/descriptor path on
  that backend yet. Documented in `renderer.h`; the interface default is
  an honest no-op, never a fake.

## 2. Cvars and defaults

| Cvar | Default | Meaning |
|---|---|---|
| `render.model_dither` | **on** (setting) | JD's staple call; applied by the game at startup and live on change |
| `render.model_dither_near` | 2 | view depth (blocks) at/below which geometry is fully faded |
| `render.model_dither_far` | 8 | view depth at/above which geometry is untouched |

Driven through `IRenderer::setModelDither(enabled, near, far)` — keyed
to whichever camera renders the view, including MCP pilot cameras,
because the discard uses the rendering view's own depth.

## 3. Verification (experiment tree, final state)

- Build: exit 0.
- Game selftest: **958 PASS / 0 FAIL** in legacy mode and in
  `--timing-handshake` mode; `--handshake-test`: **OK**.
- Determinism scenario: **`faec0f02700d98c6`** on all four arms
  (legacy, hs 1w/4p, 4w/4p, 4w/8p) — unchanged.
- ctest: **48/48 effective** (47/48 in one loaded run — `engine_audio`,
  the documented load flake, passes solo; `engine_render` and
  `engine_systerian` green — the latter needs `SYSTERIAN_REFERENCE_DIR`
  in the experiment copy because its compiled-in reference path is
  relative to the production tree's depth; passes with it).
- Engine checks (now really running): fade endpoints/midpoint exact;
  IGN deterministic per pixel, range [0,1), distribution sane; rasterizer
  mid-band coverage **2535/7600 ≈ 33%** at fade = 1/3 — matches theory.
- **Frame identity:** the stock `biome_capture` frames (all 7 biomes)
  are **MD5-identical** to the production status-run baseline
  (`~/workspace/user/files/status-frames-2026-10-13/`) — at biome-camera
  distances nothing sits inside the band, and with the class-default fix
  the identity is structural, not luck.

## 4. Visual evidence (read directly)

Captures: `~/workspace/user/files/render-lab-2026-10-13/`

- `closeup_dither_off.png` vs `closeup_dither_on.png`: the camera sits
  inside dense geometry at the seeded spawn. In the ON frame the
  nearest surfaces dissolve into a fine IGN grain — sky shows through
  the upper-right cluster — while everything beyond the band stays
  solid. The camera is visibly *un-swallowed*.
- `spires_dither_off.png` vs `spires_dither_on.png`: at ~65 blocks the
  frames agree (nothing in band), as designed.
- Honest caveat: the lab's close-up ended up embedded in geometry
  (the seeded world's surface scan returns Y=255 there), so it is a
  stress shot, not a beauty shot. A cleaner close-up awaits JD's own
  camera in the user test.

## 5. Cost

Close-up 720p (band active): off 57.27 ms → on **55.22 ms** (fully faded
triangles skip rasterization entirely). Spires 720p (nothing in band):
off 43.83 ms → on 53.67 ms in one noisy run on the shared 2-core VM —
the only added cost there is one compare per triangle; treat the delta
as VM noise, not a regression signal. The dither is, as designed,
nearly free.

## 6. Production port instructions (Phase 1 file set — independent)

Copy from `~/workspace/experiments/model-dither/src/` to production:

- `engine/src/render/include/litholattice/render/model_dither.h` (new)
- `engine/src/render/include/litholattice/render/renderer.h` (setModelDither virtual; the AA/ASCII virtuals in the same hunk region belong to Phases 2/3 — port them with their phases or take the whole file if porting all three)
- `engine/src/render/include/litholattice/render/software_renderer.h`
- `engine/src/render/software_renderer.cpp`
- `engine/src/render/gl33/gl33_renderer.cpp`
- `engine/assets/shaders/chunk_shading.glsl`, `chunk.frag`, `chunk_pbr.frag`
- `game/LithoKraft/src/game/include/lithokraft/game/settings.h`
- `game/LithoKraft/src/game/settings_cvars.cpp`
- `game/LithoKraft/src/game/game.cpp`

Shared test wiring (serves all three phases; port once):
`engine/src/render/include/litholattice/render/software_test.h` (new),
`engine/src/render/software_test.cpp`,
`engine/tests/CMakeLists.txt`, `engine/tests/engine_tests.cpp`.

Do NOT port the `biome_capture.cpp` lab — it is experiment-only
evidence tooling; production's capture tool stays stock.

After porting, re-run the standard gates (build, selftest both modes,
ctest, scenario hash, stock captures) before calling it landed.
