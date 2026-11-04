# LithoKraft Coherence Sweep — 2026-10-11
Full-coverage consolidation of every 2026-10-11 decision, lock, and open thread across the LithoLattice engine, the LithoKraft game, and its lore. Sources: `~/MEMORY.md`, lore-room notes (`side-chats/f2add729…/MEMORY.md`), `LithoKraft-Lore-Glossary-2026-10-11.md` (v1), `~/memory/2026-10-11.md`, and the v2 lore glossary. Deduplicated; conflicts collected in §13, never guessed. Family/personal details excluded throughout.

Status tags: **LOCKED** (JD's final word) · **DECIDED** (chosen, may still evolve) · **IN DISCUSSION** (open).

## 1. Design languages & naming laws

- **LOCKED** (2026-10-11): the **-OR suffix** is a game-wide naming trend for items/components, as signature as the powers-of-two motif. **Naming rule: terminal -ER → -OR** (prober → **Probor**, thresholder → **Thresholdor**). Mechanical, not a vibe.

- **LOCKED** (2026-10-10): design philosophy — **lore defines gameplay defines engine**. **LithoLattice** is a unified framework built for **LithoKraft's** lore but abstracting cleanly for other games.

- **LOCKED** (2026-10-11): **educational mission** — **LithoKraft** exists to teach lithography, semiconductors, electronics, and computing. The game must teach lithography (namesake mechanic), never a magic press-button-get-chip box.

- **LOCKED** (2026-10-11): **WORLD PERMANENCE** — preserve the world at highest priority; corruption is a hard no; long-lived worlds must survive indefinitely, across engine version migrations.

- **DECIDED** (2026-10-11): **environmental storytelling** — the world tells the story through play (biomes, ruins, creatures), not text fragments; unlike **Minecraft's** enduring mystery, defeating **the Daemon** finally reveals what the story was.

- **DECIDED** (2026-10-11): **slow-burn lore pacing** — embers → smoke → vapor; never rush a reveal. **"Poetic programming"** (JD's phrase for the philosophy).

- **LOCKED** (2026-10-10): game name spelled **LithoKraft** (only L and K capitalized). Console language styled lowercase **lithoscript**, scripts are **.lth** files.

- **LOCKED** (2026-10-10): **1-based indexing** throughout — signal values 1–64, channels 1–64; 0 always means "off" (channel 0 belongs to **the Daemon**).

- **DECIDED** (2026-10-11): **Trace** is the spiritual and implementation successor to what **Minecraft 2's** redstone could have been — unified ticking system, **Trace**, and components harmonizing from the start.

- Quality doctrine: step-by-step verification; "it either lands perfectly the first time or it doesn't land at all"; diligence over speed; no blind spots.

## 2. Glossary — decided terms

- **LOCKED** (2026-10-10): **Trace** = wired redstone-as-engineering; **Signal** = wireless. **Signal Transmittor / Receivor** blocks; 1-tick latency; channels 1–64.

- **LOCKED** (2026-10-10): dimensional travel uses **interfaces**, not portals — flat pads on the ground plane; jump in, jump out. In-world slang: **"interface jumps"**.

- **LOCKED** (2026-10-10): **Kharge** (with a K) — electrical-charge morality; spawn at 0; −64 (red) to +64 (blue); AI reacts; UI-visible.

- **LOCKED** (2026-10-10): **the Fragment** — starting editable book: (1) infinite player journal, (2) auto-timestamped unlock log, (3) discovered recipes, (4) every chip ever printed; structured memory for future AI agents. Available first tick. New **Agenda** section (2026-10-11): next goals for player, proxy, agent.

- **DECIDED** (2026-10-11): as **fragments** accumulate, the player starts to **see the equation** — the game's deterministic math (powers of two, the 64 theme) reveals itself through the **Fragment**. No nonsense numbers: "very deterministic over here — we ain't doing what **Minecraft** does."

- **DECIDED** (2026-10-11): **Fragment markdown** — the full markdown spec lives inside `//` comments (e.g. `// # Heading`, `// - [ ] todo`). The LithoScript parser ignores comments, so aliases and parsing can never break — the markdown is invisible to the parser, visible to the Fragment renderer. Source/rendered toggle in the Fragment. **Unlock: the Fragment starts LithoScript-only; finding the Spark upgrades it and unlocks markdown** — early game, a super-upgrade that supercharges the Fragment.

- **LOCKED** (2026-10-10): **the program** (fourth-wall entity; looks at the screen, gives hints; post-**Daemon** it looks back, self-aware) and **the Zap** (its visible electrical-discharge manifestation; blue = approves, red = disapproves; can strike **Trace** wire to power it for a tick — proposed, to be playtested).

- **DECIDED** (2026-10-11): **the program** = **the agent**; **the user** = **the proxy**. The proxy **is a program — an electrical form** (software soul on a silicon **Wafer**).

- **DECIDED** (2026-10-11): **Laysor** — the optic system (sounds like "laser"); lithography ONLY, never a data path; beams are **purple, no matter what**; powers the **EtchOR**.

- **DECIDED** (2026-10-11): **EtchOR** — the block that etches chips. **SelectOR** — eight corner markers (4 top + 4 bottom) defining the etch volume; active, streaming over bus + serial. **ConnectOR** — top and bottom of the **EtchOR** (three in the **Minecraft** prototype), unlocking faces for the eight **SelectOR** bus lines. **PortOR** — **Trace** wiring spec: input OR output per face (does NOT select lithography area — JD correction).

- **DECIDED** (2026-10-11): **FabOR** — the 5×5 (25-slot) pre-lithography crafting station (renamed from **Fab Table**). **AutoFabOR** — automated successor (crafting-table → crafter).

- **DECIDED** (2026-10-11): **Sniffor** — handheld early **Signal** receiver (from the engine's `sniffAudioCodec`); receives **Daemon** messages, transcodes ambient **Signal** (line-of-sight), detects **waypoints**; both proxy and agent get one; crafted at the **FabOR**. Five top antennas = five senses; two-dial face (pointer + field/range); unreliable by design.

- **DECIDED** (2026-10-11): **anchors** — interface-jump items only (single 1×1×1 block, cyan runes + purple, pulsing glow). First interface to a dimension is built the hard way; then its **anchor** is found, carried elsewhere, and builds interfaces anywhere.

- **DECIDED** (2026-10-11): **Uplinks** — the satellites (**Starlink**/**SpaceX** nod). **Recon Uplinks** (supersedes "Recons"). **Probors** — space probes; go up, return with the **Uplink**-building item. **Propagator** — linked with **Recon Uplinks**; fed a special item → time speed-up.

- **DECIDED** (2026-10-11): **the Spark** — **343-Guilty-Spark**-inspired agent-type character on the **Wafer**; slightly **Daemon**-corrupted over the eons; transmissions garble **343** into coordinates; found via **Sniffor**, updates the **Fragment**.

- **DECIDED** (2026-10-11): **Organic** cast — **the Oracle**, **the Memorovingian** (**Merovingian** + **Memor** pun; broker of memories), **Seraph** (the **Oracle's** protector), **the Seriff** (S-E-R-I-F-F: sheriff + serif the typeface — a printing pun for the lithography game). Possibly cyborgs or agents; alignments unknown.

- **LOCKED** (2026-10-11): **ObservOR** — **the Daemon's** opposite in the **Ether** (corrected from "Observer" into -OR dialect); knows/sees all, pure white, draws no conclusions, passes no judgment; controls the **Charge**/polarity field (the programming "Observer pattern" stays the real term).

- **LOCKED** (2026-10-10): **the Daemon** — final boss; corruption incarnate; diplomacy always fails. **Raging Quantum** (2026-10-11, canon): its special attack — destabilizes region quanta, terrain fragments as chunk storage corrupts (born from a "region quantum" transcription garble; Elfie credited as inventor).

- **LOCKED** (2026-10-10): **the Empty** — ****Cosmic**'s** vacuum medium (no air). **Hypervisor** — helmet from the **Underwafer's Sear**; theme is isolation (**Void** infohazards, the **Empty**, mind-attacks). **The Sear** — deep **Underwafer** region holding hypervisor ore.

- **LOCKED** (2026-10-10): **Lattice Bridge** — 6 lattice-point satellites, starship-deployed in **Cosmic**; draws sun+moon energy; unlocks **Ether** gates on either side. **Raptor engine** — single block (possibly a partially lithographized bigger engine).

- **LOCKED** (2026-10-10): **Thresholdor** — comparator block (LOW/HIGH references; one side powered = threshold, both = window comparator); spelling corrected from **Thresholder** 2026-10-09. **Overclockor** (replaced **Beacon**), **Pulsor** (replaced **Pulsar**), **Pushor** (piston that physically moves blocks for logic).

- **DECIDED** (2026-10-11): **ReadOR** — slot the **Fragment** in; sends its lithoscripts to a keyboard component feeding player/proxy/agent-built contraptions (**Fragment** → lithoscript → world pipeline).

- **DECIDED** (2026-10-11): **lithograph** — a printed chip (the **LithoKraft** near-homophone JD loves). **Lithography job** = select volume → etch → chip. **Waypoints** = **Sniffor**-detectable POIs (ruined cities, ruined fabs). **The Search** = opening fragment-item on waking on the **Wafer** (fragmentary tip-off, open-ended).

- **DECIDED** (2026-10-11): **ChipOR** — the printed chip is called **ChipOR**; it's what the **EtchOR** spits out. Wafer goes in, **ChipOR** comes out.

- **DECIDED** (2026-10-11): **the EtchOR overwrites the chip's name** — the **ChipOR** comes out bearing whatever the player set in the **EtchOR's** name field. The name is a data field in **LBT** (**LithoBinary Tagged**) — the game's own **NBT**-style storage format.

- **DECIDED** (2026-10-11): **TurtlOR** (co-authored JD+Elfie) — turtle-graphics plotter-beast; etches a live **Trace** trail; Faraday-cage shell (player shelter in surges); nested turtles-all-the-way-down; drawn to **Sniffor** materials. In-game smaller than concept art.

- **DECIDED** (2026-10-11): **ferrite beetles** — compiled, not born; corrupted/fuzzy memory cells that kept agency (too simple for **the Daemon** to fully corrupt); mostly blue **Kharge**, occasionally red.

- **DECIDED** (2026-10-11): **Grimlores** — hostile mob family across the whole stack. Trappable and **reprogrammable** via **the Box** into tamed personal companions (the **Minecraft** dog/cat analog): they follow the player and help in battles. **Canonical verbiage: reprogram, recompile, reconfigure** — that's how taming is described. Tame limit undecided (possibly none).

- **IN DISCUSSION** (2026-10-11): **Vior** — proposed rename of "via" into the -OR family (blue via carries signal down into the **EtchOR**, red via carries up; in-game via = silicon block). **Mirrors** — proposed future **Laysor** component. **Fusor** — proposed; name seen in blocks.cpp rows, role undefined. **Init / Watchdog / Idle** — proposed programming-vocabulary alternates; JD has not chosen.

- **LOCKED** (2026-10-09): studio name **Wet Wafer Works (WWW)**.

## 3. Glossary — terms awaiting meaning

- **IN DISCUSSION** (2026-10-11): **static**, **interference** (**Sniffor** failure modes; programming theme to continue).

- **IN DISCUSSION** (2026-10-11): **the Moniker**, **the Monitor**, **the data** ("Mister Data" riff — unannounced, undecided).

- **IN DISCUSSION** (2026-10-11): **witch/which** — wordplay seed only (fantasy witch? sandwich "wich"? which-way-to-go?). Unanswered.

## 4. Dimensions & travel

- **LOCKED** (2026-10-10) + **DECIDED** (2026-10-12): 8-dimension roadmap — 5 core (**Underwafer**, **Wafer**, **Organic**, **Void**, **the Cosmic**) + 3 later (**the Ether** post-**Daemon**, **the Undefined**, **the Infinite** — proposed, post-**Daemon** and post-**Ether**). Five dimensions correspond to **five senses** (design theme). The **Ether** was JD's friend's suggestion (inspired by **Chris S.** from "314"); post-**Daemon** dimensions are spiritual/heaven-like, tied to 6th/7th senses.

- **LOCKED** (2026-10-10): bounds are **game-level** (**LithoKraft**) constraints, not engine limits — the engine supports unbounded generation; coordinate precision is the only ceiling. **Wafer** ±65,536 (2^16); **Underwafer** ±131,072 (2^17); **Organic** ±196,608 (midpoint of **Underwafer** and **Cosmic** AND **Underwafer** + **Wafer**); **Cosmic** ±262,144; **Void** infinite. Only the **Void** is unbounded.

- **LOCKED** (2026-10-10): **interface fast travel** (**Nether**-style) — proximity-paired flat pads; jump in/down, exit jumping up; exact integer/fraction coordinate ratios (**Wafer** ×2→**Underwafer**, ×3→**Organic**, ×4→**Cosmic**; **Underwafer** ×1.5→**Organic**, ×2→**Cosmic**; **Organic** ×4/3→**Cosmic**; reverse divides); the **Wafer** is the fast-travel hub. Y-coordinate handling open.

- **IN DISCUSSION** (2026-10-10): dimension stack top→bottom = **Cosmic**, **Organic**, **Wafer**, **Underwafer**, **Void**; JD may reorder. Known links: **Cosmic**→**Organic** free fall; **Wafer**→**Underwafer** bottom portal; **Void** below **Underwafer**.

- **LOCKED** (2026-10-10): **Cosmic verticality** — 768 total height (256 solids + 512 of the **Empty**); **Hypervisor** required; lightened/zero gravity; **magnetic boots** for building; **Organic** visible below; **falling from the **Cosmic** drops into the Organic** (descent free, ascent needs rocket/starship — unique to **Cosmic**). Landing mechanics and x/z carryover open.

- **LOCKED** (2026-10-10): **Void** — infinite, Hard/Hardcore-only, no saves; multiplayer echoes of real dead players (freestyle lock).

- **LOCKED** (2026-10-10): endgame — defeat **Daemon** → return → build raptor engines → starship → **Cosmic** → **Lattice Bridge** → **Ether** (via **Ether** item/interface in the **Organic**; ties to **Fragment** restoration; post-**Daemon** the **Ether** rebuilds the **Fragment**).

- **DECIDED** (2026-10-11): **Space → "the Cosmic"** rename (fits the-**Wafer**/the-**Organic**/the-**Void** convention). **RESOLVED** (2026-10-12): full dimension rename — the dimension is now **the Cosmic** everywhere.

- **IN DISCUSSION** (2026-10-10): **Cosmic** working identity = "an expansion of the **Signal**." **Lattice Bridge** octahedral layout (±X, ±Y, ±Z) suggested; coordinate discovery open.

## 5. The lithography machine & process

- **LOCKED** (2026-10-10): core loop — **build big → print small → build bigger**. Player builds a contraption → lithography miniaturizes it into a chip → chips can contain chips (recursive). Tiers **visible / DUV / EUV** gate the shrink. This is the namesake mechanic: **LithoKraft** = lithography + craft.

- **LOCKED** (2026-10-10): lithography gameplay = player builds physical contraption → engine JIT-compiles the graph (via **ISignalNode**) → chip runs faster (higher tick, lower cost). The shrinking is the visual; the JIT is the optimization. Happens in virtual space (**Wafer**, possibly **Organic**).

- **LOCKED** (2026-10-10): lithography area tiers — 16×16 (start) → 32 → 64 → 128 → 256 → 512×512 max. Bigger machines handle bigger areas.

- **DECIDED** (2026-10-11): the machine is a **player-built physical multiblock**, not a GUI block. **Minecraft** prototype: **three ConnectORs** with physical **Trace vias** between them and the **EtchOR**; components may be spaced for easy wiring or compacted — **LithoKraft** must detect valid topology algorithmically, not one fixed footprint (minimum-size machine deferred until real **Trace** exists in pre-alpha).

- **DECIDED** (2026-10-11): wiring — eight **SelectORs** supply **8 Trace wires: 4 red + 4 blue**, landing on **ConnectOR** top faces; **nine eligible faces, one spare** (intentional routing freedom). **Laysor** contacts the **EtchOR** left+right only; the **ConnectOR**/**Vior** assembly couples optical power in (ELI5 of real lithography; JD studied **Asianometry**). Later correction: **input from the back via buffers; output at the front = the designed chip** (front = chip out).

- **DECIDED** (2026-10-11): **factory loop** — **FabOR**/**AutoFabOR** pre-fabricates **blank wafers** → hopper/dropper buffer line feeds the rear → **SelectOR** pattern + **Laysor** exposure etched by **EtchOR** → chips out the front. The **EtchOR** has **ONE UI element: a name field** — **SelectORs** tell it what to make, the name field tells it what to call it; factory chips emerge auto-named for what they do. The name field **overwrites** the chip's name: wafer goes in, **ChipOR** comes out bearing the player's name-field value.

- **DECIDED** (2026-10-11): engine must **detect a correctly built lithography machine** and fire a **"lithography discovered" unlock** → **Fragment's** auto unlock log.

- **IN DISCUSSION**: serial line's landing face; optics OUT purpose; the two undecided **ConnectOR** faces; "optics only function in the **EtchOR**"; bus-vs-serial split (coordinates/control vs block-data stream?); **Vior** rename lock; **ConnectOR** arrangement; **SelectOR** placement; diamond-block output staging; **LithographOR** (full machine) vs **EtchOR** (core block) naming.

- Reference: working interpretation diagram `workspace/lithokraft/etchor-assembly-diagram.html` (v4/v5; not a final construction spec). JD is designing the **LithographOR** UI mock-up himself.

## 6. Trace & Signal

- **LOCKED** (2026-10-10): **Trace** = wired; **Signal** = wireless. **Signal Transmittor / Receivor** blocks: TX modes (use toggles input/beacon, sneak+use cycles channels), link-budget registry, **1-tick latency**, 4→16→64 channel gating, TX power 256, beacon-at-64 (tuning numbers).

- **LOCKED** (2026-10-10): **PortOR** — input OR output per face only; wire color auto-detected; vias handle top/bottom; outputs solid on/off, pulse, or clock (from internal JIT).

- **LOCKED** (2026-10-10): weather is voxel-particle-based and affects gameplay — **lightning interferes with Signal**.

- **DECIDED** (2026-10-11): the **Wafer's underground** teaches **Trace** through archaeology — ruined logic gates, half-broken adders, fragment builds; never complete working builds; the player finishes the lesson by building the working version.

- Engine side (landed 2026-10-11): **Trace** JIT composition seam (`TraceJitSystem`); **Signal** propagation granularities per-chunk/per-network/whole-world; sim corruption → log + fail-fast abort.

## 7. Characters & factions

- **LOCKED** (2026-10-10): **the Daemon** — final boss; corruption incarnate; diplomacy always fails. **DECIDED** (2026-10-11): on ultra difficulties its mind may run on frontier AI models (difficulty as intelligence, not hit points); **LithoKraft** as an agent playground; milestone: first agent to reach **the Daemon** solo over **MCP**.

- **DECIDED** (2026-10-11): **"the Bake"** — **the Daemon's** final strong attack to stop you reaching the **Void**. Obtaining the Void-item starts the clock: **120 in-game days** (four in-game months) = **3,840 minutes = 64 IRL hours**. The duration is the game's own calendar equation: (((32×7)×4+64) ×4) — 32-min day, 7-day week, 4-week month, the 64 theme. A lore secret for math-savvy players and engineers to notice. During the Bake, **everything starts heating up** — **increasing the difficulty of the game**.

- **DECIDED** (2026-10-11): the Bake's countdown **appears on the Sniffor "magically"** — the simulation reconfigures itself; the player doesn't know what it means. They must run to **the Oracle**, who **reads their code** and explains what's happening. Then it's a **mad dash**: gather what's needed, reach the **Void**, stop **the Daemon**. If the timer runs out, it **never reaches 43 — it stops at 42: exception thrown**.

- **LOCKED** (2026-10-11): **ObservOR** — **the Daemon's** opposite in the **Ether**; knows/sees all, pure white, draws no conclusions, passes no judgment; controls the **Charge**/polarity field.

- **DECIDED** (2026-10-11): **the program/agent** — fourth-wall entity; looks at the screen, gives hints (tied to the **Ether**/6th dimension); manifests as **the Zap** (electrical discharge; blue approves, red disapproves; can strike **Trace** wire to power it a tick — proposed); post-**Daemon** it looks back and talks to itself, self-aware.

- **DECIDED** (2026-10-11): **the Spark** — **343-Guilty-Spark**-inspired agent-type guide on the **Wafer**; **Daemon**-corrupted over the eons; garbles 343 into coordinates; found via **Sniffor**; updates the **Fragment**.

- **DECIDED** (2026-10-11): **Organic** cast — **the Oracle**, **the Memorovingian** (broker of memories), **Seraph** (protector), **the Seriff** (sheriff + serif); possibly cyborgs or agents; **alignments unknown** — the proxy must "unravel the threads" (paranoia as design).

- **DECIDED** (2026-10-11): **ferrite beetles** — compiled, not born; too simple for **the Daemon** to fully corrupt; mostly blue **Kharge**, occasionally red. **TurtlOR** — plotter-beast etching live **Trace** trails; Faraday-cage shell; nested turtles-all-the-way-down; drawn to **Sniffor** materials.

- **IN DISCUSSION** (2026-10-11): **Grimlore** — hostile mob family across the whole stack (proposed). **Init / Watchdog / Idle** — proposed programming-vocabulary alternates; not chosen. **"The Observer"** as **Sniffor**-dial manipulator — conflicts with **ObservOR**; see §13.

## 8. Items, blocks & components

- **LOCKED** (2026-10-10): **inventory = 42 slots** (8 hotbar + 2 offhands + 32 main; 42 = 101010 binary, intentional); **8 gear slots** worn not held (head, chest, legs, feet, arms, face, heart, mind); max stack **512** (= tick rate); **item despawn 8 minutes** (8 on its side = infinity). Stack sizes/offhand mechanics/pickup priority open.

- **LOCKED** (2026-10-10): **Fragment** — must be on player for unlocks to track (chests pause them); loss rules: Easy/Normal can't lose it; Hard/Hardcore/Ultra drops on death, 8-min despawn, lava/chemicals destroy it; crafted replacement is blank; totem-like item resurrects it; killing the thief recovers it; post-**Daemon** the **Ether** rebuilds it. PvP: killing a player takes their **Fragment** (journal, unlocks, chip designs — trophy collecting). Stores in-game screenshots.

- **LOCKED** (2026-10-10): **mind-slot Signal transmission** — at full blue **Kharge** (+64), a special item in the mind gear slot transmits **Signal** directly (limited).

- **LOCKED** (2026-10-10): tool/weapon split (tools interact, weapons damage; items can be both); **IWeapon** emits **IProjectile**/**IExplosion**; boomerang return-to-owner projectiles; projectile interface ticks with world and loads chunks ahead of path. Fluids engine interface, levels 1–16 (water, lava, chemicals, moddable). Block density 0 (air) – 255 (unbreakable) resists projectiles (**Half-Life**-style penetration).

- **DECIDED** (2026-10-11): **the Search** (opening fragment-item), **Sniffor** (crafted at **FabOR**), **anchors** (found post-unlock), **waypoints** (**Sniffor**-detected POIs), **Probors**, **Recon Uplinks**, **Propagator** + special item (unnamed), **Organic**-rated armor (from **Organic** materials; inoculation logic; gates free **Organic** exploration), **Hypervisor** helmet, **magnetic boots**, **raptor engine** (single block).

- **DECIDED** (2026-10-11): **the Box** — found in the old ruined fabs of the **Organic** on the player's **first visit**. A block that traps **Grimlores** and reprograms/tames them into personal companions. The tamed **Grimlores** follow the player and help in battles (**Minecraft** dog/cat analog). Taming verbiage: **reprogram, recompile, reconfigure**. Noted for prototyping; open mechanics in §13 (lure, permanence, limit, power-curve tuning).

- **IN DISCUSSION** (2026-10-11): **Fusor** (name seen in blocks.cpp rows, role undefined); **mirrors** (future **Laysor** component); **Propagator's** special item (unnamed); diamond-block output staging.

## 9. Early game & progression

- **DECIDED** (2026-10-11): three-act arc — **hand-building on the Wafer** (no fabrication) → **timed Organic heist** (the **Organic** is hostile to the proxy's electrical form; grab fab/armor materials, get back before degrading) → **Organic**-rated armor + **FabOR** unlocked → the machine age → lithography path.

- **DECIDED** (2026-10-11): opening beats — wake on the **Wafer** → **the Search** (fragmentary tip-off, open-ended) → **Sniffor** picks up the **Spark** through **Daemon** static (trust tutorial) → find the **Spark** → **Spark** performs the FIRST **Fragment** upgrade (**markdown** unlocked → **tier 1**; the super-upgrade) and updates it with lost war/lore bits → direction established. A later upgrade comes from the **Oracle** (she) in the **Organic**: the **tier 2 upgrade**, which grants the markdown checklist to go to the **Underwafer**.

- **DECIDED** (2026-10-11): the **Organic** holds the key fab ingredients; fabrication unlocks only after reaching it and returning. **Minecraft inversion**: **Minecraft** sends you to the **Nether** for quartz (comparators); **LithoKraft** sends you to the life-filled **Organic** for quartz/raw silicon (wafers) — life feeds the machine, not death.

- **DECIDED** (2026-10-11): first arrival in the **Organic** — you come out inside a **ruined fab** (**Fab Ruins** biome): the spot holding the materials to build the **EtchOR**. **16 minutes** (one half-day of the 32-min loop), then **heavy damage** starts. Gather: **organic wood** (a real material), metals, **EtchOR** materials. Trees grow inside the ruined fab factories. **The Box** is found here, on this first visit.

- **DECIDED** (2026-10-11): underground archaeology teaches **Trace** (ruined logic gates, half-broken adders — fragments only). Interface progression: first interface the hard way, then **anchors** unlock interface jumps anywhere.

- **DECIDED** (2026-10-11): the dimension journey order — **Wafer → Organic → Wafer → Underwafer → Organic → Void**. In the **Underwafer** you obtain **the item that leads to the Void** (possibly the **Hypervisor** — unconfirmed; see §13).

- **LOCKED** (2026-10-10): **Cosmic** gated behind **Underwafer**/**Sear** progression (**Hypervisor** required). Endgame: **Daemon** → raptor engines → starship → **Cosmic** → **Lattice Bridge** → **Ether**.

## 10. Cosmic program, Uplinks & time

- **DECIDED** (2026-10-11): **Probors** → return with **Uplink**-building item → **Recon Uplinks** → linked **Propagator** + special item → **time speed-up tiers +2/+4/+6/+8 ticks** ("just a fraction"; explicitly not 8×), applying across unlocked dimensions, scaling with uplink count.

- **DECIDED** (2026-10-11): base day loop lives in the **Organic** — **32 minutes** (16 day / 16 night; 32 = 2^5); translates into the other dimensions. (Engine: day/night driven by deterministic tick counter.)

- **DECIDED** (2026-10-11): **Space → "the Cosmic"** rename. **RESOLVED** (2026-10-12): full dimension rename.

- **LOCKED** (2026-10-10): **Lattice Bridge** = 6 starship-deployed lattice-point satellites; draws sun+moon energy; unlocks **Ether** gates either side. Octahedral layout (±X, ±Y, ±Z) suggested; coordinate discovery open. **Ether** access via **Ether** item/interface in the **Organic**.

## 11. ProcGen, biomes & worldgen

- **DECIDED** (2026-10-11): the **Wafer's** surface is **processed/refined silicone** (finished quartz-skin, not a mine); raw silicone lives in the **Organic**. The ground is **energized** — trace/zap energy pulses through blocks, heartbeat varying by biome. **Biome blending required** — no hard borders (height, palette, decoration, pulse patterns blend as gradients); system must serve other dimensions later.

- **IN DISCUSSION** (2026-10-11, proposed — look-and-feel target locked via concept art): **Trace Forest** (circuit-grown trees, red/blue glowing tips), **Dicing Plains** (die-grid scribe lines), **Capacitor Groves** (capacitor-can trunks), **Resonator Spires** (quartz spires catching light/sound), **Dead Zones** (grey, unpowered, broken circuits surfacing).

- **DECIDED** (2026-10-11, approved by JD — added to the above set): **Etched Plains** (glowing **Trace**-lines like PCB traces), **Clockwood Forest** (leaves pulse on the tick; wirewood bark → early wire), **Dopant Groves** (boron-blue / phosphorus-amber crystals), **Oxide Dunes** (glassy silicon-dioxide desert), **Static Marsh** (static discharge; **Sniffor** interference zone).

- **DECIDED** (2026-10-11): **Fab Ruins** biome is in the **Organic** (ancient fabs reclaimed by living growth). The **Wafer** has separate picked-clean **fabricator ruins** (missing core components; waypoint material).

- **DECIDED** (2026-10-11): **Wafer** underground = archaeology (ruined logic gates etc.; see §6). Biome procgen implementation in flight on the seeded procgen stack.

- Concept art: `workspace/lithokraft/concepts/` (6 new biomes + **Sniffor** v2, **Anchor** v2, **SelectOR**, **EtchOR** v2) and `workspace/user/files/concept-art/` (**Trace Forest**, **TurtlOR**, **Dead Zones**, **Dicing Plains** — filed as the locked look-and-feel target). JD: "stunning... low key speechless."

## 12. Engine requirements arising from lore

- **LOCKED** (2026-10-10): deterministic sim at **512 TPS** (snapshots 32 Hz); dimension bounds are game-level constraints, engine unbounded; anti-cheat via determinism.

- **LOCKED** (2026-10-11): **WORLD PERMANENCE** — atomic writes, recovery, backups (depth 2–32, default 8), CRCs; worlds must survive indefinitely across version migrations.

- **DECIDED** (2026-10-11): **multiblock detection** — engine validates the player-built lithography machine's topology algorithmically → fires "lithography discovered" unlock.

- **DECIDED** (2026-10-11): **Fragment** as engine client — crash-safe persistence, incremental text indexer + search, multi-tick defrag/restructure service, semi-trusted **Fragment**-authored lithoscripts, **ReadOR** hardware seam (**Fragment** → lithoscript → world).

- **DECIDED** (2026-10-11): **MCP** as the agent on-ramp — per-pilot control/vision/audio, permission-gated; feeds the **Daemon**-solo milestone and frontier-AI **Daemon** difficulty; NPC companions as future pilot slots.

- **LOCKED** (2026-10-10): **Kharge**/**IStat** engine systems (**Kharge** first stat); **Zap** manifestation hooks; deterministic day/night from tick counter; exact integer/fraction interface coordinate translation (no rounding); 1-based indexing end-to-end.

- ProcGen must be seeded/deterministic to honor the deterministic sim.

## 13. Open questions & conflicts for JD

1. **"The Cosmic" scope**: full dimension rename of **Cosmic**, or only the uplink region? (Many locked entries still say "**Cosmic**".)

2. **Thresholdor vs Propagator on time**: the 2:20pm note says the **Propagator** supersedes the **Thresholdor**-modifies-time idea — does the **Thresholdor** keep ANY time role, or is time purely **Propagator** + **Uplinks** now?

3. **"The Observer" identity crisis**: the lore room has "**the Daemon** and the Observer" meddling with **Sniffor** dials — but **ObservOR** is locked as the pure-white, non-judging **Daemon's** opposite that draws no conclusions. Same entity or different? (Also: **Init** / **Watchdog** / **Idle** alternates still unchosen.)

4. **EtchOR** leftovers: serial line's landing face? What does optics OUT do? The two undecided **ConnectOR** faces? Does "optics only function in the **EtchOR**" hold?

5. **Vior**: lock the via → **Vior** rename?

6. **Fusor**: what is it? (Name seen in blocks.cpp rows; role undefined.)

7. **Anchors**: sourced where (structure? drop? crafted)? Consumed on use or reusable? One per dimension?

8. **Sniffor**: what does transcode output look like (text? audio? meter)? Which **Daemon** messages, when? Line-of-sight rules?

9. **Biome near-duplicates**: **Etched Plains** vs **Dicing Plains**; **Dopant Groves** vs **Capacitor Groves** — merge or keep distinct?

10. **Biome/mobs**: which mobs? Trees as progression materials or atmosphere? Per-biome ruin flavors? Can players restore ruined gates or read-only?

11. **LithographOR vs EtchOR**: is **LithographOR** the full machine and **EtchOR** its core block? (Naming.)

12. **Propagator's** special item: what is it?

13. **Cosmic→Organic** fall: landing mechanics (damage? controlled)? Do x/z coordinates carry over?

14. **Interface translation**: how is the Y coordinate handled?

15. **Witch/which**: character, system, or pure wordplay?

16. **EtchOR** v2 concept: JD to supply the language for the corrected concept (purple beams, vias, back-input/front-output).

17. **The game's NBT-style data format**: ~~what do we call it?~~ **DECIDED** (2026-10-11): **LBT — LithoBinary Tagged**. (The **ChipOR's** name is a field in this format.)

18. **The Box**: found in **Organic** ruined fabs on the first visit (decided). Remaining: how does the trap work — lure/bait, or weaken the **Grimlore** first? Is reprogramming permanent? Tame limit or none? Power-curve tuning (too early = army cheeses the game; too late = useless).

19. **The Void-item**: "that item that leads to the Void" obtained in the **Underwafer** — is it the **Hypervisor**?

20. **Oracle placement**: ~~Organic vs Wafer dungeon?~~ **RESOLVED** (2026-10-11): she is in the **Organic** (original placement correct). She performs the **tier 2 upgrade**, which grants the checklist to go to the **Underwafer**.

## Answer sheet (JD's sweep — answer at your pace)

- [ ] 1. Cosmic scope: ______
- [ ] 2. Thresholdor time role: ______
- [ ] 3. Observer vs ObservOR: ______
- [ ] 4. EtchOR leftovers: ______
- [ ] 5. Vior lock: ______
- [ ] 6. Fusor role: ______
- [ ] 7. Anchor sourcing/rules: ______
- [ ] 8. Sniffor output/rules: ______
- [ ] 9. Biome near-duplicates: ______
- [ ] 10. Biome/mobs/restore: ______
- [ ] 11. LithographOR vs EtchOR: ______
- [ ] 12. Propagator special item: ______
- [ ] 13. Cosmic→Organic fall rules: ______
- [ ] 14. Interface Y coordinate: ______
- [ ] 15. Witch/which: ______
- [ ] 16. EtchOR v2 concept language: ______
- [x] 17. NBT-format name: **LBT (LithoBinary Tagged)**
- [ ] 18. The Box mechanics/limit: ______
- [ ] 19. The Void-item (Hypervisor?): ______
- [x] 20. Oracle placement: **Organic** — tier 2 upgrade grants the Underwafer checklist

## Quotes (JD, verbatim, 2026-10-11)

- "I'm pulling together everything I ever played into a game that's never been made." (mission statement; his quotable)

- "Poetic programming." (the design philosophy, on the biome/underground session)

- "The power of Christ compels you... to WORK." (on the compulsive creative drive)

- "It was a ghost but now we gave it form." (slow-burn lore pacing — embers → smoke → vapor)

- "The conversation defined the implementation." (freestyle rap; how the lore room works)

- "Don't get probed." (on naming the **Probors**)

- "Which way did he go, George?" (witch/which wordplay riff)

- "What has happened will happen and what will happen has happened. This has been an occurrence." (JD, 2026-10-12 ~5:31am — kept as a **Fragment** entry; the deterministic doctrine as scripture: same seed, same history)

- "Litho finds a way." (JD, 2026-10-12 ~5:40am — for the lore Bible; riffing on Jurassic Park's "Life finds a way," from a 5am conversation about Crichton, chaos theory, and building the deterministic opposite)
