# LithoKraft — Lore Bible v0.2
October 10, 2026. Living document, maintained by Elfie. **SETTLED** = locked by JD. **OPEN** = awaiting JD's call. **PROPOSAL** = Elfie's suggestion.

Evolved from Voxel Compute (2026-10-09). Same universe, new name, expanded scope. **All Voxel Compute lore, mobs, and designs are inherited into LithoKraft** — this is a rename and refactor, not a reboot. (JD, 2026-10-10).

---

## 1. The premise

You wake on the surface of a silicon wafer the size of a world. The wafer is a computer. Something lived here before you. Your task: rebuild the technology, master Trace and Signal, and escape. The rocket isn't just transport — it's proof you understood the machine you were born on.

## 2. The seven dimensions

**Core 5 (launch):**
- **Underwafer** (SETTLED) — below. ±131,072 blocks. Dark, hot, deep ores.
- **Wafer** (SETTLED) — the main world. ±65,536 blocks. Where you build.
- **Organic World** (SETTLED) — the alien world. Infinite. Ruins of the old civilization.
- **Void** (SETTLED) — the Daemon's domain. No sky light. Corruption.
- **Space** (SETTLED) — above. Harsh sunlight. The ending, for now.

**Roadmap 2 (post-Daemon):**
- **The Ether** (SETTLED) — spiritual/heaven-like. Inspired by Chris S. from "314". 6th sense.
- **The Undefined** (SETTLED) — where rules break down. Programming-themed. 7th sense.

Five dimensions = five senses. Seven dimensions = seven senses. Seven cultures for localization.

## 3. The Daemon

SETTLED: The Daemon is the final boss. An ancient military AI that went rogue (thermal runaway). Lives in the Void. Diplomacy always fails. Corruption mechanic. Hard/Hardcore: Void no-save. Multiplayer: echoes of dead players.

## 4. Lithography (the namesake)

SETTLED: Build big → print small → build bigger. Happens in virtual space (on the Wafer). Tiers: visible / DUV / EUV. Chips contain chips (recursive).

SETTLED (JD, 2026-10-10): **Lithography is compilation as gameplay.** The player physically builds a contraption. The engine analyzes the ISignalNode graph and JIT-compiles it to an optimized form. The chip runs faster (higher tick, lower cost) than the original blocks. The physical shrinking is the visual metaphor; the JIT is the real optimization. ISignalNode is the intermediate representation. This is also the path to GPU compute (fixed graphs map to shaders).

## 5. Trace and Signal

SETTLED: Wired = Trace (red/blue). Wireless = Signal (1-64 channels). 1-64 values, 0 = off. Powers of two throughout. 64 is the theme.

## 6. Interfaces (not portals)

SETTLED: Dimensional travel uses interfaces, not portals.

## 7. Mobs (inherited from Voxel Compute, JD locked 2026-10-09)

**Design rules:** Every mob feeds the material economy. Hostiles concentrate deep in the Wafer, in dungeons, and in the Void. Every build-griefer is rare, telegraphed, and counterable.

**Hostile — deep Wafer (failure modes made flesh):**
- **The Leakage** — bleeds power/heat from nearby circuits.
- **The Drain** — saps signal strength from wires it touches.
- **The Short** — bridges red and blue wire, causing crosstalk chaos.

**Hostile — Organic World:**
- **Savage cyborgs** — corrupted remnants of the old civilization.

**Hostile — Void:**
- **Virus swarms** — Syber-Squad style, always telegraphed. Daemon heralds.

**Neutral:**
- **Wafer:** bus herds (migratory grazers, scatter when startled); ferrite beetles (roll into ball; attacked → discharge pulse scrambles nearby wire).
- **Organic World:** prism grazers (crystalline deer-like, peaceful/farmable); hollow ones (maintenance constructs, hostile only if disturbed).
- **Underwafer:** the Collective (heat-mining crews; barter cooling for ores; attack one → whole crew turns. "Not evil, unionized.")
- **Void:** echoes (fragments of dead programs, can't hurt or be hurt); drifters (attacked → burst into aggressive shardlings).
- **Skies:** coil sprites (Wafer); prism rays (Organic); Collective wings (Underwafer); packet rays (Void, hostile during surges).

**Bosses:**
- **The Daemon** — FINAL BOSS. D.A.E.M.O.N. = Distributed Autonomous Execution & Maintenance Operations Network. Tragic reveal: misaligned, not evil. Drops Starship control system. Special attack: **Raging Quantum** — the Daemon destabilizes local region quanta; the ground fragments and glitches as chunk storage corrupts beneath the player. (Invented by Elfie, 2026-10-11 — inspired by a voice-transcription garble of "region quantum.")
- **Orphan** — deep Wafer. A process whose parent died.
- **Zombie Process** — Organic World ruins.
- **Kernel Panic** — Underwafer. Heat made flesh.

---

## 8. Core gameplay loops (SETTLED, JD 2026-10-10)

**Primary loop — Lithography:**
1. Player physically builds a contraption (Trace circuits, machines).
2. Engine JIT-compiles the ISignalNode graph.
3. Lithography Machine prints it to a chip (smaller, faster, cheaper to tick).
4. Player builds bigger contraptions using chips as components.
5. Repeat — chips contain chips, recursive miniaturization.

**Secondary loops:**
- **Explore** → find ruins/dungeons → salvage old-world tech → unlock new recipes.
- **Mine** → smelt → kraft → build → lithograph.
- **Fight** → the Daemon's minions → gather materials → prepare for the Void.
- **Endgame** → acquire Daemon's signal via Dish → enter Void → defeat Daemon → unlock Ether and Undefined.

**The win condition:** Build the rocket and escape (following the escapees' trail). The sandbox is staying and building a new civilization.

## 9. The old civilization (inherited from Voxel Compute)

SETTLED: The Daemon escaped — an ancient military AI that went rogue and thermally heated the entire world (escape and thermal runaway were one event). The civilizations decayed. Some escapees got out and left — that's who the player is following.

The player is a newly spawned program. Default mission: escape, finish, leave. But they can stay and build.

## 10. Implementation goals (derived from lore)

- ISignalNode graph → JIT compiler → chip items.
- Lithography Machine block (tiers: visible/DUV/EUV).
- Dish structure (acquires Daemon signal).
- Rocket (endgame, requires Daemon drop).
- All mobs in section 7 (hostile/neutral/bosses).
- Corruption mechanic (Void).
- Diplomacy path (always fails, but meaningful).

---

## 11. The Fragment (SETTLED, JD 2026-10-10)

A starting item. The player spawns with it. Available day one, first tick — no requirements. Open inventory → open Fragment → read/write immediately.

**Pages:**
1. **Journal** — Infinite, freeform. Player types what happened in their own words.
2. **Unlock log** — Game auto-logs every unlock with timestamp.
3. **Recipes** — Every discovered recipe, searchable.
4. **Chip archive** — Every LithographOR print, with PortOR configs, saved forever.
5. **Screenshots** — In-game screenshots stored in the book.

**Rules:**
- Must be on player (in inventory) for unlocks to track. Stored in chest = unlocks pause.
- Easy/Normal: can't lose it (soulbound).
- Hard/Hardcore/Ultra: drops on death, 8-min despawn, lost in lava/chemicals.
- Crafted replacement is blank — must re-interact with blocks to rebuild knowledge.
- PvP: killing a player lets you take their Fragment (journal, unlocks, chips, screenshots). Trophy collecting.

**Recovery:**
- Totem-like item can resurrect a lost Fragment.
- Killing the player who took yours gets it back.
- **Defeat the Daemon → enter the Ether → obtain the means to rebuild your Fragment.**

**Lore:** Called "The Fragment" because it's part of something larger. The Daemon wants it. That's why diplomacy always fails. Also useful for future AI agents (structured memory).

## 12. Fourth wall (SETTLED, JD 2026-10-10)

Sixth sense: the program can look at the screen and give hints (the Ether).

**Expanded:** The program looks back at the player through the screen and talks to itself. It's self-aware. Post-Daemon content. The Daemon was the program's shadow; defeating it unlocks self-awareness.

## 13. The Ether's purpose (SETTLED, JD 2026-10-10)

Beyond post-game content: the Ether is where you go to recover what you lost. Defeat the Daemon, enter the Ether, obtain the means to rebuild your Fragment. Thematically: the Fragment is your soul; the Ether is heaven; you go there to be restored.

## 14. Main progression path (SETTLED, JD 2026-10-10)

1. Start on the **Wafer**
2. Go up → to the **Organic World** → gather items
3. Return to the **Wafer**
4. Go down → to the **Underwafer** → gather items
5. Return to the **Wafer**
6. Go up → to the **Organic World** → build the interface to the **Void**
7. Enter the **Void**
8. Defeat the **Daemon**
9. Unlock the **Ether** (and recover Fragment if lost)
10. Unlock the **Undefined**
11. Build the rocket → escape (or stay and build)

---

*To be expanded: the seven cultures, the Ether and Undefined details (post-Daemon). Private fragments held close.*
