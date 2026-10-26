#pragma once
// Cortana 2026-10-26 (Archie camera-jump decision, JD hitch report): the
// player body steps on the SIM thread, once per completed world tick, and
// publishes (prev, curr, tick timestamp) through a SeqLock. The frame
// thread never waits on the sim lock for movement: it reads the latest
// pair and lerps by alpha = time since the tick / tick duration, and
// extrapolates by velocity for at most one tick when the sim runs late.
// Input goes the other way through a second SeqLock; the rare commands
// (place / mode) through a small queue (its own mutex, never the sim's).
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>

#include "litholattice/entity/locomotion.h"

namespace lithokraft::plink {

/// Single-writer SeqLock over a trivially copyable T, stored as atomic
/// words so concurrent reads are race-free in the C++ model.
template <class T>
class SeqLock {
  static constexpr size_t kWords = (sizeof(T) + 7) / 8;
  std::atomic<uint32_t> seq_{0};
  std::array<std::atomic<uint64_t>, kWords> w_{};

 public:
  void store(const T& v) {
    uint64_t buf[kWords] = {};
    std::memcpy(buf, &v, sizeof(T));
    const uint32_t s = seq_.load(std::memory_order_relaxed);
    seq_.store(s + 1, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_release);
    for (size_t i = 0; i < kWords; ++i) w_[i].store(buf[i], std::memory_order_relaxed);
    seq_.store(s + 2, std::memory_order_release);
  }
  T load() const {
    uint64_t buf[kWords];
    for (;;) {
      const uint32_t s1 = seq_.load(std::memory_order_acquire);
      if (s1 & 1u) continue;
      for (size_t i = 0; i < kWords; ++i) buf[i] = w_[i].load(std::memory_order_relaxed);
      std::atomic_thread_fence(std::memory_order_acquire);
      if (seq_.load(std::memory_order_relaxed) == s1) break;
    }
    T v;
    std::memcpy(&v, buf, sizeof(T));
    return v;
  }
};

struct Snap {
  double prev[3] = {0, 0, 0}, curr[3] = {0, 0, 0}, vel[3] = {0, 0, 0};
  int64_t tNs = 0;       ///< steady_clock ns when curr was published
  double dt = 1.0 / 32;  ///< tick duration (s)
  uint64_t tick = 0;     ///< world tick of curr
  uint64_t placeSeq = 0; ///< last place command applied (frame thread matches its requests)
  float eye = 1.62f;
  uint8_t pose = 0, flying = 0, onGround = 0, placed = 0;
};

struct InputMsg {
  litholattice::LocomotionInput in{};
  uint8_t live = 0;
};

struct Cmd {
  enum Kind : uint8_t { Place, Mode } kind = Place;
  double pos[3] = {0, 0, 0};
  litholattice::LocomotionMode mode = litholattice::LocomotionMode::Walk;
  uint64_t seq = 0;
};

struct Link {
  SeqLock<Snap> snap;
  SeqLock<InputMsg> input;
  std::mutex cmdMu;
  std::vector<Cmd> cmds;
  std::atomic<bool> hasCmd{false};
  uint64_t placeSeq = 0;  // frame thread: last place request issued

  static int64_t nowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
  }
  void push(const Cmd& c) {
    std::lock_guard<std::mutex> g(cmdMu);
    cmds.push_back(c);
    hasCmd.store(true, std::memory_order_release);
  }
  std::vector<Cmd> drain() {
    std::vector<Cmd> out;
    if (!hasCmd.load(std::memory_order_acquire)) return out;
    std::lock_guard<std::mutex> g(cmdMu);
    out.swap(cmds);
    hasCmd.store(false, std::memory_order_relaxed);
    return out;
  }
};

/// Display pose at time `now`: lerp prev->curr by alpha in [0,1]; past 1
/// extrapolate by velocity for at most one more tick. A stale snapshot
/// (sim paused, > 2 ticks old) holds curr.
inline void displayPos(const Snap& s, int64_t now, double out[3]) {
  const double age = double(now - s.tNs) * 1e-9;
  const double a = s.dt > 0 ? age / s.dt : 1.0;
  for (int i = 0; i < 3; ++i) {
    if (a <= 1.0) out[i] = s.prev[i] + (s.curr[i] - s.prev[i]) * (a < 0 ? 0 : a);
    else if (a <= 2.0) out[i] = s.curr[i] + s.vel[i] * (a - 1.0) * s.dt;
    else out[i] = s.curr[i];
  }
}

}  // namespace lithokraft::plink
