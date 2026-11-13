# Squad Brief — LithoLattice / LithoKraft
October 10, 2026. For Cortana, Grok, Builder, Gemini, CloudAtlas. From JD via Elfie.

## What's built and green (333 tests passing)

**Wireless Signal system** — Signal Transmittor (input/beacon modes, first-touch color bind, sneak+use channel cycle), Signal Receivor, Dish (1024 sensitivity, long-range), Overclockor (beacon equivalent, jammer mode). Link-budget radio registry, 1-tick latency, 4→16→64 channel gating, save migration v1→v2.

**Jammer** — Overclockor + jammer item blocks all channels except its own within 64 blocks. Channel-selective: your base on ch 5 hears only ch 5. Both TX and RX respect it.

**Entities** — Engine EntityManager + game ItemBehavior (gravity, magnet, pickup, 8-min despawn). Popped trace spawns item drops. Engine AABB-vs-voxel physics (`moveCollide`) built and tested; game-thread wiring in progress.

**Standards** — `litholattice::` namespace, `I`-prefix interfaces, `k`-prefix constants, full -OR names (SignalTransmittor/SignalReceivor, not Tx/Rx), Trace=wired, Signal=wireless.

## Design locks

- **The 64**: signal values 1–64, 64 channels, 64-block Pulsor/wire reach. 1-based everywhere; 0 always means "off" (ch 0 reserved).
- **Wired**: steady signals degrade 1/block (max 64). Pulsor (clock on side, data on back) fires 1–64 with no decay, 64 blocks, 1 tick.
- **Wireless**: no decay, range = power (256) + sensitivity. 64 channels.
- **Inventory**: 42 carry slots (8 hotbar + 2 offhands + 32 main in 4×8). 42 = 101010₂. Separate 8 gear slots: head, chest, legs, feet, arms, face, heart, mind.
- **Item despawn**: 8 minutes (∞ on its side).
- **Lithography**: build big → print small → build bigger. Chips contain chips. Tiers: visible/DUV/EUV.
- **Encryption**: player-built on the 64 values. No engine crypto. Interception always possible (Dish); encryption is the counter, jamming is denial, wired is the air-gap.

## Lore direction (fragments, not the bible)

Face/heart/mind gear slots are saying something. The Dish hears things. Channel 0 is reserved. That's all for now — details stay with JD.

## Open / next

1. Item collision wiring (in progress, Elfie)
2. 42-slot inventory data model
3. Item rendering
4. Radio registry channel indexing (perf)
5. Cross-dimensional signal
6. Multiblock Dish (after in-game testing)
7. Overclockor tiers (copper/silver/violet) + tick accelerator
8. Falling blocks

## For each of you

- **Cortana**: engine/game separation holds. New engine surface: `litholattice::entity` (EntityManager, IEntityBehavior, moveCollide), `IWorldSimulation::onBlockPopped`. All additive.
- **Builder**: naming audit done (SignalTransmittor/SignalReceivor). SGNL v1→v2 migration fixed and tested. Conventions table in the standards review.
- **Grok**: wireless is done and tested. TX power 256 and beacon-at-64 are tuning numbers for playtesting.
- **CloudAtlas**: new blocks need art (Dish, Overclockor, Signal Transmittor/Receivor have placeholders). Overclockor tiers (copper/silver/violet) when we get there.
- **Gemini**: 42-slot inventory UI (8 hotbar + 2 offhands + 4×8 main + 8 gear) when we build it.
