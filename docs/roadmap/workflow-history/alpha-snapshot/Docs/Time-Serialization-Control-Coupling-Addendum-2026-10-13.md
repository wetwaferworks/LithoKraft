# Time Serialization — Control Coupling Addendum
Prepared 2026-10-13 (Elfie, proactive research pass). Project content only.
Companion to: Time-Serialization-Workbench-2026-10-13.md
Status: JD directive recorded, implementation NOT yet tasked.

## JD's directive (2026-10-13 12:44 EDT, verbatim substance)

Hook the control system up just like the Timing Handshake:
- Process controls BEFORE and AFTER the handshake cycle, so key
  presses / events are in line and SERIALIZED with the handshake
  (deterministic, tick-stamped input application; "very very
  responsive").
- The control system does NOT need the orders-of-magnitude rates.
  "It just needs the single power of two" — one power-of-two gear
  off the master clock.
- This extends JD's 04:56 EDT Time Serialization direction:
  serialize the whole algorithm, input included.

## Why this is a real gap in the tree today (verified 2026-10-13)

1. The movement input itself is ready: `LocomotionInput`
   (engine/src/entity/.../locomotion.h) is a pure function input —
   step() has no wall clock, no RNG, and the SAME struct is fed by
   the human path and the MCP control surface. Determinism at the
   locomotion layer is already designed in.
2. The MCP pilot path is NOT serialized with the sim:
   `IPlayerController::ControlFrame` (engine/src/mcp/.../server.h)
   is consumed "on the game's main thread at the frame pump; each
   pump consumes at most one staged batch, so the effective input
   rate can never exceed the frame rate." Input timing therefore
   depends on render frame rate today — the exact coupling JD's
   directive removes.
3. Edge-triggered actions are the risk case: ControlFrame carries
   edge fields (usePressed / attackPressed / placePressed /
   dropPressed, "this frame only") alongside level-triggered held
   buttons (LocomotionInput is level-only). At a frame pump, an
   edge can land between ticks, be coalesced, or be applied at a
   tick that depends on FPS. Tick-stamping edges at the handshake
   boundary is what makes them replayable.
4. The gear machinery exists: ClockDomain / SimThread already
   support power-of-two clock domains (kMaxClockDomains = 4), and
   the Timing Handshake authority + phase barrier are in
   engine/src/sim/.../timing_handshake.h. The control gear is a
   new domain/phase user, not new infrastructure.

## Design sketch (Elfie proposal — JD locks, nothing here is locked)

- **Pre-phase control pass:** at handshake phase open, drain the
  staged input queue in arrival order; stamp each event with the
  worldTick it will apply at; publish it as part of the phase's
  input snapshot (same discipline as component input snapshots in
  the main Time Serialization pass).
- **Post-phase control pass:** after commit, drain again — events
  that arrived during the phase are stamped for the NEXT tick,
  never retro-applied. Before + after is a sampling bracket around
  the cycle, not two application points.
- **One gear:** controls run on a single power-of-two domain off
  the master clock (candidate reading: a divisor of 512 TPS, not
  a ladder of rates). Which divisor is JD's call — see below.
- **Unify the queues:** human input and MCP ControlFrames stage
  into the same serialized queue, converting MCP edge fields to
  tick-stamped edge events and held fields to level state. MCP
  rate limits (120/s sustained, 240 burst — rate_limit.h) stay at
  the tool layer; the queue only orders what was admitted.
- **Replay gate:** an input log (tick-stamped events) replayed
  headless must reproduce the determinism scenario hash
  faec0f02700d98c6-class behaviour for movement-affecting inputs —
  same input log, same world, regardless of the frame rate the
  log was captured at.

## Open calls for JD at kickoff (do not guess these)

1. Which single power-of-two gear for controls? (e.g. every tick
   at 512, or a 32 / 64 / 128 divisor — JD said "the single power
   of two" without naming the exponent.)
2. Does the post-phase pass ever apply within the same cycle for
   responsiveness (JD's "very very responsive"), or is it strictly
   stamp-for-next-tick? The sketch above chooses next-tick to
   protect determinism — JD to confirm or overrule.
3. Scope: player locomotion + MCP pilots only, or also console /
   lithoscript `wait`-driven input and agent (IPlayerAgent) input?
   Sketch assumes pilots + human first.

## Sequencing

Slots into the existing Time Serialization workbench as a second
workstream beside component serialization (workbench §"Why this is
the long pole"). No new baselines: selftest 958 PASS / 0 FAIL,
ctest 48/48, handshake DEFAULT OFF, determinism hash must hold.

Sources: JD directive 2026-10-13 12:44 EDT (memory 2026-10-13.md,
Timing Handshake x Control System coupling entry);
engine/src/entity/include/litholattice/entity/locomotion.h;
engine/src/mcp/include/litholattice/mcp/server.h (IPlayerController
frame-pump comment); engine/src/mcp/include/litholattice/mcp/
rate_limit.h; engine/src/sim/include/litholattice/sim/sim_thread.h
(ClockDomain gears); Time-Serialization-Workbench-2026-10-13.md.
