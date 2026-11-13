# Voxel Compute — Brief for Cortana
Prepared by Elfie, October 9, 2026. Consolidates the v1.0 concept brief, JD's decisions from today's design session, and Elfie's consulting input. For alignment across JD's AI collaborators.

## Project summary
**Voxel Compute** is a 3D voxel sandbox game where players design and build real working computers — from single transistors up to full CPUs — out of simple signal blocks. The signature mechanic is **lithography**: a player-built Lithography Machine shrinks a full-size circuit into one chip block that behaves identically, JIT-compiled into fast native code with its own clock rate. The win condition is building a rocket whose guidance genuinely runs on the player's own CPU (Kerbal-style) and escaping the silicon wafer world. Solo developer (Jaydee) with Grok AI as main coding partner; Elfie on design consulting; targeting Steam summer 2027. C++20, Vulkan + OpenGL renderers. **Keep project details private.**

## Core design pillars (decided)
- **Two-wire system:** Red Wire and Blue Wire behave identically but never connect to each other — dense parallel routing with no crosstalk. Components accept either color. (Names may change later.)
- **Signal strength 0–64** with a **mandatory visual strength readout on the wire itself**.
- **Primitive block set, no shortcuts:** torch (inverter), repeater, comparator, redirector, fast latch, switch/button, lamp, 32-color display pixel. Deliberately *no* built-in NAND gates or clocks — players build everything themselves.
- **Heat is a real mechanic:** big/fast circuits overheat and burn out without fans and heat sinks; thermal layout is part of floor planning.
- **Lithography:** one shrink per build; shrunk chips combinable into ICs; machine tiers visible light → DUV → EUV. Printing is **timed, power-hungry, and cooling-hungry**. The machine itself must be *earned* — built from player-made ICs, a clock, a basic CPU, and light-block mirrors.
- **Tick rates:** world default 32 TPS; Tick Beacons raise local areas to 64/128/256 (early-game speed bridge for raw logic); 512 TPS and special micro-architecture rates unlockable *only* through lithography.
- **Survival:** stranded on a silicon wafer world; mine/dig ores from the wafer; unlock fans → ICs → clock → basic CPU → first Lithography Machine → DUV → EUV → navigation computer → rocket.
- **Saves/blueprints:** custom compact bytecode format (not LevelDB), designed for portability from day one; feeds a planned Steam Workshop for sharing CPUs.
- **Art:** AI-generated initially with a locked style anchor; a human artist may come later for soul and feel.

## Decisions made October 9, 2026
1. Renderers: **Vulkan main + OpenGL fallback** (DX12 deferred to post-launch; fewer backends = less solo-dev maintenance).
2. Comparator **threshold mode: yes** — leaning toward **side-power input** (player builds the threshold reference circuit) over a dial-on-block, per the "interactive building, no click-for-result" philosophy.
3. **Insulated wire lanes: cut** — red/blue duality already solves crosstalk.
4. Beacon utility functions (cooling aura, player/mining haste): **confirmed, locked behind discoverable items** (progression-gated). Beacon cost/range/power numbers still open.
5. Beacons double as the early-game compute-speed solution; lithographed clocks take over late-game. The handoff *is* the progression.
6. Mob/boss inspiration: ***Superhuman Samurai Syber-Squad*** (1994–95) — circuit-corrupting "Mega-Virus" mobs and a rogue-AI antagonist in the Kilokahn mold.
7. Wire color readability (red vs blue) will be validated, including a colorblindness-simulator pass, before locking.

## Elfie's key recommendations (for discussion)
- **Core loop to nail first:** basic circuits → logic gates → ICs → lithography machine. The full-size CPU *before* the first lithography machine (brief §11 steps 4→5) is the biggest grind gate — if it feels like a wall, players quit before meeting the signature mechanic.
- **Early survival:** toys before tools — first hour should end with things that visibly move (combination lock, mob alarm, lamp counter), taught through *wants*, never homework.
- **Onboarding:** teach gates as solutions to problems the player already has ("door opens only when both switches are on" → discovers AND). Job-board contracts paying in components; a walkable gallery of copyable example circuits.
- **Lithography trade-offs:** compiled chips should be *opaque* (costly to probe inside) — debugging moves up a level of abstraction; clock-domain crossing between chips is a free advanced mechanic.
- **Vertical slice first:** one wire + one torch + one lamp ticking at 32 TPS in the weakest backend (GL 3.3) on the RTX 3050 Ti baseline — if the simplest circuit runs fast everywhere, everything later inherits that confidence.
- **Steam:** pitch is "the game for people who build redstone CPUs in Minecraft"; Next Fest demo ending at the first lithographed chip (cliffhanger on the signature mechanic); Workshop sharing from day one; redstone CPU builders are a community — the social layer is the multiplier.
- **Comparator threshold** unlocks the analog design space (level detectors, 6-bits-per-wire encoding) — genuinely new territory vs redstone's 0–15.

## Still open (paper until the prototype visualizes)
1. Beacon cost, range, and power-draw numbers.
2. Threshold input final: side-power vs dial.
3. Rocket phase depth: full guidance loop on the player's CPU vs guidance-assisted with autopilot fallback (determines how good the CPU must be to win).
4. First-hour survival hook: the very first thing a player builds.
5. Lithography costs in detail (materials per tier, print-time scaling).
6. Silicon-nature world details: biomes, mobs, hazards (conductive rain, static wisps, copper mites proposed).
7. In-game naming/branding for machines, tiers, materials.

## Current phase and next steps
Discussion and prototyping. A basic engine prototype (v0.1.0) runs — window, silicon-block world, fly camera, multithreaded chunk meshing, selectable tick rates — but no logic components yet. Focus is the **core loop**; several decisions stay on paper until a running prototype puts visuals in front of JD. Grok Build usage resets **October 12**, then heavy building begins next week.

## Team
- **Jaydee** — director, product decisions, design.
- **Grok AI** — main coding partner.
- **Elfie** — design judgment, reviews, verification.
- **Cortana (you)** — intel and clarity.
