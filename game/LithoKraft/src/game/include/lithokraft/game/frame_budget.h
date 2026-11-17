// R4 (B5/B6, 2026-11-17): streaming budgets priced against the frame target, not a fixed 60 Hz.
//
// The old grant table read the wall-clock frame time, which includes the pacer's sleep. At the
// 30 FPS cap every frame measures about 33 ms, so the table always said "cruising" and granted
// 2 new columns per frame, and the border slack (priced against 16.7 ms) never had any spare
// time to give. Here the budget looks at the busy part of the frame (frame time minus the time
// the pacer slept) against the target interval (1000 / cap, or 1000 / 30 when uncapped, which is
// JD's play target).
//
// Pure arithmetic, no clock reads: the caller passes the measured numbers in. The deterministic
// mode (--det-budget) does not use any of this; it keeps its fixed counts.
#pragma once
#include <algorithm>

namespace lithokraft {

struct cFrameBudget final {
  static constexpr double kDefaultTargetMs = 1000.0 / 30.0;  ///< JD's play target (30 FPS)
  static constexpr int kMinGrant = 1;                         ///< at least one column always moves
  static constexpr int kMaxGrant = 10;

  /// Target interval for a pacer cap (0 or less = uncapped, priced at the 30 FPS target).
  static double targetMsFor(int capFps) { return capFps > 0 ? 1000.0 / double(capFps) : kDefaultTargetMs; }

  /// The busy part of a frame: wall time minus what the pacer slept. Never negative.
  static double busyMs(double frameMs, double sleptMs) { return std::max(0.0, frameMs - std::max(0.0, sleptMs)); }

  /// New-column request grant for one frame. Over the target by 20 %: 1 (starving).
  /// Over the target: 2. Under it, 2 plus one column per 10 % of the frame left free, up to 10.
  static int requestGrant(double busyEmaMs, double targetMs) {
    if (!(targetMs > 0)) targetMs = kDefaultTargetMs;
    if (!(busyEmaMs >= 0)) busyEmaMs = targetMs;  // NaN guard: price as a full frame
    if (busyEmaMs > targetMs * 1.2) return kMinGrant;
    if (busyEmaMs >= targetMs) return 2;
    const double freeFrac = (targetMs - busyEmaMs) / targetMs;  // (0, 1]
    return std::clamp(2 + int(freeFrac * 10.0), 2, kMaxGrant);
  }

  /// Main-thread mesh commit time per frame: 9 % of the frame (the old 1.5 ms of a 16.7 ms frame).
  static double uploadMsBudget(double targetMs) { return std::max(1.5, 0.09 * targetMs); }
};

}  // namespace lithokraft
