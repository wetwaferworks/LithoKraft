// Readout-only behaviours for blocks driven directly by the signal propagation (signal.cpp).
#include <format>
#include "components.h"
#include "lithokraft/content/signal.h"

namespace lithokraft::components {
namespace {

class cWire final : public cBlockBehavior {
 public:
  std::string describe(const BlockDef& d, CellState s) const override {
    return std::format("{} | strength {}", d.name, stValue(s));
  }
  /// 2026-11-05 (JD): Trace colour property (same path LEDs use via cTorch).
  int colour(const BlockDef& def) const override { return int(def.channel); }
};

/// Plain lamps (LEDs use cTorch): state 1 = lit.
class cOnOff final : public cBlockBehavior {
 public:
  std::string describe(const BlockDef& d, CellState s) const override {
    return std::format("{} | {}", d.name, (s & 1) ? "ON" : "off");
  }
};

class cColorLamp final : public cBlockBehavior {
 public:
  std::string describe(const BlockDef& d, CellState s) const override {
    const int v = stValue(s);
    if (!v) return std::format("{} | tier {} | off", d.name, d.tier);
    return std::format("{} | tier {} (wrap) | strength {} -> colour {}", d.name, d.tier, v, colorLampIndex(v, d.tier));
  }
};

}  // namespace

void registerPassive(const Registrar& add) {
  add("wire", std::make_unique<cWire>());
  add("on_off", std::make_unique<cOnOff>());
  add("color_lamp", std::make_unique<cColorLamp>());
}

}  // namespace lithokraft::components
