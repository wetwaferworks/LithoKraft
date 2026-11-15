// 2026-10-31 (JD) / 2026-11-14 (JD, TheReefs): LL_THREAD_LAYOUT selects the layout before the
// thread census boots. The permanent three are always reserved first:
//   (1) main game loop, (2) timing handshake + control, (3) tick/sim.
// Handshake shards still run inline on the tick thread under LL_HS_LAYOUT=original (sim-path
// unchanged, hashes stay put); the control/event thread is the control half of that pair.
// After those three, remaining = max(hw-3, 1) and all render/vertex/mesh work (dedicated mesh
// pool, threeway render/executor, software raster bands, reserve job workers) shares a budget
// of remaining+2, from planRenderSide() in the census. On 4 hw threads that is 3 render-side
// threads; on a 12-thread workstation, 11.
//   8 (default) = those env defaults (LL_HS_LAYOUT=original, LL_THREEWAY=1, LL_DED_LAYOUT=8);
//                 actual worker counts come from the census plan, not fixed 3+2.
//   0 / legacy  = previous env defaults (shared pool + handshake shards); the same render-side
//                 budget still applies, with mesh folded into the shared pool.
// Each component env var the user already set wins.
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
    return "threads: layout 8 (default; LL_THREAD_LAYOUT=0 for legacy): permanent 3 = main game loop + "
           "timing-handshake/control + tick/sim (shards still inline on tick under original handshake); "
           "render-side budget = remaining+2 from the census (mesh + reserve pool + software bands + "
           "threeway executor share it) [LL_HS_LAYOUT=" + g("LL_HS_LAYOUT") +
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
