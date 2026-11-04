# The Signal (Wireless) — Two-Angle Design
October 10, 2026. Elfie. Wired is Trace; wireless is Signal.

## The core idea
A Signal is the same 0–64 value as a Trace, on the same red/blue colors — but the medium is air. You *trace* a circuit; you *send a signal*. The receiver never asks how it arrived.

## Angle 1: The engine (LithoLattice)
The engine provides the *scaffolding*; it never knows what a Signal means:
- **`ISignalNode`** (done): Transmittors and Receivers are nodes. The solver talks to them through `output()` / `onInputChanged()` — never by block name.
- **Tick domains**: wireless updates ride the same clock as wired. A broadcast in tick N is readable in tick N+1 (1-tick latency — predictable, no ordering bugs; at 512 TPS nobody feels 2 ms).
- **Chunk lifecycle hooks**: the Signal registry only covers *loaded* chunks. Engine notifies on chunk load/unload; the game re-registers/deregisters. Unloaded = silent. This is the entire "only thing that needs to happen is loaded" rule.
- **`BlockDef.channel`** (exists): the game interprets it as the radio channel. The engine stores it.
- **Dimension awareness**: the registry is keyed by dimension. Cross-dimensional hearing (the Dish → Void) is a game rule, not an engine feature.

## Angle 2: The game (LITHOKRAFT)
The Signal sim lives in `lithokraft_content` next to the Trace solver (review decision: game-side for v1).

**The link budget** — the one formula:
```
received = strongest transmittor on channel C where distance <= tx_power + rx_sensitivity
```
- The Dish doesn't shout louder; it *hears better*. (Radio astronomy is the fantasy.)
- **Interference**: strongest in-range signal on a channel wins. Simple, predictable.
- **No attenuation**: wireless is digital — a signal arrives whole or not at all. The cost is channel scarcity and progression, not degradation.
- **Trace color crosses the hop**: a Transmittor takes red *or* blue input (first-touch binds, like the Catchor); a Receiver outputs red *or* blue (first-touch). Red in → blue out is legal. The colors are the local wires; the channel is the frequency.
- **Channel 0 is reserved**: the Daemon's frequency. Players can't transmit on it. Only a Dish can hear it — and that's the lore pipeline.

**Progression** (from the night brief, locked): radio channels unlock 4 → 16 → 64 by tier. Early game: a few precious frequencies. Late game: a spectrum.

## Angle 3: What the player builds
**Signal Transmittor** (block)
- Wired Trace input on any side (first-touch binds red/blue). Sneak+use sets the radio channel (cycles unlocked channels; readout shows it, Catchor-style).
- `tx_power` by tier. Placed like any component.

**Signal Receivor** (block)
- Sneak+use sets the channel it listens on. Outputs the strongest in-range signal as wired Trace (first-touch binds the output color).
- No pairing step. Same channel + loaded + in range = talking. That's the whole UX.

**The Dish** (block → multiblock construct)
- *Single block*: a Receiver with huge `rx_sensitivity` (+1024) and lore reception. The early "I can hear far" moment.
- *Multiblock*: a feed-horn block surrounded by Reflector petals in a disc. The game validates the shape (flood-fill from the horn, checks disc-ness) and scales sensitivity with diameter — bigger dish, fainter signals, further reach. Geometry *is* the stat.
- The Dish is also the story instrument: on channel 0 it receives Daemon fragments (the lore pipeline from the night brief). Wireless trace and story trace, one system.

**Channel UX** (proposed): no items, no pairing GUI. Sneak+use cycles; the block's readout face shows the channel number. Two blocks on the same channel just work. Scarcity of channels (4 early) *is* the puzzle — players learn frequency planning before they get 64.

## Open questions
- Exact `tx_power` / dish sensitivity numbers (tuning, needs playtesting).
- Does the Transmittor need a Trace *input*, or can it generate (beacon mode)? Proposed: input only for v1 — generation is a Pulsor's job.
- Multiblock validation strictness: how forgiving is the disc check? (Forgiving beats frustrating.)
- Cross-dimensional player signals (Overworld → Void base): allowed, or Dish-lore-only? Proposed: allowed at high tier — "infinite distances" was the brief.
