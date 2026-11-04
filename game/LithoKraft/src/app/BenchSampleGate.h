#pragma once
// Bench steady-state gate (JD 2026-10-30, implementation Cortana): a --bench run measures
// frames only once the initial streaming fill has settled (every column in the stream
// footprint resident, its first-wave meshes back, no mesh work queued), so p99 is a
// steady-state render number instead of a mix of fill and render. Workload paths whose
// streaming IS the measurement (stream / soak / showcase) are exempt and open at the
// warm-up as before. A cap (LL_BENCH_SETTLE_CAP seconds, default 120) opens the gate
// even if streaming never settles; the reason is reported.
#include <chrono>

namespace lithokraft {

class cBenchSampleGate {
 public:
  using Clock = std::chrono::steady_clock;
  enum class Reason { Pending, Settled, Cap, Exempt };

  /// warmupS: minimum seconds before sampling; capS: open regardless after this long past the warm-up.
  void configure(double warmupS, double capS, bool exempt);
  /// True when the caller should probe streaming this frame (warm-up passed, gate still shut, not exempt).
  bool wantsProbe(Clock::time_point tStart, Clock::time_point now) const;
  /// Per frame; opens the gate when due. Returns true on the frame it opens.
  bool update(Clock::time_point tStart, Clock::time_point now, bool settled);
  bool open() const { return reason_ != Reason::Pending; }
  Reason reason() const { return reason_; }
  const char* reasonName() const;
  /// Seconds from run start to the gate opening (0 while shut).
  double openedAfterS() const { return openedAfterS_; }
  double sinceOpenS(Clock::time_point now) const;
  /// Bench camera clock: run time up to the warm-up, held there while the gate is shut,
  /// then warm-up + time since opening (the path runs the same course relative to sampling).
  double cameraSeconds(Clock::time_point tStart, Clock::time_point now) const;
  double capS() const { return capS_; }
  /// LL_BENCH_SETTLE_CAP (seconds), default 120.
  static double capFromEnv();

 private:
  double warmupS_ = 3.0, capS_ = 120.0, openedAfterS_ = 0.0;
  bool exempt_ = false;
  Reason reason_ = Reason::Pending;
  Clock::time_point openedAt_{};
};

}  // namespace lithokraft
