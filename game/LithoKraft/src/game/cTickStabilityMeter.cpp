// Tick stability meter: see cTickStabilityMeter.h. 2026-10-31 (JD).
#include "lithokraft/game/cTickStabilityMeter.h"
#include <cstdio>

namespace lithokraft {

const cTickStabilityMeter::Sample* cTickStabilityMeter::findBack(double age) const {
  if (count_ < 2) return nullptr;
  const Sample& now = ring_[(head_ + kRing - 1) % kRing];
  const Sample* best = &ring_[(head_ + kRing - count_) % kRing];  // oldest
  for (size_t i = 2; i <= count_; ++i) {
    const Sample& s = ring_[(head_ + kRing - i) % kRing];
    if (now.t - s.t >= age) return &s;
    best = &s;
  }
  return best;
}

void cTickStabilityMeter::sample(uint64_t worldTick, uint64_t droppedTicks, int targetTps,
                                 std::chrono::steady_clock::time_point now) {
  if (t0_ < 0) { base_ = now; t0_ = 0; }
  const double t = std::chrono::duration<double>(now - base_).count();
  if (count_ > 0) {
    const Sample& last = ring_[(head_ + kRing - 1) % kRing];
    if (worldTick < last.tick || droppedTicks < last.dropped) count_ = 0;  // world reload: restart the window
    else if (t - last.t < 0.01) return;  // <= 100 samples/s keeps 1024 slots > 10 s
  }
  ring_[head_] = {t, worldTick, droppedTicks};
  head_ = (head_ + 1) % kRing;
  if (count_ < kRing) ++count_;
  const Sample& cur = ring_[(head_ + kRing - 1) % kRing];
  auto rate = [&](const Sample* s, double& tps, uint64_t& drop) {
    tps = 0; drop = 0;
    if (!s || cur.t - s->t <= 0) return;
    tps = double(cur.tick - s->tick) / (cur.t - s->t);
    drop = cur.dropped - s->dropped;
  };
  double a1, a5; uint64_t d1, d5;
  rate(findBack(1.0), a1, d1);
  rate(findBack(5.0), a5, d5);
  tps1_.store(a1, std::memory_order_relaxed);
  tps5_.store(a5, std::memory_order_relaxed);
  drop1_.store(d1, std::memory_order_relaxed);
  drop5_.store(d5, std::memory_order_relaxed);
  dropTotal_.store(droppedTicks, std::memory_order_relaxed);
  target_.store(targetTps, std::memory_order_relaxed);
  Level lv = Level::Green;
  if (targetTps > 0) {
    const double tgt = double(targetTps);
    if (d1 > uint64_t(tgt * 0.02) || a1 < tgt * 0.85) lv = Level::Red;           // dropping, or well below
    else if (d1 > 0 || a1 < tgt * 0.98 || a5 < tgt * 0.98) lv = Level::Yellow;   // dipping
  }
  level_.store(int(lv), std::memory_order_relaxed);
}

void cTickStabilityMeter::setFrozen(bool f) {
  const bool was = frozen_.exchange(f, std::memory_order_relaxed);
  if (f) {
    level_.store(int(Level::Neutral), std::memory_order_relaxed);
  } else if (was) {
    count_ = 0; t0_ = -1;  // 2026-11-05 (JD #6): resume without counting paused wall-time as a TPS dip
  }
}

std::string cTickStabilityMeter::line() const {
  if (frozen_.load(std::memory_order_relaxed)) return "TPS paused";
  char b[160];
  std::snprintf(b, sizeof b, "TPS %.0f (1s)  %.0f (5s) / %d  dropped %llu in 5s (%llu total)", tps1s(), tps5s(), target(),
                (unsigned long long)dropped5s(), (unsigned long long)droppedTotal());
  return b;
}

}  // namespace lithokraft
