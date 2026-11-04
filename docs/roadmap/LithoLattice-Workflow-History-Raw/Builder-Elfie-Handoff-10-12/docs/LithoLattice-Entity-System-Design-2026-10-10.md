# LithoLattice Entity System — Design
October 10, 2026. Elfie. Greenfield: no entity module exists yet.

## Why now
Popped trace needs to become an item drop (JD locked 2026-10-10, Minecraft-style). Item entities are the first citizens; mobs, agents, and projectiles follow on the same foundation.

## Engine (LithoLattice): the `entity` module
```
Entity            // id, type, pos, vel, polarity, age, data
EntityManager     // spawn / tick / despawn, spatial query, save/load
IEntityBehavior   // per-type tick (item physics, mob AI hooks)
```

- **Entity**: `uint64_t id`, `uint32_t type` (game-defined), `double x,y,z`, `double vx,vy,vz`, `uint8_t polarity` (the Binary Pair Principle — every entity has a pair value; items use 0), `uint32_t age`, `uint32_t data` (type-specific: item block id, count).
- **EntityManager**: owns entities for one dimension. `spawn()`, `despawn()`, `tick(dt)` (integrates physics, ages, culls), `querySphere()` (pickup checks, AI senses). Ticks on the game thread (not the sim thread — entities are presentation-adjacent; the sim stays blocks-only).
- **IEntityBehavior**: game registers per-type behaviors. The engine calls `onTick(entity, dt)` and `onCollide`. Item behavior (gravity, ground rest, magnet-to-player, despawn timer) is the first implementation — game-side.
- **Save/load**: entities serialize per chunk (or per dimension) through the storage module. Item entities persist; transient ones (particles) don't.
- **Render**: the engine exposes an entity render hook (billboard/item mesh); the game supplies meshes. v1: items render as spinning mini block cubes.

## Game (LITHOKRAFT): item drops
- **Trace pop** (unsupported trace): spawns an item entity `{type: item, data: red_trace|blue_trace, count: 1}` with a small random velocity. (Replaces the current "set to air".)
- **Pickup**: within 1.5 blocks of the player, the item magnetizes; on contact it auto-collects. v1: collected items go to a simple player-held count (no full inventory yet — inventory is the next system).
- **Despawn**: 5 minutes, Minecraft-style. Blinking in the last 10 seconds (presentation polish, cheap).
- **Polarity**: items are polarity 0 (neutral). Mobs later get their dimension's pairing (P/N, +/-, M/F, signal/echo, matter/antimatter).

## Build order
1. Engine `entity` module: Entity, EntityManager, IEntityBehavior, save/load hooks. ✅ (this session: header + manager)
2. Game item behavior: physics, magnet, despawn.
3. Trace pop → spawn item entity (replaces set-to-air).
4. Player pickup → held count.
5. (Later) Inventory system, mob behaviors, polarity/breeding.

## Open questions
- Item stacking: merge nearby same-type items? (Minecraft does; propose yes.)
- Should items burn in lava / Kiln layer? (Flavor; propose yes in Underwafer heat.)
- Cross-dimensional items: do dropped items travel through portals with the player?
