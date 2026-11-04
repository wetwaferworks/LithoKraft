# Sub-block Subdivision Modeling System — Report (2026-10-11)

JD authorized and confirmed: subdivision is a **modeling system** — subdivide → add detail → save → place. Seven workstreams, all landed.

## Design source
- `~/workspace/goals/lithokraft-game/files/LithoKraft-Lore-Glossary-v2-2026-10-11.md` — "Sub-block subdivision" + "ChipOR viewport" (both JD-locked 2026-10-11)
- Concept art: `~/workspace/user/files/concept-art/` (locked look-and-feel)

## What was built

### 1. Sparse octree core (engine/src/subdiv/)
- `SparseOctree`: 1m block → 64³ micro-cells max (6 levels, powers of two), interior nodes exist only with non-empty children. Empty tree ~56 bytes; single level-6 cell ≪ 64³ bytes.
- True value semantics (deep-copy; the contract draft's defaulted copies would have leaked — corrected).
- `SubdivStore`: chunk-scoped block→octree map, versioned serialization (`"LTHS"`, v1, strict deserialize — 7 malformation classes rejected).
- Deterministic: fixed (level,z,y,x) iteration, no hashing/thread-locals/RNG; `hashLeaves` (FNV-1a).
- **Tests: 74/74 PASS**, ASan+UBSan clean, incl. 3000-op differential property test vs dense 64³ reference. ctest `engine_subdiv`.

### 2. Mesher LOD integration (engine/src/render/micro_mesher.h/.cpp)
- `MicroQuad` (24 B float positions; BinaryQuad's 8-byte layout untouched), `meshOctreeLOD`, `appendChunkMicroGeometry` (the chunk-build integration point), `selectLOD`.
- LOD policy (powers of two): <4m → LOD 6 (full 64³, ~1.6 cm cells), doubling distance per step down to LOD 0 = single cube. Majority-vote downsample (solids stay solid).
- FOV composition: micro-quads strictly inside the block AABB → existing `aabbInFrustumMargin` covers them unchanged; chunk-build contract masks subdivided blocks as air in the normal mesh (no double-draw).
- **Tests: 14/14 PASS** (LOD0 == plain cube, LOD6 micro-faces, greedy merge, bit-identical determinism, monotonicity, empty→0, FOV-bounds assertion).

### 3. Model format + pipeline (game/LithoKraft/src/content/model/)
- `.lthm` v1: `"LTHM"` + u16 version + u16 flags + name/author + u8 maxLevel + u32 leafCount + 5-byte leaves + CRC32 (IEEE). Spec in `FORMAT.md`. Versioning policy: major bumps on layout change.
- `Model`, `ModelBuilder` (fillBox/setCell/clearCell/build), `saveModel`/`loadModel` (bad magic/CRC/version → clean errors, never crashes), `placeModel` (merge semantics: model leaves overwrite, rest preserved).
- Library: `game/LithoKraft/assets/models/` (+ README index).
- **Tests: 55/55 PASS** (`model_selftest`), incl. save-twice byte-identical, corruption detection, version rejection.

### 4. ChipOR viewport (game, blocks 51–52)
- `lithokraft:chipor` (51): plain solid printed chip. `lithokraft:chipor_viewport` (52): opaque body + transparent top (`kShadeGlass`) + miniaturized model in the top half (interior box, 1/16 units).
- Renders workstream 3's real `model::loadModel` on `assets/models/<name>.lthm`; default `chipor-demo.lthm` (silicon die, red+blue traces, torch post, via pillar; 15 leaves). Missing/corrupt/`..` names → clean empty.
- Static, render-only, flat-shaded, no sim — per the locked spec.
- **Tests: 22/22 PASS** (`lithokraft_chipor_selftest`).

### 5. First models (8 .lthm in assets/models/)
Authored at level 6 through `ModelBuilder`, sparse (0.5–3% of full 64³):
- **bashor** (3,928 leaves) — chunky head, stepped peen, `>_` bash prompt etched in red/blue trace
- **pickor** (2,552) — twin downward-curving points, blue-hot tips, red inlay
- **chopor** (2,746) — broad flared blade, bright edge, "chops code" zigzag inlay
- **heapor** (2,096) — T-grip, cupped scoop, glowing blue rim
- **seedor** (2,070) — angled copper tines, five floating procgen seeds
- **slicor** (1,358) — long blade, blue fuller, copper guard, red gem pommel
- **trace_tree** (4,316) — stepped copper via-trunk, 6 forking branches, alternating red/blue tips — matches concept art
- **turtlor** (7,852) — faceted Faraday shell with glowing seams, blue eyes, red-tipped antennae, baby rider — matches concept art
- **Tests: ALL GREEN** (`authored_models_selftest`): round-trips, byte-identical re-author, checked-in files match code.

### 6. Animated ground traces (engine/src/render/ground_traces.h/.cpp)
- `groundTraceEmissive(worldTime, worldPos, biomeWeights, ...)`: flowing packet-dashes along Manhattan trace lines — the wafer thinking. Deterministic in (time, position); sim-time (pause freezes), never wall-clock.
- Per-biome: forest dense blue flow, plains sparse grid-aligned, groves warm slow, spires bright quick, Dead Zones dark. Blends across boundaries with the pulse's weight vectors.
- Additive-only shader changes (`chunk_shading.glsl`, marked); GL33 uniforms + Vulkan push constants; `render.ground_traces` cvar (default ON), composes additively with `render.ground_pulse`.
- **Tests: green** (determinism, Dead-Zone zero, packet motion, blend continuity).

### 7. Silica — "silica grass" (game, blocks 53–54)
- `lithokraft:silica_grass` (53, "Silica Grass" — basic name on purpose): Cross-sprite silicon fern, kTransparent|kAttachable|kRandomTicks.
- `lithokraft:tilled_silicon` (54): farmland. Farm loop: till (SeedOR) → plant silica seed → grow (random tick) → harvest 2–3 seeds → replant.
- Random-tick only (8 picks/section/tick): light-gated spread onto silicon/scribe/dense/flecked — never dead_silicon (dead stays dead); dark = dormant; unsupported pops + sheds seeds.
- Per-biome tint/density via `SilicaShadeCache` (forest pale-green, plains warm silver, groves amber, spires violet, Dead Zones 0).
- Textures: silica_grass / silica_seed / silica_tilled at 64/32/16 across all three packs.
- Registry hookup applied by coordinator after ChipOR rows (append-only order preserved).
- **Tests: 13/13 PASS** (`lithokraft_silica_selftest`).

## Corrections caught (JD's rule: try, catch, accept)
1. Mesher's mid-landing "blocker" (traceP0/traceP1) — traces worker finished landing right after; verified clean, no action needed.
2. Silica header missing `IWorldSimulation::DirtyFn` include — worker fixed, builds clean.
3. Octree contract's defaulted copies would leak — corrected to deep-copy (API unchanged).
4. Mesher/SubdivStore interface drift (`find`/`empty` vs `getBlockOctree`/`blockCount`) — resolved in-tree before landing.
5. ChipOR worker caught its own mid-file row placement via its registry test — moved append-only.

## How JD views it in-game
- Place **ChipOR Viewport**, look down through the transparent top → the demo circuit miniaturized under glass.
- Models in `assets/models/*.lthm` — placeable via `placeModel`; the authoring tool `author_models` regenerates all 8.
- Cvars: `render.ground_pulse`, `render.ground_traces` (both live-toggle, archived).
- Silica: find it spreading on silicon near spawn; till with SeedOR, plant seeds, farm.

## Test summary
| Workstream | Tests | Result |
|---|---|---|
| Octree core | 74 selftest (ASan/UBSan) | PASS |
| Mesher LOD | 14 | PASS |
| Model format | 55 | PASS |
| ChipOR viewport | 22 | PASS |
| First models | authored_models_selftest | PASS |
| Ground traces | selftest | PASS |
| Silica | 13 | PASS |
| Full build | all targets incl. `lithokraft` exe | CLEAN |
| ctest (final, 2026-10-12) | 25/27 pass | only the 2 grandfathered failures (`engine_sim`, `lithokraft_selftest`), untouched |

Two post-handoff corrections by the coordinator (JD's rule): (1) the chipor selftest hardcoded registry size 53 — updated to track the canonical guard (`kSilicaTilledSoil+1`), since the silica hookup added rows 53–54; (2) the scout structures test stripped ALL features for its no-structures comparison, but tree features have since been wired into wafer.json — tree cells failed the structure-only attribution check. Fixed to strip only `type=="structure"` features. Both green.

## Open items (for JD's lock)
- Silica render-side per-biome tint: `Shape::Cross` bypasses `FaceLook` — needs a new engine shade mode or biome-variant block ids (weight→tint pipeline built, application pending).
- Silica light: engine `ILighting` not plumbed into sim — open-sky fallback for now, query injectable later.
- Player input bindings for till/plant/harvest (actions.cpp) — sim functions ready.
- ChipOR per-block model names need block-entity plumbing (fixed `chipor-demo` default for now).
