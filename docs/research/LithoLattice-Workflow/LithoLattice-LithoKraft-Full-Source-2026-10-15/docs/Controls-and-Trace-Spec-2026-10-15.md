# Controls & Trace Spec: audit + Bedrock-style proposal (2026-10-15 22:05 EDT, Cortana)

Status: design only, nothing implemented. Sources read: content/blocks.cpp, components/*.cpp, signal_world.cpp,
game/game.cpp handleWorldActions, game/actions.cpp, engine sim_thread.h, player_locomotion.h.

## 1. Current behaviour (audited)

### 1.1 Trace components (role / flags / interaction today)
| Block | Role | Flags | Right-click (Use) | Sneak+Use (sim supports) |
|---|---|---|---|---|
| Red / Blue Trace | Wire | transparent, flat-pick, Custom strip | empty hand: cycles adjacent Redirector front | - |
| Red / Blue Via | Wire | solid, transparent | - | - |
| Red / Blue Torch | Torch | attachable cross | - | - |
| Repeater | Repeater | directional, interactive | cycles delay 1..8 (state aux, 3 bits) | - |
| Redirector | Redirector | dir, interactive | cycles output centre -> left -> right | - |
| Catchor | Catchor | dir, interactive | cycles output | **steps match value** |
| Thresholdor | Thresholdor | dir | reset | set threshold |
| Pulsor / Memor / Fast Latch | own roles | dir | - | - |
| Switch / Button | Switch / Button | solid, cuts wire, interactive, attachable | toggle / press | - |
| Keyboard | Keyboard | dir, interactive | opens typing | - |
| Signal Transmittor / Receivor / Jammer | radio roles | dir | install/remove jammer (jammer) | cycle channel |
| Dish, Overclockor, Tone, Color Lamps, Lamp, Emitor | own roles | dir/comp | - | some: channel |
| Lithography (EtchOR, LithographOR, PortOR, Fusor, ChipOR) | own roles | interactive | machine UIs / etch | - |

### 1.2 Input & placement
- Bindings (actions.cpp): LMB Break, **RMB = Place AND Use**, MMB PickBlock, LShift Sneak (fly: down), C Crawl,
  LCtrl Sprint, Space Jump (fly: up), E inventory, Q drop, 1-9 / wheel hotbar. Gamepad mirrors.
- Right-click resolution (game.cpp handleWorldActions), in order:
  1. Sneak + Use on a **Catchor only** -> alt-use (match value).
  2. Use on a kInteractive block (or a flat wire with an empty hand), not sneaking -> Use.
  3. Use with an item that has an item-use handler and places no block -> item use.
  4. Otherwise Place the selected block. Facing = the horizontal look direction (front points away from the player).
- **Edge-triggered only** (`pressed()`): no hold-to-repeat for place/break.
- Reach: `kReach = 8` blocks (+0.5), look range 64 (engine sim_thread.h). Bedrock: 5 survival, 12 creative.
- Pick block: within reach; selects a matching hotbar stack, creative creates one if none.
- Poses: Stand 1.62 / Sneak 1.27 / Crawl 0.4 eye height, sneak edge-guard, forced crawl in 1-block gaps, Fly mode.
  No kneel pose. No IK (engine anim has iksolve/riggen, not wired to the player).
- No forward/bridge placement. Placement always targets the face the ray hits.

## 2. Gaps vs Minecraft Bedrock
1. **BUG (likely):** sneak+use is gated to Catchor in game.cpp, while the sim's kCmdAltUse dispatch is generic.
   Thresholdor threshold, radio channels and the jammer channel are documented as sneak+use but are **unreachable** in game.
2. Sneak+right-click never places against an interactable. Bedrock: sneaking always places. Here the Catchor's sneak
   consumes it, and other interactables fall to Place only because `usable` is false while sneaking (inconsistent).
3. No hold-to-repeat place/break (Bedrock: ~4 actions/s while held, creative instant-break repeat ~ every 0.25 s).
4. No bridge placement (Bedrock: looking down and back while walking keeps placing at the edge).
5. Reach 8 for every mode (Bedrock 5 survival / 12 creative).
6. No kneel pose. Crawl is a key (Bedrock: crawl only forced). IK not applied.
7. Repeater delay 1..8 ticks in 1-tick steps. The requested 2/4/8/16 with a button model is a different design (sec 4).
8. Block shapes: only Cube / Slab / Cross / Custom exist. No stairs/ramps/partials as states.
9. No liquids in the game (worldgen has an engine water.cpp, unused by the Wafer). Cattails use a biome stand-in.

## 3. Proposed control scheme (Windows/Linux, KB+mouse; flying unchanged)
| Input | Action |
|---|---|
| LMB | Break. **Hold** = repeat every 250 ms (creative) / after the break completes (survival) |
| RMB on an interactable | Interact (Use) |
| RMB elsewhere | Place. **Hold** = repeat every 200 ms, only onto a NEW target cell (no double-place on the same cell) |
| **Sneak + RMB** | **Always Place** (even against interactables), Bedrock rule |
| **Alt + RMB** (or Sneak + scroll / Sneak + MMB, see D2) | **Secondary setting** (Catchor match, Thresholdor threshold, radio channel) |
| MMB | Pick block (creative: create; survival: select) |
| LShift | Sneak (edge guard; fly: down) |
| C | Crawl toggle (forced crawl stays) |
| Ctrl | Sprint |
| Z (proposed) | Kneel (lower eye ~0.9; can work under a 1.5-block gap; cosmetic for now) |
- **Hold-to-repeat** covers Trace and every placeable item. Trace placed with RMB held while walking lays a line, one cell
  per new target.
- **Bridge placement** (Bedrock-style): while walking, if the ray hits nothing within reach but the player stands at an edge
  and looks down/back, place against the block under the feet toward the movement direction (target = feet-1 + move dir).
  Applies in Walk mode only.
- **Reach**: survival 5, creative 8 (keep 8; Bedrock 12 is long for circuit work). Value in Settings.
- **Pick block**: unchanged, plus a Ctrl+MMB variant that copies state (repeater delay, facing), like Java pick-with-NBT.
- **IK note**: engine anim/iksolve exists. A later pass can drive arm reach to the hit point and foot planting on slabs/stairs.
  Render-only, never sim.
- **Conflict resolution**: sneak is reserved for placement, so the Catchor's sneak+use match cycling moves to the
  secondary-setting input (D2). The same input covers every alt-use component (fixes bug 1).

## 4. Repeater redesign (requested)
- Model: a **low slab** (4/16 tall) built as an octree voxel model like the foliage (shared instance mesh, level 4), with
  **4 inner buttons** labelled 2/4/8/16. Exactly one is pressed (sunken, glowing) = the delay in ticks.
- Interaction: the ray hit point on the top face picks the quadrant -> that button becomes active (direct select, no cycling).
  Plain RMB elsewhere on the repeater cycles to the next delay.
- State: 2 bits (0..3 -> 2/4/8/16) in aux. Facing bits unchanged. Model state = the active button (render-side,
  appearance selects the mesh variant).
- **Sim impact**: delays change from 1..8 to 2/4/8/16. This changes circuit timing, the handshake hash (circuit scene
  uses delay-8; 8 survives) and save compatibility (old 1..8 values need a migration map). See D4.
- Collision: becomes a slab (Trace can run beside it; items sit at 4/16).

## 5. Block shape states (queued)
- Add Shape variants as state, not new ids: full, bottom/top slab, stair (4 facings x upside-down), ramp (wedge), partial
  (quarter/eighth). Store in aux/facing bits, or a "shape" field for blocks that opt in.
- Collision boxes per shape (player locomotion fit-tests already exist). Mesher: box models per shape (BoxModel path),
  greedy only for full cubes.
- Sim: shapes are cosmetic/collision only unless JD decides wires climb ramps (D6).

## 6. Liquids (queued)
- Non-colliding (no player collision; swim/slow movement), rendered with the blended pass.
- Spread: flow level 0..N with a default spread of 16 cells. Viscosity per liquid = ticks per spread step (water 1x,
  higher for thick liquids: e.g. "coolant" 2x, "wax" 6x if the wax canon arrives). Deterministic, a sim-side stepped update.
  Spreading changes the world, so **liquids are sim, not render**: they move the hash wherever they exist (D7).
- Worldgen can then place real water for cattails (replacing the capacitor_groves stand-in).

## 7. Decisions JD must make
- **D1** Sneak+RMB always places (Bedrock) even on interactables? (recommended: yes)
- **D2** Secondary-setting input for Catchor/Thresholdor/radio: Alt+RMB, Sneak+scroll, or a radial/UI? (recommended: Alt+RMB)
- **D3** Hold-repeat rates (place 200 ms, break 250 ms) and whether repeat applies in survival.
- **D4** Repeater 2/4/8/16 replaces 1..8 (timing + save migration + hash change), or keep 1..8 and show 4 buttons as presets?
- **D5** Reach: survival 5 / creative 8 (or Bedrock's 12)?
- **D6** Do shapes (ramps/stairs) carry Trace or signals, or are they purely cosmetic?
- **D7** Liquids in the sim (hash moves where they exist) and their names/viscosities; water in the Wafer worldgen?
- **D8** Kneel key and purpose (cosmetic vs gap clearance); keep C as a crawl toggle?
- **D9** Bridge placement on by default, or a setting?
## Decisions + implementation status (Cortana 2026-10-15 2225)
- D1 sneak-place, D2 Alt-secondary, D3 hold-repeat 200/250 ms, D4 repeater 2/4/8/16 + migration, D5 reach 5/16, D9 bridging (game.bridging): IMPLEMENTED.
- D8: C crawl toggle IMPLEMENTED. Kneel (Z, 1.5 high, eye ~1.27): DEFERRED. It needs control flag bit4 "kneel" in the player-input event, which changes the control format. Plan: bump the control stream to v2 with the flag; v1 streams read kneel=0; Locomotion Pose::Kneel shares the Sneak height without the edge guard.
- D6 inclines (SPEC, not implemented): Ramp (1:1) and Stair (two 0.5 steps) shape blocks with a facing. Collision gives the floor height as a function of local position along the facing (ramp: h=t; stair: h=0.5 or 1.0, but walk-up allows <=0.5 step-ups without a jump). Locomotion: a step-assist of 0.55 for grounded movement, with a smoothed camera y (lerp ~80 ms) so stairs feel like a ramp. Sneak edge guard treats ramps as floor. The mesher needs custom geometry (sloped quads). The block shape state uses the facing plus a top/bottom (upside-down) bit.
- Future IVehicle interface (not implemented): `struct IVehicle { virtual void mount(Player&); virtual void dismount(); virtual void applyInput(const LocoInput&, float dt); virtual Pose seatPose() const; virtual AABB bounds() const; virtual void serialize(Writer&) const; }`. The sim owns the vehicle state for determinism, and inputs go through the existing control stream as a vehicle-input event. Targets: Tron light cycles (90-degree turns, trail walls as transient blocks) and gliders (lift/drag, no fall damage on glide).
- D7 liquids: deferred.

## Kneel, Prayer, Static, Charge (Cortana 2026-10-15 2245, JD 22:36/22:38)
- Kneel: IMPLEMENTED as a triple sneak tap (0.6 s window), derived in LocomotionController from sneak history. No format change. Latches; released by a tap, a jump, crawl or Fly. Height 1.2, eye 1.0. The Z key was dropped.
- Audit of what exists:
  - Static exists as IStat "static" (0..INT32_MAX, default 0) on GamePlayerAdapter, with the certificate curve in certificates.h. Nothing grants it yet.
  - Charge exists twice, and the two DISAGREE: IStat "kharge" registered 0..64, default 0; and kharge.h KhargeState -64..+64 (red/blue morality, IKharge, unused). JD's +/- honor meter matches kharge.h, so the registered stat range should become -64..64 (decision needed).
  - There is NO in-game day/time counter anywhere (lighting is static per dimension).
  - Locomotion (and therefore kneel) runs on the game thread from sampled input, not in the sim.
- So prayer is SPECCED, not built, as instructed:
  - Day clock (sim): `worldTick` (already the sim tick) / `kTicksPerDay` (proposal 24000 at 20 tps-equivalent; JD to set), so day = tick / kTicksPerDay. Deterministic and needs no new field if derived from the saved tick count; verify the tick count is saved.
  - Prayer (sim-side): the player's pose must be visible to the sim. Option A: the sim runs the same LocomotionController derivation from the control-stream sneak bits (no format change; preferred). Option B: a pose flag (format change, needs JD's OK). The sim counts consecutive Kneel ticks; at 65 a prayer completes once per kneel; max 3 per day.
  - Reward (placeholder): +2% of the static needed for the next level (staticTotalForLevel(L+1) - staticTotalForLevel(L)), minimum 1. Scaled by Charge sign later (open).
  - New save fields (player.dat, version bump needed): `prayerDay` (u32) and `prayersToday` (u8). Charge already rides on the stat block, if stats are saved (verify).
  - Charge: separate morality meter -64..+64, not touched by prayer unless JD decides otherwise.
