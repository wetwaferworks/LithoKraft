// 2026-11-04 (CloudAtlas components-v2): see lithokraft/content/component_models.h.
#include "lithokraft/content/component_models.h"
#include <algorithm>
#include <vector>

#ifndef LITHOKRAFT_MODELS_DIR
#define LITHOKRAFT_MODELS_DIR ""
#endif

namespace lithokraft {
namespace {
constexpr const char* kModels[] = {
    "breakor_closed", "breakor_tripped", "fusor_sum", "memor_idle", "thresholdor_compare", "thresholdor_subtract",
    "multiplexor_sel_red", "multiplexor_sel_blue", "redirector_straight", "redirector_left", "redirector_right",
    "pulsor_idle", "pulsor_firing", "latchor_off", "latchor_on", "randor_idle", "randor_rolling",
    "catchor_idle", "catchor_firing", "keyboard_idle", "switch_off", "switch_on", "button_up", "button_pressed",
    "routor_mode1", "routor_mode2", "routor_mode3", "dividor",
    "dialor_v01", "dialor_v32", "dialor_v64", "watchor", "bitor_and", "bitor_or", "bitor_xor", "bitor_nor", "pushor_retracted", "swivelor_d00", "swivelor_d16", "swivelor_d32", "swivelor_d64",
    "via_single", "via_top", "via_bottom", "via_mid", "via_elbow_single", "via_elbow_upjoint", "via_elbow_sidejoint", "via_elbow_mid"};  // B9 2026-11-12: CloudAtlas Via B lattice cage + elbows  // B9 2026-11-12: CloudAtlas Swivelor row B detent stills
    //  // B9 2026-11-12: CloudAtlas Pushor row B retracted (extended stills illustrative, not installed)
    //  // C9 2026-11-12: CloudAtlas Bitor row B AND / OR / XOR / NOR stills (JD 6:39 AM ET ranges)  // B9 2026-11-12: CloudAtlas Watchor row B (eye cube)  // 2026-11-11: CloudAtlas Dialor concept C stills  // 2026-11-05: CloudAtlas Dividor  // PlayB: CloudAtlas Routor
}  // namespace

const cComponentModels& cComponentModels::instance() {
  static const cComponentModels all;
  return all;
}

cComponentModels::cComponentModels() {
  const std::string root(LITHOKRAFT_MODELS_DIR);
  if (root.empty()) { report_ = "no models dir"; return; }
  for (const char* n : kModels) {
    model::LoadResult r = model::loadModel(root + "/components/" + n + ".lthm");
    if (r.error != model::LoadError::Ok || r.model.geometry.empty()) {
      report_ += std::string(n) + ".lthm: " + (r.error == model::LoadError::Ok ? std::string("empty geometry") : r.message) + "; ";
      continue;
    }
    std::vector<model::Model> blocks;
    blocks.push_back(std::move(r.model));
    // 2026-11-05 (JD E2 Switch off lever): components-v2 .lthm are level 5 (32^3). foliage::meshModels
    // default LOD 4 majority-downsamples 2x2x2 and erases thin levers (switch_off had 0 lever cells at
    // L16; switch_on kept only the tip). Mesh at the model's maxLevel (at least 4).
    // 2026-11-05 (JD, H2): back to LOD 4 like E2 -- the vertex format stores 1/16-block positions, so LOD 5 broke every v2 model.
    foliage::Mesh m = foliage::meshModels(blocks, 4);
    m.fromAsset = true;
    if (!m.quads.empty()) meshes_.emplace(n, std::move(m));
  }
}

const foliage::Mesh* cComponentModels::find(std::string_view name) const {
  const auto it = meshes_.find(std::string(name));
  return it == meshes_.end() ? nullptr : &it->second;
}

int cComponentModels::topAt(const foliage::Mesh& m, int x, int z) {
  int y = -1;
  for (const foliage::Quad& q : m.quads)
    if (q.face == 2 && x >= q.lo[0] && x < q.hi[0] && z >= q.lo[2] && z < q.hi[2]) y = std::max(y, int(q.hi[1]));
  return y;
}
}  // namespace lithokraft
