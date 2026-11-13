# Software-Renderer Anti-Aliasing Suite — Phase 2 Report (2026-10-13)

**Status: COMPLETE and verified** in the experiment tree
`~/workspace/experiments/model-dither/`. Production tree untouched
(verified against the fix-program manifest: zero diffs). JD's orders:
implement ALL worthwhile techniques as selectable named modes, with
**coverage restoration as the flagship** ("the most advanced one"), so
he can review the looks himself and so the ladder covers everything
from retro hardware and basic Chromebooks to state-of-the-art CPUs
with no GPU.

**Salvage note:** crew 1 wrote the algorithm library
(`render/software_aa.cpp`, 367 lines: FXAA-class filter, CMAA-lineage
coverage restoration, SSAA resolve, mode-name plumbing) but left it
orphaned — not in CMake, no cvar, no call sites, never compiled into
anything. This phase kept their algorithms (they are sound — see the
measured behavior below), and built everything around them: build
registration, renderer integration, the `render.aa` cvar, game
plumbing, tests, and the capture/cost evidence.

---

## 1. The ladder

`render.aa` = `off | fxaa | cmaa | ssaa2x`, switchable at runtime, no
restart. All modes are presentational post/raster-output stages on the
CPU framebuffer; none touches the sim (scenario hash unchanged:
`faec0f02700d98c6`).

| Mode | Technique | Hardware class (JD's inclusion ladder) |
|---|---|---|
| `off` | none — the baseline | anything; also the reserved default until JD picks |
| `fxaa` | FXAA-class post filter (Lottes lineage, clean-room) | Chromebook / retro floor: cheapest real AA |
| `cmaa` | **Coverage restoration** — conservative morphological AA (MLAA → SMAA → CMAA2 lineage, clean-room variant) | mainstream CPU without GPU: the flagship |
| `ssaa2x` | true 2× supersampling + box resolve | strong CPU, quality-first reference |

Composition: AA runs on the finished world frame (after sprites),
before overlays/HUD, so HUD and crosshair stay at native resolution in
every mode (in `ssaa2x` renderFrame rasterizes the world at 2× and
resolves before the overlay stage). It composes with model dithering
(Phase 1) without interaction — dither discards fragments during
rasterization; AA filters the result.

**Default: `off`.** Crew 1's header already recorded the reasoning and
I kept it: JD reviews the capture sets and picks the shipped default
himself. Nothing in this phase pre-empts that call.

## 2. Coverage restoration — what "cmaa" actually does

JD picked the family; the concrete algorithm is a clean-room variant in
the CMAA2 lineage, chosen because full SMAA is a GPU-shaped pipeline
(LUT textures, three passes tuned for graphics hardware) while CMAA's
conservative philosophy — *change as little of the image as possible,
and only where a real edge lives* — is both the most advanced and the
most CPU-honest of the family. As built (`applyCmaaPost`):

1. **Edge detection** on luma steps **and** depth-buffer steps (the
   renderer hands over its NDC-z buffer), with CMAA2's conservatism: a
   monotone luma ramp on both sides of a boundary is a *texture
   gradient*, not a geometry edge, and is rejected unless the depth
   buffer vouches for a geometry step. Thin trace lines and texture
   detail survive.
2. **Coverage restoration proper:** each edge run is walked; the edge
   line's subpixel offset at the run ends is reconstructed from the
   adjacent edges (the turn signal), and the line's coverage of each
   touched pixel is integrated **analytically** — the restored coverage
   *is* the blend weight. No lookup textures.
3. Composite from the source copy.

Documented deviations from full CMAA2 (also in the code header): the
shape library is reduced to long-edge runs + end turns (no Z-pattern
special cases), and weights are analytic instead of SMAA's precomputed
area texture. An exactly axis-aligned edge has exact 0/1 coverage and
blends **nothing** — verified by a dedicated engine check.

## 3. Measured behavior (spires torture scene, 1280×720, vs `off`)

Objective pixel-change census against the `off` frame — this is the
cleanest one-line summary of each technique's character:

| Mode | Pixels changed | Mean \|Δ\| where changed | Character |
|---|---|---|---|
| fxaa | 8.25% | 29.7 | broad and gentle — softens widely |
| cmaa | **2.88%** | **63.8** | touches almost nothing, corrects hard where it acts — the conservative signature |
| ssaa2x | 20.99% | 14.1 | everything, gently — the reference |

Visual read (crops inspected directly): on voxel content most edges
are axis-aligned steps, so all three modes are *subtle* at scene
scale — the differences concentrate on diagonal silhouettes (spire
edges against sky, slab diagonals). CMAA's crop keeps texture
interiors bit-identical to `off` while smoothing those silhouettes;
FXAA's crop is a touch softer overall, including inside textures;
SSAA is uniformly the cleanest and visibly the most expensive way to
get there. JD's eyeball call stands — the captures are his to judge:
`~/workspace/user/files/render-lab-2026-10-13/spires_aa_*.png`.

## 4. Cost (post-pass ms; scene rasterization measured separately)

2-core VM, Release; rasterization of the spires scene ≈ 47–49 ms at
720p, ≈ 88–95 ms at 1080p, ≈ 45 ms at 854×480 on this machine (the VM
ceiling applies — read the *relative* costs).

| Mode | 854×480 post | 1280×720 post | 1920×1080 post |
|---|---|---|---|
| fxaa | 13.34 ms | 27.78 ms | 71.81 ms |
| cmaa | — | **14.06 ms** | **32.52 ms** |
| ssaa2x | — | render@2× 267.60 ms + resolve 55.42 ms (total frame ≈ 323 ms) | — |

Two honest surprises in the data:

- **CMAA is cheaper than the FXAA-class filter at both resolutions** —
  the FXAA path searches endpoints per edge pixel over a full-frame
  copy, while CMAA touches only detected edges. The flagship is also
  the fast one on CPU. (On GPU the ranking inverts; this ladder is a
  CPU ladder.)
- **The SSAA box resolve is slow** (55 ms for a 720p target): a scalar
  2×2 loop. It's correct and it's the quality reference, but if JD
  favors ssaa2x the resolve is the first optimization target (SIMD /
  row-pair summation). Flagged, not hidden.

At Chromebook scale (854×480), fxaa's 13.3 ms post on this throttled
VM implies a few ms on real low-end silicon — the floor rung holds.

## 5. Survey, citations, license notes

Technique survey (the first crew began this; completed here). All
implementations in the tree are **clean-room from the published
technique descriptions** — no third-party code was copied, so no
license obligations attach; sources consulted:

- **FXAA** — Timothy Lottes (NVIDIA), *FXAA 3.9*; the canonical
  shader circulates under NVIDIA's permissive copyright notice (e.g.
  the AmplifyCreations/FXAA mirror on GitHub). Structure followed:
  local-contrast gate, subpixel term, edge-direction endpoint search.
- **MLAA** — Alexander Reshetov, *Morphological Antialiasing* (2009):
  the lineage root (edge detection → pattern classification →
  blending weights by covered area).
- **SMAA** — Jimenez, Echevarria, Sousa, Gutierrez, *SMAA: Enhanced
  Subpixel Morphological Antialiasing*, Computer Graphics Forum
  (EUROGRAPHICS 2012); reference code at github.com/iryoku/smaa
  (MIT). Its area/search LUT design is the main thing our variant
  deliberately replaces with analytic coverage (LUTs are GPU-shaped).
- **CMAA / CMAA2** — Intel (Filip Strugar et al.), *Conservative
  Morphological Anti-Aliasing 2.0*; official sample at
  github.com/GameTechDev/CMAA2 (Apache License 2.0), a DX11/12
  compute-shader reference. Our variant follows its conservative
  philosophy and edge/coverage structure, re-derived for a scalar CPU
  pipeline.
- **AGG-style analytic coverage** (Anti-Grain Geometry) informed the
  rasterizer-coverage alternative; for a post-stage design the
  morphological family fit the existing framebuffer architecture
  better, so analytic coverage lives in the CMAA variant's coverage
  integrals instead.
- Dither-pattern cross-check (Phase 1): interleaved gradient noise,
  Jorge Jimenez (2014).

## 6. Verification

Standard gates, final tree state: build exit 0; game selftest
**958/0 both modes**; `--handshake-test` OK; scenario hash
`faec0f02700d98c6` ×4 arms; ctest 48/48 effective (engine_audio load
flake passes solo). No ADDED game-selftest checks in this phase.

New engine checks (in `litholattice_tests render`, all passing):
mode-name roundtrip + unknown-name rejection; **fxaa and cmaa each
soften a synthetic step edge while leaving flat interiors
bit-untouched**; **cmaa blends nothing on an exactly-aligned edge**
(conservatism proof); ssaa resolve is the exact 2×2 average (one white
sample in four → 25% gray, bit-exact).

OFF-state identity: `spires_aa_off_1280x720.png` is the untouched
raster (post = 0.00 ms), and the stock biome frames remain MD5-equal
to the production baseline (Phase 1 report, §3).

## 7. Production port instructions (Phase 2 file set — independent)

- `engine/src/render/include/litholattice/render/software_aa.h` (new)
- `engine/src/render/software_aa.cpp` (new)
- `engine/src/render/CMakeLists.txt` — add `software_aa.cpp` to
  `litholattice_render` (the `software_ascii.cpp` + systerian-link
  parts of that hunk belong to Phase 3)
- `engine/src/render/include/litholattice/render/renderer.h` —
  `setAaMode` virtual only
- `engine/src/render/include/litholattice/render/software_renderer.h`
  + `engine/src/render/software_renderer.cpp` — AA state, scratch,
  `applyAaPostPass`, renderFrame AA/SSAA stage
- `game/LithoKraft/src/game/include/lithokraft/game/settings.h` —
  `aaMode` field
- `game/LithoKraft/src/game/settings_cvars.cpp` — `render.aa` row + sync
- `game/LithoKraft/src/game/game.cpp` — apply at init + on settings change
- Shared test wiring (once for all phases): `software_test.h` (new),
  `software_test.cpp`, `engine/tests/CMakeLists.txt`,
  `engine/tests/engine_tests.cpp`

Re-run the standard gates after porting.
