#pragma once
// Fix (b) re-mesh churn (2026-10-30): hold a streamed column's FIRST L0 mesh until its
// 8 horizontal (Moore) neighbours are resident (the 4 edges, plus the 4 corners whose
// arrival also re-meshes it under P8). Before, each arriving neighbour bumped the
// column's BorderGen and re-meshed it, so a streamed column was meshed ~4.4 times
// (LL_WORK_TRACE: 77% of mesh jobs redundant). Columns on the streaming frontier would
// wait forever, so a hold expires after kMaxTicks sim ticks (2 s at the default 32 TPS;
// sim ticks, never wall time; the promote budget is counted the same way) and meshes with the neighbours it has, exactly as before the hold.
// Mesh bytes for a given world state are unchanged: only WHEN the first mesh runs moves.
// LL_L0_NEIGHBOUR_HOLD=0 disables it (A/B).
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "lithokraft/game/IColumnPresence.h"

namespace lithokraft {

class cL0NeighbourHold {
 public:
  static constexpr uint64_t kMaxTicks = 64;  ///< floor: 2 s at the reference 32 TPS
  static constexpr int kMaxSeconds = 2;       ///< expiry = kMaxSeconds of sim ticks at the target rate (never wall time)
  /// Expiry in sim ticks for a target tick rate: max(kMaxTicks, kMaxSeconds * tps).
  static uint64_t maxTicksFor(int targetTps);
  void setMaxTicks(uint64_t n) { maxTicks_ = n; }
  uint64_t maxTicks() const { return maxTicks_; }

  cL0NeighbourHold();
  ~cL0NeighbourHold();  ///< logs the hold/release counters once
  bool enabled() const { return enabled_; }
  bool empty() const { return held_.empty(); }
  bool isHeld(uint64_t key) const { return held_.count(key) != 0; }

  /// True when the column's 8 Moore neighbours are all resident.
  static bool neighboursPresent(const IColumnPresence& world, int cx, int cz);
  /// Start holding a column (key = ChunkKey::make(cx, cz)) at sim tick `tick`.
  void hold(uint64_t key, uint64_t tick);
  /// Move the columns whose neighbours arrived (or whose hold expired) into `out`.
  void collectReady(const IColumnPresence& world, uint64_t tick, std::vector<uint64_t>& out);
  void forget(uint64_t key) { held_.erase(key); }
  void clear() { held_.clear(); }

  uint64_t heldTotal() const { return heldTotal_; }
  uint64_t releasedByNeighbours() const { return releasedByNeighbours_; }
  uint64_t releasedByTimeout() const { return releasedByTimeout_; }

 private:
  std::unordered_map<uint64_t, uint64_t> held_;  ///< column key -> sim tick it was held at
  bool enabled_ = true;
  uint64_t maxTicks_ = kMaxTicks;
  uint64_t heldTotal_ = 0, releasedByNeighbours_ = 0, releasedByTimeout_ = 0;
};

}  // namespace lithokraft
