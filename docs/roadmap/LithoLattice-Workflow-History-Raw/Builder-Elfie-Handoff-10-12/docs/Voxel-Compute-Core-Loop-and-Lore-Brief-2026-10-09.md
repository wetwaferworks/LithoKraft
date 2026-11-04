# Voxel Compute — Core Loop & Lore Brief
October 9, 2026. Prepared by Elfie from the day's design discussion. For Cortana and the squad. Companion to the Lore Bible v0.1 (`Voxel-Compute-Lore-Bible-v0.1.md`). **Keep private.**

---

## 1. Core gameplay loop, start to finish

1. **Wake on the Wafer.** You are an agent — a program — on a silicon wafer the size of a world. First minutes: wonder (fly over the wafer), first toy (complete the inverter gap, watch the lamp die — pure cause and effect).
2. **Learn the primitives.** Red/blue wire, torches, lamps. First job-board contract: an AND-gate door lock. Gates taught as solutions to wants, never homework.
3. **Build the first portal** (basic circuits) → step into the **Organic World**. First interdimensional trip ends hour one. Gather natural materials.
4. **Back on the Wafer:** logic gates → fans and heat sinks (heat matters from here on) → ICs → a clock → a basic CPU.
5. **Explore Organic World ruins** (Egyptian/Aztec-styled tech centers, desolated modern cities — placed structures, not fully procedural). Find the **partially-built typewriter controller**, plus books and blueprints that teach lore *and* hand you the next build goal.
6. **Build the text editor and compiler.** First program is trivial (blink a lamp) — toy before tool. In-game language: custom player-built assembly (~12 instructions) plus one built-in learning ISA.
7. **Build the first Lithography Machine** (visible light). First prints are simple — 2-bit adders, counters — teaching the *verb* (build → print → chip) before demanding a CPU.
8. **Climb the tiers:** DUV (CPU parts) → EUV (full SoC). Earn the **Pulsar** from a Wafer chip dungeon — go inside the chip, fight viruses, take the schematic.
9. **Build the dimensional portals** in the Organic World hub: Underworld needs heat-shielding agent armor; the Void needs a lithography-key portal built with optics.
10. **Underworld** (backside of the wafer): deep mining, startin and EUV-tier ores, heat as the constant enemy. **Void** (cyberspace): chip dungeons, bosses, the rogue intelligence's domain.
11. **Build the navigation computer, then the rocket.** Your CPU flies the guidance and staging — clean landing if your engineering holds; autopilot fallback gives a rough landing if it doesn't.
12. **Escape to space = win.** Space is the ending for now, with new worlds left visibly open for later.

**The rhythm:** Wafer → Organic → Wafer → Organic → Underworld/Void → Space. The back-and-forth keeps both home dimensions relevant all game. Tension (Wafer/Void) alternates with release (Organic World).

## 2. Dimensions

| Dimension | Role | Identity | Entry gate |
|---|---|---|---|
| Wafer | Start, tech lab, home base | Clean silicon, chip dungeons | — (start) |
| Organic World | Hub, gathering, portals | "The world that forgot" — fantasy surface (castles, magic), ruined tech underneath | Basic circuits (portal) |
| Underworld | Deep mining, endgame ores | The backside of the wafer — dark, hot, deep | Heat-shielding agent armor |
| Void | Bosses, chip dungeons | Cyberspace — not physical, Tron-wrong | Lithography-key portal (optics) |
| Space | Ending (expansion hook) | — | Rocket |

## 3. Lore (from the Lore Bible v0.1)

- **Premise:** you are a program waking on a world-sized computer. Something lived here before you.
- **The old civilization:** technologically advanced, ended, left ruins/dungeons/salvage/books. *What ended them is OPEN* — candidates: (A) their rogue AI escaped, (B) thermal runaway baked the wafer, (C) they finished and left.
- **The antagonist:** a rogue intelligence (Kilokahn mold). Corrupts circuits, spawns virus mobs, always telegraphs its surges. Lives in the Void. Origin and name OPEN.
- **The agent:** who deployed you and what you remember is OPEN.
- **Delivery rules:** books + blueprints do double duty (story + next build goal); ruins are environmental storytelling; chip dungeons are unfinished work made explorable; teach through wants, never lore-dump.
- **Tone:** wonder first, exposition later; the player stays slightly unsure; magic and tech coexist unexplained.

## 4. Organic World detail (from today's discussion)

- Fantasy timeline: castles, magical elements, wilderness — **mostly peaceful**, the breathing dimension.
- Ruined tech centers (Egyptian/Aztec styling) and desolated modern-style cities serve as **dungeons**, generated as placed structures for bounded cost.
- Unique materials: fan organics, optics-grade quartz (litho mirrors), rocket fuel components.
- Mobs use familiar archetypes with original mechanics — and **every mob feeds the material economy**. The "zombie" carries salvaged civilization components the tech tree needs. No mob exists that doesn't answer "why do I care?" with a material.

## 5. Art direction notes (from today's discussion)

- Style anchor (locked): flat, smooth, anti-aliased, dark outline on wafer-gray casing.
- Upcoming batches: heat/cooling → ores → lithography machine (hero piece) → beacons → Organic World identity → rocket parts → portals/dungeon dressing.
- **Tier trim system:** copper → silver → violet → white-gold, applied game-wide so progression reads through trim color alone.
- **Heat visual language:** emissive orange edges scaling with temperature, shared across all heat-producing blocks.
- **Etched wafer silicon:** faint circuit traces in the base silicon texture — "you live on a chip" in every screenshot.

## 6. Decisions locked today

- Portal gating per dimension (circuits / heat armor / litho key).
- Space = ending now, expansion later.
- Typewriter controller found in Organic World ruins (exploration reward + lore + progression gate in one beat).
- Organic World = "the world that forgot"; fantasy surface, dead tech underneath.
- Mob economy rule: every mob drops something the tech tree wants.
- Lore Bible v0.1 exists and is the canon reference; Elfie maintains it.

## 7. Still open

- What ended the old civilization (three candidates above — JD picks).
- Nature/name/endgame role of the rogue intelligence.
- Who the agent is to the old civilization.
- Beacon cost/range/power numbers.
- Lithography material costs per tier and print-time scaling.
- Virus attack frequency and shielding definitions.
- Retro sound direction (proposal on the table: "the computer sings" — reactive FM/chip-tune audio).
