#include <cstdlib>
#include <cctype>
#include "lithokraft/game/game_ui.h"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>
#include "lithokraft/content/blocks.h"
#include "lithokraft/content/block_families.h"  // Item 4: palette groups + tab membership
#include "lithokraft/content/items.h"  // itemDisplayName / itemIconRef (P2b-1 stack rendering)

namespace lithokraft {
namespace {

std::string joinLines(const std::vector<std::string>& lines) {
  std::string out;
  for (size_t i = 0; i < lines.size(); ++i) out += (i ? "\n" : "") + lines[i];
  return out;
}

const char* blockName(BlockId b) { return b == kAir ? "empty" : blockRegistry().def(b).name.data(); }

std::string blockDescription(BlockId b) {
  std::string text = blockName(b);
  if (const BlockDef& d = blockRegistry().def(b); d.tier)
    text += " - tier " + std::to_string(d.tier) + " palette, wrap mapping (colour = (s-1) mod " + std::to_string(d.tier) + ")";
  return text;
}

}  // namespace

constexpr float kSplashSeconds = 5.f;

constexpr int kCarryData = 1 << 20;  // playtest 2026-10-16: carry-grid slot data base (> any BlockId)
std::string blockIcon(BlockId b) { return "block:" + std::to_string(int(b)); }

cGameUi::cGameUi(Callbacks callbacks) : cb_(std::move(callbacks)) {}

void cGameUi::applyTheme(const ui::Theme& theme) {
  ui::Theme t = theme;
  t.setScale(ui_.theme().scale());
  setDebugStyle(t, autoScale_);
  ui_.setTheme(std::move(t));
}

// 2026-10-31 (JD): hud_debug = the label style with textScale autoK / k, so the F3 overlay renders at
// the auto GUI scale whatever hudScale is (gameplay HUD only follows hudScale). LL_DEBUG_HUDSCALE=1: old.
void cGameUi::setDebugStyle(ui::Theme& t, float autoK) {
  static const bool old = [] { const char* e = std::getenv("LL_DEBUG_HUDSCALE"); return e && e[0] == '1'; }();
  autoScale_ = autoK > 0.f ? autoK : 1.f;
  ui::Style st = t.style("label");
  const float k = t.scale() > 0.f ? t.scale() : 1.f;
  st.textScale = old ? 1.f : autoScale_ / k;
  // 2026-11-01 (JD 12:58): F3 text AND the controls panel share ONE fixed size, independent of
  // hudScale: hud_small's 0.5 at the AUTO GUI scale (textScale 0.5 * autoK / k), i.e. the panel's
  // look at hudScale=auto. LL_DEBUG_SMALL=0: F3 = the 10-31 auto-size label, controls = hud_small.
  static const bool small = [] { const char* e = std::getenv("LL_DEBUG_SMALL"); return !(e && e[0] == '0'); }();
  ui::Style fixed = t.style("hud_small");
  if (small && !old) {
    fixed.textScale = 0.5f * autoScale_ / k;
    st = fixed;
  }
  t.setStyle("hud_debug", st);
  t.setStyle("hud_controls", fixed);
  // 2026-10-31 (JD): opaque modal dialogs (inventory) so the world / F3 never bleed through.
  ui::Style dlg = t.style("dialog");
  dlg.background.a = 1.f;
  t.setStyle("dialog", dlg);
}

void cGameUi::rebuildHud() {
  if (hud_) ui_.destroyLater(ui_.root().removeChild(*hud_));
  hud_ = &ui_.root().add<ui::cWidget>();
  hud_->setId("hud");
  const float k = s();

  debugPanel_ = &hud_->add<ui::cPanel>("hud");
  debugPanel_->setDynamicSurface(true);  // JD paint ruling (2026-11-09): F3 text is a dynamic readout surface
  debugPanel_->setLayout(ui::Layout::at({0, 0}, {0, 0}, {8 * k, 24 * k}));  // Playtest 1 #5: top padding (was 8; first line clipped under the header)
  // 2026-10-16 (JD): the look-at line renders red so it stands out; the rest is unchanged.
  auto& debugStack = debugPanel_->add<ui::cStackPanel>(ui::Orientation::Vertical, 0.f);
  // 2026-10-31 (JD): F3 text ignores hudScale and stays at the auto size (hud_debug; LL_DEBUG_HUDSCALE=1 = old).
  debugText_ = &debugStack.add<ui::cLabel>("", "hud_debug");
  debugLook_ = &debugStack.add<ui::cLabel>("", "hud_debug");
  debugLook_->setColor({1.f, 0.25f, 0.25f, 1.f});
  debugTextPost_ = &debugStack.add<ui::cLabel>("", "hud_debug");

  // 2026-10-31 (JD): tick stability readout, top right, coloured by cTickStabilityMeter.
  tickPanel_ = &hud_->add<ui::cPanel>("hud");
  tickPanel_->setDynamicSurface(true);  // JD paint ruling (2026-11-09): the tick/TPS line is a dynamic readout surface
  tickPanel_->setId("tick-panel");
  tickPanel_->setLayout(ui::Layout::at({1, 0}, {1, 0}, {-8 * k, 8 * k}));
  tickText_ = &tickPanel_->add<ui::cLabel>("", "hud_small");

  controlsPanel_ = &hud_->add<ui::cPanel>("hud");
  controlsPanel_->setId("controls-panel");
  // Cortana 2026-10-16 (standards review): sits ABOVE the hotbar + slot-name rows (it used to
  // overlap the hotbar at 2x GUI scale) and uses the compact hud_small text.
  // 2026-11-08 (layout diagnosis item 2): the panel's placement now lifts
  // by its REAL measured height (the facade measure fix), so this offset
  // is only the clearance above the hotbar: hotbar (40*hotbarScale) plus
  // BOTH name-label rows above it (slotName_ and lookName_, 24 each) plus
  // margins. The old sum cleared only one label row and the block's last
  // lines sat on the looked-at name.
  controlsPanel_->setLayout(ui::Layout::at({0, 1}, {0, 1}, {8 * k, -(6 + 40 * hotbarScale_ + 48 + 12) * k}));
  controlsText_ = &controlsPanel_->add<ui::cLabel>("", "hud_controls");  // 2026-11-01 (JD): fixed size
  controlsText_->setColor({0.9f, 0.95f, 1.f, 1.f});

  banner_ = &hud_->add<ui::cPanel>("hud");
  banner_->setLayout(ui::Layout::at({0.5f, 0.3f}, {0.5f, 0}));
  auto& bannerStack = banner_->add<ui::cStackPanel>(ui::Orientation::Vertical, 0.f);
  bannerTitle_ = &bannerStack.add<ui::cLabel>();
  bannerTitle_->setColor({1.f, 0.85f, 0.4f, 1.f});
  bannerText_ = &bannerStack.add<ui::cLabel>();
  bannerText_->setColor({0.6f, 0.9f, 1.f, 1.f});

  // 2026-10-25 (JD, Post viewer): the full 256 characters of a looked-at Post, centred, at HUD
  // text size (far larger than the 1/16 glyphs on the panel; diagonal posts show only 160 there).
  postView_ = &hud_->add<ui::cPanel>("panel");
  postView_->setLayout(ui::Layout::at({0.5f, 0.5f}, {0.5f, 0.5f}));
  auto& pvStack = postView_->add<ui::cStackPanel>(ui::Orientation::Vertical, 0.f);
  postViewTitle_ = &pvStack.add<ui::cLabel>();
  postViewTitle_->setColor({0.82f, 0.62f, 1.f, 1.f});
  postViewText_ = &pvStack.add<ui::cLabel>();
  postViewText_->setColor({0.96f, 0.98f, 1.f, 1.f});
  postViewHint_ = &pvStack.add<ui::cLabel>("", "hud_small");
  postViewHint_->setColor({0.75f, 0.8f, 0.9f, 1.f});

  splash_ = &hud_->add<ui::cLabel>("Powered by LithoLattice");
  splash_->setId("splash");
  splash_->setColor({0.75f, 0.85f, 1.f, 1.f});
  splash_->setLayout(ui::Layout::at({1, 1}, {1, 1}, {-8 * k, -8 * k}));

  auto& bar = hud_->add<ui::cStackPanel>(ui::Orientation::Horizontal, 4 * k * hotbarScale_, "hud");
  bar.setId("hotbar");
  bar.setLayout(ui::Layout::at({0.5f, 1}, {0.5f, 1}, {0, -6 * k}));
  for (int i = 0; i < kHotbarSlots; ++i) {
    ui::cSlot& slot = bar.add<ui::cSlot>(ui::Vec2{40 * k * hotbarScale_, 40 * k * hotbarScale_});  // kHotbarScale (JD 2026-10-25)
    slot.setScale(hotbarScale_ / kInventoryScale);  // frame + caption grow with the slot
    slot.setStyleClass("hotbar_slot");  // C9 2026-11-12 (JD): own outline class; D9: #8A2EFF, 3 px
    // 2026-10-24 (JD): one row of 16; captions are the direct-select keys (1..9, 0), slots 11..16 are wheel-only.
    slot.setCaption(i < 9 ? std::to_string(i + 1) : i == 9 ? std::string("0") : std::string()).setData(i);
    slot.on("click", [this](ui::cWidget& w) {
      if (cb_.selectSlot) cb_.selectSlot(static_cast<ui::cSlot&>(w).data());
    });
    slots_[size_t(i)] = &slot;
  }
  slotName_ = &hud_->add<ui::cLabel>();
  slotName_->setLayout(ui::Layout::at({0.5f, 1}, {0.5f, 1}, {0, -(40 * hotbarScale_ + 6 + 24) * k}));
  lookName_ = &hud_->add<ui::cLabel>();  // 2026-10-16 (JD): one line above the held-item name
  lookName_->setLayout(ui::Layout::at({0.5f, 1}, {0.5f, 1}, {0, -(40 * hotbarScale_ + 6 + 24 + 24) * k}));
  lookName_->setColor({1.f, 1.f, 1.f, 1.f});
  slotName_->setShadow(true);  // 2026-11-01 (JD): held-item + looked-at names always get a drop shadow
  lookName_->setShadow(true);

  // Cursor stack (2026-11-11): the carried stack's floating cell. It
  // lives in the HUD root tree (never the dialog tree, so palette
  // rebuilds cannot drop it), LAST among the HUD children, flagged as
  // a dynamic surface: it paints on the dynamic readout layer, above
  // the modal dialog, and pointer motion re-expands only that layer
  // (JD paint ruling, 2026-11-09). The HOST is disabled on purpose:
  // hit testing stops at a disabled widget without consulting its
  // children, so the cell that rides exactly under the pointer can
  // never steal the click it hovers over. The cell itself stays
  // enabled, so its paint is the ordinary slot paint (a disabled
  // widget's background would solve to the disabled colour).
  // Position comes from the facade's polled pointer at the tail of
  // update(); the cell size matches a creative-main cell at this
  // build scale (40 * k * kInventoryScale).
  cursorCell_ = 40 * k * kInventoryScale;
  cursorHost_ = &hud_->add<ui::cWidget>();
  cursorHost_->setId("cursor-stack");
  cursorHost_->setDynamicSurface(true);
  cursorHost_->setEnabled(false);
  cursorHost_->setVisible(false);
  cursorSlot_ = &cursorHost_->add<ui::cSlot>(ui::Vec2{cursorCell_, cursorCell_});
  refreshCursor(cursor_);  // re-seat the display copy on the fresh widgets
}

void cGameUi::refreshCursor(const ItemStack& stack) {
  cursor_ = stack;
  if (cursorSlot_) {
    cursorSlot_->setIcon(stack.empty() ? std::string() : itemIconRef(stack.itemId));
    cursorSlot_->setCaption(stack.empty() || stack.count <= 1 ? std::string() : std::to_string(stack.count));
    cursorSlot_->setTooltip(stack.empty() ? std::string()
                                          : itemDisplayName(stack.itemId) + " x" + std::to_string(stack.count));
  }
  // The cell shows only while the creative screen is open and a stack
  // is carried; visibility flips ride the dynamic-membership path, so
  // they cost one dynamic repaint, never a main-layer rebuild.
  if (cursorHost_) cursorHost_->setVisible(!stack.empty() && inventory_ != nullptr);
}

void cGameUi::clearCursorDisplay() {
  cursor_ = ItemStack{};
  if (cursorSlot_) {
    cursorSlot_->setIcon(std::string());
    cursorSlot_->setCaption(std::string());
    cursorSlot_->setTooltip(std::string());
  }
  if (cursorHost_) cursorHost_->setVisible(false);
}

void cGameUi::openInventory(int selectedSlot) { openInventory(selectedSlot, {}); }

namespace {
// B9 four-row (2026-11-12): the capacity readout derives from the layout constants
// ("N \ 82 - 41,984 max": N filled carry slots of kCarrySlots, kCarrySlots x kMaxStack items).
std::string creativeCapacityText(int filled) {
  std::string mx = std::to_string(kCarrySlots * kMaxStack);
  for (int i = int(mx.size()) - 3; i > 0; i -= 3) mx.insert(size_t(i), ",");
  return std::to_string(filled) + " \\ " + std::to_string(kCarrySlots) + " - " + mx + " max";
}
}  // namespace

void cGameUi::refreshCarry(const std::vector<ItemStack>& carry) {
  if (!inventory_ || carrySlots_.empty()) return;
  carry_ = carry;
  for (size_t i = 0; i < carrySlots_.size() && i < carry_.size(); ++i) {
    const ItemStack& st = carry_[i];
    carrySlots_[i]->setIcon(st.empty() ? std::string() : itemIconRef(st.itemId));
    carrySlots_[i]->setCaption(st.empty() || st.count <= 1 ? std::string() : std::to_string(st.count));
    carrySlots_[i]->setTooltip(st.empty() ? std::string() : itemDisplayName(st.itemId) + " x" + std::to_string(st.count));  // 2026-10-31 (JD)
  }
  if (creativeCap_) {  // Item 4: "N \ 82 - 41,984 max" (N = filled carry slots; B9: kCarrySlots x kMaxStack)
    int filled = 0;
    for (size_t i = 0; i < carry_.size() && i < size_t(kCarrySlots); ++i)
      if (!carry_[i].empty()) ++filled;
    creativeCap_->setText(creativeCapacityText(filled));
  }
}

namespace {
// Item 4 tab membership (Bedrock's five tabs mapped onto the registry's
// own data): Construction = Building family; Nature = the foliage set
// (the content layer's named foliage ids); Equipment = the wearable /
// usable content items (the item registry's non-block items: tools, the
// Fragment, gear items... the palette is block-based, so this tab lists
// those item-backed blocks the registry carries); Items = everything
// else, which is where the Trace components (the logic pieces) live;
// All = everything.
bool isFoliageBlock(BlockId b) {
  static const std::vector<BlockId> kFoliage = {kFoliageCattail, kFoliageBush, kFoliageFern, kFoliageReeds,
                                                kFoliageCrystalShrub, kFoliageMoss};
  return std::find(kFoliage.begin(), kFoliage.end(), b) != kFoliage.end();
}
bool creativeTabHas(int tab, BlockId b) {
  if (tab == 4) return true;
  const BlockFamily f = cBlockFamilies::get().familyOf(b);
  switch (tab) {
    case 0: return f == BlockFamily::Building;
    case 1: return false;  // Equipment holds ITEM entries, added by the rebuild (not blocks)
    case 2: return f != BlockFamily::Building && !isFoliageBlock(b);
    case 3: return isFoliageBlock(b);
    default: return true;
  }
}
std::string lowerAscii(std::string_view s) {
  std::string out(s);
  for (char& c : out) c = char(std::tolower(uint8_t(c)));
  return out;
}
}  // namespace

void cGameUi::rebuildCreativePalette() {
  if (!paletteHost_) return;
  paletteHost_->clearChildren();
  // The rebuild replaces the palette subtree mid-frame (tab switch,
  // filter edit, group toggle). The dialog around it re-centres only
  // when its measure CHANGES (DialogHost::layout), and the WS4 gate in
  // cUiRoot::layout skips same-generation frames... a tab whose content
  // measures the same as the previous tab's would keep the dialog at
  // the OLD placement, and the new grid's rects would be stamped
  // relative to that stale origin while hit-testing walks the dialog
  // chain elsewhere. Relayout the dialog itself after the rebuild so
  // the whole subtree re-places in the same frame it appears.
  ui::cWidget* dlgW = paletteHost_;
  while (dlgW && dlgW->facadeId() != "inventory") dlgW = dlgW->parentW();
  const bool hadRect = dlgW && dlgW->rect().w > 0;
  const lithogui::Rect dlgRect = dlgW ? dlgW->rect() : lithogui::Rect{};
  const float k = std::max(1.f, s());
  const float cell = 40 * k * kInventoryScale;
  const int cols = 9;  // the mock's palette density (was 7: sparser than approved)
  static const char* kTabNames[5] = {"Construction", "Equipment", "Items", "Nature", "All"};
  for (int t = 0; t < 5; ++t)
    if (creativeTabBtns_[size_t(t)])
      creativeTabBtns_[size_t(t)]->setText(std::string(t == creativeTab_ ? "> " : "") + kTabNames[t] + (t == creativeTab_ ? " <" : ""));
  auto& col = paletteHost_->add<ui::cStackPanel>(ui::Orientation::Vertical, 4 * k);
  if (creativeTab_ == 4) {  // the filter lives in All (Bedrock, not Java)
    auto& filter = col.add<ui::cTextBox>();
    filter.setId("creative-filter");
    filter.setPlaceholder("Filter blocks...");
    if (!creativeFilter_.empty()) filter.setText(creativeFilter_);
    filter.setLayout(ui::Layout::sized({0, 28 * k}));
    filter.on("change", [this, &filter](ui::cWidget&) {
      const std::string f = lowerAscii(filter.text());
      if (f != creativeFilter_) {
        creativeFilter_ = f;
        rebuildCreativePalette();
      }
    });
  }
  // Deterministic source order (the family-grouped palette enumeration);
  // the All tab deduplicates to one entry per block (Bedrock search rule).
  const std::vector<BlockId> all = paletteBlocksGrouped();
  std::vector<BlockId> shown;
  for (BlockId b : all) {
    if (!creativeTabHas(creativeTab_, b)) continue;
    if (creativeTab_ == 4 && !creativeFilter_.empty() &&
        lowerAscii(blockRegistry().def(b).name).find(creativeFilter_) == std::string::npos)
      continue;
    shown.push_back(b);
  }
  auto& scroll = col.add<ui::cScrollView>();
  {  // the scroll view takes the palette sub-frame's leftover height;
     // its width is the sub-frame's width (stack cross-stretch)
    ui::Layout lay;
    lay.flex = 1;
    scroll.setLayout(lay);
  }
  auto& content = scroll.emplaceContent<ui::cStackPanel>(ui::Orientation::Vertical, 4 * k);
  if (creativeTab_ == 1) {
    // Equipment: the registered non-block items (tools, weapons, the
    // Fragment, materials), each with its item icon and name tooltip.
    auto& grid = content.add<ui::cGridPanel>(cols, ui::Vec2{cell - 4 * k, cell - 4 * k}, 4 * k);
    grid.setFitWidth(true);  // cells solved from the sub-frame's width
    for (const ItemDef& d : itemRegistry()) {
      if (d.itemId == 0) continue;
      ui::cSlot& slot = grid.add<ui::cSlot>(ui::Vec2{cell - 4 * k, cell - 4 * k});
      slot.setIcon(itemIconRef(d.itemId)).setData(int(d.itemId));
      slot.setTooltip(std::string(d.name));
      // Cursor stack (2026-11-11): palette slots dispatch on the
      // press edge; while a stack is carried the press is the DISCARD
      // (paletteClickCursor), never a give on top of the discard.
      slot.setPressActivated(true);
      slot.on("press", [this](ui::cWidget& w) {
        if (carrying()) {
          if (cb_.paletteClickCursor) cb_.paletteClickCursor();
          return;
        }
        if (cb_.pickItem) cb_.pickItem(uint32_t(static_cast<ui::cSlot&>(w).data()), (ui_.modifiers() & ui::kModShift) != 0);
      });
    }
    return;
  }
  auto addGrid = [&](const std::vector<BlockId>& items) {
    auto& grid = content.add<ui::cGridPanel>(cols, ui::Vec2{cell - 4 * k, cell - 4 * k}, 4 * k);
    grid.setFitWidth(true);  // cells solved from the sub-frame's width
    for (BlockId b : items) {
      ui::cSlot& slot = grid.add<ui::cSlot>(ui::Vec2{cell - 4 * k, cell - 4 * k});
      slot.setIcon(blockIcon(b)).setData(int(b));
      slot.setTooltip(blockName(b));  // hover tooltip with the name (theme scale = hudScale)
      // Cursor stack (2026-11-11): press-edge dispatch; while
      // carrying, the press discards instead of giving (see the
      // Equipment slot above).
      slot.setPressActivated(true);
      slot.on("press", [this](ui::cWidget& w) {
        if (carrying()) {
          if (cb_.paletteClickCursor) cb_.paletteClickCursor();
          return;
        }
        if (cb_.pickBlock) cb_.pickBlock(BlockId(static_cast<ui::cSlot&>(w).data()), (ui_.modifiers() & ui::kModShift) != 0);
      });
    }
  };
  if (creativeTab_ == 4) {
    addGrid(shown);  // no collapsible groups in All (Bedrock)
  } else {
    // Collapsible variant groups, keyed by registry family (Bedrock's
    // sub-sections). A family with one block shows as a bare grid.
    static const char* kGroupNames[4] = {"Trace & signals", "Lamps", "Building", "Other blocks"};
    for (int f = 0; f < 4; ++f) {
      std::vector<BlockId> items;
      for (BlockId b : shown)
        if (int(cBlockFamilies::get().familyOf(b)) == f) items.push_back(b);
      if (items.empty()) continue;
      const std::string gname = kGroupNames[f];
      const bool multi = creativeTab_ == 2 || items.size() > 1;
      if (multi) {
        const bool open = creativeGroupOpen_.count(gname) ? creativeGroupOpen_[gname] : true;
        auto& head = content.add<ui::cButton>(std::string(open ? "- " : "+ ") + gname + " (" + std::to_string(items.size()) + ")");
        head.setId("creative-group");
        head.on("click", [this, gname](ui::cWidget&) {
          creativeGroupOpen_[gname] = !(creativeGroupOpen_.count(gname) ? creativeGroupOpen_[gname] : true);
          rebuildCreativePalette();
        });
        if (!open) continue;
      }
      addGrid(items);
    }
  }
  if (hadRect && dlgW) dlgW->place(ui::fromGui(dlgRect), dlgW->theme());
}

void cGameUi::openInventory(int selectedSlot, const std::vector<ItemStack>& carry) {
  if (inventory_) return;
  carrySlots_.clear();
  carry_.clear();
  clearCursorDisplay();  // a screen always opens empty-handed (plan 5.2: the game's close chain guarantees the truth; the display follows)
  const float k = std::max(1.f, s());
  const float invK = k * kInventoryWindowScale;  // 2026-11-11 (JD): +25% inventory window design size
  const float cell = 40 * k * kInventoryScale;  // nominal cell (fit grids solve their real cell from their sub-frame)
  // 2026-11-09 next-build-2 (Item 4): the COMBINED CREATIVE SCREEN (see
  // the header note). P2b-1 honesty carried forward: the palette half
  // GIVES full stacks into the player's real inventory (the same store
  // the HUD shows and player.lat persists); the inventory half IS that
  // store, laid out as JD's Nov 3 reference.
  // 2026-11-09 creative rework (JD ruling, 12:06 EST: "set the GUI in
  // one global frame that is the the screen size and then anchor all
  // the sub frames to that frame"): the screen sits in ONE global
  // cFrame that owns the screen's design size (1152 x 568 at scale 1,
  // times kInventoryWindowScale 1.25 per JD 2026-11-11).
  // The DialogHost spans the viewport and centres the dialog, capping
  // it to the viewport minus margins; the frame is flex-seated in the
  // dialog body, so at a smaller viewport the frame takes the capped
  // rect and every section below solves inside it. Each section is a
  // sub-frame panel anchored to the global frame (picker panel,
  // inventory panel, Agent panel, hotbar section), and grid cells are
  // SOLVED from the sub-frame's allotted width (cGridPanel fit mode),
  // never summed from constants... the Item 4 original built sibling
  // stacks at fixed sizes inside a content-sized dialog, which is why
  // the hotbar row escaped the right edge on JD's machine.
  ui::cDialog& dlg = ui::cDialog::show(ui_, "Creative inventory");
  dlg.setId("inventory");
  // Build 189 fix (2026-11-09, JD's live playtest + his hierarchy
  // ruling 20:34 EST): the design size is declared in the SAME units
  // the contents are built in... build units, k-baked like every cell,
  // margin and spacing below. The rework's "design unscaled" exception
  // (the theme scale multiplies measures at paint, so declare 1152 x
  // 568 raw) split the screen into two unit systems: at GUI scale 2
  // the frame solved at HALF the size its contents assumed, the Agent
  // column and the hotbar section arranged past the dialog's bottom
  // edge, and JD's machine showed exactly that. One unit system per
  // screen: the global frame is the screen, the two windows below are
  // its subframes, and everything inside a window anchors to that
  // window. When the design exceeds the viewport (2304 x 1136 at
  // scale 2 on a 1920x1080 screen, or 2880 x 1420 after the 2026-11-11
  // 25% window bump) DialogHost caps the dialog to the viewport minus
  // margins and the flex/fit solves absorb the cap... the same solve
  // the 1600x1000 bench exercised all along at scale 1.
  auto& frame = dlg.body().add<ui::cFrame>(1152 * invK, 568 * invK);
  frame.setId("creative-global-frame");
  {
    ui::Layout lay;
    lay.flex = 1;  // at a capped dialog the frame takes the body's rect, not its design size
    frame.setLayout(lay);
  }
  auto& halves = frame.add<ui::cStackPanel>(ui::Orientation::Horizontal, 8 * k);
  // ---------------- LEFT: the Bedrock-model creative picker ----------
  // A window subframe of the global frame (JD's hierarchy): it carries
  // its own design size in build units and the picker panel fills it;
  // nothing inside the panel solves against anything outside it.
  auto& leftWin = halves.add<ui::cFrame>(497 * invK, 568 * invK);
  leftWin.setId("creative-picker-frame");
  {
    ui::Layout lay;
    lay.flex = 1;  // the mock's split: palette 1, inventory 1.3 (about 44% / 56%)
    leftWin.setLayout(lay);
  }
  auto& left = leftWin.add<ui::cPanel>();
  left.setId("creative-picker");
  left.setBackgroundColor({0.145f, 0.16f, 0.19f, 1.f});
  left.setBorderColor({0.32f, 0.35f, 0.40f, 1.f});
  // Cursor stack (2026-11-11): the palette BACKGROUND is a discard
  // target while carrying, so a player aiming at the creative side
  // cannot miss into a give by a few pixels. The press tap hears only
  // presses no live widget claimed (slots route their own discard /
  // give; tab buttons, the filter field and group headers consume
  // theirs, so navigation survives carrying... plan section 6).
  left.setPressTap(true);
  left.on("press", [this](ui::cWidget&) {
    if (carrying() && cb_.paletteClickCursor) cb_.paletteClickCursor();
  });
  auto& leftCol = left.add<ui::cStackPanel>(ui::Orientation::Vertical, 4 * k);
  leftCol.setLayout(ui::Layout::fill(ui::Edges::all(10 * k)));
  auto& tabs = leftCol.add<ui::cStackPanel>(ui::Orientation::Horizontal, 2 * k);
  static const char* kTabNames[5] = {"Construction", "Equipment", "Items", "Nature", "All"};
  for (int t = 0; t < 5; ++t) {
    auto& b = tabs.add<ui::cButton>(kTabNames[t]);
    b.setId("creative-tab");
    b.on("click", [this, t](ui::cWidget&) {
      if (creativeTab_ != t) {
        creativeTab_ = t;
        rebuildCreativePalette();
      }
    });
    creativeTabBtns_[size_t(t)] = &b;
  }
  paletteHost_ = &leftCol.add<ui::cWidget>();
  paletteHost_->setId("creative-palette");
  {
    ui::Layout lay;
    lay.flex = 1;
    paletteHost_->setLayout(lay);
  }
  auto& note = leftCol.add<ui::cLabel>(
      "Palette is an infinite source. Click: full stack (512) in your hand; click any slot on the right to put it down.\n"
      "Shift-click: straight to the first free hotbar slot (else slot " + std::to_string(selectedSlot + 1) + "). Click a slot on the right: pick its stack up.\n"
      "Click again: put it down, merge it, or swap. Click the palette while holding: put it back.\n"
      "Q over a slot: throw one item into the world. Shift+Q: throw the whole stack. E/Esc: close", "hud_small");
  note.setId("palette-note");
  // ---------------- RIGHT: JD's Nov 3 inventory, in its sub-frame ----
  // The inventory window subframe, sibling to the picker window under
  // the same law: its own design size in build units, the panel fills
  // it, and the carry grid, Agent panel and hotbar section inside the
  // panel anchor to this window and nothing else.
  auto& rightWin = halves.add<ui::cFrame>(647 * invK, 568 * invK);
  rightWin.setId("creative-inventory-frame");
  {
    ui::Layout lay;
    lay.flex = 1.3f;
    rightWin.setLayout(lay);
  }
  auto& right = rightWin.add<ui::cPanel>();
  right.setId("creative-inventory-half");
  right.setBackgroundColor({0.078f, 0.082f, 0.122f, 1.f});  // the mock's navy inventory panel
  right.setBorderColor({0.56f, 0.64f, 0.85f, 1.f});
  auto& rightCol = right.add<ui::cStackPanel>(ui::Orientation::Vertical, 6 * k);
  rightCol.setLayout(ui::Layout::fill(ui::Edges::all(10 * k)));
  auto fillSlot = [this, k](ui::cWidget& parent, int flat, const char* id, ui::Vec2 size) -> ui::cSlot& {
    ui::cSlot& slot = parent.add<ui::cSlot>(size);
    slot.setId(id);
    slot.setData(kCarryData + flat);
    // Cursor stack (2026-11-11): carry slots dispatch on the PRESS
    // edge and hand EVERY press to the game, filled or empty: the
    // game resolves lift / place / merge / swap from its cursor
    // state. This replaces the Item 4 click-to-put-back, which JD's
    // finding retired (it discarded in place and read as "clicking
    // does nothing"); discard now happens only as the deliberate
    // palette press while carrying.
    slot.setPressActivated(true);
    slot.on("press", [this](ui::cWidget& w) {
      const int flat = static_cast<ui::cSlot&>(w).data() - kCarryData;
      if (flat >= 0 && flat < kTotalSlots) {
        // D9 (JD): Shift-click quick-moves a hotbar / main stack to the other section (Minecraft-style).
        const bool quickMove = (ui_.modifiers() & ui::kModShift) != 0 && !carrying() && cb_.shiftClick &&
                               (flat < kInvHotbarSlots || (flat >= kMainFlatBase && flat < kMainFlatBase + kMainSlots));
        if (quickMove) cb_.shiftClick(flat);
        else if (cb_.cursorClick) cb_.cursorClick(flat);
      }
    });
    carrySlots_.push_back(&slot);
    return slot;
  };
  // The slots are pushed in flat allSlots order (hotbar, L hand, R hand,
  // main, gear) so refreshCarry's indexing stays the flat order, while
  // the WIDGETS are placed in the reference layout below.
  std::vector<ui::cSlot*> byFlat(size_t(kTotalSlots), nullptr);
  {
    // C9 2026-11-12 (JD 6:17-6:18 AM ET): Agent panel ON TOP of the inventory pane, the 32 main slots
    // (four rows of EIGHT) ON THE BOTTOM, close to the hotbar; inventory and hotbar centred in the pane.
    // The Agent panel keeps B9's widgets (worn columns with Accessories beside them) at full width.
    auto& agent = rightCol.add<ui::cPanel>();
    agent.setId("creative-agent");
    agent.setBackgroundColor({0.10f, 0.085f, 0.12f, 1.f});
    agent.setBorderColor({0.69f, 0.29f, 0.56f, 1.f}, 2.f);  // the mock's #b04a8f
    auto& agentCol = agent.add<ui::cStackPanel>(ui::Orientation::Vertical, 4 * k);
    // D9 2026-11-12 (JD): the magenta frame must enclose the whole equipment block. C9 filled the
    // panel with fill(8k insets), but a stretched child's insets never count toward its parent's
    // measured height (cWidget::measure), so the panel measured 16k short and Arms / Feet and the
    // figure hung over its bottom edge (into the Inventory caption row). The column now fills the
    // panel's padded content box with no extra insets: the panel's own padding is the frame's
    // padding on all four sides, and the panel height (so the 4x8 grid's cells) is unchanged.
    agentCol.setLayout(ui::Layout::fill());
    auto& agentTitle = agentCol.add<ui::cLabel>("Agent", "hud");
    agentTitle.setCentered(true);
    // Bars side by side (the mock): each column is a label line (name
    // left in its colour, value right) ABOVE a thin coloured track.
    // No text rides on the track, so nothing can spill past a bar.
    auto& barsRow = agentCol.add<ui::cStackPanel>(ui::Orientation::Horizontal, 6 * k);
    auto makeBar = [&](const char* name, ui::Color nameColor, ui::Color fill, const char* val0,
                       ui::cProgressBar*& trackOut, ui::cLabel*& valOut) {
      auto& colB = barsRow.add<ui::cStackPanel>(ui::Orientation::Vertical, 2 * k);
      {
        ui::Layout lay;
        lay.flex = 1;
        colB.setLayout(lay);
      }
      // Label line (rework fix, 2026-11-09, convicted by the live
      // 1600x1000 shots): the name sits on its own line and the value on
      // the line under it, both above the track. Two separate labels in
      // one horizontal row overlapped ("Healt64\\64"), and one combined
      // "Health 64\\64" label still overflowed the column sideways into
      // its neighbour ("Health 64\\Kharge 0\\64Static 0"): at this
      // panel width each column is only about 80px, and the combined
      // line measures wider. Stacked, each piece fits its column on its
      // own line... the value line is at most "64\\64" wide.
      auto& nm = colB.add<ui::cLabel>(name, "hud_small");
      nm.setColor(nameColor);
      valOut = &colB.add<ui::cLabel>(val0, "hud_small");
      valOut->setColor({0.75f, 0.78f, 0.85f, 1.f});
      trackOut = &colB.add<ui::cProgressBar>();
      trackOut->setFillColor(fill);
      trackOut->setLayout(ui::Layout::sized({0, 7 * k}));
    };
    makeBar("Health", {0.95f, 0.44f, 0.37f, 1.f}, {0.88f, 0.28f, 0.23f, 1.f}, "0\\64", agentHealth_, agentHealthVal_);
    agentHealth_->setId("agent-health");
    makeBar("Kharge", {0.45f, 0.72f, 0.95f, 1.f}, {0.25f, 0.62f, 0.85f, 1.f}, "0\\64", agentKharge_, agentKhargeVal_);
    agentKharge_->setId("agent-kharge");
    makeBar("Static", {0.85f, 0.66f, 0.23f, 1.f}, {0.85f, 0.66f, 0.23f, 1.f}, "0", agentStatic_, agentStaticVal_);
    agentStatic_->setId("agent-static");
    auto& agentBody = agentCol.add<ui::cStackPanel>(ui::Orientation::Horizontal, 8 * k);  // B9 four-row: worn | accessories
    agentBody.setId("creative-agent-body");
    auto& wornRow = agentBody.add<ui::cStackPanel>(ui::Orientation::Horizontal, 4 * k);
    {
      ui::Layout lay;
      lay.flex = 1;
      wornRow.setLayout(lay);
    }
    auto& wornL = wornRow.add<ui::cStackPanel>(ui::Orientation::Vertical, 3 * k);
    auto& figure = wornRow.add<ui::cPanel>();
    figure.setId("agent-figure");
    figure.setBackgroundColor({0.05f, 0.05f, 0.08f, 1.f});
    figure.setBorderColor({0.30f, 0.30f, 0.38f, 1.f});
    {
      ui::Layout lay;
      lay.flex = 1;  // the figure region takes the space between the worn columns, full row height
      figure.setLayout(lay);
    }
    figure.add<ui::cLabel>("Agent", "hud_small").setLayout(ui::Layout::at({0.5f, 0.5f}, {0.5f, 0.5f}));
    auto& wornR = wornRow.add<ui::cStackPanel>(ui::Orientation::Vertical, 3 * k);
    // Worn layout per the reference: left of the figure Head/Face/Chest/
    // Arms, right of it Heart/Mind/Legs/Feet. Gear flat order is head,
    // chest, legs, feet, arms, face, heart, mind (kGearSlots + slot).
    static const struct { const char* name; int gear; } kWornL[4] = {{"Head", 0}, {"Face", 5}, {"Chest", 1}, {"Arms", 4}};
    static const struct { const char* name; int gear; } kWornR[4] = {{"Heart", 6}, {"Mind", 7}, {"Legs", 2}, {"Feet", 3}};
    // JD alignment correction (2026-11-09 08:01 EST): Head/Face/Chest/
    // Arms form ONE clean aligned column keyed to Chest, no staggered
    // offset. Every left label gets the width of the widest ("Chest"),
    // so all four slots start at the same x. (The right column mirrors
    // it: slot first at a shared x, label after.)
    float wornLabelW = 0;
    for (const auto& w : kWornL) wornLabelW = std::max(wornLabelW, dlg.theme().textWidth(w.name, 1.f));
    for (const auto& w : kWornL) {
      auto& rowp = wornL.add<ui::cStackPanel>(ui::Orientation::Horizontal, 3 * k);
      auto& lab = rowp.add<ui::cLabel>(w.name, "hud_small");
      lab.setLayout(ui::Layout::sized({wornLabelW, 0}));
      byFlat[size_t(kCarrySlots + w.gear)] = &fillSlot(rowp, kCarrySlots + w.gear, "creative-worn", {40 * k, 40 * k});
    }
    for (const auto& w : kWornR) {
      auto& rowp = wornR.add<ui::cStackPanel>(ui::Orientation::Horizontal, 3 * k);
      byFlat[size_t(kCarrySlots + w.gear)] = &fillSlot(rowp, kCarrySlots + w.gear, "creative-worn", {40 * k, 40 * k});
      rowp.add<ui::cLabel>(w.name, "hud_small");
    }
    // Capacity row (the mock's caprow): title left, capacity right -- C9: under the Agent panel,
    // directly above the main grid.
    auto& invHead = rightCol.add<ui::cStackPanel>(ui::Orientation::Horizontal, 6 * k);
    invHead.setId("creative-inventory-head");
    invHead.add<ui::cLabel>("Inventory", "hud");
    auto& capSpacer = invHead.add<ui::cWidget>();
    {
      ui::Layout lay;
      lay.flex = 1;
      capSpacer.setLayout(lay);
    }
    creativeCap_ = &invHead.add<ui::cLabel>(creativeCapacityText(0), "hud_small");
    creativeCap_->setId("creative-capacity");
    // C9 main grid: 4 rows x 8, one fit-box grid that takes the height the Agent panel and the hotbar
    // section leave (flex), cells square and evenly spaced (one 4k gap), as large as the rect allows,
    // centred horizontally and sitting at the bottom of the rect (next to the hotbar).
    auto& mainGrid = rightCol.add<ui::cGridPanel>(kMainCols, ui::Vec2{cell - 4 * k, cell - 4 * k}, 4 * k);
    mainGrid.setId("creative-main-grid");
    mainGrid.setFitBox(true);
    {
      ui::Layout lay;
      lay.flex = 1;
      mainGrid.setLayout(lay);
    }
    // Main grid widgets (flat 18..49, C9: 4 rows of 8) in layout
    // position, row-major (carrySlots_ order is fixed up from byFlat below).
    for (int i = 0; i < kMainSlots; ++i)
      byFlat[size_t(kMainFlatBase + i)] = &fillSlot(mainGrid, kMainFlatBase + i, "creative-main", {cell - 4 * k, cell - 4 * k});
    // Accessories: INSIDE the Agent panel (the mock), 3 columns x 2
    // rows, each slot with its label under it, both anchored inside
    // their cell. The 6 jewelry slots have no storage in the current
    // Inventory model (gear is the 8 worn slots only)... display
    // slots, honestly empty, carrying the design's labels.
    auto& accCol = agentBody.add<ui::cStackPanel>(ui::Orientation::Vertical, 4 * k);  // B9 four-row: beside the worn row
    accCol.setId("creative-accessories-col");
    {
      ui::Layout lay;
      lay.flex = 0.8f;
      accCol.setLayout(lay);
    }
    auto& accTitle = accCol.add<ui::cLabel>("Accessories", "hud_small");
    accTitle.setCentered(true);
    auto& accBox = accCol.add<ui::cStackPanel>(ui::Orientation::Vertical, 4 * k);
    accBox.setId("creative-accessories");
    static const char* kAcc[6] = {"Necklace", "L Wrist", "R Wrist", "Belt", "L Ankle", "R Ankle"};
    for (int rowA = 0; rowA < 2; ++rowA) {
      auto& accRow = accBox.add<ui::cStackPanel>(ui::Orientation::Horizontal, 4 * k);
      accRow.setLayout(ui::Layout::sized({0, 54 * k}));
      for (int colA = 0; colA < 3; ++colA) {
        const char* a = kAcc[rowA * 3 + colA];
        auto& cellP = accRow.add<ui::cPanel>("none");
        {
          ui::Layout lay;
          lay.flex = 1;
          cellP.setLayout(lay);
        }
        auto& s = cellP.add<ui::cSlot>(ui::Vec2{36 * k, 36 * k});
        s.setId("creative-accessory");
        s.setTooltip(std::string(a) + " (no accessory storage in the inventory model yet)");
        s.setLayout(ui::Layout::at({0.5f, 0}, {0.5f, 0}, {0, 0}, {36 * k, 36 * k}));
        auto& lab = cellP.add<ui::cLabel>(a, "hud_small");
        lab.setCentered(true);
        lab.setLayout(ui::Layout::at({0.5f, 0}, {0.5f, 0}, {0, 38 * k}));
      }
    }
    // Hotbar section: spans the inventory sub-frame's full width under
    // BOTH the grid and the Agent panel (the mock). Head row: "Hotbar"
    // left, the hover readout centred (the mock's Selected line's
    // place). The 18 slots (Left Hand, 16, Right Hand... hands
    // OUTSIDE the 16) are one fit grid: cells solved from the
    // section's width, so both hands always sit inside the row.
    auto& hotHead = rightCol.add<ui::cStackPanel>(ui::Orientation::Horizontal, 6 * k);
    hotHead.add<ui::cLabel>("Hotbar", "hud");
    inventoryHover_ = &hotHead.add<ui::cLabel>(" ", "hud_small");
    inventoryHover_->setCentered(true);
    {
      ui::Layout lay;
      lay.flex = 1;
      inventoryHover_->setLayout(lay);
    }
    auto& hotGrid = rightCol.add<ui::cGridPanel>(18, ui::Vec2{30 * k, 30 * k}, 3 * k);
    hotGrid.setId("creative-hotbar");
    hotGrid.setFitWidth(true);
    byFlat[16] = &fillSlot(hotGrid, 16, "creative-hand-l", {30 * k, 30 * k});
    for (int i = 0; i < kInvHotbarSlots; ++i) byFlat[size_t(i)] = &fillSlot(hotGrid, i, "creative-hotbar-slot", {30 * k, 30 * k});
    byFlat[17] = &fillSlot(hotGrid, 17, "creative-hand-r", {30 * k, 30 * k});
    // Hand labels under the hand cells, anchored at the cells' centres
    // (1/36 and 35/36 of the row), "keys 1-8" centred beneath.
    auto& hotLabels = rightCol.add<ui::cWidget>();
    hotLabels.setLayout(ui::Layout::sized({0, 15 * k}));
    auto& lhLab = hotLabels.add<ui::cLabel>("Left Hand", "hud_small");
    lhLab.setCentered(true);
    lhLab.setLayout(ui::Layout::at({1.f / 36.f, 0}, {0.5f, 0}));
    auto& keysLab = hotLabels.add<ui::cLabel>("keys 1-8", "hud_small");
    keysLab.setCentered(true);
    keysLab.setColor({0.62f, 0.66f, 0.75f, 1.f});
    keysLab.setLayout(ui::Layout::at({0.5f, 0}, {0.5f, 0}));
    auto& rhLab = hotLabels.add<ui::cLabel>("Right Hand", "hud_small");
    rhLab.setCentered(true);
    rhLab.setLayout(ui::Layout::at({35.f / 36.f, 0}, {0.5f, 0}));
  }
  // carrySlots_ must be the flat order for refreshCarry: rebuild it from
  // byFlat (fillSlot pushed in layout order above).
  carrySlots_.clear();
  for (int i = 0; i < kTotalSlots; ++i)
    if (byFlat[size_t(i)]) carrySlots_.push_back(byFlat[size_t(i)]);
  inventory_ = &dlg;  // refreshCarry needs it set
  rebuildCreativePalette();
  refreshCarry(carry);
  dlg.on("close", [this](ui::cWidget&) {
    inventory_ = nullptr;
    inventoryHover_ = nullptr;
    carrySlots_.clear();
    clearCursorDisplay();  // display only: the game runs the return chain off inventoryClosed
    creativeCap_ = nullptr;
    agentHealth_ = agentKharge_ = agentStatic_ = nullptr;
    agentHealthVal_ = agentKhargeVal_ = agentStaticVal_ = nullptr;
    paletteHost_ = nullptr;
    creativeTabBtns_.fill(nullptr);
    if (cb_.inventoryClosed) cb_.inventoryClosed();
  });
  inventory_ = &dlg;
  // The dialog's "close" handler nulls the creative-screen pointers when
  // the close EMITS (Escape / title-bar X). A game-asked closeModal emits
  // nothing (see closeInventory), so closeInventory nulls them itself...
  // both paths leave the pointers dead before the tree can be re-shown.
}

void cGameUi::openPause() {
  if (pause_) return;
  ui::cDialog& dlg = ui::cDialog::show(ui_, "Paused");
  dlg.setId("pause");
  dlg.addButton("Resume").setId("pause-resume");  // closes -> "close" -> pauseClosed
  dlg.addButton("Settings", [this] { if (cb_.openSettings) cb_.openSettings(); }, false).setId("pause-settings");
  dlg.addButton("Save world", [this] { if (cb_.saveWorld) cb_.saveWorld(); }, false).setId("pause-save");  // 2026-10-31 (JD)
  dlg.addButton("Load world", [this] { if (cb_.loadWorld) cb_.loadWorld(); }, false).setId("pause-load");  // 2026-10-31 (JD)
  dlg.addButton("Exit to menu", [this] { if (cb_.exitToMenu) cb_.exitToMenu(); }, false).setId("pause-exit-menu");
  dlg.addButton("Quit to desktop", [this] { if (cb_.quit) cb_.quit(); }).setId("pause-quit");
  dlg.on("close", [this](ui::cWidget&) {
    pause_ = nullptr;
    if (cb_.pauseClosed) cb_.pauseClosed();
  });
  pause_ = &dlg;
}

void cGameUi::openMenu(const std::string& version) {
  if (menu_) return;
  version_ = version;
  const float k = std::max(1.f, s());
  menuScale_ = k;
  ui::cWidget& m = ui_.root().add<ui::cWidget>();
  m.setId("menu");
  m.setLayout(ui::Layout::fill());
  auto& center = m.add<ui::cStackPanel>(ui::Orientation::Vertical, 10 * k);
  center.setId("menu-center");
  center.setLayout(ui::Layout::at({0.5f, 0.44f}, {0.5f, 0.5f}));
  auto& title = center.add<ui::cLabel>("LITHOKRAFT", "title");
  title.setId("menu-title");
  title.setCentered(true);
  auto& tagline = center.add<ui::cLabel>("A LITHOLATTICE GAME", "menu_tagline");  // 2026-10-16: contrast plate
  tagline.setId("menu-tagline");
  tagline.setCentered(true);
  tagline.setColor({0.85f, 0.92f, 1.f, 1.f});  // 2026-10-16: was {0.55,0.65,0.8} (low contrast)
  center.add<ui::cDivider>(ui::Orientation::Horizontal, 2.f * k).setId("menu-divider");
  auto& play = center.add<ui::cButton>("Play");
  play.setId("menu-play");
  play.setLayout(ui::Layout::sized({600 * k, 52 * k}));
  play.on("click", [this](ui::cWidget&) { if (cb_.play) cb_.play(); });
  auto& settings = center.add<ui::cButton>("Settings");
  settings.setId("menu-settings");
  settings.setLayout(ui::Layout::sized({600 * k, 52 * k}));
  settings.on("click", [this](ui::cWidget&) { if (cb_.openSettings) cb_.openSettings(); });
  auto& exit = center.add<ui::cButton>("Exit");
  exit.setId("menu-exit");
  exit.setLayout(ui::Layout::sized({600 * k, 52 * k}));
  exit.on("click", [this](ui::cWidget&) { if (cb_.quit) cb_.quit(); });
  auto& footerL = m.add<ui::cLabel>("v" + version_);
  footerL.setId("menu-version");
  footerL.setLayout(ui::Layout::at({0, 1}, {0, 1}, {12 * k, -12 * k}));
  auto& footerR = m.add<ui::cLabel>("singleplayer");
  footerR.setId("menu-mode");
  footerR.setLayout(ui::Layout::at({1, 1}, {1, 1}, {-12 * k, -12 * k}));
  menu_ = &m;
  if (settings_) m.setVisible(false);  // 2026-10-16: a rebuild under an open Settings dialog stays hidden
  ui_.setFocus(&play);
}

void cGameUi::closeMenu() {
  if (!menu_) return;
  ui::cWidget* m = menu_;
  menu_ = nullptr;
  ui_.destroyLater(ui_.root().removeChild(*m));
}

void cGameUi::rebuildMenu() {
  closeMenu();
  openMenu(version_);
}

void cGameUi::openSettings(const GameSettings& cur) {
  if (settings_) return;
  const float k = std::max(1.f, s());
  auto state = std::make_shared<GameSettings>(cur);
  ui::cDialog& dlg = ui::cDialog::show(ui_, "Settings");
  dlg.setId("settings");
  auto& tabs = dlg.body().add<ui::cTabView>();
  tabs.setId("settings-tabs");

  // One vertical page per tab; each row is a fixed-width label plus the control.
  auto page = [&tabs](const char* name) -> ui::cStackPanel& {
    auto p = std::make_unique<ui::cStackPanel>(ui::Orientation::Vertical, 8.f);
    ui::cStackPanel* raw = p.get();
    tabs.addTab(name, std::move(p));
    return *raw;
  };
  auto row = [k](ui::cStackPanel& pg, const char* name) -> ui::cStackPanel& {
    auto& r = pg.add<ui::cStackPanel>(ui::Orientation::Horizontal, 8.f);
    r.setStretch(false);
    r.add<ui::cLabel>(name).setLayout(ui::Layout::sized({250 * k, 0}));
    return r;
  };
  auto changed = [this, state] {
    if (cb_.settingsChanged) cb_.settingsChanged(*state);
  };

  // ---- Audio: the engine's synth buses through the master gain.
  {
    ui::cStackPanel& pg = page("Audio");
    ui::cStackPanel& vr = row(pg, "Master volume");
    ui::cSlider& vol = vr.add<ui::cSlider>(0.f, 100.f, cur.masterVolume * 100.f, 1.f);
    vol.setId("set-volume");
    ui::cLabel& volText = vr.add<ui::cLabel>();
    volText.setId("set-volume-text");
    auto showVol = [&volText](float v) {
      char buf[16];
      std::snprintf(buf, sizeof buf, "%d%%", int(v + 0.5f));
      volText.setText(buf);
    };
    showVol(vol.value());
    vol.on("change", [changed, state, showVol, &volText](ui::cWidget& w) {
      const float v = static_cast<ui::cSlider&>(w).value();
      state->masterVolume = v / 100.f;
      showVol(v);
      changed();
    });
    ui::cCheckbox& mute = pg.add<ui::cCheckbox>("Mute audio", cur.muted);
    mute.setId("set-mute");
    mute.on("change", [changed, state](ui::cWidget& w) {
      state->muted = static_cast<ui::cCheckbox&>(w).checked();
      changed();
    });
  }

  // ---- Video: the window (display mode live, vsync at boot) and the UI scale.
  {
    ui::cStackPanel& pg = page("Video");
    ui::cStackPanel& dr = row(pg, "Display mode");
    const std::vector<std::string> dispItems{kSettingsDisplayModes, kSettingsDisplayModes + kSettingsDisplayModeCount};
    ui::cDropdown& disp = dr.add<ui::cDropdown>(dispItems, cur.displayMode);
    disp.setId("set-display");
    disp.on("change", [changed, state](ui::cWidget& w) {
      state->displayMode = static_cast<ui::cDropdown&>(w).selected();
      changed();
    });
    ui::cCheckbox& vsync = pg.add<ui::cCheckbox>("VSync (applies on restart)", cur.vsync);
    vsync.setId("set-vsync");
    vsync.on("change", [changed, state](ui::cWidget& w) {
      state->vsync = static_cast<ui::cCheckbox&>(w).checked();
      changed();
    });
    {  // 2026-10-24 (JD): frame cap + buffering
      ui::cStackPanel& fr = row(pg, "Frame cap");
      const std::vector<std::string> capItems{"30", "60", "Uncapped", "Auto"};
      const int capSel = cur.fpsCap == 60 ? 1 : cur.fpsCap == 0 ? 2 : cur.fpsCap < 0 ? 3 : 0;
      ui::cDropdown& cap = fr.add<ui::cDropdown>(capItems, capSel);
      cap.setId("set-fpscap");
      cap.on("change", [changed, state](ui::cWidget& w) {
        static const int kCaps[4] = {30, 60, 0, -1};
        state->fpsCap = kCaps[std::clamp(static_cast<ui::cDropdown&>(w).selected(), 0, 3)];
        changed();
      });
      ui::cStackPanel& br = row(pg, "Buffering (Vulkan, on restart)");
      const std::vector<std::string> bufItems{"Double (2)", "Triple (3)"};
      ui::cDropdown& buf = br.add<ui::cDropdown>(bufItems, cur.swapImages >= 3 ? 1 : 0);
      buf.setId("set-buffering");
      buf.on("change", [changed, state](ui::cWidget& w) {
        state->swapImages = static_cast<ui::cDropdown&>(w).selected() == 1 ? 3 : 2;
        changed();
      });
    }
    {  // 2026-10-24 (JD): floor pulse Off / Slow / Normal (render only, live)
      ui::cStackPanel& fp = row(pg, "Floor pulse");
      const std::vector<std::string> fpItems{"Off", "Slow", "Normal"};
      ui::cDropdown& fpd = fp.add<ui::cDropdown>(fpItems, std::clamp(cur.floorPulse, 0, 2));
      fpd.setId("set-floorpulse");
      fpd.on("change", [changed, state](ui::cWidget& w) {
        state->floorPulse = std::clamp(static_cast<ui::cDropdown&>(w).selected(), 0, 2);
        changed();
      });
    }
    ui::cStackPanel& hr = row(pg, "HUD scale");
    const std::vector<std::string> hudItems{"Auto", "1x", "2x", "3x", "4x"};  // Cortana 2026-10-16: GUI scale 1-4
    ui::cDropdown& hud = hr.add<ui::cDropdown>(hudItems, cur.hudScale);
    hud.setId("set-hud");
    hud.on("change", [changed, state](ui::cWidget& w) {
      state->hudScale = static_cast<ui::cDropdown&>(w).selected();
      changed();
    });
    // PBR phase 2 (Worker 3): BinaryQuad/PackedVertex coexistence. Boot-only like the
    // tick radius - switching mesh formats needs a world (re)load.
    ui::cStackPanel& mr = row(pg, "Mesher (restart)");
    const std::vector<std::string> mesherItems{kSettingsMesherNames, kSettingsMesherNames + kSettingsMesherCount};
    ui::cDropdown& mesher = mr.add<ui::cDropdown>(mesherItems, cur.mesher);
    mesher.setId("set-mesher");
    mesher.on("change", [changed, state](ui::cWidget& w) {
      state->mesher = static_cast<ui::cDropdown&>(w).selected();
      changed();
    });
    // PBR phase 2 (Worker 4): the PBR transparency tier applies live via
    // setPbrTier on the active renderer - no restart needed, unlike the mesher
    // above. The tier ladder is independent of GraphicsTier by design (JD locked
    // all four tiers user-selectable); the hint only suggests a pairing.
    ui::cStackPanel& pr = row(pg, "PBR transparency (live)");
    const std::vector<std::string> pbrItems{kSettingsPbrTierNames,
                                           kSettingsPbrTierNames + kSettingsPbrTierCount};
    ui::cDropdown& pbr = pr.add<ui::cDropdown>(pbrItems, cur.pbrTier);
    pbr.setId("set-pbrtier");
    pbr.on("change", [changed, state](ui::cWidget& w) {
      state->pbrTier = static_cast<ui::cDropdown&>(w).selected();
      changed();
    });
    ui::cLabel& pbrHint =
        pg.add<ui::cLabel>("Pairs well with GraphicsTier High/Ultra; independent setting.");
    pbrHint.setId("pbr-tier-hint");
  }

  // ---- Graphics: Cortana 2026-10-16 (JD): every software ("Litho") renderer fidelity
  // option, so software-only players can pick pretty vs fast. GL33/Vulkan ignore these.
  {
    ui::cStackPanel& pg = page("Graphics");
    pg.add<ui::cLabel>("Software renderer quality (Auto = 30+ fps on 4-core machines)");
    auto drop = [&](const char* name, const char* id, std::vector<std::string> items, int sel, auto apply) {
      ui::cStackPanel& r = row(pg, name);
      ui::cDropdown& d = r.add<ui::cDropdown>(items, sel);
      d.setId(id);
      d.on("change", [changed, state, apply](ui::cWidget& w) { apply(*state, static_cast<ui::cDropdown&>(w).selected()); changed(); });
    };
    auto box = [&](const char* name, const char* id, bool on, auto apply) {
      ui::cCheckbox& c = pg.add<ui::cCheckbox>(name, on);
      c.setId(id);
      c.on("change", [changed, state, apply](ui::cWidget& w) { apply(*state, static_cast<ui::cCheckbox&>(w).checked()); changed(); });
    };
    // JD 2026-10-24: renderer switch. Saves the world and relaunches the game with the new backend.
    const int rSel = cur.renderer == "gl33" ? 1 : cur.renderer == "vulkan" ? 2 : 0;
    drop("Renderer (saves the world and restarts)", "set-renderer", {"Software", "OpenGL 3.3", "Vulkan"}, rSel,
         [](GameSettings& s, int i) { s.renderer = i == 1 ? "gl33" : i == 2 ? "vulkan" : "software"; });
    const int texSel = cur.texRes == 16 ? 1 : cur.texRes == 32 ? 2 : cur.texRes == 64 ? 3 : cur.texRes == 128 ? 4 : 0;
    // 2026-10-24 (JD): 128 = the HD pack on OpenGL 3.3 / Vulkan (Auto picks it too); applies on restart.
    // The software renderer stays capped at 64.
    drop("Texture resolution (F9; 128 = GPU, on restart)", "set-texres", {"Auto", "16", "32", "64", "128"}, texSel,
         [](GameSettings& s, int i) { s.texRes = i == 1 ? 16 : i == 2 ? 32 : i == 3 ? 64 : i == 4 ? 128 : 0; });
    box("Distance mipmaps (no far sparkle)", "set-mipmaps", cur.mipmaps, [](GameSettings& s, bool v) { s.mipmaps = v; });
    const int aaSel = cur.aaMode == "fxaa" ? 1 : cur.aaMode == "cmaa" ? 2 : cur.aaMode == "ssaa2x" ? 3 : 0;
    drop("Anti-aliasing", "set-aa", {"Off", "FXAA", "CMAA", "SSAA 2x"}, aaSel,
         [](GameSettings& s, int i) { s.aaMode = i == 1 ? "fxaa" : i == 2 ? "cmaa" : i == 3 ? "ssaa2x" : "off"; });
    // PROVISIONAL (JD 2026-10-24): texture-filtering stand-ins. OpenGL 3.3 / Vulkan only.
    if (cur.renderer == "gl33" || cur.renderer == "vulkan") {
      drop("Texture filter (provisional)", "set-tex-filter", {"Sharp", "Bilinear", "Trilinear"}, std::clamp(cur.texFilter, 0, 2),
           [](GameSettings& s, int i) { s.texFilter = i; });
      const int afSel = cur.anisotropy >= 16 ? 4 : cur.anisotropy >= 8 ? 3 : cur.anisotropy >= 4 ? 2 : cur.anisotropy >= 2 ? 1 : 0;
      drop("Anisotropic filtering (provisional, capped at GPU max)", "set-anisotropy", {"Off", "2x", "4x", "8x", "16x"}, afSel,
           [](GameSettings& s, int i) { s.anisotropy = i == 0 ? 1 : 1 << i; });
    } else {
      pg.add<ui::cLabel>("Texture filter / anisotropic filtering: OpenGL 3.3 and Vulkan only (provisional)").setId("tex-filter-note");
    }
    drop("Bloom", "set-bloom", {"Auto", "Off", "On"}, cur.bloom < 0 ? 0 : cur.bloom == 0 ? 1 : 2,
         [](GameSettings& s, int i) { s.bloom = i == 0 ? -1 : i == 1 ? 0 : 1; });
    box("Corner ambient occlusion", "set-ao", cur.ao, [](GameSettings& s, bool v) { s.ao = v; });
    box("Trace lithography shimmer", "set-shimmer", cur.shimmer, [](GameSettings& s, bool v) { s.shimmer = v; });
    drop("Trace Bulb shell", "set-bulb-shell", {"Ghostly", "Solid"}, cur.bulbShellSolid ? 1 : 0,  // 1551 (JD): all renderers
         [](GameSettings& s, int i) { s.bulbShellSolid = i == 1; });
    box("Emissive block light", "set-emissive-light", cur.emissiveLight, [](GameSettings& s, bool v) { s.emissiveLight = v; });
    // JD 2026-10-16: world streaming and render / stream distance (boot-only).
    ui::cCheckbox& sc = pg.add<ui::cCheckbox>("World streaming: generate ahead of you (restart)", cur.streaming);
    sc.setId("set-streaming");
    sc.on("change", [changed, state](ui::cWidget& w) { state->streaming = static_cast<ui::cCheckbox&>(w).checked(); changed(); });
    ui::cStackPanel& rr = row(pg, "Render / stream distance, chunks (0 = auto: Vulkan 64, others 32; restart)");
    ui::cNumberField& nf = rr.add<ui::cNumberField>(0, 64, cur.streamRadius);
    nf.setId("set-stream-radius");
    nf.on("change", [changed, state](ui::cWidget& w) { state->streamRadius = static_cast<ui::cNumberField&>(w).value(); changed(); });
    // Archie Phase 1 (JD 2026-11-04): far-LOD detail slider (software renderer; live, never beyond the view).
    ui::cStackPanel& lr = row(pg, "Far LOD detail (0 off, 1 high, 2 balanced, 3 fast)");
    ui::cSlider& ls = lr.add<ui::cSlider>(0.f, 3.f, float(cur.lodDetail), 1.f);
    ls.setId("set-lod");
    ls.on("change", [changed, state](ui::cWidget& w) { state->lodDetail = int(static_cast<ui::cSlider&>(w).value() + 0.5f); changed(); });
    // JD Vulkan push (Cortana 2026-10-16): one toggle per Vulkan effect (live; GL33 / software keep their look).
    pg.add<ui::cLabel>("Vulkan effects (live; OpenGL / software unchanged)");
    struct FxBox { const char* name; const char* id; int bit; };
    static const FxBox kFxBoxes[] = {{"Normal maps (etched blocks, bulbs)", "set-vk-normal", 1},
                                     {"Per-pixel lighting", "set-vk-lighting", 2},
                                     {"HDR bloom (glow masks)", "set-vk-bloom", 4},
                                     {"Pulsing glow (tick-synced)", "set-vk-pulse", 8},
                                     {"GPU particles (Trace sparks)", "set-vk-particles", 16},
                                     {"Distance fog", "set-vk-fog", 32},
                                     {"Blended Trace Bulb glass", "set-vk-bulb-blend", 64}};
    for (const FxBox& fb : kFxBoxes) {
      const int bit = fb.bit;
      box(fb.name, fb.id, (cur.vkFx & bit) != 0, [bit](GameSettings& s, bool v) { s.vkFx = v ? (s.vkFx | bit) : (s.vkFx & ~bit); });
    }
    // 0.0.1.54 [C54-1] (Proposed by JD, Built by Cortana): far LOD (2x for chunks 33-48, 4x beyond; render-only).
    box("Far LOD (chunks 33-64)", "set-vk-far-lod", cur.vkFarLod, [](GameSettings& s, bool v) { s.vkFarLod = v; });
    // F56-4 (Proposed by JD, Built by Cortana): F3-style perf readout, off by default (live).
    box("Performance readout (FPS, frame / update / GPU ms)", "set-perf-readout", cur.fpsCounter, [](GameSettings& s, bool v) { s.fpsCounter = v; });
  }

  // ---- Sim: the tick rate and the EUV domain apply live; the tick radius at boot.
  {
    ui::cStackPanel& pg = page("Sim");
    ui::cStackPanel& tr = row(pg, "Tick rate (live)");
    std::vector<std::string> tpsItems;
    int tpsSel = 0;
    for (int i = 0; i < kSettingsTpsModeCount; ++i) {
      tpsItems.push_back(std::to_string(kSettingsTpsModes[i]));
      if (kSettingsTpsModes[i] == cur.tps) tpsSel = i;
    }
    ui::cDropdown& tps = tr.add<ui::cDropdown>(tpsItems, tpsSel);
    tps.setId("set-tps");
    tps.on("change", [changed, state](ui::cWidget& w) {
      state->tps = kSettingsTpsModes[static_cast<ui::cDropdown&>(w).selected()];
      changed();
    });
    ui::cStackPanel& rr = row(pg, "Simulation distance, chunks (restart)");
    ui::cNumberField& radius = rr.add<ui::cNumberField>(0, 512, cur.tickRadius);
    radius.setId("set-radius");
    radius.on("change", [changed, state](ui::cWidget& w) {
      state->tickRadius = static_cast<ui::cNumberField&>(w).value();
      changed();
    });
    {  // F56-7 (Proposed by JD, Built by Cortana): round sim area
      ui::cCheckbox& rnd = pg.add<ui::cCheckbox>("Round simulation area (restart)", cur.roundSimArea);
      rnd.setId("set-round-sim");
      rnd.on("change", [changed, state](ui::cWidget& w) { state->roundSimArea = static_cast<ui::cCheckbox&>(w).checked(); changed(); });
    }
    ui::cCheckbox& euv = pg.add<ui::cCheckbox>("EUV domain (live)", cur.euv);
    euv.setId("set-euv");
    euv.on("change", [changed, state](ui::cWidget& w) {
      state->euv = static_cast<ui::cCheckbox&>(w).checked();
      changed();
    });
  }

  dlg.addButton("Close").setId("settings-close");
  // Cortana 2026-10-16 (standards review): hide the launch menu while Settings is open (its
  // buttons bled around the dialog); shown again when the dialog closes.
  if (menu_) menu_->setVisible(false);
  dlg.on("close", [this](ui::cWidget&) { settings_ = nullptr; if (menu_) menu_->setVisible(true); });
  settings_ = &dlg;
}

void cGameUi::closeSettings() {
  if (!settings_) return;
  ui::cDialog* d = settings_;
  settings_ = nullptr;  // closeModal emits no "close": no callback for a close the game asked for
  ui_.closeModal(*d);
  if (menu_) menu_->setVisible(true);  // 2026-10-16: restore the menu hidden by openSettings
}

void cGameUi::closePause() {
  if (!pause_) return;
  ui::cDialog* d = pause_;
  pause_ = nullptr;
  ui_.closeModal(*d);
}

void cGameUi::closeInventory() {
  if (!inventory_) return;
  ui::cDialog* d = inventory_;
  inventory_ = nullptr;  // close() emits "close": no callback for a close the game asked for
  inventoryHover_ = nullptr;
  carrySlots_.clear();
  clearCursorDisplay();  // display only: Game::setInventoryOpen ran the return chain before this call
  // Null the creative-screen pointers BEFORE closeModal, mirroring the
  // dialog's own "close" handler: closeModal emits no "close", so
  // without this the Agent bar / palette pointers kept naming widgets
  // of a closed screen (found 2026-11-09: the paint-ruling selftest
  // closed the screen and the following update() still wrote the bars,
  // which also kept the F3 readout panel's inventory gate stale in
  // test order). The dialog object itself is retained by the modal
  // stack; the pointers are re-seated by openInventory on the next open.
  creativeCap_ = nullptr;
  agentHealth_ = agentKharge_ = agentStatic_ = nullptr;
  agentHealthVal_ = agentKhargeVal_ = agentStaticVal_ = nullptr;
  paletteHost_ = nullptr;
  creativeTabBtns_.fill(nullptr);
  ui_.closeModal(*d);
}  // NOTE (2026-11-09): the dialog widget tree OUTLIVES a game-asked close
   // (the modal stack retains it), so the creative-screen pointers (Agent
   // bars, palette host) stay valid here... unlike an Escape close, whose
   // "close" handler nulls them. Do not "tidy" this by nulling them:
   // a game-asked close can be followed by the screen being re-shown.

namespace {
// next-build-2 GUI phase timing (measure-only; see game_ui.h GuiFrameTimes).
double guiMsBetween(std::chrono::steady_clock::time_point a, std::chrono::steady_clock::time_point b) {
  return std::chrono::duration<double, std::milli>(b - a).count();
}
}  // namespace

void cGameUi::update(const HudModel& m, ui::Vec2 viewport, float dt) {
  const auto guiT0 = std::chrono::steady_clock::now();  // model phase: this fan-out
  const float k = float(effectiveGuiScale(hudScalePref_, viewport.x, viewport.y));
  const float hs = hotbarScaleFor(viewport.x, k);  // 2026-10-25: re-lay the hotbar when the cap changes
  if (k != scale_ || hs != hotbarScale_) {
    if (k != scale_) {
      ui::Theme t = ui_.theme();
      t.setScale(k);
      setDebugStyle(t, float(effectiveGuiScale(0, viewport.x, viewport.y)));
      ui_.setTheme(std::move(t));
    }
    scale_ = k;
    hotbarScale_ = hs;
    rebuildHud();
  }
  if (menu_ && k != menuScale_) rebuildMenu();
  hud_->setVisible(m.visible);
  age_ += dt;
  splash_->setVisible(age_ < kSplashSeconds);
  debugPanel_->setVisible(!m.debugLines.empty() && !menu_ && !inventory_);  // 2026-10-31 (JD): hidden under the inventory  // Playtest 1 #5: not over the main menu
  {  // split around the "look: " line (red); with no look line everything stays in debugText_
    std::vector<std::string> pre, post;
    std::string look;
    bool seen = false;
    for (const std::string& ln : m.debugLines) {
      if (!seen && ln.rfind("look: ", 0) == 0) { look = ln; seen = true; }
      else (seen ? post : pre).push_back(ln);
    }
    debugText_->setText(joinLines(pre));
    debugLook_->setText(look);
    debugLook_->setVisible(seen);  // after the panel's own flip above: membership already settled, this routes dynamic
    debugTextPost_->setText(joinLines(post));
    debugTextPost_->setVisible(!post.empty());
  }
  tickPanel_->setVisible(!m.tickLine.empty() && !menu_);  // visibility before the text below: a first show then paints once, not twice
  if (!m.tickLine.empty()) {
    static const ui::Color kLv[4] = {{0.35f, 1.f, 0.45f, 1.f}, {1.f, 0.85f, 0.25f, 1.f}, {1.f, 0.3f, 0.3f, 1.f}, {0.75f, 0.8f, 0.9f, 1.f}};  // 2026-11-05 Neutral=paused
    tickText_->setText(m.tickLine);
    tickText_->setColor(kLv[m.tickLevel < 0 ? 0 : m.tickLevel > 3 ? 3 : m.tickLevel]);
  }
  controlsText_->setText(joinLines(m.controlLines));
  if (agentHealth_) {  // Item 4: the creative screen's Agent bars follow the model
    auto bar = [](ui::cProgressBar* b, ui::cLabel* val, int v, int mx) {
      b->setValue(mx > 0 ? float(v) / float(mx) : 0.f);
      if (val) val->setText(std::to_string(v) + "\\" + std::to_string(mx));
    };
    bar(agentHealth_, agentHealthVal_, m.vitHealth, m.vitHealthMax);
    bar(agentKharge_, agentKhargeVal_, m.vitKharge, m.vitKhargeMax);
    agentStatic_->setValue(1.f);
    if (agentStaticVal_) agentStaticVal_->setText(std::to_string(m.vitStatic));
  }
  banner_->setVisible(!m.bannerLines.empty());
  bannerTitle_->setText(m.bannerLines.empty() ? std::string() : m.bannerLines.front());
  bannerText_->setText(m.bannerLines.size() > 1 ? m.bannerLines[1] : std::string());
  postView_->setVisible(m.postViewLines.size() >= 2);
  if (m.postViewLines.size() >= 2) {
    postViewTitle_->setText(m.postViewLines.front());
    std::vector<std::string> body(m.postViewLines.begin() + 1, m.postViewLines.end() - 1);
    postViewText_->setText(joinLines(body));
    postViewHint_->setText(m.postViewLines.back());
  }
  for (int i = 0; i < kHotbarSlots; ++i) {
    ui::cSlot& slot = *slots_[size_t(i)];
    const ItemStack& st = m.hotbar[size_t(i)];
    // Real stacks (P2b-1): block-backed stacks render as blocks, tools and
    // the Fragment render with their own item icons -- the Fragment in
    // slot 0 is finally visible in the HUD.
    slot.setIcon(itemIconRef(st.itemId));
    slot.setSelected(i == m.selected);
    // 2026-11-08 (WS3): hotbar slots carry tooltip text like the palette
    // and carry slots always did; the facade's hover tooltip driver is
    // what finally displays it (plus palette and carry hovers).
    const std::string tip = st.empty() ? std::string()
                                : st.count > 1 ? itemDisplayName(st.itemId) + " x" + std::to_string(st.count)
                                               : itemDisplayName(st.itemId);
    if (slot.tooltip() != tip) slot.setTooltip(tip);
  }
  {
    const ItemStack& sel = m.hotbar[size_t(std::clamp(m.selected, 0, kHotbarSlots - 1))];
    std::string name = itemDisplayName(sel.itemId);
    if (!sel.empty() && sel.count > 1) name += " x" + std::to_string(sel.count);
    slotName_->setText(name);
  }
  if (lookName_) {
    lookName_->setText(m.lookName);
    lookName_->setVisible(!m.lookName.empty());
  }
  if (inventoryHover_) {
    const auto* over = dynamic_cast<const ui::cSlot*>(ui_.hover());
    const bool inInventory = over && ui_.topModal() == inventory_;
    std::string hover = " ";
    if (inInventory && over->data() >= kCarryData) {  // playtest 2026-10-16: carry grid slot
      const size_t i = size_t(over->data() - kCarryData);
      if (i < carry_.size() && !carry_[i].empty())
        hover = itemDisplayName(carry_[i].itemId) + " x" + std::to_string(carry_[i].count);
      else hover = "(empty)";
    } else if (inInventory) {
      hover = blockDescription(BlockId(over->data()));
    }
    inventoryHover_->setText(hover);
  }
  const auto guiT1 = std::chrono::steady_clock::now();
  ui_.update(dt);
  const auto guiT2 = std::chrono::steady_clock::now();
  ui_.layout(viewport);
  // Cursor stack (2026-11-11): seat the carried cell on the polled
  // pointer AFTER layout, by direct placement. place() re-seats the
  // widget's rect and arranges its child; it does not advance any
  // paint generation (verified against the facade: setLayout/place
  // are generation-free, and cUiRoot::layout is paintGen-gated), so a
  // pointer move re-expands ONLY the dynamic layer that the cell
  // rides. The cell is centred on the pointer like the reference.
  if (cursorHost_ && cursorHost_->visible() && ui_.pointerSeen()) {
    const ui::Vec2 p = ui_.lastPointer();
    const float half = cursorCell_ * 0.5f;
    cursorHost_->place({p.x - half, p.y - half, cursorCell_, cursorCell_}, ui_.theme());
  }
  const auto guiT3 = std::chrono::steady_clock::now();
  guiTimes_.modelMs = guiMsBetween(guiT0, guiT1);
  guiTimes_.updateMs = guiMsBetween(guiT1, guiT2);
  guiTimes_.layoutMs = guiMsBetween(guiT2, guiT3);
}

void cGameUi::paint(HudLayer& hud, const BlockMaterials& materials, const std::function<const Image*(BlockId)>& icon,
                   const std::function<const Image*(uint32_t)>& itemIcon) const {
  const auto paintT0 = std::chrono::steady_clock::now();
  // Litho GUI migration: cUiRoot::paint takes texture-id resolvers (the
  // draw list's image quads carry ids); the game's resolvers are name-based,
  // so translate id -> name through the facade registry and run the exact
  // old logic. The slot tint multiplies the swatch (the icon path of the
  // HUD renderer has no tint channel; tints there are the identity in
  // practice -- block/item icons are drawn untinted).
  ui_.paint(hud, [&materials](int textureId) -> std::optional<ui::Color> {
    const std::string& image = ui::iconTextureName(textureId);
    constexpr std::string_view kPrefix = "block:";
    if (image.substr(0, kPrefix.size()) != kPrefix) return std::nullopt;
    int id = 0;
    const std::string_view num = std::string_view(image).substr(kPrefix.size());
    if (std::from_chars(num.data(), num.data() + num.size(), id).ec != std::errc() || id <= 0 ||
        id >= int(blockRegistry().size()))
      return std::nullopt;
    const auto& c = materials.swatch(BlockId(id));
    const ui::Color tint = ui::iconTint(textureId);
    return ui::Color{c[0] * tint.r, c[1] * tint.g, c[2] * tint.b, tint.a};
  }, [&icon, &itemIcon](int textureId) -> const Image* {
    const std::string& image = ui::iconTextureName(textureId);
    constexpr std::string_view kItemPrefix = "item:";
    if (image.substr(0, kItemPrefix.size()) == kItemPrefix) {
      // Non-block item icon (a tool, the Fragment): the item icon atlas.
      if (!itemIcon) return nullptr;
      int id = 0;
      const std::string_view num = std::string_view(image).substr(kItemPrefix.size());
      if (std::from_chars(num.data(), num.data() + num.size(), id).ec != std::errc() || id <= 0)
        return nullptr;
      return itemIcon(uint32_t(id));
    }
    constexpr std::string_view kPrefix = "block:";
    if (!icon || image.substr(0, kPrefix.size()) != kPrefix) return nullptr;
    int id = 0;
    const std::string_view num = std::string_view(image).substr(kPrefix.size());
    if (std::from_chars(num.data(), num.data() + num.size(), id).ec != std::errc() || id <= 0 || id >= int(blockRegistry().size()))
      return nullptr;
    return icon(BlockId(id));  // block icon image; the swatch above is the fallback
  });
  guiTimes_.paintMs = guiMsBetween(paintT0, std::chrono::steady_clock::now());
  guiTimes_.paintRebuilt = ui_.paintRebuilt();
  guiTimes_.paintDraws = ui_.paintDrawCount();
  guiTimes_.paintRects = ui_.paintRectCount();
  guiTimes_.paintTexts = ui_.paintTextCount();
  guiTimes_.paintDrawsOnly = ui_.paintDrawOnlyCount();
  guiTimes_.paintIcons = ui_.paintIconCount();
  guiTimes_.dynRebuilt = ui_.dynamicRebuilt();
  guiTimes_.dynDraws = ui_.dynamicDrawCount();
}

void cGameUi::paintSplit(HudLayer& hud, litholattice::HudLayerSplit& split, const BlockMaterials& materials,
                         const std::function<const Image*(BlockId)>& icon,
                         const std::function<const Image*(uint32_t)>& itemIcon) const {
  // Same resolver translation as paint(), but through the facade's
  // split entry so the stream refs ride along (cure 1, 2026-11-10).
  const auto paintT0 = std::chrono::steady_clock::now();
  ui_.paintSplit(hud, split, [&materials](int textureId) -> std::optional<ui::Color> {
    const std::string& image = ui::iconTextureName(textureId);
    constexpr std::string_view kPrefix = "block:";
    if (image.substr(0, kPrefix.size()) != kPrefix) return std::nullopt;
    int id = 0;
    const std::string_view num = std::string_view(image).substr(kPrefix.size());
    if (std::from_chars(num.data(), num.data() + num.size(), id).ec != std::errc() || id <= 0 ||
        id >= int(blockRegistry().size()))
      return std::nullopt;
    const auto& c = materials.swatch(BlockId(id));
    const ui::Color tint = ui::iconTint(textureId);
    return ui::Color{c[0] * tint.r, c[1] * tint.g, c[2] * tint.b, tint.a};
  }, [&icon, &itemIcon](int textureId) -> const Image* {
    const std::string& image = ui::iconTextureName(textureId);
    constexpr std::string_view kItemPrefix = "item:";
    if (image.substr(0, kItemPrefix.size()) == kItemPrefix) {
      if (!itemIcon) return nullptr;
      int id = 0;
      const std::string_view num = std::string_view(image).substr(kItemPrefix.size());
      if (std::from_chars(num.data(), num.data() + num.size(), id).ec != std::errc() || id <= 0)
        return nullptr;
      return itemIcon(uint32_t(id));
    }
    constexpr std::string_view kPrefix = "block:";
    if (!icon || image.substr(0, kPrefix.size()) != kPrefix) return nullptr;
    int id = 0;
    const std::string_view num = std::string_view(image).substr(kPrefix.size());
    if (std::from_chars(num.data(), num.data() + num.size(), id).ec != std::errc() || id <= 0 || id >= int(blockRegistry().size()))
      return nullptr;
    return icon(BlockId(id));
  });
  guiTimes_.paintMs = guiMsBetween(paintT0, std::chrono::steady_clock::now());
  guiTimes_.paintRebuilt = ui_.paintRebuilt();
  guiTimes_.paintDraws = ui_.paintDrawCount();
  guiTimes_.paintRects = ui_.paintRectCount();
  guiTimes_.paintTexts = ui_.paintTextCount();
  guiTimes_.paintDrawsOnly = ui_.paintDrawOnlyCount();
  guiTimes_.paintIcons = ui_.paintIconCount();
  guiTimes_.dynRebuilt = ui_.dynamicRebuilt();
  guiTimes_.dynDraws = ui_.dynamicDrawCount();
}

}  // namespace lithokraft
