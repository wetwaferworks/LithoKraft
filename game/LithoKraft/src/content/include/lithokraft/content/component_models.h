// 2026-11-04 (CloudAtlas components-v2): shared render meshes of the v2 component models,
// assets/models/components/<block>_<state>.lthm (32^3, front = -Z = north, the engine's local frame).
#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
#include "lithokraft/content/foliage.h"
namespace lithokraft {
/// Loaded once (magic static, thread-safe init), read-only afterwards: lock-free for the meshers.
class cComponentModels {
 public:
  static const cComponentModels& instance();
  /// Mesh for "<block>_<state>" (e.g. "memor_idle"), nullptr when the asset is missing.
  const foliage::Mesh* find(std::string_view name) const;
  /// Top (face 2) height in 1/16 of the mesh over local point (x, z) in 1/16, -1 when none.
  static int topAt(const foliage::Mesh& m, int x, int z);
  const std::string& report() const { return report_; }
 private:
  cComponentModels();
  std::unordered_map<std::string, foliage::Mesh> meshes_;
  std::string report_;
};
}  // namespace lithokraft
