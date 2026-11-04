// World snapshot: see cWorldSnapshot.h. 2026-10-31 (JD: Save World).
#include "lithokraft/game/cWorldSnapshot.h"
#include <chrono>

namespace lithokraft {

std::unique_ptr<cWorldSnapshot> cWorldSnapshot::capture(const std::string& dim, const ChunkMap& world,
                                                        const cSignalSim& signal,
                                                        const std::unordered_set<uint64_t>* dirty) {
  const auto t0 = std::chrono::steady_clock::now();
  auto s = std::make_unique<cWorldSnapshot>();
  s->dim_ = dim;
  s->state_ = std::make_shared<std::map<uint64_t, std::vector<uint8_t>>>(signal.exportState());
  auto copy = [&](const ChunkColumn& col) {
    if (col.edgeShell) return;  // Q12: a demoted column is clean; its truth is on disk / worldgen
    auto c = std::make_unique<ChunkColumn>();
    c->cx = col.cx; c->cz = col.cz;
    for (size_t i = 0; i < col.subs.size(); ++i)
      if (col.subs[i] && col.subs[i]->nonAir) { c->subs[i] = std::make_unique<SubChunk>(*col.subs[i]); s->bytes_ += sizeof(SubChunk); }
    c->materialPalette = col.materialPalette;  // PBR chunk materials ride along (interned handle: shared, not copied)
    c->cellText = col.cellText;  // 2026-10-25 Posts: panel text (PTXT payload)
    s->columns_.push_back(std::move(c));
  };
  if (dirty) {
    std::unordered_set<uint64_t> keys(dirty->begin(), dirty->end());
    for (const auto& [k, b] : *s->state_) keys.insert(k);
    for (uint64_t k : keys)
      if (auto it = world.columns().find(k); it != world.columns().end()) copy(*it->second);
  } else {
    for (const auto& [k, col] : world.columns()) copy(*col);
  }
  s->captureMs_ = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  return s;
}

}  // namespace lithokraft
