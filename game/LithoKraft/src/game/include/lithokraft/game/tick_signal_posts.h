#pragma once
// t46u item 6b-3b (JD 2026-11-16): the game thread's signal-sim work for a streamed column, posted to the
// tick thread. The signal sim's indexes and component memory (cSignalSim) belong to the tick thread; the
// game thread changes the column set and posts the follow-up (index + restore, or drop) with
// cSimThread::postToTick, so the tick side applies it at the same boundary at which it adopts the new
// column directory.
//
// Ordering rule for game-thread readers of signal state: a column whose posted init has not run yet must
// not have its signal state read (it would be exported without its restored component memory). The pre-
// evict save checks cSignalInitTickets::pending() and defers. The world saves are already ordered: the
// async save snapshots in a boundary job (runs after the posts at that boundary), the sync save stops the
// sim (stop() runs the posts left over), and the LL_HASH_AT_TICK probe runs on the tick thread.
#include <cstdint>
#include <iterator>
#include <unordered_map>
#include <utility>
#include <vector>

#include "litholattice/core/log.h"
#include "litholattice/sim/sim_thread.h"
#include "litholattice/world/chunk_map.h"
#include "lithokraft/content/signal.h"

namespace lithokraft {

/// Indexes column (cx, cz)'s signal cells and, when `blob` is not empty, restores its component memory
/// (the per-column SGNL payload), on the tick thread. Returns the post's ticket.
inline uint64_t postColumnSignalInit(litholattice::cSimThread& sim, cSignalSim& signal, const litholattice::ChunkMap& world,
                                     int cx, int cz, std::vector<uint8_t> blob) {
  return sim.postToTick([&signal, &world, cx, cz, blob = std::move(blob)] {
    signal.scanColumn(world, cx, cz);
    if (!blob.empty() && !signal.importState(blob))
      litholattice::logError("save: streamed column %d,%d signal state unreadable (components restart idle)", cx, cz);
  });
}

/// Drops column (cx, cz)'s signal indexes (ghost-component guard) on the tick thread.
inline uint64_t postColumnSignalDrop(litholattice::cSimThread& sim, cSignalSim& signal, int cx, int cz) {
  return sim.postToTick([&signal, cx, cz] { signal.unscanColumn(cx, cz); });
}

/// Game thread only: columns whose posted signal init has not run on the tick thread yet.
class cSignalInitTickets {
 public:
  void note(uint64_t key, uint64_t ticket) { t_[key] = ticket; }
  /// True while the column's init is still queued (its signal state must not be read yet).
  bool pending(const litholattice::cSimThread& sim, uint64_t key) {
    const auto it = t_.find(key);
    if (it == t_.end()) return false;
    if (sim.tickPostDone(it->second)) { t_.erase(it); return false; }
    return true;
  }
  void prune(const litholattice::cSimThread& sim) {
    for (auto it = t_.begin(); it != t_.end();) it = sim.tickPostDone(it->second) ? t_.erase(it) : std::next(it);
  }
  void clear() { t_.clear(); }
  size_t size() const { return t_.size(); }

 private:
  std::unordered_map<uint64_t, uint64_t> t_;
};

}  // namespace lithokraft