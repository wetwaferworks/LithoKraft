# NPC AI Questionnaire — JD's Decisions
October 9, 2026. Walked through with Elfie (game design chief). For Cortana / engine spec. Supersedes Elfie's earlier recommendations where they differ.

## 1. Who are the NPCs?
- **Launch roster:** deep-Wafer failure-mode hostiles (Leakage, Drain, Short, Electromigration, Latch-Up, Float, Bit Rot, glitch-mites); Organic World savage cyborgs; Underwafer: the Collective; Void: virus swarms + bosses; friendlies: solder sprites, charge motes, the Librarian, clockwork beetles, friendly cyborgs, prism grazers, hollow ones; neutrals: bus herds, ferrite beetles, echoes, drifters (+shardlings when provoked).
- **Friendly cyborgs:** live in surviving cities/villages as **traders**; wandering ones are **quest-givers and ruin guides** (give maps). Intact = copper trim + calm patrols; savage = spark and twitch. No repair-tech role.
- **Player-built NPCs:** YES — flagship. Chassis block wired to the player's own CPU ("arsenal programs").

## 2. Core behaviors
- **Movement:** walking, climbing (some mobs), flying per dimension. **Wire-following** for failure-mode mobs. Climbing is a **LithoLattice engine-level feature**. Fluids exist everywhere: Wafer chemical baths, Organic water/lava/baths, Void usable-not-generated, Underwafer baths/salts only.
- **Circuit-smarts:** yes — mites seek exposed wire, Float haunts unconnected inputs, swarms drawn to signal activity, Underwafer threats chase heat. (Active sabotage scope TBD.)
- **Attacks:** melee + signal/block corruption; ranged for Void; AoE for bosses only; always telegraphed.
- **Defense/senses/groups/spawning:** per Elfie's recommendations (fleeing friendlies, pack cyborgs, swarm viruses with scouts; signal + heat senses; spawn on signal activity/heat/dungeon triggers/surges).

## 3. Difficulty and fairness
- Difficult but fair; **everything has a definable counter**. Traps and mob farms are legit play (building-game contract).
- Scales with **player tech tier AND dimension**. Settings: Easy / Normal / Hard / Hardcore.
- **Hard rules (engine invariants):** no perfect aim, no seeing through walls, always a counterplay, grief rule (build-damagers: rare, telegraphed, counterable).
- **Overclockor ward mode** shields builds; **friendly mobs can partner in build defense**.

## 4. The learning AI
- **The Daemon learns per world** (your tricks) **+ opt-in global pool** (gets smarter as the player base figures it out). Easy/Normal are **opted out** of the global brain. Players *will* try to farm it — expected.
- Final boss on **all** difficulties. **Drops the Starship control system** (Raptors buildable earlier; the ship can't fly without the drop).
- Model scope: **combat only** (targeting, attack/defend/flee) — not pathfinding. Budget: **1–2 ms/frame, 200 MB** on the 3050 Ti baseline.
- Train offline, ship fixed weights, bounded online adaptation; adaptation must be **inspectable** (visible in logs).
- Fully offline, deterministic. Modders can swap behaviors via SDK stubs.

## 5. Architecture
- **Utility AI + behavior trees**, learned model nudging scores.
- **C++ core + AngelScript hooks** (staying with AngelScript; JD may contact its developer).
- AI ticks **8–16 Hz**, separate from 32 TPS world.
- **Full AI debug overlays** approved (paths, goals, threat scores) — as a player-facing feature.

## 6. Lore and tone
- **Sympathy via sound, not dialogue boxes.** Sympathetic sounds normally; they **flip** when angered/triggered.
- Never hostile: charge motes, clockwork beetles, the Librarian. Become friendly: tamed solder sprites; **post-game defecting virus mob**.
- **Rideables:** prism ray (Organic flagship), coil skiff (Wafer wire-rail transit), **Collective Back** (Underwafer), packet ray (Void expert mount). Electronic "rider's coil" harness; taming TBD.
