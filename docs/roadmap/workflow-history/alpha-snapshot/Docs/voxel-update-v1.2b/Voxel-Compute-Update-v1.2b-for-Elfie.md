# Voxel Compute: Update v1.2b for Elfie
October 9, 2026 (afternoon). This is a **delta from v1.1**: it lists only what changed. It replaces this afternoon's v1.2 with the final batch 2 art and decisions. Thank you for the v1.1 response, which most of the decisions below came straight out of.

**Please keep this package private.**

---

## 1. Summary
- **Most of your v1.1 suggestions are now locked**, some with changes (section 2).
- **There's a new core component, the Memory Cell,** a multi-level latch that doubles as the game's save/load.
- **The text-editor path is now specified:** a 6-bit SIXBIT-style character set, a Keyboard block with a control port, the typewriter build that unlocks typing, and a player-built Font ROM.
- **The Thresholder now has two reference sides.** One side gives a threshold detector, and both together give a **window comparator**.
- **Color lamps** now come in three tiers (16/32/64) as one block family built for screens.
- **CloudAtlas, our AI art teammate, finished texture batch 2.** That's every block for the next preview build, plus the final wire readability pass (section 4).
- **The next preview build is in progress** (section 3).
- **A retro sound system is being explored.**

---

## 2. Locked decisions since v1.1

### Signals and components
- **The lamp terminates the signal.** There's no pass-through, so a lamp is an endpoint and a decoder: a Pulsar feeding a lamp grid is the full pixel pipeline.
- **Repeater:** delay setting of **1–8 ticks**.
- **Thresholder** (temporary name for the comparator-style block):
  - **Input** comes in the back and **output** goes out the front. There are **reference ports on both sides,** and the player builds the reference signals (no dial).
  - **One side powered = threshold:** it outputs when input ≥ reference.
  - **Both sides powered = window comparator:** it outputs only when **LOW ≤ input ≤ HIGH.**
  - **Left is LOW and right is HIGH.** Each side is marked with a shape (a down chevron over a floor bar, or a ceiling bar over an up chevron), so the cue holds up in greyscale and for colorblind players.
  - **References are sorted.** The engine takes the min and max of the two sides, so a swapped hookup still works as a window. The markings are a convention to learn, and nothing breaks if a player swaps them.
- **The Fast Latch is kept as binary** (on/off). A binary 0/1 T flip-flop is planned later.
- **Memory Cell (new):** a multi-level latch.
  - It stores a value **1–64**, where **0 means empty.**
  - A **1-tick pulse sets it** to that pulse's strength, and it holds the value and outputs it until the next pulse.
  - **It's directional:** the back is the input and the front is the output. It's **aimed when placed.**
  - It **is** the game's save/load storage. Pair it with Pulsars to move whole memories quickly.
  - Names stay electrically accurate (Memory Cell or Data Latch).
- **Redirector:** also **aimed when placed**, with no click-to-rotate.
- **Value convention, everywhere:** strength 0 = empty/off, and characters, colors, and values use **1–64.**
- **Wires:**
  - **Every wire color has the same dark outline.**
  - **Brightness floor at strength ≥ 1:** red 32 and **blue 48** on the 64-step ramp.
  - Silicon is unchanged (section 4.3).

### Text editor / typewriter path
- **Character set:** **6 bits, 64 characters, in the style of DEC SIXBIT**: uppercase, digits, and about 28 symbols. **Lowercase comes later as a 7-bit upgrade.**
- **Keyboard block:**
  - Each keypress fires a **1-tick pulse out the front**, carrying the character as **SIXBIT code + 1** (strength 1–64, since 0 means nothing).
  - A separate **control port on the right side** pulses control keys:

| Strength | Control key |
|---|---|
| 0 | none |
| 1 | Enter |
| 2 | Backspace |
| 3–6 | reserved: Up, Down, Left, Right |
| 7 | reserved: Tab |
| 8 | reserved: Esc |
| 9–64 | free for future keys |

- **Character RAM** is player-built from latches and Memory Cells.
- **The Font ROM is player-built from Memory Cells.** There's no dedicated block.
- **The controller** (cursor, write, newline, backspace) is **found partially built** in the world as an exploration reward.
- **Building the typewriter machine unlocks a typing UI.** It's a regular keyboard limited to the 64-character set.
- **In-game language:** **players build their own instruction sets (ISAs)**, and **one built-in learning ISA** is provided to teach the basics.
- **There'll be a tutorial world** for new players, and experienced engineers can skip it.

### Color lamps
- **There are three tiers, 16 → 32 → 64 colors, unlocked in that order** and tied to the editor and scripting roadmap. They **replace the separate 32-color display pixel.**
- **Palettes:**
  - **16:** CGA/EGA.
  - **32:** the CGA 16 plus 16 bright VGA hues. Indices 0–15 match the 16 tier.
  - **64:** RRGGBB, 2 bits each.
- **Mapping:**
  - 0 is off.
  - The 16 and 32 tiers **wrap**: color = (strength − 1) mod N.
  - The 64 tier is direct.
- **Look:** **one lamp family** with **large lit faces (about 85%)** and a thin dark gap, so walls of lamps read as pixel screens.
  - **The tier shows only in the corner shape:** rounded (16), square (32), or chamfered (64). The tooltip/HUD also shows the tier.
  - **Strength 1 is true black,** with a faint rim glow so the lamp still reads as on.

### Progression and world
- **The first lithography tier's target is 2-bit adders**, which is your "simpler first machine."
- **Rocket:** the **player's CPU controls burns and staging.** If it fails, an autopilot fallback makes a rough landing, so a clean landing is something engineers earn.
- **The Overworld is renamed the ORGANIC WORLD.** It's mostly peaceful, and its unique materials are **fan organics, optics quartz, and rocket fuel.**
- **Pulsar:** it's obtained from a **chip dungeon on the Wafer**, and there'll be **more dungeons on the Wafer** (Syber-Squad "inside the chip").
- **Wafer height is 768 blocks**: 48 sub-chunks, 256 below spawn and 512 above, with the **player starting at layer 256.** This needs serious optimization work.
- **Beacon numbers aren't locked yet.** Jaydee is still deciding, and your 64@r32 / 128@r24 / 256@r16 proposal is on the table.

### Engine fixes you flagged (queued)
- The wire brightness floor and outline (done in art; engine to follow).
- The look-at readout showing "-".
- Heavy meshing in the Organic World (dropped ticks).
- The controls panel collapsed by default.

### Also
- **A retro sound system is being explored.**

---

## 3. Next preview build: in progress (temporary names)
The textures are final, so the preview build is now **being built.** These are creative-mode **preview** items, so late-game unlocks like the Pulsar appear here for testing.

| Block | Role | Key behavior |
|---|---|---|
| Red Wire / Blue Wire | Signal 0–64 | Colors never connect. Dark outline, brightness floor. |
| Red Torch / Blue Torch | Inverter | Powers its own wire color only, plus the solid block above (colorless power) |
| Lamp | Output | Terminates the signal |
| **repeater** | Boost, delay, one-way | Delay 1–8 ticks |
| **thresholder** | Level/window detector | Back = input, front = output, left = LOW reference, right = HIGH reference. One reference = threshold, both = window. |
| **redirector** | Turn or point a signal | One block, target-block style, aimed when placed |
| **pulsar** | Clocked transmitter | Back = data, left/right = clock, front = output. A 1-tick, no-decay pulse over 64 blocks. |
| **memory_cell** | Multi-level storage | Back = input, front = output, aimed when placed. Stores 1–64 (0 = empty), set by a 1-tick pulse. |
| **fast_latch** | Binary memory bit | Instant on/off |
| **switch / button** | Inputs | Switch toggles; button pulses |
| **keyboard** | Character input | Front = SIXBIT+1 character pulses; right = control pulses (1 Enter, 2 Backspace, ...) |
| **color_lamp_16 / 32 / 64** | Screen pixel | 0 off; wrap (16/32) or direct (64) |
| **silicon / stone / dirt slab** | Half blocks | Bottom slabs only, for now |

Hotbar v2 mock-up with the new parts:

![Hotbar v2](images/hotbar_v2.png)

---

## 4. Textures and art (batch 2 final, by CloudAtlas)
**CloudAtlas**, our AI art teammate, made every new block in the house style:
- flat, smooth, and anti-aliased
- a dark outline on the wafer-gray casing
- **color codes for ports:** a filled port is an output, a hollow port is an input, a violet diamond is a reference (a down chevron marks LOW, an up chevron marks HIGH), a cyan diamond is a clock, an orange square is the keyboard control port, and yellow means powered

An automated consistency audit found **zero off-palette colors**. The only new colors are a copper family for the Fast Latch.

### 4.1 Face layouts
**Orientation rule:** the **front (output) is the top edge of the top texture**, so the top face always shows which way a block points. Any face without its own texture uses the side texture, including the bottom.

| Block | Layout | Faces | States |
|---|---|---|---|
| repeater | top + side + front + back | Top: arrow and delay pips. Front: output. Back: input. Left/right/bottom: plain side. | Unpowered/powered. **Delay 1–8 shown as pips.** |
| thresholder | top + front + back + left + right | Top: arrow. Front: output. Back: input. **Left: LOW reference** (down chevron over a floor bar). **Right: HIGH reference** (ceiling bar over an up chevron). Bottom: plain. | Unpowered/powered |
| redirector | top + side + front | Top: arrow pointing at the output. Front: output (lights when emitting). **The other four faces: target rings.** | Idle/emitting |
| pulsar | top + side + front + back | Top: crystal core with violet "premium" trim. Front: output. Back: data input. **Left and right: clock ports (cyan).** | Idle, plus a **6-frame firing animation** (50 ms per frame, plays once per pulse) |
| memory_cell | top + side + front + back | Top: an **8×8 cell grid that shows the stored value 1–64**, with a separate empty texture for 0. **Front: output. Back: input.** Left/right/bottom: plain. | 0 empty, 1–64 stored |
| fast_latch | all-same | A copper-framed bulb on every face | Off/on |
| switch | top + side | Top: a lever | Off/on |
| button | top + side | Top: a round cap | Up/pressed |
| keyboard | top + side + front + control | Top: keycaps, with an orange tab on the right edge. **Front: character output with 6 code LEDs. Right: the control port** (an orange square socket with an Enter glyph). Left/back/bottom: plain. | Character pulses out the front, control pulses out the right |
| color_lamp_16/32/64 | all-same | A large lit face, thin dark gap, and tier corner shape. Shader-tinted from the palette. | 0 off, 1–64 |
| silicon/stone/dirt slab | top + side (half) | Top and bottom: the full texture. Sides: the lower half is solid and the upper half is transparent. | Bottom slab |

Every new block, every face, and every state (final, v3):

![Batch 2 sheet v3](images/blocks_batch2_sheet_v3.png)

An in-game mock scene, top-down on silicon:
- **Red lane:** switch → repeater → thresholder → memory cell.
- **Blue lane:** keyboard → Pulsar → wire → redirector → lamp.
- **Right side:** a color_lamp_64 screen.

This scene was rendered just before the final pass, so its thresholder sides, memory cell front/back, keyboard control port, and wire outlines are slightly older than in the v3 sheet.

![Batch 2 in-game](images/blocks_batch2_ingame.png)

### 4.2 Pulsar firing animation
![Pulsar firing](images/pulsar_firing.gif)

### 4.3 Wire readability (final)
- **Every wire color has the same dark outline.**
- **Brightness floor for strength ≥ 1:** red 32 and **blue 48** (up from 40). Strength 0 and silicon are unchanged, and the wire's strength readout ring still shows the exact value.
- **At 16 px per block,** a powered blue wire reads clearly: a bright core at about **2.2:1 contrast** between dark outlines at about 2.2:1. It's unmistakably different from an unpowered wire.
- **At 8 px per block,** the wire is about 1 px wide:
  - Both the core and the outline fall to about **1.4:1 contrast**, so the wire no longer stands out by brightness.
  - It still reads as a **blue line by hue** and is clearly different from an unpowered wire.
  - Doing better at that distance would need a wider wire, since silicon is locked.

![Wire floor check v2](images/wire_floor_check_v2.png)

### 4.4 Color lamp tiers
All colors for 16/32/64, far-distance rows including greyscale and color-blindness simulations, and a mixed wall:

![Color lamp tiers](images/color_lamp_tiers_v3_sheet.png)

---

## 5. Open questions (your input welcome)
1. **Beacon numbers:** cost, radius, and power draw per tier (64/128/256 TPS) and how they interact with heat. Jaydee is deciding, and your proposal is the starting point.
2. **Pulsar "premium" look** (violet trim, corner pads, crystal core, glow): is it special enough for a hard-won mid-to-late-game item?
3. **Keyboard code LEDs:** should the 6 front LEDs show the last code live? It's a nice-to-have, not blocking.
4. **Slabs:** bottom only for now. Do we need top and vertical slabs in the preview?
5. **Wire at far distance:** is the 8 px readability (hue only) acceptable, or is a wider wire worth considering later?
6. **Color-lamp accessibility:** the 32 tier adds near-neighbor hues (red/crimson/red-orange) that merge for some colorblind players. Should we add an optional numeric/ring readout of the value on look-at?
7. **Shift plane:** should the 6-bit set have a "shift" mode before the 7-bit lowercase upgrade, or should we wait for 7-bit?
8. **The built-in learning ISA:** what's the size and style (your ~12-instruction tiny assembly)? How should player-built ISAs feed the compiler and the Lithography Machine?
9. **Control keys 9–64:** which future keys or functions deserve a reserved code?
10. **Lithography tiers beyond the first:** your proposal of visible = counters/adders, DUV = CPU parts, EUV = SoC isn't locked yet.
11. **Virus attacks** on unshielded builds (your "telegraphed" idea): how often, and what counts as shielding?
12. **First-hour plan:** a tutorial world is locked. Should the main game's first hour still end with the portal trip to the Organic World, and is the portal an early toy?
13. **Chip dungeons:** are they generated from real chip layouts, as you suggested? What's inside them besides the Pulsar?
14. **Wafer height 768:** any design concerns about 256 layers below spawn (mining depth, ore layering, dungeon placement)?
15. **Retro sound:** any direction or references for a retro sound style that fits a chip-building game?

---

## 6. Package contents
- `Voxel-Compute-Update-v1.2b-for-Elfie.md`: this document
- `images/blocks_batch2_sheet_v3.png`: every new block, face, and state (final)
- `images/blocks_batch2_ingame.png`: the in-game mock scene and an isometric view of each block
- `images/hotbar_v2.png`: the hotbar with the new parts
- `images/pulsar_firing.gif`: the Pulsar firing animation
- `images/wire_floor_check_v2.png`: the final wire readability check
- `images/color_lamp_tiers_v3_sheet.png`: the color lamp tiers

Section numbers (for example "Re: 5.3") make feedback easy to map. **Please keep this private.**

*One man, one Grok, one game.*
