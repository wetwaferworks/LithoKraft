// Watchor (JD 2026-11-11 design session section 6 + CloudAtlas row B, approved 10:40 PM ET).
// Engine id lithokraft:watchor. Spelling is WATCHOR (never Watcher). Row B look: eye cube, full block.
#pragma once
#include <string>
#include "components.h"
namespace lithokraft::components {
/// LOCKED: output out the BACK, full strength 64, exactly one tick, on any change.
/// LOCKED: refire at most every two ticks (fire one tick, rest one tick); a change that
///         arrives on the rest tick produces no pulse.
/// STATED: the front (local north) is the eye; it watches the block directly ahead and any
///         state change there fires it. The side faces have no function (no input, no output).
/// The watched cell is read in the component pass before any component applies (sorted,
/// order-free), so a Watchor sees a change one tick after it happens: chained Watchors shift
/// phase by one tick (design session section 6, STATED).
/// OPEN (not decided): a block appearing where air was, a block being
/// removed, a resin assembly sweeping through the watched cell (see openEdge in watchor.cpp).
class cWatchor final : public cCompBehavior {
 public:
  ComponentStep tick(const ComponentInputs& in, CellState st, ComponentMemory& m) const override;
  int emitToward(int dir, CellState st, const ComponentMemory& m) const override;
  std::string describe(const BlockDef& d, CellState s) const override;
};
}  // namespace lithokraft::components
