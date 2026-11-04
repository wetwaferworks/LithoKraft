# LithoKraft — Alpha Tester Brief

**For:** Citrus
**From:** JD (Jaydee Wetwork)
**Date:** 2026-10-11
**Status:** Pre-alpha. This is real code, not mockups — the architecture diagrams attached are generated from the actual codebase.

---

## What this is

LithoKraft is an original voxel sandbox game built on a custom C++ engine called LithoLattice. It's a real engine — Vulkan, OpenGL, and a software (CPU) renderer — not a mod, not a prototype, not AI-generated screenshots. The diagrams attached to this brief are rendered from the engine's actual module structure.

Alpha is targeted within the next two weeks. Right now it's pre-alpha: I'm ironing out bugs before it goes to testers.

## The engine is the pitch

If you're a redstone person, here's what matters:

**Trace (the logic system).** Red and blue wire run side by side without co-mingling. Signal strength 1–64 across wires (not 0–15). The traditional on/off system is there too. Designed by someone with years of redstone experience who's also a programmer — the quirks that make redstone painful (update-order dependence, locational behavior) don't exist here because the sim is deterministic.

**Tick engine.** 512 TPS deterministic simulation. And this is the part I want you to see: the engine ships with `tick_freeze`, `tick_step`, and `tick_unfreeze` console commands. Freeze the sim, step tick-by-tick through a contraption like a code debugger, verify the logic, unfreeze and watch it run at full speed. Java has this. Bedrock never got it. We ship it day one.

**Components.** Simple blocks that pair well:
- **Pulsor** — sends a pulse of a specific signal strength across a wire in one tick
- **CatchOR** — dial it to catch a specific Trace strength (1–64); input on one side, output on the other
- Together: 6-bit serial data transmission at high speed, one tick per value

**Lithography.** Build a physical contraption, and the engine JIT-compiles the logic graph into a chip — smaller, faster, cheaper to tick. Build big, print small, build bigger. Chips can contain chips.

**Signal.** Wireless Trace. Same 1–64 signal system, no wires.

**MCP built in.** The engine has a Model Context Protocol interface for agent control — AI agents can play the game through a real controller API, not screen-scraping.

**Moddable and extensible.** Clean mod interface for extending the game. Source stays closed during alpha — if you want to look inside, bust out your debuggers and reverse engineers.

## Platform

Linux native. Developed on Debian — you're on Debian, so you're covered. Also builds for Windows.

## What alpha testing means

- **No cost.** Testing and feedback is all I ask.
- **Focus:** creative and survival modes, starting in the first dimension (the Wafer).
- **What I need from you specifically:** storage and logistics systems. Buffers (hoppers), catches (chests), and the surrounding tech — that's the area least fleshed out right now, and it's your specialty. I want your feedback on those components above everything else.
- **Your place in history:** alpha testers are credited in the game's history. The most performant builds get legacy status.

## What's attached

Three architecture diagrams (PNG), generated from the codebase:
1. **Engine architecture** — the 8 engine modules, tick engine, renderers, mod interface
2. **Procedural generation** — world-gen pipeline, biome features, structure placement
3. **Game systems** — LithoKraft's game-layer systems: components, inventory, dimensions, UI

Story and lore details are intentionally omitted from these diagrams — you'll discover those in-game.

---

*Questions? Ask. Want to see it run first? I'm open to a screen-share or in-person demo before you commit to anything.*
