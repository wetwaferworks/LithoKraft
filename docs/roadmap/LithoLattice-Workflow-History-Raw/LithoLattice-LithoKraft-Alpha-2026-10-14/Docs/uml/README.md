# UML diagrams

Architecture diagrams for the engine and the game. Every box/arrow was
verified against the source tree (class names, function signatures, and
relationships checked in headers/sources; uncertain relationships omitted).

Sources are Graphviz DOT (rendered with `dot -Tpng -Gdpi=130`).

| File | PNG | Covers |
|---|---|---|
| engine-architecture.dot | engine-architecture.png | Engine subsystems: core, sim, procgen, subdiv, render, console, storage, platform services (net/script/mods/audio/entity/fluids/lighting) |
| engine-procgen.dot | engine-procgen.png | Procgen class detail: NoiseGraph, GeneratorPipeline stages 1–6, all 5 ICarver impls, 7 IFeature impls, surface rules (196/98 depth caps), tree/shrub generators |
| lithokraft-game.dot | lithokraft-game.png | Game on the engine: Game, Fragment (FragmentState tiers, markdown renderer), content, lithography machines (EtchOR→ChipOR), dimensions, entities, engine seams |

Simplifications (accuracy over beauty):
- The tagged binary format (LBT) is shown as a note — the name is decided but no format class exists in code yet.
- Story/lore elements attached to the Fragment are notes, not code classes.
- Per-biome JSON wiring and worldgen placement integration are open items, shown only as config edges.
