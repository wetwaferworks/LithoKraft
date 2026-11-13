// Randor (JD 2026-10-31): random range block.
#pragma once
#include <string>
#include "components.h"
#include "lithokraft/content/randor_rng.h"  // 2026-11-04 delay roll
namespace lithokraft::components {
/// Back = range N (1..64), left / right = clock: each rising edge rolls kLow..N (randor_rng.h).
/// Front = the roll, held at that strength until the next roll (not a pulse). State value = N (the
/// top readout shows the range, never the roll); the roll lives in ComponentMemory::history (saved).
class cRandor final : public cCompBehavior {
 public:
  /// PlayB (JD t4331u): fixed 8 ticks input -> output (was a random 2..64 per event). The RNG still
  /// decides the rolled value (tick()); only the delay is fixed.
  int delayTicks(CellState, int64_t, uint64_t) const override { return 8; }
  /// Pure in inputs + edges (the cMemor / cPulsor pattern): the roll happens only on a clock
  /// edge, prevClk persists in memory, and every input change revives it — a changed neighbour
  /// output wakes its neighbours, marked nets wake their readers, soft-power changes wake the
  /// components around the block — while a pending scheduled output is never skipped.
  bool quiescent(CellState, const ComponentMemory&) const override { return true; }
  ComponentStep tick(const ComponentInputs& in, CellState st, ComponentMemory& m) const override;
  std::string describe(const BlockDef& d, CellState s) const override;
};
}  // namespace lithokraft::components
