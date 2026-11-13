// Swivelor (JD 2026-11-12, B9). See swivelor.h.
#include "swivelor.h"
#include <format>
#include "lithokraft/content/signal.h"
namespace lithokraft::components {
ComponentStep cSwivelor::tick(const ComponentInputs&, CellState st, ComponentMemory&) const {
  // OPEN: no control face -- no input is read (wells reserved), so the detent never changes here.
  return {0, 0, st};
}

std::string cSwivelor::describe(const BlockDef& d, CellState s) const {
  const int q = swivelorDetentQuarter(stValue(s));
  if (q < 0) return std::format("{} | detent OPEN ({}) | control face OPEN | facing {}", d.name, stValue(s), facingName(stFacing(s)));
  return std::format("{} | detent {}{} | control face OPEN | facing {}", d.name, stValue(s), q == 0 ? " (home)" : "", facingName(stFacing(s)));
}
}  // namespace lithokraft::components
