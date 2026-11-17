// Archie Phase 1 (JD 2026-11-04): the view-distance mesh gate. Nothing outside the view radius is
// queued or meshed; columns that leave the view are retired and re-mesh when they return.
// R4 (B8, 2026-11-17): one ring of hysteresis. A meshed column that drops just outside the view
// keeps its meshes drawn ("kept"); they are retired only past view + kKeepBand. If it comes back
// it re-meshes, and the old meshes stay up until the replacements upload (no tombstone first),
// so turning or strafing across the edge no longer punches holes.
#pragma once
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <unordered_set>

namespace lithokraft {
/// Which columns may mesh, given the stream centre and view radius. Lock-free: the centre and radius
/// are relaxed atomics (inView may be called from the sim thread's remesh path); the column sets
/// are game-thread only.
/// Chebyshev distance by default, diamond (|dx|+|dz|) when the stream area is a diamond.
/// Until a centre is set everything is in view. LL_VIEW_MESH_GATE=0 turns the gate off.
class cViewMeshGate final {
 public:
  static uint64_t key(int cx, int cz) { return (uint64_t(uint32_t(cx)) << 32) | uint32_t(cz); }
  static int keyX(uint64_t k) { return int(int32_t(uint32_t(k >> 32))); }
  static int keyZ(uint64_t k) { return int(int32_t(uint32_t(k))); }
  bool enabled() const { return on_; }
  bool centred() const { return set_.load(std::memory_order_relaxed); }
  int radius() const { return r_.load(std::memory_order_relaxed); }
  static constexpr int kKeepBand = 1;  ///< R4 (B8): rings past the view where meshes stay drawn
  /// Is column (cx, cz) inside the view?
  bool inView(int cx, int cz) const { return within(cx, cz, 0); }
  /// R4 (B8): inside the view or the keep band just outside it.
  bool inKeep(int cx, int cz) const { return within(cx, cz, kKeepBand); }
  /// Moves the centre / radius. Returns true when anything changed (the caller then calls sweep()).
  bool setCentre(int cx, int cz, int r, bool diamond) {
    if (!on_) return false;
    const uint64_t c = key(cx, cz);
    const bool changed = !set_.load(std::memory_order_relaxed) || c != centre_.load(std::memory_order_relaxed) ||
                         r != r_.load(std::memory_order_relaxed) || diamond != diamond_.load(std::memory_order_relaxed);
    centre_.store(c, std::memory_order_relaxed); r_.store(r, std::memory_order_relaxed);
    diamond_.store(diamond, std::memory_order_relaxed); set_.store(true, std::memory_order_release);
    return changed;
  }
  /// Column (cx, cz) is not meshed because it is beyond the view (retired, or never meshed).
  void defer(int cx, int cz) { if (on_) deferred_.insert(key(cx, cz)); }
  bool deferred(int cx, int cz) const { return deferred_.count(key(cx, cz)) != 0; }
  /// Back in the view: meshes again. Kept meshes stay drawn until the new ones replace them.
  void undefer(int cx, int cz) { deferred_.erase(key(cx, cz)); kept_.erase(key(cx, cz)); }
  void forget(int cx, int cz) { deferred_.erase(key(cx, cz)); kept_.erase(key(cx, cz)); }
  void clear() { deferred_.clear(); kept_.clear(); set_.store(false, std::memory_order_release); }
  size_t deferredCount() const { return deferred_.size(); }
  /// R4 (B8): (cx, cz) left the view into the keep band. It is deferred (no new meshes) but its
  /// meshes stay drawn until it leaves the band.
  void keep(int cx, int cz) { if (on_) { deferred_.insert(key(cx, cz)); kept_.insert(key(cx, cz)); } }
  bool kept(int cx, int cz) const { return kept_.count(key(cx, cz)) != 0; }
  /// Its meshes were retired: still deferred, nothing drawn.
  void retired(int cx, int cz) { if (on_) { deferred_.insert(key(cx, cz)); kept_.erase(key(cx, cz)); } }
  size_t keptCount() const { return kept_.size(); }

  /// One sweep decision for a resident column (game thread). Stay: nothing to do. Keep: call
  /// keep(). Retire: retire its meshes, then call retired(). Enter: undefer() and mesh it.
  enum class Step { Stay, Keep, Retire, Enter };
  Step step(int cx, int cz) const {
    const bool held = deferred(cx, cz);
    if (inView(cx, cz)) return held ? Step::Enter : Step::Stay;
    if (!held) return inKeep(cx, cz) ? Step::Keep : Step::Retire;
    return kept(cx, cz) && !inKeep(cx, cz) ? Step::Retire : Step::Stay;
  }

 private:
  bool within(int cx, int cz, int band) const {
    if (!on_ || !set_.load(std::memory_order_acquire)) return true;
    const uint64_t c = centre_.load(std::memory_order_relaxed);  // cx and cz in one word: never torn
    const int dx = std::abs(cx - keyX(c)), dz = std::abs(cz - keyZ(c));
    return (diamond_.load(std::memory_order_relaxed) ? dx + dz : (dx > dz ? dx : dz)) <= r_.load(std::memory_order_relaxed) + band;
  }
  bool on_ = [] { const char* e = std::getenv("LL_VIEW_MESH_GATE"); return !(e && e[0] == '0'); }();
  std::atomic<bool> set_{false}, diamond_{false};
  std::atomic<uint64_t> centre_{0};
  std::atomic<int> r_{1 << 20};
  std::unordered_set<uint64_t> deferred_;  // game thread only
  std::unordered_set<uint64_t> kept_;      // game thread only; always a subset of deferred_
};
}  // namespace lithokraft
