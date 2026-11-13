// Bitor (JD 2026-11-12, B9). See bitor.h.
#include "bitor.h"
#include <algorithm>
#include <format>
#include "lithokraft/content/signal.h"
namespace lithokraft::components {
namespace {
int clampStrength(int v) { return std::clamp(v, 0, kMaxSignal); }
constexpr uint8_t kAuxControlLit = 1u;  // CloudAtlas art: control tint lit when control strength > 0
}  // namespace

int bitorAnd(int west, int east) {
  const int r = clampStrength(west) & clampStrength(east);
  return r > kMaxSignal ? kMaxSignal : r;  // RULED: over-range saturates at 64, never 0
}

int bitorOpFor(int control) {
  // LOCKED (JD 6:39 AM ET 2026-11-12): 0-15 AND, 16-31 OR, 32-47 XOR, 48-64 NOR.
  const int c = clampStrength(control);
  return c < 16 ? 0 : c < 32 ? 1 : c < 48 ? 2 : 3;
}

int bitorApply(int op, int west, int east) {
  const int a = clampStrength(west), b = clampStrength(east);
  int r = 0;
  switch (op & 3) {
    case 0: r = a & b; break;
    case 1: r = a | b; break;
    case 2: r = a ^ b; break;
    default: r = (~(a | b)) & 0x7f; break;  // NOR over the 7 bit places 1..64 (C9 reading of "bit patterns")
  }
  return r > kMaxSignal ? kMaxSignal : r;  // RULED: over-range saturates at 64, never 0
}

ComponentStep cBitor::tick(const ComponentInputs& in, CellState st, ComponentMemory& m) const {
  const int f = stFacing(st);
  const int west = clampStrength(in.left(f)), east = clampStrength(in.right(f));  // local west / east
  const int control = clampStrength(in.back(f));
  // C9 (JD 6:44 AM ET): recompute two ticks after a NEW value on west / east / back. Last seen
  // inputs: prevIn (west), prevSide (east), prevClk (control); timer = ticks left (0 = none pending).
  // A change seen at tick T publishes at T + 2; a further change restarts the two ticks (INTERPRETATION).
  // The settle pass re-runs from the pre-tick memory snapshot, so this advances once per tick.
  int out = m.out, op = bitorOpOf(st);
  if (west != m.prevIn || east != m.prevSide || control != m.prevClk) {
    m.prevIn = uint8_t(west); m.prevSide = uint8_t(east); m.prevClk = uint8_t(control);
    m.timer = uint8_t(kBitorDelayTicks);
  } else if (m.timer > 0 && --m.timer == 0) {
    op = bitorOpFor(m.prevClk);
    out = bitorApply(op, m.prevIn, m.prevSide);
  }
  out = clampStrength(out);
  return {out, 0, makeState(out, f, (control > 0 ? kAuxControlLit : 0) | (op << 1), out > 0)};
}

std::string cBitor::describe(const BlockDef& d, CellState s) const {
  static constexpr const char* kOp[4] = {"AND", "OR", "XOR", "NOR"};
  return std::format("{} | {} | out {} | facing {}", d.name, kOp[bitorOpOf(s)], stValue(s), facingName(stFacing(s)));
}
}  // namespace lithokraft::components
