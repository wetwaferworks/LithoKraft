// FrameHashRecorder self-test (Cortana 2026-11-09, Data hashes.txt spec).
// Test-first: the recorder is a stub, so these checks are expected to FAIL until
// the tick-paced implementation lands. Headless, software renderer only.
// LF line endings.
#include "lithokraft/game/frame_hash_recorder_selftest.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "lithokraft/game/FrameHashRecorder.h"
#include "litholattice/render/software_renderer.h"

namespace lithokraft {
namespace {

int fails = 0;
void check(bool ok, const char* what) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++fails;
}

namespace fs = std::filesystem;

constexpr int kW = 32;
constexpr int kH = 24;
// Fixed tick schedule: frame i is grabbed at tick 100 + i*4 (tick-paced, not wall-clock).
uint64_t tickForFrame(int frameIndex) { return 100ull + uint64_t(frameIndex) * 4ull; }

FrameHashHeader baseHeader(const char* renderer) {
  FrameHashHeader h;
  h.format_version = 1;
  h.commit_sha = "0000000000000000000000000000000000000000";  // pinned; real runs fill full SHA
  h.build_type = "DebugOpt";
  h.build_flags = "selftest";
  h.renderer = renderer;
  h.width = kW;
  h.height = kH;
  h.pixel_format = "RGB24";
  h.input_source = "bench:pinned-clear-pose";
  h.input_sha256.clear();
  h.thread_count = 1;
  h.device = "CPU";
  h.tps = 20;
  h.seed = 0xC0FFEEULL;
  h.hash_algorithms = "fnv1a64,sha256";
  h.prev_record_hash = cFrameHashRecorder::kZeroRecordHash;
  return h;
}

// Deterministic software-renderer frame: clear to a solid colour, convert to RGB24.
std::vector<uint8_t> softwareClearRgb(uint32_t argb) {
  litholattice::cSoftwareRenderer sw;
  sw.resize(uint32_t(kW), uint32_t(kH));
  sw.clear(argb);
  const auto& fb = sw.framebuffer();
  std::vector<uint8_t> rgb(size_t(kW) * size_t(kH) * 3);
  for (size_t i = 0, n = rgb.size() / 3; i < n; ++i) {
    const uint32_t p = fb.pixels[i];
    rgb[3 * i] = uint8_t(p >> 16);
    rgb[3 * i + 1] = uint8_t(p >> 8);
    rgb[3 * i + 2] = uint8_t(p);
  }
  return rgb;
}

void feedRun(cFrameHashRecorder& rec, const std::vector<uint8_t>& rgb, int frames) {
  for (int i = 0; i < frames; ++i) {
    rec.setSimStateHash("n/a");  // engine exposes finalWorldHash on demo trailer only
    rec.onFrameAtTick(rgb, kW, kH, tickForFrame(i));
  }
  rec.close();
}

bool readFile(const fs::path& p, std::string* out) {
  std::ifstream in(p, std::ios::binary);
  if (!in) return false;
  out->assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return true;
}

// (1) Two identical software-render runs -> byte-identical manifests + equal record_hash.
void testIdenticalRuns() {
  std::printf("frame_hash: (1) identical software-render runs\n");
  const auto rgb = softwareClearRgb(0xFF336699u);
  fs::path root = fs::temp_directory_path() / "lithokraft-frame-hash-selftest" / "identical";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root / "a", ec);
  fs::create_directories(root / "b", ec);

  cFrameHashRecorder a(baseHeader("software"));
  cFrameHashRecorder b(baseHeader("software"));
  feedRun(a, rgb, 4);
  feedRun(b, rgb, 4);

  const bool wa = a.writeManifest(root / "a");
  const bool wb = b.writeManifest(root / "b");
  check(wa && wb, "identical: both runs write hashes.txt");

  std::string ma, mb;
  const bool ra = readFile(root / "a" / "hashes.txt", &ma);
  const bool rb = readFile(root / "b" / "hashes.txt", &mb);
  check(ra && rb && ma == mb && !ma.empty(),
        "identical: hashes.txt byte-identical across two software-render runs");
  check(!a.recordHash().empty() && a.recordHash() == b.recordHash(),
        "identical: record_hash equal and non-empty");
  check(a.frameCount() == 4 && b.frameCount() == 4, "identical: frame_count == 4");
}

// (2) Frame N always lands on tick T; ticks stable across runs.
void testTickStability() {
  std::printf("frame_hash: (2) frame N -> tick T stability\n");
  const auto rgb = softwareClearRgb(0xFF112233u);
  fs::path root = fs::temp_directory_path() / "lithokraft-frame-hash-selftest" / "ticks";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root / "a", ec);
  fs::create_directories(root / "b", ec);

  cFrameHashRecorder a(baseHeader("software"));
  cFrameHashRecorder b(baseHeader("software"));
  feedRun(a, rgb, 5);
  feedRun(b, rgb, 5);
  check(a.writeManifest(root / "a") && b.writeManifest(root / "b"),
        "ticks: both runs write hashes.txt");

  std::string ma, mb;
  check(readFile(root / "a" / "hashes.txt", &ma) && readFile(root / "b" / "hashes.txt", &mb),
        "ticks: manifests readable");

  // Expect each frame line to carry frame_index and the pinned tick.
  bool allTicks = !ma.empty() && ma == mb;
  for (int i = 0; i < 5 && allTicks; ++i) {
    const std::string needle = "frame_index=" + std::to_string(i);
    const std::string tickNeedle = "tick=" + std::to_string(tickForFrame(i));
    if (ma.find(needle) == std::string::npos || ma.find(tickNeedle) == std::string::npos) allTicks = false;
  }
  check(allTicks, "ticks: frame N always records tick T; stable across runs");
}

// (3) Negative control: flip one pixel -> verify FAIL at that frame, message names it.
void testNegativePixelFlip() {
  std::printf("frame_hash: (3) negative control (one pixel flip)\n");
  auto rgb = softwareClearRgb(0xFF445566u);
  auto rgbFlip = rgb;
  // Flip one byte in frame content that will be used for the "actual" run's frame 2.
  if (!rgbFlip.empty()) rgbFlip[size_t(kW) * 3 + 3] ^= 0xFFu;  // one channel of pixel (1,1)

  fs::path root = fs::temp_directory_path() / "lithokraft-frame-hash-selftest" / "neg";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root / "expected", ec);
  fs::create_directories(root / "actual", ec);

  cFrameHashRecorder exp(baseHeader("software"));
  for (int i = 0; i < 4; ++i) {
    exp.setSimStateHash("n/a");
    exp.onFrameAtTick(rgb, kW, kH, tickForFrame(i));
  }
  exp.close();

  cFrameHashRecorder act(baseHeader("software"));
  for (int i = 0; i < 4; ++i) {
    act.setSimStateHash("n/a");
    const auto& src = (i == 2) ? rgbFlip : rgb;
    act.onFrameAtTick(src, kW, kH, tickForFrame(i));
  }
  act.close();

  check(exp.writeManifest(root / "expected") && act.writeManifest(root / "actual"),
        "negative: both manifests written");

  const FrameHashVerifyResult vr =
      cFrameHashRecorder::verify(root / "expected" / "hashes.txt", root / "actual" / "hashes.txt");
  check(!vr.ok && vr.first_fail_frame == 2,
        "negative: verify fails at first affected frame_index 2");
  const bool namesFrame =
      vr.message.find("frame 2") != std::string::npos ||
      vr.message.find("frame_index=2") != std::string::npos ||
      vr.message.find("frame_index 2") != std::string::npos;
  check(namesFrame, "negative: failure message names frame 2");
}

// (4) Frame count mismatch fails verification.
void testFrameCountMismatch() {
  std::printf("frame_hash: (4) frame count mismatch\n");
  const auto rgb = softwareClearRgb(0xFF778899u);
  fs::path root = fs::temp_directory_path() / "lithokraft-frame-hash-selftest" / "count";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root / "expected", ec);
  fs::create_directories(root / "actual", ec);

  cFrameHashRecorder exp(baseHeader("software"));
  feedRun(exp, rgb, 4);
  cFrameHashRecorder act(baseHeader("software"));
  feedRun(act, rgb, 3);

  check(exp.writeManifest(root / "expected") && act.writeManifest(root / "actual"),
        "count: both manifests written");
  const FrameHashVerifyResult vr =
      cFrameHashRecorder::verify(root / "expected" / "hashes.txt", root / "actual" / "hashes.txt");
  check(!vr.ok, "count: verify fails on frame_count mismatch");
  const bool mentionsCount =
      vr.message.find("frame_count") != std::string::npos ||
      vr.message.find("frame count") != std::string::npos ||
      vr.message.find("count") != std::string::npos;
  check(mentionsCount, "count: failure message mentions frame count");
}

// (5) Different renderer field -> separate record (record_hash / manifest differ).
void testRendererSeparatesRecords() {
  std::printf("frame_hash: (5) renderer field separates records\n");
  const auto rgb = softwareClearRgb(0xFF99AABBu);
  fs::path root = fs::temp_directory_path() / "lithokraft-frame-hash-selftest" / "renderer";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root / "software", ec);
  fs::create_directories(root / "fallback", ec);

  cFrameHashRecorder sw(baseHeader("software"));
  cFrameHashRecorder fb(baseHeader("lithokraft-fallback"));
  feedRun(sw, rgb, 3);
  feedRun(fb, rgb, 3);

  check(sw.writeManifest(root / "software") && fb.writeManifest(root / "fallback"),
        "renderer: both manifests written");

  std::string ms, mf;
  check(readFile(root / "software" / "hashes.txt", &ms) &&
            readFile(root / "fallback" / "hashes.txt", &mf) && !ms.empty() && ms != mf,
        "renderer: software vs fallback yields distinct hashes.txt");
  check(!sw.recordHash().empty() && sw.recordHash() != fb.recordHash(),
        "renderer: record_hash differs when renderer field differs");
}

}  // namespace

int runFrameHashRecorderSelfTest() {
  fails = 0;
  std::printf("frame_hash_recorder selftest\n");
  std::printf("  stub implemented()=%s (expect false until real sink lands)\n",
              cFrameHashRecorder::implemented() ? "true" : "false");
  testIdenticalRuns();
  testTickStability();
  testNegativePixelFlip();
  testFrameCountMismatch();
  testRendererSeparatesRecords();
  std::printf("frame_hash_recorder: %d failure(s)\n", fails);
  return fails;
}

}  // namespace lithokraft
