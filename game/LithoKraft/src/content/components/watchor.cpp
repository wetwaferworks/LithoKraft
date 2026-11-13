// Watchor (JD 2026-11-12, B9). See watchor.h.
#include "watchor.h"
#include <format>
#include "lithokraft/content/signal.h"
namespace lithokraft::components {
namespace {
// Memory (no new saved fields; reuses the SGNL v3 ones, like the Repeator's 64-tick line):
//   historyHi           = last observed state of the watched cell
//   prevIn | prevSide<<8 = last observed block id of the watched cell
//   history bit 0       = primed (a first observation exists)
//   history bit 1       = fired last tick (this tick is the rest tick)
constexpr uint8_t kPrimed = 1u, kFiredLast = 2u;
}  // namespace

ComponentStep cWatchor::tick(const ComponentInputs& in, CellState st, ComponentMemory& m) const {
  const int f = stFacing(st);
  const bool rest = (m.history & kFiredLast) != 0;
  bool fire = false;
  if (in.watchValid) {
    const BlockId lastId = BlockId(unsigned(m.prevIn) | (unsigned(m.prevSide) << 8));
    const CellState lastSt = CellState(m.historyHi);
    if (m.history & kPrimed) {
      const bool changed = lastId != in.watchId || lastSt != in.watchState;
      // OPEN (JD design session section 6 / section 9 question 4, pack README): a block appearing
      // where air was, or a block being removed, is NOT decided. Not sealed here: no pulse for that
      // change in this pass, the snapshot just follows the world. A resin sweep is OPEN too (no
      // Resin / Pushor exists yet). Decide before giving these a behaviour.
      const bool openEdge = lastId != in.watchId && (lastId == kAir || in.watchId == kAir);
      fire = changed && !openEdge && !rest;  // LOCKED: a change on the rest tick produces no pulse
    }
    m.historyHi = uint16_t(in.watchState);
    m.prevIn = uint8_t(in.watchId & 0xFFu);
    m.prevSide = uint8_t((in.watchId >> 8) & 0xFFu);
    m.history = uint8_t(kPrimed | (fire ? kFiredLast : 0u));
  } else {
    m.history = uint8_t(m.history & ~kFiredLast);
  }
  // LOCKED: full strength 64 for exactly one tick. On = firing (the row B art lights only the
  // back output tint for that tick; the eye does not change).
  const int out = fire ? kMaxSignal : 0;
  return {out, 0, makeState(0, f, 0, fire)};
}

int cWatchor::emitToward(int dir, CellState st, const ComponentMemory& m) const {
  return dir == dirBack(stFacing(st)) ? m.out : 0;  // output out the BACK only; front = eye, sides inert
}

std::string cWatchor::describe(const BlockDef& d, CellState s) const {
  return std::format("{} | {} | facing {}", d.name, stOn(s) ? "out 64" : "watching", facingName(stFacing(s)));
}
}  // namespace lithokraft::components
