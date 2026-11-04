#include "BenchSampleGate.h"

#include <algorithm>
#include <cstdlib>

namespace lithokraft {

namespace {
double secondsSince(cBenchSampleGate::Clock::time_point a, cBenchSampleGate::Clock::time_point b) {
  return std::chrono::duration<double>(b - a).count();
}
}  // namespace

void cBenchSampleGate::configure(double warmupS, double capS, bool exempt) {
  warmupS_ = warmupS;
  capS_ = capS;
  exempt_ = exempt;
  reason_ = Reason::Pending;
  openedAfterS_ = 0.0;
}

bool cBenchSampleGate::wantsProbe(Clock::time_point tStart, Clock::time_point now) const {
  return !open() && !exempt_ && secondsSince(tStart, now) > warmupS_;
}

bool cBenchSampleGate::update(Clock::time_point tStart, Clock::time_point now, bool settled) {
  if (open()) return false;
  const double t = secondsSince(tStart, now);
  if (t <= warmupS_) return false;
  if (exempt_) reason_ = Reason::Exempt;
  else if (settled) reason_ = Reason::Settled;
  else if (t > warmupS_ + capS_) reason_ = Reason::Cap;
  else return false;
  openedAt_ = now;
  openedAfterS_ = t;
  return true;
}

const char* cBenchSampleGate::reasonName() const {
  switch (reason_) {
    case Reason::Settled: return "settled";
    case Reason::Cap: return "cap";
    case Reason::Exempt: return "exempt";
    default: return "pending";
  }
}

double cBenchSampleGate::sinceOpenS(Clock::time_point now) const { return open() ? secondsSince(openedAt_, now) : 0.0; }

double cBenchSampleGate::cameraSeconds(Clock::time_point tStart, Clock::time_point now) const {
  if (!open()) return std::min(secondsSince(tStart, now), warmupS_);
  return warmupS_ + secondsSince(openedAt_, now);
}

double cBenchSampleGate::capFromEnv() {
  const char* e = std::getenv("LL_BENCH_SETTLE_CAP");
  const double v = e && *e ? std::atof(e) : 120.0;
  return v > 0 ? v : 120.0;
}

}  // namespace lithokraft
