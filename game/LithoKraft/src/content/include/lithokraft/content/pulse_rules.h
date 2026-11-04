// 2026-11-01 (JD, PlayK): Pulsor hold length and Repeator <-> Catchor / Pulsor direct links.
#pragma once
#include <cstdlib>
#include "lithokraft/content/signal.h"

namespace lithokraft {

/// Playtest switches for the PlayK Pulsor / Catchor rulings. Read once from the environment.
/// LL_PULSOR_HOLD=N : a fired pulse stays high N ticks (default 4, 1 = old one-tick pulse, max kPulsorAnimTicks).
/// LL_REP_LINK=0    : turn off direct Repeator -> Catchor input and Pulsor -> Repeator output (default on).
class cPulseRules final {
 public:
  static int holdTicks() {
    static const int v = [] {
      const char* e = std::getenv("LL_PULSOR_HOLD");
      const int n = e ? std::atoi(e) : 4;
      return n < 1 ? 1 : (n > kPulsorAnimTicks ? kPulsorAnimTicks : n);
    }();
    return v;
  }
  static bool repeatorLinks() {
    static const bool on = [] { const char* e = std::getenv("LL_REP_LINK"); return !(e && e[0] == '0' && e[1] == '\0'); }();
    return on;
  }
  /// LL_HARD_ANY=0 : only a Repeator hard-powers the block it faces (old). Default on (JD 2026-11-01 PlayL):
  /// ANY component's steady output facing a solid powerable block hard-powers it (Memor front included).
  static bool hardAnyComponent() {
    static const bool on = [] { const char* e = std::getenv("LL_HARD_ANY"); return !(e && e[0] == '0' && e[1] == '\0'); }();
    return on;
  }
};

}  // namespace lithokraft
