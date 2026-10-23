// LithoKraft build version. Since 2026-10-23 (JD, fix run 2) the counter is a rolling decimal integer in
// LITHOKRAFT_BUILD_COUNTER.txt (tree root), bumped per build; public label from LITHOKRAFT_VERSION.txt.
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

/// "<public label> build N" of this build, e.g. "Alpha 0.0.1 build 7" (generated header).
const char* buildVersion();
/// "LithoKraft Alpha a.b.c.d (commit) Release|Debug built <date time>".
std::string buildInfo();

}  // namespace lithokraft
