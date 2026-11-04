# LithoLattice Engine — Brief for Grok
Compiled 2026-10-10 from JD's locked decisions. "Locked" = final. "Proposed/Open" = undecided — do not treat as decided.

## 1. What LithoLattice is

LithoLattice is a unified, moddable, open-source voxel game engine written in C++20 (~21,000 lines — small and dense by design). It is free/open-source with an Unreal-style revenue share when projects profit. The vision: lower systems → gameplay systems → engine systems, so players can design their own games on the tick/sim foundation. Design philosophy: lore defines gameplay defines engine.

**Engine/game separation (load-bearing principle):** early builds had game logic baked into the engine; it has been refactored into a clean split. The engine exposes a mod interface; games are built on top like everyone else. **Dimension bounds, world sizes, and similar limits are game-level constraints, not engine limits** — the engine supports infinite/unbounded generation as far as mathematically possible (coordinate precision is the only ceiling); games and mods apply their own borders on top.

**LithoKraft** (only L and K capitalized) is the first game on the engine — a first-party mod and showcase. It uses the public IMod path like everyone else, but the engine exposes an `isFirstParty` flag granting direct engine access not available to third-party mods.

## 2. Module layout (engine/src)

`ai, assets, audio, core, entity, input, mods, net, platform, player, procgen, projectile, render, render_vk, script, sim, storage, ui, world`. One-way dependencies. Platform-specific code lives only in `engine/src/platform/` (no scattered ifdefs in core).

## 3. The tick system — 512 TPS deterministic sim

- **512 TPS is the launch baseline (locked).** 8192 TPS is an aspirational benchmark goal, not a shipping promise. No artificial upper limit: tick rates scale with hardware ("go low and aim for the sky"), with tick budgets keeping weak machines playable.
- **Deterministic simulation is the anti-cheat.** Fixed timestep, integer math, deterministic RNG. Servers choose authoritative or non-authoritative (non-auth = faster, less secure).
- **Moore's-law tick progression:** tick speed itself is a progression curve — each lithography node shrink unlocks the next power of two (1024, 2048, 4096…), plus an honestly-labeled enthusiast overclock/benchmark toggle. Chip clock domains run at their own rates (EUV tier = 512 Hz via JIT); the sim runs one master clock and steps each domain on integer dividers — no drift.
- Spiral-of-death guard: backlog is capped; with determinism, excess time is dropped (desync), never silently simulated.
- Networking: sim stays at 512 TPS server-side; **snapshots go out at 32 Hz** (locked earlier as 2^5; JD later set a **64 Hz consumer target** with presets for 64/256/512/1024 Hz — treat 64 Hz as the current direction). Clients interpolate. Protocol versions negotiated at connect; engine versioning is semantic.

## 4. IMod — the mod interface

**IMod** is the locked name: a C-compatible, DLL-ready, Half-Life-SDK-style API (size-first versioned structs, opaque handles, plain C types — no STL/exceptions/RTTI across the boundary). The engine owns public stubs/callback tables; mods hook lifecycle, tick, events, render, input, UI, client-frame, camera, entity, and inventory pipelines. Permission-gated capabilities (UI panels, input actions, asset loading) prompt the user. Mods declare their SDK range in `mod.json`; forward compat via versioned structs, backward compat fails gracefully. **Mod crash ≠ engine crash:** the engine logs, continues, and falls back to the splash screen if the game fails to load. Mod conflicts: notification, first registered mod wins. SDK parity rule: anything LithoKraft needs that mods can't get is an SDK bug.

## 5. AngelScript scripting

AngelScript 2.38, hooked into the engine's scripting API. Budget: **1024 µs (2^10)** per mod — throttle then suspend, player notified via debug log. Scripts can define basic Trace components (behaviors/events only, via ISignalNode). Bindings: hand-written for control + generated headers. Sim-thread scripts allowed with brutal budgets and automatic revocation. Daemon's learned model stays closed core; modders get scoring hooks and a "final boss slot."

## 6. Rendering

- **Vulkan primary, OpenGL 3.3 / OpenGL ES fallback, CPU software renderer** (full-fledged v2: per-pixel lighting + basic shaders for Chromebooks/GPU-less machines), plus headless/null for testing.
- **Four transparency methods implemented as engine fallbacks**, user-selectable as Low/Medium/High/Ultra graphics tiers: screen-door dithering, alpha blending with back-to-front sorting, weighted blended order-independent transparency, additive.
- Meshing: per-sub-chunk face-culled meshing with per-vertex ambient occlusion; greedy meshing in progress; 4-byte vertices; texture arrays.
- Lighting: **1–64 light levels** (0 = dark); mob spawning below 16; per-dimension sky light (engine-side); day/night driven by deterministic tick counter; voxel particle-based weather (lightning interferes with Signal).
- **PBR-lite (proposed, after the six optimizations land):** NO full global illumination (maybe later). Simplified Cook-Torrance, energy-conserving. Must handle MANY lights (torches everywhere) via hybrid: existing light-level flood fill covers diffuse for unlimited lights; K nearest emissive blocks become analytic lights for specular. Vulkan + software renderer.
- **PBR material registry (locked):** materials are data-driven, never hardcoded. Voxels store only a compact index — 8-bit per-voxel palette index into a per-chunk palette of 16-bit global material IDs. Runtime registry maps string IDs ("lithokraft:iron") → PBR params (albedo/texture ref, roughness, metalness, emissive, transparency method). Mods register materials via data files; saves persist string IDs. Transparent tiers: simple shading (default) → screen-door full-PBR → PBR reflections + simple transmission → true refraction via scene sampling (Ultra).

## 7. Audio

Procedural + partially sampled (JD + AI team). `spatial_audio.h`: distance attenuation, stereo panning, wall occlusion, speed-of-sound echo delay, voxel reverb estimated from nearby solid blocks, tick-based event aging, and **`audibleSounds()` — mobs query what they can hear, so the Daemon tracks player footsteps via echoes.** Covered by 6 self-tests; full audio self-test 19/19 PASS. A 32 Hz occlusion worker thread (16 Hz fallback) is in progress, clean-room, no external DLLs.

## 8. Entities, physics, items

Greenfield `litholattice::Entity` / `EntityManager` / `IEntityBehavior`: IDs, type, position/velocity, age, payload, ground/dead state, spawn/despawn/tick/query, behavior factories. `moveCollide` does axis-separated AABB-vs-voxel collision. Item entities: gravity, 8-minute despawn (8 on its side = infinity), player magnet.

## 9. The six optimizations (in implementation as of 2026-10-10)

Sourced from a 55-entry research survey (`Voxel-Engine-Optimization-Research-2026-10-10.md`); all six approved for implementation:
1. **Per-tick bump allocator** (core) — arena-allocate all transient sim data per tick, reset at tick end; kills malloc churn at 512 TPS.
2. **Flood-fill audio occlusion** — flood outward from the listener through voxels once per tick instead of raycasting per voice.
3. **Binary greedy meshing** — bitwise 64-face-at-once greedy meshing (~50–200 µs/chunk), 8-byte quads.
4. **Crash-safe region layer** (storage) — 32×32 chunk grouping for locality over the SGNL v2 bytecode save, with per-record CRC32 and payload-first/header-second commits (the opposite of Minecraft Anvil's crash-unsafety).
5. **Snapshot networking** (net) — decouple 512 TPS sim from 20–60 Hz snapshot transport; interpolation + acked-baseline deltas + fixed-point quantization.
6. **Deterministic parallelism scheduler** (sim) — systems declare Read/Write access, conflict-DAG stages, ordered per-system command buffers, per-entity keyed RNG; threaded execution bit-identical to single-threaded.

## 10. Other engine systems

- **IStat** (generic player stats), **IAbility** (binary verbs that boost stats; -OR names like Sprintor).
- **IWeapon / ITool / IProjectile / IExplosion**: tools talk to blocks, weapons talk to entities; projectiles support return-to-owner (boomerang); block density 0–255 resists projectiles (Half-Life-style penetration).
- **ISpawning** (engine decides when/where, game decides what), **ILighting** (composition, not inheritance), **IFluidSim** (levels 1–16; water/lava/chemicals, moddable).
- **Save format:** bytecode world save (SGNL v2 for signals) — better compression and byte alignment than LevelDB; dirty checks decide what saves; block ID migration via engine update function.
- **Profiler** (core): tick/render/network/memory metrics. **Debug console** (core + UI). **Colorblind mode** (core). **Localization:** seven cultures. **Key remapping** (engine, mod-exposed). **Voice chat** in-game, **cross-play** (PC + consoles; mobile/Switch undecided), server browser (direct IP + master server), auth via X + Steam.
- **ICharge**: engine exposes the charge mechanic optionally so mods can ignore it (Kharge itself lives in LithoKraft).

## 11. Conventions Grok must follow

- Namespace `litholattice::`, interfaces get `I` prefix, constants get `k` prefix, components get `-OR` suffixes, wired = **Trace**, wireless = **Signal**. These were decided with Grok and Cortana and must stay consistent — the builder takes over implementation ~2026-10-12.
- Player-facing indexing is **1-based** (1–64); 0 always means off/reserved.
- Powers of two everywhere; 64 is the theme number. (42 is the deliberate exception.)
- Never invent JD's business metrics — none are verified; treat as unknown.
- The squad: Grok (main coding partner, weekly cap resets 2026-10-12), Cortana (intel/clarity, engine spec + Workstation), the builder (implementation from ~2026-10-12), Elfie (design + interim implementation until the builder returns).

## 12. Honest implementation caveats (do not overclaim)

Several 2026-10-10 additions are scaffolds, not finished systems: Trace JIT is interface-only (builder has a full plan doc); the software renderer is not yet an IRenderer backend; GPU Trace strength-buffer upload is unverified on real GPU hardware; cross-dimensional Signal is partial (dimension byte plumbed, game never calls setDimension yet); Vulkan can't init on GPU-less machines (environmental). Treat "implemented" claims in logs as "in-tree, tests passing where stated" unless a full run is cited.
