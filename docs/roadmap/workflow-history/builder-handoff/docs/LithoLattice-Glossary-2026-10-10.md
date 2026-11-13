# LithoLattice / LithoKraft Glossary
October 10, 2026. Every term in one place.

## Engine

- **LithoLattice** — the voxel game engine. Moddable via IMod.
- **Tick** — one simulation step. 512 TPS baseline.
- **TPS** — ticks per second. 512 launch, up to 8192 ("the forever game").
- **Deterministic** — same inputs → same outputs, every time. No wall-clock in sim.
- **ISignalNode** — interface for Trace components (the IR for JIT).
- **JIT** — just-in-time compiler. Compiles ISignalNode graphs to faster form.
- **CompiledGraph** — JIT output: optimized, faster to tick.

## Trace (wired)

- **Trace** — wired signal system (red/blue). Like redstone but 1-64.
- **Red Trace** / **Blue Trace** — the two wire colors. Never connect to each other.
- **Signal value** — 1-64 (0 = off). Powers of two theme.
- **-OR naming** — all components end in -OR: Repeator, Catchor, Portor, etc.
- **Via** — vertical wire (red/blue side by side, never link sideways).
- **Portor** — lithography port. Defines input/output for a chip face. Dumb by design.

## Signal (wireless)

- **Signal** — wireless system. 1-64 channels.
- **Transmittor** — sends wireless Signal.
- **Receivor** — receives wireless Signal.
- **TX power** — 256 (tuning number).
- **Dish** — long-range receiver. Hears channel 0 (the Daemon).

## Lithography

- **Lithography** — build big → JIT-compile → print small → build bigger.
- **Lithographer** — the machine block that prints chips.
- **Chip** — printed block containing a JIT-compiled Trace graph.
- **Port** — a doorway to the smaller, faster circuitry. 6 faces = 6 bits = 64 states.
- **16x16** — starter lithography area. Tiers up to 512x512.

## Blocks

- **BlockDef** — block definition (name, flags, shape, texture, behavior).
- **Density** — resists projectiles (0 = air, 255 = unbreakable).
- **Falls** — sand-like behavior (becomes entity when unsupported).

## Rendering

- **Mesher** — converts voxel data to triangles.
- **PackedVertex** — 16-byte vertex format.
- **Software renderer** — CPU rasterizer. No GPU needed.
- **FXAA** — fullscreen anti-aliasing (post-process blur).
- **Normal mapping** — per-pixel normals from texture.
- **Bump mapping** — per-pixel normals from heightfield.

## Game (LithoKraft)

- **LithoKraft** — the game. First-party mod on LithoLattice.
- **Kharge** — electrical-charge morality. -64 (red) to +64 (blue).
- **IStat** — generic player stat system (engine).
- **IAbility** — active abilities that boost stats (engine, -OR naming).
- **Fab Table** — 5x5 crafting station.
- **42** — inventory slots (8 hotbar + 2 offhand + 32 main). The exception to powers of two.

## Dimensions

- **Wafer** (±65K) — main world. **Underwafer** (±131K) — below.
- **Organic World** — alien, infinite. **Void** — Daemon's domain.
- **Space** — above. **Ether** — post-Daemon, 6th sense. **Undefined** — post-Daemon, 7th sense.
- **Interface** — dimensional travel method (not portals).

## Modding

- **IMod** — mod interface. C ABI, versioned structs, opaque handles.
- **mod.json** — mod manifest (id, version, SDK range, permissions).
- **First-party** — team mods (extra engine access). LithoKraft is mod #1.
- **AngelScript** — scripting language. 1024µs budget, deterministic, sandboxed.

## AI / Mobs

- **The Daemon** — final boss. D.A.E.M.O.N. = Distributed Autonomous Execution & Maintenance Operations Network.
- **Kharge** — AI reads this to react to players.
- **Sixth sense** — fourth-wall breaking (post-Daemon, the Ether).

## Numbers

- **64** — the theme. Signal values, light levels, Kharge, chip faces.
- **512** — TPS baseline, max stack size.
- **42** — inventory slots (the deliberate exception).
- **1-based** — everything starts at 1, 0 means off.
- **Powers of two** — everywhere except 42.
