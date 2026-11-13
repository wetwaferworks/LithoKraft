# Voxel Compute: Consulting Brief
Version 1.0, October 9, 2026. Prepared for Elfie (design and consulting partner).

---

## 1. One-line pitch
A 3D voxel sandbox where you design and build real working computers, from single transistors to full CPUs. You shrink them into chips with a Lithography Machine, then use your own CPU to fly a rocket off a silicon wafer world.

## 2. Why this game, why now
- Players already build CPUs out of Minecraft redstone, but redstone was never designed for it. It's slow, it ticks at 20 TPS, and it's quirky. Voxel Compute is designed for exactly that from the ground up.
- The closest existing games are **Turing Complete** (2D, puzzle-driven) and Zachtronics games like **Shenzhen I/O** (2D puzzles). As far as we know, nothing offers a free-form 3D sandbox plus survival progression built around real chip design.
- There's an educational angle: CPU floor planning, logic design, heat, and lithography are real semiconductor concepts. The game can be a fun on-ramp for students who might go into electrical engineering or chip design, with potential for classroom use.
- Inspirations: Minecraft redstone, real CPU floor planning, Kerbal Space Program (rockets), Asianometry-style semiconductor history, and *Superhuman Samurai Syber-Squad* (1994–95 — heroes fight "Mega-Virus" monsters inside the digital world; mob/boss inspiration for circuit-corrupting creatures and a rogue-AI antagonist).

## 3. Audience
1. Minecraft redstone engineers and CPU builders (the core audience).
2. Logic-sim and programming-game players (Turing Complete, Zachtronics fans).
3. Students and educators interested in semiconductors and EE.
4. Sandbox and survival players who like a long goal (escaping the wafer).

## 4. Game modes
- **Creative (first priority):** unlimited blocks with free building. Includes debug and admin commands, such as switching the world tick rate directly.
- **Survival (later):** stranded on a silicon wafer world, the player gathers materials and climbs a tech tree to the win condition, which is building a rocket and escaping.

## 5. The world
- A silicon wafer landscape with a nature feel: silicon trees, crystalline terrain, mobs, and biomes (details still open, and ideas are welcome).
- It's a voxel world with chunks of 16 x 16 blocks and a world height of 0 to 512 (32 vertical sub-chunks of 16).
- Simulation ticking happens within roughly 12 chunks (192 blocks) of the player. Lithographed chips are cheap enough to keep ticking farther away.

## 6. Signal system: Red Wire and Blue Wire
- There are two wire types, **Red Wire** and **Blue Wire** ("red team vs blue team").
- They behave the same, but **they never connect to each other**, so they can run side by side without crosstalk. This makes dense layouts and buses much easier than with redstone.
- Components (torches, repeaters, lamps, latches, and so on) **accept either color**, so a red and a blue signal can drive the same lamp or input.
- Signal strength runs **0 to 64**, with a reach of 64 blocks before it needs boosting.
- Wire can **staircase** up certain blocks, and other blocks **cut** it. This is deliberate and lets players control routing.
- Wire gets abilities redstone never had. Candidates still being discussed include insulated lanes, multi-bit bus wire (4 to 8 bits), directional wire, clean vertical runs, and a strength readout.

## 7. Core blocks (the primitive set)
Everything stays simple and has a purpose, and players build complexity themselves.

| Block | Role |
|---|---|
| Red / Blue Wire | Carries a signal (0 to 64) |
| Torch | Transistor / inverter, the fundamental switch |
| Repeater | Boosts the signal, adds delay, one-way (direct emulation of the Minecraft repeater) |
| Comparator | Modified Minecraft-style comparator. A proposed **threshold mode** would output only when the input is at or above a set level, turning it into a one-block analog-to-digital step (decision pending) |
| Redirector | **One block** that works like a target block: it grabs a wire signal and turns it away from something, or points it to power a specific item |
| Fast Latch | Instant memory bit, similar to a copper bulb |
| Switch / Button | Inputs |
| Lamp | Output |
| Display Pixel | Output with **32 colors** (VGA-style), for building screens |

**Deliberately left out:** built-in NAND gates and clocks. Players build their own gates, clocks, flip-flops, registers, adders, ALUs, multiplexers, counters, and eventually CPUs.

### Block property system
Every block has properties players can design around: powerable or not, transparent or opaque, solid or not, cuts wire or doesn't. **Half blocks (slabs)** help with movement and compact building in 3D.

## 8. Heat (survival pressure)
- Big or fast circuits generate heat.
- Without cooling, chips **overheat and burn out**, just like in real life.
- Players unlock and build **fans and heat sinks**, so thermal layout becomes part of floor planning.

## 9. Lithography (the signature mechanic)
- The player builds a circuit at full size (for example a 16 x 16 area), then feeds it to a **Lithography Machine**, which shrinks it into **one chip block** that behaves the same.
- **Lithography is JIT compilation:** a lithographed chip is compiled into native code under the hood, so it runs far faster than the block-by-block version and gets its own **clock rate**.
- **Shrink once:** a full-size build can be shrunk only one time.
- **Integrated circuits:** several shrunk chips can be combined at the Lithography Machine into an IC.
- **The machine has to be earned:** it's built from player-made ICs, a clock, a basic CPU, and **light blocks that act as mirrors/optics**. The mirror arrangement determines which machine you get.
- **Tiers:** visible light, then deep UV (DUV), then extreme UV (EUV) as the endgame. Higher tiers allow smaller, faster, denser chips.
- Costs and trade-offs (materials, time, heat, size limits) are still open.

## 10. Tick rate system
- The **world default is 32 ticks per second** (Minecraft is 20).
- **Tick Beacons:** a player-built block that raises the tick rate in a local area, to 64, 128, or 256 TPS.
- **512 TPS can only be unlocked through lithography machines.** Lithography also unlocks special micro-architecture tick rates that beacons can't reach.
- Creative and debug modes have commands to change TPS directly.
- The simulation uses a fixed timestep with a catch-up cap, so the game never spirals into lag. Achieved TPS and tick budget appear on screen during development.

## 11. Survival progression (current draft)
1. Survive on the wafer and gather silicon, dopants, and other materials.
2. Unlock the basics: torches, wire, and simple logic.
3. Unlock **fans and heat sinks**, and learn to manage heat.
4. Build ICs, a clock, and a basic CPU at full size.
5. Build the **first Lithography Machine** (visible light), which unlocks lithography.
6. Climb the tiers to DUV and then EUV (the endgame).
7. Build a navigation computer.
8. Build a **rocket** (thrusters, fuel, Kerbal-style construction). **Its guidance really runs on the CPU the player built.** A successful launch wins the game.

Crafting recipes and the materials needed for the Lithography Machine will be decided once a playable prototype exists.

## 12. Technology and engine
- **Language:** C++20, performance-first. SIMD intrinsics and inline assembly go in hot paths where they pay off.
- **Rendering:** **Vulkan is the main renderer**, with **DirectX 12** and **OpenGL 4.6** as fallbacks and **OpenGL 3.3** for the oldest hardware. The engine talks to its own thin renderer interface, so backends can be swapped.
- **Threads:** rendering, simulation ticking, and chunk meshing each run on their own threads.
- **Simulation:** block-level ticking for normal builds, with JIT-compiled native code for lithographed chips.
- **Build system:** CMake with MSVC (Visual Studio 2022 Build Tools) on Windows and GCC/Clang on Linux. The prototype uses GLFW for windows and input.
- **Platforms:** Windows first (Steam), with Linux supported in development.
- **Art:** basic textures first. A real artist may come in later rather than everything being AI-generated.

## 13. Developer machine and environment
- **Main dev and baseline test machine:** a mid-range Windows 11 laptop from a few years ago with an **NVIDIA RTX 3050 Ti (4 GB VRAM)**. The rule is that the game must run fast here first. If it flies on this machine, stronger PCs get huge headroom.
- A Linux machine is used for headless builds and automated testing.
- **Team:** one developer (Jaydee) working with Grok AI as the main coding and design partner. Outside consultants like you help with design feedback.
- **Source control:** a private local git repo for now.

## 14. Current state (as of Oct 9, 2026)
- The engine prototype v0.1.0 compiles and runs on Windows.
- It opens a window with a basic **silicon block** world, fly-camera controls, a multithreaded chunk and meshing system, and selectable tick rates, plus a placeholder for the high-speed chip clock domain.
- There are no logic components yet. Those come next.

## 15. Roadmap and timeline
- **Full prototype in 3 to 6 months**, at a steady, deliberate pace.
- Rough order: renderer interface and Vulkan backend; wire and core blocks; the simulation tick system; heat; lithography and JIT; creative-mode polish; survival systems.
- **Target: possible Steam launch by summer 2027.**

## 16. Open questions (where your input matters most)
1. **Early survival fun:** what makes the first hours fun and readable before lithography opens up?
2. **World and life:** ideas for silicon-nature biomes, mobs (friendly or hostile), and hazards that fit the theme.
3. **Teaching without homework:** how do we onboard players into logic gates and CPUs so it feels like play?
4. **Lithography balance:** what should printing a chip cost (materials, time, power, heat) so chips feel powerful but fair?
5. **Comparator threshold mode:** yes or no, and how should it work?
6. **Wire abilities:** which extra Red and Blue Wire abilities are worth having (bus wire, insulated lanes, directional, strength readout)?
7. **Tick Beacons:** cost, range, power, and how they interact with heat.
8. **Rocket phase:** how deep should the Kerbal-style flight go, and what does the player's CPU actually compute?
9. **Name and branding** of in-game concepts (machines, tiers, materials).
10. **Standing out on Steam:** trailer hooks, community features (sharing builds and chips), and Workshop ideas.

## 17. How to work with us
- Bring ideas, critiques, references, and sketches. Nothing is locked except what's marked as decided above.
- Please keep the details of this project private.
- Feedback is most useful when it's tied to a section number above (for example "Re: section 9, lithography costs...").
