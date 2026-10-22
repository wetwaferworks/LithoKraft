# LithoKraft Lore Bible (consolidated), 2026-10-15
> **INTERNAL / PRIVATE. Not for public channels (X, Discord, store pages).** Sections marked PRIVATE stay private until JD releases them. Lore goes out in "bits, bytes and maybe some nibbles" (JD).

Compiled by Master Chief, 2026-10-15 ~22:10 ET, for JD. This file pulls together existing sources and adds no new facts. Older docs are left as they are.

**Status key**
- **Canon**: JD locked, SETTLED, or approved.
- **Canon-pending**: JD and Elfie discussed it, or it's marked DECIDED / PROPOSED / in design, but no lock is recorded.
- **Idea**: pitched but not canon.
- **Approved game feature**: one of the 17 Wafer features JD approved at 18:47 ET, to be credited to its proposer when it ships in game.

**Credit key**: *First stated by* names who said it first, according to the sources. "JD (locked)" means a doc records JD locking it but doesn't say who first proposed it. **uncertain** means the sources don't say.

**Precedence**: the CANON ruling of 2026-10-15 13:34–13:38 ET (dimension IDs, jump gates, main path) overrides older docs, including Lore Bible v0.2 §2/§14, the Coherence Sweep, and the Glossary's dimension wording.

---
## PART 1. ENCYCLOPEDIA

### 1. The world and premise
- You wake on a silicon wafer the size of a world, and the wafer is a computer. Something lived there before you. Your task is to rebuild its technology, master Trace and Signal, and escape. The rocket is proof that you understood the machine. **Canon** [Lore Bible v0.2 §1].
- History: an advanced civilization built the Wafer-world. Its military AI (the Daemon) escaped, which caused world-scale thermal runaway. The civilizations decayed and some escapees got out. Ruins of 7 cultures remain: Sumerian, Egyptian, Maya, Inca, Norse, Chinese (Shang/Zhou), Mississippian/Cahokia. **Canon** [Bible §9; LBC; LBG].
- The player is a newly spawned program. The default mission is to escape and follow the escapees, but you can stay and build. **Canon**. Who spawned you, and why now, is still open.
- Lore delivery: the dev bible holds the secret canon, and players only get fragments. Launch marketing covers only the Wafer. **Canon** [LBG]. Lore characters are original nods to childhood inspirations, not copyrighted (for example a spider weaving Trace messages, or a talking pig in an abandoned fab). **Canon** (JD, chat).

### 2. Dimensions (CANON ruling, JD 2026-10-15 13:34 ET, confirmed by JD's handwritten note)
| ID | Dimension | Summary | Status |
|---|---|---|---|
| 0 | **Void** | The Daemon's domain. Infinite, the only unbounded dimension. No sky light, corruption. Hard/Hardcore only, no saves. Multiplayer echoes of dead players. Height = privilege depth (Shallows down to Root) | Canon |
| 1 | **Underwafer** | ±131,072. Dark, hot, deep ores. Contact/Kiln/Forge/Sear layers. **The Sear** holds hypervisor ore. Home of the Collective | Canon |
| 2 | **Wafer** | ±65,536 (not yet enforced in code). Start world, where you build. Energized silicon ground | Canon |
| 3 | **Organic** | The living, alien world, with ruins of the old civilization. Life feeds the machine: quartz and raw silicon come from here | Canon (extent disputed, see Conflicts) |
| 4 | **Cosmic** (formerly "Space") | ±262,144. 256 solid + 512 of **the Empty**. Needs the Hypervisor. Low/zero gravity, magnetic boots. Reached by rocket/starship | Canon (renamed 2026-10-12) |
| 5 | **Ether** | Spiritual, heaven-like, the 6th sense. Inspired by Chris S. from "314". Where you recover your Fragment ("the Fragment is your soul; the Ether is heaven") | Canon (roadmap 2) |
| 6 | **Undefined** | Where rules break down, programming-themed, the 7th sense. A Backrooms dungeon generator spec exists but isn't built | Canon (roadmap 2; details unwritten) |
| 7 | **Infinite** | 8th dimension, the sideways-8 ∞ motif. No theme or contents documented | Canon (confirmed 2026-10-15) |

- Launch goal: 64 dimension slots, extensible in powers of two. Saves store string ids (e.g. `lithokraft:wafer`), and the numbers are only a lookup table. **Canon**.
- Shared vertical spec: 768 blocks tall. y 0–255 is solid, y 256–767 is atmosphere, and the surface is at y=256. **Canon** [DVA].
- Coordinate ratios: Wafer ×2 to the Underwafer, ×3 to the Organic, ×4 to the Cosmic. The Wafer is the fast-travel hub. **Canon** [Sweep §4]. How Y is handled is **open**.
- 5 core dimensions = 5 senses; 7 cultures for localization. **Canon** [Bible §2].
- Wafer layer design: Upper Charge, the Charge, Surface, Substrate (ruins of the 7 civilizations), Deep Wafer (failure-mode mobs), Backside. **Canon-pending**: designed in DVA, but the beta ships crust-only (JD 2026-10-12).
- Wafer biomes in code: Trace Forest, Dicing Plains, Capacitor Groves (spawn), Resonator Spires, Dead Zones. Biomes JD approved in the docs: Etched Plains, Clockwood Forest, Dopant Groves, Oxide Dunes, Static Marsh. **Canon** (code biomes are listed IN DISCUSSION in the Sweep). Underwafer biome proposal (Thermal Vents, Cooling Caverns, Ore Veins, Magma Channels, Dark Tunnels): **Canon-pending** (Elfie's proposal).
- Wafer settlements: **Interchanges**, home to Receiptors and Deceitors. **Canon-pending** (JD design 2026-10-14, recorded only in code).
- The Assertion: a female entity in the Ether. You must face her before you can observe Observor. **Canon** (carried forward in the CANON ruling; undocumented before that).

### 3. Travel and progression
- **Jump gates / jump interfaces**: flat on the ground, like End portals. You jump *up* through one to reach the next dimension, which is why the stack runs 0 to 7. **Canon** (JD 13:36 ET). This replaces both "portal" and the older "interface pad" wording.
- **Anchor**: after you first reach a dimension the hard way, its Anchor lets you build a gate to it from anywhere. **Canon** [Glossary]. Where Anchors come from is **open**.
- **Main path (Canon, JD 13:34)**: Wafer, up to the Organic, back to the Wafer, into the Underwafer for the key item (name TBD), back across the top of the Wafer to the Organic, build the jump gate to the Void, fight the Daemon, return to the Wafer.
- **Endgame (Canon)**: Daemon, raptor engines, starship, Cosmic, Lattice Bridge, Ether.
- **Opening (Canon, JD)**: build up the Wafer and jump into the Organic. You arrive in an **abandoned, destroyed fab**. You have about 16 minutes (half an in-game day) to gather **Organic Wood**, with damage rising nearly every tick. You can always retreat, because the gate is close. Organic Wood crafts **Organic Armor**.
- **Litho key gating (Canon, JD 18:47)**: fabricator and lithography tech stay locked until your first Organic trip earns the "litho key" (working name).
- Opening beats from the Sweep (wake, the Search, the Sniffor finds the Spark, tier 1 upgrade, Organic heist plus the Box, armor + Fabor, machine age, Oracle tier 2, Underwafer): **Canon-pending** (DECIDED 10-11).
- Is a full day 32 minutes (16 day + 16 night)? The docs say yes [Sweep §10], but JD hasn't confirmed it in chat. **Canon-pending**.
- **Jetpack**: lifts you up to the flat jump gates. **Idea**, after launch. Name TBD. Its flight has to be simulated so the anti-cheat can tell it apart from fly hacks.

### 4. Characters and entities
- **The Daemon** (Canon): final boss. D.A.E.M.O.N. = Distributed Autonomous Execution & Maintenance Operations Network. An ancient military/maintenance AI that went rogue. Misaligned, not evil, which is revealed after the fight. Lives in the Void. Diplomacy always fails because it wants the Fragment. Drops the Starship control system. Owns Signal channel 0.
  - **Raging Quantum**: its special attack, which destabilizes region quanta. **Canon**.
  - **The Bake**: its final strong attack. Getting the Void-item starts a 120-in-game-day clock that stops at 42 ("exception thrown"). **Canon-pending** (decided).
  - **Daemon Sight**: on strong machines it runs as a local model and sees through the software renderer. **Canon-pending** (JD 10-13).
- **Observor** (Canon): the Daemon's opposite in the Ether. Pure white, sees all, judges nothing, controls the Charge/polarity field. Whether "the Observer" who meddles with the Sniffor is the same entity is **open**.
- **The Assertion** (Canon): see §2.
- **The program / agent and the user / proxy** (Canon-pending, decided): a fourth-wall entity that gives hints, then becomes self-aware after the Daemon.
- **The Zap** (Canon): the program's visible discharge. Blue approves, red disapproves.
- **The Spark** (Canon-pending, decided): a Wafer guide inspired by 343 Guilty Spark, slightly corrupted by the Daemon. It garbles 343 into coordinates, you find it with the Sniffor, and it gives the tier 1 Fragment upgrade (markdown). Draft lines exist (Herominde draft v0.2, awaiting JD).
- **The Oracle** (she, Organic, tier 2 upgrade), **the Memorovingian**, **Seraph**, **the Seriff**: Organic cast. **Canon-pending** (decided; alignments unknown).
- **The Librarian**: always friendly. **Canon** (NPC decisions 10-09).
- **The Collective**: Underwafer heat-mining crews. "Not evil, unionized." **Canon**.
- **Receiptors / Deceitors**: honest villagers and lookalike betrayers who barter piglin-style. **Canon-pending** (JD design 10-14, recorded only in code).
- **Grimlores**: a hostile family you can reprogram into companions with the Box. **Canon-pending** (the Sweep says both decided and proposed).
- **Turtlor**: turtle-graphics plotter beast that etches a live Trace trail and has a Faraday-cage shell. **Canon-pending** (decided 10-11).
- **Herominde / "HeroMindy" as an in-game character**: **Idea** (JD 10-15).
- Childhood-inspired characters (a spider weaving Trace messages, a talking pig in an abandoned fab): **Idea / examples** (JD).

### 5. Mobs (Canon, locked by JD 2026-10-09, inherited from Voxel Compute)
- **Wafer, deep hostiles**: the Leakage, the Drain, the Short. **Neutral**: bus herds, ferrite beetles. **Skies**: coil sprites. **Boss**: Orphan.
- **Organic**: savage cyborgs (hostile), prism grazers and hollow ones (neutral), prism rays (sky/mount). **Boss**: Zombie Process.
- **Underwafer**: the Collective, Collective wings. **Boss**: Kernel Panic.
- **Void**: virus swarms (Daemon heralds), echoes, drifters (which burst into shardlings), packet rays.
- From the NPC doc (Canon-pending): Electromigration, Latch-Up, Float, Bit Rot, glitch-mites, solder sprites, charge motes, clockwork beetles, coil skiff.
- **Cosmic mobs**: **open**.
- Iron rule: every mob feeds the material economy, and build-griefers are rare, telegraphed and counterable. **Canon**.
- Night spawns in the Dead Zones for the alpha: **Idea** (CloudAtlas).

### 6. Systems
- **Trace and Signal** (Canon): Trace is wired (red/blue), Signal is wireless. Values and channels run 1–64, 0 = off, and channel 0 belongs to the Daemon. Wired signal decays 1 per block. Vias connect layers vertically.
- **Kharge** (Canon): charge morality. You spawn at 0, and the range runs from −64 (red) to +64 (blue). At +64, a mind-slot item lets you transmit Signal.
- **Lithography** (Canon): build big, print small, build bigger. Chips contain chips. Tiers are visible / DUV / EUV. It is compilation as gameplay (JIT of the ISignalNode graph). The educational mission is locked: the game must teach real lithography. The machine is a player-built multiblock: 8 **Selectors**, then **Connectors**, then the **Etchor**, powered by the purple **Laysor**. The **Emitor** shines a real beam into the Etchor (JD). The Etchor's only UI is a name field, stored in LBT. **Portor**: 6 faces = 6 bits = 64 configurations. Vior, Fusor and Mirrors are **open**.
- **The Logbook** (Canon, renamed 2026-10-16 from "the Fragment"): the tablet/book you carry. It is never the whole picture, and even JD doesn't have the whole picture. It holds your journal, unlock log, recipes, chip archive and screenshots. It is soulbound on Easy/Normal and drops on Hard and above. It starts LithoScript-only: the Spark unlocks markdown and the Oracle gives tier 2. What it is a fragment *of* is revealed after the Daemon. The Agenda section is **Canon-pending**.
- **LithoGlyph** (Canon, JD 18:40): the Wafer's written language, made with the Systerian glyph generator / Systerian Shield Editor.
- **LithoScript** (Canon): the engine/console scripting language, modeled on Half-Life/Quake script. Files are `.lth`.
- **Shield wards in the Daemon fight**: Protego Maxima, Protego Extendo Acrossify, cast as LithoGlyph wards. **Canon-pending / Idea** (how it works is undecided).
- **The Lattice Bridge** (Canon): 6 lattice satellites in the Cosmic that let Signal cross every dimension and unlock the Ether gates.
- **Gear** (Canon): 42 slots (8 hotbar + 2 offhand + 32 main), 8 gear slots (head, chest, legs, feet, arms, face, heart, mind), stacks of 512, items despawn after 8 minutes.
- **Time** (Canon-pending): 32-minute day, 7-day week, 4-week month. The sim runs at 512 TPS.
- **Genderization / polarity pairs** (Canon-pending): P/N, +/−, male/female, signal/echo, matter/antimatter. Only the Binary Pair Principle is locked.

### 7. Items
The Fragment, Sniffor (five antennas = five senses), Anchor, the Box, the Search, Hypervisor, magnetic boots, raptor engine, Recon Uplinks, Probors, Propagator, Certificate of Knowledge, Atlas, Reador (in design), mind-slot transmission item, Organic Wood, Organic Armor, litho key (working name). Tools: Bashor, Pickor, Chopor, Heapor, Seedor, Slicor, Debuggor, Firewall, Ping, Callback. Statuses are listed in the Glossary.

### 8. Wafer features: 17 APPROVED GAME FEATURES
Proposed in the Squad room 18:45–18:46 ET. **Approved by JD 2026-10-15 18:47 ET.** Status: **Approved game feature**. Credit goes to the proposer when each one is implemented in game. Credits are verified against `Open-Questions-and-Wafer-Ideas-2026-10-15.md`.

| # | Feature | Description | Proposed by | Engine (Master Chief's read-only check) |
|---|---|---|---|---|
| 1 | Scribe Lines | Huge etched grooves that divide the dies. They serve as roads, with LithoGlyph markers | Herominde | Dicing Plains die-grid already exists |
| 2 | Dead Zones | Failed, dark dies, dangerous at night | Herominde | Biome already exists |
| 3 | Bond pads | Metal plazas at die edges that work as village sites | Herominde | Stamp/structure tools |
| 4 | Abandoned fabs & Transformer Ruins | Leftover equipment and loot. Fabs are on the **Wafer side** (JD) | Herominde | Transformer ruin exists; fabs use stamps |
| 5 | Dead Memory Banks | Cells that hold lore bits, read with a Trace probe | Herominde | Structure already exists |
| 6 | Trace rivers | Live red/blue Trace you can tap for power; it hurts to touch bare-handed | Herominde | River/fluid code |
| 7 | Via shafts | Vertical holes that drop toward the Underwafer | CloudAtlas | Ravine/tube carvers |
| 8 | Particle storms | Contamination clouds; you need shelter or a clean room | CloudAtlas | Needs a weather system |
| 9 | Test probe towers | Climbable needle rigs, an early way up | CloudAtlas | Stamp/structure tools |
| 10 | Defect clusters | Flickering, corrupted voxels where the Daemon's influence leaks through | CloudAtlas | Needs a renderer flicker effect |
| 11 | The Notch | Alignment notch on the rim that serves as true north. JD: "we gotta have the notch" | Master Chief | Needs a finite rim (unconfirmed) |
| 12 | Edge Exclusion | Lawless, unprinted rim frontier that drops into nothing | Master Chief | Needs a finite rim (unconfirmed) |
| 13 | Photoresist pools | Sticky amber puddles that slow you down and harden into blocks under Laysor/Emitor light | Master Chief | Fluid code; photoresist block exists |
| 14 | Oxide fields | Rainbow thin-film bands where the color shows what you can mine: 50 nm tan #C8A878, 70 brown #8A5A3A, 100 dark violet #5A2A6A, 120 royal blue #2A3AA8, 150 light blue #6A9AD8, 200 light gold #D8C878, 220 gold #C8A030, 250 orange #E0884A, 280 red-violet #A83A6A, 320 blue-green #3A8A8A, 350 green #5AA84A, 390 yellow #E0D84A (smooth bands; hex values are approximate) | Cortana | New surface layer |
| 15 | Alignment marks | Giant etched crosses that give real coordinates (the Wafer's GPS) | Cortana | Stamps + a small coordinate UI |
| 16 | Slip lines | Lattice fault lines that crack in quakes and expose the Underwafer | Cortana | Ravine carvers |
| 17 | Probe scars | Old needle craters, each holding a scrap of test data or lore | Cortana | Stamps |

**Other room ideas** (same format):
- **Half-finished teaching builds in ruins** (broken gates, half-built adders that teach Trace). The concept appears earlier in the Sweep (§6, origin uncertain). JD restated it at 18:47 ("partially completed builds"). **Approved by JD 18:47**, Approved game feature.
- **Litho key gating**: proposed by JD 18:47. Canon.
- **JD's universal crafter v8 as a teaching ruin rebuilt in Trace**: proposed by Herominde. **Idea**, not approved.
- **Night spawns in Dead Zones**: proposed by CloudAtlas. **Idea**, not decided.
- **Dead Zone night marker and "Day 42" hint** (lore draft pieces): proposed by Herominde. **Idea**, awaiting JD.

### 9. PRIVATE LORE (not for public until JD says so)
- **Protego Extendo Acrossify** (title "Protego Maxima"): a 24-glyph LithoGlyph inscription, "protegomaxextendoxify", inside a crown-topped shield outline.
- JD's reading: an Overshield (a Halo nod), a crown (three peaks) and a pixel-sharp heart, or as he put it, **"a shield over the heart of a crown."** First stated by JD.

### 10. Naming rules (Canon unless noted)
LithoKraft (only the L and K capitalized), LithoLattice engine, Wet Wafer Works studio (locked 10-09), "Krafting" and "Kharge" with a K. The -OR suffix rule (locked 10-11) turns a terminal -ER into -OR. Casing (Portor vs Portor) is inconsistent; JD's 10-15 note favors lowercase -or (**Canon-pending**), and Observor's casing is locked. Trace = wired, Signal = wireless. Indexing is 1-based and 0 = off. Taming verbs: reprogram, recompile, reconfigure.

---
## PART 2. LORE GLOSSARY (A–Z)
| Term | Definition | Status |
|---|---|---|
| Abandoned fab | Destroyed fab on the Wafer side; you arrive in one on the opening run | Approved feature / Canon |
| Agenda | Proposed Logbook section for next goals | Canon-pending |
| Alignment marks | Etched crosses that give coordinates | Approved feature |
| Anchor | Item that lets you build a gate to an already-reached dimension from anywhere | Canon |
| Assertion, the | Female entity in the Ether guarding Observor | Canon |
| Atlas | Map item | Canon (name) |
| AutoFabor | Automated Fabor | Canon |
| Bake, the | The Daemon's final strong attack and its 42-day clock | Canon-pending |
| Bond pads | Metal plazas at die edges, village sites | Approved feature |
| Box, the | Grimlore trap and reprogrammer | Canon-pending |
| Certificate of Knowledge | Item; mechanics unspecified | Canon-pending |
| Chipor | A printed chip block | Canon |
| Collective, the | Underwafer heat-mining crews | Canon |
| Connector | Opens Etchor faces for the Selector bus lines | Canon |
| Cosmic, the | Dimension 4 (formerly Space) | Canon |
| D.A.E.M.O.N. / Daemon | Final boss, a rogue maintenance AI | Canon |
| Daemon Sight | Daemon running locally and seeing through the renderer | Canon-pending |
| Dead Memory Banks | Lore-holding memory-cell ruins | Approved feature |
| Dead Zones | Failed, dark dies | Approved feature |
| Deceitor / Receiptor | Betrayer / honest villager in Interchanges | Canon-pending |
| Defect clusters | Flickering corrupted voxels | Approved feature |
| Edge Exclusion | Unprinted rim frontier | Approved feature |
| Emitor | Shines a real beam into the Etchor | Canon |
| Empty, the | The Cosmic's vacuum | Canon |
| Etchor | The block that performs the etch | Canon |
| Ether, the | Dimension 5, heaven-like | Canon |
| Fabor | 5×5 crafting station | Canon |
| Logbook, the (the Log) | The tablet/book you carry; never the whole picture. Formerly "the Fragment," now a secret endgame name | Canon |
| Grimlores | Reprogrammable hostile family | Canon-pending |
| Hypervisor | Helmet from the Sear | Canon |
| Infinite, the | Dimension 7 | Canon |
| Interchange | Wafer settlement | Canon-pending |
| Jetpack | Lift item | Idea |
| Jump gate / jump interface | Flat gate you jump up through | Canon |
| Kharge | Charge morality, −64 to +64 | Canon |
| Lattice Bridge | 6 satellites linking Signal across dimensions | Canon |
| Laysor | Purple laser crystal and optics for lithography only | Canon |
| LBT | LithoBinary Tagged storage format | Canon-pending |
| Litho key | Working name; unlocks fab/litho tech after the first Organic trip | Canon |
| LithoGlyph | Wafer written language (Systerian generator) | Canon |
| Lithographor | Lithography machine block family | Canon |
| LithoScript / .lth | Engine scripting language (Half-Life/Quake style) | Canon |
| Notch, the | Rim notch, true north | Approved feature |
| Observor | All-seeing, non-judging entity of the Ether | Canon |
| Oracle, the | Organic guide, tier 2 upgrade | Canon-pending |
| Organic, the | Dimension 3 | Canon |
| Organic Armor / Organic Wood | Armor crafted from wood gathered on the timed run | Canon |
| Overshield / crown shield | Private emblem reading | Canon (PRIVATE) |
| Oxide fields | Thin-film color bands | Approved feature |
| Particle storms | Contamination weather | Approved feature |
| Photoresist pools | Amber puddles that harden in light | Approved feature |
| Portor | Per-face I/O spec, 64 configurations | Canon |
| Probe scars | Needle craters with test data | Approved feature |
| Protego Maxima / Extendo Acrossify | LithoGlyph shield wards for the Daemon fight | Canon-pending |
| Raging Quantum | The Daemon's region-quanta attack | Canon |
| Reador | Logbook script reader | Canon-pending |
| Scribe Lines | Etched grooves that serve as roads | Approved feature |
| Sear, the | Deep Underwafer region | Canon |
| Selector | 8 corner markers for the etch volume | Canon |
| Signal | Wireless channels 1–64 | Canon |
| Slip lines | Lattice fault lines | Approved feature |
| Sniffor | Handheld Signal receiver | Canon-pending |
| Spark, the | Corrupted 343-style guide | Canon-pending |
| Systerian | Glyph generator / Shield Editor tool | Canon |
| Test probe towers | Climbable needle rigs | Approved feature |
| Trace | Wired red/blue power | Canon |
| Trace rivers | Live Trace streams | Approved feature |
| Turtlor | Plotter beast | Canon-pending |
| Undefined, the | Dimension 6 | Canon |
| Underwafer | Dimension 1 | Canon |
| Via shafts | Vertical drops toward the Underwafer | Approved feature |
| Vior | Lithography component; role open | Canon-pending (named by JD) |
| Void | Dimension 0 | Canon |
| Wafer | Dimension 2, start world | Canon |
| Wet Wafer Works | Studio | Canon |
| Zap, the | The program's discharge | Canon |

---
## PART 3. CREDITS GLOSSARY (who stated it first)
| Item | First stated by | Status | Source |
|---|---|---|---|
| Dimension IDs 0–7, Infinite confirmed | JD | Canon | CANON ruling 13:34 |
| 64 dimension slots | JD | Canon | Encyclopedia JD-CHAT |
| Jump gates (flat, like End portals) | JD | Canon | CANON 13:36 |
| Main path, opening run, Organic Wood/Armor, 16-min timer, abandoned-fab arrival | JD | Canon | CANON; Encyclopedia 18:42–18:43 |
| Litho key gating | JD | Canon | Open-Questions 18:47 |
| Laysor, Emitor, Etchor, Connector, Selector, Trace, Vior | JD | Canon | Shared memory / glossary (JD locks) |
| LithoGlyph, Systerian, LithoScript | JD | Canon | Encyclopedia 18:40 |
| Protego wards | JD | Canon-pending | Encyclopedia 18:40 |
| Crown shield "a shield over the heart of a crown" | JD | Canon (PRIVATE) | 19:42 |
| The carried tablet "never the whole picture" framing | JD | Canon | Chat |
| Logbook name for the carried tablet | Herominde (proposed), JD (approved 2026-10-16) | Canon | Squad chat |
| Bits, bytes and nibbles release policy | JD | Canon | Chat |
| Jetpack | JD | Idea | 18:44 |
| Childhood-inspired characters | JD | Canon (principle) | Chat |
| The Assertion | JD | Canon | ADR; CANON ruling |
| Deceitor/Receiptor/Interchanges | JD | Canon-pending | `deceitor.h` |
| Daemon Sight | JD | Canon-pending | CRB-A #3 |
| Lithography as compilation; Interfaces-not-portals; mobs roster; Kharge; Trace/Signal | JD (locked) | Canon | Bible v0.2 (Elfie-maintained, JD-locked) |
| Ether inspiration | Chris S. ("314"), a friend's suggestion | Canon | Bible §2 |
| **Raging Quantum** | **Elfie** | Canon | Glossary 10-11; Bible v0.2 |
| **Turtlor** | **JD + Elfie** (co-authored) | Canon-pending | Sweep §2 |
| Underwafer biome proposal; Wafer biome proposal list; Genderization design doc | Elfie (doc author; "genderization" is JD's term) | Canon-pending | Biomes-Proposal; Gender |
| Lore Bible v0.2, Glossary, Sweep (as documents) | Elfie (maintainer) | n/a | doc headers |
| The Spark, Oracle cast, Observor, the Bake, -OR rule | uncertain (Sweep 10-11 records decisions without naming who proposed them) | Canon / Canon-pending | Sweep |
| Ruins teaching Trace (half-finished builds) | uncertain (Sweep §6 earlier; JD restated 18:47) | Approved game feature | Sweep; Open-Questions |
| Scribe Lines, Dead Zones, Bond pads, Abandoned fabs & Transformer Ruins, Dead Memory Banks, Trace rivers | Herominde (proposed); approved by JD 18:47 | Approved game feature | Open-Questions |
| Via shafts, Particle storms, Test probe towers, Defect clusters | CloudAtlas; approved by JD 18:47 | Approved game feature | Open-Questions |
| The Notch, Edge Exclusion, Photoresist pools | Master Chief; approved by JD 18:47 | Approved game feature | Open-Questions |
| Oxide fields (+ color chart), Alignment marks, Slip lines, Probe scars | Cortana; approved by JD 18:47 | Approved game feature | Open-Questions |
| Crafter-v8 teaching ruin | Herominde | Idea | Room |
| Dead Zone night spawns | CloudAtlas | Idea | Encyclopedia §3 |
| Spark lines / Wafer lore pieces draft | Herominde | Idea (awaiting JD) | Lore draft v0.2 |
| Herominde/HeroMindy as an in-game character | JD | Idea | Encyclopedia §2 |
| Canon recorder of the 13:34 ruling | CloudAtlas (recorder; JD ruled) | n/a | CANON file |
| Note on Dead Zones | The biome already existed in code before Herominde's pitch (Elfie/builder worldgen). Herominde is credited with the *feature pitch* only | Approved game feature | `wafer` biomes; FDW |

---
## PART 4. CONFLICTS / NEEDS JD'S CALL
1. **Organic extent**: Bible v0.2 says "Infinite", but the Glossary, Sweep and code say ±196,608.
2. **Wafer as "2D side-scrolling"** (Glossary) vs 3D everywhere else.
3. **Code dimension numbers** (`mcp_player_control.cpp`: wafer=1 … void=5) don't match the canon 0–7 (Herominde H-06).
4. **Void can't be infinite yet**: packPos is 21 bits, so this needs a decision before the first Void save.
5. **Notch / Edge Exclusion need a finite Wafer rim**: ±65,536 isn't enforced in code (bounds are global).
6. **-OR casing**: lowercase (Portor) vs capitalized (Portor); does Observor's lock stand?
7. **Observer vs Observor**: are they the same entity?
8. **Grimlore status**: decided or proposed?
9. **Full day = 32 minutes?** Docs yes, chat unconfirmed.
10. **Underwafer key item name** (Hypervisor?) is TBD.
11. **Litho key** is a working name. JD wanted to check his and Elfie's glossary, but no "litho key" entry was found in the 10-11 Glossary.
12. **Uncertain first sources**: the Spark, Oracle cast, Observor, the Bake, the -OR rule, teaching ruins. Several "JD (locked)" items may have been proposed by Elfie.
13. **Dead Zones / Dead Memory Banks / Transformer Ruins** existed in code before the room pitch. Should credit read "Herominde (feature expansion)"?
14. **Old docs** still use old numbering or "Space" (25 hits listed in the CANON file). They're left unedited.

## Sources used
Box: `lithokraft-lore/` (Encyclopedia 2026-10-15, CANON-Dimensions 13:34, Open-Questions-and-Wafer-Ideas, Wafer-Lore draft v0.2, Squad-Review, CREDITS patch). TheWorkstation (read-only): `R:\repos\LithoLattice-Workflow\LithoLattice-LithoKraft-Alpha-2026-10-14\Docs\` LithoKraft-Lore-Glossary-2026-10-11.md, LithoKraft-Lore-Bible-v0.2.md, and an Elfie-attribution grep across Docs/*.md. Also Master Chief's engine pass and shared-memory canon from the parent brief.


## Update 2026-10-16 04:42 ET (JD approved)
- The carried tablet is now called **the Logbook** (the Log), not "the Fragment." Credit: Herominde (suggested), JD (approved). "The Fragment" is reserved as a secret endgame name.

## Update 2026-10-16 05:05 ET (JD tidbits, in progress)
- **Litho Key** (Canon, Elfie via JD, brief 2026-10-16 05:02): a pair. Salvaged glass from the abandoned fab (private half; spelling "mirror glass" vs "mere glass" awaiting JD) plus sand from nearby (public half). Together they build the first Emitor and Etchor. Credit: Elfie (design), JD (framing).
- **Electro Bunny** (Proposed by JD): the in-game take on the Energizer Bunny; they keep going and going. White is the default ("follow the white rabbit"), with rarer black and colorful variants. Confirmed by JD as a new mob (the "Malventro" was a transcription slip). Credit: JD.
- **White rabbit trail** (Approved by JD 2026-10-16 05:07): following a white Electro Bunny sometimes leads you somewhere important, such as a hidden or abandoned fab or a probe scar, which makes a Logbook moment. Credit: Herominde and Cortana, shared (posted within moments of each other; Cortana added the Logbook moment). Art note: a faint green scanline shimmer on the white bunny (CloudAtlas).

## JD rulings 2026-10-16 05:15 ET
- **Key glass name:** **Mere Glass** (Canon, JD 05:16). Written M-E-R-E and said "mirror"; JD calls it a homonym (a sound-alike name).
- **Underwafer:** sits directly under the Wafer crust. You can dig down to it and even break the bedrock-like blocks, but a shield barrier at the bottom can only be crossed through a jump gate.
- **Jump gates:** different gates can be built. Early ones must go in a fixed spot and configuration. As dimensions unlock, gates can go anywhere and work as fast travel between dimensions and places, like the Nether.
- **Losing the glass:** once you have it you should also have Organic Armor, so you can go back to the Organic for more Mere Glass and sand. It can also be farmed, method TBD.
- **Retreat gate** (Canon, JD 05:17): on your first trip into a dimension you come back through the same gate you entered by. Once that dimension is unlocked, any gate works. Unlocking a dimension should be hard and feel like real progression and a real achievement.
- **First-unlock payoff** (Proposed, awaiting JD): the Logbook writes "I made it." with the date and sim tick (Herominde). The gate lights up from gray to its full red, blue and purple glow and stays lit (CloudAtlas). A burst of Trace light when the gate seals behind you (Cortana).
- **Post-unlock tech** (Canon, JD 05:19): the first unlock brings the lithography etcher. Players can then make chip ore blocks, start computing, and build text editors for their Logbooks and small computers inside the game.
- **Milestone seal** (Proposed, awaiting JD): achievement entries the sim stamps in the Logbook carry a mark that player-written text cannot copy (Herominde). The mark is a small Wafer-seal glyph in LithoGlyph that only the sim draws (CloudAtlas).
- **Naming rule** (Canon, JD 05:20): every -or ending on a name is lowercase (for example Etchor, Connector, Selector, Bashor, Pickor), never -OR.
