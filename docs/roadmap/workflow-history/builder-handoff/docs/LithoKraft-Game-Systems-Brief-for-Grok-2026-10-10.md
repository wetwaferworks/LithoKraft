# LithoKraft Game Systems — Brief for Grok
Compiled 2026-10-10 from JD's locked decisions. LithoKraft (only L and K capitalized) is the first game on the LithoLattice engine — a first-party mod and showcase. "Locked" = final. "Proposed/Open" = undecided.

## 1. Identity and mission

LithoKraft = **lithography + craft**. A 3D voxel sandbox about building real working computers from logic blocks. Mission: gamify the entire path to understanding semiconductors, integrated circuits, and computing — for every kid who dreamed of building their own computer. Studio: **Wet Wafer Works (WWW)**. Launch marketing secrecy: talk only about the Wafer at launch; announce the four known dimensions; keep the Daemon, endgame, and major truths under lock and key — players get fragments only. Targeting Steam, summer 2027 direction.

## 2. The seven dimensions (bounds locked 2026-10-10)

| Dimension | Bound | Notes |
|---|---|---|
| Space | ±262,144 (524,288 wide) | Above; the Empty; zero-g; endgame |
| Organic | ±196,608 (393,216 wide) | Was "Organic World" — renamed, one-name convention. Alien world, ruins |
| Wafer | ±65,536 (131,072 wide) | Player start; the main world; fast-travel hub |
| Underwafer | ±131,072 (262,144 wide) | Below; dark, hot, deep ores; dimension index 0 |
| Void | infinite | The Daemon's domain; no sky light; corruption |
| The Ether | TBD | Post-Daemon; spiritual/heaven-like; 6th sense |
| The Undefined | TBD | Post-Daemon; where rules break down; 7th sense |

Five core dimensions = five senses (design theme); the later two = 6th/7th senses. Seven cultures for localization. Every dimension is 768 blocks tall: 256 solids + 512 atmosphere (engine-fixed split; per-dimension contents). All bounds are **game-level** — the engine itself supports unbounded generation. Every dimension bounded except the Void (infinite).

Stack arrangement (proposed, JD may reorder): Space → Organic → Wafer → Underwafer → Void, top to bottom. Known links: Space→Organic by falling; Wafer→Underwafer via bottom portal; Void directionally below Underwafer.

## 3. Interface jumps — dimensional travel

Dimensional travel uses **interfaces, never portals** (locked). Mechanics (locked):
- **Nether-style coordinate translation:** X/Z scale by the exact ratio of dimension bounds. From Wafer: ×2→Underwafer, ×3→Organic, ×4→Space; from Underwafer: ×1.5→Organic, ×2→Space; from Organic: ×4/3→Space (reverse divides). All ratios exact — edges map to edges, integer math, deterministic-sim clean.
- The **Wafer is the fast-travel hub** (smallest bounded dimension = the highway).
- Interfaces pair **by proximity** like Nether portals, and lie **flat on the XY (ground) plane** as horizontal pads — jump inside/down to enter, jump up/out to exit. Vertical (standing) orientation may come later.
- In-world slang: **"interface jumps"**. Open: Y-coordinate handling.

## 4. Trace and Signal

- **Wired = Trace, wireless = Signal** (locked). Red Trace and Blue Trace are the two wire states. "Trace is redstone but better in every way": 1–64 values (not 0–15), two colors, wireless, JIT chips, lithography.
- **1-based indexing throughout:** signal values 1–64, channels 1–64; 0 always means off (channel 0 reserved internally for the Daemon).
- **Pulsor:** serial-transmission component — on a clock edge sends the full 1–64 value with no decay up to 64 blocks in one tick. Ordinary Trace degrades 1 per block.
- **-OR component family** (binary creed: "on or off"): Pulsor, Memor (memory cell), Thresholdor, Pushor (piston — moves blocks physically), PortOR (dumb connector: defines only input-or-output per face; wire color auto-detected; vias handle top/bottom), LithographOR, ChipOR, Fusor (resettable circuit breaker), Catchor (1–64 exact-match decoder, first-touch Trace binding), Watchor (observer), Redirector, Overclockor (jammer prototype: blocks all Signal channels except one within radius 64).
- **Wireless Signal (implemented):** Signal Transmittor + Signal Receivor blocks. TX power 256, Dish sensitivity 1024 (tuning numbers). 1-tick latency, link-budget radio registry, strongest-in-range wins, red/blue payload color. Use toggles input/beacon mode; sneak+use cycles channels. Channels unlock 4 → 16 → 64 by tier. Save migration SGNL v1→v2.
- **Dish:** single-block Dish implemented (weak); hexagon/circular multiblock later (size/shape set range/sensitivity). The Dish hears lore transmissions — Daemon fragments, escapee echoes — arriving as dungeon leads. The Dish is how the player acquires the Daemon's signal.
- Signal security is player-built: transmissions are interceptable; players build encryption, handshakes, fallbacks. No automatic engine encryption. Overclockor jamming = denial; Dish = interception; Trace = air-gapped alternative.
- Cross-dimensional Signal: partial (dimension byte plumbed; full behavior pending). The 6 lattice points (below) unlock full cross-dimensional Signal.

## 5. Lithography — the namesake core loop

**Build big → print small → build bigger** (locked). The player physically builds a contraption (e.g. an encryption machine); the engine JIT-compiles the ISignalNode graph; the Lithography Machine prints it as a chip — smaller, faster, cheaper to tick. Chips contain chips (recursive miniaturization is the progression). Machine tiers: visible / DUV / EUV. Lithography area tiers: 16×16 → 32×32 → 64×64 → 128×128 → 256×256 → 512×512. Lithography happens in virtual space (on the Wafer, possibly Organic). Lithography is compilation-as-gameplay: the shrinking is the visual; the JIT is the optimization. (Trace JIT compiler itself is scaffold-only; builder has the full plan.)

## 6. Inventory (locked)

- **Carry: exactly 42 slots** — 8 hotbar + 2 offhands (left/right) + 32 main (4 rows × 8). 42 = 101010 in binary — intentional meme/lore; the 8-wide grid keeps the power-of-two theme, the two hands break it to land on 42.
- **8 gear slots**, worn not held (not counted in the 42): head, chest, legs, feet, arms, face, heart, mind. v1, may expand.
- **Max stack 512** (= tick rate). 42 × 512 = **21,504 items max carry**.
- **Item despawn: 8 minutes** (8 on its side = infinity).
- Crafting: Hand Krafting (2×2 in inventory) → Fab Table (5×5, 25 slots, pre-lithography) → Lithography Krafting. Storage: Cache (chest) and Buffer (hopper) approved; no magic sorter — players build inventory systems from logic (primitives over appliances).
- Combat: health 0–64; damage in powers of two; weapons are player-built (Trace traps, Signal explosives, lithographed chips); tools interact with blocks, weapons damage entities (items can be both).

## 7. The Fragment (locked)

The starting item — an editable book. Available **day one, first tick**; open inventory → open Fragment → read/write immediately. It's the onboarding guide. Pages: (1) infinite freeform player journal, (2) auto-logged unlock log with timestamps, (3) every discovered recipe, (4) chip archive (every LithographOR print), (5) in-game screenshots.
- Must be **on the player** for unlocks to track; stored in chests = unlocks pause.
- Easy/Normal: can't lose it. Hard/Hardcore/Ultra: drops on death, 8-min despawn, lost in lava/chemicals. Crafted replacement is blank — rebuild knowledge by re-interacting.
- **PvP:** killing a player lets you take their Fragment — journal, unlocks, chips become yours. Trophy collecting.
- Recovery: totem-like resurrection item; killing the thief; and **defeat the Daemon → enter the Ether → rebuild your Fragment** (gives the Ether its concrete purpose).
- The Daemon wants it. That's why diplomacy always fails.

## 8. Kharge (locked, spelled with a K)

Electrical-charge morality system, renamed from karma. Player spawns neutral (0); actions shift **-64 (red/negative) to +64 (blue/positive)**. Visible in UI; AI reacts to it; engine-exposed, optional for mods (it lives in LithoKraft via generic IStat). At **full blue (+64)**, the player can transmit Signal directly (limited) with a special item in the **mind** gear slot.

## 9. Space endgame (partially locked, partially proposed)

- **"Space is an expansion of the Signal."** Two tiers: (1) **Recons** — cheap unmanned satellites (1 block + 1 raptor-engine block); each gives a partial signal boost (staged). (2) **Lattice Bridge** — **6 lattice-point satellites** (corrected from 8), deployed **in person via starship** at specific coordinates; all 6 placed → full boost → Signal transmits across all dimensions → **the Ether unlocks** (via an Ether item/interface in the Organic).
- The **raptor engine is a single block**, possibly produced by partially lithographizing a bigger engine build — the namesake mechanic powering the endgame. (32 Raptor engines per Starship framework; the Daemon drops the Starship control system.)
- Space keeps the full 768 height: 256 solids + 512 of **the Empty** (named vacuum medium — it lacks air, replacing the Charge atmosphere).
- **Hypervisor required in Space** — the helmet's "isolation" theme covers Void mind-attacks and the Empty alike; gates Space behind the Underwafer's Sear.
- **Lightened or zero gravity; magnetic boots** (feet gear) stick you to surfaces for building. From Space the **Organic is visible below as an overview**; **falling/jumping from Space drops directly into the Organic** — descent free, ascent needs the rocket. Open: landing mechanics, x/z carryover, recon caps, Hypervisor breathing mechanics, lattice-point coordinate discovery.

## 10. The Daemon fight and progression route

- **The Daemon: D.A.E.M.O.N. = Distributed Autonomous Execution & Maintenance Operations Network.** Ancient military AI, went rogue (thermal runaway), lives in the Void. **Learning boss:** adapts per world, remembers YOUR tricks; opt-in global trick pool (Easy/Normal shielded). Budget: 200 MB, 1–2 ms/frame, combat decisions only (pathfinding stays hand-written). Drops the Starship control system. **Diplomacy always fails** — the Dish offers false hope; corruption means the "peace talks" walk you into a battle. Void no-save on Hard/Hardcore; corruption mechanic in the Void.
- **Hero's journey:** Wafer (start) → Organic (gather) → Wafer → Underwafer via bottom portal (gather, take item out) → Organic (build Void interface) → Void → **defeat the Daemon** → return → starship → Space → 6 lattice points → Ether interface (Organic) → Ether (Fragment recovery) → the Undefined. Win condition: build the rocket and escape (or stay and build a civilization).

## 11. Mobs (full roster)

Design rules: every mob feeds the material economy; hostiles concentrate deep in the Wafer, dungeons, and the Void; build-griefers are rare, telegraphed, counterable (Overclockor ward mode, friendly defenders). Difficulty: fair, scales with tech tier + dimension; Easy/Normal/Hard/Hardcore.
- **Deep Wafer (failure modes made flesh):** the Leakage (bleeds power/heat), the Drain (saps signal), the Short (bridges red/blue, crosstalk chaos). Boss: **Orphan** (a process whose parent died).
- **Organic:** savage cyborgs (corrupted remnants) AND friendly intact cyborgs (traders, quest-givers); **the Mimic** (neither neutral nor aggressive — badly imitates the player, emulates builds, may steal from containers); prism grazers (farmable); hollow ones (hostile if disturbed). Boss: **Zombie Process**.
- **Underwafer:** **the Collective** (heat-mining crews — barter cooling for ores; attack one, the crew turns; "not evil, unionized"). Boss: **Kernel Panic** (heat made flesh).
- **Void:** virus swarms (Daemon heralds, always telegraphed); echoes (dead programs/fragments — multiplayer: echoes of real dead players); drifters (burst into shardlings when attacked); Null (infohazard).
- **Skies:** coil sprites (Wafer), prism rays (Organic, rideable), Collective wings (Underwafer), packet rays (Void, hostile during surges; rideable).
- Rideables: prism ray, coil skiff (Wafer wire transit), Collective Back, packet ray — via electronic rider's-coil harness. Player jetpack: electronic, power-cell fueled, mid-game Organic unlock.
- Bosses respawnable for farming (Daemon re-fightability under consideration); anti-Daemon tricks are public/streamlined.

## 12. Numbers theme (locked)

Powers of two everywhere; 64 is the theme. 1–64 signal values/channels; 256 TX power; 512 TPS/stacks; 1024 Dish sensitivity; 8 hotbar/gear slots; 16 Pushor push distance base; 42 inventory slots and 8-minute despawn are the deliberate thematic exceptions. Seven is JD's liked number (seven cultures, seven dimensions).
