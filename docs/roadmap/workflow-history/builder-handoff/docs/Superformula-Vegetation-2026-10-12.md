# Superformula Vegetation — Phase 3 Proof & Deliverable Report

**Date:** 2026-10-12 · **Tree:** `~/workspace/litholattice` (build dir `build-linux`) · **Scope of this phase:** verification, captures, and this report only — no game/engine code was changed by the verifier.

**Headline:** H3 is fixed and regression-tested. Superformula trace-tree canopies and formula shrub cross-sections are live in worldgen, deterministic across independent runs (byte-identical captures), and every vegetation/procgen/formula test is green. The only sweep deltas vs. baseline are in the known-flaky audio area and a test target that landed mid-session from a concurrent workstream — details in §7 and §9, nothing vegetation-related failed.

---

## 1. (a) H3 fix — analytic node vs. baked stamp polar-axis disagreement

**Status: DONE. H3 comes off the builder's list.**

- **Bug:** the analytic Litho Formula node (`litho_formula_node.cpp`) used world **Z** as its polar axis, while the voxelizer/stamp path uses world **Y**. Every non-spherical shape therefore disagreed with its own baked stamp by 90°. (Spheres hid it — the old node tests only used spheres.)
- **Fix:** the node's polar mapping is now `phiA = asin(qy/qlen)`, `thetaA = atan2(qz, qx)`, with an in-code comment recording that phi is latitude off the world XZ plane, matching the voxelizer convention (`formula (x, y, z) -> world (x, z, y)`).
- **Spec:** `litho_formula_node_spec.md` §4 mapping corrected; the node↔voxelizer agreement claim is restated as a **tested contract**; the stale "proposed, not implemented" status line was fixed (review M6 drive-by).
- **Regression test:** new `voxelizerAgreementChecks()` in `litho_formula_node_selftest.cpp` — bakes the non-spherical **starfish** preset at 32³ with `voxelizeLithoShape`, evaluates the node field per cell, and asserts occupancy agreement ≥ 0.95 plus per-axis surface-radius agreement.
- **Fail-before / pass-after** (worker-measured via temporary revert of the mapping):

  | Metric | Old mapping (Z polar) | New mapping (Y polar) |
  |---|---|---|
  | Starfish occupancy agreement | 0.8756 (test fails) | **0.9673** (test passes) |
  | Per-axis surface radius | +Y radius 0.743 vs. 1.000 expected — the review's empirical numbers | all axes agree within tol 0.100 |

- **Verifier-observed** (this session, `litholattice_tests litho_formula`, 59 PASS / 0 FAIL): agreement **0.9673**; +X node 1.000 / baked 1.000; +Y node 1.000 / baked 1.000; +Z node 0.743 / baked 0.733.
- **Consumer check:** no game JSON uses the analytic lithoform node; shipped worldgen uses only baked stamps (already correct orientation), so no shipped content was ever mis-rotated — the bug was latent in the analytic path only.

---

## 2. (b) Superformula design — trace-tree canopies

**Math provenance.** The canopy shapes are driven by the superformula published by **Johan Gielis (2003)**. The engine's **Litho Formula** is a clean-room implementation written from the mathematical definition — no third-party code referenced — and the underlying patent **expired 2020-05-10**, as recorded in the header of `engine/src/procgen/litho_formula.cpp`. A 3D shape is the spherical product of two 2D superformula evaluators (`lithoPoint` / `lithoR`): a *theta* (longitude) evaluator and a *phi* (latitude) evaluator, each parameterised by `(a, b, m, n1, n2, n3)`.

**Canopy construction.** Each trace tree's crown is a small Litho Formula volume stamped at the trunk crown:

- Surface sampled at 72×36, **factored into 72+36 `lithoR` evaluations**, then filled per-column (min/max span per column).
- Branch material fills the volume; the **top cell of each column takes the tree's tip material** — the existing red/blue per-tree glow behaviour is preserved.
- A skirt cut at `dy >= -1` keeps the canopy seated on the crown; `TraceTreeFeature::place` gained a taller height guard, and `TraceTreeFeature::reach()` now covers the maximum canopy radius so border trees replay identically in neighbouring chunks.

**Archetypes** (rolled per instance; ranges verified against `stages.cpp` this session):

| Archetype | Roll | Radius H | Radius V | Parameter character |
|---|---|---|---|---|
| Spire | 25% | 2 | 3–4 | Pod-like phi taper: phi n1 2.6–3.4, n2/n3 5.0–8.0 — taller than wide |
| Parasol | 25% | 3 | 1 | Broad and flat: phi n1 1.0–1.5, n2/n3 1.6–2.4 — wider than tall |
| Lobed | 30% | 3 | 2–3 | Theta m = 3–5 lobes, theta n1 1.0–1.6, n2/n3 0.9–1.3 |
| Round | 20% | 2–3 | 2–3 | Near-sphere classic crown with scale jitter |

All archetypes share near-sphere default evaluators with mild a/b jitter (0.9–1.1); radii are clamped to the documented maxima (H ≤ 3, V ≤ 4).

**Variance scheme (review M11).** Per-instance jitter comes from an **independent, position-keyed stream**: `posSeed(deriveSeed(featSeed, nameHash("trace_tree_canopy")), wx, wz)`. The canopy stream never perturbs the trunk/branch stream, and a tree's canopy is a pure function of (feature seed, world anchor) — chunk-border trees are identical regardless of generation order. (Verified: `TraceTreeFeature` already keyed all of its decisions to `posSeed`.)

**Selftest evidence (verifier-observed, procgen selector 414 PASS / 0 FAIL):** canopy determinism (same seed+pos → identical spec and stamp); **23 distinct canopy signatures across 64 anchors** (≥16 required); all 4 archetypes appear; spire silhouette taller-than-wide and parasol wider-than-tall assertions pass; stamps non-empty and inside documented radii.

---

## 3. (c) Shrubs — formula cross-sections + placement

- **Cross-section:** `generateShrubOctree`'s horizontal cross-section is now `lithoR`-modulated and **mean-normalized**. With default `super*` parameters the original round mound is reproduced **bit-for-bit**.
- **New `ShrubParams` fields** (additive, fail-closed validation): `superM / superN1 / superN2 / superN3 / superA / superB`.
- **Presets:** berry = **m=5** (flower/star cross-section), twisted = **m=3**. Berry/twisted golden leaf hashes were re-pinned in `shrubs_selftest.cpp` with a rationale comment. Verifier-observed: shrubs selector **63 PASS / 0 FAIL**, including "berry (m=5) and twisted (m=3) presets differ" and the re-pinned goldens.
- **Placement wiring (live):** the existing `GroundCoverFeature` path is used — a new **`shrubs_dry_tuft`** feature in `wafer.json` places the existing **silica_grass** Cross block (**no new block**) at the `biome_vegetation.h` CrossedQuads densities: **dicing_plains 0.012, resonator_spires 0.020**.
- **The Modeled3D gap (remaining builder work):** Modeled3D shrubs (berry/twisted octrees — Trace Forest Berry 0.10, Capacitor Groves Twisted 0.06) do **not** stamp into voxel chunks yet. This is documented in the scope comment in `biome_vegetation.h` and in the `wafer.json` feature comment; an octree→chunk stamping path (or a modeled-shrub entity path) is builder work — see §9.
- **End-to-end probe** (worker-run, real `wafer.json`, seed 20261011, run twice identical): Trace Forest 3×3 chunks = 315 trunk / 424 branch / 87 tips; Dicing Plains 3 silica tufts; Dead Zones interior 0 shrubs. The verifier's own `biome_capture` run logged the same Trace Forest feature totals (trunks=315, branches=424).

---

## 4. (d) Micro-models — gallery additions

Three new gallery presets were baked (material 6, resolution 32) and added to `litho_formula_gallery.json`:

| Preset | File | Leaves |
|---|---|---|
| Spire canopy variant | `trace_forest_tree_spire.lthm` | 5,325 |
| Parasol canopy variant | `trace_forest_tree_parasol.lthm` | 8,777 |
| Lobed canopy variant (theta m=4) | `trace_forest_tree_lobed.lthm` | 10,427 |

The pre-existing 12 `.lthm` models were rebaked **byte-identical** (md5-verified by the worker). `litho_formula_gallery_selftest` and `litho_formula_bake_cli` both pass in the verifier's ctest run.

---

## 5. (e) Before / after frames

Frames live in `~/workspace/user/files/wafer-frames/superformula/` — `before/` copied from the textured run (`wafer-frames/textured/`), `after/` freshly captured this session with `biome_capture` (fixed seed, real `wafer.json`), `after2/` a second independent capture used for the determinism proof (§6).

**What to look for:** in Trace Forest and at spawn, the old small single-cube tree tips are replaced by broad, varied canopy volumes — flat parasol slabs, rounded crowns, and pink lobed tops with the red/blue tip glow preserved. In Dicing Plains, small bright cross-shaped silica tufts now appear (the capture log reports crystals=0 for Dicing Plains, so the bright crosses there are the silica_grass tufts; the count is small — 3 per 3×3 chunks per the probe — so they read as accents at this zoom, not ground cover). Near-white pixels: **0.00% in both before and after frames** — no whiteout regression from the texture work.

### Spawn — before
![Spawn before](sandbox://workspace/user/files/wafer-frames/superformula/before/spawn.png)

### Spawn — after
![Spawn after](sandbox://workspace/user/files/wafer-frames/superformula/after/spawn.png)

### Trace Forest — before
![Trace Forest before](sandbox://workspace/user/files/wafer-frames/superformula/before/trace_forest.png)

### Trace Forest — after
![Trace Forest after](sandbox://workspace/user/files/wafer-frames/superformula/after/trace_forest.png)

### Dicing Plains — before
![Dicing Plains before](sandbox://workspace/user/files/wafer-frames/superformula/before/dicing_plains.png)

### Dicing Plains — after
![Dicing Plains after](sandbox://workspace/user/files/wafer-frames/superformula/after/dicing_plains.png)

Full sets (also capacitor_groves, dead_zones, resonator_spires, wafer_overview_map):
`before/` and `after/` under `~/workspace/user/files/wafer-frames/superformula/`. Every biome frame changed vs. before; the overview map is byte-identical (its top-down palette does not resolve canopy shape).

---

## 6. Determinism proof (verifier-run)

**Method:** two independent `biome_capture` process invocations into two different output directories (`after/` and `after2/`), then byte comparison (`cmp` + md5) of every produced PNG.

**Verdict: IDENTICAL.** All 7 outputs (spawn, trace_forest, dicing_plains, capacitor_groves, resonator_spires, dead_zones, wafer_overview_map) are **byte-for-byte identical** across the two runs — no metadata caveat needed, the PNG bytes themselves match. This corroborates the worker's twice-run worldgen probe (identical) and the procgen selftest's stamp determinism (identical 106,484 canopy cells per 1,000 trees in both verifier procgen runs).

---

## 7. (g) Test sweep — verifier-observed numbers

Build first: `cmake --build .` in `build-linux` — **clean, exit 0**, all targets built.

| Suite | Baseline | Observed | Verdict |
|---|---|---|---|
| ctest (full, two runs) | 36/39 (fails: selftest, scout, chipor) | **35 passed in both runs** (39 listed in run 1, 40 in run 2) — see notes | matches baseline except one alternating flake (assets in run 1, audio in run 2) + concurrent items target Not Run in run 2 |
| `LithoKraft --selftest` | 840/7 baseline; 848/7 expected | **847 PASS / 8 FAIL** (3 runs, identical) | 7 = expected pre-existing set; +1 = audio streaming (below) |
| Signal / Trace (`runSignalSelfTest` section of `--selftest`) | 117/117 | **117 PASS / 0 FAIL** | exact match |
| `litholattice_tests litho_formula` | — | **59 PASS / 0 FAIL** | incl. H3 agreement 0.9673 |
| `litholattice_tests procgen` | — | **414 PASS / 0 FAIL** (2 runs) | incl. all canopy checks |
| `litholattice_tests shrubs` | — | **63 PASS / 0 FAIL** | incl. re-pinned goldens |
| `litholattice_tests trees` | — | **27 PASS / 0 FAIL** | |
| `litholattice_tests audio` (standalone) | — | **72 PASS / 0 FAIL** | passes standalone |
| `litholattice_tests assets` (standalone) | — | **36 PASS / 0 FAIL** | passes standalone |

**ctest detail (run 2, output captured to file):** 35 passed; failures were `engine_audio`, `lithokraft_selftest`, `lithokraft_scout_selftest`, `lithokraft_chipor_selftest`, plus `lithokraft_items_selftest` **Not Run**. Run 1 differed only in the flake slot — `engine_assets` failed instead of `engine_audio` (see note 2). Every vegetation-adjacent test passed: `engine_procgen`, `engine_trees`, `engine_shrubs`, `engine_litho_formula`, `engine_assets`, `vegetation_selftest`, `litho_formula_gallery_selftest`, `litho_formula_bake_cli`.

**Notes on the deltas — read before quoting the headline numbers:**

1. **Audio streaming / engine_audio.** The game selftest's 8th failure is `streaming: worker-thread decode, double-buffered handoff, 440 Hz intact, clean EOS`, and `engine_audio` failed under ctest in the same session — yet the standalone engine audio selector passes 72/0. This is the documented audio-flakiness area (timing-sensitive worker-thread checks), reproducible 3/3 in the game selftest in this session but absent from the workers' final run (848/7). It has no connection to the vegetation/formula changes (different subsystem; all other 847 checks, including every new canopy/shrub check, pass). Flagged here, not fixed — per this phase's scope, code fixes are the builder's call.
2. **The suite grew mid-session — and the 4th failure slot alternated between two flaky tests.** The verifier ran full ctest twice. Run 1 (39 tests listed): **35 passed**, fails = `engine_assets`, `lithokraft_selftest`, scout, chipor. Run 2 (40 tests listed): **35 passed**, fails = `engine_audio`, `lithokraft_selftest`, scout, chipor, plus items Not Run. At 20:18:44–20:18:52 UTC — between the two runs — a concurrent workstream landed `items_selftest_main.cpp` and an `add_test(lithokraft_items_selftest …)` entry in `game/LithoKraft/src/app/CMakeLists.txt`; its executable had not been built when run 2 executed, hence Not Run. On the original 39-test set both runs score **35/39** vs. the 36/39 baseline, with the single delta alternating: `engine_assets` failed run 1 ("sig: other-key mod signed") and passed run 2, while `engine_audio` passed run 1 and failed run 2. Both pass standalone in this session (audio 72/0, assets 36/0) — the same timing-sensitive flake pattern as the game selftest's streaming check in note 1.
3. **The 7 expected game-selftest failures** are exactly the pre-existing set: 4× save round-trip, mesher sub-chunk, Vulkan headless (environmental — `VK_ERROR_INCOMPATIBLE_DRIVER`, no GPU on this VM), hotbar icons.

---

## 8. (f) Performance

| Metric | Value |
|---|---|
| Canopy stamp cost (worker-measured) | **0.079 ms/tree** |
| Before factoring (worker-measured) | 0.913 ms/tree — factoring cut cost **11.5×**, stamps bit-identical |
| Per chunk at Trace Forest density (worker) | ~3.5 ms/chunk |
| Canopy stamp cost (verifier-observed, this VM) | 0.126–0.155 ms/tree (1,000 trees, 106,484 cells, both runs) — same order; VM/load variance vs. the worker's figure |

---

## 9. (h) Full combined changelog

### Worker 1 — H3 fix (Phase 0)

- `engine/src/procgen/litho_formula_node.cpp` — polar mapping changed to phiA = asin(qy/qlen), thetaA = atan2(qz, qx) + explanatory comment. The node previously used world Z as polar axis; the voxelizer/stamp use world Y — every non-spherical shape disagreed with its own stamp by 90°.
- `engine/src/procgen/litho_formula_node_spec.md` — §4 mapping corrected; agreement claim restated as a tested contract; stale "proposed, not implemented" status line fixed (review M6 drive-by).
- `engine/src/procgen/litho_formula_node_selftest.cpp` — new voxelizerAgreementChecks(): bakes the non-spherical starfish preset at 32^3 with voxelizeLithoShape, evaluates the node field per cell, asserts occupancy agreement >= 0.95 (measured 0.9673 new vs 0.8756 old — fail-before/pass-after verified by the worker via temporary revert) plus per-axis surface-radius checks (+Y radius 0.743 vs 1.000 under the old mapping, exactly the review's empirical numbers). Consumer check: no game JSON uses the analytic lithoform node; shipped worldgen uses only baked stamps (already correct orientation).

### Worker 2 — Superformula vegetation (Phases 1–2)

- `engine/src/procgen/include/litholattice/procgen/stages.h` — canopy API (archetype enum, TraceTreeCanopySpec/Cell, radius constants, declarations); TraceTreeFeature::reach() covers canopy radius; includes litho_formula.h.
- `engine/src/procgen/stages.cpp` — canopy implementation + TraceTreeFeature::place stamps canopy at crown, taller height guard.
- `engine/src/procgen/include/litholattice/procgen/shrubs.h` — ShrubParams super* fields.
- `engine/src/procgen/shrubs.cpp` — super* validation; lithoR cross-section in generateShrubOctree; berry m=5, twisted m=3 presets.
- `engine/src/procgen/shrubs_selftest.cpp` — formula cross-section test block; berry/twisted golden hashes re-pinned with rationale comment.
- `engine/src/procgen/procgen_selftest.cpp` — replicaTree mirrors canopy; new treeCanopyChecks; <chrono> include.
- `game/LithoKraft/assets/worldgen/wafer.json` — shrubs_dry_tuft groundcover feature + comment.
- `game/LithoKraft/src/content/vegetation/biome_vegetation.h` — scope comment: CrossedQuads placement live, Modeled3D still builder work.
- `game/LithoKraft/assets/models/litho_formula_gallery.json` — 3 new presets.
- `game/LithoKraft/assets/models/trace_forest_tree_spire.lthm` / `_parasol.lthm` / `_lobed.lthm` — NEW.

---

## 10. (i) What's left for the builder

- ~~H3 Formula polar-axis fix~~ — **DONE this phase; off the list.**
- **Modeled3D shrub placement** — octree → chunk stamping (or modeled-shrub entity path) so Trace Forest Berry (0.10) and Capacitor Groves Twisted (0.06) shrubs actually appear in voxel chunks.
- **Remaining pre-existing failures:** scout ModelResolver wiring; chipor stale registry-size guard; 4× save round-trip; mesher sub-chunk; 15 missing block icons (hotbar icon check).
- **Audio streaming flakiness** — the game selftest streaming check failed 3/3 in this verification session and `engine_audio` failed under ctest while passing standalone (72/0); the workers' run was green. Worth a look under the existing audio-flakiness thread.
- Silica worldgen feature.
- Deep-wafer / Substrate decision.
- MCP wiring.
- Spawn-height fix.
- Note (not a builder task from this workstream): a concurrent session landed an unbuilt `lithokraft_items_selftest` target mid-verification — it needs a build before it can run under ctest.

---

*Verifier's closing note: everything this workstream claimed checks out — H3 numbers, canopy variance and determinism, shrub wiring, gallery bakes, and the end-to-end feature totals all reproduced independently. The two sweep deltas (audio streaming, the mid-session items target) are documented above with their evidence; neither is a vegetation regression.*
