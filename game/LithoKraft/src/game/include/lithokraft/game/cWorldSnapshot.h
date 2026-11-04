// World snapshot (2026-10-31, JD: Save World). The copy a save writes: the SGNL export of the
// signal sim plus deep copies of the columns to save. Captured ONCE, wherever the caller owns the
// world for a moment (the tick thread at a tick boundary for the async save, the stopped-sim
// window for the synchronous exit save); after that it belongs to whoever commits it, so the
// encode + write never touches live world memory and nobody waits on it.
#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>
#include "lithokraft/content/signal.h"
#include "lithokraft/game/engine.h"
#include "litholattice/world/chunk_map.h"

namespace lithokraft {

class cWorldSnapshot final {
 public:
  /// Copies the columns in `dirty` plus every column holding signal state (nullptr = every
  /// column) and exports the signal memory. Read-only on `world` / `signal`.
  static std::unique_ptr<cWorldSnapshot> capture(const std::string& dim, const ChunkMap& world,
                                                 const cSignalSim& signal, const std::unordered_set<uint64_t>* dirty);
  const std::string& dimension() const { return dim_; }
  size_t columnCount() const { return columns_.size(); }
  size_t bytesCopied() const { return bytes_; }      ///< sub-chunk voxel bytes copied (memory cost)
  double captureMs() const { return captureMs_; }    ///< time the capture held the world
  /// Hands the payload to the committer (once).
  std::shared_ptr<std::map<uint64_t, std::vector<uint8_t>>> takeSignalState() { return std::move(state_); }
  std::vector<std::unique_ptr<ChunkColumn>> takeColumns() { return std::move(columns_); }

 private:
  std::string dim_;
  std::shared_ptr<std::map<uint64_t, std::vector<uint8_t>>> state_;
  std::vector<std::unique_ptr<ChunkColumn>> columns_;
  size_t bytes_ = 0;
  double captureMs_ = 0;
};

}  // namespace lithokraft
