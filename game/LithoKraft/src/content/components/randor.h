// Randor (JD 2026-10-31): random range block.
#pragma once
#include <string>
#include "components.h"
namespace lithokraft::components {
/// Back = range N (1..64), left / right = clock: each rising edge rolls kLow..N (randor_rng.h).
/// Front = the roll, held at that strength until the next roll (not a pulse). State value = N (the
/// top readout shows the range, never the roll); the roll lives in ComponentMemory::history (saved).
class cRandor final : public cCompBehavior {
 public:
  ComponentStep tick(const ComponentInputs& in, CellState st, ComponentMemory& m) const override;
  std::string describe(const BlockDef& d, CellState s) const override;
};
}  // namespace lithokraft::components
