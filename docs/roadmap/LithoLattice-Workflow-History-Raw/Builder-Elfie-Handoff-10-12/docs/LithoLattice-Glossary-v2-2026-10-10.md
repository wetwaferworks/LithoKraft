# LithoLattice / LithoKraft Glossary v2
October 10, 2026 — expanded from the v1 pass earlier today.

**New or heavily expanded since v1:** JIT & compilation, scene graph & profiling, wireless link detail, DRAM-in-a-chip and frame buffer patterns, inventory data model, software renderer v2, network model, save & versioning, audio/accessibility/input, combat & tools. Corrections welcome — locks are marked.

---

## 1. Engine core & code conventions

- **LithoLattice** — the voxel game engine. A unified framework: lower systems → gameplay systems → engine systems. Moddable via IMod.
- **litho engine / lattice engine** — shorthand for the LithoLattice engine. JD uses "litho engine," "lattice engine," and "litho lattice engine" interchangeably; they all mean this.
- **LithoKraft** — the first game on the engine. A first-party mod: uses IMod like everyone else, but with direct engine access the engine exposes via an `isFirstParty` flag. Spelled LithoKraft — only the L and the K capitalized.
- **litholattice::** — the engine C++ namespace. Locked.
- **I-prefix** — interfaces (IMod, IStat, IAbility, ITraceJit, ISignalNode, IWeapon, IProjectile, IExplosion). Locked.
- **k-prefix** — constants. Locked.
- **-OR** — component and block names end in -OR: Portor, Memor, Pulsor, Catchor, Repeator, Transmittor, Receivor. (Sortor was cut.) Locked.
- **Trace / Signal terminology** — wired is Trace, wireless is Signal, everywhere. Locked.
- **1-based indexing** — signal values 1–64, channels 1–64. 0 always means "off" (or reserved — channel 0 belongs to the Daemon). Player-friendly; no zero-based leaks. Locked.
- **Tick** — one simulation step. Baseline 512 TPS.
- **TPS** — ticks per second. 512 at launch; the "forever game" vision scales toward 8192.
- **Deterministic sim** — same inputs → same outputs, every time. No wall-clock inside the sim. Doubles as the anti-cheat foundation.
- **Script timeout** — 1024µs (2^10). Over-budget scripts are throttled, then suspended; the player is notified via the debug log. Locked.

## 2. JIT & compilation

- **ISignalNode** — the interface for Trace components. The intermediate representation (IR) the JIT consumes.
- **JIT (just-in-time compiler)** — compiles a player's physical Trace wiring (an ISignalNode graph) into a faster compiled form. The shrinking is the visual; the JIT is the optimization.
- **ITraceJit** — the JIT interface: `compile(nodes, count)` → `CompiledGraph`; `execute(graph)` → runs the compiled form; `free(graph)` → cleanup.
- **CompiledGraph** — JIT output. Holds the bytecode, the node count, and the tick cost. A chip is the physical item that holds a CompiledGraph.
- **Basic JIT (v1)** — flattens the ISignalNode array into contiguous storage for cache locality. No topological sort yet — it compiles, it runs, and it's faster than scattered node pointers by design.
- **Topological sort** — planned JIT optimization: order nodes so every node's inputs are computed before it runs.
- **Constant folding / dead-code elimination** — planned JIT optimizations, to be validated once real circuits exist to test against.
- **JIT correctness gate** — interpreted vs JIT run side-by-side for 2000+ ticks per scene, every Trace state compared. Same pattern as the existing incremental-vs-full-pass match.
- **Cache locality** — why the basic JIT wins: contiguous node storage instead of scattered pointers.

## 3. Trace (wired)

- **Trace** — the wired signal system (red/blue wires). Redstone treated as an engineering discipline; signal values 1–64.
- **Red Trace / Blue Trace** — the two wire colors. They never connect to each other.
- **Wire color auto-detection** — a Portor detects wire color on its own; no manual configuration. Part of the Portor simplification.
- **Via** — vertical wire. Red and blue run side by side and never link sideways.
- **Portor** — the lithography port. Simplified (locked): defines input OR output per face only. Outputs three shapes: solid on/off, pulse, or clock signal (from its internal JIT). Vias handle top and bottom. Dumb by design — a doorway to the smaller, faster circuitry. 6 faces = 6 bits = 64 states.
- **Memor** — memory cell. The storage primitive.
- **Pulsor** — pulse generator. The clock primitive.
- **Catchor** — catch/latch component. The hold-state primitive.
- **Repeator** — signal repeater. (-OR naming example.)
- **DRAM-in-a-chip** — the pattern: 64 Memors in a 16×16 lithography area, address 1–64 via Signal (or Trace) into a Portor IN, a clock pulse into a Portor IN to latch the read, the stored value out through a Portor OUT. Print it and one Chip block is a 64-word RAM.
- **Frame buffer** — 64×64 Color Lamps as the display, Memor banks as frame memory, a chip as the display controller — all built from the same primitives.

## 4. Signal (wireless)

- **Signal** — the wireless system. 64 channels (1–64, 1-based).
- **Signal Transmittor** — sends wireless Signal. TX modes: use toggles input/beacon; sneak+use cycles channels.
- **Signal Receivor** — receives wireless Signal.
- **TX power** — 256. A tuning number for in-game playtesting, not physics. Approved as the starting value.
- **Beacon** — Transmittor mode that broadcasts at channel 64. Also a tuning number.
- **1-tick latency** — every wireless link adds exactly one tick.
- **Link budget** — the radio registry's per-link accounting of range and power.
- **4→16→64 channel gating** — progression: 4 channels early game, 16 mid, 64 late.
- **Save migration** — wireless Signal blocks migrate old saves forward (built 2026-10-10).
- **Weather interference** — lightning interferes with Signal. Emerges from the engine weather system, not a special case.
- **Dish** — long-range receiver. Hears channel 0 (the Daemon). How forgiving its disc check is gets decided after in-game testing.
- **Channel 0** — reserved. The Daemon's channel. Everywhere else, 0 means off.

## 5. Lithography

- **Lithography** — the namesake mechanic: LithoKraft = lithography + craft. Build big → JIT-compile the graph → print small → build bigger. After a player builds a contraption (an encryption machine, signal fallbacks), lithography miniaturizes the blocks — makes them smaller.
- **Lithographer / Lithography Machine** — the machine block that prints chips. Tiers (visible / DUV / EUV) gate how far the shrink goes.
- **Lithography area tiers** — 16×16 (start) → 32×32 → 64×64 → 128×128 → 256×256 → 512×512 (max). Bigger machines handle bigger areas. Locked.
- **Chip** — a printed block containing a JIT-compiled Trace graph. Chips can contain chips: recursive miniaturization is the progression.
- **Lithography gameplay loop** — player builds the physical contraption → engine JIT-compiles the graph via ISignalNode → the chip runs faster (higher tick, lower cost).

## 6. Scene graph & profiling

- **Scene graph** — the engine's spatial/object hierarchy: how blocks, entities, and compiled graphs are organized for traversal, culling, and ticking. (Yes — the term is right.)
- **Graph engineering** — the discipline of building and optimizing the ISignalNode graphs the JIT compiles: node layout, edge costs, traversal order.
- **Profiler** — a core engine system. Tracks tick, render, network, and memory metrics for bottleneck detection while scaling. Locked.
- **Headless benchmark (`--perf-headless`)** — runs the sim with no window and no GPU. Reports tick average and p99, component counts, remeshes/sec. Respects `--bench-scene` (fixed 2026-10-10).
- **Bench scenes** — 8 code-generated test worlds (`--bench-scene pattern-wall --scene-size 64`, `wire-torture`, …). Generated from code, so every machine builds the exact same worlds and baselines compare across machines.
- **pattern-wall 64** — the JIT's best target: 13,199 components; 0.63ms headless on the laptop, 1.255ms on the dev VM. The worst case is the best optimization target.
- **wire-torture** — stress scene: 1,445 components; 0.048ms on the dev VM at 512 TPS (2.5% of budget).
- **p99** — 99th-percentile tick time. The number that matters for hitches — not the average.
- **Incremental-vs-full-pass match** — the existing self-test: incremental updates must exactly equal a full recompute. The template for the JIT correctness gate.
- **Tick budget** — at 512 TPS each tick gets ~1.95ms. pattern-wall 64 at 1.255ms is 64.3% of budget on the VM.

## 7. Rendering

**Renderer shorthands** (as used in the tree — file/dir prefixes):
- **vk** — Vulkan. Backend lives in `render_vk/`, files prefixed `vk_` (vk_device, vk_pipelines…). Primary high-end renderer.
- **gl** — OpenGL. Backend in `render/gl33/` (GL 3.3 target).
- **pbr** — physically based rendering. Material/shading files prefixed `pbr_` (pbr_brdf, pbr_materials…).
- **software** — the software renderer. Files prefixed `software_` (software_renderer, software_opt…). No `sw` shorthand in the tree.
- **Mesher** — converts voxel data to triangles.
- **PackedVertex** — 16-byte vertex format.
- **Software renderer** — CPU rasterizer. No GPU required: baseline testing, headless VMs, low-end devices. Locked.
- **Software renderer v2** — the full-fledged CPU path: per-pixel lighting + basic shaders (diffuse + ambient + textures). Chromebooks and GPU-less machines run the real game. Locked 2026-10-10.
- **Per-pixel lighting** — normals interpolated across each triangle (barycentric); diffuse (N·L) computed per pixel; ambient + diffuse model. Toggled with `setPerPixelShading(true/false)` — fast flat shading vs quality per-pixel.
- **FragmentShaderFn** — function-pointer "shader" interface for the CPU renderer. `basicFragmentShader` = texture sample + diffuse + ambient. `ShaderVaryings` carries normal, UV, layer, world position, interpolated per pixel.
- **Textures** — RGBA 16×16 tiles, sampled per face (`setTextures()`).
- **Transparency sorting** — two-pass: opaque first, transparent after. Per-triangle sort is a future optimization.
- **FXAA** — fullscreen anti-aliasing as a 3×3 blur post-process pass (`applyFXAA()`).
- **Normal mapping** — per-pixel normals decoded from a normal texture (RGB 0–255 → −1.0…1.0), used for lighting instead of the interpolated vertex normal. Falls back to vertex normals when unset. Implemented 2026-10-10.
- **Bump mapping** — same idea, but normals derived from a heightmap gradient (samples neighbors — pricier per pixel). Not implemented; waits on heightmap textures.
- **Tessellation** — deliberately not needed. Voxels are already quads (2 triangles per face); subdividing flat faces adds no meaningful detail. Normal mapping is the right tool for voxels.

## 8. World & dimensions

- **Wafer** — the main world. ±65,536 blocks (2^16) from 0,0. Locked.
- **Underwafer** — below the Wafer. Double: ±131,072 blocks (2^17). Locked.
- **Organic World** — alien, infinite. **Void** — the Daemon's domain, infinite. **Space** — above the Wafer.
- **Five senses** — design theme: the five core dimensions correspond to five senses. Locked.
- **Roadmap: 7 dimensions** — 5 core (Underwafer, Wafer, Organic World, Void, Space) + 2 post-Daemon (the Ether, the Undefined). The core/endgame loop stays with the 5. Locked.
- **The Ether** — post-Daemon dimension. Spiritual/heaven-like, tied to the 6th sense. (A friend's suggestion, via Chris S. from "314".)
- **The Undefined** — post-Daemon dimension, tied to the 7th sense.
- **Interface** — dimensional travel uses interfaces, not portals. Locked.
- **Day/night** — driven by the tick counter (deterministic); the engine exposes time-of-day; sky light varies with it. Locked.
- **Weather** — an engine system, voxel particle-based. Affects gameplay — lightning interferes with Signal. Locked.
- **Silicon Plains** — proposed starter biome (procedural generation). A pre-alpha item: design exists, no code yet.

## 9. Blocks, fluids & lighting

- **BlockDef** — block definition: name, flags, shape, texture, behavior.
- **Block density** — resists projectiles. 0 = air, 255 = unbreakable. Half-Life-style penetration. Locked.
- **Falls** — sand-like behavior: becomes an entity when unsupported.
- **Fluids** — engine interface, levels 1–16. Water, lava, chemicals (moddable). Locked.
- **Lighting** — 1–64 levels (0 = dark). Mobs spawn below 16 (a quarter of 64). Sky light is per-dimension, engine-side. Ray-traced approximation is future work. Locked.

## 10. Items & inventory

- **42** — carry slots, exactly: 8 hotbar + 2 offhands (left/right) + 32 main (4 rows × 8). 42 = 101010 in binary — alternating bits: 32+8+2. Intentional meme and lore. Locked.
- **8 gear slots** — worn, not held: head, chest, legs, feet, arms, face, heart, mind. Not counted in the 42. v1; may expand later. Locked.
- **512** — max stack size, equal to the tick rate. 42 × 512 = 21,504 items max carry. Locked.
- **Despawn** — dropped items despawn after 8 minutes. 8 on its side is ∞. Locked.
- **Open questions** — per-item stack sizes, offhand mechanics, pickup priority. These gate item rendering and inventory integration.
- **Fab Table** — 5×5 crafting station, 25 slots for complex recipes. The pre-lithography tier. Locked (design; not implemented).

## 11. Stats, abilities & Kharge

- **IStat** — the engine's generic player-stat system.
- **Kharge** — spelled with a K. Electrical-charge morality: the player spawns neutral (0); actions shift it from −64 (red/negative) to +64 (blue/positive). Visible in the UI. Engine-exposed and optional for mods. The AI reacts to it. LithoKraft's first stat (moved out of the engine). Locked.
- **IAbility** — the engine ability interface (-OR naming). Abilities are a separate future system. (Interface implemented 2026-10-10; UI not yet.)

## 12. Mobs & AI

- **The Daemon** — the final boss. Corruption incarnate; diplomacy always fails (freestyle lock). Hears channel 0. In multiplayer, the Void holds echoes of real dead players.
- **Sixth sense** — fourth-wall breaking: the program can look at the screen and give hints. Tied to the Ether (6th dimension). Locked.
- **Audio + AI** — procedural plus partially sampled audio, made by JD and the AI team. 3D spatial sound with voxel-based reflections and echoes (uses block solidity/opacity). Ties into the AI system. Locked.

## 13. Modding & scripting

- **IMod** — the mod interface. C-compatible, DLL-ready, Half-Life-SDK style. Mods declare their SDK range in mod.json. Forward compatibility via versioned structs (size first); backward compatibility fails gracefully. A mod crash doesn't take the engine down: log and continue. If the game fails to load, the engine falls back to the splash screen. Locked.
- **mod.json** — the manifest: id, version, SDK range, permissions.
- **First-party** — LithoKraft is mod #1. It uses IMod like everyone else, plus direct engine access the engine flags with `isFirstParty`.
- **AngelScript** — the scripting language. Budget 1024µs (2^10). Scripts can define basic Trace components (behaviors/events only). Bindings are hand-written plus generated headers for DLLs. Over-budget scripts are throttled, then suspended. Locked.
- **Mod conflicts** — the player gets a notification; the first registered mod gets priority. Locked.

## 14. Multiplayer & network

- **512 TPS sim / 32 Hz snapshots** — the sim runs at 512 ticks/sec; the network sends snapshots at 32 Hz (2^5). 512 packets/sec would drown in UDP overhead. The client interpolates between snapshots. Locked.
- **Server browser** — direct IP plus a master server. Locked.
- **Voice chat** — in-game, an engine feature. Locked.
- **Cross-play** — an engine feature. PC + consoles; mobile/Switch undecided. Locked.
- **Authoritative choice** — servers choose authoritative or not. Non-authoritative is faster and less secure. The deterministic sim underpins anti-cheat either way. Locked.

## 15. Save & versioning

- **Bytecode world save** — compact and byte-aligned; superior to LevelDB in compression. Dirty checks decide what actually saves. Locked.
- **SGNL v2** — the signal serialization format inside the save. Locked.
- **Block ID migration** — handled by a core engine update function. Locked.
- **Semantic versioning** — major.minor.patch. Protocol versions are negotiated at connect. IMod exposes engine, SDK, and protocol versions at runtime. Locked.

## 16. Audio, accessibility & input

- **Colorblind mode** — a core engine feature (UI + render). Locked.
- **Localization** — seven cultures. Locked.
- **Key remapping** — an engine feature, exposed to mods. Locked.
- **Debug console** — part of the core engine and the UI. Locked.

## 17. Combat & tools

- **Tool/weapon split** — tools interact with blocks; weapons deal damage to entities. An item can be both (a tool with `meleeDamage`).
- **IWeapon** — emits IProjectile or IExplosion.
- **IProjectile** — engine-side; ticks with the world and loads chunks ahead of its path. Supports return-to-owner (boomerang). Locked.
- **Auth** — X (x.com) API + Steam API for login. Future: Grok integration for LithoLattice/LithoKraft auth. Locked.

## 18. The numbers theme

- **64** — signal values, channels, light levels, Kharge range, chip-face states. The theme.
- **512** — TPS baseline, max stack size.
- **1024µs** — the script budget (2^10).
- **42** — the deliberate exception (101010 in binary).
- **8** — hotbar slots, gear slots, despawn minutes (∞ on its side). The 8-wide grid is the power-of-two theme; the two hands break it to land on 42.
- **1-based** — everything player-facing starts at 1; 0 means off.
- **Powers of two** — everywhere except 42.
