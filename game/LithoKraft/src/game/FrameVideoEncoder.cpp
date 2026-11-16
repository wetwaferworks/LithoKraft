#include "lithokraft/game/FrameVideoEncoder.h"
#include <cstdlib>
#include <filesystem>
#include "litholattice/core/log.h"

#if defined(__wasm__)
#define LK_PCLOSE(p) ((void)(p), 0)  // never opened on web (open() fails safe)
#elif defined(_WIN32)
#define LK_POPEN _popen
#define LK_PCLOSE _pclose
#else
#define LK_POPEN popen
#define LK_PCLOSE pclose
#endif

namespace lithokraft {

cFrameVideoEncoder::cFrameVideoEncoder(std::string dir, Clock::time_point start, int fps)
    : dir_(std::move(dir)), start_(start), fps_(fps) {}

cFrameVideoEncoder::~cFrameVideoEncoder() { close(); }

bool cFrameVideoEncoder::open(int width, int height) {
  std::error_code ec;
  std::filesystem::create_directories(dir_, ec);
  const char* ff = std::getenv("LL_FFMPEG");
  const std::string out = (std::filesystem::path(dir_) / "frames.mp4").string();
  std::string cmd = std::string("\"") + (ff ? ff : "ffmpeg") + "\" -loglevel error -y -f rawvideo -pix_fmt rgb24 -s " +
                    std::to_string(width) + "x" + std::to_string(height) + " -r " + std::to_string(fps_) +
                    " -i - -c:v libx264 -preset ultrafast -tune animation -crf 12 -threads 4 -pix_fmt yuv420p -movflags +faststart \"" +
                    out + "\"";
#if defined(__wasm__)
  // 2026-11-16 (S5 web): no processes in the browser, so no ffmpeg pipe. Fail safe: the
  // frame dump is off for the run, with one clear log line (close() sees no pipe).
  (void)cmd;
  litholattice::logError("frame-dump: video encoding is not available in the web build (no ffmpeg process); frame dump off");
  failed_ = true;
  return false;
#elif defined(_WIN32)
  cmd = "\"" + cmd + "\"";  // cmd.exe /c quoting
  pipe_ = LK_POPEN(cmd.c_str(), "wb");
#else
  pipe_ = LK_POPEN(cmd.c_str(), "w");
#endif
  if (!pipe_) {
    litholattice::logError("frame-dump: cannot start ffmpeg");
    failed_ = true;
    return false;
  }
  w_ = width;
  h_ = height;
  litholattice::logInfo("frame-dump: %dx%d @%d fps -> frames.mp4", width, height, fps_);
  return true;
}

void cFrameVideoEncoder::onFrame(const std::vector<uint8_t>& rgb, int width, int height) {
  if (failed_) return;
  if (!pipe_ && !open(width, height)) return;
  if (width != w_ || height != h_) return;  // resized mid-run: the ffmpeg stream is fixed-size
  ++grabbed_;
  const double t = std::chrono::duration<double>(Clock::now() - start_).count();
  const long long due = (long long)(t * fps_) + 1;
  for (int k = 0; written_ < due && k < 4; ++k, ++written_) std::fwrite(rgb.data(), 1, rgb.size(), pipe_);
  if (written_ < due) written_ = due;  // far behind: drop instead of a burst
}

void cFrameVideoEncoder::close() {
  if (!pipe_) return;
  LK_PCLOSE(pipe_);
  pipe_ = nullptr;
  litholattice::logInfo("frame-dump: closed (%lld frames grabbed, %lld written)", grabbed_, written_);
}

}  // namespace lithokraft
