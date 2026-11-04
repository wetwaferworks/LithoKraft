# Detailed Implementation UML — LithoLattice Engine + LithoKraft Game

Implementation-level class diagrams, every solid box verified against the actual
source tree. The earlier subsystem-level diagrams live in `../uml/`.

## Diagram index

| Diagram | Files | Subsystem |
|---|---|---|
| Engine core | [engine-core.png](engine-core.png) / [engine-core.dot](engine-core.dot) | sim (DeterministicScheduler, TickArena, ITickable, Trace JIT), core (JobSystem, TripleBuffer), console (ConsoleRegistry, tokenizer, completer, delimiter pairing), storage (RegionFile, AnvilRegion, atomic writes), mods (C-ABI + ModLoader) |
| Procgen + subdivision | [procgen.png](procgen.png) / [procgen.dot](procgen.dot) | INoise + 7 impls, NoiseGraph (18 node types), Litho Formula, GeneratorPipeline + 7 stages, 5 ICarvers, 10 IFeatures, water rules, loader, structures, trees/shrubs, SparseOctree, micro-mesher, .lthm pipeline |
| Platform — render | [platform-render.png](platform-render.png) / [platform-render.dot](platform-render.dot) | IRenderer hierarchy (GL33 / Software / Vulkan), factories, ChunkMesh/PackedVertex, BinaryQuad + PBR coexistence |
| Platform — services | [platform-services.png](platform-services.png) / [platform-services.dot](platform-services.dot) | audio (4 decoders), net (channels, handshake, BlockUpdate V3), scripting (AngelScript sandbox), mods, UI/input, fluids, lighting, MCP, AI, assets, index, defrag, crypto |
| Game — Fragment | [game-fragment.png](game-fragment.png) / [game-fragment.dot](game-fragment.dot) | FragmentState (tiers, upgradeFragment, LFS1 v2), markdown renderer (deliberate subset), FragmentGallery (LFG1), engine seam extractComments |
| Game — world | [game-world.png](game-world.png) / [game-world.dot](game-world.dot) | Game orchestrator, dimensions (wafer + organic registered), block registry + -OR family, SignalSim, ChipOR viewport, silica farming, vegetation binding, entities, EtchOR→ChipOR, MCP adapters |

## Legend

- **White box** — real code, verified by reading the source.
- **Blue box / cluster** — engine seam (game→engine boundary).
- **Dashed gray box, «planned»** — decided but NOT in code.
- **Dashed amber box, «stub»** — exists but deliberately unimplemented inside.
- **«placeholder»** — real code that does nothing yet (e.g. DungeonSlot).
- Inheritance = empty arrowhead; usage = labeled arrow; composition = filled diamond.

## Missing-item inventory (all verified against code)

### Engine core
1. **IRegistry** — «planned». Zero hits tree-wide. Concept-only (dotted naming for cvars/settings/mod properties).
2. **Bytecode Trace JIT VM** — «planned». `BasicTraceJit` flattens node pointers; headers say "no real bytecode"; factory commented out.
3. **ClockDomain** — stub; header literally says "STUB: no logic yet".
4. No `config/` module — cvars live in **console** (lithoscript), not a config module.
5. No C++ `IMod` class — mod system is C-ABI structs (`LithoLatticeMod`, `LithoLatticeModApi`) + `ModLoader`.

### Procgen + subdivision
6. **Multi-block trees** — follow-up. v1 ceiling: one 64-cell `SparseOctree` per tree.
7. **Per-dimension JSON wiring for terrain features** — OPEN game-design call. The pipeline-building mechanism exists (`loadGenerator`); the wiring decisions don't.

### Platform
8. **Camera system interface** — «planned». No `CameraSystem`/`ICamera` anywhere; only `camPos` fields.
9. **Codec stubs** — WAV decodes fully; FLAC/Opus/Vorbis parse container+headers only, `decode()` not implemented. Vendoring is a builder decision.
10. **Encryption** — `CryptoSession`/`NullCipher` present; real ciphers are stubs.
11. **SoftwareRenderer** claims `IRenderer` inheritance in its comment but the class declaration has **no base class** (shown with a dashed "not yet" arrow).

### Game
12. **The Cosmic** — NOT in `createDimensions()` (wafer + organic only); NO `cosmic.json` (only `wafer.json`, `organic.json`); the id string exists only in `McpPlayerController::dimensionNumber()`.
13. **The Spark** — "a character, not yet in code" (fragment_state.h). `upgradeFragment()` has **no real caller** — only the selftest hook.
14. **The Oracle** — zero code mentions. Lore only.
15. **Fragment UI** — doesn't exist. `RenderedDoc`/`FragmentGallery` are models awaiting a UI. Checklist checked-state is NOT persisted.
16. **EtchOR / LithographOR** — block registration is commented out / DISABLED; `SimRole::LithographOR` is an enum entry only. No machine prints chips from builds.
17. **LBT** — zero code mentions. No ChipOR name field; per-block names need block-entity plumbing that doesn't exist yet.
18. **Vegetation placement** — mapping table only (`kBiomeVegetationTable`); the decoration/placement stage is not implemented.
19. **6 approved biomes not in wafer.json** — EtchedPlains, ClockwoodForest, DopantGroves, OxideDunes, StaticMarsh, FabRuins (Organic).
20. **The Daemon** — channel 0 reserved ("never unlocks"); the Dish "hears channel 0". No entity in code — comments only.
21. **Grimlores / the Box** — zero code mentions. In-code entities: item drops + falling blocks only (item rendering itself is TODO).
22. **Radio unlock progression (4→16→64)** — `setRadioChannelsUnlocked()` exists but has no callers.
23. **Builder seams** — `ILighting` not plumbed into sim (game passes null); `ExtensionLoader` wiring into mod startup not done; `meshMiniatureLOD` is a reference impl awaiting `subdiv::meshOctreeLOD`.

## Caveats / couldn't-verify

- `game-world.png` (8848×5434) and `procgen.png` (10855×9632) are large by necessity — zoom/pan to read. The others fit one screen.
- `render_vk` internals (vk_device/swapchain/pipelines) mapped shallowly: only the `VulkanRendererDesc` contract + factory.
- `headless_harness.h` carries a stale doc comment (registry 40 vs 36); blocks.cpp now has 56 rows. Doc needs a refresh.
- Code's Organic display name is "Organic World" (id `organic`); lore docs say "Organic".
- Uncertain relationships were deliberately omitted rather than guessed.
- PNG metadata audited: only IHDR/bKGD/IDAT/IEND chunks — no timestamps, no author data.
