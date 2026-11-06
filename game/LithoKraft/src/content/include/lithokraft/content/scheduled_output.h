// 2026-11-04 (JD t4304u / t4306u): the thin shared release queue. Each component CLASS owns its delay
// (cCompBehavior::delayTicks); this only holds the computed output and releases it on that tick.
// Per-cell value type (lives in the sim's Comp, touched only by the sim's tick thread): no locks,
// no allocation (fixed ring). Deterministic: only the tick number and the cell key go in.
#pragma once
#include <array>
#include <cstdint>
namespace lithokraft {
class cScheduledOutput {
 public:
  struct Out {
    uint8_t out = 0, out2 = 0;
    bool pulse = false;
    bool operator==(const Out& o) const { return out == o.out && out2 == o.out2 && pulse == o.pulse; }
  };
  static constexpr int kMaxDelay = 64;
  /// Feed this tick's computed output (`raw`); returns what the cell publishes now. A second call in
  /// the same tick (the same-tick settle pass) replaces that tick's entry.
  Out step(uint64_t now, int delay, Out raw);
  bool pending() const { return count_ != 0; }
 private:
  struct Entry { uint64_t due = 0; Out o{}; };
  std::array<Entry, kMaxDelay + 1> ring_{};
  uint8_t head_ = 0, count_ = 0;
  Out last_{}, pub_{};
  uint64_t lastAt_ = ~uint64_t(0);
  Out beforeLast_{};
};
}  // namespace lithokraft
