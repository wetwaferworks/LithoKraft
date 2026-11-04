// World load soak (2026-10-31, JD: verify Load World). Test hook, default OFF.
// LL_WORLD_LOAD_SOAK="default,testworld,flatlab,D:\some\world,save,..." walks the list one step
// every LL_WORLD_LOAD_SOAK_SECONDS (default 12) of play: a world id or folder is loaded
// in-process, "save" saves, "quit" exits. Each step logs the process thread count and RAM so
// leaks across load cycles show in the log. The step index survives the Game rebuild (static).
#pragma once
#include <atomic>
#include <chrono>
#include <string>
#include <vector>

namespace lithokraft {

class cWorldLoadSoak final {
 public:
  cWorldLoadSoak();
  bool active() const { return !steps_.empty(); }
  /// Call each frame while playing; returns the step to run now ("" = none).
  std::string tick();
  /// "threads=N ram=M MB" for the current process.
  static std::string processStats();

 private:
  std::vector<std::string> steps_;
  double period_ = 12.0;
  std::chrono::steady_clock::time_point last_{};
  static std::atomic<int> next_;
};

}  // namespace lithokraft
