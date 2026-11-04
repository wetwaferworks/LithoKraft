// World config builder (2026-10-31, JD: Load World). Turns a world choice - a cWorldCatalog
// entry's flags or a world folder picked from the filesystem - into the GameConfig a fresh Game
// is built from, IN-PROCESS (no relaunch, no dimension switch). It applies the same rules the
// launch path in main.cpp applies to those flags: test world / bolt lab rebuild fresh (no save),
// the default world saves to <data root>/saves/world (playtest builds: saves/playtest), and a
// saved world's level.json seed and sim/stream area win over the launch configuration.
// LL_WORLD_RELAUNCH=1 restores the old relaunch-the-exe picker.
#pragma once
#include <string>
#include <vector>
#include "lithokraft/game/game.h"

namespace lithokraft {

class cWorldConfigBuilder final {
 public:
  struct Launch {
    GameConfig base;              ///< the launch config BEFORE any saved-world override (seed, area)
    std::string explicitSaveDir;  ///< --save-dir as given at launch ("" = none)
    bool noSave = false;          ///< --no-save at launch
    bool playtestSpawn = false;   ///< LK_PLAYTEST_SPAWN build
    int romScreen = 32;           ///< --rom-screen at launch
    std::string dataRoot;         ///< <data root> (saves/, logs/, config/)
  };
  static bool inProcess();  ///< LL_WORLD_RELAUNCH != "1" (default: in-process)
  /// `args`: catalog flags (+ --spawn); `folder`: a world folder ("" = use the args).
  static GameConfig build(const Launch& l, const std::vector<std::string>& args, const std::string& folder,
                          std::string* note);
  /// True when `dir` looks like a LithoKraft world (level.json, player.lat or blocks.names).
  static bool looksLikeWorld(const std::string& dir);
};

}  // namespace lithokraft
