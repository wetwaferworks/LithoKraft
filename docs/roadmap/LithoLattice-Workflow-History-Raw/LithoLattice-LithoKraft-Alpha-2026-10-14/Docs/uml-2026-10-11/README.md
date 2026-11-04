# UML diagrams — 2026-10-11

Fresh diagrams generated 2026-10-11 (~23:30) while the code review ran.
Supersedes the morning's stale diagrams. Every box/arrow verified against
the tree at ~/workspace/litholattice (class names, function signatures, and
relationships checked in headers/sources; uncertain relationships omitted).

Sources are Graphviz DOT (rendered with `dot -Tpng -Gdpi=130`).

| File | PNG | Covers |
|---|---|---|
| engine-architecture.dot | engine-architecture.png | Engine subsystems: core, sim, procgen, subdiv, render, console, storage, platform services (net/script/mods/audio/entity/fluids/lighting) |
| engine-procgen.dot | engine-procgen.png | Procgen class detail: NoiseGraph, GeneratorPipeline stages 1–6, all 5 ICarver impls, 7 IFeature impls, surface rules (196/98 depth caps), tree/shrub generators |
| lithokraft-game.dot | lithokraft-game.png | LithoKraft game on the engine: Game, Fragment (FragmentState tiers, markdown renderer), content, lithography machines (EtchOR→ChipOR), dimensions, entities, engine seams |

Simplifications (accuracy over beauty):
- LBT (LithoBinary Tagged) is shown as a note — the name is decided but no LBT class exists in code yet.
- The Spark / Oracle / Grimlores are lore notes, not code classes.
- Lore-only systems (the Bake, Kharge, the Box mechanics) are not diagrammed.
- Per-biome JSON wiring and worldgen placement integration are open items, shown only as config edges.
