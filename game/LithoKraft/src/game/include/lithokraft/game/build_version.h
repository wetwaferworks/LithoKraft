// 2026-10-16 (JD): LithoKraft build version "Alpha a.b.c.d". The counter lives in the tracked file
// build_counter.txt at the tree root and is bumped on every build (cmake/bump_build_counter.cmake).
// Display / log only: never part of the determinism hash.
#pragma once
#include <array>
#include <string>

namespace lithokraft {

/// One build bump: d += 1; each field runs 0..63, reaching 64 resets it and carries left.
/// Mirrors bump_build_counter.cmake (selftested: 0.0.1.63 -> 0.0.2.0, 0.0.63.63 -> 0.1.0.0).
inline std::array<int, 4> bumpBuildVersion(std::array<int, 4> v) {
  for (int i = 3; i >= 0; --i) {
    if (++v[size_t(i)] < 64) return v;
    v[size_t(i)] = 0;
  }
  return v;  // 63.63.63.63 wraps to 0.0.0.0
}

/// "Alpha a.b.c.d" of this build (generated header).
const char* buildVersion();
/// "LithoKraft Alpha a.b.c.d (commit) Release|Debug built <date time>".
std::string buildInfo();

}  // namespace lithokraft
