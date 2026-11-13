// Bitor (JD 2026-11-11 design session section 7 + CloudAtlas row B "Op readout", approved 10:28 PM ET).
// Engine id lithokraft:bitor. Spelling is BITOR (never Biter / BitOr).
#pragma once
#include <string>
#include "components.h"
namespace lithokraft::components {
/// Faces (JD): local WEST and local EAST = the two operands (commutative); BACK = control strength
/// selects the op; FRONT = output. Strengths 0..64 are bit patterns over 1, 2, 4, 8, 16, 32, 64.
/// LOCKED (JD 6:39 AM ET 2026-11-12, Cortana's table): the control strength selects the op by RANGE:
///   0-15 AND, 16-31 OR, 32-47 XOR, 48-64 NOR (the last range is one wider: 0..64 is 65 values).
///   Control 0 (the default) = AND.
/// RULED: an over-range result emits 64 (saturate), never 0; no special look.
/// LOCKED (JD 6:44 AM ET): the output recomputes TWO TICKS after a new value arrives on the west,
/// east or back face. INTERPRETATION (C9, not sealed): a change while a recompute is pending
/// restarts the two ticks from the most recent change (change at T and T+1 -> update at T+3).
/// The armed-op still / lamps follow the op in force on the same two-tick cadence as the output.
/// State: value = the published output; aux bit 0 = control > 0 now (back tint); aux bits 1-2 =
/// the op in force (0 AND, 1 OR, 2 XOR, 3 NOR).
class cBitor final : public cCompBehavior {
 public:
  int delayTicks(CellState, int64_t, uint64_t) const override { return 2; }  // informational: tick() times it
  bool queuedOutput() const override { return false; }  // own two-tick debounce in tick() (restart on change)
  ComponentStep tick(const ComponentInputs& in, CellState st, ComponentMemory& m) const override;
  std::string describe(const BlockDef& d, CellState s) const override;
};
/// The LOCKED op (control 0): bitwise AND of two strengths, saturated at 64 (RULED).
int bitorAnd(int west, int east);
/// C9: the op the control strength selects (0 AND, 1 OR, 2 XOR, 3 NOR) by JD's locked ranges.
int bitorOpFor(int control);
/// C9: op `op` on two strengths (7 bit places 1..64); over-range saturates at 64 (RULED).
int bitorApply(int op, int west, int east);
/// C9: the op in force recorded in a Bitor state (aux bits 1-2).
inline int bitorOpOf(CellState s) { return (stAux(s) >> 1) & 3; }
/// The two-tick recompute delay (JD 6:44 AM ET).
inline constexpr int kBitorDelayTicks = 2;
}  // namespace lithokraft::components
