// World catalog: see cWorldCatalog.h. 2026-10-30 (JD).
#include "lithokraft/game/cWorldCatalog.h"
#include "lithokraft/content/component_test_world.h"  // 2026-11-12 (JD): Component Test spawns

namespace lithokraft {

cWorldCatalog::cWorldCatalog() {
  const std::vector<std::string> examples{"spawn", "testarea", "stand", "screen16", "grove", "display", "framebuffer",
                                          "fb-wall", "wall-pixel", "pattern", "sawtooth", "redirector", "litho",
                                          "circuits", "wire-torture", "clocks", "pixel-display"};
  std::vector<std::string> bolt{"spawn", "lab"};
  for (int i = 1; i <= 10; ++i) bolt.push_back("b" + std::to_string(i));
  for (int i = 1; i <= 9; ++i) bolt.push_back("a" + std::to_string(i));
  bolt.push_back("r0"); bolt.push_back("hud");
  entries_ = {
      {"default", "Default world", {}, {"spawn"}},
      {"testworld", "Test World", {"--test-world"}, {"spawn", "testarea", "grove"}},
      {"testworld-examples", "Test World + Examples", {"--test-world-examples"}, examples},
      {"wire-torture", "Wire Torture", {"--bench-scene", "wire-torture"}, {"spawn"}},
      {"flatlab", "Flat Lab (Trace)", {"--flat-lab"}, examples},
      {"boltlab", "Bolt PCHIE Lab", {"--bolt-lab"}, bolt},
      // 2026-11-12 (JD t5296u): "call it component test, and put it in the menu". One spawn per pad.
      {"component-test", "Component Test", {"--component-test"}, cComponentTestWorld::spawnIds()},
  };
}

const cWorldCatalog& cWorldCatalog::instance() {
  static const cWorldCatalog c;
  return c;
}

int cWorldCatalog::indexFor(const std::string& benchScene, const std::string& dimension, bool examples) const {
  std::string id;
  if (benchScene == "boltlab") id = "boltlab";
  else if (benchScene == "componenttest") id = "component-test";
  else if (benchScene == "testworld" && dimension == "flatlab") id = "flatlab";
  else if (benchScene == "testworld") id = examples ? "testworld-examples" : "testworld";
  else if (benchScene == "wire-torture") id = "wire-torture";
  else if (benchScene == "default" && dimension == "wafer") id = "default";
  for (size_t i = 0; i < entries_.size(); ++i)
    if (entries_[i].id == id) return int(i);
  return -1;
}

}  // namespace lithokraft