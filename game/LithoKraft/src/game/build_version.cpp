// 2026-10-16 (JD): the only file that includes the per-build generated header, so a build bump
// recompiles just this translation unit.
#include "lithokraft/game/build_version.h"
#include "lithokraft_build_counter.h"
#ifndef LITHOKRAFT_COMMIT
#define LITHOKRAFT_COMMIT "nogit"
#endif

namespace lithokraft {
const char* buildVersion() { return LITHOKRAFT_ALPHA_VERSION; }
const char* gameCounter() { return LITHOKRAFT_GAME_COUNTER; }
std::string buildInfo() {
#ifdef NDEBUG
  const char* type = "Release";
#elif defined(LK_DEBUGOPT)
  const char* type = "DebugOpt";  // 2026-10-25: optimised, debug logging kept
#else
  const char* type = "Debug";
#endif
  return std::string("LithoKraft ") + LITHOKRAFT_GAME_COUNTER + " | " + LITHOKRAFT_ALPHA_VERSION + " (" + LITHOKRAFT_COMMIT + ") " + type + " built " + __DATE__ + " " + __TIME__;
}
}  // namespace lithokraft
