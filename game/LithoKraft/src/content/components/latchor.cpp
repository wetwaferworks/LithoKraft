// Latchor (2026-10-31, JD; was cFastLatch in logic_components.cpp).
#include "latchor.h"
#include <cstdlib>
#include <format>
#include "lithokraft/content/signal.h"

namespace lithokraft::components {

// Encoding keeps the legacy Fast Latch states bit-identical (sim goldens): the default value 64 is
// stored as the old output (0 off / 64 on); a value 1..63 is stored as itself in both states.
int cLatchor::configured(CellState s) {
  const int v = stValue(s);
  return v > 0 && v < kMaxSignal ? v : kMaxSignal;
}
static CellState encodeLatchor(int value, int facing, bool on) {
  return makeState(value >= kMaxSignal ? (on ? kMaxSignal : 0) : value, facing, 0, on);
}
ComponentStep cLatchor::tick(const ComponentInputs& in, CellState st, ComponentMemory& m) const {
  const int f = stFacing(st);
  // 2026-10-31 (JD): the Latchor fires only on a tick whose back input is EXACTLY its set value
  // (1..64); any other value outputs nothing. LL_LATCHOR_MATCH=0 restores the old toggle latch.
  static const bool match = [] { const char* e = std::getenv("LL_LATCHOR_MATCH"); return !(e && e[0] == '0'); }();
  if (match) {
    const int v = configured(st);
    const bool hit = in.back(f) == v;
    m.prevIn = hit;
    return {hit ? v : 0, 0, encodeLatchor(v, f, hit)};
  }
  const bool high = in.back(f) > 0;
  const bool on = (high && !m.prevIn) ? !stOn(st) : stOn(st);
  m.prevIn = high;
  const int v = configured(st);
  return {on ? v : 0, 0, encodeLatchor(v, f, on)};
}
std::optional<CellState> cLatchor::useAlt(CellState st, ComponentMemory& /*m*/) const {
  return encodeLatchor(configured(st) % kMaxSignal + 1, stFacing(st), stOn(st));
}
std::string cLatchor::describe(const BlockDef& d, CellState s) const {
  return std::format("{} | {} | value {} | facing {} | sneak+use: set value", d.name, stOn(s) ? "ON" : "off", configured(s), facingName(stFacing(s)));
}

}  // namespace lithokraft::components
