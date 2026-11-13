// Pushor (JD 2026-11-12, B9). See pushor.h.
#include "pushor.h"
#include <algorithm>
#include <format>
#include "lithokraft/content/signal.h"
namespace lithokraft::components {
ComponentStep cPushor::tick(const ComponentInputs& in, CellState st, ComponentMemory&) const {
  const int f = stFacing(st);
  // Five Trace faces: back, east, west (horizontal inputs), top (wire on top + the block above), bottom (the
  // block below). The front (resin pushing face) is not read.
  const int p = std::max({in.back(f), in.left(f), in.right(f), in.top, in.pushVert});
  const bool powered = p > 0 || in.pushQuasi;
  // OPEN: extension distance (and with it spit / push / retract, immovable blocks, resin assemblies).
  return {0, 0, makeState(0, f, in.pushHard ? kPushorAuxHard : 0u, powered)};
}

std::string cPushor::describe(const BlockDef& d, CellState s) const {
  const char* p = !stOn(s) ? "unpowered" : (stAux(s) & kPushorAuxHard) ? "hard powered" : "powered";
  return std::format("{} | {} | push distance OPEN | facing {}", d.name, p, facingName(stFacing(s)));
}
}  // namespace lithokraft::components
