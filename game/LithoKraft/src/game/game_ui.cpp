#include "lithokraft/game/game_ui.h"
#include <algorithm>
#include <charconv>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>
#include "lithokraft/content/blocks.h"
#include "lithokraft/content/items.h"  // itemDisplayName / itemIconRef (P2b-1 stack rendering)
#include "litholattice/render/hud_ui_renderer.h"
#include "litholattice/ui/containers.h"
#include "litholattice/ui/lists.h"

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
  ui_.setTheme(std::move(t));
}

void cGameUi::rebuildHud() {
  if (hud_) ui_.destroyLater(ui_.root().removeChild(*hud_));
  hud_ = &ui_.root().add<ui::cWidget>();
  hud_->setId("hud");
  const float k = s();

  debugPanel_ = &hud_->add<ui::cPanel>("hud");
  debugPanel_->setLayout(ui::Layout::at({0, 0}, {0, 0}, {8 * k, 8 * k}));
  // 2026-10-16 (JD): the look-at line renders red so it stands out; the rest is unchanged.
  auto& debugStack = debugPanel_->add<ui::cStackPanel>(ui::Orientation::Vertical, 0.f);
  debugText_ = &debugStack.add<ui::cLabel>();
  debugLook_ = &debugStack.add<ui::cLabel>();
  debugLook_->setColor({1.f, 0.25f, 0.25f, 1.f});
  debugTextPost_ = &debugStack.add<ui::cLabel>();

  controlsPanel_ = &hud_->add<ui::cPanel>("hud");
  // Cortana 2026-10-16 (standards review): sits ABOVE the hotbar + slot-name rows (it used to
  // overlap the hotbar at 2x GUI scale) and uses the compact hud_small text.
  controlsPanel_->setLayout(ui::Layout::at({0, 1}, {0, 1}, {8 * k, -(6 + 40 * kInventoryScale + 24 + 12) * k}));
  controlsText_ = &controlsPanel_->add<ui::cLabel>("", "hud_small");
  controlsText_->setColor({0.9f, 0.95f, 1.f, 1.f});

  banner_ = &hud_->add<ui::cPanel>("hud");
  banner_->setLayout(ui::Layout::at({0.5f, 0.3f}, {0.5f, 0}));
  auto& bannerStack = banner_->add<ui::cStackPanel>(ui::Orientation::Vertical, 0.f);
  bannerTitle_ = &bannerStack.add<ui::cLabel>();
  bannerTitle_->setColor({1.f, 0.85f, 0.4f, 1.f});
  bannerText_ = &bannerStack.add<ui::cLabel>();
  bannerText_->setColor({0.6f, 0.9f, 1.f, 1.f});

  splash_ = &hud_->add<ui::cLabel>("Powered by LithoLattice");
  splash_->setId("splash");
  splash_->setColor({0.75f, 0.85f, 1.f, 1.f});
  splash_->setLayout(ui::Layout::at({1, 1}, {1, 1}, {-8 * k, -8 * k}));

  auto& bar = hud_->add<ui::cStackPanel>(ui::Orientation::Horizontal, 4 * k * kInventoryScale, "hud");
  bar.setId("hotbar");
  bar.setLayout(ui::Layout::at({0.5f, 1}, {0.5f, 1}, {0, -6 * k}));
  for (int i = 0; i < kHotbarSlots; ++i) {
    ui::cSlot& slot = bar.add<ui::cSlot>(ui::Vec2{40 * k * kInventoryScale, 40 * k * kInventoryScale});  // +25% (JD 2026-10-16)
    // 2026-10-24 (JD): one row of 16; captions are the direct-select keys (1..9, 0), slots 11..16 are wheel-only.
    slot.setCaption(i < 9 ? std::to_string(i + 1) : i == 9 ? std::string("0") : std::string()).setData(i);
    slot.on("click", [this](ui::cWidget& w) {
      if (cb_.selectSlot) cb_.selectSlot(static_cast<ui::cSlot&>(w).data());
    });
    slots_[size_t(i)] = &slot;
  }
  slotName_ = &hud_->add<ui::cLabel>();
  slotName_->setLayout(ui::Layout::at({0.5f, 1}, {0.5f, 1}, {0, -(40 * kInventoryScale + 6 + 24) * k}));
  lookName_ = &hud_->add<ui::cLabel>();  // 2026-10-16 (JD): one line above the held-item name
  lookName_->setLayout(ui::Layout::at({0.5f, 1}, {0.5f, 1}, {0, -(40 * kInventoryScale + 6 + 24 + 24) * k}));
  lookName_->setColor({1.f, 1.f, 1.f, 1.f});
}

void cGameUi::openInventory(int selectedSlot) { openInventory(selectedSlot, {}); }

void cGameUi::refreshCarry(const std::vector<ItemStack>& carry) {
  if (!inventory_ || carrySlots_.empty()) return;
  carry_ = carry;
  for (size_t i = 0; i < carrySlots_.size() && i < carry_.size(); ++i) {
    const ItemStack& st = carry_[i];
    carrySlots_[i]->setIcon(st.empty() ? std::string() : itemIconRef(st.itemId));
    carrySlots_[i]->setCaption(st.empty() || st.count <= 1 ? std::string() : std::to_string(st.count));
  }
}

void cGameUi::openInventory(int selectedSlot, const std::vector<ItemStack>& carry) {
  if (inventory_) return;
  carrySlots_.clear();
  carry_.clear();
  const float k = std::max(1.f, s());
  // P2b-1 honesty: this dialog is a creative BLOCK PALETTE, not the player
  // inventory -- it lists every block in the registry and GIVES a full
  // stack of the clicked block into the player's real inventory hotbar
  // (the same inventory the HUD shows and player.lat persists).
  const std::string title = "Block palette (creative) - click: give a full stack to hotbar slot " +
                            std::to_string(selectedSlot + 1) +
                            ", shift-click: first free slot, E/Esc: close";
  ui::cDialog& dlg = ui::cDialog::show(ui_, title);
  dlg.setId("inventory");
  auto& note = dlg.body().add<ui::cLabel>(
      "Creative palette: every block in the registry. A click gives a full stack (512) "
      "into your inventory hotbar; placing blocks consumes from the stack.");
  note.setId("palette-note");
  const int cols = 9, count = int(blockRegistry().size()) - 1;  // every block type except air
  const int rows = (count + cols - 1) / cols;
  const float cell = 40 * k * kInventoryScale;  // Cortana 2026-10-16: +25% slots/icons (JD)
  auto& scroll = dlg.body().add<ui::cScrollView>();
  const float gridH = float(rows) * cell;
  scroll.setLayout(ui::Layout::sized({float(cols) * cell + ui::cScrollBar::kThickness, std::min(gridH, 5 * cell)}));
  auto& grid = scroll.emplaceContent<ui::cGridPanel>(cols, ui::Vec2{cell - 4 * k, cell - 4 * k}, 4 * k);
  for (int i = 0; i < count; ++i) {
    const BlockId b = BlockId(i + 1);
    if (isSideVia(b)) continue;  // 2026-10-25: side Vias are placement variants of the Via, not palette entries
    ui::cSlot& slot = grid.add<ui::cSlot>(ui::Vec2{cell - 4 * k, cell - 4 * k});
    slot.setIcon(blockIcon(b)).setData(int(b));
    slot.on("click", [this](ui::cWidget& w) {
      if (cb_.pickBlock) cb_.pickBlock(BlockId(static_cast<ui::cSlot&>(w).data()), (ui_.modifiers() & ui::kModShift) != 0);
    });
  }
  if (!carry.empty()) {
    // Playtest 2026-10-16: every carry slot (42 = hotbar 8 + offhand 2 + main 32), 14 per row.
    dlg.body().add<ui::cLabel>("Carry (50): hotbar 1-16, offhand L/R, main 1-32 - click: swap into hotbar slot " +
                              std::to_string(selectedSlot + 1));
    const int ccols = 14;
    auto& cgrid = dlg.body().add<ui::cGridPanel>(ccols, ui::Vec2{cell - 4 * k, cell - 4 * k}, 4 * k);
    for (int i = 0; i < int(carry.size()); ++i) {
      ui::cSlot& slot = cgrid.add<ui::cSlot>(ui::Vec2{cell - 4 * k, cell - 4 * k});
      slot.setData(kCarryData + i);
      slot.on("click", [this](ui::cWidget& w) {
        if (cb_.pickCarry) cb_.pickCarry(static_cast<ui::cSlot&>(w).data() - kCarryData);
      });
      carrySlots_.push_back(&slot);
    }
    inventory_ = &dlg;  // refreshCarry needs it set
    refreshCarry(carry);
  }
  inventoryHover_ = &dlg.body().add<ui::cLabel>(" ");
  inventoryHover_->setCentered(true);
  dlg.on("close", [this](ui::cWidget&) {
    inventory_ = nullptr;
    inventoryHover_ = nullptr;
    carrySlots_.clear();
    if (cb_.inventoryClosed) cb_.inventoryClosed();
  });
  inventory_ = &dlg;
}

void cGameUi::openPause() {
  if (pause_) return;
  ui::cDialog& dlg = ui::cDialog::show(ui_, "Paused");
  dlg.setId("pause");
  dlg.addButton("Resume").setId("pause-resume");  // closes -> "close" -> pauseClosed
  dlg.addButton("Settings", [this] { if (cb_.openSettings) cb_.openSettings(); }, false).setId("pause-settings");
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
  ui_.closeModal(*d);
}

void cGameUi::update(const HudModel& m, ui::Vec2 viewport, float dt) {
  const float k = float(effectiveGuiScale(hudScalePref_, viewport.x, viewport.y));
  if (k != scale_) {
    scale_ = k;
    ui::Theme t = ui_.theme();
    t.setScale(k);
    ui_.setTheme(std::move(t));
    rebuildHud();
  }
  if (menu_ && k != menuScale_) rebuildMenu();
  hud_->setVisible(m.visible);
  age_ += dt;
  splash_->setVisible(age_ < kSplashSeconds);
  debugPanel_->setVisible(!m.debugLines.empty());
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
    debugLook_->setVisible(seen);
    debugTextPost_->setText(joinLines(post));
    debugTextPost_->setVisible(!post.empty());
  }
  controlsText_->setText(joinLines(m.controlLines));
  banner_->setVisible(!m.bannerLines.empty());
  bannerTitle_->setText(m.bannerLines.empty() ? std::string() : m.bannerLines.front());
  bannerText_->setText(m.bannerLines.size() > 1 ? m.bannerLines[1] : std::string());
  for (int i = 0; i < kHotbarSlots; ++i) {
    ui::cSlot& slot = *slots_[size_t(i)];
    const ItemStack& st = m.hotbar[size_t(i)];
    // Real stacks (P2b-1): block-backed stacks render as blocks, tools and
    // the Fragment render with their own item icons -- the Fragment in
    // slot 0 is finally visible in the HUD.
    slot.setIcon(itemIconRef(st.itemId));
    slot.setSelected(i == m.selected);
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
  ui_.update(dt);
  ui_.layout(viewport);
}

void cGameUi::paint(HudLayer& hud, const BlockMaterials& materials, const std::function<const Image*(BlockId)>& icon,
                   const std::function<const Image*(uint32_t)>& itemIcon) const {
  cHudUiRenderer renderer(hud, [&materials](std::string_view image) -> std::optional<ui::Color> {
    constexpr std::string_view kPrefix = "block:";
    if (image.substr(0, kPrefix.size()) != kPrefix) return std::nullopt;
    int id = 0;
    const std::string_view num = image.substr(kPrefix.size());
    if (std::from_chars(num.data(), num.data() + num.size(), id).ec != std::errc() || id <= 0 ||
        id >= int(blockRegistry().size()))
      return std::nullopt;
    const auto& c = materials.swatch(BlockId(id));
    return ui::Color{c[0], c[1], c[2], 1.f};
  }, [&icon, &itemIcon](std::string_view image) -> const Image* {
    constexpr std::string_view kItemPrefix = "item:";
    if (image.substr(0, kItemPrefix.size()) == kItemPrefix) {
      // Non-block item icon (a tool, the Fragment): the item icon atlas.
      if (!itemIcon) return nullptr;
      int id = 0;
      const std::string_view num = image.substr(kItemPrefix.size());
      if (std::from_chars(num.data(), num.data() + num.size(), id).ec != std::errc() || id <= 0)
        return nullptr;
      return itemIcon(uint32_t(id));
    }
    constexpr std::string_view kPrefix = "block:";
    if (!icon || image.substr(0, kPrefix.size()) != kPrefix) return nullptr;
    int id = 0;
    const std::string_view num = image.substr(kPrefix.size());
    if (std::from_chars(num.data(), num.data() + num.size(), id).ec != std::errc() || id <= 0 || id >= int(blockRegistry().size()))
      return nullptr;
    return icon(BlockId(id));  // textured icon; the swatch above is the fallback
  });
  ui_.paint(renderer);
}

}  // namespace lithokraft
