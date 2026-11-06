// PlayB (JD t4343u-t4349u): Routor. See routor.h.
#include "routor.h"
#include <algorithm>
#include <format>
#include "lithokraft/content/signal.h"
namespace lithokraft::components {
ComponentStep cRoutor::tick(const ComponentInputs& in, CellState st, ComponentMemory& m) const {
  const int f = stFacing(st), md = mode(st);
  const int back = std::clamp(in.back(f), 0, kMaxSignal), left = std::clamp(in.left(f), 0, kMaxSignal),
            right = std::clamp(in.right(f), 0, kMaxSignal);
  int a = back, b = 0, bdir = 0;
  if (md == kMode1) b = right;
  else if (md == kMode2) b = left;
  else {  // mode 3: the side path keeps the side that went live first; its output side is never read
    int dir = m.timer & 3, rest = m.timer >> 2;
    if (dir == kToLeft && right > 0) b = right;
    else if (dir == kToRight && left > 0) b = left;
    else {
      if (dir != 0) { dir = 0; rest = 3; }
      if (rest > 0) --rest;
      else if (right > 0) { dir = kToLeft; b = right; }
      else if (left > 0) { dir = kToRight; b = left; }
    }
    m.timer = uint8_t((rest << 2) | dir);
    bdir = b > 0 ? dir : 0;
  }
  // Publish last tick's paths (1-tick delay), queue this tick's.
  const int outA = m.prevIn, outB = m.prevSide;
  m.history = m.prevClk;
  m.prevIn = uint8_t(a); m.prevSide = uint8_t(b); m.prevClk = uint8_t(bdir);
  const int v = std::max(outA, outB);
  return {outA, outB, makeState(v, f, md, v > 0)};
}
int cRoutor::emitToward(int dir, CellState st, const ComponentMemory& m) const {
  const int f = stFacing(st), md = mode(st), l = dirLeft(f), r = dirRight(f);
  if (md == kMode1) return dir == l ? m.out : dir == f ? m.out2 : 0;
  if (md == kMode2) return dir == r ? m.out : dir == f ? m.out2 : 0;
  if (dir == f) return m.out;
  if (dir == l && m.history == kToLeft) return m.out2;
  if (dir == r && m.history == kToRight) return m.out2;
  return 0;
}
std::optional<CellState> cRoutor::useAlt(CellState st, ComponentMemory& m) const {
  m.prevIn = m.prevSide = m.prevClk = 0; m.history = 0; m.timer = 0;  // a new routing starts empty
  return CellState(makeState(0, stFacing(st), (mode(st) + 1) % 3, false));
}
std::string cRoutor::describe(const BlockDef& d, CellState s) const {
  static constexpr const char* kNames[3] = {"mode 1: back -> left, right -> front", "mode 2: back -> right, left -> front",
                                            "mode 3: back -> front, left <-> right"};
  return std::format("{} | {} | out {} | facing {}", d.name, kNames[mode(s)], stValue(s), facingName(stFacing(s)));
}
}  // namespace lithokraft::components
