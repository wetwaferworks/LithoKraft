#include "lithokraft/game/L0NeighbourHold.h"

#include <cstdlib>
#include <cstring>

#include "litholattice/core/log.h"

namespace lithokraft {

cL0NeighbourHold::cL0NeighbourHold() {
  const char* e = std::getenv("LL_L0_NEIGHBOUR_HOLD");
  enabled_ = !(e && std::strcmp(e, "0") == 0);
}

bool cL0NeighbourHold::neighboursPresent(const IColumnPresence& world, int cx, int cz) {
  // All 8 Moore neighbours: P8 bumps a column's BorderGen for corner arrivals too (corner
  // apron cells feed AO), so waiting only for the 4 edges still left ~4 corner re-meshes.
  for (int dz = -1; dz <= 1; ++dz)
    for (int dx = -1; dx <= 1; ++dx)
      if ((dx || dz) && !world.hasColumn(cx + dx, cz + dz)) return false;
  return true;
}

cL0NeighbourHold::~cL0NeighbourHold() {
  if (heldTotal_)
    litholattice::logInfo("l0 neighbour hold: %llu held, %llu released by neighbours, %llu by expiry",
                          (unsigned long long)heldTotal_, (unsigned long long)releasedByNeighbours_,
                          (unsigned long long)releasedByTimeout_);
}

uint64_t cL0NeighbourHold::maxTicksFor(int targetTps) {
  const uint64_t t = targetTps > 0 ? uint64_t(kMaxSeconds) * uint64_t(targetTps) : kMaxTicks;
  return t > kMaxTicks ? t : kMaxTicks;
}

void cL0NeighbourHold::hold(uint64_t key, uint64_t tick) {
  if (held_.emplace(key, tick).second) ++heldTotal_;
}

void cL0NeighbourHold::collectReady(const IColumnPresence& world, uint64_t tick, std::vector<uint64_t>& out) {
  for (auto it = held_.begin(); it != held_.end();) {
    const int cx = int(int32_t(uint32_t(it->first >> 32))), cz = int(int32_t(uint32_t(it->first)));
    const bool ready = neighboursPresent(world, cx, cz);
    const bool expired = !ready && tick - it->second >= maxTicks_;
    if (!ready && !expired) { ++it; continue; }
    ++(ready ? releasedByNeighbours_ : releasedByTimeout_);
    out.push_back(it->first);
    it = held_.erase(it);
  }
}

}  // namespace lithokraft
