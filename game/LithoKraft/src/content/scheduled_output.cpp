// 2026-11-04 (JD t4304u): see lithokraft/content/scheduled_output.h.
#include <cstddef>
#include "lithokraft/content/scheduled_output.h"
namespace lithokraft {
cScheduledOutput::Out cScheduledOutput::step(uint64_t now, int delay, Out raw) {
  if (delay <= 0) { count_ = 0; last_ = pub_ = raw; lastAt_ = now; return raw; }
  if (delay > kMaxDelay) delay = kMaxDelay;
  if (lastAt_ == now && count_ != 0) {
    // Same tick again (settle pass): replace this tick's entry (the newest).
    Entry& e = ring_[size_t((head_ + count_ - 1) % ring_.size())];
    if (e.due >= now + 1) {
      if (raw == beforeLast_) --count_;  // settled back to the previous value: nothing to schedule
      else e.o = raw;
      last_ = raw;
    }
  } else if (!(raw == last_)) {
    if (count_ == ring_.size()) { pub_ = ring_[head_].o; head_ = uint8_t((head_ + 1) % ring_.size()); --count_; }
    ring_[size_t((head_ + count_) % ring_.size())] = Entry{now + uint64_t(delay), raw};
    ++count_;
    beforeLast_ = last_;
    last_ = raw;
    lastAt_ = now;
  }
  while (count_ != 0 && ring_[head_].due <= now) {
    pub_ = ring_[head_].o;
    head_ = uint8_t((head_ + 1) % ring_.size());
    --count_;
  }
  return pub_;
}
}  // namespace lithokraft
