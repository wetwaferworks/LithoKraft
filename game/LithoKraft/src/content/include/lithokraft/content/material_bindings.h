// JD Vulkan push (Cortana 2026-10-16, Proposed by JD, Built by Cortana): material bindings for
// .lthm models. A .lthm leaf stores only a material id; bindings/<model-id>.json maps each id
// to a pack texture and a shade ("emissive", "alpha_blend", "tex", ...). Read when the model
// is loaded (loadBoundModel). CloudAtlas sets: bulb glass rims 40 / 42 (alpha-blended, sorted),
// bulb cores 41 / 43 (glow), Trace 5 / 6 (glow); etched blocks 50 (base) / 51 (line).
#pragma once
#include <string>
#include <vector>
#include "model.h"

namespace lithokraft {

struct MaterialBinding {
  int material = 0;
  std::string texture;  ///< pack texture name (resources.json "textures")
  std::string shade;    ///< "tex" | "emissive" | "alpha_blend" | "cutout"
  bool glow = false;    ///< feeds the Vulkan bloom
};

struct ModelBindings {
  std::string model;  ///< model id (file stem)
  std::vector<MaterialBinding> materials;
  const MaterialBinding* find(int material) const {
    for (const MaterialBinding& b : materials) if (b.material == material) return &b;
    return nullptr;
  }
};

/// Default bindings folder: assets/lithokraft/materials/bindings.
std::string materialBindingsDir();
/// Reads `<dir>/<modelId>.json`. False (+ *err) if missing or malformed; `out` is then empty.
bool loadModelBindings(const std::string& dir, const std::string& modelId, ModelBindings& out, std::string* err = nullptr);

struct BoundModel {
  model::LoadResult load;
  ModelBindings bindings;
  bool hasBindings = false;
};
/// Loads a .lthm and its bindings (by the file stem) in one step - the material loader hook.
BoundModel loadBoundModel(const std::string& lthmPath, const std::string& bindingsDir = materialBindingsDir());

}  // namespace lithokraft
