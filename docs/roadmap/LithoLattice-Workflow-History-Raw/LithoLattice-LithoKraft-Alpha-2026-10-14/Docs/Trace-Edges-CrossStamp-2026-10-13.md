# Trace Edges + Cross-Stamp — 2026-10-13

**Directives (JD, 2026-10-13 20:10 EDT):** (1) "Make the sprites and then do the cross hatching in 3D" — bridge 2D sprites to 3D micro-scale crossed geometry via the subdividing modeler. (2) Trace running through the edges where micro-cell faces meet — "every so often the plants pulse with the trace on the edges of their voxel edges" — via the renderer.
**Design ruling added mid-run (JD, 2026-10-13 20:49 EDT):** transparency is KEYED OUT of 3D models — sub-threshold pixels become real see-through holes in the geometry. Applied in full to Item 1 (see §1.3) and demonstrated in the ring capture (§3.4).

**Work tree:** `~/workspace/experiments/trace-edges/` — a copy of settled production taken **2026-10-14T00:11:43Z** (pre-work md5 manifest: 7,953 files; 1,698 source files). Production was never written by this crew (proof in §5). Crew: two workers (cross-stamp; trace edges) + coordinator (integration, gates, capture tool). Build dir `build-trace` (CMake root is `engine/`; on this VM, GLFW needs `CPATH=<model-dither sysroot>/usr/include` for the Xcursor headers — an environment note, not a code issue).

---

## 1. Item 1 — Cross-stamp (sprites → crossed micro-geometry)

### 1.1 API (new files `engine/src/procgen/cross_stamp.{h,cpp}`, module `litholattice_procgen`)

```cpp
struct CrossStampParams {
  int level = 6;          // octree level 3..6 (N = 2^level cells per block edge)
  int thickness = 1;      // plane band thickness in micro-cells, 1..4
  uint8_t material = 1;   // raw entry point's material for every filled cell
  uint8_t alphaKey = 128; // JD ruling: stamp only pixels with alpha >= alphaKey
};
SparseOctree stampSpriteCross(const uint8_t* rgba, int size, CrossStampParams p);            // raw sprite
SparseOctree stampSpriteCross(const uint8_t* rgba, int size, const bool* stemMask, CrossStampParams p); // two-material
```

- Input: square RGBA sprite, side 1..256, any size (a 16×16 sprite at level 6 expands ×4 per axis). Output: a `subdiv::SparseOctree` in block-local cell coordinates `[0, N)³`, ground-anchored (sprite bottom row at y = 0).
- The two-material variant takes a per-pixel stem mask and stamps stem pixels with `ShrubParams.stemMaterial` and the rest with `leafMaterial` — the same two materials the Modeled3D path uses.
- **Fail-closed everywhere:** null/empty sprite, bad size, level outside 3..6, thickness outside 1..4, `material == 0`, `alphaKey == 0` → empty tree. No exceptions, no partial stamps.

### 1.2 Geometry convention (matches the mesher's flat cross)

The two planes are the block's **diagonals** — the same pair the mesher's `emitCross` draws for `Shape::Cross` (corner (0,0)→(N,N) and (0,N)→(N,0)), so a cross-stamped plant stands exactly where the flat crossed quads stand, at the same footprint and height. Each sprite pixel expands to a k×k patch of micro-cells on each plane (k = N / spriteSize); a plane claims a cell when the cell centre falls inside the pixel's diagonal band of the requested thickness (plane A wins ties where the diagonals cross). Sprite row 0 is the top; the stamp is upright (verified against `emitCross`'s UV orientation and the capture frames).

### 1.3 The alpha-keying ruling (JD 2026-10-13 20:49) — as implemented

- A pixel stamps **iff `alpha >= alphaKey`**. Below the key: no cell, on either plane — a real hole in the octree, see-through from every angle, not a dark cell.
- **Default 128 = the engine's existing cutout convention:** `chunk_shading.glsl` discards with `if (a < 0.5) discard;` on the cutout path and the GL33 sprite pass sets its alpha test to 0.5 — keep iff alpha/255 ≥ 0.5 iff alpha ≥ 128. The stamp's default matches the renderer exactly, and `alphaKey` is a named parameter for tuning.
- Canonical generated sprites are binary-alpha (0/255), so keying changes nothing for them (verified: identical results under both rules); it bites exactly where it should — anti-aliased edges and semi-transparent texels in arbitrary texture input.
- **Selftest proof (group G8):** a synthetic sprite with an interior transparent hole and pixels at alpha 64 / 127 / 128 / 200 — both plane projections equal the keyed mask, the 64 and 127 pixels stamp nothing on either plane, 128 and 200 stamp filled cells, and full lines of cells along X and along Z through the hole centre are empty (a ray passes through the cross at the hole). The hole is also visible in the ring capture (§3.4) and countable in its numbers: 380 keyed pixels → exactly 3,040 leaves; the 60 sub-key pixels contributed zero cells.

### 1.4 The options wiring ("we like options")

`PlantStyle::CrossStamped = 2` (additive; `FlatCross = 0`, `Modeled3D = 1` unchanged) and, in `shrubs.h`:

```cpp
SparseOctree generateShrubCrossStamp(const ShrubParams& params, int level, int thickness);
```

It builds the plant's sprite from the same `ShrubParams` (same silhouette function, same stem rule — bottom 30% centre columns) and stamps it. A formula plant is now producible three ways from one params struct: **(a)** flat crossed quads, **(b)** Modeled3D mound (`generateShrubOctree`), **(c)** cross-stamped micro-geometry. Selftests prove (c) ≡ raw-stamping (a)'s sprite, cell for cell, and that the stamp is seed-invariant in mask exactly like the sprite (seed varies colour only).

### 1.5 Selftests (in the `shrubs` engine-test module; binary prints `cross-stamp: 80 checks, 0 failures`)

Projection fidelity at 64×64 (both plane projections reproduce the keyed sprite mask exactly); an independent dense recount from the sprite alone at levels 3/4/5/6 × thickness 1/2/3; a tiny hand-computed 4×4 case; thickness growth (strictly more cells, subset relation); determinism (identical `leaves()` across repeats); the fail-closed matrix; the legacy-texture path (any `plantTexturePixels` texture stamps; texture family 0 still fails closed); **G8 alpha-keying group (§1.3)**; and three convention pins locking the behaviour to the tree's existing sprite conventions.

### 1.6 Measured (311 three-lobe sprite, 111 keyed texels of 256)

| Level | Cells per block edge | Stamp leaves | Edge segments (§2) |
|---|---|---|---|
| 3 | 8 | 60 | 482 |
| 4 | 16 | 222 | 1,782 |
| 5 | 32 | 888 | 6,674 |
| 6 | 64 | 3,552 | 25,782 |

Level 6 closed form check: 111 px × 16 cells/plane × 2 planes = 3,552 exactly.

### 1.7 Note on the existing Modeled3D path (ruling item 4)

`generateShrubOctree` fills its volume by a density RNG against an ellipsoid/formula falloff — it has no alpha channel and no sprite, so there is no sub-threshold space being filled opaquely; the ruling does not apply to it and it was **not** reworked. The ruling's bite is on sprite→geometry paths, which is the cross-stamp (done) and any future texture-stamping path (must key the same way — the `alphaKey` parameter is the pattern to copy).

---

## 2. Item 2 — Trace edge pulse (renderer)

### 2.1 Trace-convention survey (what was matched)

- **Tint:** the ground pulse's default trace tint **(0.45, 0.62, 1.0)** — the pale circuit blue the Wafer ground traces already glow with. The edge pulse uses the identical tint as its default.
- **Shape:** the ground-trace convention of a **constant low glow + a traveling bright packet** (smoothstep window moving along the line). The edge pulse is `baseGlow 0.15 + amplitude 0.85 × packet(u)`, one packet per edge per period.
- **Individuality:** ground traces phase their lines by a per-line hash; edges do the same — each edge segment carries a phase from a deterministic multiplicative hash of its endpoints, so the geometry sparkles with unsynchronized pulses rather than blinking in unison.
- **Clock law:** phase comes from `FrameParams::timeSec` — the presentation/wall clock the game already feeds the renderer — **never** sim state or `worldTick`. The scenario hash cannot move (proven in §4), and pausing the sim does not freeze the shimmer (it is a render effect, like the ground pulse).

### 2.2 API (new files `engine/src/render/trace_edges.{h,cpp}`, module `litholattice_render`)

```cpp
struct TraceEdgeParams { float periodSec = 4.f; float strength = 1.f;
                         Vec3 tint{0.45f, 0.62f, 1.0f}; float baseGlow = 0.15f; float amplitude = 0.85f; };
struct TraceEdgeSegment { float x0,y0,z0, x1,y1,z1, phase; };   // world-space, phase in [0,1)
float traceEdgeEmissive(const TraceEdgeSegment& s, float u, double tSec, const TraceEdgeParams& p);
std::vector<TraceEdgeSegment> extractOctreeEdges(const subdiv::SparseOctree& tree, int level,
                                                 float originX, float originY, float originZ);
```

- `extractOctreeEdges` works on **any** `SparseOctree` (cross-stamps, voxelized formula models, Modeled3D mounds — anything stamped): it densifies at the given level, emits the 4 edges of every face that borders empty space, and de-duplicates shared edges (a lone cell yields exactly its 12 edges; two adjacent cells yield 20, not 24). Bad level / empty tree → empty list, fail-closed.
- `traceEdgeEmissive` evaluates the pulse at parameter `u` along a segment: a smoothstep packet on `fract(u − t/period + phase)`, computed in double precision. Brightness is bounded in [baseGlow, baseGlow + amplitude]; `strength` is applied at the draw site (a display gain, not part of the emissive law).

### 2.3 Renderer integration (software backend, the default)

- `IRenderer::setTraceEdges(on, periodSec, strength)` (default no-op for other backends); `SoftwareRenderer` implements it, plus a public direct-surface `renderTraceEdges(segments, count, viewProj, timeSec)` for lab/MCP-style callers.
- `FrameParams` gains borrowed `traceEdges` / `traceEdgeCount` pointers (same pattern as the sprite pass). In `renderFrame`, the edge pass runs **after the world and sprite passes, before AA/ASCII/HUD**, drawing 1-px lines with the renderer's own strict-`<` depth test against the raw depth buffer (the pass is occluded by geometry in front, never x-ray; a code comment in the tree that implied `<=` was found wrong and the pass matches the actual convention).
- **Cvars (archived, like the other render settings):** `render.trace_edges` (bool, **default OFF**), `render.trace_edge_period_s` (float 0.25–60, default 4), `render.trace_edge_strength` (float 0–4, default 1). With the feature OFF the pass is skipped entirely and frames are **pixel-identical** (selftest-proven, §2.4).
- **Not yet wired in the live game:** `game.cpp` applies the cvars to the renderer, but nothing in the game yet populates `FrameParams::traceEdges` per frame (there is no live octree-edge producer in the world path yet). The renderer seam is complete and capture-proven; populating it is step 1 of the port (§6).

### 2.4 Selftests (render module: **316 PASS / 0 FAIL**, of which "Trace edge pulse total: 26 checks passed")

Extraction (11): lone cell 12 segments, two adjacent cells 20 (dedup), phase determinism/range, empty/invalid → empty, origin offset. Emissive (7): bounds, periodicity (max diff over a period 1.49e-07), the bright peak measurably travels along the segment (argmax u 0.048 → 0.810 between t=0 and t=1). Renderer (8): OFF = pixel-identical frame; ON changes only edge pixels; a segment behind a wall writes nothing (occlusion); strength 0 = base glow only; tint applied.

### 2.5 Backends

Software: done (above). **GL33: not implemented** (the interface method is a no-op there). **Vulkan: not implemented.** Both are honest follow-ups (§6) — the extraction and emissive law are backend-free and shared; only the line draw is per-backend.

---

## 3. Captures — my reading of every frame

Capture tool: `trace_edges_capture` (experiment tree only), software renderer, 1280×720, dusk-slate lab backdrop, basic diffuse shading, one shared texture array. Subjects stand on a slate ground slab. Files in `trace-edges-shots/` next to this report.

**A note on frame (a)'s construction:** the software renderer's basic fragment shader performs **no alpha test** (its varyings don't even carry the shade mode), and its sprite pass collapses sprites to flat average-colour quads — so a single textured cross quad paints sub-key texels as opaque black (the first render did exactly that; it is the artifact JD's ruling targets). Frame (a) is therefore drawn as one small quad **per keyed texel** (alpha ≥ 128) on the two diagonal planes — pixel-faithful to the cutout result the GL33 path produces. The stamp frames need no such help: they are geometry.

### 3.1 The 311 three-lobe plant (twisted preset, seed 311; sprite 111/256 texels keyed)

- **(a) `plant311-a-flat-sprite.png` — flat crossed sprite.** The familiar flat plant: green per-texel canopy with colour variation, brown stem at the bottom centre, both diagonal planes visible as an X from this angle, and the sprite's transparent texels genuinely absent — backdrop shows through the canopy's gaps. (A faint dark sliver at some texel borders is the engine's 0..16 UV sampling convention bleeding a neighbouring transparent texel's black RGB; it is a texture-sampling artifact of the lab presentation, not geometry.)
- **(b) `plant311-b-cross-stamp.png` — cross-stamped, no edges.** The same plant as real micro-geometry: two crossed walls of green micro-voxel columns/fins at level 6, brown stem cells at the base centre, see-through slots between the columns where the sprite is transparent. It reads clearly as the (a) plant extruded into the cross — same silhouette, real depth, real holes.
- **(c) `plant311-c-edges-phase0..3.png` + `phase0/phase2-close.png` — with trace edges, t = 0,1,2,3 s of the 4 s period.** A constant faint blue edge-glow outlines the micro-cells at all phases; the bright pulse packet travels — most legible on the centre spine column (the planes' intersection), where a dotted bright run climbs between phases, and in the close-ups, where the brightest dashes visibly relocate along the column edges from t=0 to t=2. **Honest assessment:** at this camera distance the effect is subtle — 1-px lines on 1/64-block edges read as travelling energy speckles rather than bold wires. The mechanism, travel, and occlusion are all proven (frames + selftests); making it *loud* is a tuning matter (the strength cvar goes to 4×; a 2-px line mode is a renderer follow-up, §6).

### 3.2 Voxelized formula model (five-lobe Litho Formula, res 32 → 6,698 cells, 6,356 edge segments)

- **(d) `formula-d-model.png` — no edges.** A violet crystalline micro-voxel solid; the stepped micro-cube construction is clearly visible even unlit by edges.
- **(e) `formula-e-edges-phase0.png` / `phase2.png` — with edges.** White-blue trace speckles run along the micro-cube edges over the whole surface; between t=0 and t=2 the bright cluster clearly relocates (mid-body band at t=0 → lower-left/bottom edges at t=2). On a dense solid the pulse reads as glittering energy travelling across the voxel edges — "the trace is in everything" on a non-plant model, which is the point of the generic extractor.

### 3.3 Ring — the alpha-keying demonstration (JD ruling)

Synthetic 32×32 sprite: an opaque annulus (380 px) with a fully transparent core and a wedge painted at alpha 80 (60 px, below the 128 key), cross-stamped at level 6 (3,040 leaves; 21,936 edge segments).

- **`ring-g-alpha-keyed.png` / `-close.png` — no edges.** The crossed annulus in micro-voxels with **two clean punched holes**: the transparent core is a genuine void — you see the backdrop straight through the middle of the cross — and the sub-key wedge is a visible gap interrupting the ring's right arc on both planes. The planes' intersection forms a chain of diamond-cross-section micro-pillars down the centre. This is the ruling made visible: holes as texture, as shape; the model has definition *because* of what was keyed out.
- **`ring-g-alpha-keyed-edges.png` / `-edges-close.png` — with trace edges (t=1).** The pulse outlines the annulus **and the rims of the holes themselves** — bright dots trace the core's inner boundary — so the keying reads even more sharply with the trace on.

---

## 4. Gates (all run on the final build of the experiment tree)

| Gate | Result |
|---|---|
| Full build | **exit 0** |
| Game selftest (`LithoKraft --selftest`) | **1184 PASS / 0 FAIL** — `ALL SELFTESTS: OK` |
| Engine tests (`litholattice_tests`, all modules) | **ENGINE TESTS OK** |
| — shrubs module (cross-stamp) | `cross-stamp: 80 checks, 0 failures` |
| — render module (trace edges) | **316 PASS / 0 FAIL**, incl. `Trace edge pulse total: 26 checks passed` |
| `--handshake-test` | **OK** |
| `--handshake-scenario 2048` | hash **`d609126cc60d71c7`** on legacy, handshake-2w, and handshake-4w arms — **unchanged** |
| ctest (full, 48 tests) | **48/48 passed** |

**Baseline reconciliation (as required):** the brief's baseline was "settled production 1184/0" — the final build's game selftest counts **exactly 1184 [PASS] lines, 0 [FAIL]**, i.e. this work moves the game selftest by zero; all added checks live in the engine-test binary (cross-stamp 80, trace-edge 26 within render's 316). Worker A independently stash-verified the same zero delta on their changes (their counting method read 1185 both with and without the changes — a one-line counting-method difference; both methods agree the delta is 0). One process note: the run's first baseline build attempt raced with both workers' edits landing mid-compile and failed (Error 2); no clean pre-edit baseline *build* exists. The baseline was instead established by the byte-identical verified copy (§5 method) plus the wiring reports' settled-production numbers, which the final build reproduces exactly (1184/0 and `d609126cc60d71c7`).

**Costs (720p, this VM):** stamp fill render ≈ **3.6 ms/frame**; trace edge pass over the plant's 25,782 segments ≈ **4.0 ms/frame** (30-rep averages, lab scene). The edge pass is an unoptimized 1-px CPU line rasterizer; cost scales with segment count (≈0.16 µs/segment incl. depth test). Levels 3–5 stamps cost proportionally less (table §1.6).

## 5. Production integrity

Verified at end of run against the pre-work manifest (`manifest-experiment-start.md5`, 1,698 source files):

- **1,687 / 1,698 files byte-identical.** Zero diffs anywhere under `engine/`.
- **11 modified files, all the concurrent planting crew's additive work:** `game/LithoKraft/src/content/blocks.cpp`, `.../include/lithokraft/content/blocks.h`, `game/LithoKraft/src/game/dimensions.cpp`, `game/LithoKraft/src/app/selftest.cpp` (their spawn-shrubs change + its checks), the six `assets/packs/pack_{16,32,64}/{manifest,resources}.json` (regenerated for their new textures), and `tools/plant_texture_gen.cpp` (already attributed — it changed during my copy window and the copy was refreshed).
- **10 added files, also theirs:** `formula_shrub_311.png` / `formula_shrub_314.png` under the three packs' `textures/blocks/` (+ `textures/ui/icons/` in pack_32/64).
- This crew wrote **nothing** to production; every change lives in `~/workspace/experiments/trace-edges/`.

## 6. Production-port instructions

**Item 1 (cross-stamp) — self-contained, port first:**
1. Copy `engine/src/procgen/cross_stamp.{h,cpp}` + `cross_stamp_selftest.cpp` into production `engine/src/procgen/` (same paths).
2. Apply the `shrubs.h` / `shrubs.cpp` deltas: `PlantStyle::CrossStamped = 2`, `generateShrubCrossStamp`, and the sprite-builder refactor it uses (the sprite builder was split so the sprite and its stem mask come from one pass — take both halves).
3. CMake: add `cross_stamp.cpp` to the procgen sources and `cross_stamp_selftest.cpp` to the test sources in `engine/src/procgen/CMakeLists.txt`; register the `cross_stamp` selftest entry in `engine/tests/engine_tests.cpp` (one line).
4. No game-side changes needed. Gate on: shrubs module (cross-stamp 80/0), game selftest unchanged, scenario hash unchanged.

**Item 2 (trace edges) — two steps:**
1. *Renderer seam (safe, OFF by default):* copy `engine/src/render/trace_edges.{h,cpp}`; apply the deltas to `renderer.h` (IRenderer method + FrameParams fields), `software_renderer.{h,cpp}`, render `CMakeLists.txt`, `software_test.cpp`; apply the game deltas (`settings.h` fields, `settings_cvars.cpp` rows + sync, both `game.cpp` apply sites). Default OFF ⇒ pixel-identical; gate on render module 316/0 + pixel-identity check in the selftests.
2. *Live population (the actual in-game feature):* add a producer that, per frame, extracts edges for the octree-backed models in view (cross-stamped plants / voxelized models) and fills `FrameParams::traceEdges`. Design choices to make at port time: extraction budget per frame (cache per model; extract on stamp, not per frame — segments are static per octree), view-distance cutoff, and segment-count cap. Until this step lands, the cvars light up only lab/MCP callers.
3. *Follow-ups (honest list):* GL33 line pass and Vulkan line pass for parity; a 2-px / strength-tuned line treatment if JD wants the pulse louder at distance; the software basic fragment shader has **no alpha test** (chunk Cross faces in the software world path rely on the sprite pass / GL33 for true cutout) — pre-existing engine gap found during capture work, not caused by this item, worth its own fix run.
