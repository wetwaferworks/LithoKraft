// Dialor (JD 2026-11-11): independent 6-bit source. See dialor.h.
#include "dialor.h"
#include <format>
#include "lithokraft/content/signal.h"
namespace lithokraft::components {
namespace {
int clampDial(int v) {
  // C9 2026-11-12 (JD): 0 IS a dial position (no signal); a fresh state (value 0) selects 0.
  // An out-of-range value (> 64) reads as 0. B9 clamped 0 / out-of-range to 1.
  if (v < 0 || v > kMaxSignal) return 0;
  return v;
}
constexpr int kAuxInhibited = 1;  // C9: back powered (the inhibit tint); dial 0 is unlit but not inhibited
}  // namespace

ComponentStep cDialor::tick(const ComponentInputs& in, CellState st, ComponentMemory&) const {
  const int f = stFacing(st);
  const int dial = clampDial(stValue(st));
  const int back = in.back(f);
  // JD 1:1: only the back inhibits. East and west are inert (ignored here).
  // Independent output: the output IS the configured dial, not an incoming signal.
  const int out = back > 0 ? 0 : dial;
  return {out, 0, makeState(dial, f, back > 0 ? kAuxInhibited : 0, out > 0)};
}

std::optional<CellState> cDialor::use(CellState st, ComponentMemory&) const {
  const int f = stFacing(st);
  const int d = clampDial(stValue(st));
  const int next = d >= kMaxSignal ? 0 : d + 1;  // C9: 64 -> 0
  return makeState(next, f, stAux(st), stOn(st));
}

std::optional<CellState> cDialor::useAlt(CellState st, ComponentMemory&) const {
  const int f = stFacing(st);
  const int d = clampDial(stValue(st));
  const int next = d <= 0 ? kMaxSignal : d - 1;  // C9: 0 -> 64
  return makeState(next, f, stAux(st), stOn(st));
}

std::string cDialor::describe(const BlockDef& d, CellState s) const {
  const int dial = clampDial(stValue(s));
  if (stAux(s) & kAuxInhibited)
    return std::format("{} | dial {} | inhibited | facing {}", d.name, dial, facingName(stFacing(s)));
  if (dial == 0) return std::format("{} | dial 0 | no signal | facing {}", d.name, facingName(stFacing(s)));
  if (stOn(s))
    return std::format("{} | dial {} | out {} | facing {}", d.name, dial, dial, facingName(stFacing(s)));
  return std::format("{} | dial {} | inhibited | facing {}", d.name, dial, facingName(stFacing(s)));
}
}  // namespace lithokraft::components
