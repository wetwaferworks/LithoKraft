#include "lithokraft/game/FrameHashRecorder.h"
#include <cstdio>
#include <string>
int main(int argc, char** argv) {
  if (argc < 3) { std::fprintf(stderr, "usage: %s expected.txt actual.txt\n", argv[0]); return 2; }
  auto r = lithokraft::cFrameHashRecorder::verify(argv[1], argv[2]);
  std::printf("ok=%d first_fail_frame=%d msg=%s\n", int(r.ok), r.first_fail_frame, r.message.c_str());
  return r.ok ? 0 : 1;
}