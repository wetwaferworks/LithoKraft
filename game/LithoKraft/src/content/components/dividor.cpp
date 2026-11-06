// Dividor (JD 2026-11-05): signal halver. See dividor.h.
#include "dividor.h"
#include <algorithm>
#include <format>
#include "lithokraft/content/signal.h"
namespace lithokraft::components {
ComponentStep cDividor::tick(const ComponentInputs& in, CellState st, ComponentMemory&) const {
  const int f = stFacing(st);
  const int back = std::clamp(int(in.back(f)), 0, kMaxSignal);
  const int out = back >> 1;  // floor(back / 2): 64 -> 32, 3 -> 1, 1 -> 0, 0 -> 0
  return {out, 0, makeState(out, f, 0, out > 0)};
}
std::string cDividor::describe(const BlockDef& d, CellState s) const {
  return std::format("{} | out {} (back / 2, rounded down) | facing {}", d.name, stValue(s), facingName(stFacing(s)));
}
}  // namespace lithokraft::components
