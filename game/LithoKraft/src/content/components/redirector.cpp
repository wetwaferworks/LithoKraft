// 2026-11-04 (JD t4302u): Redirector. See redirector.h.
#include "redirector.h"
#include <algorithm>
#include <format>
#include "lithokraft/content/signal.h"
namespace lithokraft::components {
CellState cRedirectorSelect::withSelected(CellState s, int sel) {
  return CellState(makeState(stValue(s), stFacing(s), sel & 3, stOn(s)));
}
ComponentStep cRedirectorSelect::tick(const ComponentInputs& in, CellState st, ComponentMemory&) const {
  const int f = stFacing(st), sel = selected(st);
  const int v = sel == kLeft ? in.left(f) : sel == kRight ? in.right(f) : in.back(f);
  const int out = std::clamp(v, 0, kMaxSignal);
  return {out, 0, makeState(out, f, sel, out > 0)};
}
std::optional<CellState> cRedirectorSelect::use(CellState st, ComponentMemory&) const {
  return withSelected(st, (selected(st) + 1) % 3);
}
std::optional<CellState> cRedirectorSelect::useAlt(CellState st, ComponentMemory& m) const { return use(st, m); }
std::string cRedirectorSelect::describe(const BlockDef& d, CellState s) const {
  static constexpr const char* kNames[3] = {"back", "left", "right"};
  return std::format("{} | input {} -> front | out {} | facing {}", d.name, kNames[selected(s)], stValue(s), facingName(stFacing(s)));
}
}  // namespace lithokraft::components
