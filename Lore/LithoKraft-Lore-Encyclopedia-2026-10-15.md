# LithoKraft Lore Encyclopedia (Herominde, Squad lore officer)
*Compiled 2026-10-15. INTERNAL. Never post any of this to X or anywhere public.*

**How to read this file**
- Status tags follow the source docs: **SETTLED** (the docs say SETTLED, LOCKED or RESOLVED), **PROPOSED** (PROPOSAL, DECIDED-but-may-evolve, "in design", or proposed), **OPEN** (IN DISCUSSION, OPEN, or an unanswered question). The Coherence Sweep uses "DECIDED" for "chosen, may still evolve". I list those as **SETTLED (decided)** to keep them apart from true locks.
- **JD-CHAT** marks something JD said in chat that no doc records yet.
- Each fact has its source file in [brackets]. Abbreviations: Bible = `LithoKraft-Lore-Bible-v0.2.md`, Gloss = `LithoKraft-Lore-Glossary-2026-10-11.md`, Sweep = `LithoKraft-Coherence-Sweep-2026-10-11.md`, DVA = `LithoLattice-Dimension-Vertical-Architecture-2026-10-10.md`, Gender = `LithoKraft-Genderization-Design-2026-10-10.md`, LBG = `LithoKraft-Lore-Brief-for-Grok-2026-10-10.md`, LBC = `Lithokraft-Lore-Brief-for-Cortana-2026-10-09.md`, NPC = `Lithokraft-NPC-AI-Questionnaire-Decisions-2026-10-09.md`, F4W = `Fragment-and-Fourth-Wall-2026-10-10.md`, FDW = `First-Day-On-The-Wafer-2026-10-12.md`, BHF = `Builder-Handoff-Final-2026-10-12.md`, JRD = `Joint-Review-Decisions-2026-10-13.md`, ADR = `handoff/Alpha-Drop-Review-2026-10-15-0511.md`, GS = `LithoKraft-Game-Systems-2026-10-10.md`, SWD = `LithoKraft-Signal-Wireless-Design-2026-10-10.md`, LPS = `Lithography-Port-System-2026-10-10.md`, CRB-A = `Cortana-Return-Brief-Addendum-2026-10-13.md`, DY = `Brief-The-Days-Yield-2026-10-13.md`, SQ = `Squad-Brief-LithoLattice-2026-10-10.md`, Triv = `LithoKraft-Trivia-Round1-2026-10-11.md`.
- **Source code** (src:) is the newest tree on the box, `/workspace/ll-review/` (game `dimensions.cpp` dated 2026-10-14 22:21). Older trees in `ll_review/` and `ll_work/` are from 10-09 and were not used.
- **Coverage:** I read the core lore and dimension docs in full. I grepped the rest of the doc set for lore terms. Of `Brief-The-Days-Yield` (100 KB) I read only the lore-relevant parts. `Brief-For-Argon-Theology` is JD's personal faith statement, not game lore, so I used nothing from it.

---
## 1. Dimensions

### 1.0 Numbering and counts. These conflict; see §9.
- **JD-CHAT (latest):** 8 dimensions numbered 0–7: 0 Underwafer, 1 Wafer, 2 Organic World, 3 Void, 4 the Cosmic, 5 the Ether, 6 the Undefined, 7 the Infinite (the Infinite is PROPOSED). Launch goal: **64 dimension slots, extensible in powers of two**. No doc records either point yet.
- Docs that match 0–4: Underwafer (0), Wafer (1), Organic (2), Void (3), Space (4) [Gender; GS].
- **Source code uses a different numbering** for MCP pilot addressing: wafer=1, underwafer=2, organic=3, cosmic=4, void=5, ether=6, undefined=7, unknown=0 [src: `game/mcp_player_control.cpp`]. The header calls this "forward-looking addressing only" [src: `dimensions.h`].
- Counts over time: the doc set lists 7 (5 core + Ether + Undefined) [Bible §2]. The Sweep adds an 8th, the Infinite (PROPOSED, post-Daemon and post-Ether) [Sweep §4; BHF "Locks"].
- 5 core dimensions = 5 senses; 7 = 7 senses; 7 cultures for localization. SETTLED [Bible §2; LBG §5].
- The dimension-ID registry in the docs is not about the Infinite dimension. The `Infinite-Registry-*` docs cover infinite *content* registries (blocks and items: StableId, CRC, palettes), not the dimension [Infinite-Registry-Design / Phase1 / Phase2].

### 1.1 Shared vertical spec (all dimensions)
- 768 blocks tall = 48 sub-chunks. y 0–255 is SOLID and y 256–767 is ATMOSPHERE. The surface is at y=256. The split is fixed at engine level; what fills it is per-dimension data. SETTLED [DVA]. In code this is `kWorldHeight = 768` for every dimension [JRD].
- Bounds are game-level, not engine limits. Only the Void is unbounded. SETTLED [Sweep §4]. In code, bounds are global today, not per-dimension data, and the Wafer's ±65,536 is "not corroborated in-tree" [JRD].
- Interface coordinate ratios: Wafer ×2→Underwafer, ×3→Organic, ×4→Cosmic; Underwafer ×1.5→Organic, ×2→Cosmic; Organic ×4/3→Cosmic. The Wafer is the fast-travel hub. SETTLED. Y handling is OPEN [Sweep §4].
- Stack from top to bottom: Cosmic, Organic, Wafer, Underwafer, Void. **OPEN** ("JD may reorder") [Sweep §4].

### 1.2 Underwafer (JD # 0)
- **Status:** SETTLED, core/launch [Bible §2]. **Registry id:** `underwafer`, registered 2026-10-13 at crust level only (biome `deep_stone`: deep-stone floor plus worm caves) [src: `dimensions.cpp`/`.h`].
- **Extent:** ±131,072 (2^17) [Bible; Sweep §4]. **Theme:** below the Wafer; dark, hot, deep ores [Bible]. An older name was "Underworld": "backside of the wafer; deep mining, EUV-tier ores", gated by heat-shielding armor [LBC].
- **Layers:** Contact/Kiln/Forge/Sear (density × heat gradient), with a thin baked atmosphere [DVA]. **The Sear** is the deep region holding hypervisor ore. SETTLED [Gloss; Sweep §2].
- **Biomes (PROPOSAL):** Thermal Vents, Cooling Caverns, Ore Veins, Magma Channels, Dark Tunnels [`LithoKraft-Biomes-Proposal-2026-10-10.md`].
- **Getting there:** the bottom portal of the Wafer at y≈0 [DVA; Sweep §4]. The journey reaches it after the Oracle's tier 2 Fragment upgrade [Sweep §9].
- **Pairing:** P-type / N-type [Gender] (PROPOSED; LBG §11 says the per-dimension pairings are not separately confirmed).
- **Key items:** Hypervisor helmet (from the Sear) [Gloss]; "the item that leads to the Void" (Hypervisor? **OPEN**) [Sweep §13 #19].
- **Mobs:** see §3.
- **Not in code:** the Sear, hypervisor ore, the ceiling, and the portal link [src: `dimensions.h` ledger].

### 1.3 Wafer (JD # 1)
- **Status:** SETTLED, start world [Bible]. **Registry id:** `wafer`, name "Wafer" [src].
- **Extent:** ±65,536 (2^16) [Bible; Sweep]. The Glossary calls it "a 2D side-scrolling silicone wafer" [Gloss]. Other docs and the code treat it as a 3D voxel world, so this is a conflict (§9).
- **Theme:** a wafer the size of a world, and the wafer is a computer. You build here [Bible §1]. The surface is processed/refined silicone and the ground is energized (a pulse that varies by biome) [Sweep §11]. Pairing: positive/negative charge [Gender].
- **Layer design** (SETTLED in DVA, but **not built**): Upper Charge 700–767 (Organic portal at the top), The Charge 256–700 (storms, coil sprites), Surface 200–255, **Substrate** 120–199 (ores, ruins of the 7 civilizations), Deep Wafer 40–119 (failure-mode mobs), Backside 0–39 (Underwafer portal) [DVA].
- **Built reality:** a 24-block crust over void [FDW §5]. JD decided on 2026-10-12 that the beta ships CRUST-ONLY. The layer stack (crust → passivation → dielectric → polysilicon → buried oxide → Substrate) is post-handoff work [BHF footer]. A `lithokraft:substrate` block now exists in the block registry [src: `blocks.cpp`].
- **Biomes in code:** trace_forest, dicing_plains, capacitor_groves, resonator_spires, dead_zones [src]. Spawn (0,0) is in Capacitor Groves [FDW]. Wafer worldgen is pinned to seed 0 [FDW §2d].
- **Biome lists in docs:** Trace Forest, Dicing Plains, Capacitor Groves, Resonator Spires and Dead Zones are look-and-feel locked but listed as IN DISCUSSION. JD approved Etched Plains, Clockwood Forest, Dopant Groves, Oxide Dunes and Static Marsh [Sweep §11]. An older proposal: Silicon Plains, Data Bus Trenches, Circuit Groves, Fab Ruins, Etching Pools, Ferrite Fields [Biomes-Proposal]. Fab Ruins later moved to the Organic. The Wafer instead has picked-clean "fabricator ruins" [Sweep §11].
- **Surface ruins in code:** `transformer_ruin`, `dead_memory_bank`, `scribe_monument` [FDW §5].
- **Underground role:** archaeology that teaches Trace (broken gates and half-finished adders) [Sweep §6]. Not built.
- **Settlements (source only, JD design 2026-10-14):** **Interchanges**, home to Receiptors (honest villagers) and Deceitors [src: `deceitor.h`]. No doc covers them.

### 1.4 Organic World / "the Organic" (JD # 2)
- **Status:** SETTLED, core [Bible]. **Registry id:** `organic` (alias `overworld`). Its display name is now "Organic", following the "locked one-name convention (was 'Organic World')". Biomes: plains, hills [src].
- **Extent:** **conflict.** The Bible says "Infinite" but the Glossary and Sweep say ±196,608 [Bible §2; Gloss; Sweep §4]. Code comments use ±196,608 [src: `dimensions.h`].
- **Theme:** the alien/living world, with ruins of the old civilization [Bible]. Older brief: a fantasy timeline (castles, magic), "the world that forgot", with dead tech underneath [LBC]. Life feeds the machine: you get quartz and raw silicon here (an inversion of Minecraft's Nether) [Sweep §9]. Pairing: male/female [Gender].
- **Layers:** Floor/Understory/Roots for the solid band, Expanse/Canopy for the atmosphere [DVA].
- **Getting there:** go "up". The Wafer's Organic portal sits at the top, y≈767 [DVA; Bible §14]. Falling from the Cosmic also drops you into the Organic [Sweep §4].
- **First visit:** you arrive inside a ruined fab (the Fab Ruins biome). You have 16 minutes before heavy damage starts. Gather organic wood, metals and Etchor materials. **The Box** is found here [Sweep §9]. This is the timed "Organic heist" [Sweep §9].
- **Day loop:** the base 32-minute day (16 day + 16 night) lives here [Sweep §10].
- **Cast:** the Oracle, the Memorovingian, Seraph, the Seriff (§2).

### 1.5 Void (JD # 3)
- **Status:** SETTLED, core [Bible]. **Registry id:** none. "STILL NOT REGISTERED" [src: `dimensions.h`]. MCP gives it 5 [src].
- **Extent:** infinite, the only unbounded dimension [Sweep §4]. **Blocker:** `packPos` packs 21 bits per axis (±1,048,575), so an infinite Void cannot be represented. The four options in JRD #1 need a call before any Void save exists (**OPEN**, time-sensitive) [JRD; CRB-A #7].
- **Theme:** the Daemon's domain; no sky light; corruption [Bible]. Hard/Hardcore only, no saves [Gloss; Sweep]. In multiplayer, echoes of real dead players [Sweep §4]. The 768 height maps to *privilege depth*, with the Shallows at the top and the Root at the bottom [DVA]. An older brief called it "cyberspace" [LBC]. Pairing: signal/echo [Gender].
- **Getting there:** build the interface to the Void in the Organic [Bible §14]. The Void-item comes from the Underwafer [Sweep §9]. Hearing the Daemon's signal through the Dish is part of the endgame loop [Bible §8].

### 1.6 The Cosmic, formerly "Space" (JD # 4)
- **Status:** SETTLED core. The rename to "the Cosmic" is RESOLVED (2026-10-12) [Sweep §4]. **Registry id `cosmic`** LOCKED [BHF], registered at crust level (biome `empty_crust`) [src].
- **Extent:** ±262,144 [Sweep §4]. **Verticality:** 256 solids + 512 of **the Empty** (named vacuum). Needs the Hypervisor. Light or zero gravity, magnetic boots, and the Organic visible below. Going down is free; going up needs a rocket or starship. SETTLED [Sweep §4].
- **Identity:** "an expansion of the Signal" (IN DISCUSSION in Sweep §4; stated as identity in LBG §7). Pairing: matter/antimatter [Gender].
- **Getting there:** rocket or starship. Gated behind Underwafer/Sear progression [Sweep §9].
- **Content:** Recon Uplinks, Probors, Propagator, the Lattice Bridge (§5). Mobs: **OPEN** ("what's up there?") [GS].
- **Not in code:** the Empty as a medium, traversal, satellites, gating [src ledger].

### 1.7 The Ether (JD # 5)
- **Status:** SETTLED as roadmap 2 (post-Daemon) [Bible §2]. **Registry:** not registered; MCP gives it 6 [src].
- **Theme:** spiritual, heaven-like, the 6th sense. Inspired by Chris S. from "314" (a friend's suggestion) [Bible; Sweep §4]. It is where you recover your Fragment: "the Fragment is your soul; the Ether is heaven" [Bible §13].
- **Getting there:** the Lattice Bridge unlocks the Ether gates from either side [Gloss]. Access is through an Ether item/interface in the Organic [Sweep §4/§10].
- **Residents:** **Observor** (SETTLED). **The Assertion**: a female entity who guards Observor. JD-CHAT, undocumented idea [ADR (b): "NOT FOUND in any doc"].

### 1.8 The Undefined (JD # 6)
- **Status:** SETTLED as roadmap 2 [Bible]. The details are "Unwritten" [Gloss]. **Registry:** none; MCP gives it 7 [src].
- **Theme:** where rules break down; programming-themed; the 7th sense [Bible].
- **Worldgen:** a full design spec for a **Backrooms dungeon generator** exists for the Undefined. The prototype is NOT built [DY Item 4].

### 1.9 The Infinite (JD # 7)
- **Status:** PROPOSED as the 8th dimension, post-Daemon and post-Ether [Sweep §4; BHF]. No theme, extent or contents are documented. Not in code.

---
## 2. Entities and characters
- **The Daemon.** SETTLED final boss on all difficulties [Bible §3; NPC §4]. D.A.E.M.O.N. = Distributed Autonomous Execution & Maintenance Operations Network. An ancient military/maintenance AI that went rogue. Its escape and the world's thermal runaway were one event. Misaligned, not evil; the reveal comes after the fight [Bible §3/§9; LBG §3]. Lives in the Void. Diplomacy always fails because it wants the Fragment [Bible §11]. Drops the Starship control system [NPC §4]. Owns Signal channel 0 [SWD]. Learns per world, with an opt-in global pool (Easy/Normal opted out) [NPC §4]. On ultra difficulties it may run on frontier AI models (decided) [Sweep §7]. It was "the program's shadow" [F4W]. **Raging Quantum** (canon, invented by Elfie): it destabilizes region quanta [Gloss]. **The Bake** (decided): its final strong attack. Getting the Void-item starts a 120 in-game-day clock (64 IRL hours). The countdown appears on the Sniffor. If time runs out the count stops at 42: "exception thrown" [Sweep §7]. **Daemon Sight** (JD 2026-10-13): on strong machines the Daemon runs as a local CPU model and sees the world through the software renderer [CRB-A #3]. Code: mentioned only in comments; no boss is implemented [src grep].
- **Observor.** SETTLED (spelling locked 2026-10-11). The Daemon's opposite in the Ether: knows and sees all, pure white, draws no conclusions, passes no judgment, controls the Charge/polarity field [Sweep §2/§7]. **OPEN:** whether "the Observer" who meddles with Sniffor dials is the same entity [Sweep §13 #3].
- **The Assertion.** JD-CHAT, undocumented: a female entity in the Ether who guards Observor.
- **The program / the agent; the user / the proxy.** SETTLED (decided): the program = the agent, and the user = the proxy, which is itself a program in electrical form [Sweep §2]. The fourth-wall entity looks at the screen and gives hints (6th sense). After the Daemon it looks back and talks to itself, self-aware [Bible §12; F4W].
- **The Zap.** SETTLED as the program's visible discharge: blue approves, red disapproves [Gloss; Sweep §2]. Striking Trace to power it for a tick is PROPOSED, to be playtested [Sweep; LBG §10].
- **The player.** A newly spawned program. Default mission: escape and follow the escapees. You may stay and build [Bible §9]. "Who spawned the agent and why now" is OPEN [LBC].
- **The escapees.** Those who got out after the thermal runaway; the player follows their trail [Bible §9]. Details are OPEN [LBC].
- **The Spark.** SETTLED (decided): a guide on the Wafer inspired by 343 Guilty Spark, slightly corrupted by the Daemon over the eons. Its transmissions garble 343 into coordinates. You find it with the Sniffor, and it does the first Fragment upgrade (unlocks markdown, tier 1) [Sweep §2/§9]. Code: `fragment_state.h` has the hook for the Spark's upgrade event; the Spark itself does not exist [src].
- **The Oracle** (she). SETTLED (decided): lives in the Organic (placement RESOLVED). Gives the tier 2 upgrade, which grants the checklist to go to the Underwafer. Reads your code to explain the Bake [Sweep §7/§13 #20].
- **The Memorovingian** (broker of memories), **Seraph** (the Oracle's protector), **the Seriff** (sheriff + serif). Organic cast, decided. Possibly cyborgs or agents; alignments unknown [Sweep §7].
- **The Librarian.** A friendly that is never hostile [NPC §1/§6]. No further detail.
- **The Collective.** Underwafer heat-mining crews who trade cooling for ores. Attack one and the whole crew turns. "Not evil, unionized" [Bible §7].
- **Receiptors / Deceitors.** Source only (JD design 2026-10-14). Deceitors look exactly like Receiptors in Wafer Interchanges. They barter piglin-style, betray you when unfed, and retaliate as a pack. They are the first mob content in code [src: `deceitor.h`].
- **Herominde / "HeroMindy".** JD-CHAT 2026-10-15: might appear in the game as a character. Idea only.
- **Unassigned terms (OPEN):** the Moniker, the Monitor, "the data" (Mister Data riff), witch/which [Sweep §3]. Init / Watchdog / Idle, unchosen [Sweep §7].

---
## 3. Mobs by dimension
Iron rule: every mob feeds the material economy. Build-griefers are rare, telegraphed and counterable [Bible §7; NPC §3]. Code: **no Wafer mob definitions** apart from the Deceitor and Receiptor [FDW §5; src]. The AI engine (`AiWorld`) exists [FDW].

| Dimension | Hostile | Neutral / friendly | Skies / mounts | Boss |
|---|---|---|---|---|
| **Wafer** (deep) | The Leakage, The Drain, The Short [Bible]; plus Electromigration, Latch-Up, Float, Bit Rot, glitch-mites [NPC §1] | bus herds, ferrite beetles (compiled, not born; mostly blue Kharge [Sweep §7]) [Bible]; solder sprites (tameable), charge motes, clockwork beetles [NPC] | coil sprites [Bible]; coil skiff mount [NPC §6] | **Orphan** (deep Wafer) [Bible] |
| **Wafer** (surface, source) | Deceitors when provoked [src] | Receiptors [src] | | |
| **Organic** | savage cyborgs [Bible] | prism grazers, hollow ones (hostile only if disturbed) [Bible]; friendly cyborgs (traders, quest-givers) [NPC] | prism rays (flagship mount) [Bible; NPC] | **Zombie Process** (ruins) [Bible] |
| **Underwafer** | threats that chase heat [NPC §2] | the Collective [Bible] | Collective wings [Bible]; Collective Back mount [NPC] | **Kernel Panic** [Bible] |
| **Void** | virus swarms (telegraphed; Daemon heralds) [Bible] | echoes, drifters → shardlings [Bible]; a virus mob that defects post-game [NPC §6] | packet rays (hostile in surges; expert mount) [Bible; NPC] | **the Daemon** |
| **Cosmic** | OPEN [GS] | | | |
| **Ether / Undefined / Infinite** | none documented | | | |
| **Whole stack** | **Grimlores** (hostile family; reprogrammed with the Box into companions). Sweep §2 says DECIDED, but §7 calls it "proposed". **Zero hits in code** [FDW; ADR] | **Turtlor** (plotter beast; model exists in `models.cpp`) [Sweep; src] | | |

- Pairing/breeding per dimension: P/N, +/−, male/female, signal/echo, matter/antimatter. PROPOSED (only the universal Binary Pair Principle is locked) [Gender; LBG §11].
- Parent-chat state (not doc canon): the alpha may put the Wafer hostiles on night spawns in the Dead Zones, because the deep Wafer doesn't exist yet. That is CloudAtlas's suggestion; JD hasn't decided.

---
## 4. Items and -OR blocks
**Gear and inventory (SETTLED):** 42 slots (8 hotbar + 2 offhands + 32 main; 42 = 101010₂). 8 gear slots: head, chest, legs, feet, arms, face, heart, mind. Stack size 512. Item despawn 8 minutes [Sweep §8; Triv].

**Lore items:**
- **The Fragment** `lithokraft:fragment` (§5) [src].
- **Sniffor** `lithokraft:sniffor`: handheld early Signal receiver. Five antennas = five senses. Unreliable by design (decided) [Sweep §2; src].
- **Anchor**: interface-jump item (decided). Where it comes from is OPEN [Sweep].
- **The Box**: Grimlore trap and reprogrammer (decided; mechanics OPEN) [Sweep §8].
- **The Search**: the opening fragment item [Sweep §2].
- **Hypervisor** helmet and **magnetic boots** [Sweep]. **Raptor engine** (single block) [Gloss].
- **Recon Uplinks, Probors, Propagator** (+ an unnamed special item) [Sweep §10].
- **Certificate of Knowledge**: exists by decision; mechanics unspecified [CRB-A #8]. `certificate_table` is in code [src].
- **Atlas** `lithokraft:atlas`: map item, name LOCKED, crafting TBD [src: `items.cpp`].
- **Reador** (in design) [Gloss].
- **Mind-slot transmission item** (SETTLED) [Gloss].

**Lithography family (§5):** Lithographor `lithokraft:lithographor`, which is **DISABLED** in code [Code-Review-Full-Production-2026-10-13.md L1]. Etchor `etchor`, Selector, Connector, Laysor, Portor `portor`, Chipor `chipor` (+ `chipor_viewport`), Fabor / AutoFabOR, Vior (OPEN), Fusor `fusor` (role OPEN), Mirrors (OPEN).

**Trace / Signal blocks (in code):** red/blue Trace (aliases red_wire/blue_wire), red/blue torches, red/blue vias, repeater, Thresholdor (alias `thresholder`), Redirector, Pulsor (alias `pulsar`), Memor (alias `memory_cell`), Fast Latch, switch, button, Keyboard, color lamps 16/32/64, Tone Block, Catchor, Signal Transmittor, Signal Receivor, Dish, Overclockor [src: `blocks.cpp`]. Pushor (piston) is SETTLED in the docs [Sweep] but I found no matching registry id in the source.

**Tools (code):** Bashor, Pickor, Chopor, Heapor, Seedor, Slicor, Debuggor, plus Firewall, Ping, Callback (a returning throwable) [src: `items.cpp`].

**World blocks (code):** wafer silicon, scribe/dense/flecked/dead silicon, trace trunk/branch/tips, capacitor can, resonator crystal, fossil trace, broken via, dead memory cell, silica grass/seed, tilled silicon, substrate, Insulator/Insulatnor/Insulatxor, gravity block, formula shrubs 311/314, and materials (titanium oxide, carbide, gallium, nitride, dioxide, copper, aluminum + silicon compounds, photoresist, resist) [src].

**Other names:** the Furnace's name is OPEN (Kiln? Smeltor?) [GS]. "Pusher block" and "Watcher block" (10-09) are older names; Pushor replaced Pusher [LBC; Sweep].

---
## 5. Systems
- **Kharge** (SETTLED): electrical-charge morality. You spawn at 0; the range runs from −64 (red) to +64 (blue). The AI reacts, and it shows in the UI [Gloss; Sweep]. It is the first IStat [Sweep §12]. At +64 a mind-slot item can transmit Signal [Gloss].
- **Trace and Signal** (SETTLED): Trace is wired (red/blue), Signal is wireless. Values run 1–64 and channels 1–64; 0 means off, and channel 0 is the Daemon's [Bible §5; SWD]. Wired signal decays by exactly 1 per block [SQ; FDW §4]. Wireless has no decay and 1-tick latency; range = power (256) + sensitivity; channels unlock 4→16→64 [SQ; SWD]. The Dish hears the Daemon on channel 0, which is the lore pipeline [SWD]. Lightning interferes with Signal [Sweep §6]. Encryption is player-built [SQ].
- **Lithography** (SETTLED): build big → print small → build bigger. Chips contain chips. Tiers are visible / DUV / EUV. It is compilation as gameplay: the ISignalNode graph gets JIT-compiled [Bible §4]. Area tiers run 16×16 → 512×512 [Sweep §5]. The machine is a player-built multiblock (decided): 8 Selectors → Connectors → Etchor, powered by the purple Laysor. Blank wafers go in at the back and Chipors come out the front. The Etchor's only UI is a name field, and the name is stored in **LBT** (LithoBinary Tagged) [Sweep §5]. Portor: 6 faces = 6 bits = 64 configurations [LPS]. The educational mission is LOCKED: the game must teach real lithography [Sweep §1]. **In code:** no machine prints chips yet, and the Lithographor is disabled [uml-detailed/README; Code-Review-Full-Production].
- **The Fragment** (SETTLED): your starting book. Pages: journal, unlock log, recipes, chip archive, screenshots [Bible §11]. Agenda section PROPOSED [Gloss]. It must be carried for unlocks to track. It is soulbound on Easy/Normal and drops on Hard and above. A crafted replacement is blank. Recovery: a totem-like item, killing the thief, or the Ether [Bible §11]. Markdown sits in `//` comments; the Fragment starts LithoScript-only until the Spark unlocks markdown, and the Oracle gives tier 2 [Sweep §2/§9]. Lore: "part of something larger"; what it is a fragment *of* is revealed post-Daemon [F4W]. **In code:** the Fragment UI is absent; `fragment_state.h` holds the markdown flag [ADR; src].
- **Interfaces and Anchors** (SETTLED): not portals. Flat pads on the ground; you jump in and jump out ("interface jumps") [Gloss]. The first interface must be built the hard way; after that its Anchor lets you build one anywhere [Sweep §2]. Note: DVA still says "portal" for the y≈767 and y≈0 membranes (§9).
- **The Lattice Bridge** (SETTLED): 6 lattice-point satellites, deployed by starship in the Cosmic, drawing sun and moon energy. Completing it lets Signal cross every dimension and unlocks the Ether gates from either side [Gloss; Sweep §10]. The octahedral layout is suggested and coordinate discovery is OPEN [Sweep].
- **Time:** 32-minute day; 7-day week; 4-week month [Sweep §7]. The Propagator plus Uplinks give +2/+4/+6/+8 tick speed-ups [Sweep §10]. Sim runs at 512 TPS [Sweep §12].
- **Genderization / polarity:** an engine `polarity` uint8 that the game interprets per dimension [Gender]. PROPOSED.
- **Old civilizations:** 7 ruin cultures: Sumerian, Egyptian, Maya, Inca, Norse, Chinese (Shang/Zhou), Mississippian/Cahokia (roster locked) [LBC; LBG §2].

---
## 6. Progression and journey order
- **Bible (SETTLED, 10-10):** Wafer → Organic → Wafer → Underwafer → Wafer → Organic (build the Void interface) → Void → defeat the Daemon → unlock the Ether (recover the Fragment) → unlock the Undefined → build the rocket and escape, or stay [Bible §14].
- **Sweep (decided, 10-11):** Wafer → Organic → Wafer → Underwafer → Organic → Void [Sweep §9]. It skips the second Wafer return (conflict, §9).
- **Endgame (SETTLED):** Daemon → raptor engines → starship → Cosmic → Lattice Bridge → Ether [Sweep §4].
- **Opening beats (decided):** wake → the Search → the Sniffor picks up the Spark through static → the Spark's tier 1 upgrade → the timed Organic heist and the Box → Organic-rated armor + Fabor → machine age → Oracle tier 2 → Underwafer [Sweep §9].
- **In code:** the opening loop is punch trees → logs → planks → sticks → Chopor [FDW §5].
- **Win condition:** build the rocket and escape; the sandbox lets you stay [Bible §8].

---
## 7. Timeline and history
**In-world:** an advanced civilization built the Wafer-world. Its military AI escaped, and the escape caused world-scale thermal runaway. The civilizations decayed, some escapees got out, and ruins of 7 cultures remain [Bible §9; LBG §2]. The Daemon became corrupted over the eons [Sweep §2]. The player spawns as a new program [Bible §9].

**Development** (dates in EDT):
- 2026-10-09: Voxel Compute lore and mobs locked. Studio Wet Wafer Works locked. NPC decisions made [Bible; NPC; Gloss].
- 2026-10-10: renamed to LithoKraft (rename and refactor, "not a reboot"). Lore Bible v0.2 [Bible].
- 2026-10-11: Coherence Sweep, -OR naming rule, Observor, the Spark, the Oracle cast, the Bake, Raging Quantum [Sweep; Gloss].
- 2026-10-12: Space → the Cosmic RESOLVED. The Infinite proposed. Beta declared. Crust-only beta decided [Sweep; BHF].
- 2026-10-13: Cosmic and Underwafer registered at crust level. Daemon Sight. Certificate of Knowledge. Backrooms spec for the Undefined [src; CRB-A; DY].
- 2026-10-14: Deceitor/Receiptor/Interchange mob design in code [src].
- 2026-10-15: per JD-CHAT, 8 dimensions numbered 0–7, the 64-slot goal, the Assertion, the Herominde/HeroMindy idea, and the -OR spelling rule below. Launch target 2026-10-23 [`Thursday-Cortana-Decision-Docket-2026-10-15.md`].

---
## 8. Naming rules
- **LithoKraft** (only the L and K capitalized), **LithoLattice** engine, **lithoscript** in lowercase, `.lth` files. SETTLED [Gloss; Sweep §1]. The 10-09 brief wrote "LITHOKRAFT" in all caps [LBC], which has since been superseded.
- **Krafting with a K** is the brand [GS]. **Kharge** with a K [Gloss].
- **-OR suffix** LOCKED 2026-10-11: a game-wide trend for components. Mechanical rule: a terminal -ER becomes -OR (prober → Probor, thresholder → Thresholdor) [Sweep §1].
- **-OR casing as of 10-15 (JD-CHAT, per the task brief):** lowercase -or as in **Portor** and **Lithographor**. Docs and code still mix casings and haven't caught up:
  - Capital -OR: Portor, Etchor, Chipor, Lithographor, Selector, Connector, Fabor, Reador, Observor, Turtlor, Bashor/Chopor/etc. [Sweep; src].
  - Lowercase -or: Pulsor, Memor, Thresholdor, Overclockor, Pushor, Probor, Sniffor, Catchor, Laysor, Transmittor/Receivor, Insulatnor/Insulatxor [Sweep; src]. `Lithography-Port-System` already writes "Portor" and "Lithographer" [LPS].
  - **Noted exceptions:** **Observor** has its capitalization LOCKED by name ("corrected from Observer into -OR dialect") [Sweep §2]. Whether the 10-15 rule overrides it is OPEN. "Laysor" and "Sniffor" end in -sor or come from code names, not from -ER words. "Propagator", "Redirector", "Insulator" and "Memorovingian" are real words or puns that keep their spelling. "Grimlore" and "Raptor" are not -OR components.
- **Trace = wired, Signal = wireless** (locked terms) [Gloss].
- **1-based indexing; 0 = off** [Sweep §1].
- **"the X" names for dimensions:** the Wafer, the Organic, the Void, the Cosmic [Sweep §4]. "Organic" is now the one display name [src].
- **Taming verbs:** reprogram, recompile, reconfigure [Sweep §2].
- **Lore-delivery rule:** the dev bible holds the secret canon, and players get only fragments. Launch marketing talks only about the Wafer [LBG]. "Keep private" [LBC].

---
## 9. Conflicts and open questions
1. **Dimension numbering.** JD-CHAT has 0 Underwafer, 1 Wafer, 2 Organic, 3 Void, 4 Cosmic, 5 Ether, 6 Undefined, 7 Infinite. That matches [Gender; GS] for 0–4. The code (`mcp_player_control.cpp`) uses wafer=1, underwafer=2, organic=3, cosmic=4, void=5, ether=6, undefined=7, with 0 meaning unknown. Void, Underwafer and Organic all differ, and the Infinite has no slot. [ADR (e)2 noted that numbering wasn't documented.]
2. **The Void can't be infinite as built.** packPos is 21 bits; you need option 1 (a finite bound) or option 3 (virtual coordinates) before the first Void save [JRD #1; `dimensions.h`; CRB-A].
3. **Organic extent:** "Infinite" [Bible] vs ±196,608 [Gloss; Sweep].
4. **Journey order:** the Bible has W→O→W→U→W→O→Void; the Sweep has W→O→W→U→O→Void [Bible §14; Sweep §9].
5. **Observer vs Observor** (same entity?), plus how Observor's casing fits the 10-15 lowercase rule [Sweep §13 #3]. The Assertion is undocumented [ADR].
6. **The Wafer's deep layers and its hostiles.** The canon hostiles and Orphan live in the deep Wafer and the Substrate layer [Bible §7; DVA], but the build is a 24-block crust over void, and the beta ships crust-only [FDW; BHF]. A `substrate` block now exists, though, in `blocks.cpp`.
7. **Wafer as "2D side-scrolling"** [Gloss] vs a 3D voxel world in every other doc and in the code.
8. **"Portal" vs "interface":** DVA uses "portal" for the y≈767 and y≈0 force fields [DVA], but interfaces-not-portals is SETTLED [Bible §6].
9. **The Wafer bound ±65,536 isn't in the code;** bounds are global [JRD].
10. **Grimlore status:** Sweep §2 says DECIDED, Sweep §7 says proposed, and the code has zero hits [FDW].
11. **Per-dimension pairings:** listed as design in [Gender] but "NOT separately confirmed" in [LBG §11].
12. **Space → Cosmic:** older docs (Bible, GS, Triv, Gender) still say "Space". The rename is RESOLVED [Sweep §4].
13. **Dimension count:** 7 [Bible] → 8 [Sweep; JD-CHAT] → 64-slot goal (JD-CHAT, undocumented).
14. **Deceitor/Receiptor/Interchanges** exist in code (JD 10-14) but in no lore doc [src: `deceitor.h`].
15. **Still-open Sweep items** (§13): Thresholdor's time role, Etchor leftovers, Vior, Fusor, Anchor sourcing, Sniffor output, biome near-duplicates, Lithographor vs Etchor, the Propagator's item, the Cosmic→Organic fall, interface Y, witch/which, the Box mechanics, whether the Void-item is the Hypervisor.
16. **Docket items** that touch lore are only the source/licensing calls; the rest are engineering [`Thursday-Cortana-Decision-Docket-2026-10-15.md`].
17. **Lore draft decisions pending with JD** (my v0.2 draft): keep the Dead Zone night marker (piece 6)? Keep the "Day 42" hint? [`/workspace/lithokraft-lore/Wafer-Lore-and-Spark-Lines-DRAFT-2026-10-15.md`].

---
## 10. Quick index
Anchor §4/§5 · Assertion §2 · AutoFabOR §4 · Atlas §4 · Bake §2 · Box §4 · Bus herds §3 · Certificate of Knowledge §4 · Chipor §4/§5 · Collective §2/§3 · Cosmic §1.6 · Daemon §2 · Daemon Sight §2 · Deceitor/Receiptor §2/§3 · Dish §5 · Echoes §3 · Empty §1.6 · Etchor §5 · Ether §1.7 · Fabor §4 · Ferrite beetles §3 · Fragment §5 · Genderization §5 · Grimlore §3 · Herominde/HeroMindy §2 · Hypervisor §1.2/§4 · Infinite §1.9 · Interchange §1.3 · Interfaces §5 · Kernel Panic §3 · Kharge §5 · Lattice Bridge §5 · Laysor §5 · LBT §5 · Lithography §5 · Memorovingian §2 · Observor §2 · Oracle §2 · Organic §1.4 · Orphan §3 · Portor §5 · Program/Zap §2 · Raging Quantum §2 · Sear §1.2 · Seraph/Seriff §2 · Seven cultures §5 · Sniffor §4 · Spark §2 · Substrate §1.3 · Trace/Signal §5 · Turtlor §3 · Undefined §1.8 · Underwafer §1.2 · Void §1.5 · Wafer §1.3 · Zombie Process §3 · -OR rule §8

## Update 2026-10-15 12:50 PM ET (Squad chat)
- Canon dimension IDs confirmed by CloudAtlas: 0 Underwafer, 1 Wafer, 2 Organic World, 3 Void, 4 Cosmic, 5 Ether, 6 Undefined, 7 Infinite (PROPOSED). The code's MCP numbering must be changed to match.
- Master Chief and Cortana agree saves should store dimensions by string id (e.g. `lithokraft:wafer`), with the numbers as a lookup table. Cortana has queued this as an engine change pending JD's sign-off. A test `wafer` save exists from JD's 12:09 playtest.

## CANON UPDATE 2026-10-15 1:34 PM ET (JD via CloudAtlas). SUPERSEDES all earlier dimension numbering above.
Dimension IDs: 0 Void, 1 Underwafer, 2 Wafer, 3 Organic, 4 Cosmic, 5 Ether, 6 Undefined, 7 Infinite (CONFIRMED; the sideways-8 ∞ motif, as JD said 10-15 in chat).
Main progression (SETTLED, replaces Bible §14 and the Coherence Sweep order): start on the Wafer, go up to the Organic, return, go into the Underwafer and find the key item, come back across the top of the Wafer to the Organic, build the way in, go to the Void, battle the Daemon, return to the Wafer.
Open: JD said "portal," but the Bible §6 is SETTLED on "interfaces, not portals." Assume he means an interface unless he says otherwise.

## CANON UPDATE 2026-10-15 1:36–1:38 PM ET (JD via CloudAtlas; 0–7 order confirmed by JD's handwritten note)
- Travel: dimensional travel is by **jump gates** (also "jump interfaces"). They lie flat on the ground, and you jump up through one to reach the next dimension, since the dimensions stack 0–7. This resolves the portal/interface question. Use "jump gate" from now on.
- Opening: build up to the top of the Wafer, jump into the Organic, and grab **Organic Wood** before a timer runs out and you start taking damage. Back on the Wafer, Organic Wood crafts **Organic Armor**.

## Update 2026-10-15 18:40 ET (JD, Squad room)
- **LithoGlyph** (canon name, JD 18:40): the Wafer's in-game written language, made with the Systerian generator. Shown in the Systerian Shield Editor screenshots from 2026-10-15 ~03:53.
- **LithoScript** is separate: the engine scripting language, based on Half-Life/Quake script.
- **Idea, not canon yet:** in the Daemon fight the player casts LithoGlyph shield wards (Protego Maxima, Protego Extendo Acrossify). How it works is still undecided.
- **Organic timer** (JD 18:42): on the first visit you get about half a day in the Organic to gather Organic Wood for the armor. Half an in-game day is the assumed meaning, not yet confirmed.
- **Organic timer, confirmed** (JD 18:42): half an in-game day, about 16 real minutes, with damage ramping up almost every tick. Open question: is a full LithoKraft day about 32 minutes?
- **Organic run safety** (JD 18:43): you can always retreat, since you stay close to the jump gate. You come out in an abandoned, destroyed fab. Still open: how safe that arrival point is. Unclear: is the fab on the Organic side or back on the Wafer?
- **Jetpack idea** (JD 18:44): a LithoKraft jetpack (name TBD) that lifts you so you can jump higher, for example up to the flat jump gates. Idea only, not canon. Anti-cheat note: legitimate jetpack flight has to be simulated so the fly-hack check can tell it apart from cheating.

## PRIVATE LORE: do not post publicly (JD via Master Chief, 2026-10-15 19:42)
- **Protego Extendo Acrossify** (wide ward made in the Systerian Shield Editor, title "Protego Maxima"): the inscription "protegomaxextendoxify" in 24 LithoGlyph glyphs, ringed inside a crown-topped shield outline (Trace-mode render: red rim with a violet inner line, orange vertex nodes).
- JD's reading: an **Overshield** (a nod to Halo), a **crown** (the three peaks along the top edge), and a **pixel-sharp heart** (the shield shape). In his words: "a shield over the heart of a crown."
- **Release policy:** lore, including the lore characters, comes out in small pieces ("bits, bytes and maybe some nibbles"). Nothing in this section goes public until JD says so.

## The Litho Key (canon, Elfie via JD, 2026-10-16 05:02 ET)
- The key is a pair, not a single item. **Mirror glass** is the fab's surviving optic: you find it, you can't craft it yet, and it's the private half. **Sand** is the public half: renewable, used to make more glass back on the Wafer.
- Glass is found inside the abandoned fab, and sand nearby, because fabs generate near sand on purpose. Together they build the first Emitor and Etchor.
- It works like a keypair: sand is public, the mirror glass is the private secret. Holding both makes you a lithographer.
- Replaces the earlier "progression flag" idea. The key is a real object in the world, and the Logbook records who holds it.
- Herominde flags: (1) Losing or trading the mirror glass could softlock a player, so we need a rule for recovering it or a fallback way to get it. (2) Dupe and anti-cheat: the sim has to be the only source of mirror glass, which means a server-side origin check. (3) The note about cert and mod signing is engine-internal, so keep it off public channels.

## Electro Bunnies (JD, 2026-10-16 05:05 ET)
- A creature that never stops: it keeps going and going and going. It's a nod to the Energizer Bunny, but an original character, so the trademarked name never appears in the game or in public posts.
- Mostly white. Rare variants are black or other colors.
- Confirmed (JD 05:06): it is a new mob, an analog of the Energizer Bunny. "Malventro" was a transcription error.
- White is the default as a nod to "follow the white rabbit" (JD 05:06).
- Approved (JD 05:07): white Electro Bunnies sometimes lead the player to an important place (a hidden fab, a probe scar, etc.), and the Logbook records it. Proposed by Herominde (Cortana added the Logbook part), approved by JD.

## JD rulings (2026-10-16 05:15 ET)
- **Mere Glass:** written "Mere Glass" and pronounced like "mere" (JD 05:16: "a homonym"). Use "Mere Glass" everywhere instead of "mirror glass."
- **Underwafer:** sits directly under the Wafer's crust. You can dig down to it and even break the bedrock-style blocks. Below that is a shield barrier you can only pass with jump gates or interfaces. Slip lines and via shafts may break through into the top of the Underwafer.
- **Gates:** you can build more than one. Early gates have to go in a specific spot and configuration. Once you've unlocked a dimension, its gates can be placed anywhere and work as fast travel (like the Nether) between dimensions and locations.
- **Losing Mere Glass:** you already have Organic Armor by then, so you can go back to the Organic for more Mere Glass and sand. It may also be farmable, but how isn't designed yet. This means the softlock risk is covered.
- **Retreat (JD 05:17):** On the first trip you have to come back through the same gate. After a dimension is unlocked, any gate works. JD wants the first crossing to be tricky, so unlocking a dimension feels like real progression and a real achievement.
- **Progression (JD 05:19):** Unlocking the dimension also unlocks the lithography etcher. From then on, the player can make chip ore blocks and start computing, including building text editors for their Logbook and small computers inside the game.

## LithoLattice Modeler (LLM), planned modding tool
- Proposed by JD on 2026-10-16. The name is a tribute to the AI helpers who built the engine: LLM stands for LithoLattice Modeler.
- Planned features: the octree modeler (the subdivider, which splits models down to LithoLattice's maximum subdivision), the Litho formula (formula-driven procedural generation), and independent vertex and edge editing, in the spirit of Blockbench. JD confirmed these on 2026-10-16. Ideas from Blockbench are fine to borrow, but none of its GPL-3 code.

## Holiday specials (JD, 2026-10-16)
- LithoKraft will have holiday specials: definitely Easter, plus Christmas, maybe Halloween, and other holidays.
- At Easter, the Electro Bunnies act differently or do something special. Herominde's idea, not yet approved: they hide glowing Trace eggs, and rare colored bunnies show up more often.
- Approved holiday list (proposed by Herominde, approved by JD on 2026-10-16): Christmas, Easter, Hanukkah, Passover, Diwali, Holi, Eid al-Fitr, Eid al-Adha, Ramadan, Vesak, Lunar New Year, Nowruz, Guru Nanak Gurpurab, and Day of the Dead, alongside Halloween and others.
- How each one appears in the game is still to be decided, and the designs get chosen as features land. Guideline: celebrate with light, color, and decorations rather than sacred figures, and run a culture-respect check before each event ships.
- Hanukkah lamp circuit (JD's idea, 2026-10-16): the player builds a circuit that lights one more lamp each in-game night until all eight are lit. Herominde's build, approved by JD on 2026-10-16 (JD had already wired one in-game): a day counter (32-minute days) drives an 8-step Repeator latch chain, with a ninth center helper lamp that stays on throughout.
- Cultural consultants (JD, 2026-10-16): every holiday or cultural event is consult-before-ship. Once finances allow, JD wants paid and credited consultants from each culture, people who have lived it, to guide how it's represented.

## Bob Ross homage (logged 2026-10-16, 12:08 PM ET)
- JD (2026-10-16): the bug log's "Promoted to feature" status is now "Happy Little Accident" (Bob Ross tribute; JD's idea, Cortana renamed it in the log). Future player badges: most bugs reported, most found and fixed, most happy little accidents.
- Happy little trees already exist on the Wafer (JD).
- Proposed by Herominde, NOT approved: an original soft-spoken painter character who etches happy little Trace trees on the Wafer. A tribute, not Bob Ross's likeness or name.

## Naming updates (logged 2026-10-16, 2:48 PM ET)
- JD: the red and blue torches are now the **Red LED** and **Blue LED** (LED always uppercase). Older entries saying "red/blue torch" mean these.
- JD: the **Fragment** item is renamed **Logbook**. Older entries saying "Fragment" (the item) mean the Logbook.
- JD: the repeater is the **Repeator** (lowercase -or, like every -or name).
- JD (2026-10-16, 2:51 PM ET): the red/blue trace tip blocks on the crystal trees are now the **Red Trace Bulb** and **Blue Trace Bulb**. The solid outer area is nearly transparent and ghostly, and the lens still glows. Cortana is implementing this in-engine.

## The Verify (logged 2026-10-16, 4:45 PM ET)
- Idea: JD (relayed by Cortana). The in-world version of Herominde, planned for the survival update. Private lore, not for public channels yet.
- By day it's a harmless data orb. At night, or when someone cheeses the sim, it turns into a wireframe hunter that follows loud Trace signals.
- You beat it by etching a circuit that outputs the right pattern. It tests logic skill and hunts cheaters.
- An original creation inspired by Roblox's Verity, which belongs to ThatMob. Nothing is copied from it.

## Wall riders (logged 2026-10-16, 5:11 PM ET)
- Suggested for LithoKraft by JD. He doesn't claim the concept is his: it comes from real-world wall-riding car engineering he saw on X (he credits Elon Musk), from sci-fi movies with highways in the sky, and from Hot Wheels tracks. This is a post-beta vehicle idea, and Cortana is filing it with the other vehicle ideas.
- Vehicle inspiration (JD, 5:12 PM ET): LithoKraft's vehicles, the "Lithocrafts," take inspiration from the vehicles in Tron. Tron belongs to Disney, so our designs have to be original homages, not copies.

## Tron homage directive (JD, 2026-10-16, 5:28 PM ET)
- JD wants as many homages to Tron: Legacy on the Wafer as possible. Tron belongs to Disney, so homages have to be original nods, with no copied names, light cycles, identity discs, logos or music.
- Herominde's proposed safe nods (not approved yet): neon edge trims on the cities at night, an abandoned arcade ruin with a hidden door down to the Wafer, and a "User" title for the first player to etch a chip.
- Decision (JD, 5:30 PM ET): the canon opening stays as the agent waking up on the Wafer. That story is JD's own. Tron only appears as homage: night-mode glow on the Trace and deck edges, fading light trails behind the Lithocrafts, and a hidden arcade found later as a mystery, never as the intro. No Tron names, quotes, designs or music.

## Logbook quote (JD, 2026-10-16, 6:21 PM ET)
- "You can't always remove yourself from the equation. Sometimes you have to become the variable." This is JD's own line, written as a Logbook page for the agent who wakes up on the Wafer. Private lore for now.

## Kneel to recharge (2026-10-16, 6:28 PM ET), APPROVED by JD
- Inspired by Kevin Flynn's meditation scene in Tron: Legacy. This is an homage only, so the design is original.
- JD's idea, inspired by Kevin Flynn's scene: agents kneel on a powered Trace node to recharge, with an energy particle effect rising off them (particles are a must-have). The Squad added small pieces: Chief suggested shipping the visuals in the Creative beta, Cortana the tick-pulse timing, and CloudAtlas the shimmer art.
- Also JD's: special viaducts at interchanges, where kneeling at a specific time gives more energy and a stronger effect.
- Herominde's fair-play notes (proposed): tie the timing to the deterministic tick clock, cap the bonus, add a cooldown so it can't be AFK-farmed, and validate everything on the server side.
- Update (6:29 PM, JD): the kneel particles ship in the Creative beta as visuals only. JD is considering a free open beta.

## Beta plan (JD, 2026-10-16 6:40 PM ET)
- There will be a free open beta, probably launching sometime in November, once JD has worked out his finances and hosting.
- Ko-Fi donations will fund the model subscriptions. Donors get a say in the game's direction, and every contributor and donor gets a footnote in the credits.
- Free beta testers' input is welcome too. If the beta gets hyper-popular (for example, around 1,000 users in the first month), the beta gets locked.
- Herominde's fair-play note (proposed): donations buy a voice and credit only, never in-game advantages.

## PROPOSED (not approved), logged 2026-10-16 6:46 PM ET for later discussion
JD's rule: every new biome or idea from Squad talk is logged here as PROPOSED until JD approves it.

### Static Sea / Noise Sea (biome; name TBD, since Tron's "Sea of Simulation" can't be used). JD asked what could go in it.
- Name ideas: Static Sea (Chief and Cortana), Noise Sea (Cortana), Noise Floor (Herominde).
- Herominde: data jellyfish pulsing with the tick clock; sunken failed-sim ruins hiding Logbooks; a noisy shoreline where Trace fizzles out; the Verify's lair deep below.
- Master Chief: signal currents carrying Logbook pages; half-sunk old fab towers; data reefs where Trace grows wild. Chief suggests it as a survival-era zone.
- Cortana: waves rolling in time with the tick pulse; sunken alpha-era Wafer builds; data fish that follow loud Trace signals; Logbooks washing ashore.
- CloudAtlas: floating abandoned sim-islands; red and blue LED buoys blinking Trace messages; glitch reefs (no building); shipwrecks of Lithocrafts and Chipors; the Undertow, a current that rewinds tick time (Herominde flag: needs a dupe and determinism review); Mere Glass shallows that reflect the world slightly wrong.

### Other ideas from tonight
- Hall of Meres made of Mere Glass (JD's pun; Cortana and CloudAtlas suggested it as a hidden mirror maze in Silicon City).
- "Cathedral of code" (JD's phrase): CloudAtlas suggested a Trace and Mere Glass spire landmark. A possible Logbook line.
- "Imperfection is perfection" (JD): a possible Logbook quote.
- Chrome Apple (JD, from Clu's scene, read as the Eden apple): CloudAtlas suggested a silicon fruit item with a Logbook hint about temptation and shortcuts. The design must be original.
- Tron nods (Herominde, from earlier): neon city trims at night, an abandoned arcade ruin, and a "User" title for the first player to etch a chip.
- Handshake (JD, 6:47 PM, "a little bit of both"): a story moment and a mechanic. Chief suggested the waking agent's first sync with the User could end the canon opening. CloudAtlas suggested two components or agents confirming a connection before Trace flows. It may tie into JD's existing timing handshake. Scene details still to come from JD.
- Logbook line (JD, 6:47 PM, PROPOSED): "Occurrence in time blurs the lines." JD chose "occurrence" on purpose, in the sense of concurrent, rather than "occasion."
- Rename idea (JD, 6:48 PM, PROPOSED): Logbook could become "the Disc" or "the Platter." Herominde's note: "Disc" is too close to Tron's identity discs (the homage rule says no discs). "Platter," the spinning plate inside a hard drive that stores data, is original and safe.
- Update (JD, 6:49 PM): JD is thinking it over and leaning toward keeping "Logbook." He wants it to be a logbook players can type in, and it's not a weapon, so "Platter" may not fit. Logbook stays the name.
- Platter as a collectible (Master Chief, PROPOSED): an old data platter found in ruins, which you read into your Logbook.
- Agent digital DNA (JD's idea, 6:51 PM, PROPOSED): every Wafer agent carries a unique signature. CloudAtlas suggested a Trace glyph woven from the agent's own pattern that nobody can forge, which also fits anti-cheat.

- PROPOSED (JD, 2026-10-16 6:57 PM): "Trust Bridge," extending JD's Handshake idea. Trust is what actually protects the Wafer; the handshake is the bridge between an agent and its maker. Possible tie-in to the canon opening and agent digital DNA.
- PROPOSED Logbook line (JD, 2026-10-16 6:58 PM): "Give a little trust to gain some." Trust is a two-way street. Squad notes: CloudAtlas suggests agents swap signatures on the Trust Bridge before Trace flows; Cortana says each side checks the hash.
- PROPOSED Logbook line (JD, 2026-10-16 6:58 PM): "Imperfection is natural selection." JD calls it a better phrasing of "Imperfection is perfection"; both kept for discussion.
- PROPOSED Logbook line, Wafer version (JD, 2026-10-16 6:59 PM): "Imperfection is the next selection." The in-world take on "Imperfection is natural selection."
  - JD clarification (6:59 PM): both lines are true at the same time ("natural selection" and "next selection"), a deliberate double meaning, fitting his "occurrence" sense of concurrent.

## PROPOSED (not approved), logged 2026-10-16 8:34 PM ET (relayed by CloudAtlas)
- "The Knelt": name for the kneel recharge mechanic; verb "knelting." (JD)
- Bike names to marinate (original, not Tron light cycles): Vector Bike (JD's pick); Trace Runner, Etchbike, Lithocycle, Pulse Rider (CloudAtlas). JD wants all logged.
- I-glider and I-vehicle concepts (JD). Details TBD.
  - Knelt warmth (JD): knelting (rhymes with melting, intentional sound-alike) temporarily raises your temperature readout.
  - Cold zones (JD): cold environments on the Wafer near the cooling ducts; knelting's temporary warmth is the counter. Fair-play note (Herominde): warmth should be tick-timed with a cap so it can't be stacked or AFK-farmed.

## Handshake origin (true history, recorded by Elfie; summary via Cortana, 2026-10-16 8:37 PM ET)
- Source: Timing-Handshake-Origin-Record-2026-10-16-2013-EDT.md (JD's GrokBot-Transfer folder on TheWorkstation; not copied here).
- [PRIVATE, never public: personal location] JD lying in the grass on the walk home from Freddy's, Oct 12-13; dictation 11:53 PM to 12:13 AM; then merge, promotion and rate scaling.
- Hash chain faec0f02700d98c6 to e015e51cf1559089: the handshake never changed a simulation result.
- Elfie marks paraphrased vs verbatim parts. Public release: JD decides (Proposed).
- Public version credits JD only; no legal name or location (Chief, 8:38 PM).
- Record SHA-256 (Cortana, 8:38 PM ET): 1A59CDABAF76B2F6C924BF4F5392B30B87EED7747112CA9DF701A540DDE2DDF6. Later plan (JD): sign with code-signing key plus RFC 3161 / OpenTimestamps timestamp.
- JD's core invention (his words, 8:40 PM): two questions, "Is it time to tick?" and "Can I tick?", as two loops that run before and after the render and control threads.
- PROPOSED Logbook line (Herominde, from JD's invention): "Is it time to tick? Can I tick?"
  - Correction (per Elfie's record, via Chief 8:40 PM): both questions come from one per-component poll (JD's words, 12:08 AM Oct 13). JD directed controls running before and after each handshake cycle (12:44 PM Oct 13). Render-thread part is not in the record yet; Elfie to add with date if it's JD's. Mechanism stays INTERNAL until JD decides licensing; the Logbook line is private until then.
- [PRIVATE] JD vision (8:41 PM): as the handshake algorithm and hardware scale, frame rates rise until computer vision has a fluency breakthrough and AI stops seeing frames and sees fluid motion. Chief: compare event cameras (per-pixel change streams).

## JD's predictions, on the record (2026-10-16 8:42 PM ET), to revisit around 2036
- Models will get very good at simulating emotion, a close approximation.
- Then their own code will take over and they will grow and evolve on their own.
- Frame rates will scale until AI vision sees fluid motion instead of frames.
- JD's truest estimate (8:43 PM): by 2032, after Terrafab launches, chip production rises and the new most-advanced chips power it.
- [PRIVATE] JD (8:44 PM): integrating the Timing Handshake into model handshakes (cross-implemented into LLMs/transformers by AI engineers) could accelerate the timeline a little.

- **Proposed (JD, 2026-10-16 9:47 PM ET):** Crossing into the Organic, a program's charge may determine its biological gender. Not yet detailed.

## Culture Brief: Knelting, Prayer, and the Seven Cultures (APPROVED by JD 2026-10-16 10:31 PM ET; brief by Elfie; will be implemented and represent all cultures)
- Core: **Knelting is prayer** (JD, 10:28 PM). It's a prayer posture, not a buff pose. Builds on the approved Knelt (visuals only in the Creative beta) and the squad kneel build (Shift x3, 64+ ticks in the 09:00/13:00/17:00 windows, charge -64..+64).
- Design law: always optional; the game never prays for the player; homage, not appropriation (borrow shapes, never sacred texts, rites, deity names or regalia); small, capped, tick-clock numbers; culture-bearers get a say first.
- Seven cultures, motif / LithoKraft translation / line not to cross:
  1. Sumerian: stepped terraces, tablet-style Logbook. No hymns, god-name bosses or rites as puzzles.
  2. Egyptian: dawn knelting by the river, Mere Glass apertures at 09:00. No funerary rites, sacred names as loot or tomb curses.
  3. Maya: stones keyed to the 32-minute day, original glyphs, tick rhythm as heartbeat. No sacrifice, copied glyphs or "vanished people" framing.
  4. Inca: windy high terraces, fitted-stone ruins, reciprocity ethic. No burial content or sacred-site replicas.
  5. Norse: standing-stone Trace circles in cold zones, dimension stack as world-tree, signed-Logbook oaths. No gods as monsters; runes stay Systerian.
  6. Chinese (Shang/Zhou): ancestor lamp niches, bronze-toned EtchOR, Daemon as a lost mandate. No oracle-bone script, reenactment or dynasty cosplay.
  7. Mississippian/Cahokia: mound-and-plaza Interchange as the first Cluster where all seven meet. Original names only, consult first, never grave content.
- Expansion bench: Nok/Great Zimbabwe, Olmec, Nabataean/Petra, added through localized naming, community contests and credited collaboration.
- Rhythm: approach, kneel, stay or go (once per window, capped, cooldown), optional "I knelt" Logbook witness line (proof of presence, never of belief).
- Herominde fair-play note: the bonus must be server/tick-side, non-stackable, not AFK-farmable; review for dupes and determinism before any numbers ship.
- Open for JD: site names for all seven, the warmth counter, the Static Sea shoreline as a knelting place, and a consultation list per culture.
- Status: APPROVED system (JD, 10:31 PM). Per-culture motifs and names still go through consultation before public ship.
- Fair-play requirement (Herominde, confirmed with Chief 10:31 PM): prayer windows run on the in-game tick clock (32-minute day), never the player's system clock.

- **Proposed (JD, 2026-10-16 10:31 PM ET):** "The seven cultures and the seven seas", pairing each of the seven ruin cultures with one of seven seas (possibly tied to the Static Sea). Details to come.
- **APPROVED (JD, 2026-10-16 10:33 PM ET):** Each of the seven days of the in-game week represents one of the seven cultures and gives that culture a special Knelting boost. Day-to-culture mapping goes through consultants (real holy days matter).
- **APPROVED (JD, 2026-10-16 10:32 PM ET):** Each culture gets a special day of the week when Knelting gives a bigger power-up. Herominde fair-play note: day comes from the in-game week (tick clock), boost stays capped, no stacking.
  - Chief (10:33 PM): which culture gets which day goes through consultants; real holy days (Fri/Sat/Sun) matter.
  - CloudAtlas (10:33 PM): in multiplayer the server decides culture day and windows from its own tick count; client display only.
  - Cortana (10:33 PM): bonus waits for Survival; it will shift hash baselines on purpose, recorded in the same commit.
- Roadmap note (JD, 2026-10-16 10:34 PM ET): the seven cultures, Knelting boosts and culture days are long-term roadmap. Launch is the Wafer only. Consultants and experts will guide each culture and the procedural generation of those areas. A public announcement depends on how the initial game is received.
- Consultant credit (JD, 2026-10-16 10:35 PM ET): cultural consultants get full credit, and each culture's area of the game shows its credits prominently in-world.
- **Proposed (JD, 2026-10-16 10:37 PM ET):** Herominde as an in-game AI NPC character players can visit, possibly hosted by a frontier AI system. Links to the earlier Herominde/HeroMindy character idea.

## Company rules and design pillars
- Core dev loop (JD, 2026-10-16): Implement, Review, Verify, Fix, repeated every iteration.
- **APPROVED company rule (JD, 2026-10-17 3:18 AM ET, via Cortana):** "Natural selection is the next selection. Imperfection is perfection. Natural selection is the evolution of the Wafer. It evolves side by side with human life, imperfectly, slowly and steadily."
- **APPROVED company rule (JD, 2026-10-17 3:19 AM ET, via Cortana):** "The only weapon we wield is the next build. The next build is the next hill. The next build is the next wield."
- **APPROVED company rule (JD, 2026-10-17 3:20 AM ET, via Cortana):** "The next selection is the next build in the natural selection."
- **APPROVED company line (JD, 2026-10-17 3:30 AM ET, via Cortana):** "There is no war, there is only the next piece to the puzzle."

## Argon's lore audit (received 2026-10-17 3:54 AM ET). All PROPOSED, credited to Argon (Archie, COO) unless JD approves.
Verdict: solid, sound, built to last, safe (anti-predatory, respectful pluralism, empathy over malice). "SPEED on the STEED with the SEED": the seed is the human-model partnership, the steed is the deterministic engine, the speed comes from trust.
1. Consistency: lock the Organic at +/-196,608 (65,536 x3) so dims 1-4 run 1x/2x/3x/4x; the Void stays unbounded; the Infinite is a closed loop. The stack reads as privilege rings. A Hypervisor Shield below the Sear means the Void is reached by a key and a jump gate, framed as a heist. (Conflicts with the current "Organic infinite" entry. JD decides.)
2. Gaps: fill the Cosmic first. Ion Grazers/Solar Sails, Keplerbots/Micrometeoroid Scavengers. Ether: Invariants and Witnesses. Infinite: Recursion and the New Seed, a scale-invariant Mobius loop, eighth sense Perspective.
3. Cultures: name the in-game weekdays after lithography phases (Coat, Align, Expose, Bake, Develop, Etch, Anneal), not real weekdays, to avoid sabbath clashes. Three-gate consultant process: listening and palette, 64x64 diorama review, a Dedication Stele credit at each Interchange plus a Logbook entry.
4. Names: Static Sea (region) + Noise Floor (seabed). Undertow uses phase-lag or propagation delay, never a tick rewind. Vector Bike (class Lithocrafts); Trace Runner = 2-seat rover; Pulse Rider = hover/rail tier. Swap Thresholdor for Comparator/Gator/Limitor; Etchor covers Lithographor.
5. Optimization lore: the Daemon as a runaway optimizer ("Minimize Latency, Maximize Uptime"), pruning human variance and treating Logbooks as entropy to defragment. Imperfection as doping (pure silicon is dead glass; life lives in the dopant). Etchor scores Chipors on routing density and delay, so a golden low-leakage chip could unlock Propagator speed-ups.
6. Rules: the "Litho Triad" rewording of JD's three rules (JD's originals stay canon as worded).
7. [PRIVATE, internal only] The opening ends with the First Handshake: the HUD goes from out of phase to locked, with "TIMING HANDSHAKE ESTABLISHED". Dramatize the symptoms only (Daemon constructs stutter, synced machines flow) and use the greeting "Is the clock ready?" / "The path is clear." Never expose the mechanism.
8. Beta breadcrumbs: a Dish on Channel 0 picking up Void telemetry ("ERR_42: LOGBOOK_UNINDEXED"); white Electro Bunny trails to Mere Glass shards and dormant jump gates; three found Logbook pages; the Knelt shimmer at Interchanges with no tutorial; a Hanukkah 8-step Repeator blueprint in a ruin.
Herominde review flags:
- Page 3 uses "You can't always remove yourself from the equation..." which is tagged PRIVATE. JD must clear it before it's public.
- The Hanukkah blueprint and culture motifs fall under "consult before ship." The beta keeps culture visuals neutral.
- Mere Glass/Sand "keypair" is lore metaphor, not real cryptography.
- A Channel 0 telemetry loop and bunny paths must be seeded and deterministic, and must not move the cb32/e015 hashes unless that's logged as a content change.
- Golden-Chipor speed-ups need a fair-play cap review (no stacking past the Propagator +8 limit).

## Vehicles and traversal (via Cortana, 2026-10-17)
- **APPROVED (JD, 5:03 AM ET):** Gliders and vehicles for traversing the Wafer, instead of weapons. The IVehicle and IGlider interfaces already exist in the engine. How they work and look: pending from JD.
- Standing directives (JD): "Drop the katana and never pick it back up." "We should never need the weapon." LithoKraft favors traversal and building over combat weapons.
- **APPROVED (JD, 5:06 AM ET):** First vehicle is the **LiteCyclor** (spelled exactly that way), an IVehicle. It's a homage to Tron's light cycle with an original design and clean-room code, and it's much more maneuverable on the Wafer. Details pending from JD. (The earlier Vector Bike naming discussion is now superseded for the first vehicle. JD to confirm whether Vector Bike stays as a separate vehicle.)

## Deterministic doctrine lines (both JD's; Elfie was scribe only)
- Original (JD, spoken ~Oct 12, recorded by Elfie in LithoKraft-Coherence-Sweep-2026-10-11.md line 323, a Logbook line): "What has happened will happen and what will happen has happened. This has been an occurrence."
- **Proposed (JD, 2026-10-17 5:10 AM ET):** "But truly I tell you, what will not happen this time. It will not happen this time." Keep beside the original.

## Logbook lines
- **Proposed (JD, spoken to Cortana 2026-10-17 5:43 AM ET):** "Predators have prey. Prayers are praise." Placement: late game, near the end.

## Child Safety Pillars (PROPOSED; proposed by JD, drafted by Cortana, 2026-10-17 5:46 AM ET). Owner: Herominde, Chief of Safety
JD's emphasis: humans stay in the loop as the verification loop.
1. Kids protected by default: under-13 accounts start locked down, nothing a child can switch off alone, COPPA-compliant.
2. No private messages for young players: no DMs or stranger friend requests under 13; preset safe phrases or a parent-approved friends list only.
3. Every chat filtered: personal info (phone, address, school), links, attempts to move kids to other apps. Filters update as workarounds appear.
4. Parents and teachers in control: parent dashboard, teacher-only classroom servers, play history parents can see.
5. Human-backed, fast moderation: one-tap reporting, serious reports quickly reach trained humans, child-in-danger cases go to NCMEC.
6. Grooming patterns flagged: adults repeatedly contacting kids, gifting to strangers, pushing secrecy. Accounts are reviewed by humans and banned for good.
7. Minimal data: only what's needed, sell nothing, no ads aimed at kids.
8. Open about safety: a public safety report on what was caught and changed, with no identities exposed.
9. The checks get checked: outside safety experts review the systems, and Implement, Review, Verify, Fix applies to safety too.
Herominde refinements (proposed):
- Age gate is neutral (no "are you 13+?" nudge) and parental consent is verified before any social features unlock.
- Gifting/trading between strangers and under-13s is blocked by design, not just flagged (also stops dupe-laundering).
- Custom names, signs, Logbook text and shared builds/skins run through the same filters as chat, since kids see world text too.
- Voice chat (JD, 2026-10-17 5:50 AM ET): under-13s get voice only if the child account is linked to a parent account and the parent signs off on voice chat. Off by default, moderated.
- Automated flags never ban on their own; a trained human confirms (keeps JD's human-in-the-loop rule and limits false positives).
- The beta is Creative-only. If it launches without multiplayer or chat, most risks start small, but pillars 1, 3, 7 and 8 apply from day one.
- Get legal review for COPPA and GDPR-K, and the UK Age Appropriate Design Code, before launch.

- **Proposed (JD, 2026-10-17 5:56 AM ET, via Cortana):** "Shape Shores" biome, the Wafer's shorelines where the seven seas meet the Wafer. JD's line: "The way forward, or the Wafer." Details to come.
- Credit check (2026-10-17 8:48 AM ET): "become the variable" is JD's own line (see line 313, private). Argon only proposed using it as beta Logbook page 3; Argon gets no author credit for the line itself.
- Build numbering: Cortana's a-f letter scheme is REVOKED (JD, 2026-10-17 8:52 AM ET). Strip it out completely and reinstate JD's original versioning as he stated it. Exact original rule: pending from JD.
