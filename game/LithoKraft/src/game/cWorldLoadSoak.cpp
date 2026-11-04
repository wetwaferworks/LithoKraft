// World load soak: see cWorldLoadSoak.h. 2026-10-31 (JD: verify Load World).
#include "lithokraft/game/cWorldLoadSoak.h"
#include <cstdio>
#include <cstdlib>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#endif

namespace lithokraft {

std::atomic<int> cWorldLoadSoak::next_{0};

cWorldLoadSoak::cWorldLoadSoak() {
  const char* e = std::getenv("LL_WORLD_LOAD_SOAK");
  if (!e || !*e) return;
  std::string cur;
  for (const char* p = e;; ++p) {
    if (*p == ',' || *p == '\0') {
      if (!cur.empty()) steps_.push_back(cur);
      cur.clear();
      if (*p == '\0') break;
    } else cur += *p;
  }
  if (const char* s = std::getenv("LL_WORLD_LOAD_SOAK_SECONDS")) period_ = std::atof(s) > 1 ? std::atof(s) : period_;
}

std::string cWorldLoadSoak::tick() {
  if (steps_.empty()) return {};
  const auto now = std::chrono::steady_clock::now();
  if (last_ == std::chrono::steady_clock::time_point{}) last_ = now;
  if (std::chrono::duration<double>(now - last_).count() < period_) return {};
  last_ = now;
  const int i = next_.fetch_add(1, std::memory_order_relaxed);
  if (i >= int(steps_.size())) return i == int(steps_.size()) ? std::string("quit") : std::string();
  return steps_[size_t(i)];
}

std::string cWorldLoadSoak::processStats() {
  char buf[96] = "threads=? ram=?";
#if defined(_WIN32)
  const DWORD pid = GetCurrentProcessId();
  int threads = 0;
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (snap != INVALID_HANDLE_VALUE) {
    THREADENTRY32 te{};
    te.dwSize = sizeof(te);
    if (Thread32First(snap, &te)) do { if (te.th32OwnerProcessID == pid) ++threads; } while (Thread32Next(snap, &te));
    CloseHandle(snap);
  }
  PROCESS_MEMORY_COUNTERS pmc{};
  pmc.cb = sizeof(pmc);
  GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
  std::snprintf(buf, sizeof(buf), "threads=%d ram=%.0f MB", threads, double(pmc.WorkingSetSize) / 1048576.0);
#endif
  return buf;
}

}  // namespace lithokraft
