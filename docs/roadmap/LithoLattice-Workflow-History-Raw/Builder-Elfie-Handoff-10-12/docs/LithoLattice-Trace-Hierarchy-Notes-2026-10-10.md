# Trace / Wire Hierarchy — Design Notes
October 10, 2026. Elfie. How the signal system layers for modders.

## The layer cake
```
Engine (LithoLattice)          Game (LITHOKRAFT, mod #1)       States
─────────────────────          ─────────────────────────       ──────
Tick domains, dirty            ISignalNode (implemented)       
chunks, scheduler              ──────────────► Wire (concept:  
                               signal-carrying network)        
                                                     └── Trace (implementation:
                                                         0-64, -1/block,
                                                         net flood-fill)
                                                            ├── Red trace (channel 1)
                                                            └── Blue trace (channel 2)
```

## What lives where
- **Engine:** timing (tick domains, scheduler), dirty propagation, chunk storage, `CellState`, `BlockDef.channel` (a `uint8_t` payload the engine stores but never interprets). The engine knows *that* signals exist, not what they mean.
- **Interface (`ISignalNode`, proposed SDK):** `output(state, face)` + `onInputChanged(...)`. The contract any signal participant honors. Engine-agnostic.
- **Wire (game concept):** a set of connected cells sharing one strength value. Flood-fill membership, attenuation rules, support rules — all game logic.
- **Trace (LITHOKRAFT's wire):** strength 0–64, −1 per block, red/blue channels that never connect, vias for lossless vertical, support = floor or ceiling, staircasing allowed.
- **Red / Blue (channel states):** the same implementation, different `channel` id. Isolation is enforced by the net solver, not by separate code paths. Adding a third channel (green trace?) is data, not a rewrite.

## The modding story
A third-party mod builds its own wire by:
1. Implementing `ISignalNode` on its blocks.
2. Writing its own solver (or copying the Trace pattern): flood-fill, attenuation, channel isolation — whatever its fiction needs (water pressure, conveyor items, gossip).
3. Using `BlockDef.channel` for its own channel ids.
4. Riding the engine's tick domains and dirty system for free.

The Trace is mod #1's *answer* to "how do signals move" — not the engine's. That's why the solver stays in `lithokraft_content` (review decision #1).

## Why two channels, not N
Red/blue is the minimum that teaches channel thinking (isolation, side-by-side runs, split redirectors). The architecture supports N channels; the game ships 2. If a modder needs 16, the solver doesn't care.

## Signal: the wireless medium (JD locked 2026-10-10)
Wired is **Trace**; wireless is **Signal**. Same 0–64 values, same channels — the medium is air, not copper. You *trace* a circuit; you *send a signal*.
```
Trace
  ├── Red/Blue trace (wired — adjacency nets)
  ├── Via (wired — vertical, lossless tubes)
  └── Signal (wireless — channel + range nets)
        ├── Transmittor (broadcasts input strength on a channel)
        ├── Receiver (listens; outputs strongest in-range signal)
        └── Dish (long-range receiver; single block = weak, hex/circular
                  multiblock = range/sensitivity scale with size/shape)
```
- A radio "net" is defined by **(channel, position, range)**, not flood-fill. Everything downstream is identical — a Memor can't tell whether its input came down a wire or through the air.
- **Channels** unlock through progression (4 → 16 → 64 by tier).
- **Pulsors over radio** stream packets wirelessly: remote screens, decoders, and instruction caches receive whole programs.
- **Interference rule (proposed):** strongest in-range signal on a channel wins. Simple, predictable; exotic interference is a later mechanic.
- The Dish doubles as the lore receiver — the same block that picks up Daemon fragments. Wireless trace and story trace are one system.

## Progression: pooled ICs → wireless → infinite reach (JD, 2026-10-10)
- The arc: **physical trace → pooled integrated circuits → wireless radio → extreme-distance transmission.** Each stage harmonizes with the last — wired for local precision, wireless for distance. Best of both worlds, one system, two media.
- Endgame wireless spans **dimensions** (the Dish already hears the Void). "Infinite distances" is literal.
- **Implementation mandate:** build the radio net system for scale on day one — channel+range indexing (never naive iteration), compact strength/channel data, sim cost proportional to transmittor count, never to distance. Get it right now so it scales later.
