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
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <map>
#include <memory>
#include <unordered_map>
#include <unordered_set>
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

/// t46u item 6b-4a' (JD 2026-11-16): non-blocking "which of these columns does the signal sim index"
/// query, with the SGNL export of the ones it does. The evict/demote pumps ask it instead of blocking in
/// runBetweenTicks (6b-4a's +2 ms update regression).
///
/// Game thread only. poll() is called every pump with the current candidate keys. Keys the tick side has
/// answered for (covers()) can be acted on; a key without an answer yet must be skipped that pump. Keys
/// not yet answered or asked are posted in one query (postToTick); nothing is re-asked while a key stays a
/// candidate, so a permanently refused candidate set (e.g. no save dir) costs no query per frame. The
/// answer is read only after tickPostDone's acquire, and the job writes it before the tick side's release.
///
/// Why an answer stays true while its key stays a candidate: candidates lie beyond the stream radius +
/// hysteresis, outside the tick area and player reach, so their signal state does not change. Their init
/// post (at load) precedes any query naming them (FIFO). A key that leaves the candidate set (evicted,
/// or back in range) loses its answer and any in-flight one, and is asked again if it returns.
/// invalidate() drops everything when the sim is replaced.
class cSignalBearingQuery {
 public:
  explicit cSignalBearingQuery(bool withExport) : withExport_(withExport) {}
  struct Answer {
    std::unordered_set<uint64_t> known;                 ///< keys answered for
    std::unordered_set<uint64_t> bearing;               ///< of those, the ones the signal sim indexes cells in
    std::map<uint64_t, std::vector<uint8_t>> exported;  ///< SGNL bytes of the bearing keys (withExport)
    bool covers(uint64_t key) const { return known.count(key) != 0; }
    void keepOnly(const std::unordered_set<uint64_t>& cur) {
      std::erase_if(known, [&](uint64_t k) { return !cur.count(k); });
      std::erase_if(bearing, [&](uint64_t k) { return !cur.count(k); });
      std::erase_if(exported, [&](const auto& kv) { return !cur.count(kv.first); });
    }
  };
  const Answer& poll(litholattice::cSimThread& sim, const cSignalSim& signal, const std::vector<uint64_t>& keys) {
    const std::unordered_set<uint64_t> cur(keys.begin(), keys.end());
    have_.keepOnly(cur);
    std::erase_if(inflight_, [&](uint64_t k) { return !cur.count(k); });
    if (pending_ && sim.tickPostDone(ticket_)) {  // merge the keys that stayed candidates throughout
      for (uint64_t k : inflight_) {
        have_.known.insert(k);
        if (pending_->bearing.count(k)) have_.bearing.insert(k);
        if (auto it = pending_->exported.find(k); it != pending_->exported.end()) have_.exported.emplace(k, std::move(it->second));
      }
      pending_.reset();
      inflight_.clear();
    }
    if (!pending_) {
      std::vector<uint64_t> ask;
      for (uint64_t k : keys)
        if (!have_.covers(k)) ask.push_back(k);
      if (!ask.empty()) {
        auto ans = std::make_shared<Answer>();
        const bool withExport = withExport_;
        inflight_.insert(ask.begin(), ask.end());
        ticket_ = sim.postToTick([&signal, ans, ask = std::move(ask), withExport] {
          for (uint64_t key : ask)
            if (signal.hasIndexedCells(int(int32_t(key >> 32)), int(int32_t(key & 0xffffffffu)))) ans->bearing.insert(key);
          if (withExport && !ans->bearing.empty())
            for (auto& [k, bytes] : signal.exportState())
              if (ans->bearing.count(k)) ans->exported.emplace(k, std::move(bytes));
        });
        pending_ = std::move(ans);
        ++asks_;
      }
    }
    return have_;
  }
  void invalidate() { have_ = Answer{}; pending_.reset(); inflight_.clear(); }
  uint64_t asks() const { return asks_; }  ///< queries posted so far (tests, diagnostics)
  bool inFlight() const { return pending_ != nullptr; }

 private:
  Answer have_;
  std::shared_ptr<Answer> pending_;  // the posted query's answer, written by the tick job
  std::unordered_set<uint64_t> inflight_;
  uint64_t ticket_ = 0;
  uint64_t asks_ = 0;
  bool withExport_;
};

}  // namespace lithokraft