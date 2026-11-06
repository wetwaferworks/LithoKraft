// PlayB (JD t4343u-t4349u): Routor, the old Redirector (lithokraft:redirector_select) turned into a
// signal crossover. Two independent paths that never mix, 1 tick each, hard power like every component.
#pragma once
#include <string>
#include "components.h"
namespace lithokraft::components {
/// Mode = aux bits 0-1 (relative to the facing), Alt + right click cycles 1 -> 2 -> 3 (saved in the cell state):
///   mode 1: back in -> left out,  right in -> front out
///   mode 2: back in -> right out, left in  -> front out (mirror)
///   mode 3: back in -> front out, left <-> right (the side path passes across; the side that went
///           live first drives, the other is its output, 3-tick rest after it drops so it cannot echo).
/// Own 1-tick delay line in ComponentMemory (queuedOutput() false; lock-free, the sim owns the memory):
/// prevIn / prevSide / prevClk = this tick's path A / path B / B direction, published next tick;
/// out / out2 / history = the published path A / path B / B direction; timer = mode-3 side state.
class cRoutor final : public cCompBehavior {
 public:
  static constexpr int kMode1 = 0, kMode2 = 1, kMode3 = 2;
  static constexpr int kToLeft = 1, kToRight = 2;  ///< mode-3 side path direction
  static int mode(CellState s) { const int v = int(stAux(s) & 3); return v > kMode3 ? kMode1 : v; }
  int delayTicks(CellState, int64_t, uint64_t) const override { return 1; }
  bool queuedOutput() const override { return false; }  // own 1-tick line in tick()
  ComponentStep tick(const ComponentInputs& in, CellState st, ComponentMemory& m) const override;
  int emitToward(int dir, CellState st, const ComponentMemory& m) const override;
  std::optional<CellState> useAlt(CellState st, ComponentMemory& m) const override;
  std::string describe(const BlockDef& d, CellState s) const override;
};
}  // namespace lithokraft::components
