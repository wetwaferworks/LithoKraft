#pragma once
// Showcase --frame-dump / --frame-hash: receiver of whole presented frames.
#include <cstdint>
#include <vector>

namespace lithokraft {

class IFrameSink {
 public:
  virtual ~IFrameSink() = default;
  /// Optional: sim tick for the upcoming onFrame (tick-paced hash recorder).
  virtual void onGrabTick(uint64_t /*tick*/) {}
  /// One presented frame, top row first, tightly packed RGB24.
  virtual void onFrame(const std::vector<uint8_t>& rgb, int width, int height) = 0;
  /// Flush and close (called once at shutdown).
  virtual void close() = 0;
};

}  // namespace lithokraft