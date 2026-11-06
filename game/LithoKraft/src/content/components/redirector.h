// 2026-11-04 (JD t4302u): Redirector, split back out of the Multiplexor (which keeps the old
// lithokraft:redirector stable id). One selected input side (back / left / right) goes out the front.
#pragma once
#include <string>
#include "components.h"
namespace lithokraft::components {
/// Selected input = aux bits 0-1 (0 back, 1 left, 2 right; relative to the facing). Use (right click)
/// and Alt-use / Alt + wheel cycle it back -> left -> right. The selected side's strength drives the
/// block in front directly (component output: hard power, any colour). Stateless besides the cell
/// state, so it is lock-free like every behaviour (the sim owns the state).
class cRedirectorSelect final : public cCompBehavior {
 public:
  int delayTicks(CellState, int64_t, uint64_t) const override { return 1; }  // 2026-11-04 (JD t4304u)
  static constexpr int kBack = 0, kLeft = 1, kRight = 2;
  static int selected(CellState s) { const int v = int(stAux(s) & 3); return v > kRight ? kBack : v; }
  static CellState withSelected(CellState s, int sel);
  ComponentStep tick(const ComponentInputs& in, CellState st, ComponentMemory& m) const override;
  std::optional<CellState> use(CellState st, ComponentMemory& m) const override;
  std::optional<CellState> useAlt(CellState st, ComponentMemory& m) const override;
  std::string describe(const BlockDef& d, CellState s) const override;
};
}  // namespace lithokraft::components
