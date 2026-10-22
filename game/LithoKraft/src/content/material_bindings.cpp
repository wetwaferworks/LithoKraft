// JD Vulkan push (Cortana 2026-10-16, Proposed by JD, Built by Cortana): see material_bindings.h.
#include "lithokraft/content/material_bindings.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "litholattice/core/json.h"

namespace lithokraft {

std::string materialBindingsDir() {
#ifdef LITHOKRAFT_MODELS_DIR
  return (std::filesystem::path(LITHOKRAFT_MODELS_DIR) / ".." / "lithokraft" / "materials" / "bindings").lexically_normal().string();
#else
  return "assets/lithokraft/materials/bindings";
#endif
}

bool loadModelBindings(const std::string& dir, const std::string& modelId, ModelBindings& out, std::string* err) {
  out = {};
  auto fail = [&](const std::string& m) { if (err) *err = m; out = {}; return false; };
  if (modelId.empty() || modelId.find_first_of("/\\") != std::string::npos || modelId.find("..") != std::string::npos)
    return fail("bad model id '" + modelId + "'");
  const std::filesystem::path p = std::filesystem::path(dir) / (modelId + ".json");
  std::ifstream in(p, std::ios::binary);
  if (!in) return fail("no bindings file " + p.string());
  std::stringstream ss;
  ss << in.rdbuf();
  litholattice::Json json;
  std::string perr;
  if (!litholattice::Json::parse(ss.str(), json, &perr)) return fail(p.string() + ": " + perr);
  out.model = json.getStr("model", modelId);
  const litholattice::Json* mats = json.get("materials");
  if (!mats || mats->type != litholattice::Json::Type::Object) return fail(p.string() + ": no \"materials\" object");
  for (const auto& [key, v] : mats->obj) {
    char* end = nullptr;
    const long id = std::strtol(key.c_str(), &end, 10);
    if (key.empty() || *end || id < 0 || id > 255 || v.type != litholattice::Json::Type::Object)
      return fail(p.string() + ": bad material key '" + key + "'");
    MaterialBinding b;
    b.material = int(id);
    b.texture = v.getStr("texture");
    b.shade = v.getStr("shade", "tex");
    b.glow = v.getBool("glow", b.shade == "emissive");
    if (b.texture.empty()) return fail(p.string() + ": material " + key + " has no texture");
    out.materials.push_back(std::move(b));
  }
  return true;
}

BoundModel loadBoundModel(const std::string& lthmPath, const std::string& bindingsDir) {
  BoundModel r;
  r.load = model::loadModel(lthmPath);
  if (r.load.error == model::LoadError::Ok)
    r.hasBindings = loadModelBindings(bindingsDir, std::filesystem::path(lthmPath).stem().string(), r.bindings);
  return r;
}

}  // namespace lithokraft
