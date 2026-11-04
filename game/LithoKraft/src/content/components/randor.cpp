// Randor (JD 2026-10-31): random range block. See randor.h.
#include "randor.h"
#include <algorithm>
#include <cstdlib>
#include <format>
#include "lithokraft/content/randor_rng.h"
#include "lithokraft/content/signal.h"
namespace lithokraft::components {
ComponentStep cRandor::tick(const ComponentInputs& in, CellState st, ComponentMemory& m) const {
  const int f = stFacing(st);
  const int n = std::clamp(in.back(f), std::max(randor::kLow, 1), kMaxSignal);
  const int clock = std::max(in.left(f), in.right(f));
  static const bool kNoRepeat = [] { const char* e = std::getenv("LL_RANDOR_NOREPEAT"); return !(e && e[0] == '0'); }();  // kill: =0
  if (clock > 0 && m.prevClk == 0)
    m.history = uint8_t(kNoRepeat ? randor::rollNoRepeat(in.rng, n, m.history) : randor::roll(in.rng, n));
  m.prevClk = uint8_t(std::min(clock, 255));
  const int out = m.history;
  return {out, 0, makeState(n, f, 0, out > 0)};
}
std::string cRandor::describe(const BlockDef& d, CellState s) const {
  return std::format("{} | range {}..{} | facing {}", d.name, randor::kLow, std::max(stValue(s), std::max(randor::kLow, 1)), facingName(stFacing(s)));
}
}  // namespace lithokraft::components
