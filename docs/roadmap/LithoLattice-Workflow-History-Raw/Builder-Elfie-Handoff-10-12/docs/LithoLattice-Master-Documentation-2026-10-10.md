# LithoLattice Engine & LithoKraft Game — Master Documentation
October 10, 2026. Compiled by Elfie. Single source of truth.

---

## Engine: LithoLattice

**What it is:** A voxel game engine. Moddable via IMod. LithoKraft is the first-party game.

### Core principles
- **Powers of two** throughout (64 is the theme; 42 is the deliberate exception for inventory).
- **1-based indexing** (0 = off/reserved).
- **Deterministic** (tick counter drives everything; no wall-clock in sim).
- **512 TPS** sim, 32 Hz network snapshots.

### Interfaces (engine/)
| Interface | Purpose |
|-----------|---------|
| IMod | Mod lifecycle, permissions, UI/input/assets |
| ISignalNode | Trace components (wired logic) |
| IProjectile | Combat projectiles (chunk-loading) |
| IWeapon | Melee/projectile/shockwave |
| ILighting | 1-64 light, sky per dimension, mob spawn check |
| ISpawning | Mob spawning (1 Hz, sampled) |
| IFluidSim | Fluids 1-16 (water/lava/chemical) |
| IEntityBehavior | Mob AI |

### Key systems
- **Trace**: wired (red/blue), 1-64 values, -OR components
- **Signal**: wireless, 1-64 channels, TX power 256, Dish sensitivity 1024
- **Lighting**: 1-64, sky per dimension, mobs spawn < 16
- **Fluids**: 1-16 levels, water/lava/chemical
- **Projectiles**: chunk-loading, density penetration
- **Spawning**: 1 Hz, 8 samples/chunk, uses ILighting
- **Day/night**: tick-driven, engine exposes time
- **Weather**: particle-based, affects gameplay
- **Audio**: 3D spatial, voxel acoustics, procedural+sampled
- **Profiler**: all metrics, for scaling

### Code structure (platform separation)
- `engine/src/platform/` — platform-specific ONLY (window, GLFW input backend)
- All other `engine/src/*/` — platform-agnostic
- Minimal `#if defined(_WIN32)` in core (2 files: log.cpp, process_memory.cpp) for tiny API differences only — this is standard practice, not scattered
- Platform backends selected at CMake configure time

### Modding (IMod)
- Static link v1, DLL-ready. No STL/exceptions/RTTI across boundary.
- Versioned structs (size first). SDK versioning.
- Permissions: content (silent) vs script/native (prompt).
- First-party flag for team mods.
- UI, input actions, assets all mod-exposable.

---

## Game: LithoKraft

**What it is:** First-party mod on LithoLattice. Uses IMod + private engine access.

### Dimensions (7)
0. Underwafer (±131K) | 1. Wafer (±65K) | 2. Organic (infinite)
3. Void (Daemon) | 4. Space | 5. Ether (post-Daemon) | 6. Undefined (post-Daemon)

### Core loops
- **Lithography**: build → JIT-compile → chip (faster, smaller)
- **Krafting**: hand (2x2) → Fab Table (5x5) → Lithography
- **Combat**: 0-64 HP, powers-of-two damage, density penetration
- **Inventory**: 42 slots (8+2+32), 8 gear, 512 stack

### Key mechanics
- Trace/Signal as above
- Fab Table (5x5) for complex recipes
- Smelting (furnace TBD)
- Mobs per dimension, Daemon in Void
- Lighting < 16 spawns mobs
- 8-minute item despawn

---

## Naming locks
- LithoKraft (L+K only), LithoLattice, Wet Wafer Works
- -OR components, I-prefix interfaces, k-prefix constants
- Trace (wired), Signal (wireless), interfaces (not portals)
- IMod (not IMAD)

## Theme numbers
- 64 (theme), 42 (inventory), 1-64 (1-based), 512 (TPS/stack)
- 5 dimensions = 5 senses, 7 dimensions = 7 senses/cultures
- Powers of two everywhere else

## From Cortana review (2026-10-10)

**Tick progression:** 512 TPS is launch baseline, NOT ceiling. Cap is power-of-2 (1024-8192 proven in static scenes). Higher tiers for lithographed chips, enthusiast tier on stronger hardware. "The forever game."

**Trace components:**
- **Catchor:** 1-64 match value (sneak+use), first-touch auto-connect, L/C/R output (never up/down). Into Redirector acts as input. Outputs colorless.
- **Redirector:** colorless output (red-to-blue transfer), split lanes, per-lane off.
- **Watchor** (not Watcher). Full -OR naming.

**Block properties:** glass and frosted glass (carry Trace, never powered, cut stairs), ceramic (insulator). Repeater into silicon gives through-block hard power. No Vertor.

**Rendering:** Screen tearing at high TPS is a feature (players build double buffering).

**Pattern ROM:** texture family blocks (circles, checkers, spheres, cells, Delaunay). WIP lost, needs rewrite.

**GPU rules:** bandwidth first, bit-pack booleans, sparse active lists, compiled chips on GPU compute. Step 2 done, Step 1 renderer pending.

**Engine notes:** volumetrics/particles (deterministic opacity), advanced lighting tiers, script textures (TexGen-style), Daemon record/replay.

**Debug console:** give, tp, tps, time, pattern loader. TPS switching.

**Daemon AI:** utility AI + behavior trees, learned model nudges scores. Deterministic record/replay for demos/training/speedrun proof.

**C naming (JD locked):** C++ stays `litholattice::`. C ABI uses `LithoLattice` prefix for types, `litholattice_` for functions. Replaces `Ll`.
