# Procedural Systems — Architecture, Capabilities & Growth Plan (2026-10-12)

![Procedural systems UML](procgen-uml-2026-10-12.png)
*Source: `procgen-uml-2026-10-12.dot` — re-render with `dot -Tpng`.*

## 1. What exists today

**GeneratorPipeline** (`engine/src/procgen/pipeline.cpp`) is the orchestrator. It owns an ordered `FeatureRegistry` of `IFeature` stages, binds them via `onPipelineBuilt()`, and exposes a `biomeAt()` probe. Everything is deterministic per world seed — placement decisions are pure functions of world position (cell-keyed RNG), so all chunks and all `--seed` runs agree exactly.

**Feature stages** (`stages.cpp`):
- **Terrain**: heightmap generation, carvers (rivers), modifiers.
- **Trees/shrubs** (`shrubs.cpp`, `trees.h`): biome-gated vegetation.
- **Water/fluids** (`water.h`).
- **StructureFeature**: the prefab placer. Takes a blueprint (list of `{d:[dx,dy,dz], m:material}` cells), places it on a cell grid with configurable density, 0/90/180/270 rotation, biome gating (`biomes` probability map), and surface validation (must sit on a specified top block, must fit vertically). Margin-aware: replays neighboring cells so structures crossing chunk borders place atomically.

**Noise layer**: Perlin, OpenSimplex2, cellular/Worley — the fields everything samples.

**Litho Formula** (`litho_formula*.cpp`, `graph*.cpp`): the model synthesizer. Graph → node → voxelizer → baked 32³ `.lthm` gallery. The Formula node is itself worldgen-placable, so generated models can appear in the world.

**Generator loader** (`generator_loader.cpp`): parses `worldgen/*.json` into pipeline configuration. Blueprints today are **inline** in the JSON.

## 2. Capabilities

- Deterministic generation: same seed → same world, bit-identical, across runs and machines.
- Chunk-border-safe structure placement with halo replay.
- Biome-gated, density-controlled, rotatable prefab placement (`StructureFeature`).
- Multi-noise terrain with carvers and modifiers.
- Formula-driven model synthesis usable as worldgen content.
- Mods can ship their own `worldgen/*.json` to add features.

## 3. Fallbacks & drawbacks (honest)

- **Blueprints are inline JSON only.** There is no named structure registry — every structure's cells are pasted into the worldgen JSON. JD cannot design a structure once and reference it from multiple dimensions/biomes. This is the gap the Structure Registry closes.
- **No spacing/exclusion solver.** Density is per-cell probability; two structures can overlap or spawn adjacent in ugly ways. Minecraft solves this with separation settings — we don't have that yet.
- **Surface approximation.** Structures sample the heightmap at the clamped column; on steep terrain the "surface" can be wrong and structures float or bury. The margin replay mitigates chunk borders, not slopes.
- **Structures sit on the surface only** (`dy >= 0` enforced). No buried ruins, no floating islands, no underwater wrecks — the spec forbids negative dy.
- **No structure variants.** One blueprint per feature. Minecraft has structure pools with weighted variants; we have exactly one.
- **Biome gating is a probability map**, not a rule system. Can't express "only on cliff edges" or "only near rivers" — just per-biome chance.
- **Ecosystem diversity is thin.** We have terrain + trees/shrubs + water + structures. No: grass/flower layers, rock formations, biome blending/transitions, underground features (caves are carvers only), dimension-specific weather effects on generation, or fauna spawning tied to procgen.
- **Formula models as structures** is dotted-line only — the wiring exists conceptually but isn't a first-class path.

## 4. Proposed: ecosystem & environment diversity

1. **Ground-cover layer**: grass tufts, flowers, pebbles, fallen leaves — cheap single/2-cell features with high density, biome-tinted. Biggest visual-diversity win per line of code.
2. **Rock formations**: cellular-noise-ridged boulders, arches, spires — reuse the Formula voxelizer with erosion-ish modifiers.
3. **Biome edge blending**: transition bands (beach between ocean/land, scree at mountain feet) instead of hard biome borders.
4. **Underground features**: ore veins (3D blobs via Worley), small caves (carver worms), buried structure support (requires lifting the `dy >= 0` restriction).
5. **Dimension palettes**: per-dimension generation profiles (the Cosmic gets void islands, the Underwafer gets lava tubes) driven by the same pipeline with different JSON.
6. **Fauna hooks**: spawn-table entries attached to biomes at gen time (passive mobs where grass is dense, etc.) — placement data the entity system reads.
7. **Weather-shaped generation**: snow caps above a height + latitude function, sand drifts on the windward side — static baking of dynamic systems.

## 5. Proposed: engine-level Structure Registry

**The ask**: JD designs structures (in-game, or as `.lthm` voxel models, or as JSON blueprints), registers them once at the engine level, and worldgen JSON references them **by name**. The procedural engine places them when it finds a valid spot, Minecraft-style.

**Design:**

```
StructureRegistry (engine, singleton like the block registry)
  ├── register(name, source)      // source: .lthm path, JSON blueprint, or in-memory cells
  ├── placement rules per entry:  // density, biomes, surface, spacing, y-range, rotation
  └── query(name) -> blueprint    // StructureFeature resolves names at pipeline build
```

- **Sources**: `.lthm` voxel models (JD builds in the game's editor, exports), JSON blueprints (current format), and in-memory (mods registering at runtime).
- **Placement solver** (new, alongside `StructureFeature`): valid-spot search with **separation** (min distance between structures, per-structure and global), **exclusion zones** (no village inside a volcano crater), **variant pools** (weighted list — "ruin" picks from 5 layouts), **y-range** (buried/underwater/floating — lifts the `dy >= 0` limit), **slope check** (reject if terrain gradient too steep — fixes floating structures).
- **Worldgen JSON** references by name: `"structure": "jd:watchtower"` instead of inline cells. Inline stays supported (backward compatible).
- **Mod API**: `registerStructure(id, def)` during mod init, same pattern as `registerBlock`. Sealed registry after boot (same seal policy as blocks).
- **Determinism**: unchanged — registry is built before generation, placement stays a pure function of (seed, cell).

**Migration path**: Phase 1 — registry + name resolution in `StructureFeature` (inline still works). Phase 2 — placement solver with separation/variants/slope. Phase 3 — `.lthm` as a structure source + in-game structure editor export.

This is engine-level, mod-extensible, and gives you the Minecraft-style "design it, register it, the world places it" loop.
