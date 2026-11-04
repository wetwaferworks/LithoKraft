#pragma once
// 2026-11-01 (JD, LL_STALL_TIMING=1, default off): main-thread stall timing for save / load, so the
// old and the lock-free (LL_LF_SAVE / LL_LF_LOAD) paths can be compared after a build. Logs one
// line per scope: "stall <what>: <ms> ms (<tag>)". Off = one cached getenv, nothing timed.
#include <chrono>
#include <cstdlib>
#include "litholattice/core/log.h"

namespace lithokraft {

class cStallTimer {
 public:
  static bool enabled() { static const bool on = [] { const char* e = std::getenv("LL_STALL_TIMING"); return !(e && e[0] == '0');  /* PlayL: default ON (JD 2026-11-01) */ }(); return on; }
  cStallTimer(const char* what, const char* tag) : what_(what), tag_(tag) { if (enabled()) t0_ = std::chrono::steady_clock::now(); }
  ~cStallTimer() {
    if (!enabled()) return;
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0_).count();
    litholattice::logInfo("stall %s: %.3f ms (%s)", what_, ms, tag_ ? tag_ : "");
  }
  cStallTimer(const cStallTimer&) = delete;
  cStallTimer& operator=(const cStallTimer&) = delete;

 private:
  const char* what_;
  const char* tag_;
  std::chrono::steady_clock::time_point t0_{};
};

}  // namespace lithokraft
