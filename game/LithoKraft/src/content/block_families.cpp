// 2026-10-31 (JD): see block_families.h.
#include "lithokraft/content/block_families.h"
#include <algorithm>

namespace lithokraft {

const cBlockFamilies& cBlockFamilies::get() {
  static const cBlockFamilies families;  // built once, immutable afterwards
  return families;
}

const char* cBlockFamilies::name(BlockFamily f) {
  switch (f) {
    case BlockFamily::Trace: return "Trace";
    case BlockFamily::Lamp: return "Lamps/LEDs";
    case BlockFamily::Building: return "Building";
    case BlockFamily::Other: return "Other";
    default: return "Unassigned";
  }
}

cBlockFamilies::cBlockFamilies() {
  static constexpr SimRole kTraceOrder[] = {
      SimRole::Wire, SimRole::Pulsor, SimRole::Memor, SimRole::Randor, SimRole::cFusorSum, SimRole::Dividor, SimRole::Repeator, SimRole::Thresholdor,
      SimRole::Redirector, SimRole::Latchor, SimRole::Catchor, SimRole::Switch, SimRole::cButton, SimRole::Keyboard,
      SimRole::Tone, SimRole::cSignalTransmittor, SimRole::cSignalReceivor, SimRole::cDish, SimRole::cOverclockor,
      SimRole::cPortor, SimRole::cFusor, SimRole::Chipor, SimRole::Lithographor, SimRole::Etchor,
      SimRole::Dialor, SimRole::Watchor, SimRole::Bitor, SimRole::Pushor, SimRole::Swivelor};  // Swivelor + Pushor + Bitor + Watchor appended B9 2026-11-12 (JD); Dialor appended 2026-11-11 (JD): 17th reserved Trace keyword; do not insert mid-list
  const auto& reg = blockRegistry();
  family_.assign(reg.size(), BlockFamily::Unassigned);
  rank_.assign(reg.size(), 0);
  for (size_t id = 1; id < reg.size(); ++id) {
    const BlockDef& d = reg.def(BlockId(id));
    const SimRole r = roleOf(d);
    BlockFamily f = BlockFamily::Other;
    int rk = 0;
    bool trace = false;
    for (SimRole t : kTraceOrder) { if (t == r) { trace = true; break; } ++rk; }
    if (trace) f = BlockFamily::Trace;
    else if (r == SimRole::Lamp || r == SimRole::ColorLamp || r == SimRole::Torch) f = BlockFamily::Lamp;
    else if (r == SimRole::None && (d.flags & kSolid)) f = BlockFamily::Building;
    family_[id] = f;
    rank_[id] = trace ? rk : 0;
  }
  for (size_t id = 1; id < reg.size(); ++id) {
    if (family_[id] == BlockFamily::Unassigned) unassigned_.push_back(BlockId(id));
    else members_[size_t(family_[id])].push_back(BlockId(id));
  }
  for (auto& m : members_)
    std::stable_sort(m.begin(), m.end(), [this](BlockId a, BlockId b) { return rank_[size_t(a)] < rank_[size_t(b)]; });
}

}  // namespace lithokraft
