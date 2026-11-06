// Dividor (JD 2026-11-05, art CloudAtlas components-v2): signal halver. Id lithokraft:dividor.
#pragma once
#include <string>
#include "components.h"
namespace lithokraft::components {
/// Front = floor(back / 2) (integer shift, rounds down), 1-tick delay. 0 in gives 0 out (no quirk modes:
/// JD "logical, not mystical"). Only the back port is read; the sides are ignored. State value = the output.
class cDividor final : public cCompBehavior {
 public:
  int delayTicks(CellState, int64_t, uint64_t) const override { return 1; }
  ComponentStep tick(const ComponentInputs& in, CellState st, ComponentMemory& m) const override;
  std::string describe(const BlockDef& d, CellState s) const override;
};
}  // namespace lithokraft::components
