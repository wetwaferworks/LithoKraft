// 2026-10-31 (JD close-hang): exit watchdog. After the final exit save, if the process is still
// alive kTimeoutMs later (a worker that never sees its stop flag, a static destructor join), it logs
// the census threads not yet RETURNED plus the OS thread count to logs/lithokraft.exitwatch.log and
// hard-exits. Lock-free: atomics plus a sleeping (not spinning) detached thread. LL_EXIT_WATCHDOG=0 kill.
#pragma once
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <thread>
#include "litholattice/core/thread_census.h"

namespace lithokraft {

class cExitWatchdog {
 public:
  static constexpr int kTimeoutMs = 5000;   ///< after the exit save
  static constexpr int kNoSaveCapMs = 60000;  ///< armed but the save never reported: give up then

  static bool enabled() {
    static const bool on = [] { const char* e = std::getenv("LL_EXIT_WATCHDOG"); return !(e && e[0] == '0'); }();
    return on;
  }
  /// Main thread, final shutdown only (not an in-process world switch). Idempotent.
  static void arm(const std::string& logDir) {
    if (!enabled() || armed().exchange(true)) return;
    std::string path = logDir.empty() ? std::string("lithokraft.exitwatch.log") : logDir + "/lithokraft.exitwatch.log";
    std::thread([path] { run(path); }).detach();
  }
  /// Game teardown, right after the exit save is on disk.
  static void noteSaved() { savedAt().store(nowMs(), std::memory_order_release); }

 private:
  static std::atomic<bool>& armed() { static std::atomic<bool> a{false}; return a; }
  static std::atomic<int64_t>& savedAt() { static std::atomic<int64_t> t{0}; return t; }
  static int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
  }
  static void run(const std::string& path) {
    const int64_t start = nowMs();
    for (;;) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));  // sleeps, never spins
      const int64_t s = savedAt().load(std::memory_order_acquire), n = nowMs();
      if ((s && n - s >= kTimeoutMs) || (!s && n - start >= kNoSaveCapMs)) break;
    }
    FILE* f = std::fopen(path.c_str(), "a");
    if (!f) f = std::fopen("lithokraft.exitwatch.log", "a");
    if (f) {
      const std::time_t t = std::time(nullptr);
      char ts[32]; std::strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", std::localtime(&t));
      std::fprintf(f, "[%s] exit watchdog: process still alive %d ms after the exit save; os threads=%zu; forcing exit\n", ts,
                   kTimeoutMs, litholattice::osThreadCount());
      auto& c = litholattice::cThreadCensus::instance();
      for (uint32_t i = 0; i < uint32_t(c.size()); ++i) {
        const auto r = c.record(i);
        if (r.state == litholattice::ThreadState::Returned) continue;
        const auto tel = c.telemetry(i);
        std::fprintf(f, "  alive: #%u '%s' state=%d os=%llu\n", i, tel.name.c_str(), int(r.state), (unsigned long long)tel.osThreadId);
      }
      std::fclose(f);
    }
    std::fflush(nullptr);
    std::_Exit(3);
  }
};

}  // namespace lithokraft
