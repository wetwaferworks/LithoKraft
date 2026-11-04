// Tick stability meter (2026-10-31, JD): rolling 1 s / 5 s average TPS and dropped ticks for
// the HUD, coloured green (holding the target), yellow (dipping), red (dropping / well below).
// Fed once per frame from the sim's lock-free SimStats triple buffer; results are published
// in relaxed atomics so any thread (HUD, MCP, logs) can read them without a lock.
#pragma once
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

namespace lithokraft {

class cTickStabilityMeter {
 public:
  enum class Level : int { Green = 0, Yellow = 1, Red = 2 };
  void sample(uint64_t worldTick, uint64_t droppedTicks, int targetTps, std::chrono::steady_clock::time_point now);
  double tps1s() const { return tps1_.load(std::memory_order_relaxed); }
  double tps5s() const { return tps5_.load(std::memory_order_relaxed); }
  uint64_t dropped5s() const { return drop5_.load(std::memory_order_relaxed); }
  uint64_t droppedTotal() const { return dropTotal_.load(std::memory_order_relaxed); }
  int target() const { return target_.load(std::memory_order_relaxed); }
  Level level() const { return Level(level_.load(std::memory_order_relaxed)); }
  std::string line() const;

 private:
  struct Sample { double t = 0; uint64_t tick = 0, dropped = 0; };
  static constexpr size_t kRing = 1024;  // >5 s at any sane frame rate (min spacing enforced)
  std::array<Sample, kRing> ring_{};
  size_t head_ = 0, count_ = 0;
  double t0_ = -1;
  std::chrono::steady_clock::time_point base_{};
  std::atomic<double> tps1_{0}, tps5_{0};
  std::atomic<uint64_t> drop1_{0}, drop5_{0}, dropTotal_{0};
  std::atomic<int> target_{0}, level_{0};
  const Sample* findBack(double age) const;
};

}  // namespace lithokraft
