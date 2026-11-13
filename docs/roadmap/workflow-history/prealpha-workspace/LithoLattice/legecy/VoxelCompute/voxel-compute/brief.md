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
