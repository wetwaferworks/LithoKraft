# LithoKraft Lore Glossary
*2026-10-11 — the world-terms companion to the engine glossaries. Locks marked; discussion items marked.*

## New 2026-10-11
- **lithoscript** (lowercase) — the console scripting language: commands, cvars, aliases, `exec`. Half-Life 1's console model, built for LithoLattice. Spoken in the debug console, scriptable by players, bound into AngelScript and the engine. Named 2026-10-11.
- **.lth file** — a plain-text LithoScript script file (our `.cfg`). Players write and modify them.
- **ReadOR** (in design) — a new -OR component: slot your Fragment inside and it sends your LithoScripts to a keyboard, which feeds them into contraptions built by you, the proxy, or the agent. The Fragment as script cartridge.
- **Raging Quantum** — the Daemon's special attack: it destabilizes region quanta, and terrain fragments as chunk storage corrupts. Invented by Elfie 2026-10-11, born from a voice-transcription garble of "region quantum." Canon.
- **Wet Wafer Works (WWW)** — the studio name. Locked 2026-10-09.

## The world
- **LithoKraft** — the game. Lithography + craft. Spelled with only the L and K capitalized. A first-party mod on the LithoLattice engine.
- **The Wafer** — the initial dimension: a 2D side-scrolling silicone wafer, ±65,536 blocks. Where every player starts.
- **The Underwafer** — below the Wafer, ±131,072. Holds the Sear in its depths.
- **The Organic** — ±196,608. The living dimension; visible below from Space.
- **Space** — ±262,144. Light or zero gravity, 512 blocks of the Empty above the solids. Entered by rocket/starship; left by jumping.
- **The Void** — infinite, below everything. Hard/Hardcore only. No saves.
- **The Ether** — post-Daemon dimension, spiritual/heaven-like. Reached via the Lattice Bridge.
- **The Undefined** — the seventh dimension. Unwritten.

## Travel
- **Interface** — dimensional travel uses interfaces, not portals. Flat pads on the ground plane; jump in, jump out.
- **Interface jump** — in-world slang for interface travel.
- **Anchor** — the item that lets you build an interface to a dimension from anywhere, once you've first reached it the hard way. Anchors are for interface jumps only.

## The charge
- **Kharge** (with a K) — electrical-charge morality. Spawn at 0; actions move you from -64 (red) to +64 (blue). The AI reacts; it's visible in the UI.
- **The Zap** — the program's visible manifestation: electrical discharge. Blue means it approves; red means it disapproves.
- **The program** — the fourth-wall entity. It can look at the screen and give hints; after the Daemon, it looks back and talks to itself.

## The Fragment
- **The Fragment** — your starting item: an editable book holding your journal (infinite pages), the auto-timestamped unlock log, every recipe you've discovered, and every chip you've ever printed. Open inventory → open Fragment → read/write from the first tick.
- **The Agenda** (proposed) — a Fragment section defining next goals and steps for the player, the proxy, and the agent: where they're heading, where they come from, what's next.
- **Mind-slot transmission** — at full blue Kharge (+64), a special item in the mind gear slot lets you transmit Signal directly.

## Lithography (the namesake loop)
- **Build big → print small → build bigger.** You build a contraption, the machine miniaturizes it into a chip, chips can contain chips.
- **LithographOR** — the lithography machine block family.
- **EtchOR** — the single block that performs the etch. Powered by the Laysor; fed by the SelectORs.
- **SelectOR** — eight corner markers (4 top + 4 bottom) defining the etch volume. They actively stream the volume to the EtchOR over a bus plus a serial line.
- **ConnectOR** — sits on the EtchOR's top and bottom, unlocking eight faces for the eight SelectOR bus lines.
- **Laysor** — the optic system (sounds like "laser"). Lithography ONLY — never a data path. Purple hue.
- **PortOR** — the Trace wiring spec: input OR output per face. Does not select lithography area.
- **FabOR** — the 5×5 crafting station. **AutoFabOR** — its automated successor.
- **Lithograph** — a printed chip. (Also the pun JD loves: *lithograph* and *LithoKraft* sound nearly identical.)
- **-OR** — the game-wide design language: components and blocks end in -OR, as signature as the powers of two.

## Signal & Trace
- **Trace** — wired redstone-as-engineering. **Signal** — wireless. Locked terminology.
- **Signal Transmittor / Signal Receivor** — wireless blocks; 1-tick latency; channels 1–64 (1-based; 0 is off, channel 0 belongs to the Daemon).

## Space program
- **The Empty** — Space's vacuum medium. No air; the Hypervisor seals you against it.
- **The Hypervisor** — the helmet from the Underwafer's Sear. Isolation is its theme: mind-attacks, the Void's infohazards, the Empty alike.
- **The Sear** — the deep Underwafer region holding hypervisor ore.
- **Recons** — cheap unmanned recon satellites; each gives a partial signal boost.
- **Raptor engine** — a single block; a partially lithographized bigger engine.
- **The Lattice Bridge** — six lattice-point satellites forming a bridge across Space. Complete it and the Signal crosses all dimensions, unlocking the Ether.

## The endgame
- **The Daemon** — the final boss. Corruption incarnate; diplomacy always fails. Upgraded options and thinking on high-end hardware.
- **The Ether gates** — unlocked from either side of the Lattice Bridge, post-Daemon.
