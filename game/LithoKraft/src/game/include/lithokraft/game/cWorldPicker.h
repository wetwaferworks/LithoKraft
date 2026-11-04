// World picker (2026-10-30, JD): the launch menu's Play opens this dialog. Left column: every world in
// cWorldCatalog; right column: the selected world's spawns (world default + named build areas). The
// current world also offers "Continue here" (Play exactly as before, no teleport).
#pragma once
#include <functional>
#include <string>
#include <vector>
#include "litholattice/ui/dialog.h"
#include "litholattice/ui/ui_root.h"
#include "lithokraft/game/cWorldCatalog.h"

namespace lithokraft {

class cWorldPicker final {
 public:
  /// world = catalog index; spawn = "" (continue here), "spawn" (world default) or a named spawn.
  using PickFn = std::function<void(int world, const std::string& spawn)>;
  /// `onFolder` (2026-10-31, Load World): adds "Open world folder..." (the OS folder dialog).
  void open(litholattice::ui::cUiRoot& ui, float k, int currentWorld, std::vector<std::string> currentSpawns, PickFn onPick,
            std::function<void()> onFolder = {});
  bool isOpen() const { return dlg_ != nullptr; }

 private:
  void showSpawns(int world);
  litholattice::ui::cDialog* dlg_ = nullptr;
  litholattice::ui::cWidget* spawnCol_ = nullptr;
  int current_ = -1;
  float k_ = 1.f;
  std::vector<std::string> currentSpawns_;
  PickFn onPick_;
  std::function<void()> onFolder_;
};

}  // namespace lithokraft