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
## Spec update (JD, 2026-10-25 10:13 ET)
- Purpose: label builds and leave directions readable in game.
- Two-block multiblock: a one-block stand below a one-block holographic
  panel. Placing needs both cells free; breaking either removes both.
- Standing or wall-mounted (wall: panel on the wall, stand below it).
- Up to 256 characters with word wrapping; editable on use; saved with the world.
- `/post <text>` sets the text of the Post under the crosshair.
- Made from the wafer's virtual or organic woods. Texture slots for Cloud:
  `post_stand`, `post_panel`, `post_panel_wall`.

## Scoped plan (each step gated: rc=0, 48/48, hashes unchanged)
1. Blocks: `lithokraft:post_stand`, `lithokraft:post_panel` with new frozen
   save ids appended (never renumber); facing in block state; box models and
   pick shapes like torch/wall torch. Two-cell place/break in the edit path.
2. Text storage: per-panel text keyed by block position in a region side
   table (e.g. a `posts` section next to r.x.z.llr), UTF-8, capped at 256, with
   save/load selftests. Old saves load with no posts.
3. Editing: use on the panel opens a small UI text box (reuse cTextBox and the
   console input focus path); `post` console command uses the sim raycast
   target; `/post` works via the new slash strip (ad080be).
4. Rendering: placeholder translucent cyan panel; text is drawn as a
   world-space quad from an offscreen text texture or HUD-projected labels
   within ~16 blocks (cheapest first step: HUD-projected text).
5. Recipe from virtual/organic wood; Cloud art drops into the slots.
Estimate: 2-3 focused passes. Hash risk is in step 1 (registry size).