// World config builder: see cWorldConfigBuilder.h. 2026-10-31 (JD: Load World).
#include "lithokraft/game/cWorldConfigBuilder.h"
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include "lithokraft/game/world_save.h"

namespace lithokraft {

bool cWorldConfigBuilder::inProcess() {
  const char* e = std::getenv("LL_WORLD_RELAUNCH");
  if (e && std::strcmp(e, "1") == 0) return false;
  // 2026-10-31 (JD, world select bounce): under threeway (LL_THREEWAY=1, on by default with
  // LL_THREAD_LAYOUT=8) the in-process Load World request is never served by main(), so every
  // non-current world fell back to the menu. Threeway runs take the relaunch path instead.
  // LL_WORLD_INPROCESS_THREEWAY=1 is the kill switch back to the old behaviour.
  const char* t = std::getenv("LL_THREEWAY");
  const char* k = std::getenv("LL_WORLD_INPROCESS_THREEWAY");
  if (t && t[0] == '1' && !(k && k[0] == '1')) return false;
  return true;
}

bool cWorldConfigBuilder::looksLikeWorld(const std::string& dir) {
  std::error_code ec;
  const std::filesystem::path p(dir);
  return std::filesystem::exists(p / "level.json", ec) || std::filesystem::exists(p / "player.lat", ec) ||
         std::filesystem::exists(p / "blocks.names", ec);
}

GameConfig cWorldConfigBuilder::build(const Launch& l, const std::vector<std::string>& args, const std::string& folder,
                                      std::string* note) {
  GameConfig c = l.base;
  c.benchScene = "default";
  c.dimension = "wafer";
  c.testWorldExamples = false;
  c.startSpawn.clear();
  c.saveDir.clear();
  c.worldFromFolder = !folder.empty();  // 2026-10-31 soak fix: "default" from a folder world must load
  std::string saveDir = folder.empty() ? l.explicitSaveDir : folder;
  bool noSave = folder.empty() ? l.noSave : false;
  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& a = args[i];
    const bool more = i + 1 < args.size();
    if (a == "--test-world") c.benchScene = "testworld";
    else if (a == "--test-world-examples") { c.benchScene = "testworld"; c.testWorldExamples = true; }
    else if (a == "--flat-lab") { c.benchScene = "testworld"; c.testWorldExamples = true; c.dimension = "flatlab"; }
    else if (a == "--bolt-lab") { c.benchScene = "boltlab"; c.dimension = "flatlab"; }
    else if (a == "--bench-scene" && more) c.benchScene = args[++i];
    else if (a == "--spawn" && more) c.startSpawn = args[++i];
    else if (a == "--renderer" && more) ++i;  // the running renderer stays
  }
  if (c.startSpawn == "spawn") c.startSpawn.clear();
  // 2026-10-31 soak fix: the launch --save-dir belongs to the LAUNCH world. Switching to another
  // catalog world must not write that world's columns into it (testworld over JD's world).
  if (folder.empty() && (c.benchScene != l.base.benchScene || c.dimension != l.base.dimension)) saveDir.clear();
  const std::filesystem::path root(l.dataRoot);
  if (l.playtestSpawn && folder.empty() && c.benchScene == "default") {
    c.benchScene = "testworld";
    if (saveDir.empty() && !noSave) saveDir = (root / "saves" / "playtest").lexically_normal().string();
  }
  if (c.benchScene == "boltlab" && saveDir.empty()) noSave = true;
  if (c.benchScene == "testworld") {
    if (l.playtestSpawn) c.testWorldExamples = true;
    c.sceneSize = l.romScreen >= 64 ? 64 : 32;
    // 2026-11-01 (JD): persists like a launch (see main.cpp): saved columns load over the fresh build.
    const char* fresh = std::getenv("LL_TESTWORLD_FRESH");
    if (saveDir.empty() && !noSave && !(fresh && fresh[0] == '1')) {
      const char* id = c.dimension == "flatlab" ? "flatlab" : (c.testWorldExamples ? "testworld-examples" : "testworld");
      saveDir = (root / "saves" / id).lexically_normal().string();
    }
    if (saveDir.empty()) noSave = true;
  }
  if (!noSave) c.saveDir = saveDir.empty() ? (root / "saves" / "world").lexically_normal().string() : saveDir;
  std::string n;
  if (!c.saveDir.empty()) {
    std::string err;
    if (const auto seed = cWorldSave::readWorldSeed(c.saveDir, &err)) c.seed = *seed;
    else if (!err.empty()) n += "level.json unreadable (" + err + "), launch seed kept; ";
    std::string aerr;
    if (const auto wa = cWorldSave::readWorldArea(c.saveDir, &aerr)) {
      c.diamondSimArea = wa->sim == litholattice::AreaShape::Diamond;
      c.roundSimArea = wa->sim == litholattice::AreaShape::Round;
      c.diamondStreamArea = wa->stream == litholattice::AreaShape::Diamond;
      c.tickRadius = wa->tickRadius;
    }
    if (!aerr.empty()) n += "world area: " + aerr + "; ";
    n += looksLikeWorld(c.saveDir) ? "saved world '" + c.saveDir + "'" : "new world in '" + c.saveDir + "'";
  } else {
    n += "fresh build, not saved";
  }
  if (note) *note = n;
  return c;
}

}  // namespace lithokraft
