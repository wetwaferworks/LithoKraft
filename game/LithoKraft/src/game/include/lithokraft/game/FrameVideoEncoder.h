#pragma once
// Showcase --frame-dump DIR: pipes RGB24 frames into ffmpeg (libx264) as DIR/frames.mp4
// at a constant 30 fps on the wall clock (duplicates up to 4 when slow, drops when far behind).
#include <chrono>
#include <cstdio>
#include <string>
#include "lithokraft/game/IFrameSink.h"

namespace lithokraft {

class cFrameVideoEncoder final : public IFrameSink {
 public:
  using Clock = std::chrono::steady_clock;
  cFrameVideoEncoder(std::string dir, Clock::time_point start, int fps = 30);
  ~cFrameVideoEncoder() override;
  void onFrame(const std::vector<uint8_t>& rgb, int width, int height) override;
  void close() override;
  long long framesWritten() const { return written_; }
  long long framesGrabbed() const { return grabbed_; }

 private:
  bool open(int width, int height);
  std::string dir_;
  Clock::time_point start_;
  int fps_;
  std::FILE* pipe_ = nullptr;
  bool failed_ = false;
  int w_ = 0, h_ = 0;
  long long written_ = 0, grabbed_ = 0;
};

}  // namespace lithokraft
