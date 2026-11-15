#pragma once
// Nemo View hand-off: the last ASCII frame Little Nemo was given (vision.capture_ascii, MCP thread)
// travels to the Nemo View window (its own thread) through a three-slot ownership hand-off, the same
// pattern as cMcpDisplayVision. The producer fills its private back slot and swaps it into the
// shared middle slot with one atomic exchange; the consumer swaps the middle slot into its private
// front slot only when the FRESH bit is set. No slot is touched by two threads at once and neither
// side waits. One producer thread and one consumer thread.
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

#include "litholattice/render/nemo_view.h"

namespace lithokraft {

class cNemoViewHub {
 public:
  static cNemoViewHub& instance() {
    static cNemoViewHub h;
    return h;
  }
  struct Entry {
    litholattice::nemoview::AsciiFrame frame;
    std::string source;      ///< "Nemo's look" (MCP capture)
    int64_t steadyMs = 0;    ///< when it was published (steady clock, ms)
    uint64_t seq = 0;        ///< 0 = nothing published yet
  };
  // ---- producer (one thread) ----
  void publish(const litholattice::nemoview::AsciiFrame& f, const char* source) {
    Entry& e = slots_[back_];
    e.frame = f;
    e.source = source ? source : "";
    e.steadyMs = nowMs();
    e.seq = ++published_;
    const unsigned prev = mid_.exchange(back_ | kFresh, std::memory_order_acq_rel);  // publish edge
    back_ = prev & kIdx;
  }
  // ---- consumer (one thread) ----
  /// The newest published entry (seq 0 when there has never been one).
  const Entry& latest() {
    if (mid_.load(std::memory_order_acquire) & kFresh) {
      const unsigned prev = mid_.exchange(front_, std::memory_order_acq_rel);  // acquire edge
      front_ = prev & kIdx;
    }
    return slots_[front_];
  }
  static int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
  }

 private:
  static constexpr unsigned kIdx = 3u, kFresh = 4u;
  Entry slots_[3];
  unsigned back_ = 0;              ///< producer-owned slot index
  std::atomic<unsigned> mid_{1u};  ///< shared slot index | kFresh
  unsigned front_ = 2;             ///< consumer-owned slot index
  uint64_t published_ = 0;         ///< producer only
};

}  // namespace lithokraft
