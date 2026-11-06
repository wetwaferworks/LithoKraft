// Latchor (2026-10-31, JD; was the Fast Latch): T flip-flop with a configurable output 1..64.
#pragma once
#include <optional>
#include <string>
#include "components.h"

namespace lithokraft::components {

/// Each rising edge at the back toggles; while on, the front outputs the configured value.
/// The configured value is the state value (1..64; 0 = legacy save -> 64). Sneak+use steps it 1..64.
class cLatchor final : public cCompBehavior {
 public:
  int delayTicks(CellState, int64_t, uint64_t) const override { return 2; }  // 2026-11-04 (JD t4304u)
  static int configured(CellState s);
  ComponentStep tick(const ComponentInputs& in, CellState st, ComponentMemory& m) const override;
  std::optional<CellState> useAlt(CellState st, ComponentMemory& m) const override;
  std::string describe(const BlockDef& d, CellState s) const override;
};

}  // namespace lithokraft::components
