# ProcGen Terrain Expansion — Implementation Report (2026-10-11)

Authorized by JD 2026-10-11 (~22:03) as the "multi-pronged attack": mountains, cave types, rivers, lakes & oceans — the terrain options the biome system was missing. Four workstreams, all landed. Foundations reused: `BiomeHeightmapTerrain` (per-biome blended heightmap), `ICarver` pipeline interface, JSON `NoiseGraph` (fbm/ridged/billow/turbulence fractals), engine fluids (levels 1–16).

JD's laws honored throughout: deterministic and seeded (same seed → same terrain), no-nonsense numbers, powers-of-two defaults. The 2 grandfathered ctest failures (`engine_sim`, `lithokraft_selftest`) were never touched.

## Workstream 1 — Mountains ✅

**Design:** extended the `NoiseGraph` node table instead of adding a new `ITerrainStage`. `BiomeHeightmapTerrain` already evaluates one graph output per biome and weight-averages heights — each biome entry now names an optional extra **uplift** output, scaled and added to its base height *before* blending. No pipeline changes, no duplicated blending logic.

**Files:**
- `engine/src/procgen/graph_nodes.cpp` — new `mountain_range` node (`amplitude·max(ridges,0)^power·smoothstep(mask_low,mask_high,mask)`; inputs `ridges`, `mask`) and `peak` node (`amplitude·max(0,1−distance/radius)^sharpness`; input `distance`, e.g. cellular F1). Factories validate; bad configs fail graph compile.
- `engine/src/procgen/include/litholattice/procgen/stages.h` — `BiomeHeightmapTerrain::Entry` gains `upliftOutput` (default −1) and `upliftAmplitude` (default 1). Backward compatible.
- `engine/src/procgen/stages.cpp` — uplift outputs interned into the existing distinct-output table; `amplitude·uplift` added per biome before weight-averaged blending. Identical behavior when no uplift set.
- `engine/src/procgen/generator_loader.cpp` — biome entries accept `"uplift": "<graph output>"`, `"uplift_amplitude": <blocks>`; unknown names / lone amplitude are load errors.
- `engine/src/procgen/procgen_selftest.cpp` — 20 new `mountainChecks`.
- `engine/docs/procgen-mountains.md` — new design doc.

**Tests:** 20/20 PASS — node math exact, 3 factory rejections, determinism (two independent loads byte-identical), 6 known-seed spot heights, amplitude linearity, zero-amplitude ≡ omitted, border continuity (max adjacent step 2 ≤ 8 over 128 columns across chunk and biome borders), relief exists (range 34 ≥ 30), 2 loader rejections.

**Config surface:** `{"type":"mountain_range","ridges":…,"mask":…,"amplitude":1,"power":1.5,"mask_low":0.35,"mask_high":0.7}`, `{"type":"peak","distance":…,"amplitude":1,"radius":0.7,"sharpness":2}`; biome entry `{"output":"h_base","material":"stone","uplift":"mountain_ranges","uplift_amplitude":64}` — character = which uplift output a biome references; amplitude in blocks per entry; node amplitude stays 1 so one subgraph is shared at different amplitudes.

## Workstream 2 — Cave types ✅

**Design:** new `ICarver` variants following the `RuinsCarver` conventions (chunk-pure, pure function of chunk seed, surface margins, no floating debris) and the `WormCaveCarver` cross-chunk replay discipline.

**Files:**
- `engine/src/procgen/include/litholattice/procgen/stages.h` — new `RavineSettings`/`RavineCarver`, `CavernSettings`/`CavernCarver`; extended `WormCaveSettings`; all three get a `BiomeProbe` for per-biome density.
- `engine/src/procgen/stages.cpp` — implementations.
- `engine/src/procgen/generator_loader.cpp` — `loadRavineCarver`, `loadCavernCarver`, shared `loadCarverDensity`; carver type list now `(worm_cave, ravine, cavern, ruins, river)`.
- `engine/src/procgen/procgen_selftest.cpp` — 40 new `caveChecks()`; `pipelineChecks` made ravine-aware.
- `engine/src/procgen/CMakeLists.txt` — one-line include-dir unblock.

**The carvers:**
- **Worms** (existing, extended): walk extracted to a lambda (byte-identical legacy stream when new fields unset — verified by replica). New per-biome `density` multiplier and `branchChance`/`maxBranches`/`branchLengthFrac`/`branchRadiusFrac` — forks rewind to the parent's walk start and fast-forward to the fork step so branches start exactly on the parent path. Defaults preserve legacy behavior.
- **Ravines** (placeholder implemented): meandering path, pinched ends, deepest/widest mid-gash, V-shaped sloped walls, cut from each column's own surface down, floored at `bottomMargin`. Open to the sky by design (no `surfaceMargin`).
- **Caverns** (new): one candidate per coarse cell (`cellSize`), ellipsoid chambers (`min/maxRadius`, `squashY`) with guaranteed core (`coreFrac`) plus 3D value-noise wobble rim (stateless, pure function of position); cleanup sweep dissolves fully-air-surrounded solids (no stranded blocks), staying inside chunk + surface margin.

**Settings (powers-of-two defaults):** `CavernSettings`: cellSize 64, chance 0.25, radius 8–16, squashY 0.5, Y 8–48, surfaceMargin 6, noiseFreq 1/8, coreFrac 0.75. `RavineSettings`: chance 1/16, length 64–128, half-width 3–8, depth 16–32, meander 0.06, bottomMargin 4. `WormCaveSettings` adds density{}, branchChance 0, maxBranches 2, branchLengthFrac 0.5, branchRadiusFrac 0.75.

**Tests:** 40/40 PASS — independent replicas over 3×3 chunks (cross-chunk continuity), determinism, margins respected, zero floating debris, wobble never eats the core, per-biome density via JSON (29 chambers all in allowed biome), loader rejections. Two real bugs caught and fixed during development: worm density gate double-rolled `chance` (would have squared sub-1.0 probabilities); branch fast-forward copied post-walk RNG state instead of the pre-walk snapshot.

**Build:** full `cmake --build` clean; `ctest -R engine_procgen` Passed; `ctest -R "^engine_"` 21/22 (only grandfathered `engine_sim`).

## Workstream 3 — Rivers ✅

**Design:** `RiverCarver : public ICarver` with a pure, directly testable `traceRiver(...)` downhill-walk function.

**Files:**
- `engine/src/procgen/include/litholattice/procgen/stages.h` — `RiverSettings`, `RiverCarver`, `RiverPoint`, `traceRiver`, `reachCells`.
- `engine/src/procgen/stages.cpp` — implementation.
- `engine/src/procgen/generator_loader.cpp` — `"carvers": [{"type": "river", …}]` with `water_material` (required), `bed_material`, `routing_output` (default `"height"`), all numeric knobs, per-biome density overrides.
- `engine/src/procgen/procgen_selftest.cpp` — 13 new `riverChecks`.
- `engine/docs/procgen-rivers.md` — new design doc.

**How it works:** seeded origin lattice (highest-of-4 start candidates, so rivers begin in hills) → gradient-descent walk on a routing height (central differences, momentum + sinusoidal meander; each step must satisfy `newH ≤ h + uphillTol` with ±30/60/90° deflection retries, else the river ends). Chunk-pure: each chunk replays origin cells within reach (WormCaveCarver pattern), writes only its own columns. Carving is a cosine cross-section (`min(actual terrain, channel)` — never raises terrain, sloped banks by construction); water fills to `freeboard` below routing height, never above the pre-carve surface; `maps.height` updated for carved columns.

**Settings:** `water` (required), `bed`, `cellSize` 256, `chance` 0.5, `maxSteps` 64, `stepLen` 4, `width`/`depth` 4/4, `freeboard` 1, `meander` 2 + `meanderFreq`, `momentum` 0.3, `minSlope` 0.02, `uphillTol` 0.5, `waterTable` 0, `minStartHeight` 8, per-biome `density[256]`.

**Fluid seam** (worldgen had none — defined here, documented): water = `water` material id with level in the state byte's value bits (`stValue` 1–16); worldgen emits level 16 (source) for every channel cell; `ColumnGenerator` already copies the state byte into chunk storage; the world layer maps the `water` material to its water `BlockDef` and reads the level into `FluidState`; `IFluidSim` tapers at runtime.

**Framework note:** added `ICarver::onPipelineBuilt` (default no-op) + `BiomeProbe` alias in `pipeline.h`; `Builder::build()` calls it so carvers bind per-biome density probes. Backward compatible. (Also used by the caves workstream.)

**Tests:** 13/13 PASS — loader accept/reject, `traceRiver` determinism, no-uphill-flow (elevations monotone within tolerance), full-pipeline seed determinism, water correctness (231 water cells: all level-16, only in carved columns, at/below pre-carve surface, bed re-materialed), chunk-pure (chunk alone == chunk with neighbours), bank shape (52 carved columns, max adjacent step 2 ≤ 3, 0 isolated).

**Build:** full `cmake --build` clean, no new warnings. No `"river"` entries added to shipped generator JSONs — game-design call.

## Workstream 4 — Lakes & oceans ✅

**Decided spec (JD 2026-10-11, both messages):** ocean max liquid depth **196**, lake max depth **98** (half), as named constants with hard enforcement. **Correction honored:** NO bedrock special-casing — the guarantee is pure arithmetic (256 solid layers below ground − 196 max depth = ≥60 solid blocks below the deepest liquid), applied across dimensions whatever their ground block is.

**Files:**
- Added: `engine/src/procgen/include/litholattice/procgen/water.h` (constants, settings, `OceanFillRule`/`LakeFillRule`/`ShorelineRule` as `ISurfaceRule`s, `runWaterSelfTest()`), `engine/src/procgen/water.cpp`, `engine/src/procgen/water_selftest.cpp` (52 checks).
- Changed: `engine/src/procgen/generator_loader.cpp` (parses `"oceans"`/`"lakes"`/`"shoreline"`; fixed rule order: surface → extraSurface → oceans → lakes → shoreline → carvers → features), `CMakeLists.txt`, `engine/tests/*` (new `engine_water` ctest), `engine/docs/PROCGEN.md`.

**Named constants:** `kMaxOceanDepth=196`, `kMaxLakeDepth=98` (=196/2), `kGroundLevel=256`, `kSolidLayersBelowGround=256`, `kMinSolidBelowOcean=60` (=256−196), `kDefaultSeaLevel=256`, `kStillWaterLevel=16`. Caps clamp unconditionally in code; the loader **rejects** `max_depth>cap` and `sea_level−max_depth<60` as config errors.

**Design:** oceans eligibility-blended (mix of terrain height and large-scale noise floor target `sea − depth`, depth∈[49,196]); terrain deeper than the cap is **raised** with the column's own surface block (or configured `"floor"`) so the surface stays flat at sea level and ≥60 solid remain below — no cliffs at biome borders. Lakes: water where (biome-eligible) ∧ (terrain below `water_table`) ∧ (smooth fbm mask > threshold); filled flat to the table; pits deeper than 98 hold exactly 98; solid never touched. Shoreline: per-column rim in `[surface−depth, surface+band]`, per-biome top/filler, runs after surface rules, never touches water, border-seamless. Deterministic: pure functions of (dimensionSeed, x, z). Fluid seam: water = ordinary material id from `"water"`; state byte = level, always 16 (source); loader fails loudly if the registry has no water block.

**Tests:** 52/52 PASS (`engine_water`) — caps, ≥60-solid invariant (min floor ≥60 in deepest case), flat still water, lake cap exactly 98 in a 200-deep pit, determinism byte-identical, cross-chunk-seam equivalence, shoreline placement, 7 loader accept/reject cases.

**Build:** `ctest` 29/33 — the 4 failures are pre-existing/unrelated and untouched (2 grandfathered; 3 worm-cave checks from the caves sibling's mid-session edit, since fixed by them; 1 pre-existing game-side `model_selftest` link error).

## Cross-workstream notes
- Three agents edited `engine/src/procgen` concurrently; all builds are green in the final state. Mid-session races produced transient failures (stale binaries, one sibling's in-progress edit) — all resolved by rebuilds; final verification runs are clean.
- The caves workstream fixed two genuine bugs it introduced mid-flight (worm density double-roll; branch RNG snapshot). The rivers workstream correctly flagged the double-roll before the fix and did not touch the sibling's code.
- `ICarver::onPipelineBuilt` + `BiomeProbe` (rivers workstream) is now shared infrastructure used by caves too.

## Test summary
| Workstream | Tests | Result |
|---|---|---|
| Mountains | 20 mountainChecks | PASS |
| Caves | 40 caveChecks | PASS |
| Rivers | 13 riverChecks | PASS |
| Lakes & oceans | 52 water checks (`engine_water`) | PASS |
| Full build | all targets | CLEAN |
| ctest (final) | `engine_procgen` + `^engine_` | green except 2 grandfathered |

## Decisions for JD to lock
1. **Mountain amplitude convention**: node amplitude = 1, per-biome `uplift_amplitude` (blocks) is the tuning knob — confirm.
2. **`mountain_range` mask input**: currently required (pass `1` to disable) — fine, or make it optional in the schema?
3. **Real Wafer numbers**: mountain amplitudes, ridge/mask frequencies, mask windows, which biomes get uplift — all open (tests used ranges=64 / peaks=96 on a 256-high world).
4. **Organic ravines are now LIVE**: `organic.json` listed the placeholder `{"type":"ravine"}` — it now carves real canyons (1/16 chance/chunk, 64–128 long, 16–32 deep). Confirm defaults or tune per dimension.
5. **Worm branches ship OFF** (`branchChance: 0`) — set `branch_chance` > 0 in a dimension's JSON to enable.
6. **Caverns aren't wired into any dimension yet** — decide which dimensions get them and with what cellSize/chance.
7. **Fluid-emission convention**: water material id + state value bits = level 1–16; worldgen emits all-source (16); sim tapers. Needs the world layer to map the `water` material → water `BlockDef` and read the level into `FluidState`.
8. **River routing-output caveat**: the walk routes on one graph output while terrain may blend several per biome — each dimension's generator JSON must expose a representative routing output (usually `"height"`).
9. **River defaults** (cell 256, chance 0.5, 64×4-block steps, width/depth 4, freeboard 1, meander 2) and whether any dimension ships rivers before alpha.
10. **Water for built-in dimensions**: `wafer.json`/`organic.json` have no oceans/lakes/river sections yet — which biomes/dimensions get water? (Defaults assume sea level 256; Organic terrain sits ~70, so real configs need your numbers.)
11. **Water block**: confirm LithoKraft's registry has a `"water"` block (or give its material name).
12. **Lake spill**: approximated (mask ∩ below-table depressions); true cross-chunk spill solving is a possible follow-up.

## Open items
- Per-dimension JSON wiring for all four features (mountains, caves, rivers, water) — game-design calls, intentionally not added to shipped configs.
- Lake true spill solving, nested-list-style markdown N/A — future options.
- Fragment UI binding for `RenderedDoc` — unrelated, tracked elsewhere.
