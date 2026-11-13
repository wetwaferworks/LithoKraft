# LithoKraft — Lore & Glossary
**v2 · 2026-10-11 · living document, maintained by Elfie**

Supersedes `LithoKraft-Lore-Glossary-2026-10-11.md` (v1). Folds in the agent/proxy terminology, the Sniffor, the Organic's new cast (the Oracle, the Memorovingian, Seraph, the Seriff), and lore from the LithoKraft Lore Bible v0.2.

**How to read the status marks:**
- **LOCKED** — decided by JD, canon.
- **IN DISCUSSION** — being worked out; do not treat as final.
- **PROPOSED** — an idea on the table, not yet decided.
- **OPEN** — awaiting JD's call.

**Lore-delivery rule (LOCKED):** the story is told in bits and fragments — incident-report books, terrain, ruins, worlds — and players construct the lore themselves. The true canon is held privately, cards-to-the-chest. (Mojang-style.)

---

## The two sides of the fourth wall (LOCKED)

- **The program** — also called **the agent**. The in-fiction entity. It can look at the screen, give hints, and manifests in the overworld as the Zap. Post-Daemon, it looks back through the screen and talks to itself — self-aware.
- **The user** — also called **the proxy**. The real-world player, as named within the fiction.

## The premise

You wake on the surface of a silicon wafer the size of a world. The wafer is a computer. Something lived here before you. Your task: rebuild the technology, master Trace and Signal, and escape — the rocket is proof you understood the machine you were born on. Win condition: build the rocket and follow the escapees' trail out. The sandbox is staying and building a new civilization.

**Storytelling doctrine (JD, 2026-10-11):** the world tells the story, not text dumps — like Minecraft, whose story remains a mystery. LithoKraft differs once: when you defeat the Daemon, you'll know what the story was. Environmental storytelling all the way down; the ending resolves it.

**Technical-play doctrine (JD, 2026-10-11):** what Minecraft avoids, LithoKraft embraces. Technical play is first-class: optimized farms that serve a function are ENCOURAGED. Mob farming encouraged — hostile mobs especially, non-hostiles too. Everything is farmable to some extent; players must be able to attain massive amounts of items. **Balance:** progression stays fair — farms are not free; they have THREATS the player must defend against (no cheesy invincible farms). Bounds: no perimeters (mass spawn-proofing), no removing large areas — farms yes, strip-mining the world no.

---

## Dimensions (LOCKED unless marked)

Seven total, in powers of two. The five core correspond to the five senses; the full seven to seven senses (design theme, LOCKED).

| Dimension | Bound | Notes |
|---|---|---|
| **The Wafer** | ±65,536 | The main world. Every player starts here. |
| **The Underwafer** | ±131,072 | Below the Wafer. Dark, hot, deep ores. Holds the Sear. |
| **The Organic** | ±196,608 | The living dimension. Visible below from Space. Ruins of the old civilization. |
| **Space** | ±262,144 | Light or zero gravity. 512 blocks of the Empty above the solids. Entered by rocket/starship; left by jumping. |
| **The Void** | Infinite | Below everything. The Daemon's domain. Hard/Hardcore: no saves. Corruption. |
| **The Ether** | — | Post-Daemon, spiritual/heaven-like. Reached via the Lattice Bridge. |
| **The Undefined** | — | The seventh dimension. Unwritten. Where rules break down; programming-themed. |

- **Dimension stack arrangement (PROPOSED):** a single tower, top to bottom: Space, Organic, Wafer, Underwafer, Void. Known links: Space→Organic by falling (free descent); Wafer→Underwafer via bottom interface; Void directionally below the Underwafer. Organic↔Wafer adjacency assumed; order not final.
- **Space physics/traversal (LOCKED):** gravity lightened or zero; **magnetic boots** are the building solution up there. Falling or jumping from Space drops you directly into the Organic — descent is free, ascent requires rocket/starship (unique to Space; other dimensions use interfaces). OPEN: landing mechanics, whether x/z carry over.
- **Space verticality (LOCKED):** Space keeps the full 768 height — 256 solids + 512 of **the Empty**. The Hypervisor helmet is required in Space too; isolation is its theme.

## Travel

- **Interface** (LOCKED) — dimensional travel uses interfaces, not portals. Flat pads on the ground plane; jump in, jump out.
- **Interface jump** (LOCKED) — in-world slang for interface travel.
- **Anchor** (LOCKED) — the item that lets you build an interface to a dimension from anywhere. Found in the dimension after you first reach it the hard way; anchors are for interface jumps only. OPEN: exact source (structure? drop? craft?), and whether consumed or a permanent key.
- **The Lattice Bridge** (LOCKED) — six lattice-point satellites deployed in person via starship, forming a bridge across Space. All six placed = full Signal boost, Signal transmits across all dimensions, Ether gates unlock on either side. (JD corrected from 8 to 6.)

---

## The charge

- **Kharge** (LOCKED) — electrical-charge morality, spelled with a K. Spawn at 0; actions move you from **−64 (red)** to **+64 (blue)**. The program reacts to it; it's visible in the UI.
- **The Zap** (LOCKED) — the program's visible manifestation: an electrical discharge in the overworld. Blue = it approves, red = it disapproves (tied to Kharge). It can strike Trace wire and power it for a tick — the program touching the world. To be playtested.
- **Sniffor** (IN DISCUSSION) — handheld early signal receiver. Picks up Daemon messages and transcodes ambient Signal around the player (line-of-sight). Crafted at a FabOR. Both the proxy *and* the agent get one.

## The Fragment

- **The Fragment** (LOCKED) — your starting item: an editable book. Available day one, first tick — open inventory → open Fragment → read/write immediately. It is the onboarding guide.
  - **Pages:** (1) journal — infinite, freeform; (2) unlock log — the game auto-logs every unlock + timestamp; (3) recipes — every discovered recipe; (4) chip archive — every chip ever printed with a LithographOR, PortOR configs saved forever; (5) **images** — the screenshots gallery (JD, 2026-10-11): full-fidelity captures and agent-fidelity (low-res) captures, both storable from lithoscript (`screenshot()` targets the Fragment's images directly).
  - **Rules:** must be on the player (in inventory) for unlocks to track — stored in a chest, unlocks pause. Easy/Normal: soulbound, can't lose it. Hard/Hardcore/Ultra: drops on death, 8-minute despawn, lost in lava/chemicals. A crafted replacement is blank — re-interact with blocks to rebuild knowledge.
  - **PvP:** killing a player lets you take their Fragment — journal, unlocks, chips, screenshots. Trophy collecting.
  - **Recovery:** a totem-like item can resurrect it; killing the player who took yours is another way; post-Daemon, the Ether rebuilds it.
  - **Lore:** called "the Fragment" because it's part of something larger. **The Daemon wants it** — that's why diplomacy always fails. Also structured memory for future AI agents.
- **The Agenda** (PROPOSED) — a Fragment section defining next goals and steps for the player, the proxy, and the agent: where they're heading, where they come from, what's next.
- **Mind-slot transmission** (LOCKED) — at full blue Kharge (+64), a special item in the mind gear slot lets you transmit Signal directly (limited).

---

## Lithography — the namesake loop

- **Build big → print small → build bigger** (LOCKED) — the core loop. You build a contraption, the machine miniaturizes it into a chip, chips can contain chips (recursive). Tiers follow the real wavelength ladder: visible → DUV → EUV, so the game teaches *why* shorter light means smaller features.
- **Lithography is compilation as gameplay** (LOCKED) — the player physically builds a contraption; the engine analyzes the ISignalNode graph and JIT-compiles it to an optimized form. The chip runs faster (higher tick, lower cost) than the original blocks. The physical shrinking is the visual metaphor; the JIT is the real optimization. ISignalNode is the intermediate representation — also the path to GPU compute (fixed graphs map to shaders).
- **Lithography job** (LOCKED) — one etch operation: select area → etch → chip. The engine must detect when a lithography machine has been built — that's an unlock, auto-logged in the Fragment.

### The -OR family (LOCKED design language)

The **-OR suffix is a game-wide naming trend** for items and components — as signature as the powers-of-two motif. Players learn to read it: see the -OR, know it's a machine component.

- **LithographOR** (LOCKED name; build TBD) — the full lithography machine.
- **EtchOR** (IN DISCUSSION) — the single block that performs the etch. Powered by the Laysor; fed by the SelectORs. Its **only UI element is a name field**: the SelectORs tell it **what to make**, the name field tells it **what to call it** — the typed name becomes the output chip's name. Design emulates real lithography machines (JD studied Asianometry videos) as a deliberate ELI5 simplification.
- **SelectOR** (LOCKED) — the eight corner markers (4 top + 4 bottom) defining a lithography job's etch volume. They stream the volume to the EtchOR as **8 Trace wires: 4 red + 4 blue**, landing on the ConnectORs' **top faces**, plus a serial line for contents (landing face open).
- **ConnectOR** (IN DISCUSSION, refined from JD's 2026-10-11 Minecraft prototype) — **three** per machine, in columns **spaced from the EtchOR** and joined to it by physical **Vior runs** (spacing eases player wiring). Exposes **nine** wire-landing faces across tops and bottoms (3×3); only **eight** SelectOR wires need landing — the spare face gives players wiring options without forced compactness.
- **Vior** ("via-or"; PROPOSED rename of "via" — JD 2026-10-11, because everything's got an -OR) — the Trace via component; in-game identity is the **silicon block**. **Blue Vior** sits atop the EtchOR, carrying signal **DOWN** from the ConnectORs; **Red Vior** sits beneath, carrying signal **UP** from the ConnectORs. Matches the PortOR spec: vias handle top/bottom.
- **Machine detection** (LOCKED) — the lithography machine is a **multiblock the engine detects algorithmically**; detection must accept **spread-out and miniaturized** variants. Sequencing: pre-alpha, JD builds and wires the design in-game with real Trace → **then** it's verified with code. Detection = the "lithography discovered" unlock.
- **Wafer in / chip out** (LOCKED-ish, from prototype) — blank **wafers enter** via hopper/dropper **input buffer lines**; finished **chips exit** the front of the machine.
- **Laysor** (LOCKED) — the optic system (sounds like "laser"). Lithography ONLY — never a data path. Purple hue. Contacts the EtchOR from the **left and right sides only** (prototype: purple purpur walls on the flanks).
- **Mirror** (PROPOSED) — real lithography machines steer light with mirrors; LithoKraft doesn't have them yet. Future component for the Laysor path.- **PortOR** (LOCKED) — the Trace wiring spec: input OR output per face. Does not select lithography area.
- **FabOR** (LOCKED) — the 5×5 crafting station (renamed from Fab Table). **AutoFabOR** (LOCKED) — its automated successor; crafting-table → crafter progression. The FabOR/AutoFabOR **pre-fabricates blank wafers**, feeding the lithography machine factory-style.
- **Fab factory** (LOCKED concept) — the automation loop: AutoFabOR stamps blank wafers → buffer line feeds them in → EtchOR etches the SelectOR pattern → chips come out the front **automatically named with what they do**.
- **Lithograph** (LOCKED) — a printed chip. The pun is the point: *lithograph* and *LithoKraft* sound nearly identical. (This chat is the lithographs lore lounge.)
- **ChipOR viewport** (LOCKED — JD, 2026-10-11) — the ChipOR has a transparent top: peer inside and see your construction miniaturized at full detail — YOUR circuit, not a generic texture, rendered via the sub-block subdivision mesher. The micro-wiring doesn't fire and there's no fancy lighting yet (maybe later); it's a static representation. The trophy case for the lithography loop: build big → print small → hold the proof.

### Sub-block subdivision (LOCKED direction — JD, 2026-10-11)

1m blocks subdivide via sparse octree to 64³ micro-cells (6 levels, powers of two), only where detail lives. Purpose-built as a **modeling system**: subdivide → add detail → save → place. First uses: the armory (BashOR, ChopOR, ...), silicon trees, mobs (TurtlOR) — geometry that 1m cubes can't express, matching the locked concept art. Render/detail-level only; the 512 TPS sim still ticks whole blocks.

### Tool & weapon names (LOCKED — JD, 2026-10-11)

Minecraft tools, programming souls, -OR bodies:
- **BashOR** — hammer. You bash things; Bash is the shell.
- **PickOR** — pickaxe. (Alt: **DecompilOR** — decompiles blocks into source.)
- **ChopOR** (LOCKED — JD, 2026-10-11) — axe. Literally chops up code: fells silicon trees and yields the pieces — bits and bytes.
- **HeapOR** — shovel. Dig the heap.
- **SeedOR** — hoe. Tills, plants seeds; seeds are procgen.
- **SlicOR** — sword. Slices arrays, slices mobs.
- **Firewall** — shield. Already perfect; the lingo bows.
- **Ping** — bow. Fires packets; arrows are requests awaiting responses.
- **Callback** (LOCKED — JD, 2026-10-11) — the returning thrown weapon (trident role). Thrown, and it always comes back. (The mace role is already covered by BashOR — no separate SmashOR needed.)

### Special items (LOCKED — JD, 2026-10-11)

- **DebuggOR** — LithoKraft's answer to Minecraft's debug stick (the one they never let players have). Rotates blocks freely after placement — no more placing it wrong the first time. A love letter to technical players and their oldest gripe.

### Atlases (LOCKED name — JD, 2026-10-11)

In-game maps are called **atlases**. 256×256 pixels (double Minecraft's 128), **8 zoom levels** (JD, 2026-10-11 — double Minecraft's 4), doubling coverage each step: 256 → 512 → 1K → 2K → 4K → 8K → **16K blocks across** at max zoom. Granularity 8/16/32/64 blocks-per-pixel. Crafting method undecided — crafted or fabbed, TBD. (Shares the map-rendering core with the `genmap()` lithoscript function.) Performance note: 16K-atlas generation needs serious optimization; coarse granularity is mandatory at high zooms.
- **ReadOR** (IN DESIGN) — slot your Fragment inside and it sends your LithoScripts to a keyboard, feeding them into contraptions built by you, the proxy, or the agent. The Fragment as script cartridge.

### The console language

- **LithoScript** (LOCKED, named 2026-10-11) — the debug console's language: Half-Life 1-style commands, typed cvars, aliases (recursion cap), `exec` of script files. Spoken in the debug console, scriptable by players, bound into AngelScript and the engine API. Spelled lowercase `lithoscript` in code.
- **.lth file** (LOCKED) — a plain-text LithoScript script file (our `.cfg`). Players write and modify them; executed from a sandboxed directory.

---

## Signal & Trace

- **Trace** (LOCKED) — wired. Redstone treated as an engineering discipline. **Signal** (LOCKED) — wireless. The two words are never swapped.
- **Signal Transmittor / Signal Receivor** (LOCKED) — the wireless blocks. 1-tick latency. Channels **1–64**, 1-based; **0 is off — channel 0 belongs to the Daemon**.
- **Recons** (PROPOSED, Space program) — cheap unmanned recon satellites (1 block + 1 raptor-engine block), each giving a partial signal boost. Staged, not all-or-nothing.

## The Space program

- **The Empty** (LOCKED) — Space's vacuum medium. It lacks air. The Hypervisor seals you against it.
- **The Hypervisor** (LOCKED) — the helmet, crafted from hypervisor ore deep in the Underwafer's **Sear**. Isolation is its theme: it seals the wearer against the Void's infohazards, mind-attacks, and the Empty alike. Gating Space behind the Underwafer/Sear progression is the point.
- **Raptor engine** (PROPOSED) — a single block; a partially lithographized bigger engine (booster + control imprinted into one block). Starship propulsion.
- **The Ether gates** (LOCKED) — unlocked from either side of the completed Lattice Bridge, post-Daemon. Ether access in the Organic comes via an Ether item/interface, tying into Fragment restoration.

## The old civilization (LOCKED)

An ancient military AI — **the Daemon** — went rogue in a thermal runaway. The escape *was* the thermal event: one catastrophe, and the civilizations decayed. Some escapees got out and left — the player is following their trail. The player is a newly spawned program. Default mission: escape, finish, leave. Staying and building is the sandbox.

## Wafer surface biomes (PROPOSED — Elfie's inspiration, 2026-10-11)

The Wafer's surface is **processed/refined silicone** — essentially pure quartz refined to elemental silicon (JD: "isn't refined silicone just a form of quartz?"). You walk on a finished chip the size of a world, but you can't scoop it up to print your own: **raw silicone for wafer-making lives in the Organic, and fab technology unlocks in the Organic** (JD-locked 2026-10-11). The wafer is the factory floor, not the mine.

- **Trace Forest** — trees grown like circuits: via trunks, trace branches, red/blue glowing tips (the Trace colors, living in the world).
- **Dicing Plains** — vast flats scored with die-grid scribe lines. Sparse, open first-spawn territory.
- **Capacitor Groves** — squat cylindrical "trees" like capacitor cans, clustered dense and maze-like, humming faintly.
- **Resonator Spires** — tall quartz spires that catch light and sing (ties to voxel audio reflections).
- **Dead Zones** — grey, unpowered, no glow. The wafer's dead skin; broken circuits poke through to the underground ruins.

**The Minecraft inversion (JD, 2026-10-11):** in Minecraft you descend into the Nether (death) for quartz to build comparators. LithoKraft flips it: you ascend to the Organic (life) for quartz to build wafers. Same rhyme, inverted moral geography — life, not death, feeds the machine.

**Energized ground (JD, 2026-10-11):** the refined silicone surface isn't dead rock — it's energized. Trace/zap energy pulses through the blocks periodically, and the pulse pattern changes per biome. The ground itself is alive with the machine's heartbeat.

**Biome blending (JD, 2026-10-11):** Minecraft-style smooth biome transitions — no hard borders. Terrain, palette, decoration density, and ground-pulse all blend across biome edges as gradients. General system: Wafer now, other dimensions later.

**Silica grass** (LOCKED — JD, 2026-10-11) — the Wafer's grass: tiny silicon fern/shrub ground cover. Spreads via random tick like Minecraft grass. Drops **silica seeds** — plantable and farmable.

**Concept art direction (LOCKED look-and-feel, 2026-10-11):** Elfie's four concept pieces (Trace Forest, TurtlOR, Dead Zones, Dicing Plains — `workspace/user/files/concept-art/`) define the in-game look and feel for the Wafer biomes. Filed in the history books. Note: the TurtlOR reads smaller in-game than depicted.

## Mobs & the Organic's cast

**Design rules (LOCKED):** every mob feeds the material economy. Hostiles concentrate deep in the Wafer, in dungeons, and in the Void. Every build-griefer is rare, telegraphed, and counterable.

### Hostiles (LOCKED roster)

- **The Leakage** — deep Wafer. Bleeds power/heat from nearby circuits. (Failure modes made flesh.)
- **The Drain** — deep Wafer. Saps signal strength from wires it touches.
- **The Short** — deep Wafer. Bridges red and blue wire, causing crosstalk chaos.
- **Savage cyborgs** — the Organic. Corrupted remnants of the old civilization.
- **Virus swarms** — the Void. Daemon heralds; always telegraphed.
- **Grimlore** (PROPOSED — JD, 2026-10-11) — *everywhere*. The gremlin made flesh: saboteur-mutants that infest the entire stack, all dimensions, all layers. No fixed form — they mutate per biome, per machine, per moment. The one hostile with no home, because everywhere is home. (The transcription gremlin is their patron saint.)
- **Orphan** (boss) — deep Wafer. A process whose parent died.
- **Zombie Process** (boss) — Organic ruins.
- **Kernel Panic** (boss) — the Underwafer. Heat made flesh.

### Neutral (LOCKED roster)

- **Wafer:** bus herds (migratory grazers, scatter when startled); ferrite beetles (roll into a ball; attacked → discharge pulse scrambles nearby wire). **Beetle lore (JD, 2026-10-11):** compiled, not born — corrupted memory cells that achieved mobility. Fuzzy with corruption, but they retained their agency: too simple for the Daemon to fully corrupt, "nothing to corrupt" — they're pure. Mostly blue charge, occasionally red.
- **Organic:** prism grazers (crystalline, deer-like, peaceful/farmable); hollow ones (maintenance constructs, hostile only if disturbed).
- **Underwafer:** the Collective (heat-mining crews; barter cooling for ores; attack one → the whole crew turns — "not evil, unionized").
- **Void:** echoes (fragments of dead programs; can't hurt or be hurt); drifters (attacked → burst into aggressive shardlings).
- **Skies:** coil sprites (Wafer); prism rays (Organic); Collective wings (Underwafer); packet rays (Void, hostile during surges).
- **TurtlOR** (PROPOSED — co-authored JD + Elfie, 2026-10-11) — Wafer. The turtle, all the way down: slow plotter-beast that etches live trace trails as it walks (turtle graphics made literal). Shell is a Faraday "paradise cage" — EM shielding. Nested: every TurtlOR carries a smaller one; nobody's found the bottom turtle. Drawn to the materials required to make the Sniffor.

### The Organic's characters (IN DISCUSSION)

Matrix-inspired, LithoKraft-original. Beautiful paranoia: nobody knows who's a cyborg, who's an agent, who's playing for which side — and the proxy has to unravel the threads.

- **The Oracle** — reads the living. Found in the Organic.
- **The Memorovingian** — traffics in *memories* the way his namesake trafficked in exiled programs. A dealer in what's remembered.
- **Seraph** — the Oracle's bodyguard. Ex-Agent; all gold code and perfect manners. Stands at the door.
- **The Seriff** — double entendre: **Sheriff** *and* **serif** — a lawman named after a typeface detail, in the game about printing. Alignment undecided; mystery intact.
- **Cyborgs & agents** — possible inhabitants of the Organic, all, some, or none. Some could be good, some bad. Unknown.

## The endgame

- **The Daemon** (LOCKED) — the final boss. Corruption incarnate; an ancient military AI, misaligned rather than evil — the tragic reveal. D.A.E.M.O.N. = **D**istributed **A**utonomous **E**xecution & **M**aintenance **O**perations **N**etwork. Diplomacy always fails (it wants the Fragment). Upgraded options and thinking on high-end hardware. Drops the starship control system.
- **Raging Quantum** (LOCKED, named 2026-10-11) — the Daemon's special attack: it destabilizes local region quanta, and the ground fragments and glitches as chunk storage corrupts beneath the player. Born from a voice-transcription garble of "region quantum." Canon — the most JD thing ever.
- **The Ether's purpose** (LOCKED) — beyond post-game content: the Ether is where you go to recover what you lost. Defeat the Daemon, enter the Ether, obtain the means to rebuild your Fragment. The Fragment is your soul; the Ether is heaven; you go there to be restored.
- **ObservOR** (LOCKED — JD, 2026-10-11) — the Daemon's flip side, found in the Ether. Knows all, sees all, observes all — omnipotent, omnipresent, there since the beginning. Draws no conclusions, passes no judgment. Pure, white, watching the whole time. The structural opposite of the Daemon: runaway action versus judgeless witness. **Controller of the Charge** — positive and negative, the polarity itself. (Programming vocabulary: the Observer pattern, tracers, watchpoints — that which watches without interfering.)
- **AssertOR** (PROPOSED — JD, 2026-10-11) — boss-like gatekeeper in the Ether, faced BEFORE you may see or speak to the ObservOR. It asserts; you prove. Pass the assertions, the program continues — fail, and it halts. The brutal boolean to the ObservOR's judgeless witness: judgment before mercy.

## Studio & design languages

- **Wet Wafer Works (WWW)** (LOCKED 2026-10-09) — the studio name.
- **-OR** (LOCKED) — the game-wide component naming trend (see the -OR family above).
- **Powers of two** (LOCKED) — the other game-wide motif: 64 channels, ±2^16/2^17 bounds, 1–64 lighting/Kharge scales.
- **64** (LOCKED) — the theme number: channels, Kharge range, value range.

---

## A–Z index

- **-OR** — game-wide component naming trend · *Lithography*
- **.lth file** — plain-text LithoScript script · *The console language*
- **64** — the theme number · *Studio & design languages*
- **Agent** — the program's other name; the in-fiction entity · *Fourth wall*
- **Agenda, The** — proposed Fragment goals section · *The Fragment*
- **Anchor** — interface-jump item, one per unlocked dimension · *Travel*
- **AutoFabOR** — automated 5×5 crafting successor · *-OR family*
- **Channel 0** — off for players; belongs to the Daemon · *Signal & Trace*
- **Chip archive** — Fragment page: every chip ever printed · *The Fragment*
- **Collective, the** — Underwafer heat-mining crews; unionized, not evil · *Mobs*
- **ConnectOR** — EtchOR top/bottom; unlocks eight faces · *-OR family*
- **Daemon, The** — final boss; D.A.E.M.O.N.; wants the Fragment · *The endgame*
- **Drain, The** — hostile; saps wire signal · *Mobs*
- **Drifters** — Void neutrals; burst into shardlings when attacked · *Mobs*
- **Echoes** — Void; fragments of dead programs · *Mobs*
- **Empty, The** — Space's vacuum medium · *Space program*
- **EtchOR** — the block that etches · *-OR family*
- **EtchOR** — the block that etches; one UI element (name field) · *Lithography*
- **Ether, The** — post-Daemon dimension; where Fragments are rebuilt · *Dimensions / Endgame*
- **Ether gates** — unlock from the completed Lattice Bridge · *Space program*
- **Fab factory** — AutoFabOR wafers → buffer line → EtchOR → auto-named chips · *Lithography*
- **FabOR** — 5×5 crafting station; pre-fabricates blank wafers · *-OR family*
- **Fragment, The** — starting book: journal, unlocks, recipes, chips, screenshots · *The Fragment*
- **Hypervisor, The** — helmet from the Sear; isolation · *Space program*
- **Interface** — dimensional travel; flat pads, not portals · *Travel*
- **Interface jump** — in-world slang for interface travel · *Travel*
- **Journal** — Fragment page 1; infinite, freeform · *The Fragment*
- **Kernel Panic** — Underwafer boss; heat made flesh · *Mobs*
- **Kharge** — charge morality, −64 to +64 · *The charge*
- **Lattice Bridge, The** — six satellites; unlocks the Ether · *Travel*
- **Laysor** — optic system; lithography only; purple · *-OR family*
- **Leakage, The** — hostile; bleeds circuit power/heat · *Mobs*
- **Lithograph** — a printed chip; the pun · *-OR family*
- **LithographOR** — the lithography machine · *-OR family*
- **Lithography job** — select area → etch → chip · *Lithography*
- **LithoKraft** — the game; lithography + craft; first-party LithoLattice mod · *New*
- **LithoScript** — console language (lowercase `lithoscript` in code) · *The console language*
- **Machine detection** — the lithography multiblock is engine-detected; accepts spread or compact builds · *Lithography*
- **Magnetic boots** — Space building solution · *Dimensions*
- **Memorovingian, the** — Organic; traffics in memories · *Characters*
- **Mirror** — proposed future Laysor component (real machines use them) · *Lithography*
- **Mind-slot transmission** — +64 Kharge Signal transmission · *The Fragment*
- **Oracle, the** — Organic; reads the living · *Characters*
- **Organic, The** — ±196,608; the living dimension · *Dimensions*
- **Orphan** — deep-Wafer boss; parentless process · *Mobs*
- **PortOR** — Trace wiring spec: input OR output per face · *-OR family*
- **Program, The** — the fourth-wall entity; also the agent · *Fourth wall*
- **Proxy, the** — the user's in-fiction name · *Fourth wall*
- **Raging Quantum** — the Daemon's special attack · *The endgame*
- **Raptor engine** — single-block starship engine · *Space program*
- **ReadOR** — Fragment-as-script-cartridge component · *-OR family*
- **Recons** — cheap unmanned signal-boost satellites · *Signal & Trace*
- **Sear, The** — deep Underwafer; hypervisor ore · *Space program*
- **SelectOR** — eight etch-volume corner markers · *-OR family*
- **Seraph** — the Oracle's ex-Agent bodyguard · *Characters*
- **Seriff, The** — Sheriff × serif; alignment unknown · *Characters*
- **Short, The** — hostile; bridges red/blue wire · *Mobs*
- **Signal** — wireless; never "wired" · *Signal & Trace*
- **Signal Receivor** — wireless block; channels 1–64 · *Signal & Trace*
- **Signal Transmittor** — wireless block; channels 1–64 · *Signal & Trace*
- **Sniffor** — handheld signal receiver; proxy and agent both get one · *The charge*
- **Space** — ±262,144; zero-g; the Empty · *Dimensions*
- **Trace** — wired; redstone as engineering · *Signal & Trace*
- **Undefined, The** — seventh dimension; unwritten · *Dimensions*
- **Underwafer, The** — ±131,072; below · *Dimensions*
- **Unlock log** — Fragment page 2; auto-timestamped · *The Fragment*
- **User, the** — the real-world player; also the proxy · *Fourth wall*
- **Via** — renamed **Vior** (proposed) · *Lithography*
- **Vior** ("via-or"; proposed) — the Trace via; blue (signal down) / red (signal up); silicon in-game · *Lithography*
- **Virus swarms** — Void hostiles; Daemon heralds · *Mobs*
- **Void, The** — infinite; the Daemon's domain · *Dimensions*
- **Wafer in / chip out** — blank wafers enter via buffer lines; chips exit the front · *Lithography*
- **Wafer, The** — ±65,536; where every player starts · *Dimensions*
- **Wet Wafer Works (WWW)** — the studio · *Studio & design languages*
- **Zap, The** — the program's electrical manifestation · *The charge*
- **Zombie Process** — Organic-ruins boss · *Mobs*

*End of v2. Corrections welcome — the dictionary lives as long as the lore does.*
