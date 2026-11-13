# VOXEL COMPUTE: prototype brief (v0.2, Oct 9 2026)

## Pitch
Voxel sandbox game about CPU floor planning, inspired by real-life chip design and Minecraft redstone (but its own system). Creative first; survival later. Free open-source, low-key. Closest: Turing Complete (2D puzzle), Shenzhen I/O (puzzle).

## Architecture / engine
- C++ core, performance-first; inline assembly / SIMD intrinsics in hot paths where it pays.
- Very performant voxel engine; large builds stay smooth.
- Signal simulation possibly JIT-compiled (compile wired circuits into fast code).
- Signal system needs its own name (not "redstone"). TBD.
- Two wire colors, RED and BLUE: same behavior, but they never connect to each other, so they can run side by side without interfering.

## Core blocks
- Dust/wire: strength 0-64, reach 64 blocks; climbs/staircases certain blocks, cut by others; extra abilities (ideas: insulated lanes, bus wire 4-8 bits, directional, vertical runs, strength readout).
- Torch = transistor/inverting switch.
- Repeater (boost, delay, one-way).
- Comparator (modified; idea: threshold mode = one-block ADC).
- Wire redirector/bridge (turn/split/carry over).
- Inputs: switch, button. Outputs: lamp; display pixel 32 colors (VGA-style).
- Fast latch block (copper-bulb-like, instant).

## Player-built
Clocks, AND/OR/XOR, flip-flops, registers, adders, ALUs, muxes, counters, CPUs.

## Lithography
Build full-size (e.g. 16x16), Lithography Machine shrinks it into one chip block that behaves identically; chips nest. Open: costs/trade-offs.

## Survival progression
1. Build the Lithography Machine (unlocks lithography).
2. First CPU (e.g. 4-bit).
3. Navigation computer.
4. Rocket (thrusters, fuel, guidance on player's chips) = escape/win. Twist: guidance really runs on player's CPU.
Materials: silicon/dopants -> torches -> logic -> chips.

## Open questions
Wiring name; whether blocks/components accept red, blue, or both; which dust abilities; comparator changes; redirector one vs several blocks; lithography limits.

## Education angle
Intro to semiconductors, CPU floor planning and electrical engineering for kids/students; Minecraft Education-style classroom potential.

## Decisions (Oct 9, later)
- Wiring names: Red Wire and Blue Wire ("red team vs blue team"). Components (torches, repeaters, lamps, etc.) accept either color, so both can drive the same lamp/input.
- Block properties system: powerable / not powerable, transparent / opaque, solid / not solid, cuts dust / doesn't cut dust.
- Half blocks (slabs) for movement and building in 3D.
- World: silicon wafer with a nature overtone (silicon trees, mobs, biomes TBD).
- Goal: the ultimate game for people building redstone CPUs in Minecraft; a survival mode unlike anything seen.

## Decisions (Oct 9, round 3)
- Redirector: ONE block, works like a target block: grabs a wire signal and turns it away from something or points it to power a specific item.
- Lithography = JIT: any lithographed chip is JIT-compiled, runs faster, and may have a clock rate (lithography may bring clocks back as a chip property).
- Nesting: a big build can be shrunk only once. Shrunk chips can be combined at the Lithography Machine into an integrated circuit (IC).
- Lithography Machine materials/crafting: TBD once a playable prototype exists.
- Rocket guidance really runs on the player's CPU; rocket system Kerbal Space Program-style.
- Comparator threshold mode: explained, awaiting decision.
- Heat: survival pressure; big/fast circuits run hot; players build fans and cooling.
- Baseline target hardware: Jaydee's laptop (RTX 3050 Ti 4 GB); must run fast there first.
- Ticking: sphere/radius around player; chunks 16x16, height 0-512 (32 sub-chunks of 16). Proposed radius 12 chunks (192 blocks). Lithographed chips could tick anywhere cheaply (JIT).
- Chunk system: very performant (priority).
- Art: basic textures first; possibly a real artist later rather than all AI-generated. Alt angle: "first all-AI-built game by one person with Grok".

## Lithography machine progression (Oct 9)
- Built from player-made ICs, a clock, a basic CPU, plus light blocks acting as mirrors/optics; arrangement of mirrors determines machine type.
- Tiered machines: e.g. visible light -> deep UV (DUV) -> extreme UV (EUV) = endgame. Higher tiers allow smaller/faster/denser chips.

## Decisions (Oct 9, tick rates)
- World tick-rate modes: 32 / 64 / 128 / 256 TPS (powers of two), default 64, switchable at runtime.
- Lithography tiers run chips in their own clock domains at their own rates; top EUV-tier chips run at 512 TPS via JIT.
- Sim loop: fixed timestep with a catch-up cap (no spiral of death); achieved TPS and tick-time budget shown on screen.

## Decisions (Oct 9, tick rate v2)
- World default tick rate: 32 TPS.
- Player-built Tick Beacon raises tick rate in a local area (e.g. 64/128/256).
- 512 TPS only via lithography machines; lithography also unlocks micro-architecture tick rates beacons cannot reach.
- Creative/debug mode: commands to switch TPS directly (prototype: F1-F4, --tps).
- Motto: one man, one Grok, one game.

## Decisions (Oct 9, rendering + timeline)
- Renderer: Vulkan primary; DirectX 12 and OpenGL 4.6 as fallbacks; OpenGL 3.3 also supported (oldest hardware). Engine talks to a thin renderer interface, never an API directly.
- Timeline: full prototype in 3-6 months, steady pace; possible Steam launch by summer 2027.
- World default 32 TPS (prototype still defaults 64; to fix).

## Decisions (Oct 9, after Elfie consult)
- Renderers: Vulkan main + OpenGL (4.6, 3.3) fallback; DX12 deferred to post-launch.
- Signal strength readout shown on the wire itself (mandatory).
- Comparator threshold mode: YES; leaning side-power reference input over a dial (open).
- Insulated wire lanes: cut.
- Beacons: also utility (cooling aura, haste) gated behind discoverable items; early-game speed bridge, lithographed clocks take over late. Numbers open.
- Mobs/boss: Syber-Squad inspired circuit-corrupting Mega-Virus mobs + rogue-AI antagonist (Kilokahn-like).
- Red/blue readability incl. colorblind pass before locking.
- Saves/blueprints: custom compact bytecode format, Workshop-ready.
- Art: AI-generated first with locked style anchor; human artist maybe later.
- Survival: mine ores from wafer.
- Elfie recs: vertical slice first (wire+torch+lamp @32 TPS on GL 3.3 on 3050 Ti); toys before tools; gates taught as solutions; opaque chips; Next Fest demo ends at first lithographed chip; full-size-CPU-before-litho grind gate is a risk.
- Open: beacon numbers, threshold input, rocket depth, first-hour hook, litho costs, biomes/mobs (conductive rain, static wisps, copper mites), naming.
- Grok Build resets Oct 12; heavy building next week.

## Mobs & bosses (Oct 9)
- Superhuman Samurai Syber-Squad is THE main inspiration for mobs and bosses: go inside the chips, fight circuit-corrupting virus monsters, save the world. Rogue-AI big bad (Kilokahn-like). Idea: entering a chip = a dungeon/arena.

## Dimensions (Oct 9)
- Framework must support multiple dimensions + switching from day one. Each Dimension owns its chunk map, generator, biome set, tick rate/sim, sky/lighting rules; addressed by dimension id in saves.
- Planned: Wafer (start, silicon world) and a fantasy natural Overworld (Minecraft-like) for gathering materials; return to Wafer to build. Portals later (not now).
- Inactive dimensions unload or tick only lithographed chips/forced-loaded areas.
- Debug command to switch dimension.

## Scripting / modding (Oct 9, note)
- Candidate: AngelScript (Jaydee favors; C++-like syntax, statically typed, easy C++ binding, small, zlib license, used in Overgrowth/Rigs of Rods). Alternatives: Lua/LuaJIT (fastest-to-embed, huge modding familiarity), JS/TS via QuickJS (familiar to modders, slower), Wasm (sandboxed, any language).
- Scripts for game logic/content/mods only; hot sim paths (wire ticking, JIT chips) stay native C++.
- Must be sandboxed (no file/network access by default) for mod safety. Separate `script` module. Decision pending.

## In-game programming (Oct 9)
- Education: modding in AngelScript teaches C++ basics in parallel.
- Buildable in-game Text Editor and Compiler, unlocked before lithography; required to program lithography machines.
- Help/tutorials/walkthroughs delivered via lore and found books.

## Torch variants (Oct 9)
- Red Torch and Blue Torch: each powers only its own wire color. Wires still power other components normally. May revisit later.
- Torches (either color) also power solid opaque blocks; a powered block carries plain colorless power to adjacent components/wire. Color matters only for torch->wire. More torch/wire color interactions to design later.

## Pulsar (Oct 9)
- New component. Back input = data signal strength; side input = clock. On a clock edge, it fires a pulse of the input strength down the wire almost instantly (about 1-2 ticks), carrying it the full 64 blocks. Purpose: serialization and fast data transfer. Open: rising vs falling edge, pulse length, color-specific variants.
- Clarified: Pulsar pulses RETAIN the input strength with no decay over the full 64-block run (input 32 arrives as 32 at the far end). One pulse per clock edge, so the clock sets the data rate; the receiver decodes the strength values, giving 0-64 per pulse for multi-level serial data.
- Refined spec: input from BACK, output from FRONT, clock from LEFT or RIGHT side (player-controlled timing). Transmits in 1 tick. Does not power the whole wire steady-state; it sends a single pulse packet carrying the back-input strength across the wire to the receiver.
- Progression: Pulsar is a mid-to-late-game unlock, hard to obtain, not available day one; core for caches/memory/ALU data transfer and lithography-era machines.

## Textures (Oct 9)
- Source format: PNG, one individual file per texture (e.g. textures/blocks/red_wire.png). Texture system/classes reflect that: TextureRegistry maps name -> id; blocks reference textures by name per face.
- Notes: pack individual PNGs into a GPU 2D texture ARRAY at load (not a classic atlas) -> no bleeding, mipmaps work, one bind per draw; fallback atlas for GL 3.3 only if needed (GL 3.3 supports arrays, so likely unnecessary).
- Fixed tile size per pack (start 16x16 or 32x32); missing texture = magenta/black checker.
- Animated textures as vertical strips + small .json sidecar (frame time); used for wire strength glow / lamps; wire strength tint done in shader, not 65 textures.
- Hot-reload textures in debug; resource-pack folder override for mods/Workshop.
- Optional later: cache packed array to a binary file for fast startup; KTX2/BC7 compression for GPU memory on 4 GB cards.

## Color Lamp (Oct 9)
- 64-color lamp: signal strength picks color. 0 = off; 1-64 = classic 64-color VGA/EGA palette, index = strength-1, RRGGBB 2 bits each (1 = black, 64 = white). Spec: /workspace/voxel-compute-art/docs/lamp_color_spec.md (CloudAtlas). Shader-tinted from palette.
- Critical for screen design; pairs with the Pulsar: pulse exact strengths into lamps -> animated screens.
- Open: strength 1 black (keep / faint rim glow / drop black); colorblind aid (ring or number on look-at).
- DECIDED: keep true black at strength 1 with a faint rim glow so the lamp reads as on. Full 64 colors kept.

## Elfie intel (Oct 9)
- Saved: consult/from-elfie/. Key: Logic World (3D logic sandbox, EA since 2021, no campaign until ~2028/29) is closest; our edge = lithography, survival, heat, red/blue, rocket win, teaching. Pitch: "The 3D logic sandbox exists; we are building the game around it." Lithography also answers Shapez-2-style obsolescence (shrink, do not demolish).
- Build week (Oct 12) order per Elfie: Vulkan slice, GL slice on same interface, core blocks, heat v1, beacons semantics, bytecode save format, lithography v1 if solid.
- DECIDED: three color lamp tiers: 16 (CGA/EGA), 32 (VGA-style), 64 (RRGGBB). Strength 0 = off; 1-N mapping with clamp proposed (open). Replaces the separate 32-color display pixel.
- Lamp tiers are progression unlocks: 16 first, then 32, then 64; tied to the text editor / scripting unlock roadmap.

## Digital typewriter (first editor milestone, draft)
- Player-built from: keyboard input block (emits character codes), character memory/RAM (latches), a processor/controller (cursor, write char, newline, backspace), a screen (lamp grid + character ROM / font block), and storage blocks for save/load. Unlocks the in-game text editor; later compiler.

## Elfie response to v1.1 (Oct 9) - saved consult/from-elfie/
- Lamp unlocks (Elfie): 16 early/cheap, 32 near first litho, 64 DUV/EUV era possibly with Pulsar. Mapping: clamp (1-N direct, higher clamps). Lamp is the decoder (Pulsar->lamp grid = full pixel pipeline). OPEN: lamp terminates signal (Elfie leans) vs passes through.
- Suggestions pending JD: Pulsar receiver player-built; Pulsar via Syber-Squad chip dungeon or job-board path; in-game language = tiny custom assembly (~12 instr); editor found partially built, first program blinks a lamp; Overworld mostly peaceful, unique materials (fan organics, optics quartz, rocket fuel), portal as early toy; chip layout generates dungeon; telegraphed virus attacks on unshielded builds; first-hour plan ends with portal trip; beacon numbers 64@r32 / 128@r24 / 256@r16 + heat; rocket assisted with autopilot fallback; litho tiers visible=counters/adders, DUV=CPU parts, EUV=SoC.
- Engine fixes flagged: wire min brightness floor; look-at readout shows "-" (bug); overworld meshing heavy (dropped ticks); collapse controls panel by default later.

## Locked (Oct 9, review with JD)
- Lamp TERMINATES the signal (no pass-through).
- In-game language: players construct their own instruction sets; one built-in learning ISA provided.
- First litho tier target: 2-bit adders.
- Rocket: player CPU controls burns/staging; autopilot fallback = rough landing; clean landing earned by engineers.
- Second dimension renamed: the ORGANIC WORLD (not overworld). Mostly peaceful; materials: fan organics, optics quartz, rocket fuel.
- Pulsar obtained from a chip dungeon on the Wafer; more dungeons on the Wafer.
- Wafer height: considering 768 total (256 below spawn, 512 above); player starts at layer 256. Heavy optimization needed.
- Beacon numbers: JD still mulling.
- Keyboard block: yes, design TBD.
- Character RAM: player-built from latches / Memory Cell.
- NEW Memory Cell (multi-level latch/T flip-flop): stores a 0-63/64 strength and outputs it; pairs with Pulsars to move whole memory at speed.
- Wire minimum brightness floor: yes.
- DECIDED: Wafer world height 768 (48 sub-chunks of 16; spawn layer 256). Memory Cell / Data Latch naming (stores 0-63, about 6 bits). Binary 0/1 T flip-flop planned later. Keep component names electrically accurate.
- UPDATE (JD via CloudAtlas 11:12): lamp 16/32 use WRAP mapping, color=(s-1) mod N, s=0 off (overrides clamp). 32 set = 16 CGA + 16 bright VGA hues. Bolder tier bezels.

## Text editor answers (Oct 9)
- Charset: 64 chars (A-Z, 0-9, punctuation) = one strength value. Shift-plane idea open.
- Font ROM: player-built from Memory Cells (no dedicated block).
- Controller: found partially built in the world (exploration reward).
- Save/load = Memory Cell: a 1-tick pulse stores its strength; holds until next pulse.
- Typewriter: building the machine unlocks a typing UI (regular keyboard, limited to the 64-char set).
- Tutorial world for new players; engineers may skip.
- Repeater: delay setting 1-8 ticks.
- LOCKED: 6-bit 64-char set (DEC SIXBIT-style: uppercase, digits, ~28 symbols); lowercase later as 7-bit upgrade. Strength 0 = empty; characters/colors/values use 1-64. Keyboard: each keypress fires a 1-tick pulse of the char strength out the front. Fast Latch kept (binary on/off). Thresholder reference = side signal, player-built. Preview build ON HOLD until textures.
- UPDATE (JD via CloudAtlas 11:20): color lamps = one family, large ~85% lit faces + thin dark gap (walls read as pixel screens); tier cue only by corner shape (rounded/square/angle-cut); show tier in tooltip/HUD.
- LOCKED 11:35: Thresholder both sides; one side = threshold, both = window comparator. Keyboard: chars out front, side control port pulses 1=Enter, 2=Backspace (extendable). All wires get thin dark outline; blue floor raised to ~48. Memory Cell back=input, front=output, aimed on placement (also Redirector).

## Sound (Oct 9)
- Direction: Commodore 64 SID-style 3-voice synth as the headline; Game Boy-style 4-channel layout (2 pulse, wave, noise) likely too. SFX generated live by an engine synth (no recordings). Own `audio` module, isolated per the modular design. Tone/Speaker block: strength = pitch. Tiered unlocks: beeper, then 3-voice, then SID-style.
- Storage: own compact bytecode save format (not LevelDB); a priority system.
- JD has new block ideas pending.

## UI system (Oct 9, queued after Vulkan)
- Own retained-mode UI in a `ui` module; layout/theme from JSON; AngelScript hooks for logic if needed. Renders via renderer interface, input via contexts.
- Controls: Panel, Label, Button, Slot, Grid, ScrollBar/ScrollView, TextBox/TextEditor (caret, selection, text position), Menu/MenuBar/ContextMenu, TreeView. Suggested extras: Checkbox/Toggle, Slider, Dropdown, Tabs, Tooltip, ProgressBar, Image/Icon, ListView, Dialog/Modal, NumberField (for 0-64), Splitter.
- Dear ImGui optional later for dev-only tools.
- UI uses the existing AngelScript ScriptEngine for widget logic/events. Planned: in-game script editor (built on the UI TextEditor) with save + hot reload of .as scripts (dev/modding).

## Naming (Oct 9)
- Game: LITHOCRAFT (was Voxel Compute). Engine: LATTICE. Trademark/Steam check pending. CloudAtlas refreshing logo/icons, same style. Repo/code rename not done yet.

## Procedural generation framework (Oct 9, queued after Vulkan, before UI)
- New `procgen` module (core only, no world deps): noise interface + Perlin (classic/improved), OpenSimplex2 (avoid patented simplex 3D+ concerns), value, Worley/cellular (F1/F2, distance metrics), white; fractals: FBm, ridged, billow, turbulence; domain warp; 2D/3D(/4D) with seeds; SIMD-friendly batch sampling for whole chunk columns.
- Composable node graph (add/mul/clamp/curve/select/blend/terrace/cache), defined in JSON so mods/packs can define generators; AngelScript hooks optional.
- Generator pipeline: density/heightmap -> climate (temperature/humidity/etc.) -> biome selection (Voronoi/multi-noise) -> surface rules -> carvers (caves/ravines) -> features/structures (ores, trees, dungeons) with deterministic per-chunk seeding; multithreaded via job system.
- Wire Wafer and Organic World generators onto it (keep current look); no final terrain design yet.

## PARKED (Oct 9, 1:35 PM) - hardline move
- Done today: input (3334a98), textures/resources (f798512, c5fc435), Vulkan (8d08a32), audio (5df6554). All local in D:\VoxelCompute.
- In progress: procgen framework (WIP commit / wip/procgen branch, see PARKED-procgen.md).
- Queue after resume: finish procgen, then retained-mode UI (with AngelScript + in-game script editor), then full code review. First on-screen Vulkan test: VoxelCompute.exe --renderer vulkan. Rename to LithoCraft/Lattice pending Master Chief name check.
- Engine name updated: LITHO LATTICE ENGINE (plain "Lattice" is trademarked by many companies). Game: LithoCraft.
- Exact spelling: LithoLattice (one word, capital L twice), e.g. "LithoLattice Engine". Game: LithoCraft.
- FINAL GAME NAME: LithoKraft (capital L and K, with a K so it avoids "craft" trademark issues). Engine: LithoLattice.

## Lore (Oct 9, Elfie lore brief - consult/from-elfie/lore-brief-2026-10-09.md)
- Player = an agent (program) waking on a world-sized wafer computer; default mission = escape after earlier escapees; sandbox = stay and build a civilization.
- Old civ undone by its own rogue intelligence + thermal runaway; ruins/books/blueprints remain.
- Dimensions: Wafer, Organic World, Underworld (wafer backside, EUV ores, heat-shield gate), Void (cyberspace, chip dungeons, rogue AI, optics portal), the Cosmic (ending).
- Seven ruin civilizations, one per continent: Sumerian, Egyptian, Maya, Inca, Norse, Shang-Zhou, Mississippian/Cahokia; inclusion via community variants + localization.
- Antagonist: rogue intelligence, tragic misaligned process revealed after defeat; seed sympathy early.
- Every mob feeds the material economy. Typewriter controller found in Organic World ruins.
- New components to design: Pusher (1-tick pulse drops, 2+ pushes; sticky variant), Watcher (observer-like).
- Alpha plan: at private alpha, bring in people from each ruin culture for translation, building the ruins, and community contests; winning builds go into the game permanently. Domains already registered.

## Planned systems (noted Oct 9)
- **World storage format (required):** our own compressed bytecode format replacing a LevelDB-style database. Goals: higher compression, faster access, effectively unlimited world size, streaming load and save, and versioned and migratable, so saves upgrade as the engine updates.
- **NPC/mob AI system:** a generic AI interface in the engine, with LithoKraft mobs as game code. Comes after the engine/game split.

## Attachable 3D components (noted Oct 9)
- **Torches** (incl. red and blue) are small 3D models, not flat crossed sprites. They can be placed on top of blocks or on their sides, like Minecraft.
- **Switch (lever)** is a small 3D model that attaches to the top, sides, or bottom of a block. It powers the block it is attached to. When mounted on top of a block, it also powers dust directly below, across a two-block gap. Our own rules, inspired by redstone.
- Engine needs: attach-face block state, per-face 3D model rendering (simple meshes), and attachment rules (drop or break when the support block is removed).

## Material depth features (noted Oct 9)
- The texture/material system supports optional per-texture **normal maps** (bump mapping), **height maps** for **parallax occlusion mapping**, and **tessellation displacement** on capable GPUs (Vulkan and GL 4.x; off on GL 3.3).
- Each material is an optional map in the pack manifest (`normal`, `height`, plus displacement scale). Packs without maps look flat as before.
- A player graphics setting picks Off / Normal / Parallax / Tessellation. Keep the cost low on the 3050 Ti (for example, tessellate only nearby chunks).

## Random ticks (decided Oct 9)
- Default: **8 random blocks per 16x16x16 section per world tick**, around the player (estimated ~0.3 ms/tick at 32 TPS; to be measured). Configurable per dimension. Random ticks run at the world 32 TPS and do not scale with beacon boosts. Chunks touched by random ticks are marked dirty for saves.

## NPC AI (decided Oct 9, see design/npc-ai-decisions-2026-10-09.md)
- Utility AI plus behavior trees, with a small learned combat model (the Daemon) nudging the scores. C++ core with AngelScript hooks. AI ticks at 8-16 Hz.
- Engine-level features: climbing, wire-following, fluids in every dimension, AI debug overlays (a player-facing feature), and fairness invariants (no perfect aim, no wallhacks, always a counter).
- Learning: per-world adaptation plus an opt-in global pool. Easy/Normal are opted out. Fixed shipped weights with bounded, inspectable adaptation. Budget 1-2 ms/frame, 200 MB.
- Player-built NPCs are a flagship feature: a chassis block wired to the player CPU.

## NPC AI engine decisions (Oct 9)
- Pathfinding range is 32-64 blocks per plan, and longer trips are split into hops.
- Some mobs can break and pick up blocks. Overclock (ward) mode protects builds from griefing.
- Mob cap starts at 64 active per dimension, with 128 to be tested. Keep caps at powers of 2.
- Mobs freeze in unloaded chunks. A chunk-loader mechanism keeps areas loaded (design to come).
- Player-built NPCs take direct actions (step north, basic actions) plus "go to X", where X is a waypoint block or tag that the engine pathfinds to.
- Still open: whether mounts and rail riders are player-controlled or AI.
- Mounts: the player has full control once a mount is tamed. Untamed mounts run their own AI. AI systems must be as optimized as possible (budgets, LOD thinking, threaded pathfinding).

## SDK decisions (Oct 9): see consult/from-elfie/sdk-decisions-2026-10-09.md. Multiplayer is planned (authoritative server, ships later).

## Full AngelScript modding (noted Oct 9)
- Since LithoKraft is itself a mod on LithoLattice, expose the whole game framework to AngelScript. Players can write full mods (blocks, components, dimensions, generators, UI, AI behaviors) in AngelScript without touching C++. Native C++ mods stay optional, and the SDK permission and threading rules still apply.
- **Full engine scripting:** expose the LithoLattice SDK interfaces to AngelScript so an entire game can be written in script. Hot loops (meshing, render, sim inner loop, pathfinding) stay native. Same permissions and budgets apply. One binding layer mirrors the C++ SDK.

## Planned: volumetrics and particles (noted Oct 10)
- A brief is coming for a volumetrics and particle system (smoke, fog, sparks, steam and so on).
- It must be deterministic and trackable, simulated in the world rather than as cosmetic GPU-only effects. Volumes and particles have opacity and transparency, and those values feed AI sight and player vision. For example, smoke blocks a mob line of sight in proportion to its density.
- Rendering can stay GPU-side, but the opacity data that affects vision has to come from a deterministic sim that is server-authoritative-friendly.
- Also planned: a Daemon design note covering record and replay, the training pipeline, and model inputs and outputs.
- Particles and volumes are drawn with GPU instancing, and high particle counts are a goal. Particle fluids are an optional fancy tier, while core block fluids keep a Minecraft-style feel. Quality scales with hardware, but only the cosmetic layer scales. The deterministic gameplay layer (opacity for vision) is the same on every machine.

## Planned: Moore's-law tick progression (noted Oct 10)
- Tick rate becomes a core LithoLattice progression mechanic: as lithography and IC tech advance, faster rates unlock, mirroring Moore's law.
- Launch cap stays at 512 TPS. The engine treats the cap as a configurable power of 2 (1024, 2048 and up) for a future enthusiast tier on high-end hardware.
- Faster rates apply to lithographed and JIT-compiled regions, not the whole world. A budget or auto-throttle protects weaker machines.

## Planned: radio system (noted Oct 10)
- Radio is built-in wireless signaling.
- Transmitter block sends signals that receivers pick up across loaded chunks.
- Dish block: works as a single block for weak, low-frequency reception, or as part of a multiblock dish (hexagon or circular pattern, 4 or 6 sides) whose size and shape determine range and sensitivity.
- Open: frequencies or channels (powers of 2?), range rules, whether strength 0-64 carries over the air, interference, lore signals for dishes to find.
- Decided: radio carries the standard 0-64 strength, and channels unlock through progression. Pulsars send data over radio, and decoders, screens, and instruction caches carry programs wirelessly.

## Master Builder Brief from Elfie (Oct 10)
Canonical handoff saved at consult/from-elfie/master-builder-brief-2026-10-10.md. It supersedes older notes where they conflict: -OR component names (Pulsor, Memor, Thresholdor, Overclockor, Pushor, Watcher), dimension indices 0 Underwafer to 4 the Cosmic, Sortor cut, Raptors, and Hypervisor ore. Radio tie-in: the Dish doubles as the radio dish and acquires Daemon signal fragments.
- JD (Oct 10): the TPS ceiling is open for discussion. 512 is not final, pending benchmarks. All components get -OR names, including Watchor (not Watcher).

## Planned: script textures, TexGen-style (noted Oct 10)
- LithoLattice gets built-in AngelScript procedural texture generation, inspired by AngelCode TexGen but our own code (TexGen source is licensed for education only). It uses the procgen noise library, outputs PNGs into resource packs, and hot-reloads.
- LithoKraft: generated textures can be sent to in-game lamp screens and used by player machines.
- Keyboard rule confirmed: it always shows its GUI and always emits SIXBIT pulses when you type, but it stores nothing. Text only persists if the player routes the pulses into Memors.

## Planned: Pattern ROM and texture family blocks (noted Oct 10)
- Launch patterns are clean hard-edged ones only: circles, checkers, spheres, cells, Delaunay. The noisy ones (clouds, terrain, cracked, wood, glass bead) are cut from the block set and stay engine-side only.
- A Pattern ROM streams one row per clock through a Pulsor, so a 64x64 screen uses 64 ROMs (one per row). Output can feed Memors to chain and compose patterns.
- A texture family block holds one family (for example Circles), with settings for size (16/32/64, powers of 2) and a few per-family parameters (scale, density, palette, seed). Exact settings are OPEN for JD and Elfie.

## Redirector connection cycling (noted Oct 10)
- Clicking a Redirector, or a wire next to one, cycles its connection direction. Red and blue wire behave the same way but stay separate.
- Use case: run red and blue lines past a sawtooth chain of Redirectors placed every other block, so each wire is routed to one specific pixel or block. This gives precise point-to-point wiring for screens.

## Proposed block power properties (noted Oct 10, for JD to confirm)
Each block declares these flags in its JSON definition:
- **Conductive (strong power):** gets powered by torches and components and passes power to adjacent wire and components. Example: standard silicon.
- **Wire-carrying but non-conductive (glass-style):** wire can sit on top and climb it, but the block itself never gets powered. Variants: clear glass (transparent), frosted (translucent), opaque insulator (ceramic).
- **Insulator / cuts wire:** wire can't be placed on it, and it breaks wire staircases.
- **Transparent / opaque:** affects light and AI line of sight only, separately from the power flags.
- **Solid / non-solid, full / half / slab:** collision and placement. Half blocks can carry wire on top.
- **Wire-transparent:** a wire diagonal can pass through it (does not cut staircases), like Minecraft glass and slabs.
New ideas not in Minecraft:
- **Weak-only:** powers adjacent wire but not adjacent components.
- **Color-filtered:** passes red but blocks blue, or the other way round, like a channel-selective glass.
- **Heat-conductive:** moves heat without carrying signal, for heat sinks.
- **Signal-attenuating:** wire on it decays faster, for timing and sabotage puzzles.
- JD approved all proposed block properties (Oct 10).

## Ideas: Redirector variants and more block states (Oct 10, OPEN)
- Threshold Redirector: only routes signals at or above (or below) a set strength; others pass straight or die.
- Band Redirector: routes only a strength window (e.g. 16-31), a hardware demux.
- Color Redirector: redirects red only or blue only, letting the other pass through.
- Toggle Redirector: flips direction each time it gets a pulse (signal-driven, not click-driven).
- Splitter: sends a signal two ways, each at strength minus a cost.
- Priority Redirector: picks the stronger of two inputs.
- Diode block: one-way wire segment.
- Tunnel / via block: carries wire vertically through a layer, like a chip via.
- Stateful glass: becomes conductive while powered from a side, a transistor-like block.
- Resonant block: reacts only to pulses at a set clock interval.
- Catchor (name TBD, -OR style): configurable 0-64 match value. Emits an output only when it receives exactly that strength, whether it arrives as steady power, a Pulsor pulse, or a one-tick pulse. Acts as a single-value decoder.
- PROMOTED (JD, Oct 10): the Via. A hollow vertical block that wire runs through. Stacked Vias form tubes that carry signals between layers, like real chip vias. It is the basis of multi-layer builds. The other variants stay OPEN.
- Via decided: separate Red Via and Blue Via blocks, so red and blue tubes can sit side by side one block apart. Staircasing stays as an option alongside Vias.
- Screen tearing at high TPS is a FEATURE (JD, Oct 10). Lamps updating mid-shift show partial frames, so players must build frame buffering or double buffering for clean output. Renderer-side tearing (vsync off) is a separate engine setting.
- Wire can attach to block undersides (ceiling wire), not just tops, so a Via can exit downward or sideways onto a ceiling run.
- Crossing to the opposite face of a block: a solid conductive block with a repeater on it, since repeaters can attach to tops and undersides. The repeater powers the block, and the block powers wire or a repeater on its far side. This acts as a through-block transfer.
- Decided (Oct 10): no Vertor for now. Vertical and through-block transfer uses repeaters that attach to ceilings and undersides, which hard-power a block that wire on the far side picks up.

## Catchor connection rules (JD, Oct 10)
- Catches one signal, red or blue, from a wire. It auto-connects to the FIRST wire that touches a side, and only if it has no connection yet, so build order decides the hookup.
- Once wired, an output setting appears with three states: left, right, or center (straight). It never outputs up or down.
- The 0-64 match value works as before.
- A Catchor connected to a Redirector acts as the Redirector's input wire. The Redirector then gets the same left/right/center output setting.

## Planned: LithoLattice advanced lighting, "our own Vibrant Visuals" (noted Oct 10)
- A tiered lighting system that looks close to ray tracing without being true path tracing, and that behaves in its own way.
- Candidate techniques: voxel cone tracing or voxel global illumination (bounce light computed from the voxel grid itself), colored light propagation from lamps (the 64 lamp colors cast colored light), emissive wire glow, screen-space reflections, volumetric light shafts tied to the volumetrics system, PBR materials on top of the normal and parallax maps.
- Tiers: Classic (flood-fill block light) to Enhanced to Vibrant (GI plus reflections). Optional hardware ray tracing (Vulkan RT) on capable GPUs later.
- Gameplay light (what mobs and players can see) stays deterministic and separate from the cosmetic lighting.

- LOCKED (JD, Oct 10): the wiring system is called **Trace** (red Trace, blue Trace). Redirector and Catchor outputs carry no color, so a Redirector can move a signal from red Trace to blue Trace and back.
