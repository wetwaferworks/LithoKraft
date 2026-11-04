# Procedural generation (`procgen`)

`src/procgen` is the world generation framework: noise, a JSON node graph, and a staged generator pipeline. It depends
on **core only**: no blocks, chunks or GL. The world module plugs it in (`world/column_generator.h`), naming materials
by block texture key. LithoKraft's Wafer and Organic World are both defined as generator files in `LithoKraft/assets/worldgen/`.

```
procgen (core)                              world                          game / app
noise.h, modifiers.h   -> graph.h  -> pipeline.h, stages.h  -> ColumnGenerator -> Dimension   ResourceManager (packs)
seed.h                              generator_loader.h        (ChunkColumn)                  --seed, --noise-preview
```

## Noise (`noise.h`, `modifiers.h`)
Every noise is an immutable, seeded `INoise` with `sample2 / sample3 / sample4` and `sampleBatch(dims, PointsView, out)`.
`PointsView` is structure-of-arrays (`x[]`, `y[]`, `z[]`, `w[]` spans) so a type can add an SSE/AVX path later without
API changes; every type uses the scalar loop today. All methods are const and thread-safe.

| Type (JSON name) | Notes | Range |
|---|---|---|
| `perlin` | Improved Perlin (2002): quintic fade, seeded permutation; 2D 8 / 3D 12 (+4) / 4D 32 gradients | [-1, 1] |
| `opensimplex2f` | Our own OpenSimplex2-style noise, fast variant: 2D triangle lattice (3 vertices), 3D rotated BCC lattice (two cubic lattices offset by ½), 4D simplex lattice (5 vertices) | [-1, 1] |
| `opensimplex2s` | Smooth variant: same lattices, larger kernel radius, every vertex in reach (2D r² ⅔, 3D r² ¾, 4D r² ¾) | [-1, 1] |
| `value` | Lattice values, cubic smoothstep (C1) | [-1, 1] |
| `cellular` | Worley: one jittered point per cell; `f1`, `f2`, `f2-f1`; `euclidean`, `manhattan`, `chebyshev`; `jitter` 0..1. The search widens past the 3^d neighbourhood only when a farther cell could still be closer, so results are exact and continuous | [0, bound] (raw distance; bound from `range()`) |
| `white` | Hash of the cell (floored coordinates) | [-1, 1) |

The OpenSimplex2 code is written from the published idea (lattice choice, kernel `(r² − d²)⁴ · dot(g, d)`), not ported
from any implementation. The 3D form uses the BCC lattice, which sidesteps the (expired, 2022) simplex-noise patent's 3D
claims; 4D uses the simplex lattice. Output scales were measured empirically so extremes land near ±1, then clamped.

Modifiers wrap any noise: `createFractal(source, {fbm|ridged|billow|turbulence, octaves, lacunarity, gain}, seed)`
(octaves shifted by seed-derived offsets; fbm/billow stay in [-1, 1], ridged/turbulence in [0, 1]) and
`createDomainWarp(source, warp, amplitude, frequency)`.

## Seeding (`seed.h`)
`mix64` (SplitMix64 finalizer), `deriveSeed(base, salt)`, `nameHash` (FNV-1a), `latticeHash`, and `Rng` (xoshiro256**).
- `dimensionSeed(worldSeed, "organic")`: every dimension gets independent noise from one world seed (`--seed n`).
- `chunkSeed(dimensionSeed, cx, cz)`: per-chunk randomness (carvers, features).
- Graph noise nodes: `deriveSeed(dimensionSeed, node "seed" or nameHash(node name))`.
- Features: `deriveSeed(chunkSeed, nameHash(feature name))`, so adding a feature does not shift the others.
Nothing depends on thread scheduling or generation order; the selftest compares serial and JobPool runs.

## Node graph (`graph.h`)
A graph has named nodes and named outputs. Inputs are a node name, a number (constant) or an inline node object.
Loading normalizes the description (`GraphDesc`): inline nodes are hoisted into named nodes (`<parent>/<input>`),
so `GraphDesc → JSON → GraphDesc` is lossless (selftest: dump → parse → dump is identical and evaluates identically).

```json
{
  "nodes": {
    "base":   {"type": "noise", "noise": "opensimplex2s", "frequency": 0.01, "fractal": {"type": "fbm", "octaves": 5}},
    "height": {"type": "scale_bias", "input": "base", "scale": 20, "bias": 64}
  },
  "outputs": {"height": "height"}
}
```

| Node | Inputs | Parameters |
|---|---|---|
| `constant` | – | `value` |
| `coordinate` | – | `axis` (`x`/`y`/`z`) |
| `wave` | – | `axis` or `direction` [x, y, z], `function` (`sin`/`cos`), `frequency`, `amplitude`, `phase` |
| `noise` | – | `noise` (type), `seed`, `frequency` (number or [x, y, z]), `offset` [x, y, z] (noise space), `dims` (2 = x/z plane, 3, 4), `w` (4D), `fractal` {type, octaves, lacunarity, gain}, `warp` {noise, amplitude, frequency, fractal}, `cellular` {return, metric, jitter} |
| `add`, `multiply`, `min`, `max` | `inputs` [...] | – |
| `clamp` | `input` | `min`, `max` |
| `curve` | `input` | `points` [[x, y], ...] (increasing x), `interpolation` (`linear` / `cubic` = monotone, no overshoot) |
| `select` | `control`, `a`, `b` | `threshold`, `falloff` (smoothstep blend over threshold ± falloff) |
| `blend` | `a`, `b`, `t` | – (t clamped to 0..1) |
| `terrace` | `input` | `step`, `smoothness` (0 = hard steps) |
| `scale_bias` | `input` | `scale`, `bias` |
| `cache` | `input` | – (keeps its values while the same points are re-bound) |

Validation rejects unknown types, unknown parameters (typos), missing inputs, unknown references and cycles, with the node
name in the message. Evaluation: `NoiseGraph::compile(desc, seed)` builds immutable nodes; `evaluate(output, ctx, out)` runs
a batch over the points bound to a per-thread `GraphContext`. Each node is computed once per binding, so several outputs
over the same points (climate + height) share their common nodes.

## Generator pipeline (`pipeline.h`, `stages.h`, `generator_loader.h`)
Per chunk column: **climate → biome selection → terrain → surface rules → carvers → features**.
`GeneratorPipeline::generate(ChunkInfo)` is const and thread-safe and returns a `ColumnBuffer` (material ids, 0 = air,
plus a state byte) and optionally the `ColumnMaps` (height, climate, biome per column).

| Stage interface | Built-in | Status |
|---|---|---|
| `ITerrainStage` | `HeightmapTerrain` (graph output → surface, `depth` blocks thick), `DensityTerrain` (3D: solid where output > 0) | done |
| `IClimateStage` | `GraphClimate` (temperature, humidity, continentalness, erosion from graph outputs; missing = `fallback`) | done |
| `IBiomeSelector` | `MultiNoiseBiomeSelector` (nearest climate point; only listed parameters count) | done |
| `ISurfaceRule` | `LayeredSurface` (top + filler per biome; optional top state = biome index) | done |
| `ICarver` | `WormCaveCarver` (cross-chunk worms replayed from every chunk in reach, keeps `surface_margin` below the surface) | example |
| | `RavineCarver` | stub (carves nothing) |
| `IFeature` (+ `FeatureRegistry`) | `OreVeinFeature` (random-walk veins replacing one material, inside the chunk) | example |
| | `DungeonSlot` | placeholder (places nothing) |

Generator file (`worldgen/<dimension>.json` in a pack; LithoKraft ships `LithoKraft/assets/worldgen/`); materials are block texture keys (`stone`, `dirt`, `grass`,
`silicon`, ...) or `air`:

```json
{
  "graph": {"nodes": {...}, "outputs": {"height": "...", "continentalness": "..."}},
  "terrain": {"type": "heightmap", "output": "height", "material": "stone", "depth": 0},
  "climate": {"continentalness": "continentalness"},
  "biomes": [{"name": "plains", "climate": {"continentalness": 0.25},
              "surface": {"top": "grass", "filler": "dirt", "depth": 3}}],
  "surface": {"top_state": "biome"},
  "carvers": [{"type": "worm_cave", "chance": 0.3, "min_y": 10, "max_y": 50, "surface_margin": 8}, {"type": "ravine"}],
  "features": [{"name": "dirt_veins", "type": "ore_vein", "material": "dirt", "replace": "stone", "count": 6, "size": 14}]
}
```
(`density` terrain takes `min_y` / `max_y`; worm caves also take `min_length`, `max_length`, `min_radius`, `max_radius`.)

## Packs, built-ins and threading
- `ResourceManager` reads `worldgen/wafer.json` and `worldgen/organic.json` from the pack stack (highest pack wins); the
  game pack is `LithoKraft/assets/`. The game's `createDimensions(WorldgenSettings)` builds each dimension's generator from
  the pack file, or, if it is missing or invalid (logged), from the copy embedded in the exe at build time
  (`cmake/embed_text_files.cmake`). F9 re-reads the files, but generators only change for worlds created afterwards.
- `Dimension::load` runs `ColumnGenerator::generate` for every column on the existing `JobPool`; one shared, immutable
  pipeline serves all workers.

## Debug tool
`LithoKraft --noise-preview <graph.json> [--preview-out f.png] [--preview-output name] [--preview-size 512]
[--preview-scale blocks/px] [--preview-y 64] [--seed n]` writes a grayscale PNG (min..max → black..white, range logged)
without opening a window. It accepts a bare graph or a generator file and uses output `height` (or the first output).

## Selftests (`--selftest`, `procgen_selftest.cpp`)
Every noise type, cellular variant, fractal and domain warp in 2D/3D/4D: finite, inside its range, uses its range, and
continuous (bounded finite-difference step); seeds reproduce and differ; batch == scalar. Graph: every node type parses,
JSON round trip is lossless and evaluates identically, cache, errors (unknown type / parameter / input / output, cycles).
Biome selector cases. Pipeline: chunks identical serially and on the JobPool, surface and state follow biomes, caves and
veins appear, distinct dimension/chunk seeds. The app selftest also checks the shipped wafer/organic generators.

## Not done yet
SIMD batch paths; ravines and dungeons (slots only); trees and other structures; AngelScript worldgen hooks; streaming
worldgen beyond the fixed world size.
