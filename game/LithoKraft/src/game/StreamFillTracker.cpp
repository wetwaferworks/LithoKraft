#include "lithokraft/game/StreamFillTracker.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "litholattice/core/log.h"

namespace lithokraft {

void cStreamFillTracker::start(Clock::time_point t0) {
  wanted_.clear(); ready_.clear(); ms_.clear();
  t0_ = t0; tEnd_ = t0;
  started_ = true; finished_ = false;
}

void cStreamFillTracker::noteWanted(uint64_t key, Clock::time_point now) {
  if (!started_ || finished_ || ready_.count(key)) return;
  wanted_.emplace(key, now);
}

void cStreamFillTracker::noteReady(uint64_t key, Clock::time_point now) {
  if (!started_ || finished_) return;
  const auto it = wanted_.find(key);
  if (it == wanted_.end() || !ready_.insert(key).second) return;
  ms_.push_back(std::chrono::duration<double, std::milli>(now - it->second).count());
  wanted_.erase(it);
}

void cStreamFillTracker::finish(Clock::time_point now) {
  if (!started_ || finished_) return;
  tEnd_ = now;
  finished_ = true;
}

cStreamFillTracker::Summary cStreamFillTracker::summary() const {
  Summary s;
  s.columns = ms_.size();
  s.totalS = std::chrono::duration<double>((finished_ ? tEnd_ : t0_) - t0_).count();
  if (ms_.empty()) return s;
  std::vector<double> v = ms_;
  std::sort(v.begin(), v.end());
  double sum = 0;
  for (const double x : v) sum += x;
  const auto pct = [&v](double p) { return v[std::min(v.size() - 1, size_t(std::ceil(p * double(v.size()))) - (p > 0 ? 1 : 0))]; };
  s.avgMs = sum / double(v.size());
  s.p50Ms = pct(0.50);
  s.p99Ms = pct(0.99);
  s.maxMs = v.back();
  s.colsPerS = s.totalS > 0 ? double(s.columns) / s.totalS : 0;
  return s;
}

void cStreamFillTracker::report(const char* renderer, size_t columnsInRadius, const char* jsonPath) const {
  const Summary s = summary();
  const char* state = finished_ ? "complete" : "incomplete";
  litholattice::logInfo("stream bench: %s, %zu of %zu columns in %.3f s, %.1f cols/s, per column avg %.1f p50 %.1f p99 %.1f max %.1f ms",
                        state, s.columns, columnsInRadius, s.totalS, s.colsPerS, s.avgMs, s.p50Ms, s.p99Ms, s.maxMs);
  std::printf("STREAMBENCH %s %s columns=%zu/%zu total=%.3f s cols/s=%.1f avg=%.1f ms p50=%.1f ms p99=%.1f ms max=%.1f ms\n", renderer, state,
              s.columns, columnsInRadius, s.totalS, s.colsPerS, s.avgMs, s.p50Ms, s.p99Ms, s.maxMs);
  std::fflush(stdout);
  if (!jsonPath || !*jsonPath) return;
  std::FILE* f = std::fopen(jsonPath, "wb");
  if (!f) { litholattice::logError("stream bench: cannot write %s", jsonPath); return; }
  std::fprintf(f,
               "{\n  \"renderer\": \"%s\", \"complete\": %s, \"columns\": %zu, \"columns_in_radius\": %zu, \"total_s\": %.4f, \"cols_per_s\": %.3f,\n"
               "  \"column_ms\": {\"avg\": %.3f, \"p50\": %.3f, \"p99\": %.3f, \"max\": %.3f}\n}\n",
               renderer, finished_ ? "true" : "false", s.columns, columnsInRadius, s.totalS, s.colsPerS, s.avgMs, s.p50Ms, s.p99Ms, s.maxMs);
  std::fclose(f);
}

}  // namespace lithokraft
