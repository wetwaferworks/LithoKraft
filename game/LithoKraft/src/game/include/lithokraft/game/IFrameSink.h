#pragma once
// Showcase --frame-dump (JD 2026-10-30): receiver of whole presented frames.
#include <cstdint>
#include <vector>

namespace lithokraft {

class IFrameSink {
 public:
  virtual ~IFrameSink() = default;
  /// One presented frame, top row first, tightly packed RGB24.
  virtual void onFrame(const std::vector<uint8_t>& rgb, int width, int height) = 0;
  /// Flush and close (called once at shutdown).
  virtual void close() = 0;
};

}  // namespace lithokraft
