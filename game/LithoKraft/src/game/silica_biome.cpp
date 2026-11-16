// Silica per-biome shade cache + scatter density (workstream 5). LF line endings.
#include "lithokraft/game/silica_biome.h"

#include <array>
#include <memory>
#include <unordered_map>
#include "lithokraft/game/cow_publish.h"
#include "lithokraft/game/wafer_biomes.h"
#include "litholattice/procgen/pipeline.h"
#include "litholattice/world/column_generator.h"

namespace lithokraft {
namespace {

int64_t chunkKey(int cx, int cz) {
  return (int64_t(uint32_t(cx)) << 32) | uint64_t(uint32_t(cz));
}

}  // namespace

struct SilicaShadeCache::Impl {
  std::shared_ptr<const litholattice::cColumnGen> gen;
  // Archie Phase 3c: COW map, atomic publish (mesher hot path, no mutex).
  // Published as a raw pointer (cow_publish.h): atomic<shared_ptr> hides a lock.
  // Lookup only, never iterated, so the unordered order cannot reach a hash.
  using Map = std::unordered_map<int64_t, SilicaShade>;
  CowPublished<Map> cache_{new Map()};
  // Cache bound: past this many chunks, a publish starts from an empty map
  // instead of copying the whole one (values are a pure function of the
  // chunk coords, so cleared entries just recompute on their next lookup).
  static constexpr size_t kMaxEntries = 4096;

  SilicaShade blend(int cx, int cz) {
    // The sanctioned sibling query: regenerate the chunk's column maps
    // (deterministic: same seed -> same weights) and average the per-column
    // weight vectors over the chunk. Cache miss only.
    litholattice::procgen::ColumnMaps maps;
    if (gen) gen->generate(cx, cz, &maps);
    float w[kWaferBiomeCount]{};
    const size_t n = maps.biome.size();
    if (n > 0) {
      const float inv = 1.f / float(n);
      for (size_t i = 0; i < n; ++i)
        for (int b = 0; b < kWaferBiomeCount; ++b)
          w[b] += biomeWeightAt(maps, i, WaferBiome(b)) * inv;
    }
    // No maps at all (n == 0): weights stay zero -> silicaShadeForWeights falls
    // back to the untinted default at density 0 (nothing scatters, nothing tints).
    return silicaShadeForWeights(w, kWaferBiomeCount);
  }
};

SilicaShadeCache::SilicaShadeCache(std::shared_ptr<const litholattice::cColumnGen> waferGen)
    : impl_(std::make_unique<Impl>()) {
  impl_->gen = std::move(waferGen);
}

SilicaShadeCache::~SilicaShadeCache() = default;

SilicaShade SilicaShadeCache::shade(int cx, int cz) {
  const int64_t k = chunkKey(cx, cz);
  SilicaShade hit{};
  const bool found = impl_->cache_.read([&](const Impl::Map& m) {
    const auto it = m.find(k);
    if (it == m.end()) return false;
    hit = it->second;
    return true;
  });
  if (found) return hit;
  // Miss: blend unpinned (slow), then publish a copy holding it. The value is a
  // pure function of (cx, cz), so a racing writer's entry for k is the same value.
  const SilicaShade fresh = impl_->blend(cx, cz);
  impl_->cache_.update([&](const Impl::Map& snap) {
    if (snap.count(k)) return static_cast<Impl::Map*>(nullptr);  // a racing writer got there first
    Impl::Map* next = snap.size() >= Impl::kMaxEntries ? new Impl::Map() : new Impl::Map(snap);
    next->emplace(k, fresh);
    return next;
  });
  return fresh;
}

float silicaScatterDensity(const litholattice::procgen::ColumnMaps& maps, size_t column) {
  float w[kWaferBiomeCount]{};
  for (int b = 0; b < kWaferBiomeCount; ++b) w[b] = biomeWeightAt(maps, column, WaferBiome(b));
  return silicaShadeForWeights(w, kWaferBiomeCount).density;
}

}  // namespace lithokraft
