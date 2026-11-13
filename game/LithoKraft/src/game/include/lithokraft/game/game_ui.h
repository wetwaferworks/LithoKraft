// The game's screens on the retained UI: HUD (debug panel, controls panel, keyboard-capture
// banner), the hotbar and the block palette (a modal dialog). The game hands in a
// HudModel each frame and reacts to callbacks; cGameUi owns the widgets and the cUiRoot.
#pragma once
#include "lithokraft/game/engine.h"
#include <array>
#include <cstdint>
#include <functional>
#include <algorithm>
#include <string>
#include <vector>
#include "litholattice/render/block_materials.h"
#include "litholattice/render/renderer.h"
#include "lithokraft/game/ui_widgets.h"
#include "lithokraft/game/ui_textedit.h"  // Item 4: the All tab's filter box (cTextBox)
#include <map>
#include "litholattice/world/block.h"
#include "lithokraft/game/inventory.h"  // HudModel hotbar = the inventory's real ItemStacks (P2b-1)
#include "lithokraft/game/settings.h"

namespace lithokraft {

constexpr int kHotbarSlots = 16;  // 2026-10-24 (JD): was 8

/// What the HUD shows this frame. `hotbar` is a snapshot of the player
/// inventory's hotbar slots (P2b-1): stacks, not bare BlockIds — a tool or
/// the Fragment in a slot renders with its own item icon and name.
struct HudModel {
  std::array<ItemStack, kHotbarSlots> hotbar{};
  int selected = 0;
  // 2026-11-09 next-build-2 (Item 4, combined creative screen): the Agent
  // panel's bars on the creative screen's inventory half. Values/max from
  // the player adapter's vitals and stats (health/kharge 0..64, static
  // unbounded so its max is its current value, bar full). Display only.
  int vitHealth = 64, vitHealthMax = 64;
  int vitKharge = 0, vitKhargeMax = 64;
  int vitStatic = 0;
  bool visible = true;                     ///< F1 hides everything
  std::vector<std::string> debugLines;     ///< empty: no debug panel (F3)
  std::string tickLine;                    ///< 2026-10-31 (JD): tick stability readout (empty: hidden)
  int tickLevel = 0;                       ///< 0 green, 1 yellow, 2 red, 3 neutral (TPS paused)
  std::vector<std::string> controlLines;   ///< controls panel (H)
  std::vector<std::string> bannerLines;    ///< keyboard capture banner (empty: none)
  std::vector<std::string> postViewLines;  ///< 2026-10-25 Post viewer: title, 16 rows, hint (empty: hidden)
  std::string lookName;                    ///< 2026-10-16 (JD): looked-at block name (empty: hidden)
};

class cGameUi {
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
    std::function<void()> saveWorld;  ///< 2026-10-31 (JD): pause menu "Save world"
    std::function<void()> loadWorld;  ///< 2026-10-31 (JD): pause menu "Load world" (opens the world picker)
    /// 2026-11-09 next-build-2 (Item 4): a filled slot on the creative
    /// screen's inventory half was clicked to put its stack BACK to the
    /// palette (creative semantics: the stack leaves the inventory; the
    /// palette is infinite so nothing is stored). Index is the flat
    /// Inventory::allSlots order (0..kTotalSlots-1), gear included.
    /// (Appended last, so positional initializers stay valid.)
    std::function<void(int flatIndex)> putBack;
    /// Item 4, Equipment tab: a palette ITEM entry (a registered item id,
    /// not a block) was clicked: give one of it (its own stack cap) into
    /// the selected hotbar slot (shift: first free slot), same as blocks.
    std::function<void(uint32_t itemId, bool shift)> pickItem;
    /// Cursor stack (2026-11-11, JD finding 07:02 EST: "the mouse is
    /// considered the hand"; Cursor-Stack-Plan-2026-11-11.md): a carry
    /// slot (flat Inventory::allSlots index) was PRESS-clicked on the
    /// creative screen. The game resolves lift / place / merge / swap
    /// from its own cursor state. Fires on the press edge (the slots
    /// are press-activated), exactly once per physical click.
    /// (Appended last, so positional initializers stay valid.)
    std::function<void(int flatIndex)> cursorClick;
    /// Cursor stack: a palette-half press landed while a stack is
    /// carried. The game discards the carried stack back into the
    /// infinite creative source (JD ruling 2026-11-10: never a world
    /// drop). Only fires while carrying; a palette press while NOT
    /// carrying is the ordinary pickBlock / pickItem give above.
    std::function<void()> paletteClickCursor;
    /// D9 2026-11-12 (JD): Shift + press on a hotbar (0..15) or main
    /// (18..49) slot while NOT carrying: the game moves the slot's whole
    /// stack to the other section (Game::shiftTransfer). Hands and gear,
    /// and any press while carrying, stay on cursorClick.
    /// (Appended last, so positional initializers stay valid.)
    std::function<void(int flatIndex)> shiftClick;
  };

  explicit cGameUi(Callbacks callbacks);
  ui::cUiRoot& root() { return ui_; }
  const ui::cUiRoot& root() const { return ui_; }

  /// Re-applies a theme (after a resource reload); the HUD scale stays.
  void applyTheme(const ui::Theme& theme);
  void openInventory(int selectedSlot);
  /// 2026-11-09 next-build-2 (Item 4): THE COMBINED CREATIVE SCREEN (JD's
  /// approved design, 2026-11-08; scope change 2026-11-09 07:52 EST
  /// superseding D5). One dialog, two halves: LEFT the Bedrock-model
  /// creative picker (five tabs Construction/Equipment/Items/Nature/All,
  /// filter inside All, collapsible family groups, infinite palette,
  /// Trace components included); RIGHT JD's Nov 3 "Inv" reference scaled
  /// to its half (capacity readout, 64-slot main grid: 4 rows of 16, B9; Agent panel with
  /// Health/Kharge/Static bars and the 8 worn slots, 6 Accessories, and
  /// the 16-slot hotbar flanked by the L/R hand slots OUTSIDE the 16).
  /// `carry` is the full flat slot vector (Inventory::allSlots order,
  /// kTotalSlots entries: hotbar, hands, main, gear). This supersedes
  /// the old palette dialog as the creative entry point; the entry flow
  /// (E key) is unchanged. Interaction (cursor stack, 2026-11-11):
  /// palette click while NOT carrying fires pickBlock / pickItem (B9,
  /// 2026-11-11: the game puts a full stack in the hand; shift = the
  /// quick give to the first free hotbar slot); a carry-slot press lifts the slot's stack
  /// onto the cursor, places it, merges it, or swaps it (the game
  /// resolves, via the cursorClick callback); a palette press WHILE
  /// carrying discards the carried stack (paletteClickCursor). The old
  /// click-a-filled-slot put-back is retired from the click path (JD's
  /// 2026-11-11 finding: it read as "clicking does nothing"); putBack
  /// stays declared for compatibility but no slot fires it any more.
  void openInventory(int selectedSlot, const std::vector<ItemStack>& carry);
  /// Refresh the open screen's inventory half (after a give / put-back).
  /// No-op when closed. (Name kept from the palette era.)
  void refreshCarry(const std::vector<ItemStack>& carry);
  /// Cursor stack display (2026-11-11): the game hands in the carried
  /// stack (empty = not carrying) beside every refreshCarry on the
  /// gesture paths. cGameUi keeps only the DISPLAY copy: the floating
  /// cell that rides the pointer on the dynamic layer (see
  /// Cursor-Stack-Plan section 7). The game's cursorStack_ is the
  /// truth; this copy is refreshed at every mutation, so the palette
  /// lambdas' carrying() belief cannot drift across a click boundary.
  void refreshCursor(const ItemStack& stack);
  bool carrying() const { return !cursor_.empty(); }
  const ItemStack& cursorDisplay() const { return cursor_; }
  /// The floating cursor cell's host widget (selftests read its rect /
  /// visibility / dynamic layer membership). Null before the first HUD
  /// build.
  ui::cWidget* cursorHostForTest() const { return cursorHost_; }
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
  /// 2026-11-11 (JD): the whole Creative inventory window (picker, slots,
  /// chrome, and Agent panel) is this multiple of its previous design size
  /// (1152 x 568 at GUI scale 1). 1.25 = +25% over the size JD liked in
  /// tonight's maximized-window session. Applied only to the three window
  /// design frames so fit-grids grow with the window; DialogHost still
  /// caps to the viewport minus margins. Not pinned: auto GUI scale stays
  /// window-size dependent (720p -> 1, a 1080p framebuffer -> 2). Left
  /// out of kInventoryScale on purpose: that constant also drives the
  /// HUD hotbar (kHotbarScale).
  static constexpr float kInventoryWindowScale = 1.25f;
  /// 2026-10-25 (JD: "scale up the UI of the hotbar by twenty-five percent"): the ONE constant for
  /// the HUD hotbar (slots, icons, selection frame, key captions, gaps and the labels above it).
  /// update() caps it so the 16-slot row always fits the window width (see hotbarScaleFor).
  static constexpr float kHotbarScale = kInventoryScale * 1.125f;
  static float hotbarScaleFor(float viewportW, float k) {
    const float rowUnits = kHotbarSlots * 40.f + (kHotbarSlots - 1) * 4.f;  // slot + gap units at scale 1
    const float fit = (viewportW - 16.f) / (k * rowUnits);
    return fit < kHotbarScale ? fit : kHotbarScale;
  }
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
  /// Split delivery (GUI draw-cost program, cure 1, 2026-11-10): as
  /// paint(), plus the facade's static/dynamic stream refs for the
  /// backends' split expansion path. `split` is reset then filled;
  /// its layers are owned by the facade and stay valid while their
  /// generations hold.
  void paintSplit(HudLayer& hud, litholattice::HudLayerSplit& split, const BlockMaterials& materials,
                  const std::function<const Image*(BlockId)>& icon = {},
                  const std::function<const Image*(uint32_t)>& itemIcon = {}) const;

  // ---- 2026-11-09 next-build-2: GUI phase profile (GUI diagnosis Section 6).
  // Per-phase times of the most recent update()/paint() calls, in ms, plus
  // the paint outcome. Written by update()/paint() on the main thread,
  // read by the app loop's spike record and the per-second profile line.
  // Measure-only: two steady-clock reads per phase, nothing else changes.
  struct GuiFrameTimes {
    double modelMs = 0, updateMs = 0, layoutMs = 0, paintMs = 0;
    bool paintRebuilt = false;
    size_t paintDraws = 0;
    size_t paintRects = 0, paintTexts = 0, paintDrawsOnly = 0, paintIcons = 0;  // step 0 split
    bool dynRebuilt = false;   // JD paint ruling (2026-11-09): the dynamic readout layer's outcome
    size_t dynDraws = 0;
  };
  const GuiFrameTimes& guiTimes() const { return guiTimes_; }

 private:
  void rebuildHud();
  mutable GuiFrameTimes guiTimes_{};  // mutable: paint() is const
  float s() const { return scale_; }

  Callbacks cb_;
  ui::cUiRoot ui_;
  float scale_ = 0;  // HUD scale the widgets were built for (0 = not built)
  ui::cWidget* hud_ = nullptr;
  float hotbarScale_ = kHotbarScale;  ///< kHotbarScale capped to the window (hotbarScaleFor)
  ui::cPanel* debugPanel_ = nullptr;
  ui::cLabel* debugText_ = nullptr;
  ui::cLabel* tickText_ = nullptr;       ///< 2026-10-31 (JD): tick stability line (coloured)
  ui::cPanel* tickPanel_ = nullptr;
  ui::cLabel* debugLook_ = nullptr;      ///< 2026-10-16 (JD): the look-at line, red
  ui::cLabel* debugTextPost_ = nullptr;  ///< debug lines after the look-at line
  ui::cPanel* controlsPanel_ = nullptr;
  ui::cLabel* controlsText_ = nullptr;
  ui::cPanel* banner_ = nullptr;
  ui::cLabel* bannerTitle_ = nullptr;
  ui::cLabel* bannerText_ = nullptr;
  ui::cPanel* postView_ = nullptr;      ///< 2026-10-25 Post viewer overlay
  ui::cLabel* postViewTitle_ = nullptr;
  ui::cLabel* postViewText_ = nullptr;
  ui::cLabel* postViewHint_ = nullptr;
  std::array<ui::cSlot*, kHotbarSlots> slots_{};
  ui::cLabel* slotName_ = nullptr;
  ui::cLabel* lookName_ = nullptr;  ///< 2026-10-16 (JD): looked-at block, above the held-item name
  ui::cLabel* splash_ = nullptr;  // "Powered by LithoLattice", bottom right for the first seconds
  float age_ = 0;
  ui::cDialog* inventory_ = nullptr;
  ui::cDialog* pause_ = nullptr;
  ui::cLabel* inventoryHover_ = nullptr;
  std::vector<ui::cSlot*> carrySlots_;   // the creative screen's inventory-half slots (flat allSlots order)
  std::vector<ItemStack> carry_;        // what it shows (hover text)
  // Cursor stack display state (2026-11-11): the carried stack's
  // DISPLAY copy and its floating cell. The host is a child of the HUD
  // root tree (NOT the dialog tree, so palette rebuilds never touch
  // it), flagged as a dynamic surface so it rides the dynamic readout
  // layer above the modal; it is positioned from the facade's polled
  // pointer at the tail of update() and is disabled, so hit testing
  // skips it and it can never steal the click it hovers over.
  ItemStack cursor_;                     // display copy (empty = not carrying)
  ui::cWidget* cursorHost_ = nullptr;    // dynamic-surface host in the HUD tree
  ui::cSlot* cursorSlot_ = nullptr;      // the floating cell inside the host
  float cursorCell_ = 0;                 // the cell's edge length at the build scale
  void clearCursorDisplay();             // display-only reset (both close paths)
  // Item 4 creative-screen state (UI-only; rebuilt on every open).
  ui::cLabel* creativeCap_ = nullptr;        // the "N \\ 82 - 41,984 max" readout (B9: from kCarrySlots x kMaxStack)
  ui::cProgressBar* agentHealth_ = nullptr;  // Agent bar TRACKS (fill refreshed per frame)
  ui::cProgressBar* agentKharge_ = nullptr;
  ui::cProgressBar* agentStatic_ = nullptr;
  ui::cLabel* agentHealthVal_ = nullptr;  // Agent bar value labels (the mock's label line above each track)
  ui::cLabel* agentKhargeVal_ = nullptr;
  ui::cLabel* agentStaticVal_ = nullptr;
  ui::cWidget* paletteHost_ = nullptr;       // left half: tab pages rebuilt on tab/filter/group change
  int creativeTab_ = 4;                      // 0 Construction, 1 Equipment, 2 Items, 3 Nature, 4 All
  std::string creativeFilter_;               // the All tab's filter text (lowercased match)
  std::map<std::string, bool> creativeGroupOpen_;  // collapsible group state, by group name
  std::array<ui::cButton*, 5> creativeTabBtns_{};
  void rebuildCreativePalette();             // rebuilds paletteHost_'s children for the current tab/filter/groups
  ui::cWidget* menu_ = nullptr;
  float menuScale_ = 0;  // UI scale the menu was built for (0 = not built)
  std::string version_;
  ui::cDialog* settings_ = nullptr;
  int hudScalePref_ = 0;  // 0 = auto
  float autoScale_ = 1.f;  // 2026-10-31: auto GUI scale for the fixed-size F3 overlay
  void setDebugStyle(ui::Theme& t, float autoK);

  void rebuildMenu();
};

/// "block:<id>" for a block icon.
std::string blockIcon(BlockId b);

}  // namespace lithokraft
