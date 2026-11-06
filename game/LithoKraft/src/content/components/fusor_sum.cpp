// Fusor (JD PlayN 2026-11-01): signal adder. See fusor_sum.h.
#include "fusor_sum.h"
#include <algorithm>
#include <format>
#include "lithokraft/content/signal.h"
namespace lithokraft::components {
ComponentStep cFusorSum::tick(const ComponentInputs& in, CellState st, ComponentMemory&) const {
  const int f = stFacing(st);
  const int back = std::min(std::max(in.back(f), int(in.switchIn)), kMaxSignal);
  // 2026-11-04 (JD): back + BOTH sides (was back + the strongest side), capped at 64.
  const int out = std::min(back + std::min(in.left(f), kMaxSignal) + std::min(in.right(f), kMaxSignal), kMaxSignal);
  return {out, 0, makeState(out, f, 0, out > 0)};
}
std::string cFusorSum::describe(const BlockDef& d, CellState s) const {
  return std::format("{} | out {} (back + left + right, max 64) | facing {}", d.name, stValue(s), facingName(stFacing(s)));
}
}  // namespace lithokraft::components