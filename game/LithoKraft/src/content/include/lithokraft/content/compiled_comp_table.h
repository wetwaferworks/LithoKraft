// 2026-11-01 (JD Option A, approved 10/31): cCompiledCompTable. LL_OPTION_A=1 turns it on (default off).
// Per component two uint32_t state words: simBits (sim thread only, written during the tick) and
// publishedBits (std::atomic, release-stored at tick commit; render / UI read it with acquire).
// The low 16 bits mirror CellState. The table is the topology-cached, coordinate-sorted dispatch
// order (the same keys as cSignalSim::compOrder_), rebuilt only on relink.
// Lock-free: readers pin with an atomic reader count, the writer swaps an atomic snapshot pointer
// and frees retired snapshots only once it observes zero readers. No mutex, no condvar.
#pragma once
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace lithokraft {

class cCompiledCompTable {
 public:
  cCompiledCompTable() = default;
  cCompiledCompTable(const cCompiledCompTable&) = delete;
  cCompiledCompTable& operator=(const cCompiledCompTable&) = delete;
  ~cCompiledCompTable() { delete snap_.load(std::memory_order_acquire); for (Snapshot* s : retired_) delete s; }

  /// Sim thread: new sorted key order (relink). Existing published values carry over by key.
  void rebuild(const std::vector<int64_t>& sortedKeys) {
    auto* s = new Snapshot;
    s->keys = sortedKeys;
    s->sim.assign(sortedKeys.size(), 0);
    s->pub = std::make_unique<std::atomic<uint32_t>[]>(sortedKeys.size());
    const Snapshot* old = snap_.load(std::memory_order_relaxed);
    for (size_t i = 0; i < sortedKeys.size(); ++i) {
      uint32_t v = 0;
      if (old) if (const auto j = old->find(sortedKeys[i]); j >= 0) v = old->sim[size_t(j)];
      s->sim[i] = v;
      s->pub[i].store(v, std::memory_order_relaxed);
    }
    dirty_.clear();
    Snapshot* prev = snap_.exchange(s, std::memory_order_seq_cst);
    if (prev) retired_.push_back(prev);
    reclaim();
  }
  /// Sim thread, during the tick: write the component's simBits word (no reader sees it yet).
  void stage(int64_t key, uint32_t bits) {
    Snapshot* s = snap_.load(std::memory_order_relaxed);
    if (!s) return;
    const auto i = s->find(key);
    if (i < 0 || s->sim[size_t(i)] == bits) return;
    s->sim[size_t(i)] = bits;
    dirty_.push_back(uint32_t(i));
  }
  /// Sim thread, tick commit: release-store every changed word into publishedBits.
  void commit() {
    Snapshot* s = snap_.load(std::memory_order_relaxed);
    if (s) for (uint32_t i : dirty_) s->pub[i].store(s->sim[i], std::memory_order_release);
    dirty_.clear();
    reclaim();
  }
  /// Any thread: last committed state bits for a component key (false if not in the table).
  bool published(int64_t key, uint32_t& out) const {
    readers_.fetch_add(1, std::memory_order_seq_cst);
    const Snapshot* s = snap_.load(std::memory_order_seq_cst);
    bool ok = false;
    if (s) if (const auto i = s->find(key); i >= 0) { out = s->pub[size_t(i)].load(std::memory_order_acquire); ok = true; }
    readers_.fetch_sub(1, std::memory_order_release);
    return ok;
  }
  size_t size() const { const Snapshot* s = snap_.load(std::memory_order_acquire); return s ? s->keys.size() : 0; }

 private:
  struct Snapshot {
    std::vector<int64_t> keys;                    // sorted (packPos order)
    std::vector<uint32_t> sim;                    // simBits: sim thread only
    std::unique_ptr<std::atomic<uint32_t>[]> pub; // publishedBits
    std::ptrdiff_t find(int64_t k) const {
      const auto it = std::lower_bound(keys.begin(), keys.end(), k);
      return it != keys.end() && *it == k ? it - keys.begin() : -1;
    }
  };
  void reclaim() {  // sim thread only; never waits
    if (retired_.empty() || readers_.load(std::memory_order_seq_cst) != 0) return;
    for (Snapshot* s : retired_) delete s;
    retired_.clear();
  }
  std::atomic<Snapshot*> snap_{nullptr};
  mutable std::atomic<uint32_t> readers_{0};
  std::vector<Snapshot*> retired_;   // sim thread only
  std::vector<uint32_t> dirty_;      // sim thread only
};

}  // namespace lithokraft
