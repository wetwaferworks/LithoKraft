# Marching Cubes by Way of Slabs — Design Capture (2026-10-13)

**Status: JD's idea, UNFINISHED — he was mid-message at 14:23 EDT when other
decisions interrupted. Nothing here is locked. This sheet captures his words,
grounds them in the current tree, and lists the decisions to put to him when
he picks the thought back up.**

## JD's idea, in substance (2026-10-13 18:23 UTC)

> "The first thing I think we're going to try to implement is marching cubes
> in a voxel form. Others usually avoided this because they use full blocks
> the whole time, but we can use half blocks — the slabs — to get the
> overhang. It'll kind of have the feeling of it and the look, and we'll be
> able to do a little bit different things…"

The message trails off there — "different things" was never specified.

## Why the slab trick is tractable here (verified in the tree, 2026-10-13)

- **Slabs are already a first-class engine shape.** `Shape` in
  `engine/src/world/include/litholattice/world/block.h` is
  `None / Cube / Cross / Slab / Custom` — a slab is not a hack, it is a
  geometry the mesher already builds.
- **Slabs already exist in the game** with textures and registry rows:
  `kSiliconSlab`, `kStoneSlab`, `kDirtSlab` (see structures.cpp row z=28 and
  the pack_16 texture recipes `*_slab_top/side.ltxr`).
- **No "marching" anything exists in the tree yet** (grep across engine +
  user files, 2026-10-13) — this is genuinely new work, not a rename.
- **The mesher seam is clean:** `meshSubChunk(MeshInput, MeshContext)` is a
  pure function on worker threads, and the standing locked decision is that
  BinaryQuad + PackedVertex coexist as user-selectable meshers — a third,
  slab-stepped mesher variant fits the established pattern instead of
  fighting it.
- **It is NOT the subdivision system.** The sparse 64³ octree is model/detail
  geometry (tools, mobs, ChipOR models) and is not sim-ticked. The slab idea
  is coarser and stronger: half-height steps (2 levels per block of height)
  give the marching-cubes *read* — sloped-looking terrain, real overhangs —
  while the world stays whole blocks in the sim, in saves, and in collision.

## The core mechanic, as Elfie understands it (proposal, not lock)

Where a full-block heightfield can only step 1 block at a time, a
slab-stepped surface steps ½ block at a time. Terrain (or a carved volume)
whose height crosses a half-block threshold emits a slab instead of a full
block or air. The silhouette gains the intermediate level marching cubes
gets from its iso-surface — overhangs and lips become expressible — at
block-grid cost: no new vertex soup, no change to the 512 TPS sim, world
permanence untouched because the stored world is still blocks + slabs.

## Decisions for JD when he resumes the thought

1. **Where does it live?** (a) Worldgen terrain only — hills/caves generate
   slab-stepped; (b) a mesher/render variant only — same world, smoother
   read; (c) player-placeable building grammar too. JD's wording ("try to
   implement… in a voxel form") leans worldgen, unconfirmed.
2. **Which axis?** Slabs as shipped are horizontal (half-height). Overhangs
   in JD's sentence suggest terrain lips; true marching cubes also steps
   sideways. Vertical/half-width variants would be new shapes — his call.
3. **Relation to the carver sweep already dispatched** (2026-10-13 18:28):
   the worldgen completeness crew is auditing carvers/noise now. Does the
   slab-stepping belong inside that program (cave lips, ravine rims) or
   stand as its own experiment tree like IDemoRecord?
4. **What were the "different things"?** The unfinished tail of his message.
   Only JD can finish that sentence — ask, don't guess.

## Status update (2026-10-13 ~19:44 UTC, background pass)

Two facts above have moved since this sheet was written:

- **The worldgen completeness crew is done, not auditing.** Its master
  report landed 19:21 UTC (selftest 1020/0 both modes; CheeseCarver built;
  Ruins upgraded). Decision 3's premise is therefore stale: the live
  question is now whether slab-stepping rides **Team B's worldgen port**
  (dispatched 19:42 UTC) or stands as its own experiment tree.
- **JD green-lit wiring everything not wired (19:41 UTC) and two 2-person
  teams were dispatched (19:42 UTC). Marching cubes appears in NEITHER
  team's work order** — Team A: control-serialization + IDemoRecord ports,
  column eviction, FragmentState callers/UI, render.fps_cap, projectile
  densities, AiWorld path. Team B: render + worldgen ports, Vulkan sprite
  pass, formula shrubs into live worldgen, Cosmic + Underwafer
  registration, EtchOR/ChipOR, certificate-signing mechanic, Void
  coordinate paper. That is expected, not an error: this idea is not
  "unwired," it is unbuilt and still missing JD's decisions 1, 2 and 4
  above — but it is the one item from today at risk of falling out of the
  sweep entirely if nobody names the gap.

## Delivery note

One short line when JD returns from his break or next asks what's queued:
"Your marching-cubes-by-slabs thought is captured on a sheet — slabs are
already a first-class shape in the engine, so it's more tractable than it
sounds; four decisions are waiting on it, including the end of the sentence
you didn't finish." Do not present any part of the mechanic sketch as locked.
