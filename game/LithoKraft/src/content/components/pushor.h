// Pushor (JD 2026-11-11 design session section 8 + CloudAtlas row B "Honeycomb face", approved 10:28 PM ET).
// Engine id lithokraft:pushor. Spelling is PUSHOR (never Pusher / Pistor). Full block.
#pragma once
#include <string>
#include "components.h"
namespace lithokraft::components {
/// STATED: the FRONT (local north) is the resin pushing face: no Trace wire, reads no power.
/// STATED: back, east, west, top and bottom accept Trace wire and read soft and hard power.
/// STATED: quasi-connectivity -- while HARD powered it soft-powers adjacent Pushors on all six
///         sides (world axes). A Pushor powered only that way does not pass it on.
/// STATED: push limit 32 blocks (recorded below; nothing is pushed in this build).
/// OPEN (JD): extension distance -> NO movement is implemented: no extend, no spit, no retract.
///            The one-tick spit vs push-and-retract rule needs a distance, so it is not wired either.
/// OPEN (JD): immovable blocks; how a resin-bonded assembly counts against the 32. Resin is not in.
/// This build only senses power: state on = powered (lights the CloudAtlas power tint), aux bit 0 =
/// hard powered (a component output driving straight into a Trace face). It emits no signal.
class cPushor final : public cCompBehavior {
 public:
  ComponentStep tick(const ComponentInputs& in, CellState st, ComponentMemory& m) const override;
  int emitToward(int, CellState, const ComponentMemory&) const override { return 0; }
  std::string describe(const BlockDef& d, CellState s) const override;
};
constexpr int kPushorPushLimit = 32;  ///< STATED (JD): push limit 32 blocks. Not applied yet (no push).
constexpr unsigned kPushorAuxHard = 1u;  ///< aux bit 0: hard powered this tick (drives quasi-connectivity)
}  // namespace lithokraft::components
