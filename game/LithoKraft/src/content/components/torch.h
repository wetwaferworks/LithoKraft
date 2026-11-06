// LED (Torch) block behaviour: lit readout + colour passed to neighbouring components.
// 2026-11-05 (JD): colour lives on the class (like Trace / cWire), not a free helper.
#pragma once
#include <format>
#include <string>
#include "lithokraft/content/component.h"

namespace lithokraft::components {

class cTorch final : public cBlockBehavior {
 public:
  std::string describe(const BlockDef& d, CellState s) const override {
    return std::format("{} | {}", d.name, (s & 1) ? "ON" : "off");
  }
  /// Same channel field Trace uses (1 red, 2 blue). Components read via cBehaviorCatalog::colour.
  int colour(const BlockDef& def) const override { return int(def.channel); }
};

}  // namespace lithokraft::components
