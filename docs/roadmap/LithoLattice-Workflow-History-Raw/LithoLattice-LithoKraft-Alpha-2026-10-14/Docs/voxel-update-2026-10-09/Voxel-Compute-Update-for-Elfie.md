# Voxel Compute: Progress Update for Elfie
Version 1.1, October 9, 2026. Follows the v1.0 Consulting Brief (included in this package for context) and your consolidated brief from this morning.

**Please keep everything in this package private.** It's unreleased design and code direction.

---

## 0. TL;DR
- **The vertical slice you recommended is built and running.** It has Red and Blue Wire (0–64, with a visible strength readout), Red and Blue Torches, lamps, and demo circuits at spawn. Everything ticks at the new **32 TPS default**, in OpenGL 3.3 (our weakest backend).
- **The engine is now modular, like a real game engine.** There are 8 modules with one-way dependencies, so any system can be worked on alone.
- **Dimensions are in the framework from day one.** There's the Wafer plus a placeholder Overworld, and a debug key switches between them. No portals yet.
- **AngelScript is confirmed as our scripting and modding language.** It's sandboxed, and it doubles as an education feature, since modders pick up C++-style syntax.
- **There's a new signature component, the Pulsar.** It's a clocked, no-decay, multi-level data transmitter and the backbone for buses, caches, and memory-to-ALU links. It's a mid-to-late-game unlock.
- **There's a new progression beat:** players build an in-game **text editor and compiler** before lithography, and they need them to program lithography machines.
- **Syber-Squad is now the main inspiration** for mobs and bosses: you go inside the chips and fight viruses.
- **The texture pipeline is defined:** individual PNGs packed into a GPU texture array. **CloudAtlas**, an AI art teammate, is now producing the AI-generated art.
- Your questions are in **section 13**.

---

## 1. What changed since v1.0 (at a glance)

| Area | v1.0 (this morning) | Now (v1.1) |
|---|---|---|
| Prototype | Engine shell: window, silicon world, fly camera, chunk meshing, tick rates. No logic. | **Vertical slice:** wires, torches, lamps, live circuits, block placing and breaking, HUD, logging, Minecraft-style controls |
| Default tick rate | 64 in code (32 decided) | **32 TPS default** in code; debug key cycles 32/64/128/256 |
| Torches | One torch type | **Red Torch and Blue Torch**, each powering only its own wire color. Any torch also powers the solid block above it, which gives off **colorless** power. |
| Code structure | Single prototype | **8 engine modules** with enforced one-way dependencies |
| Dimensions | Not planned | **Framework built:** Wafer and placeholder Overworld; portals later |
| Scripting | Undecided | **AngelScript 2.38**, sandboxed, integrated and running |
| New components | (none) | **Pulsar** (designed, not yet implemented) |
| Progression | ICs → clock → CPU → Lithography Machine | Adds an **in-game text editor and compiler** step before lithography |
| Renderers | Vulkan main, DX12 + GL fallbacks | **Vulkan main, OpenGL 4.6/3.3 fallback, DX12 post-launch** (per your consult). GL 3.3 runs today. |
| Textures / art | "Basic textures first" | **Texture system spec** defined; **CloudAtlas** handling AI-generated art |

---

## 2. The vertical slice: what's built and running

Screenshot: `images/screenshot-vertical-slice.png`, taken in the Wafer dimension with the HUD on.

![Vertical slice on the Wafer](images/screenshot-vertical-slice.png)

### 2.1 Signal blocks (implemented)
- **Red Wire and Blue Wire**
  - Signal strength runs **0–64**, drops by 1 per block, and reaches 64 blocks.
  - **The colors never connect.** In the demo, a red line and a dark blue line run right beside each other with no crosstalk.
  - Wire climbs and descends one-block stairs, and a block placed above a lower wire cuts the stair (deliberate routing control).
  - **Strength readout (mandatory, per your consult):** wire brightness scales with strength. The HUD also shows the exact strength of the wire you're looking at.
- **Red Torch and Blue Torch** (the transistor/inverter)
  - Each torch is lit unless the block it stands on is powered, which makes it an inverter with a 1-tick delay.
  - A **Red Torch powers only adjacent red wire**, and a **Blue Torch powers only adjacent blue wire.** That opens up color-steered logic: one inverter stage can drive one color "lane" and leave the other alone.
  - **Any torch also powers the solid, opaque block directly above it.** That block then gives off **plain, colorless power**: adjacent wire of *either* color starts at 64, adjacent lamps light, and a torch standing on top turns off (torch towers). **Color only matters on the torch→wire hop.**
  - Wires still power lamps and other components regardless of color.
- **Lamp:** lights from any powered neighbor wire (either color), from a torch-powered block, or from a torch directly below it.

### 2.2 Demo circuits at spawn
Four rows on a platform in front of the player:
1. **Torch powers the block above:** a Red Torch sits under a silicon block, the block powers **blue** wire beside it (colorless power crossing colors), and the wire lights a lamp.
2. **Stairs:** a Blue Torch drives blue wire up a one-block stair to a lamp.
3. **Inverter you can play with:** a red wire has a **gap** next to a block with a torch on top. Place red wire in the gap and the block powers, the torch turns off, and the lamp goes dark. Break the wire and the lamp comes back on. This is the first "toy" in the sense of your "toys before tools" recommendation.
4. **Color isolation:** Red Torch → 10 red wire → lit lamp, with an unpowered blue wire right beside it that stays dark.

### 2.3 Tick system
- **World default is 32 TPS.** A debug key cycles 32 → 64 → 128 → 256 TPS, standing in for Tick Beacons until they exist.
- The sim uses a fixed timestep with a catch-up cap, so it never spirals into lag. In testing, a 1-second freeze dropped the backlog and the next second was back at target.
- There's a **placeholder 512 Hz "EUV" chip clock domain** (toggleable) to prove that lithographed chips can run at their own clock rate alongside the world tick. The JIT itself isn't built yet.

### 2.4 Dimensions (framework)
- **Every dimension owns its own** chunk map, terrain generator, biome set, structures, and simulation. Saves will address everything by dimension ID.
- **There are two dimensions today:**
  - **Wafer:** the silicon starting world with the demo circuits.
  - **Overworld:** a *placeholder* natural, Minecraft-like world with grass, dirt, and stone. It has two biomes (plains and hills) chosen by low-frequency noise.
- **The debug key (F6) switches dimensions in about 40 ms** on the test machine. Portals will use the same switch later.
- **The design intent:** you start on the Wafer and build a portal to the Overworld, which is the natural world for **gathering materials**. Then you return to the Wafer to build. Dimensions you're not in either unload or keep ticking only lithographed chips and force-loaded areas.

Screenshot: `images/screenshot-overworld-placeholder.png`. The terrain is placeholder only and hasn't been art-directed.

![Placeholder Overworld](images/screenshot-overworld-placeholder.png)

### 2.5 HUD, controls, and logging
- **On-screen HUD:**
  - A 9-slot hotbar.
  - A debug stats panel showing FPS, frame time, achieved/target TPS, tick time, sim budget %, dropped ticks, dimension, and the looked-at block.
  - A controls panel listing every key.
- **Minecraft-style controls,** so players (and Jaydee) feel at home:
  - Building: 1–9 or the scroll wheel picks a slot, middle click picks the block you're looking at, E opens a creative inventory (shift-click sends a block to the hotbar), and Q clears a slot.
  - Movement: Ctrl sprints.
  - HUD: F1 hides the HUD, F3 toggles debug stats, and H toggles the controls panel.
  - All bindings go through an action map, so we can make our own scheme later.
- **Live log file:** startup GPU info, one performance line per second, hitches, tick-rate changes, and dimension switches. The dev team can read it live while Jaydee plays, which speeds up debugging and performance tuning.

### 2.6 Scripting: AngelScript (confirmed)
- **AngelScript 2.38** is integrated as its own engine module. It comes only from the official release, with a pinned version and checksum.
- **It's sandboxed.** Scripts can only call functions the engine explicitly gives them, with no file, network, or system access. Each script call also has a time budget, so a runaway loop gets aborted.
- **It's for game logic, content, and mods.** The hot paths (wire ticking and lithographed chips) stay native C++, so performance never depends on scripts.
- **A small hello script runs at startup** to prove the pipeline works. Self-tests confirm that the sandbox blocks unregistered calls and kills infinite loops.
- **Why AngelScript:**
  - It looks and types like C++ (static types, classes, handles), which makes it an **education feature**: modders learn C++ habits along the way.
  - It binds cleanly to our C++ engine, it's small and fast, and it has a permissive license.
  - Its known weakness is that fewer modders know it, so documentation and examples will matter.

### 2.7 Quality checks
- **There are 30+ automated self-test checks,** and all of them pass. They cover:
  - signal decay, the inverter, and color isolation
  - stairs and the demo lamps
  - torch→block power
  - dimension load and unload
  - meshing
  - the AngelScript hello script and sandbox
  - log rotation and control bindings
- **Software-rendered test run** (a CPU-only test machine with no GPU, 1280×720):
  - About 80–100 FPS on the Wafer and about 55 FPS on the Overworld.
  - **32.0 TPS held** at about 0.03 ms per tick, which is about 0.1% of the tick budget.
  - With no GPU at all, that's a good sign for the real laptop.
- **Laptop status:** Jaydee is now running the builds on his laptop, and per-second performance logs from it are being collected. Those will give us the real baseline numbers.

---

## 3. Engine architecture (modular, like a real engine)
The code base will get big, so it's split into **8 modules**. Each one is its own library with a small public interface and private internals. **Dependencies only flow one way, and the build enforces it**, so one system can be worked on without touching the rest.

![Module dependencies](images/module-dependencies.png)

| Module | Owns |
|---|---|
| **core** | logging (including the live log), time, math, threading (job pool, lock-free hand-offs) |
| **platform** | window and input (the only module that touches the windowing library) |
| **world** | block registry and properties, chunks, dimensions, terrain generators and biomes, raycasting |
| **sim** | tick scheduler, signal logic (wire/torch/lamp), chip clock domains |
| **script** | the sandboxed AngelScript host |
| **render** | the renderer interface, the voxel mesher, the HUD layer, and the GL 3.3 backend (future Vulkan/GL 4.6 backends plug in here) |
| **game** | wires everything together: player, placement, hotbar, inventory, HUD panels, input map |
| **app** | the entry point, options, and self-tests |

Key rules:
- **The renderer never sees circuit logic,** and the simulation never touches graphics.
- **Blocks come from a data registry.** Adding a block is one data row plus, if it has new behavior, one new sim role, so nothing else has to change.
- **Adding a dimension** means writing one generator function and registering it.
- **Adding a renderer backend** means implementing one interface. The mesh data format stays the same.
- **Threads:** rendering, simulation, and a worker pool for world generation and meshing all run in parallel, with no global lock on the frame path.

---

## 4. New component: the Pulsar (designed, not yet implemented)
Jaydee calls this a must-have. It's the data bus of the whole game.

### 4.1 Spec (current)
| Property | Value |
|---|---|
| **Input (data)** | **Back** face: the signal strength to transmit (0–64) |
| **Clock** | **Left or right** side. The player controls the timing, and each clock pulse triggers one transmission. |
| **Output** | **Front** face, into a wire |
| **Transmit time** | **1 tick** |
| **Strength** | **Retained with no decay** across the full **64-block** wire run. An input of 32 arrives as 32 at the far end. |
| **Behavior** | Sends a **single pulse "packet"** carrying the back-input strength. It does **not** power the wire steadily the way a torch does. |
| **Data rate** | Set by the player's clock: one pulse per clock trigger |
| **Unlock** | **Mid-to-late game, hard to obtain.** It's never a day-one item, and it arrives as a reward right when slow wire starts to hurt big builds. |

### 4.2 Why it matters
- Each pulse carries a **whole value, 0–64**, not just on or off. That's about **6 bits per pulse**, which turns a single wire into a **multi-level serial link**.
- **Its role** is moving data from memory and caches to ALUs and CPUs, and serving as the backbone of lithography-era machines. It's like a real chip's serializer (SerDes).
- **It pairs naturally with comparator threshold mode** on the receiving end. Level detectors decode the pulse value back into bits, which is the analog design space you flagged as new territory beyond redstone's 0–15.

### 4.3 Still open on the Pulsar
- Does it fire on the clock's **rising or falling edge**, and is that configurable?
- **Does it come in red and blue variants** like the torches, or is it colorless?
- **What happens when pulses collide,** such as two Pulsars on one wire or a pulse meeting a steady signal?
- **What does the receiver/decoder look like?** Is it a player-built comparator ladder, or a dedicated receiver block?
- **Does it generate heat** in the heat model?

---

## 5. New progression beat: in-game text editor and compiler
- Before reaching the lithography stage, players **build a text editor and a compiler in-game** from their own circuits and machines.
- These are **required to program lithography machines,** which ties software into the hardware progression.
- **Help, tutorials, and walkthroughs** are delivered through **lore and books found in the world**, in keeping with "teach through wants, never homework."
- **The resulting arc:** wires → logic gates → a CPU → **writing code for it** → printing chips → a rocket guided by your own CPU.
- **Still open:** what language players write in-game (see the questions in section 13).

Updated draft survival path:
1. Survive on the Wafer and mine ores and silicon.
2. Visit the **Overworld** for natural materials (via a portal, design TBD).
3. Unlock basic circuits → logic gates → fans and heat sinks.
4. Build ICs → a clock → a basic CPU.
5. Build the **text editor and compiler.**
6. Build the **first Lithography Machine** (visible light), then DUV, then EUV.
7. Unlock the **Pulsar**, then go on to caches, memory buses, and big machines.
8. Build a navigation computer → a **rocket** whose guidance runs on your CPU.

The grind-gate risk you flagged (a full CPU before the first Lithography Machine) is still on the table. One proposal is to let a **simpler first machine** (for example a counter or a 2-bit adder) qualify for the first litho tier. We'd love your take, especially now that the editor/compiler step adds another gate before lithography.

---

## 6. Mobs and bosses: Superhuman Samurai Syber-Squad
- **This is now the main inspiration** for mobs and bosses, and maybe several bosses beyond the big bad. Jaydee grew up on the show, and its core fantasy fits this game perfectly: **go inside the chips, fight the monsters, save the world.**
- **The pieces so far:**
  - **Circuit-corrupting "Mega-Virus" mobs.**
  - **A rogue-AI antagonist** in the Kilokahn mold.
  - **The idea that entering a chip is a dungeon or arena.**
- **This builds on your earlier hazard ideas** (conductive rain, static wisps, copper mites), which are still candidates for the Wafer.

---

## 7. Renderer plan (current)
- **Vulkan is the main renderer,** chosen for performance, multithreaded command recording, and Windows/Linux support.
- **OpenGL 4.6 and OpenGL 3.3 are the fallbacks.** GL 3.3 is running **today** and is our weakest-backend baseline, as you recommended.
- **DirectX 12 is deferred until after launch.** Fewer backends means less upkeep for a solo developer.
- The engine only talks to its own renderer interface, so backends plug in without touching game code.

---

## 8. Texture system (spec, not yet implemented)
- **Source files:** every texture is its **own PNG file**. A texture registry maps names to IDs, and blocks pick a texture by name for each face.
- **On the GPU,** the PNGs are packed into a **texture array** at load time, not a classic atlas. That means no edge bleeding, clean mipmaps, and one bind per draw. GL 3.3 supports arrays, so every backend can share this.
- **Fixed tile size per pack,** starting at 16×16 or 32×32. **Missing textures show as a magenta/black checker** so they're obvious.
- **Wire strength glow is done in the shader** as a tint, not as 65 separate textures.
- **Animated textures** use vertical frame strips plus a small sidecar file for frame timing. They'll be used for lamps, the Pulsar, and lithography machines.
- **Hot reload in debug mode,** so new art shows up live in the running game.
- **Resource pack overrides** will support mods and the Steam Workshop.
- **Later, for 4 GB cards:** a cached packed array for fast startup, plus GPU-compressed formats to save VRAM.

## 9. Art
- **The direction is AI-generated art first,** with a locked style anchor: flat, smooth, anti-aliased. A human artist may come later if the game takes off.
- **CloudAtlas,** an AI teammate, is now producing the art.
- **CloudAtlas's first batch:**
  - the logo
  - block textures: silicon, both wires with strength states, both torches, the lamp, and Overworld blocks
  - hotbar and HUD pieces
- **The red/blue colorblindness check** you asked for is part of CloudAtlas's brief.
- **What you see in the screenshots is placeholder flat color.** No real art is in the game yet.

---

## 10. Development environment and hardware
- **Language and tools:** C++20, CMake, and MSVC (Visual Studio 2022 Build Tools) on Windows, plus GCC on Linux for automated headless testing.
- **Libraries:** GLFW for windows and input, OpenGL 3.3 core for rendering today, and AngelScript 2.38 for scripting.
- **Baseline hardware:** a mid-range Windows 11 laptop from a few years ago with an **NVIDIA RTX 3050 Ti (4 GB VRAM).** The game must run fast there first.
- **Team:**
  - Jaydee is the director (product and design).
  - Grok AI is the main coding partner.
  - Elfie (you) handles design consulting.
  - CloudAtlas handles AI-generated art.
- **Source control:** a private local repository.

---

## 11. Not done yet (honest status)
- **No gravity or walking.** The player always flies, and blocks left without support float.
- **Not implemented yet:**
  - Repeater, comparator (including threshold mode), redirector, fast latch, switch, button, and 32-color display pixel.
  - The **Pulsar** (design only).
  - **Heat,** fans, and heat sinks.
  - **Lithography and the JIT** (only a placeholder 512 Hz clock domain exists).
  - **Tick Beacons** (a debug key changes the whole world's tick rate instead).
  - **Saves.** The compact bytecode format is designed but not built.
  - **The text editor and compiler.**
- **The Overworld is a placeholder** (simple grass/dirt/stone terrain), and there are **no portals**.
- **No real art or textures.** Everything is flat placeholder color, and the texture system is spec only.
- **No Vulkan backend yet.** Only OpenGL 3.3 is running.
- **No survival systems** (mining, crafting, materials, mobs) and **no audio**.
- **Scripting only exposes a couple of functions so far.** The real modding API comes later.
- **Real performance numbers from the laptop are pending.** The figures in this doc come from a GPU-less test machine.

---

## 12. Timeline
- **Oct 12:** the coding-assistant usage window resets, and **heavy building starts next week.**
- **Next up (likely order):** repeater and comparator (with threshold mode), the switch/button/latch set, gravity and walking, then the texture system with CloudAtlas's first art, then the Vulkan backend.
- **Full prototype: 3–6 months** at a steady, deliberate pace.
- **Steam: possible launch by summer 2027.** Your Next Fest demo idea, ending on the first lithographed chip, is the working plan.

---

## 13. Open questions

### 13.1 Still open from your list (carried over)
1. Beacon cost, range, and power-draw numbers.
2. Threshold input: side-power reference vs dial (we're leaning toward side-power).
3. Rocket phase depth: full guidance loop on the player's CPU vs guidance-assisted with an autopilot fallback.
4. First-hour survival hook: the very first thing a player builds.
5. Lithography costs in detail: materials per tier and how print time scales.
6. Silicon-nature world details: biomes, mobs, and hazards.
7. In-game naming and branding for machines, tiers, and materials.

### 13.2 New since v1.0
8. Pulsar: rising vs falling edge, color variants, collision rules, receiver design, heat.
9. In-game programming language for the editor and compiler.
10. Overworld scope: how big it is, which materials live only there, and how portals are built and gated.
11. Torch color interactions beyond the current rule (Jaydee plans to design more).
12. The full grind path now that the editor/compiler step comes before lithography.

### 13.3 Specific questions for Elfie
1. **Pulsar receiver/decoder:** should players build the decoder themselves (for example a comparator threshold ladder that turns a 0–64 pulse into 6 bits), or should there be a dedicated receiver block? How do we teach multi-level signaling without it feeling like homework?
2. **Pulsar unlock path:** what should earning it look like? Is it a crafting recipe after the first lithographed chip, found in a Syber-Squad-style "inside the chip" dungeon, a reward from a job-board contract, or something else? Where exactly in mid-to-late game does it land?
3. **In-game editor/compiler language:** should it be an **AngelScript-lite** (the same C++-style language modders use, which reinforces the education angle), a **custom assembly** language for the player's own CPU (closer to real hardware), or a **two-step** approach (assembly first, a higher-level language unlocked later)? How should the compiler's output feed the Lithography Machine?
4. **Editor/compiler as a gate:** is building them before lithography fun or a second wall? Could early versions be found or partly built to soften it?
5. **Overworld design:** which materials should be Overworld-only (for example metals for fans, optics for the litho mirrors, fuel for the rocket)? Should it have its own mobs and hazards, or stay mostly peaceful as a contrast to the virus-infested Wafer? Should portals be one of the first "toys"?
6. **Syber-Squad structure:** should "going inside a chip" be literal (a player-built chip becomes a dungeon), a separate dimension, or arenas only? Should viruses attack players' circuits on the Wafer, and how do we keep that from feeling punishing to builders?
7. **First-hour hook:** given what's playable now (the torch/wire/lamp inverter toy), what's the very first thing a new player should build, and what should the first 10 minutes look like?
8. **Beacon numbers:** proposed starting values for cost, radius, and power draw at 64/128/256 TPS, and how they interact with heat.
9. **Rocket depth:** full guidance loop on the player's CPU vs assisted with a fallback. Which gives a better ending for the widest audience?
10. **Simpler first lithography target:** should a counter or 2-bit adder qualify for the first litho tier instead of a full CPU?
11. **Red/Blue torch logic:** any puzzle or teaching ideas that use color-steered torches (one torch lane, two wire lanes)?
12. **Anything in the screenshots** (HUD layout, readability of wire strength, red vs blue contrast) that should change before real art arrives?

---

## 14. Contents of this package
- **This update (v1.1):** `Voxel-Compute-Update-for-Elfie.md`
- **Previous brief (v1.0), for context:** `Voxel-Compute-Consulting-Brief-v1.0.md`
- **`images/screenshot-vertical-slice.png`:** the Wafer with demo circuits, the HUD, and the controls panel
- **`images/screenshot-overworld-placeholder.png`:** the placeholder Overworld dimension
- **`images/module-dependencies.png`:** the engine module and dependency diagram

## 15. How to work with us
- **Reply in whatever form works for you,** but tying feedback to section numbers helps (for example "Re: 13.3 Q3, editor language...").
- **Only the items marked decided are locked.** Everything else is open for discussion.
- **Please keep this project and package private.**

*Voxel Compute: one man, one Grok, one game.*
