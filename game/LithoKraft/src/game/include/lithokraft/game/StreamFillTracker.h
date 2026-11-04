#pragma once
// Streaming bench (JD 2026-10-30, implementation Cortana): measures how long the initial
// streaming fill takes, from run start until every column in the stream footprint is
// fully streamed in, plus each column's own stream time. A column's clock starts on the
// first frame it is seen in the footprint and stops on the first frame it is resident,
// at stage >= StagedRender (first mesh wave back, or all air), not L0-held and with no
// first-wave meshes pending (the same test as the render bench's settle gate). Sampling
// is per frame, so times have one-frame resolution. Runs alongside --bench (--stream-bench).
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace lithokraft {

class cStreamFillTracker {
 public:
  using Clock = std::chrono::steady_clock;

  struct Summary {
    size_t columns = 0;    ///< footprint columns that streamed in
    double totalS = 0;     ///< run start -> every footprint column ready
    double avgMs = 0, p50Ms = 0, p99Ms = 0, maxMs = 0;  ///< per-column stream time
    double colsPerS = 0;   ///< columns / totalS
  };

  void start(Clock::time_point t0);
  bool started() const { return started_; }
  bool finished() const { return finished_; }
  /// A footprint column was seen this frame (starts its clock the first time).
  void noteWanted(uint64_t key, Clock::time_point now);
  /// A footprint column is fully streamed in (stops its clock the first time).
  void noteReady(uint64_t key, Clock::time_point now);
  /// Every footprint column is ready: freeze the total time.
  void finish(Clock::time_point now);
  Summary summary() const;
  /// Logs and prints the STREAMBENCH line; writes JSON to jsonPath when non-empty.
  void report(const char* renderer, size_t columnsInRadius, const char* jsonPath) const;

 private:
  std::unordered_map<uint64_t, Clock::time_point> wanted_;  ///< key -> first seen, until ready
  std::unordered_set<uint64_t> ready_;
  std::vector<double> ms_;
  Clock::time_point t0_{}, tEnd_{};
  bool started_ = false, finished_ = false;
};

}  // namespace lithokraft
