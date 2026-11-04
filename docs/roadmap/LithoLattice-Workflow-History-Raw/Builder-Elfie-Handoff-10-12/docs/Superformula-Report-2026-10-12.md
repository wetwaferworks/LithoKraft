# Litho Formula Voxelizer — Core Build Report (2026-10-12)

**Status:** CORE COMPLETE. Sampler → voxelizer → gallery all landed, tested, and green.
The §5 integration forks (procgen node wiring, worldgen use, gallery curation) remain
PROPOSED pending JD's lock — the `lithoform` node spec below is design-only, nothing wired.

**Naming:** per JD's 2026-10-12 directive, the implementation is the **Litho Formula** in all
code, docs, gallery, CLI, and reports. (This report file keeps its assigned name; its
contents use "Litho Formula" throughout.) Two-tier attribution honored: source headers carry
the canonical credit (Gielis 2003 / Genicap / No Man's Sky as inspiration only — no claim
NMS uses the superformula); no Gielis/Genicap/NMS names appear in user-facing strings, CLI
help, or test output. Verified by scan — the only matches repo-wide are the header blocks.

**Clean-room:** implemented solely from the mathematical definition (proposal §2). No
third-party code referenced. Core patent expired 2020-05-10.

---

## 1. Worker 1 — Core sampler ✅

**Files created** (canonical Litho Formula header on each):
- `engine/src/procgen/include/litholattice/procgen/litho_formula.h` — public header
  (placed in the public include dir per repo convention, not `src/procgen/`, so the
  `litholattice/procgen/...` include path works for all consumers)
- `engine/src/procgen/litho_formula.cpp` — implementation; only `<cmath>`/`<algorithm>`,
  zero engine dependencies
- `engine/src/procgen/litho_formula_selftest.cpp` — self-test (Checker pattern)

**Files edited:** `engine/src/procgen/CMakeLists.txt`, `engine/tests/engine_tests.cpp`,
`engine/tests/CMakeLists.txt` (new `engine_litho_formula` ctest).

**API (final, consumed by Workers 2/3):**
```cpp
namespace litholattice::procgen::litho_formula {
struct LithoParams { double a=1.0, b=1.0, m=0.0, n1=1.0, n2=1.0, n3=1.0; };
double lithoR(double phi, const LithoParams& p);
struct Vec3 { double x=0.0, y=0.0, z=0.0; };
Vec3 lithoPoint(double theta, double phi, const LithoParams& pTheta, const LithoParams& pPhi);
int runLithoFormulaSelfTest();
}
```
Degenerate rails: |n1| clamped ≥ 1e-6 (sign preserved), |r| capped at 1e6, |a|/|b| floored at
1e-9, non-finite input → radius 0 / origin. C++17-compatible.

**Test evidence:**
- `cd ~/workspace/litholattice/engine/build && ./tests/litholattice_tests litho_formula` →
  **14/14 PASS**: circle anchor max |r−1| = 2.2e-16 over 720 angles; m-fold symmetry max dev
  1.78e-15 (m∈{3,5,6}); hand spots `r(0)=1 r(π/4)=1 r(π/2)=1`; 3D axis pins (1,0,0)/(0,1,0)/
  (0,0,1); sphere grid max ||p|−1| = 1.1e-16; degenerate sweep (n1∈{1e-9,−1e-9,0}, m=0,
  exponents ±1e9, a=b=0, NaN/Inf inputs) all finite; m=0 → constant radius 4; bitwise
  determinism.
- `ctest -R "engine_(procgen|trees|shrubs|water|litho_formula)"` → **5/5 passed**.
- Zero warnings under -Wall -Wextra in new files. Combined library (all three workers'
  code) builds clean.

**Fixes found:** none — all touched code is new; nothing existing needed repair.

**Caveats:** full default `all` build fails at a pre-existing environmental step
(FetchContent GLFW needs `X11/Xcursor/Xcursor.h`, missing on this headless VM) — same
family as the grandfathered Vulkan-headless issue, unrelated to this change.

---

## 2. Worker 2 — Voxelizer ✅

**Files created:**
- `engine/src/procgen/include/litholattice/procgen/litho_formula_voxelize.h` (+ selftest header)
- `engine/src/procgen/litho_formula_voxelize.cpp`
- `engine/src/procgen/litho_formula_voxelize_selftest.cpp` (8 checks)

**Files edited:** `engine/src/procgen/CMakeLists.txt`, `engine/src/procgen/procgen_selftest.cpp`
(hooked into existing `engine_procgen` ctest).

**API (final):**
```cpp
namespace litholattice::procgen::litho_formula {
struct VoxelResult {
  int resolution = 0;  // 0 = rejected/invalid
  std::vector<std::array<int,3>> filled;  // coords in [0,resolution), ascending (z,y,x), deterministic
};
VoxelResult voxelizeLithoShape(const LithoParams& pTheta, const LithoParams& pPhi,
                               int resolution, uint8_t material = 1);  // material reserved; use emitToOctree 3-arg
void emitToOctree(const VoxelResult& v, litholattice::subdiv::SparseOctree& tree);
void emitToOctree(const VoxelResult& v, litholattice::subdiv::SparseOctree& tree, uint8_t material);
int runLithoFormulaVoxelizeSelfTest();
}
```
Pipeline (worker's own design): uniform sampling (8/voxel θ, 4/voxel φ) → per-column
continuous min/max sample z → fill cells whose centers lie in the span → largest-6-connected-
component speckle filter. Axis mapping: formula (x,y,z) → grid (x,z,y) (polar axis = game-up
y). Resolution ∈ {8,16,32,64} else rejected with stderr diagnostic (no silent clamping).

**Test evidence:**
- `./tests/litholattice_tests procgen` → all 8 checks PASS: sampler anchor r=1 @1e-12;
  sphere volume at 64³: **N=129593, rel err 3.85%** (inside the 6% bar); watertight:
  **105015/105015** deep-interior cells filled; resolutions 16/32/64 bounded, octree at
  levels 4/5/6 with `leafCount == filled.size()`; invalid (24,0,7,100,−8) rejected; NaN
  params → empty; star (m=5) non-empty/bounded/deterministic.
- Cross-module: `litho_formula`, `subdiv`, `trees`, `shrubs`, `water`, `core`, `sim`,
  `world`, `storage` test binaries all PASS.

**Bugs found and fixed (with test evidence):**
1. Parity-fill leak: naive even-odd toggle under-counted merged shell crossings → fill
   leaked to grid edge (255 cells at d>32.5 on a r=31 sphere; N inflated to 133228).
   Fixed with a leak-proof pairing rule, then superseded by the span fill.
2. `lround` mis-quantization: boundary samples landed one cell off (62.5→63). Fixed with
   `floor` (containing cell).
3. Binary-shell volume bias: +6.8% overcount vs the 6% bar → replaced with sub-cell
   continuous-span fill → **+3.85%**.
4. Resolution clamp→reject: 24 was silently clamped to 16; now rejected with diagnostic.
5. Latent `bestLabel==-1`: all-empty filter result would have emitted all 262k cells; guarded.

**Caveat:** sphere-volume tolerance (6%) has ~1.5× headroom over the measured 3.9%
discretization error — consider pinning the exact count or widening slightly later.

---

## 3. Worker 3 — Preset gallery + bake CLI + node spec ✅

**Files created:**
- `engine/src/procgen/include/litholattice/procgen/litho_formula_gallery.h` +
  `litho_formula_gallery.cpp` — preset registry; JSON is the source of truth, code registry
  is the validating loader `loadLithoFormulaGallery(path, out, err)` (name charset,
  param presence, resolution ∈ [8,64], material ∈ [1,255], duplicate rejection).
- `engine/src/procgen/litho_formula_node_spec.md` — `lithoform` node spec (design only).
- `game/LithoKraft/assets/models/litho_formula_gallery.json` — 12-preset data file.
- `game/LithoKraft/assets/models/<12 presets>.lthm` — baked models (JD decides keep/drop).
- `game/LithoKraft/src/content/model/litho_formula_bake.{h,cpp}` — shared bake path
  (voxelize → `ModelBuilder(name, "litho-formula-gallery")` → `setCell(level…)` at
  level log2(res) → `saveModel`).
- `game/LithoKraft/src/content/model/bake_litho_formula.cpp` — CLI modeled on
  `author_models.cpp` (`bake_litho_formula [outdir] [gallery.json]`, same printf style).
- `game/LithoKraft/src/content/model/litho_formula_gallery_selftest.cpp` + bake CLI smoke test.

**Files edited:** `engine/src/procgen/CMakeLists.txt` (also removed a duplicate
`litho_formula_voxelize.cpp` entry from a mid-task CMake race with Worker 2 — merged
cleanly), `game/LithoKraft/src/content/model/CMakeLists.txt` (bake lib, CLI exe, 2 new
ctests).

**Preset table** (worker's own tuning from the mathematical definition; all res 32 / level 5):

| # | name | theta {a,b,m,n1,n2,n3} | phi {a,b,m,n1,n2,n3} | mat | filled | description |
|---|---|---|---|---|---|---|
| 1 | starfish | 1,1,5, 0.9,1.0,1.0 | 1,1,2, 2.0,2.0,2.0 | 12 | 9926 | Five-armed starfish lying flat, spiky arms from a pinched five-fold radius |
| 2 | shell | 1,1,3, 1.0,0.8,1.4 | 0.7,1,2, 1.8,1.4,2.2 | 8 | 8761 | Asymmetric whorl shell, coiled profile, flattened spiral read |
| 3 | blossom | 1,1,6, 1.4,1.2,1.2 | 1,1,2, 2.5,2.0,2.0 | 10 | 12001 | Six-petaled blossom, soft rounded petals around a domed heart |
| 4 | roundedcube | 1,1,4, 3.5,4.0,4.0 | 1,1,4, 3.5,4.0,4.0 | 2 | 24223 | Cube with fully rounded edges and gently bulged faces |
| 5 | rotor | 1,0.7,3, 1.1,1.0,1.0 | 1,1,2, 3.0,2.0,2.0 | 3 | 7049 | Three-blade rotor, asymmetric blades swept back from the hub |
| 6 | pod | 1,1,2, 2.0,2.0,2.0 | 1,1,2, 3.0,8.0,8.0 | 1 | 3773 | Vertical seed pod, prolate capsule tapering to pointy poles |
| 7 | urchin | 1,1,14, 0.7,1.4,1.4 | 1,1,6, 0.9,1.2,1.2 | 7 | 8015 | Spiky urchin ball, fourteen sharp radial spines |
| 8 | pebble | 1,0.85,3, 2.2,2.0,2.0 | 1,1,2, 2.2,2.0,2.0 | 2 | 12788 | Smooth river pebble, near-spherical with a soft three-fold wobble |
| 9 | swirl | 1,1,2.5, 1.2,1.0,1.0 | 1,1,2, 2.0,2.0,2.0 | 6 | 10806 | Twisted swirl from non-integer symmetry, one lobe sweeping past the other |
| 10 | peanut | 1,1,2, 0.7,0.6,0.6 | 1,1,2, 2.0,2.0,2.0 | 1 | 5673 | Peanut with a pinched waist, concave dimple reading almost hollow |
| 11 | acorn | 1,1,4, 1.8,1.6,1.6 | 1,0.8,2, 1.2,1.0,2.6 | 1 | 12517 | Organic acorn teardrop, narrow crown widening to a round base |
| 12 | lantern | 1,1,8, 1.5,1.3,1.3 | 1,1,4, 1.6,1.4,1.4 | 5 | 10053 | Eight-sided paper lantern, gently squared and glowing |

Materials use the workstream-5 authoring palette (1 brown, 2 steel, 3 silver, 5 red glow,
6 blue glow, 7 dark shell, 8 light shell, 10 blue seam, 12 amber). Descriptions contain no
Gielis/Genicap/superformula/No-Man's-Sky names (two-tier rule verified by scan).

**Test evidence:**
- `ctest -R "engine_procgen|engine_litho_formula|litho_formula_gallery_selftest|litho_formula_bake_cli"` → **4/4 PASS** (final coordinator re-run confirmed).
- Neighboring suites `model_selftest|authored_models_selftest|vegetation_selftest|engine_trees|engine_shrubs|engine_storage|engine_core` → 7/7 PASS.
- Every preset: non-empty, bounded in [0,32)³, no NaN; each `.lthm` reloads via `loadModel`
  with `leafCount == filledCells`, author stamp `litho-formula-gallery`, byte-identical
  across two bakes (determinism). An independent Python port of the final fill algorithm
  reproduced all 12 presets' counts exactly.

**Fixes found:** parity-fill leak in the worker's own draft → largest-6-connected-component
speckle filter (kept through Worker 2's rewrite); duplicate CMake entry from the mid-task
race → removed; printf arg bug in selftest → fixed.

---

## 4. Integration verification (coordinator)

- Combined library (all three workers' code) builds clean; Worker 3's code consumes Worker
  1's public API and Worker 2's contract with no conflicts (one duplicate private header
  deleted, one duplicate CMake entry removed — both resolved by the workers).
- Canonical header verified on all 10 new files; repo-wide leak scan: no
  Gielis/Genicap/No-Man's-Sky/superformula mentions outside the header blocks.
- Final coordinator run: `ctest -R "litho_formula|engine_procgen"` → **4/4 PASS**.
- New ctests: `engine_litho_formula`, `litho_formula_gallery_selftest`,
  `litho_formula_bake_cli` (plus voxelizer checks inside existing `engine_procgen`).

---

## 5. `lithoform` node spec — for JD's lock (design only, nothing wired)

Spec file: `engine/src/procgen/litho_formula_node_spec.md`. Verbatim summary:

A **generator-style node** named `lithoform` evaluating the Litho Formula analytically per
sample point and writing a **density-like value**, composing with existing modifier nodes
(`select`, `blend`, `clamp`, …) exactly like `noise` does. Table entry:
`{"lithoform", {}, false, {"theta","phi","radius","falloff","rotation","center"}, makeLithoform}` —
no required graph inputs (pure generator). Params: `theta`/`phi` objects
`{a,b,m,n1,n2,n3}`, `radius` (default 8, >0), `falloff` (default 1, ≥0; 0 = hard mask),
`rotation` [rx,ry,rz], `center` [x,y,z]. No `seed` — the shape is deterministic; variety
comes from the scatter stage. Eval: `q = unrotate(p − center)`; spherical angles of `q`;
`R = |lithoPoint(θ,φ)|`; inside iff `|q|/radius ≤ R` (star-shaped about origin, single
radius comparison); density `d = (R − |q|/radius)/(falloff/radius)`, smoothstepped to
0..1 (or hard mask at falloff=0). ~6 pow + trig per point ≈ one noise octave; worldgen
samples sparsely. Graph-build-time validation via the existing `err` channel; immutable,
thread-safe.

JSON example (rock arch blended into terrain) and two worldgen paths in the spec:
**(a) baked `.lthm` stamping** — recommended for alpha, reuses the tested save/place
pipeline via the existing structure stage; **(b) analytic node in chunk density graphs** —
later, needs per-instance param binding in the scatter stage (doesn't exist yet).

**Open questions for JD (from the spec):**
1. Wire now or later — ship core+gallery first, or wire `lithoform` now?
2. Worldgen use in alpha — scatter Litho Formula structures, or authored `.lthm` only?
3. Density semantics — smooth 0..1 field with falloff (recommended) vs. hard mask?
4. Optional `mask` input — keep v1 without (gate downstream), or add?
5. Per-instance variety — preset+radius+rotation enough, or future seeded param jitter?
6. Preset authorship — JD curates by hand, or crew proposes + JD vetoes? (12 shipped are
   the crew's opening set.)

---

## 6. Left for JD / builder

1. **§5 forks above** — the three locks (node wiring, worldgen use, gallery curation).
2. **Baked `.lthm` gallery** (`game/LithoKraft/assets/models/litho_formula_*.lthm`, 12 files) —
   JD decides keep vs. drop; they're regenerable via `bake_litho_formula` any time.
3. **Pre-existing bug surfaced (not fixed — untouched module, reporting):**
   `lithokraft_silica_selftest` aborts because `makeWorld()` in
   `game/LithoKraft/src/app/silica_selftest_main.cpp` never calls `setRegistry()`,
   tripping `ChunkMap::set`'s assertion. One-line fix for whoever owns silica.
4. **Consideration:** sphere-volume tolerance (6%) has ~1.5× headroom over measured 3.9%
   discretization error — pin the exact count or widen slightly later.
5. **Environmental (pre-existing):** full `all` build needs X11 headers for third-party
   GLFW; Vulkan headless on this VM. Neither is related to this work.
6. Untouched per rules: sim core, trace_jit, netcode, save format, Infinite Registry work.
   The `lithoform` node is spec-only — zero changes to `graph_nodes.*`.

*Report written from the three workers' verified results; test commands and outputs above
are as observed. "Litho Formula" naming and two-tier attribution applied throughout.*
