// ModelResolver over the baked Litho Formula gallery (.lthm files).
//
// Reuses the loadModelCells discipline from chipor_viewport.cpp: names are file
// stems only (sanitised), loads go through LITHOKRAFT_MODELS_DIR, and a
// mutex-guarded cache keeps each model to a single load. Octree leaves are
// expanded to unit cells at 2^maxLevel so the worldgen stamper works on plain
// voxels; the gallery bakes at level 5, i.e. a 32-cell grid.

#include "litho_stamp_models.h"

#include <algorithm>
#include <atomic>
#include <map>
#include <mutex>
#include <set>
#include <string>

#include "litholattice/core/log.h"
#include "model.h"

namespace lithokraft::model {
namespace {

// --stub-models toggle (set by main.cpp before Game construction).
std::atomic<bool> g_stubEnabled{false};

// The stub: the smallest useful stampable model — a flat 8x8 plate one
// cell high. Palette 1 is mapped by both shipped lithoform_stamp
// features (wafer: dead_silicon; organic: dirt), so the loader's
// unmapped-palette skip can never empty it out.
litholattice::procgen::LithoStampModel stubModel() {
  litholattice::procgen::LithoStampModel m;
  m.size = 8;
  for (int z = 0; z < 8; ++z)
    for (int x = 0; x < 8; ++x)
      m.cells.push_back(litholattice::procgen::LithoStampCell{x, 0, z, 1});
  return m;
}

bool saneName(std::string_view name) {
  const std::string key(name);
  return !key.empty() && key.size() <= 128 && key.find('/') == std::string::npos &&
         key.find('\\') == std::string::npos && key.find("..") == std::string::npos;
}

std::optional<litholattice::procgen::LithoStampModel> loadOnce(std::string_view name) {
  if (!saneName(name)) return std::nullopt;
#ifdef LITHOKRAFT_MODELS_DIR
  const std::string key(name);
  const auto loaded =
      loadModel(std::string(LITHOKRAFT_MODELS_DIR) + "/" + key + ".lthm");
  if (loaded.error != LoadError::Ok) return std::nullopt;
  const auto leaves = loaded.model.geometry.leaves();
  int maxLevel = 0;
  for (const auto& c : leaves) maxLevel = std::max(maxLevel, int(c.level));
  if (maxLevel <= 0 || maxLevel > 8) return std::nullopt;
  litholattice::procgen::LithoStampModel m;
  m.size = 1 << maxLevel;
  for (const auto& c : leaves) {
    if (c.material == 0) continue;
    const int s = 1 << (maxLevel - int(c.level));
    const int x0 = int(c.x) * s, y0 = int(c.y) * s, z0 = int(c.z) * s;
    for (int y = y0; y < y0 + s; ++y)
      for (int z = z0; z < z0 + s; ++z)
        for (int x = x0; x < x0 + s; ++x)
          m.cells.push_back(litholattice::procgen::LithoStampCell{x, y, z, c.material});
  }
  if (m.cells.empty()) return std::nullopt;
  return m;
#else
  (void)name;
  return std::nullopt;
#endif
}

}  // namespace

void setLithoStampModelStubEnabled(bool on) {
  g_stubEnabled.store(on, std::memory_order_release);
  if (on)
    litholattice::logInfo("models: --stub-models ON — unresolved Litho Formula presets resolve to a flat 8x8 stub wafer (bench only)");
}

bool lithoStampModelStubEnabled() { return g_stubEnabled.load(std::memory_order_acquire); }

std::optional<litholattice::procgen::LithoStampModel> loadLithoStampModel(std::string_view name) {
  static std::mutex mutex;
  static std::map<std::string, std::optional<litholattice::procgen::LithoStampModel>, std::less<>>
      cache;
  static std::set<std::string> stubLogged;  // names whose stub substitution was logged (under mutex)
  const std::string key(name);
  {
    std::lock_guard<std::mutex> lock(mutex);
    const auto it = cache.find(key);
    if (it != cache.end()) return it->second;
  }
  auto m = loadOnce(name);
  const bool stubbed = !m && lithoStampModelStubEnabled();
  if (stubbed) m = stubModel();
  {
    std::lock_guard<std::mutex> lock(mutex);
    cache[key] = m;
    // Log inside the lock, once per name: the cached stub means this
    // branch runs only on the name's first resolution anyway, and the
    // set keeps it to one line even if two threads raced the first load.
    if (stubbed && stubLogged.insert(key).second)
      litholattice::logWarn("models: --stub-models: unresolved preset '%s' — flat 8x8 stub wafer substituted "
              "(bench only; the real registry reconciles with the workstation tree)",
              key.c_str());
  }
  return m;
}

litholattice::procgen::ModelResolver makeLithoStampModelResolver() {
  return [](std::string_view name) { return loadLithoStampModel(name); };
}

}  // namespace lithokraft::model
