// lithokraft_fragment_selftest: headless Fragment state tests (no window,
// no GL context). Exit code = failure count. LF line endings.
#include "litholattice/core/quiet_crt.h"
#include <cstdio>

#include "lithokraft/game/fragment_selftest.h"

int main() {
  litholattice::quietCrtDialogsForTests();  // CRT asserts -> stderr, no dialog
  const int fails = lithokraft::runFragmentSelfTest();
  std::printf("exit: %d\n", fails);
  return fails ? 1 : 0;
}
