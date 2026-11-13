// Fusor (JD PlayN 2026-11-01): signal adder. The old circuit-breaker Fusor is now the Breakor (cFusor class, id lithokraft:fusor).
#pragma once
#include <string>
#include "components.h"
namespace lithokraft::components {
/// Front = min(64, back + left + right) (2026-11-04 JD: both sides; was back + strongest side). Inputs are read like every component (trace, Switch,
/// hard / soft powered blocks). State value = the output (readout).
class cFusorSum final : public cCompBehavior {
 public:
  int delayTicks(CellState, int64_t, uint64_t) const override { return 6; }  // 2026-11-04 (JD t4304u)
  /// Pure function of the current inputs (tick() reads no memory and tracks no edges), so it
  /// is quiescent whenever its inputs stand still; the wake system (changed neighbours, marked
  /// nets, soft-power wakes) revives it on any input change, and a pending scheduled output
  /// is never skipped.
  bool quiescent(CellState, const ComponentMemory&) const override { return true; }
  ComponentStep tick(const ComponentInputs& in, CellState st, ComponentMemory& m) const override;
  std::string describe(const BlockDef& d, CellState s) const override;
};
}  // namespace lithokraft::components