# Roadmap: robotics and safety

**Status:** Proposed. JD (t2345u) asked for these to be considered seriously.

LithoLattice and its in-engine AI are meant to carry over to real robot bodies. Two
requirements from the [safety watchlist](../safety/SAFETY_WATCHLIST.md) are on the
roadmap as serious items:

- **Black Box Mystery Solved (watchlist R1):** a replayable, tamper-evident decision log.
  - Every input, sensor sample and AI decision (NPC/mob AI, the AI framework, and AI agents acting through the engine) is recorded with its inputs and tick number.
  - Deterministic replay reproduces every decision tick by tick.
  - Entries are hash-chained so the record can stand up as evidence.
- **Physical interaction limits (watchlist R2):** actuator force and grip limits are enforced below the AI layer, so no AI decision can exceed a safe force near people (for example, shaking a human's hand gently).

The full requirements, the existing building blocks and the open work are in
[SAFETY_WATCHLIST.md](../safety/SAFETY_WATCHLIST.md).
