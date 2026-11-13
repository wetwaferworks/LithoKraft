// cFrameHashRecorder Ã¢â‚¬â€ tick-paced hashes.txt sink (Data spec, Cortana 2026-11-09).
// One frame per sim tick grab; no wall-clock dup/drop. record_hash chains frame
// lines only (plus prev_record_hash). wall_clock_start is written but excluded
// from the chain. Paths never enter hashed content.
#include "lithokraft/game/FrameHashRecorder.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "litholattice/assets/image.h"
#include "litholattice/crypto/sha256.h"
#include "litholattice/crypto/signing.h"

namespace lithokraft {
namespace {

using litholattice::crypto::Sha256;
using litholattice::crypto::sha256;
using litholattice::crypto::toHex;
using litholattice::crypto::fromHex;

std::string hexU64(uint64_t v) {
  char buf[17];
  std::snprintf(buf, sizeof(buf), "%016llX", static_cast<unsigned long long>(v));
  return buf;
}

std::string nowIso8601Utc() {
  using clock = std::chrono::system_clock;
  const auto t = clock::to_time_t(clock::now());
  std::tm tm{};
#if defined(_WIN32)
  gmtime_s(&tm, &t);
#else
  gmtime_r(&t, &tm);
#endif
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                tm.tm_hour, tm.tm_min, tm.tm_sec);
  return buf;
}

uint64_t fnv1a64FromRgb(const std::vector<uint8_t>& rgb, int width, int height) {
  std::vector<uint8_t> rgba(size_t(width) * size_t(height) * 4);
  const size_t n = size_t(width) * size_t(height);
  for (size_t i = 0; i < n; ++i) {
    rgba[4 * i + 0] = rgb[3 * i + 0];
    rgba[4 * i + 1] = rgb[3 * i + 1];
    rgba[4 * i + 2] = rgb[3 * i + 2];
    rgba[4 * i + 3] = 255;
  }
  litholattice::Image img(width, height, std::move(rgba));
  return img.hash();
}

std::string sha256Hex(const uint8_t* data, size_t len) {
  const auto d = sha256(data, len);
  return toHex(d.data(), d.size());
}

bool readAll(const std::filesystem::path& p, std::string* out) {
  std::ifstream in(p, std::ios::binary);
  if (!in) return false;
  out->assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return true;
}

// Split on '\n'; drop optional trailing '\r'. Keep empty lines.
std::vector<std::string> splitLines(const std::string& text) {
  std::vector<std::string> lines;
  size_t i = 0;
  while (i <= text.size()) {
    size_t j = text.find('\n', i);
    if (j == std::string::npos) j = text.size();
    std::string line = text.substr(i, j - i);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    lines.push_back(std::move(line));
    if (j == text.size()) break;
    i = j + 1;
  }
  return lines;
}

int parseFrameIndex(const std::string& line) {
  const std::string key = "frame_index=";
  const size_t p = line.find(key);
  if (p == std::string::npos) return -1;
  size_t i = p + key.size();
  if (i >= line.size() || line[i] < '0' || line[i] > '9') return -1;
  int v = 0;
  while (i < line.size() && line[i] >= '0' && line[i] <= '9') {
    v = v * 10 + (line[i] - '0');
    ++i;
  }
  return v;
}

bool isFrameLine(const std::string& line) {
  return line.compare(0, 12, "frame_index=") == 0;
}

}  // namespace

cFrameHashRecorder::cFrameHashRecorder(FrameHashHeader header) : header_(std::move(header)) {
  if (header_.prev_record_hash.empty()) header_.prev_record_hash = kZeroRecordHash;
  if (header_.hash_algorithms.empty()) header_.hash_algorithms = "fnv1a64,sha256";
  if (header_.pixel_format.empty()) header_.pixel_format = "RGB24";
  if (header_.renderer.empty()) header_.renderer = "software";
  if (header_.device.empty()) header_.device = "CPU";
  wall_clock_start_ = nowIso8601Utc();
  recomputeRecordHash();
}

cFrameHashRecorder::~cFrameHashRecorder() { close(); }

bool cFrameHashRecorder::implemented() { return true; }

void cFrameHashRecorder::setCurrentTick(uint64_t tick) { current_tick_ = tick; }

void cFrameHashRecorder::onGrabTick(uint64_t tick) {
  last_sim_tick_ = tick;
  if (!ordinal_ticks_) setCurrentTick(tick);
}

void cFrameHashRecorder::setOutputDir(std::filesystem::path dir) { output_dir_ = std::move(dir); }

void cFrameHashRecorder::setOrdinalTicks(bool on) { ordinal_ticks_ = on; }
void cFrameHashRecorder::setSkipFrames(int n) { skip_frames_ = n < 0 ? 0 : n; }
void cFrameHashRecorder::setMaxFrames(int n) { max_frames_ = n < 0 ? 0 : n; }
void cFrameHashRecorder::setDedupSimTicks(bool on) { dedup_sim_ticks_ = on; }
void cFrameHashRecorder::setMinSimTick(uint64_t n) { min_sim_tick_ = n; }
void cFrameHashRecorder::setRequireContiguousSimTicks(bool on) { require_contiguous_sim_ticks_ = on; }


void cFrameHashRecorder::setSimStateHash(std::string hex_or_na) {
  current_sim_hash_ = hex_or_na.empty() ? "n/a" : std::move(hex_or_na);
}

std::string cFrameHashRecorder::makeFrameLine(const FrameEntry& e) const {
  // Canonical frame line. renderer is included so software vs fallback yield
  // distinct record_hash chains (Data: separate records by renderer field).
  std::ostringstream os;
  // sim worldTick is intentionally NOT on the chained line under ordinal_ticks_
  // (present/sim phase jitter); it is appended to sim_ticks.txt in onFrame.
  os << "frame_index=" << e.index << " tick=" << e.tick << " renderer=" << header_.renderer
     << " fnv1a64=0x" << hexU64(e.fnv1a64) << " sha256=" << e.sha256_hex
     << " sim_state_hash=" << e.sim_state_hash;
  return os.str();
}

void cFrameHashRecorder::recomputeRecordHash() {
  std::array<uint8_t, 32> H{};
  if (!fromHex(header_.prev_record_hash, H.data(), 32)) {
    // Corrupt prev: treat as zeros so the recorder still produces a digest.
    H.fill(0);
  }
  for (const FrameEntry& e : frames_) {
    Sha256 s;
    s.update(H.data(), H.size());
    s.update(reinterpret_cast<const uint8_t*>(e.line.data()), e.line.size());
    H = s.final();
  }
  record_hash_ = toHex(H.data(), H.size());
}

void cFrameHashRecorder::onFrameAtTick(const std::vector<uint8_t>& rgb, int width, int height, uint64_t tick) {
  current_tick_ = tick;
  onFrame(rgb, width, height);
}

void cFrameHashRecorder::onFrame(const std::vector<uint8_t>& rgb, int width, int height) {
  if (closed_) return;
  if (width <= 0 || height <= 0) return;
  const size_t need = size_t(width) * size_t(height) * 3;
  if (rgb.size() < need) return;

  ++presents_seen_;
  if (presents_seen_ <= skip_frames_) return;
  if (min_sim_tick_ > 0 && last_sim_tick_ < min_sim_tick_) return;
  if (max_frames_ > 0 && static_cast<int>(frames_.size()) >= max_frames_) return;

  if (ordinal_ticks_ && dedup_sim_ticks_) {
    // Present-path: at most one record per sim tick (Data: no wall-clock dups).
    if (last_sim_tick_ == last_recorded_sim_tick_) return;
  }
  if (require_contiguous_sim_ticks_ && last_recorded_sim_tick_ != ~0ull) {
    // Every sim tick presented once, in order: reject jumps/skips.
    if (last_sim_tick_ != last_recorded_sim_tick_ + 1ull) return;
  }
  if (ordinal_ticks_ && dedup_sim_ticks_) {
    last_recorded_sim_tick_ = last_sim_tick_;
  } else if (require_contiguous_sim_ticks_) {
    last_recorded_sim_tick_ = last_sim_tick_;
  }


  if (header_.width == 0) header_.width = width;
  if (header_.height == 0) header_.height = height;

  FrameEntry e;
  e.index = static_cast<int>(frames_.size());
  if (ordinal_ticks_) {
    // Stable ordinal tick for cross-run record_hash; sim_tick kept on the line.
    e.tick = 100ull + uint64_t(e.index);
    current_tick_ = e.tick;
  } else {
    e.tick = current_tick_;
  }
  e.fnv1a64 = fnv1a64FromRgb(rgb, width, height);
  e.sha256_hex = sha256Hex(rgb.data(), need);
  e.sim_state_hash = current_sim_hash_.empty() ? "n/a" : current_sim_hash_;
  e.line = makeFrameLine(e);
  frames_.push_back(std::move(e));
  recomputeRecordHash();
  if (!output_dir_.empty()) {
    std::error_code ec;
    std::filesystem::create_directories(output_dir_, ec);
    char name[64];
    std::snprintf(name, sizeof(name), "frame_%04d.rgb", frames_.back().index);
    const auto path = output_dir_ / name;
    std::ofstream raw(path, std::ios::binary | std::ios::trunc);
    if (raw) raw.write(reinterpret_cast<const char*>(rgb.data()), std::streamsize(need));
    if (ordinal_ticks_) {
      std::ofstream st(output_dir_ / "sim_ticks.txt", std::ios::binary | std::ios::app);
      if (st) st << frames_.back().index << ' ' << last_sim_tick_ << '\n';
    }
  }
}

void cFrameHashRecorder::close() {
  if (closed_) return;
  closed_ = true;
  if (!output_dir_.empty()) {
    const bool ok = writeManifest(output_dir_);
    std::fprintf(stderr, "frame-hash: writeManifest(%s) frames=%d record_hash=%s ok=%d\n",
                 output_dir_.string().c_str(), frameCount(), record_hash_.c_str(), int(ok));
  }
}

int cFrameHashRecorder::frameCount() const { return static_cast<int>(frames_.size()); }

std::string cFrameHashRecorder::recordHash() const { return record_hash_; }

bool cFrameHashRecorder::writeManifest(const std::filesystem::path& outDir) const {
  std::error_code ec;
  std::filesystem::create_directories(outDir, ec);
  const auto path = outDir / "hashes.txt";
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) return false;

  auto line = [&](const std::string& s) { out << s << '\n'; };

  line("format_version=" + std::to_string(header_.format_version));
  line("commit_sha=" + header_.commit_sha);
  line("build_type=" + header_.build_type);
  line("build_flags=" + header_.build_flags);
  line("renderer=" + header_.renderer);
  line("resolution=" + std::to_string(header_.width) + "x" + std::to_string(header_.height));
  line("pixel_format=" + header_.pixel_format);
  line("input_source=" + header_.input_source);
  line("input_sha256=" + header_.input_sha256);
  if (!header_.model_sha256.empty()) line("model_sha256=" + header_.model_sha256);
  if (!header_.weights_hash.empty()) line("weights_hash=" + header_.weights_hash);
  line("thread_count=" + std::to_string(header_.thread_count));
  line("device=" + header_.device);
  line("tps=" + std::to_string(header_.tps));
  line("seed=" + std::to_string(header_.seed));
  line("hash_algorithms=" + header_.hash_algorithms);
  line("prev_record_hash=" + header_.prev_record_hash);
  line("");  // blank separator before frames

  for (const FrameEntry& e : frames_) line(e.line);

  line("");
  line("frame_count=" + std::to_string(frames_.size()));
  line("record_hash=" + record_hash_);
  // wall_clock_start is informational and must NOT appear in hashes.txt: the
  // manifest excludes per-run varying fields so two identical runs are
  // byte-identical. Written beside the manifest as wall_clock.txt instead.
  out.flush();
  if (!out) return false;
  out.close();
  {
    std::ofstream wc(outDir / "wall_clock.txt", std::ios::binary | std::ios::trunc);
    if (wc) wc << "wall_clock_start=" << wall_clock_start_ << '\n';
  }
  return true;
}

FrameHashVerifyResult cFrameHashRecorder::verify(const std::filesystem::path& expected,
                                                 const std::filesystem::path& actual) {
  FrameHashVerifyResult r;
  std::string expText, actText;
  if (!readAll(expected, &expText)) {
    r.message = "verify: cannot read expected hashes.txt";
    return r;
  }
  if (!readAll(actual, &actText)) {
    r.message = "verify: cannot read actual hashes.txt";
    return r;
  }

  const auto expLines = splitLines(expText);
  const auto actLines = splitLines(actText);

  auto findFrameCount = [](const std::vector<std::string>& lines) -> int {
    for (const auto& L : lines) {
      if (L.compare(0, 12, "frame_count=") == 0) {
        return std::atoi(L.c_str() + 12);
      }
    }
    return -1;
  };
  auto collectFrames = [](const std::vector<std::string>& lines) {
    std::vector<std::string> f;
    for (const auto& L : lines)
      if (isFrameLine(L)) f.push_back(L);
    return f;
  };

  const auto expFrames = collectFrames(expLines);
  const auto actFrames = collectFrames(actLines);
  const int expCount = findFrameCount(expLines);
  const int actCount = findFrameCount(actLines);

  if (expCount >= 0 && actCount >= 0 && expCount != actCount) {
    r.message = "frame_count mismatch: expected " + std::to_string(expCount) + " actual " +
                std::to_string(actCount);
    return r;
  }
  if (static_cast<int>(expFrames.size()) != static_cast<int>(actFrames.size())) {
    r.message = "frame_count mismatch: expected " + std::to_string(expFrames.size()) + " actual " +
                std::to_string(actFrames.size());
    return r;
  }

  for (size_t i = 0; i < expFrames.size(); ++i) {
    if (expFrames[i] != actFrames[i]) {
      const int idx = parseFrameIndex(expFrames[i]);
      r.first_fail_frame = (idx >= 0) ? idx : static_cast<int>(i);
      r.message = "frame " + std::to_string(r.first_fail_frame) +
                  " mismatch (frame_index=" + std::to_string(r.first_fail_frame) + ")";
      return r;
    }
  }

  // Also require record_hash footer equality when present.
  auto findRecordHash = [](const std::vector<std::string>& lines) -> std::string {
    for (const auto& L : lines)
      if (L.compare(0, 12, "record_hash=") == 0) return L.substr(12);
    return {};
  };
  const std::string er = findRecordHash(expLines);
  const std::string ar = findRecordHash(actLines);
  if (!er.empty() && !ar.empty() && er != ar) {
    r.message = "record_hash mismatch";
    return r;
  }

  r.ok = true;
  r.message = "ok";
  return r;
}

}  // namespace lithokraft
