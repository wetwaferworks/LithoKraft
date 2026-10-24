// The game's screens on the retained UI: HUD (debug panel, controls panel, keyboard-capture
// banner), the hotbar and the block palette (a modal dialog). The game hands in a
// HudModel each frame and reacts to callbacks; GameUi owns the widgets and the UiRoot.
#pragma once
#include "lithokraft/game/engine.h"
#include <array>
#include <functional>
#include <algorithm>
#include <string>
#include <vector>
#include "litholattice/render/block_materials.h"
#include "litholattice/render/renderer.h"
#include "litholattice/ui/controls.h"
#include "litholattice/ui/dialog.h"
#include "litholattice/ui/ui_root.h"
#include "litholattice/world/block.h"
#include "lithokraft/game/inventory.h"  // HudModel hotbar = the inventory's real ItemStacks (P2b-1)
#include "lithokraft/game/settings.h"

namespace lithokraft {

constexpr int kHotbarSlots = 8;

/// What the HUD shows this frame. `hotbar` is a snapshot of the player
/// inventory's hotbar slots (P2b-1): stacks, not bare BlockIds — a tool or
/// the Fragment in a slot renders with its own item icon and name.
struct HudModel {
  std::array<ItemStack, kHotbarSlots> hotbar{};
  int selected = 0;
  bool visible = true;                     ///< F1 hides everything
  std::vector<std::string> debugLines;     ///< empty: no debug panel (F3)
  std::vector<std::string> controlLines;   ///< controls panel (H)
  std::vector<std::string> bannerLines;    ///< keyboard capture banner (empty: none)
  std::string lookName;                    ///< 2026-10-16 (JD): looked-at block name (empty: hidden)
};

class GameUi {
 public:
  struct Callbacks {
    std::function<void(int slot)> selectSlot;              ///< hotbar slot clicked
    std::function<void(BlockId, bool shift)> pickBlock;    ///< inventory cell clicked
    std::function<void()> inventoryClosed;                 ///< the dialog closed itself (Esc / E)
    std::function<void()> pauseClosed;                     ///< pause menu closed (Resume / Esc)
    std::function<void()> quit;                            ///< Quit to desktop
    std::function<void()> play;                            ///< menu Play button: enter creative/testing
    std::function<void()> exitToMenu;                      ///< pause menu: back to the launch menu
    std::function<void()> openSettings;                    ///< menu / pause: open the settings screen
    std::function<void(const GameSettings&)> settingsChanged;  ///< a settings control changed
    /// (Last, so positional initializers stay valid.) Playtest 2026-10-16: carry slot (0..41, Inventory::allSlots order) clicked in the
    /// inventory dialog's "Carry" grid: swap it into the selected hotbar slot.
    std::function<void(int carryIndex)> pickCarry;
  };

  explicit GameUi(Callbacks callbacks);
  ui::UiRoot& root() { return ui_; }
  const ui::UiRoot& root() const { return ui_; }

  /// Re-applies a theme (after a resource reload); the HUD scale stays.
  void applyTheme(const ui::Theme& theme);
  void openInventory(int selectedSlot);
  /// Playtest 2026-10-16: the same dialog plus a "Carry (42)" grid showing every carry slot
  /// (hotbar 1-8, offhand L/R, main 1-32); a click swaps that slot into the selected hotbar slot.
  void openInventory(int selectedSlot, const std::vector<ItemStack>& carry);
  /// Refresh the open dialog's carry grid (after a swap). No-op when closed / no grid.
  void refreshCarry(const std::vector<ItemStack>& carry);
  void closeInventory();
  bool inventoryOpen() const { return inventory_ != nullptr; }
  void openPause();
  void closePause();
  bool pauseOpen() const { return pause_ != nullptr; }
  /// The launch menu overlay (title, tagline, divider, Play / Settings / Exit, version
  /// footer) over the cinematic wafer backdrop. `version` is shown in the footer.
  void openMenu(const std::string& version);
  void closeMenu();
  bool menuOpen() const { return menu_ != nullptr; }
  /// The settings screen (a modal over the menu or the pause dialog).
  void openSettings(const GameSettings& current);
  void closeSettings();
  bool settingsOpen() const { return settings_ != nullptr; }
  /// 0 = auto (from the framebuffer size), else a fixed GUI scale 1-4.
  void setHudScalePref(int pref) { hudScalePref_ = pref < 0 ? 0 : pref > kMaxGuiScale ? kMaxGuiScale : pref; }
  /// Cortana 2026-10-16 [GAME] (JD GUI overhaul): the whole-number GUI scale used for a framebuffer of
  /// w x h pixels (physical pixels, so it is DPI-aware). Auto (pref 0) = the largest scale whose
  /// 960 x 540 reference canvas fits (720p -> 1, 1080p -> 2, 1440p -> 2, 4K -> 4). A fixed pref
  /// (1-4, like Minecraft's GUI scale) is capped to what fits a 640 x 360 safe canvas (the 600-unit menu column then always fits), so a large
  /// scale in a small window never pushes the menus off-screen.
  static constexpr int kMaxGuiScale = 4;
  /// Cortana 2026-10-16 (JD playtest): inventory screen, its slots/icons and the HUD hotbar draw at
  /// this multiple of the GUI scale (1.25 = +25% for readability). Renderer-agnostic: every backend
  /// paints the same UI rects.
  static constexpr float kInventoryScale = 1.25f;
  static int effectiveGuiScale(int pref, float w, float h) {
    auto fit = [w, h](float rw, float rh) { const int a = int(w / rw), b = int(h / rh); return a < b ? a : b; };
    const int autoK = std::clamp(fit(960.f, 540.f), 1, kMaxGuiScale);
    const int maxK = std::clamp(fit(640.f, 360.f), 1, kMaxGuiScale);
    return pref <= 0 ? autoK : std::min(pref, maxK);
  }

  /// Syncs widgets with `model`, lays out for `viewport` and advances UI time.
  void update(const HudModel& model, ui::Vec2 viewport, float dt);
  /// Paints into `hud.draws`; block icons ("block:<id>") use the material
  /// swatches / block icons, item icons ("item:<itemId>") use `itemIcon`
  /// (the cResourceMgr's per-item icon atlas, P2b-1).
  void paint(HudLayer& hud, const BlockMaterials& materials, const std::function<const Image*(BlockId)>& icon = {},
             const std::function<const Image*(uint32_t)>& itemIcon = {}) const;

 private:
  void rebuildHud();
  float s() const { return scale_; }

  Callbacks cb_;
  ui::UiRoot ui_;
  float scale_ = 0;  // HUD scale the widgets were built for (0 = not built)
  ui::Widget* hud_ = nullptr;
  ui::Panel* debugPanel_ = nullptr;
  ui::Label* debugText_ = nullptr;
  ui::Label* debugLook_ = nullptr;      ///< 2026-10-16 (JD): the look-at line, red
  ui::Label* debugTextPost_ = nullptr;  ///< debug lines after the look-at line
  ui::Panel* controlsPanel_ = nullptr;
  ui::Label* controlsText_ = nullptr;
  ui::Panel* banner_ = nullptr;
  ui::Label* bannerTitle_ = nullptr;
  ui::Label* bannerText_ = nullptr;
  std::array<ui::Slot*, kHotbarSlots> slots_{};
  ui::Label* slotName_ = nullptr;
  ui::Label* lookName_ = nullptr;  ///< 2026-10-16 (JD): looked-at block, above the held-item name
  ui::Label* splash_ = nullptr;  // "Powered by LithoLattice", bottom right for the first seconds
  float age_ = 0;
  ui::Dialog* inventory_ = nullptr;
  ui::Dialog* pause_ = nullptr;
  ui::Label* inventoryHover_ = nullptr;
  std::vector<ui::Slot*> carrySlots_;   // playtest 2026-10-16: the dialog's carry grid
  std::vector<ItemStack> carry_;        // what it shows (hover text)
  ui::Widget* menu_ = nullptr;
  float menuScale_ = 0;  // UI scale the menu was built for (0 = not built)
  std::string version_;
  ui::Dialog* settings_ = nullptr;
  int hudScalePref_ = 0;  // 0 = auto

  void rebuildMenu();
};

/// "block:<id>" for a block icon.
std::string blockIcon(BlockId b);

}  // namespace lithokraft
