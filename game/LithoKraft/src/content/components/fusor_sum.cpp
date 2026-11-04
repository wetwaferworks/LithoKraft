// Fusor (JD PlayN 2026-11-01): signal adder. See fusor_sum.h.
#include "fusor_sum.h"
#include <algorithm>
#include <format>
#include "lithokraft/content/signal.h"
namespace lithokraft::components {
ComponentStep cFusorSum::tick(const ComponentInputs& in, CellState st, ComponentMemory&) const {
  const int f = stFacing(st);
  const int back = std::min(std::max(in.back(f), int(in.switchIn)), kMaxSignal);
  const int side = std::min(std::max(in.left(f), in.right(f)), kMaxSignal);
  const int out = std::min(back + side, kMaxSignal);
  return {out, 0, makeState(out, f, 0, out > 0)};
}
std::string cFusorSum::describe(const BlockDef& d, CellState s) const {
  return std::format("{} | out {} (back + side, max 64) | facing {}", d.name, stValue(s), facingName(stFacing(s)));
}
}  // namespace lithokraft::components