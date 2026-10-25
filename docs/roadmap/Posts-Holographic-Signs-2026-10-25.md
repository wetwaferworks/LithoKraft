# Posts (holographic signs) - roadmap entry, 2026-10-25

Requested by JD 2026-10-25 10:10 ET. Status: planned, not started.

## What
"Posts" are LithoKraft's signs: a holographic look, placed like Minecraft signs
(standing on a post, or flat on a wall), crafted from the wafer's virtual or
organic woods (the trace-trunk wood family; see items.cpp "Wafer's wood
equivalent" and procgen trees such as clockwood wirewood).

## Phases
1. Placeholder Post block: partial block, two variants (standing, wall) picked
   from the clicked face, like the torch / wall-torch pair (attachFaceFor in
   world/block.h, kTorchModel / kTorchWallModel box models in
   content/appearance.cpp). Pick shape via pickShapeBoxes (c7fe3f0).
2. Holographic placeholder look: translucent, emissive-tinted panel. Needs a
   translucent/blend path for block models (no per-block translucent flag
   exists yet; the vulkan blend pass is used by other draws today).
3. Recipe from virtual/organic woods (one Post per wood type, or one tinted).
4. Texture slot for Cloud (CloudAtlas): `post` and `post_wall` keys in
   resources.json, placeholder until art lands.
5. Text feature (deferred): per-block text storage in the save, edit UI, and
   text rendering in the world.

## Why not done as a quick placeholder now
It is not small: it needs a new frozen save id (blocks.names; selftest at
selftest.cpp:595 freezes ids), a new translucent block-model path in both
renderers, two box models with facing, a recipe, and resources.json slots.
Each of those risks the scenario hashes and the 48-test gate, so it should be
its own pass.