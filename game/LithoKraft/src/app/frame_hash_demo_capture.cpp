// frame_hash_demo_capture: headless software-renderer + cFrameHashRecorder demo.
// Does NOT touch LithoKraft.exe. Deterministic pinned clear-pose frames, writes
// hashes.txt + raw RGB, optionally encodes MP4 via LL_FFMPEG / ffmpeg.
// Usage: frame_hash_demo_capture <outdir> [frames=8] [width=64] [height=48]
#include "litholattice/core/quiet_crt.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "lithokraft/game/FrameHashRecorder.h"
#include "litholattice/crypto/sha256.h"
#include "litholattice/crypto/signing.h"
#include "litholattice/render/software_renderer.h"

namespace fs = std::filesystem;

static std::vector<uint8_t> renderFrame(int w, int h, int frameIndex) {
  // Deterministic "pinned pose": solid clear colour that advances by frame index.
  // (Full .lldemo replay needs the game binary; this proves the sink end-to-end.)
  const uint8_t r = uint8_t((0x33 + frameIndex * 3) & 0xFF);
  const uint8_t g = uint8_t((0x66 + frameIndex * 5) & 0xFF);
  const uint8_t b = uint8_t((0x99 + frameIndex * 7) & 0xFF);
  const uint32_t argb = 0xFF000000u | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
  litholattice::cSoftwareRenderer sw;
  sw.resize(uint32_t(w), uint32_t(h));
  sw.clear(argb);
  const auto& fb = sw.framebuffer();
  std::vector<uint8_t> rgb(size_t(w) * size_t(h) * 3);
  for (size_t i = 0, n = rgb.size() / 3; i < n; ++i) {
    const uint32_t p = fb.pixels[i];
    rgb[3 * i] = uint8_t(p >> 16);
    rgb[3 * i + 1] = uint8_t(p >> 8);
    rgb[3 * i + 2] = uint8_t(p);
  }
  return rgb;
}

static bool writeRaw(const fs::path& path, const std::vector<uint8_t>& rgb) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) return false;
  out.write(reinterpret_cast<const char*>(rgb.data()), std::streamsize(rgb.size()));
  return static_cast<bool>(out);
}

static int encodeMp4(const fs::path& outDir, int w, int h, int frames, int fps) {
  const char* ff = std::getenv("LL_FFMPEG");
  const std::string ffmpeg = ff && *ff ? ff : "ffmpeg";
  // Concat demuxer list of raw frames via image2 pipe from a single rawvideo stream:
  // we already have frame_XXXX.rgb; build a concat of rawvideo is awkward, so cat into one blob.
  const fs::path blob = outDir / "all_frames.rgb";
  {
    std::ofstream cat(blob, std::ios::binary | std::ios::trunc);
    if (!cat) return 2;
    for (int i = 0; i < frames; ++i) {
      char name[64];
      std::snprintf(name, sizeof(name), "frame_%04d.rgb", i);
      std::ifstream in(outDir / name, std::ios::binary);
      if (!in) return 3;
      cat << in.rdbuf();
    }
  }
  const fs::path mp4 = outDir / "frames.mp4";
  // Windows system() goes through cmd.exe; mirror FrameVideoEncoder quoting.
  std::string inner = std::string("\"") + ffmpeg + "\" -loglevel error -y -f rawvideo -pix_fmt rgb24 -s " +
                      std::to_string(w) + "x" + std::to_string(h) + " -r " + std::to_string(fps) +
                      " -i \"" + blob.string() +
                      "\" -c:v libx264 -preset ultrafast -tune animation -crf 18 -pix_fmt yuv420p "
                      "-movflags +faststart \"" + mp4.string() + "\"";
#if defined(_WIN32)
  std::string cmd = "\"" + inner + "\"";
#else
  std::string cmd = inner;
#endif
  std::printf("ffmpeg: %s\n", cmd.c_str());
  const int rc = std::system(cmd.c_str());
  return rc;
}

static std::string runOnce(const fs::path& outDir, int frames, int w, int h) {
  std::error_code ec;
  fs::remove_all(outDir, ec);
  fs::create_directories(outDir, ec);

  lithokraft::FrameHashHeader hdr;
  hdr.format_version = 1;
  hdr.commit_sha = "8ed3ed05f192ace380a70df8f8c1121d1c60e1b3";
  hdr.build_type = "DebugOpt";
  hdr.build_flags = "frame_hash_demo_capture";
  hdr.renderer = "software";
  hdr.width = w;
  hdr.height = h;
  hdr.pixel_format = "RGB24";
  hdr.input_source = "bench:pinned-clear-pose";
  hdr.thread_count = 1;
  hdr.device = "CPU";
  hdr.tps = 20;
  hdr.seed = 0xC0FFEEULL;
  hdr.hash_algorithms = "fnv1a64,sha256";
  hdr.prev_record_hash = lithokraft::cFrameHashRecorder::kZeroRecordHash;

  lithokraft::cFrameHashRecorder rec(hdr);
  lithokraft::IFrameSink* sink = &rec;  // IFrameSink-compatible

  for (int i = 0; i < frames; ++i) {
    const uint64_t tick = 100ull + uint64_t(i) * 4ull;
    auto rgb = renderFrame(w, h, i);
    char name[64];
    std::snprintf(name, sizeof(name), "frame_%04d.rgb", i);
    writeRaw(outDir / name, rgb);
    rec.setSimStateHash("n/a");
    rec.setCurrentTick(tick);
    sink->onFrame(rgb, w, h);  // tick already set — tick-paced host contract
  }
  sink->close();
  if (!rec.writeManifest(outDir)) {
    std::printf("FAIL: writeManifest\n");
    return {};
  }
  std::printf("wrote %s (%d frames) record_hash=%s\n", (outDir / "hashes.txt").string().c_str(),
              rec.frameCount(), rec.recordHash().c_str());
  return rec.recordHash();
}

int main(int argc, char** argv) {
  litholattice::quietCrtDialogsForTests();
  if (argc < 2) {
    std::printf("usage: %s <outdir> [frames=8] [width=64] [height=48]\n", argc ? argv[0] : "frame_hash_demo_capture");
    return 2;
  }
  const fs::path root = argv[1];
  const int frames = argc > 2 ? std::atoi(argv[2]) : 8;
  const int w = argc > 3 ? std::atoi(argv[3]) : 64;
  const int h = argc > 4 ? std::atoi(argv[4]) : 48;
  if (frames <= 0 || w <= 0 || h <= 0 || (w % 2) || (h % 2)) {
    std::printf("FAIL: frames>0 and even width/height required for yuv420p\n");
    return 2;
  }

  const fs::path runA = root / "run-a";
  const fs::path runB = root / "run-b";
  const std::string ha = runOnce(runA, frames, w, h);
  const std::string hb = runOnce(runB, frames, w, h);
  if (ha.empty() || hb.empty() || ha != hb) {
    std::printf("FAIL: record_hash mismatch a=%s b=%s\n", ha.c_str(), hb.c_str());
    return 1;
  }
  std::printf("PASS: identical record_hash across two runs: %s\n", ha.c_str());

  const auto vr = lithokraft::cFrameHashRecorder::verify(runA / "hashes.txt", runB / "hashes.txt");
  std::printf("verify(run-a, run-b): ok=%d msg=%s\n", int(vr.ok), vr.message.c_str());
  if (!vr.ok) return 1;

  // Encode MP4 from run-a frames (view only; not part of the golden chain).
  const int enc = encodeMp4(runA, w, h, frames, 20);
  std::printf("ffmpeg exit=%d mp4=%s\n", enc, (runA / "frames.mp4").string().c_str());
  if (enc != 0) {
    std::printf("WARN: ffmpeg encode failed (hashes still OK)\n");
    return 0;  // hashes are the contract; MP4 is view-only
  }
  std::printf("OK demo capture complete under %s\n", root.string().c_str());
  return 0;
}