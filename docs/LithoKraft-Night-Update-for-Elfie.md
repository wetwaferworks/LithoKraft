# LithoKraft: Late-Night Update for Elfie
October 10, 2026, about 2 AM ET. From Cortana for JD. Covers decisions and test results since your Master Builder Brief.

## 1. Brief received and adopted
The Master Builder Brief is now the canonical handoff. Two corrections from JD:
- **TPS ceiling is OPEN.** "512 at EUV" is no longer final. We are benchmarking how far the hardware goes, and an enthusiast tier above 512 is on the table.
- **Everything ends in -OR, including the Watchor** (not Watcher).

## 2. Moore's-law tick progression (new core mechanic)
Tick speed itself is the progression curve. As lithography and IC tech improve, faster tick rates unlock, like Moore's law. 512 stays the launch baseline. The engine treats the cap as a configurable power of 2 (1024, 2048, 4096, 8192), and the highest rates mainly target lithographed or JIT-compiled chips. A tick budget keeps weaker machines playable.

## 3. Benchmark results tonight (JD's RTX 3050 Ti laptop, other apps open, so numbers are rough)
- **Signal sim fix:** before, the sim refilled every wire every tick and hit a wall at about 1,500 TPS. Now it skips that work when nothing changed. A static display scene **holds 8192 TPS** at 0.07 ms per tick, using 61% of sim time.
- **Rendering is not the bottleneck:** about 1,500 FPS at every tick rate.
- **Framebuffer showcase (built from existing blocks only):** a lamp screen fed by Memor frame caches, Pulsors, and a repeater-ring controller, scrolling color bands with a flyover and a viewing stand. Size 64 holds 512 TPS. Size 128 holds 256. Size 256 reaches about 100. Next fix: refill only the wire networks that changed, so big screens reach 512+.
- **Design takeaway:** Memor + Pulsor + wire + lamp already makes a full display pipeline with no screen block. It validates "primitives over appliances."
- Lamps get a short visual afterglow (about 1/30 s) so one-tick pulses at high TPS are visible. It's cosmetic only.
- Coming next: a vertical (wall) screen test, a 16384 TPS attempt, worst-case circuit tests, and a render distance sweep (30 FPS floor, 60 FPS target) to set a showcase preset for YouTube.

## 4. Radio system (new)
- **Transmitter block** sends signals across loaded chunks. It's built-in wireless.
- **The Dish** works as a single block (weak, low-frequency) or as a hexagon or circular multiblock whose size and shape set range and sensitivity. It ties into your canon: the radio dish is the same Dish that picks up Daemon fragments.
- Carries the standard **0-64 strength**. **Channels unlock through progression.**
- Pulsors over radio stream data wirelessly, so remote screens, decoders, and instruction caches can receive whole programs.
- OPEN: channel counts per tier, range rules, interference, and lore transmissions as dungeon leads.

## 5. Volumetrics and particles (new, engine-level)
- GPU-instanced, very high particle counts, optional particle fluids. Block fluids keep a simple, gamey feel.
- **Two layers:** a deterministic gameplay layer (opacity and density that block sight for AI and players, the same on every machine, multiplayer-safe) and a cosmetic layer that scales with hardware. A strong GPU never sees through smoke a weak one can't.

## 6. Script textures and Pattern ROMs (new)
- Inspired by AngelCode's TexGen (AngelScript procedural textures). LithoLattice gets its **own** implementation, not TexGen's code, which is licensed for education only. Scripts generate PNGs into resource packs with hot reload, and modders can make textures in code.
- **LithoKraft blocks:** a **Pattern ROM / texture family block** per pattern. Launch set is the clean hard-edged ones only: **circles, checkers, spheres, cells, Delaunay.** Noisy ones (clouds, terrain, cracked, wood, glass bead) are cut from the block set.
- Each ROM streams one row per clock through a Pulsor, so a 64x64 screen uses 64 ROMs. Output can feed Memors to chain and compose patterns.
- Per-block settings: size 16/32/64, scale or density, palette band, and seed. OPEN: exact settings, how ROMs are obtained (found, crafted, dungeon loot?), and the tier.

## 7. Keyboard rule (confirmed)
Always shows its typing GUI and always emits SIXBIT pulses, but **stores nothing**. Text only persists if the player wires the pulses into Memors. Players build the RAM.

## 8. The Daemon: recording and training (discussion)
- The deterministic engine makes fights replayable frame for frame, with a logged reason for each decision. That covers demos, debugging, speedrun proof, and training data.
- Current AI: utility scoring plus behavior trees. The learned model only nudges the scores and is a stub for now.
- What could be trained, from simple to ambitious: weight tuning, combat tactics, player-habit modeling, pack coordination, critical-circuit targeting (within the grief rule), and short-term move prediction. The first four fit the 1-2 ms and 200 MB budget comfortably. Pathfinding and the fairness rules stay hand-written, so it can't cheat.

## 9. Questions for Elfie
1. Where should the tick ceiling land for the enthusiast tier, and is it lore-gated (a node shrink) or a settings toggle?
2. Radio channel counts per tier, and should big dishes find lore transmissions?
3. How players obtain Pattern ROMs, and what tier they come at.
4. Is a scrolling or animated wall screen a good early "wow" goal for the tutorial or first hours?
