// Dialor (JD 2026-11-11, 1:1 rulings + CloudAtlas concept C): independent 6-bit
// signal source. Id lithokraft:dialor. Spelling is DIALOR (never Dialer).
#pragma once
#include <optional>
#include <string>
#include "components.h"
namespace lithokraft::components {
/// C9 2026-11-12 (JD): the top dial selects 0..64 (65 positions; 0 = no signal). A newly placed
/// Dialor (state value 0) selects 0. use() = +1, useAlt() = -1, wrapping 0 <-> 64.
/// (B9 was 1..64 with no 0 position.)
/// Output is the dial value, not an incoming signal. When the block faces north,
/// output is on the north face (front); faces rotate with facing.
/// Only the back inhibits: out = back_powered ? 0 : dial. East and west are inert.
/// One-tick delay (JD 1:1, "At one tick"). 17th reserved Trace keyword.
class cDialor final : public cCompBehavior {
 public:
  int delayTicks(CellState, int64_t, uint64_t) const override { return 1; }
  ComponentStep tick(const ComponentInputs& in, CellState st, ComponentMemory& m) const override;
  std::optional<CellState> use(CellState st, ComponentMemory& m) const override;
  std::optional<CellState> useAlt(CellState st, ComponentMemory& m) const override;
  std::string describe(const BlockDef& d, CellState s) const override;
};
}  // namespace lithokraft::components
