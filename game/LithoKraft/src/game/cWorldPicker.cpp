// World picker: see cWorldPicker.h. 2026-10-30 (JD).
#include "lithokraft/game/cWorldPicker.h"

namespace lithokraft {

// namespace ui = lithokraft::ui;  // MSVC C2386: redundant self-alias inside namespace lithokraft (ui:: already resolves here)

void cWorldPicker::open(ui::cUiRoot& root, float k, int currentWorld, std::vector<std::string> currentSpawns, PickFn onPick,
                        std::function<void()> onFolder) {
  if (dlg_) return;
  // Validate the caller's world index against the catalog (bug hunt
  // #11): an out-of-range current_ made showSpawns read past the end of
  // entries(). Fall back to 0; an empty catalog shows no spawns at all.
  const size_t catSize = cWorldCatalog::instance().entries().size();
  if (currentWorld < 0 || size_t(currentWorld) >= catSize) currentWorld = 0;
  k_ = k; current_ = currentWorld; currentSpawns_ = std::move(currentSpawns); onPick_ = std::move(onPick);
  onFolder_ = std::move(onFolder);
  ui::cDialog& dlg = ui::cDialog::show(root, "Choose a world");
  dlg.setId("world-picker");
  auto& row = dlg.body().add<ui::cStackPanel>(ui::Orientation::Horizontal, 16 * k_);
  auto& worlds = row.add<ui::cStackPanel>(ui::Orientation::Vertical, 6 * k_);
  worlds.setId("world-list");
  const auto& cat = cWorldCatalog::instance().entries();
  for (size_t i = 0; i < cat.size(); ++i) {
    const std::string title = cat[i].title + (int(i) == current_ ? "  (loaded)" : "");
    auto& b = worlds.add<ui::cButton>(title);
    b.setId("world-" + cat[i].id);
    b.setLayout(ui::Layout::sized({300 * k_, 40 * k_}));
    b.on("click", [this, i](ui::cWidget&) { showSpawns(int(i)); });
  }
  if (onFolder_) {  // 2026-10-31 (JD, Load World): any world folder on disk
    auto& f = worlds.add<ui::cButton>("Open world folder...");
    f.setId("world-folder");
    f.setLayout(ui::Layout::sized({300 * k_, 40 * k_}));
    f.on("click", [this](ui::cWidget&) {
      const std::function<void()> fn = onFolder_;
      if (dlg_) dlg_->close();
      if (fn) fn();
    });
  }
  auto& scroll = row.add<ui::cScrollView>();
  scroll.setId("world-spawns");
  scroll.setLayout(ui::Layout::sized({300 * k_, 6 * 46 * k_}));
  spawnCol_ = &scroll.emplaceContent<ui::cStackPanel>(ui::Orientation::Vertical, 6 * k_);
  dlg.addButton("Back").setId("world-picker-back");
  dlg.on("close", [this](ui::cWidget&) { dlg_ = nullptr; spawnCol_ = nullptr; });
  dlg_ = &dlg;
  showSpawns(current_ >= 0 ? current_ : 0);
}

void cWorldPicker::showSpawns(int world) {
  if (!spawnCol_) return;
  spawnCol_->clearChildren();
  // Validate here too (bug hunt #11): button handlers pass catalog
  // indices, but showSpawns is also reachable with current_ and any
  // future caller; an empty catalog has nothing to show.
  const auto& entries = cWorldCatalog::instance().entries();
  if (entries.empty()) return;
  if (world < 0 || size_t(world) >= entries.size()) world = 0;
  const auto& e = entries[size_t(world)];
  std::vector<std::pair<std::string, std::string>> items;  // (spawn key, button text)
  if (world == current_) {
    items.push_back({"", "Continue here"});
    for (const std::string& s : currentSpawns_) items.push_back({s, s == "spawn" ? "World spawn" : s});
  } else {
    for (const std::string& s : e.spawns) items.push_back({s, s == "spawn" ? "World spawn" : s});
  }
  spawnCol_->add<ui::cLabel>(e.title).setCentered(true);
  for (const auto& [key, text] : items) {
    auto& b = spawnCol_->add<ui::cButton>(text);
    b.setId("spawn-" + (key.empty() ? std::string("continue") : key));
    b.setLayout(ui::Layout::sized({280 * k_, 40 * k_}));
    b.on("click", [this, world, key](ui::cWidget&) {
      const PickFn pick = onPick_;
      if (dlg_) dlg_->close();
      if (pick) pick(world, key);
    });
  }
}

}  // namespace lithokraft