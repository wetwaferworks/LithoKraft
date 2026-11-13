#pragma once
// Tick-paced hash-emitting frame sink (Data manifest spec, Cortana 2026-11-09).
// Implements IFrameSink so it can sit on cFrameDumpHub; pacing is by sim tick,
// never wall-clock. Writes hashes.txt (header / per-frame / footer+record_hash).
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "lithokraft/game/IFrameSink.h"

namespace lithokraft {

/// Manifest header fields written once. Paths and wall-clock times are excluded
/// from anything that feeds record_hash (input is a basename + content hash, or
/// a bench path ID — never an absolute path).
struct FrameHashHeader {
  int format_version = 1;
  std::string commit_sha;       ///< full git SHA when available
  std::string build_type;       ///< e.g. DebugOpt / Release
  std::string build_flags;      ///< free-form compile flags label
  std::string renderer;         ///< "software" or a fallback name — separate records
  int width = 0;
  int height = 0;
  std::string pixel_format;     ///< "RGB24" (IFrameSink wire) or "RGBA8"
  std::string input_source;     ///< .lldemo basename, or bench path ID
  std::string input_sha256;     ///< SHA-256 of .lldemo bytes; empty for bench ID
  std::string model_sha256;     ///< optional; only when a PCHIE run drives it
  std::string weights_hash;     ///< optional; only when a PCHIE run drives it
  int thread_count = 1;
  std::string device;           ///< "CPU" or "CUDA"
  int tps = 0;
  uint64_t seed = 0;
  std::string hash_algorithms;  ///< e.g. "fnv1a64,sha256"
  std::string prev_record_hash; ///< 64 zero hex for the first record in a chain
};

struct FrameHashVerifyResult {
  bool ok = false;
  int first_fail_frame = -1;  ///< frame_index of first mismatch; -1 if N/A
  std::string message;
};

/// Tick-paced recorder. Preferred entry: onFrameAtTick. Plain onFrame uses the
/// tick last set by setCurrentTick (so a hub that only has IFrameSink can still
/// drive it if the host updates the tick each grab).
class cFrameHashRecorder final : public IFrameSink {
 public:
  static constexpr const char* kZeroRecordHash =
      "0000000000000000000000000000000000000000000000000000000000000000";

  explicit cFrameHashRecorder(FrameHashHeader header);
  ~cFrameHashRecorder() override;

  void setCurrentTick(uint64_t tick);
  /// Optional per-frame sim state hash. Pass "n/a" when the engine does not
  /// expose one at grab time (current workbench: only demo trailer finalWorldHash).
  void setSimStateHash(std::string hex_or_na);

  /// Preferred: one presented RGB24 frame grabbed at sim tick `tick`.
  void onFrameAtTick(const std::vector<uint8_t>& rgb, int width, int height, uint64_t tick);

  void onGrabTick(uint64_t tick) override;
  /// When set, onFrame also writes frame_NNNN.rgb and close() writes hashes.txt here.
  void setOutputDir(std::filesystem::path dir);
  /// When true (hub/present captures), tick= in the chain is frame ordinal; sim_tick keeps worldTick.
  void setOrdinalTicks(bool on);
  void setSkipFrames(int n);
  void setMaxFrames(int n);  // 0 = unlimited
  /// When false, allow multiple presents per sim tick (frozen still capture).
  void setDedupSimTicks(bool on);
  /// Ignore presents until onGrabTick has seen worldTick >= n (0 = off).
  void setMinSimTick(uint64_t n);
  /// When true, only record when last_sim_tick_ == last_recorded_sim_tick_ + 1
  /// (first accepted tick after min_sim_tick_ may be any). Skips jumps/gaps.
  void setRequireContiguousSimTicks(bool on);

  void onFrame(const std::vector<uint8_t>& rgb, int width, int height) override;
  void close() override;

  int frameCount() const;
  /// Rolling chain final value H_n = SHA-256(H_{n-1} || line_n).
  std::string recordHash() const;
  /// Writes hashes.txt under outDir. False on I/O error.
  bool writeManifest(const std::filesystem::path& outDir) const;
  /// Compare two manifests under the Data rules. On failure, message names the
  /// first affected frame_index when a frame line differs; frame_count mismatch
  /// is reported without a frame index.
  static FrameHashVerifyResult verify(const std::filesystem::path& expected,
                                      const std::filesystem::path& actual);

  static bool implemented();

 private:
  struct FrameEntry {
    int index = 0;
    uint64_t tick = 0;
    uint64_t fnv1a64 = 0;
    std::string sha256_hex;
    std::string sim_state_hash;
    std::string line;  ///< canonical frame line (no newline); feeds the chain
  };

  std::string makeFrameLine(const FrameEntry& e) const;
  void recomputeRecordHash();

  FrameHashHeader header_;
  uint64_t current_tick_ = 0;
  std::string current_sim_hash_ = "n/a";
  bool closed_ = false;
  std::vector<FrameEntry> frames_;
  std::string record_hash_;
  std::string wall_clock_start_;  ///< informational only; excluded from record_hash
  std::filesystem::path output_dir_;
  bool ordinal_ticks_ = false;
  uint64_t last_sim_tick_ = 0;
  uint64_t last_recorded_sim_tick_ = ~0ull;
  int skip_frames_ = 0;
  int max_frames_ = 0;
  bool dedup_sim_ticks_ = true;
  uint64_t min_sim_tick_ = 0;
  bool require_contiguous_sim_ticks_ = false;
  int presents_seen_ = 0;
};

}  // namespace lithokraft