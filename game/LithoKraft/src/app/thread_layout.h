// 2026-10-31 (JD): LL_THREAD_LAYOUT selects the thread layout before the thread census boots.
//   8 (default) = 3 permanent threads (main game loop, tick loop, control+event loop: LL_HS_LAYOUT=original),
//                 3 for vertex meshing + rendering (render thread LL_THREEWAY=1 + 2 dedicated lock-free
//                 mesh/vertex workers LL_DED_LAYOUT=8), 2 shared lock-free pool workers held in reserve.
//   0 / legacy  = the previous layout (shared pool cores-2 + handshake shards).
// Each component env var the user already set wins. Never exceeds hardware threads (census clamps).
#pragma once
#include <cstdlib>
#include <cstring>
#include <string>

class cThreadLayout {
 public:
  static void apply() {
    const char* v = std::getenv("LL_THREAD_LAYOUT");
    legacy_ = v && (v[0] == '0' || std::strcmp(v, "legacy") == 0);
    if (legacy_) return;
    setDefault("LL_HS_LAYOUT", "original");
    setDefault("LL_THREEWAY", "1");
    setDefault("LL_POOL_LOCKFREE", "1");
    setDefault("LL_DED_LAYOUT", "8");
  }
  static bool legacy() { return legacy_; }
  static std::string describe() {
    if (legacy_) return "threads: layout legacy (LL_THREAD_LAYOUT=0): shared pool cores-2 + handshake shards";
    auto g = [](const char* k) { const char* e = std::getenv(k); return std::string(e ? e : "-"); };
    return "threads: layout 8 (default; LL_THREAD_LAYOUT=0 for legacy): permanent 3 = main game loop + tick loop + "
           "control/event loop; mesh+render 3 = render thread + 2 dedicated lock-free mesh/vertex workers; "
           "reserve 2 = shared lock-free pool workers (idle until lent) [LL_HS_LAYOUT=" + g("LL_HS_LAYOUT") +
           " LL_THREEWAY=" + g("LL_THREEWAY") + " LL_POOL_LOCKFREE=" + g("LL_POOL_LOCKFREE") +
           " LL_DED_LAYOUT=" + g("LL_DED_LAYOUT") + "]";
  }
 private:
  static void setDefault(const char* k, const char* v) {
    if (std::getenv(k)) return;
#ifdef _WIN32
    _putenv_s(k, v);
#else
    setenv(k, v, 0);
#endif
  }
  static inline bool legacy_ = false;
};
