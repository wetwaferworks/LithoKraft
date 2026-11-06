// Silica per-biome shade cache + scatter density (workstream 5). LF line endings.
#include "lithokraft/game/silica_biome.h"

#include <array>
#include <atomic>
#include <memory>
#include <unordered_map>
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
  using Map = std::unordered_map<int64_t, SilicaShade>;
  std::atomic<std::shared_ptr<const Map>> cache_{std::make_shared<const Map>()};

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
  for (;;) {
    auto snap = std::atomic_load_explicit(&impl_->cache_, std::memory_order_acquire);
    if (const auto it = snap->find(k); it != snap->end()) return it->second;
    const SilicaShade fresh = impl_->blend(cx, cz);
    auto next = std::make_shared<Impl::Map>(*snap);
    const auto [it, inserted] = next->emplace(k, fresh);
    (void)inserted;
    std::shared_ptr<const Impl::Map> expected = snap;
    if (std::atomic_compare_exchange_weak_explicit(
            &impl_->cache_, &expected, std::shared_ptr<const Impl::Map>(std::move(next)),
            std::memory_order_release, std::memory_order_acquire))
      return it->second;
  }
}

float silicaScatterDensity(const litholattice::procgen::ColumnMaps& maps, size_t column) {
  float w[kWaferBiomeCount]{};
  for (int b = 0; b < kWaferBiomeCount; ++b) w[b] = biomeWeightAt(maps, column, WaferBiome(b));
  return silicaShadeForWeights(w, kWaferBiomeCount).density;
}

}  // namespace lithokraft
