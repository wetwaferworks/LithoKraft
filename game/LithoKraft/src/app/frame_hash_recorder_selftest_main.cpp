// lithokraft_frame_hash_selftest: headless FrameHashRecorder contract tests.
// Exit code = failure count. No window / GPU. LF line endings.
#include "litholattice/core/quiet_crt.h"
#include <cstdio>
#include "lithokraft/game/frame_hash_recorder_selftest.h"

int main() {
  litholattice::quietCrtDialogsForTests();
  const int fails = lithokraft::runFrameHashRecorderSelfTest();
  std::printf("exit: %d\n", fails);
  return fails ? 1 : 0;
}
