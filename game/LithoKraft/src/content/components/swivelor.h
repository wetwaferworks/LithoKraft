// Swivelor, revised conception (JD 2026-11-11 design session section 4 + CloudAtlas row B "Gear ring + resin
// mount", approved 10:28 PM ET). Engine id lithokraft:swivelor. Spelling is SWIVELOR (never Swiveler). Full block.
#pragma once
#include <string>
#include "components.h"
#include "swivelor_detent.h"
namespace lithokraft::components {
/// STATED: rotary base; the resin-bonded body on top rides the turn; always clockwise from above (64 then 0 is
///         a full clockwise turn, never a rewind); detents 0 / 16 / 32 / 64, one quarter turn each, 0 = home;
///         it checks its swept area before turning and has collision.
/// OPEN (JD): which face takes the detent strength -- there is NO control port. The four side wells are
///            reserved and unassigned: this block reads no input on any face and emits nothing.
/// OPEN (JD): connection cap; whether this revises the 2026-11-09 fine-aim spec or is a second mode.
/// READ-BACK (unconfirmed): lower targets are reached by wrapping clockwise (16 > 32 > 64 > 0).
/// Not implemented (needs the OPEN control face and Resin): turning, carrying an assembly, the clearance
/// check, collision. The block holds its detent (state value; home 0 on placement).
class cSwivelor final : public cCompBehavior {
 public:
  ComponentStep tick(const ComponentInputs& in, CellState st, ComponentMemory& m) const override;
  int emitToward(int, CellState, const ComponentMemory&) const override { return 0; }
  std::string describe(const BlockDef& d, CellState s) const override;
};
}  // namespace lithokraft::components
