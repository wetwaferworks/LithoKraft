// Cortana 2026-10-15 0455: PORT getpid on _WIN32
#include "lithokraft/game/game_ui_selftest.h"
#include <algorithm>  // D9: std::max / std::min over an initializer list
#include <array>  // B9: testHudSlotResidency
#include <cmath>
#include <cstdio>
#include <cstring>  // B9: memcmp in testHudSlotResidency
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#if defined(_WIN32)
#include <process.h>  // _getpid (Windows has no unistd.h)
#define getpid _getpid
#else
#include <unistd.h>  // getpid: per-run unique temp dirs (G-M7, P2b-2)
#endif
#include "lithokraft/game/game_ui.h"
#include "lithokraft/content/blocks.h"  // paletteBlocksGrouped: first palette slot (2026-10-31)
#include "lithokraft/content/items.h"  // kItemFragment / kItemBashor / itemIconRef (P2b-1 HUD binding)
#include "lithokraft/game/script_editor.h"
#include "lithokraft/game/ui_script_bindings.h"
#include "litholattice/mods/mod_interface.h"  // kPermUI for the script sandbox
#include "litholattice/render/hud_ui_renderer.h"
#include "litholattice/render/overlay_geometry.h"
#include "lithokraft/game/ui_textedit.h"
#include "lithokraft/game/ui_widgets.h"
#include "lithokraft/game/cWorldPicker.h"  // 2026-11-07: world picker geometry probe (JD bug report)

namespace lithokraft {
namespace {

class Checker {
 public:
  void operator()(bool ok, const std::string& what) {
    std::printf("  [%s] game ui: %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) ++fails_;
  }
  int fails() const { return fails_; }

 private:
  int fails_ = 0;
};

ui::Vec2 center(const lithogui::Widget& w) { return {w.rect().x + w.rect().w * 0.5f, w.rect().y + w.rect().h * 0.5f}; }
void click(ui::cUiRoot& ui, ui::Vec2 p, uint8_t mods = ui::kModNone) {
  ui::UiEvent down = ui::UiEvent::down(p), up = ui::UiEvent::up(p);
  down.mods = up.mods = mods;
  ui.handleEvent(ui::UiEvent::move(p));
  ui.handleEvent(down);
  ui.handleEvent(up);
}

std::string readFile(const std::filesystem::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

void writeFile(const std::filesystem::path& p, const std::string& text) {
  std::ofstream out(p, std::ios::binary | std::ios::trunc);
  out << text;
}

void testHudRenderer(Checker& check) {
  // Litho GUI migration: the old cHudUiRenderer was an immediate-mode
  // IUiRenderer (fillRect/drawText/drawImage methods); the new one consumes
  // a lithogui::DrawList through render(). This test drives the same scene
  // through a DrawList and asserts the same contract in the new terms:
  // paint order kept, nested clips intersect, images resolve by texture id
  // to swatches (magenta if unknown).
  HudLayer hud;
  hud.rects.push_back({0, 0, 10, 10, 1, 1, 1, 1});
  lithogui::DrawList dl;
  dl.fillRect({0, 0, 100, 100}, {0, 0, 1, 1});
  dl.pushClip({10, 10, 20, 20});
  dl.pushClip({20, 0, 100, 100});  // intersects to (20,10)-(30,30)
  dl.fillRect({0, 0, 100, 100}, {0, 1, 0, 1});
  lithogui::TextStyle ts;
  ts.size = 16.f;
  ts.lineHeightMul = 1.0f;
  dl.textRun({0, 12, 64, 16}, "ABCDEFGH", ts, {1, 1, 1, 1}, 12.8f);
  dl.popClip();
  dl.popClip();
  const int redId = ui::iconTextureId("red");
  const int nopeId = ui::iconTextureId("nope");
  dl.imageQuad({0, 0, 4, 4}, redId, 0.5f);
  dl.imageQuad({0, 0, 4, 4}, nopeId, 1.0f);
  cHudUiRenderer r(hud, [](int id) -> std::optional<lithogui::Color> {
    if (id == ui::iconTextureId("red")) return lithogui::Color{1, 0, 0, 1};
    return std::nullopt;
  });
  r.render(dl);
  std::vector<HudVertex> v;
  buildHudVertices(hud, v);
  const bool draws = hud.draws.size() == 5 && hud.draws[0].b == 1.f && hud.draws[1].g == 1.f &&
                     hud.draws[1].clip.x0 == 20 && hud.draws[1].clip.y0 == 10 && hud.draws[1].clip.x1 == 30 &&
                     hud.draws[1].clip.y1 == 30 && hud.draws[2].text == "ABCDEFGH" && hud.draws[2].scale == 1.f &&
                     hud.draws[2].clip.x0 == 20 && hud.draws[2].clip.x1 == 30;
  check(draws && v.size() > 6, "cHudUiRenderer: draws after the legacy HUD, nested clips intersect, text run kept");
  const bool images = hud.draws.size() == 5 && hud.draws[3].r == 1 && hud.draws[3].a == 0.5f && hud.draws[4].g == 0 &&
                      hud.draws[4].r == 1 && hud.draws[4].b == 1;
  check(images, "cHudUiRenderer: paint order kept; images resolve to swatches (magenta if unknown)");
}


void testHudTexturedIcons(Checker& check) {
  // Textured icons (Phase B, 2026-11-09): one icon = one draw + one
  // textured quad, sampling an atlas tile whose pixels equal what the
  // raster path painted (blend class: the fitted box-filter output;
  // cutout class: the source thresholded at the 128 boundary).
  using namespace litholattice;
  invalidateHudIconCache();  // clean atlas + generation for this test
  // Blend-class source: 64x64 fitted into a 32x32 rect. Deterministic
  // pattern with a fully transparent corner block.
  std::vector<uint8_t> px(64 * 64 * 4);
  for (int y = 0; y < 64; ++y)
    for (int x = 0; x < 64; ++x) {
      uint8_t* p = &px[(size_t(y) * 64 + x) * 4];
      p[0] = uint8_t(x * 4);
      p[1] = uint8_t(y * 4);
      p[2] = 128;
      p[3] = (x < 8 && y < 8) ? 0 : 255;
    }
  const Image blendSrc(64, 64, px);
  // Cutout-class source: 16x16 magnified into a 48x48 rect. Left half
  // alpha 200 (kept, opaque), right half alpha 100 (dropped).
  std::vector<uint8_t> cx(16 * 16 * 4);
  for (int y = 0; y < 16; ++y)
    for (int x = 0; x < 16; ++x) {
      uint8_t* p = &cx[(size_t(y) * 16 + x) * 4];
      p[0] = 40;
      p[1] = uint8_t(y * 16);
      p[2] = 90;
      p[3] = x < 8 ? 200 : 100;
    }
  const Image cutSrc(16, 16, cx);
  const int blendId = ui::iconTextureId("texicon-blend");
  const int cutId = ui::iconTextureId("texicon-cut");
  auto icons = [&](int id) -> const Image* {
    if (id == blendId) return &blendSrc;
    if (id == cutId) return &cutSrc;
    return nullptr;
  };
  {  // blend class: exactly one icon draw, atlas stamped, 6 verts all tex==1
    HudLayer hud;
    lithogui::DrawList dl;
    dl.imageQuad({0, 0, 32, 32}, blendId, 1.0f);
    cHudUiRenderer r(hud, [](int) -> std::optional<lithogui::Color> { return std::nullopt; }, icons);
    r.render(dl);
    bool ok = hud.draws.size() == 1 && hud.draws[0].icon && hud.iconAtlas && hud.iconAtlasVersion > 0;
    std::vector<HudVertex> v;
    buildHudVertices(hud, v);
    ok = ok && v.size() == 6;
    for (const auto& vtx : v) ok = ok && vtx.tex == 1.f;
    check(ok, "textured icons: a fitted icon is ONE draw, atlas stamped, expanding to one tex==1 quad");
    // Tile pixels equal the fitted box-filter output (alpha < 8 zeroed).
    const Image& fit = downsampleIcon(blendSrc, 32);  // fittedIcon(src, 32, 32) is this, cached (hud_ui_renderer.cpp)
    const int entry = hudIconAtlas().tileFor(blendSrc, false, 32, 32);
    float u0, v0, u1, v1;
    hudIconAtlas().tileUv(entry, u0, v0, u1, v1);
    const Image& snap = *hud.iconAtlas;
    const int tx0 = int(std::lround(u0 * snap.width())), ty0 = int(std::lround(v0 * snap.height()));
    bool pix = entry >= 0;
    for (int y = 0; y < 32 && pix; ++y)
      for (int x = 0; x < 32 && pix; ++x) {
        const uint8_t* want = fit.pixel(x, y);
        const uint8_t* got = snap.pixel(tx0 + x, ty0 + y);
        pix = want[3] < 8 ? (got[3] == 0) : (got[0] == want[0] && got[1] == want[1] && got[2] == want[2] && got[3] == want[3]);
      }
    check(pix, "textured icons: blend tile pixels equal the raster path's fitted output");
  }
  {  // cutout class: one icon draw over the magnified rect; tile = thresholded source
    HudLayer hud;
    lithogui::DrawList dl;
    dl.imageQuad({0, 0, 48, 48}, cutId, 1.0f);
    cHudUiRenderer r(hud, [](int) -> std::optional<lithogui::Color> { return std::nullopt; }, icons);
    r.render(dl);
    bool ok = hud.draws.size() == 1 && hud.draws[0].icon && hud.draws[0].w == 48.f && hud.draws[0].h == 48.f;
    const int entry = hudIconAtlas().tileFor(cutSrc, true, 0, 0);
    float u0, v0, u1, v1;
    hudIconAtlas().tileUv(entry, u0, v0, u1, v1);
    const Image& snap = *hud.iconAtlas;
    const int tx0 = int(std::lround(u0 * snap.width())), ty0 = int(std::lround(v0 * snap.height()));
    bool pix = entry >= 0;
    for (int y = 0; y < 16 && pix; ++y)
      for (int x = 0; x < 16 && pix; ++x) {
        const uint8_t* got = snap.pixel(tx0 + x, ty0 + y);
        pix = x < 8 ? (got[0] == 40 && got[3] == 255) : (got[3] == 0);
      }
    check(ok && pix, "textured icons: a magnified icon is ONE draw; cutout tile is the thresholded source");
  }
  {  // oversize source declines to the counted fallback lane
    const Image big(4100, 4, std::vector<uint8_t>(4100 * 4 * 4, 255));
    const uint64_t before = hudIconStats().declines;
    check(hudIconAtlas().tileFor(big, true, 0, 0) == -1 && hudIconStats().declines == before + 1,
          "textured icons: a source over the atlas cap declines, counted");
    check(hudIconStats().textured >= 2, "textured icons: emissions counted in the icon stats");
  }
}

void testScriptBindings(Checker& check) {
  cScriptEngine scripts;
  ui::cUiRoot ui;
  cUiScriptBindings bindings(scripts, ui);
  const bool ready = [&] {
    if (!scripts.init()) return false;
    scripts.setModPermissions(kPermUI);  // the ui_* bindings require the UI permission
    return bindings.registerApi();
  }();
  ui.setScriptHost(&bindings);
  auto& stack = ui.root().add<ui::cStackPanel>();
  stack.setLayout(ui::Layout::at({0, 0}, {0, 0}, {}, {200, 200}));
  auto& btn = stack.add<ui::cButton>("Go");
  btn.setId("go");
  btn.onScript("click", "void onGo(const string &in)");
  stack.add<ui::cLabel>("").setId("lbl");
  stack.add<ui::cNumberField>(0, 64, 0).setId("n");
  const bool built = scripts.loadSource("ui_test", R"(
    void onGo(const string &in id) {
      ui_setText("lbl", "clicked " + id);
      ui_setValue("n", ui_value("n") + 7);
    }
    void onClose(const string &in id) { ui_close("popup"); }
    void onLoad() {
      ui_show("{\"type\": \"panel\", \"id\": \"popup\", \"children\": [{\"type\": \"button\", \"id\": \"x\", \"text\": \"X\", \"on\": {\"click\": \"void onClose(const string &in)\"}}]}");
    })");
  ui.layout({800, 600});
  click(ui, center(btn));
  click(ui, center(btn));
  auto* lbl = ui.root().findAs<ui::cLabel>("lbl");
  auto* n = ui.root().findAs<ui::cNumberField>("n");
  check(ready && built && lbl && lbl->text() == "clicked go" && n && n->value() == 14,
        "script bindings: widget event calls the script handler, which reads / writes widgets by id");
  scripts.callHook("void onLoad()");
  ui.layout({800, 600});
  ui::cWidget* x = ui.root().find("x");
  const bool shown = x != nullptr;
  if (x) click(ui, center(*x));
  check(shown && ui.root().find("popup") == nullptr, "script bindings: ui_show builds JSON UI; its handler closes itself");
}

void testScriptEditor(Checker& check) {
  namespace fs = std::filesystem;
  const fs::path dir = fs::temp_directory_path() / ("lithokraft_ui_selftest_scripts-" + std::to_string(::getpid()));
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  writeFile(dir / "a.as", "void onLoad() { log(\"v1\"); }\n");
  writeFile(dir / "b.as", "int helper() { return 1; }\n");
  cScriptEngine scripts;
  scripts.init();
  scripts.loadDirectory(dir.string());
  scripts.callHook("void onLoad()");
  ui::cUiRoot ui;
  ui.layout({1280, 720});
  int reloads = 0;
  cScriptEditor editor(ui, scripts, dir, [&] {
    ++reloads;
    scripts.callHook("void onLoad()");
  });
  editor.open();
  ui.layout({1280, 720});
  const bool opened = editor.isOpen() && editor.currentFile() == "a.as" && editor.editor() &&
                      editor.editor()->text().find("v1") != std::string::npos && !editor.modified();
  check(opened && scripts.lastLog() == "v1", "script editor: opens with the first scripts/*.as file");

  ui::cTextEditor& ed = *editor.editor();
  ed.focus();
  ed.buffer().selectAll();
  ed.buffer().insert("void onLoad() { log(\"v2\"); }\n");
  ui.handleEvent(ui::UiEvent::keyDown(ui::Key::S, ui::kModCtrl));
  check(readFile(dir / "a.as").find("v2") != std::string::npos && reloads == 1 && scripts.lastLog() == "v2" && !editor.modified(),
        "script editor: Ctrl+S saves and hot-reloads (onLoad ran again)");

  ed.buffer().setCaret({0, 0});
  ed.buffer().selectAll();
  ed.buffer().insert("void onLoad() {\n  log(\"v3\")\n  oops;\n}\n");
  const bool saved = editor.save();
  scripts.callHook("void onLoad()");
  check(!saved && !scripts.buildErrors().empty() && scripts.lastLog() == "v2" && ed.buffer().caret().line >= 1 &&
            editor.status().find("compile failed") != std::string::npos,
        "script editor: a compile error keeps the old scripts running, jumps to the error (" +
            (scripts.buildErrors().empty() ? std::string("-") : scripts.buildErrors().front()) + ")");

  ed.buffer().insert("x");
  ui.handleEvent(ui::UiEvent::keyDown(ui::Key::Escape));
  const bool asked = editor.isOpen() && ui.topModal() && ui.topModal()->facadeId() != "script-editor";
  ui.handleEvent(ui::UiEvent::keyDown(ui::Key::Escape));  // cancel the question
  const bool stillOpen = editor.isOpen() && editor.modified();
  editor.close(true);
  check(asked && stillOpen && !editor.isOpen() && !ui.modalOpen(), "script editor: Esc with unsaved changes asks first");
  fs::remove_all(dir, ec);
}

void testGameUi(Checker& check) {
  int selected = -1, closed = 0;
  BlockId picked = kAir;
  bool shift = false;
  int putBackFlat = -1;  // cursor stack (2026-11-11): must NEVER fire now... the click path routes to cursorClick
  uint32_t pickedItem = 0;
  int cursorFlat = -1, cursorCalls = 0, paletteCursorCalls = 0;
  cGameUi gui({[&](int s) { selected = s; }, [&](BlockId b, bool sh) { picked = b; shift = sh; }, [&] { ++closed; },
              {}, {}, {}, {}, {}, {},
              {},  // pickCarry (unused here)
              {},  // saveWorld
              {},  // loadWorld
              [&](int flat) { putBackFlat = flat; },
              [&](uint32_t id, bool) { pickedItem = id; },
              [&](int flat) { cursorFlat = flat; ++cursorCalls; },  // cursorClick (2026-11-11)
              [&] { ++paletteCursorCalls; }});                      // paletteClickCursor (2026-11-11)
  HudModel m;
  m.hotbar = {ItemStack{1, 1}, ItemStack{2, 1}, ItemStack{3, 1}, {}, {}, {}, {}, {}};
  m.selected = 1;
  m.debugLines = {"FPS 60", "TPS 32"};
  m.controlLines = {"H show controls"};
  gui.update(m, {1280, 720}, 0.016f);
  ui::cWidget* bar = gui.root().root().find("hotbar");
  const bool hud = bar && bar->children().size() == kHotbarSlots && bar->rect().y + bar->rect().h <= 720 && bar->rect().x > 0;
  if (bar) click(gui.root(), center(*bar->children()[4]));
  check(hud && selected == 4, "HUD: hotbar is bottom-centred; clicking a slot selects it");

  // P2b-1 HUD binding: the model carries the inventory's real stacks, and
  // the widgets render THEM -- the Fragment and a tool show their item
  // icons (not block swatches), a block stack shows its block icon, and
  // the selected slot's name is the stack's real name.
  {
    HudModel im;
    im.hotbar = {ItemStack{kItemFragment, 1}, ItemStack{kItemBashor, 1}, ItemStack{uint32_t(kLamp), 33},
                 {}, {}, {}, {}, {}};
    im.selected = 0;
    gui.update(im, {1280, 720}, 0.016f);
    ui::cWidget* bar2 = gui.root().root().find("hotbar");
    const ui::cSlot* s0 = bar2 ? dynamic_cast<const ui::cSlot*>(bar2->children()[0].get()) : nullptr;
    const ui::cSlot* s1 = bar2 ? dynamic_cast<const ui::cSlot*>(bar2->children()[1].get()) : nullptr;
    const ui::cSlot* s2 = bar2 ? dynamic_cast<const ui::cSlot*>(bar2->children()[2].get()) : nullptr;
    check(s0 && s0->icon() == itemIconRef(kItemFragment) && s0->selected(),
          "HUD: hotbar slot 0 renders the Fragment stack with its item icon, selected");
    check(s1 && s1->icon() == itemIconRef(kItemBashor), "HUD: a tool stack renders with its item icon");
    check(s2 && s2->icon() == itemIconRef(uint32_t(kLamp)), "HUD: a block stack renders with its block icon");
    // The selected slot's name label (the label laid out above the bar).
    bool nameOk = false;
    const auto findName = [&nameOk](const auto& self, const lithogui::Widget& w) -> void {
      if (const auto* l = dynamic_cast<const ui::cLabel*>(&w); l && l->text() == "The Logbook") nameOk = true;
      for (const auto& c : w.children()) self(self, *c);
    };
    findName(findName, gui.root().root());
    check(nameOk, "HUD: the selected slot's name reads 'The Fragment' (the real stack, not a block)");
  }
  gui.update(m, {1920, 1080}, 0.016f);  // auto GUI scale (2026-10-16): 2 from 1920x1080 up
  const bool scaled = bar != gui.root().root().find("hotbar") && gui.root().theme().scale() == 2.f;
  check(scaled, "HUD: rebuilt at scale 2 for 1080p windows (auto)");
  {  // 2026-10-25 (JD): hotbar = kHotbarScale (x1.125), capped to fit; centred, on screen at 1080p
    gui.update(m, {1920, 1080}, 0.016f);
    const ui::cWidget* hb = gui.root().root().find("hotbar");
    const bool fits = hb && hb->rect().x >= 0 && hb->rect().x + hb->rect().w <= 1920 && hb->rect().y + hb->rect().h <= 1080 &&
                      std::abs((hb->rect().x + hb->rect().w * 0.5f) - 960.f) <= 2.f;
    gui.update(m, {2560, 1080}, 0.016f);
    const ui::cWidget* wide = gui.root().root().find("hotbar");
    const bool full = wide && std::abs(wide->children()[0]->rect().w - 40 * 2 * cGameUi::kHotbarScale) < 0.5f;
    check(fits && full && cGameUi::kHotbarScale == cGameUi::kInventoryScale * 1.125f,
          "HUD: hotbar x1.125 from one constant; capped to fit and centred at 1920x1080, full size on wider windows");
    gui.update(m, {1920, 1080}, 0.016f);
  }

  {  // 2026-11-08 (layout diagnosis items 1+2, WS2): the tick panel and
    // the controls block must land FULLY on screen at desktop, laptop
    // and small-window sizes. Both panels wrap a bare fill label; before
    // the facade measure fix they measured 0x0, the tick text ran off
    // the right edge (only "TI" visible on JD's machine) and the
    // controls text ran off the bottom.
    HudModel lm;
    lm.tickLine = "TICK 128.0 TPS 512.0 MSPT 2.0 STABLE";
    lm.tickLevel = 0;
    lm.controlLines = {"WASD move", "Space jump", "E inventory", "F3 debug", "Esc pause",
                       "1-9,0 select", "Wheel select", "LMB break", "RMB place", "Shift sneak",
                       "Ctrl sprint", "Tab scores", "T chat", "Enter confirm", "Q drop",
                       "R rotate", "G grid", "H help", "J journal", "K knelt"};
    bool allFit = true;
    for (const ui::Vec2 vp : {ui::Vec2{1920, 1080}, ui::Vec2{1280, 720}, ui::Vec2{800, 600}}) {
      gui.update(lm, vp, 0.016f);
      gui.update(lm, vp, 0.016f);  // second frame: scale settled, HUD rebuilt
      const ui::cWidget* tick = gui.root().root().find("tick-panel");
      const ui::cWidget* ctrl = gui.root().root().find("controls-panel");
      const auto inside = [vp](const ui::cWidget* w) {
        return w && w->rect().x >= 0 && w->rect().y >= 0 && w->rect().x + w->rect().w <= vp.x &&
               w->rect().y + w->rect().h <= vp.y && w->rect().w > 0 && w->rect().h > 0;
      };
      const auto childInside = [inside](const ui::cWidget* w) {
        return w && !w->children().empty() && inside(dynamic_cast<const ui::cWidget*>(w->children()[0].get()));
      };
      allFit = allFit && inside(tick) && childInside(tick) && inside(ctrl) && childInside(ctrl);
    }
    check(allFit, "layout: tick panel and controls block fully on screen at 1920x1080, 1280x720 and 800x600");
    gui.update(m, {1280, 720}, 0.016f);  // settle the viewport back toward scale 1 (the pacing block settles fully itself)
  }

  {  // 2026-11-08 (WS3): hotbar hover tooltip. Rest the pointer on a
    // filled hotbar slot past the dwell; the facade tooltip popup opens
    // with the stack's name. (JD was blocked on building without these.)
    ui::cWidget* barNow = gui.root().root().find("hotbar");  // re-find: layout checks rebuilt the HUD
    gui.root().handleEvent(ui::UiEvent::move(center(*barNow->children()[1])));
    for (int i = 0; i < 6; ++i) gui.update(m, {1280, 720}, 0.1f);
    check(gui.root().tooltipVisible(), "tooltip: resting on a hotbar slot opens the item tooltip");
    gui.root().handleEvent(ui::UiEvent::move({4, 4}));
    gui.update(m, {1280, 720}, 0.016f);
    check(!gui.root().tooltipVisible(), "tooltip: moving off the slot closes it");
  }

  {  // 2026-11-09 next-build-2 (cheap cut c-i): a pointer move that stays
    // inside the SAME widget changes no hover state, so it must not move
    // the paint generation (the gate survives mouse wiggle); a move that
    // crosses to another widget must still bump it (hover is paint).
    ui::cWidget* barNow = gui.root().root().find("hotbar");
    const ui::Vec2 c1 = center(*barNow->children()[1]);
    gui.root().handleEvent(ui::UiEvent::move(c1));
    gui.update(m, {1280, 720}, 0.016f);
    const uint64_t gen0 = gui.root().paintGeneration();
    gui.root().handleEvent(ui::UiEvent::move({c1.x + 2.f, c1.y + 1.f}));  // still inside slot 1
    check(gui.root().paintGeneration() == gen0,
          "paint gate: a pointer move inside one widget does not bump the paint generation (cheap cut c-i)");
    gui.root().handleEvent(ui::UiEvent::move(center(*barNow->children()[2])));  // crosses to slot 2
    check(gui.root().paintGeneration() > gen0,
          "paint gate: a pointer move that changes the hover target still bumps the paint generation");
    gui.root().handleEvent(ui::UiEvent::move({4, 4}));
    gui.update(m, {1280, 720}, 0.016f);
  }

  {  // 2026-11-11 (pacing instrumentation, JD's dirty-flag question):
    // a settled session with a frozen model must show the dirty flag
    // at rest. The debug lines stay constant, so after warm-up the
    // dynamic readout layer alone is allowed to repaint (its
    // generation advances once per frame by design); the MAIN layer
    // must not rebuild at all. Under frozen inputs each dynamic
    // repaint reproduces identical bytes, so the no-op counter is
    // EXPECTED to track the dynamic repaint count here: that is the
    // measured cost shape of the by-design per-frame readout layer,
    // on its own tiny layer, never the main one. Then ONE visible
    // change (a readout line moves, panel already shown) must cost
    // exactly the dynamic layer, never the main one.
    HudLayer hud;
    BlockMaterials mats;
    const auto paintNow = [&] { gui.paint(hud, mats, {}, {}); };
    // The tick panel is shown/hidden by tickLine emptiness
    // (game_ui.cpp: tickPanel_->setVisible(!m.tickLine.empty())): an
    // empty-to-nonempty move is a first show, a real visibility
    // change that legitimately rebuilds the main layer once. Set the
    // line BEFORE the zeros so the probe measures text churn on an
    // already-shown panel, not the first show.
    m.tickLine = "TPS 128 (1s)  128 (5s) / 128  dropped 3 in 5s (3 total)";
    // Warm-up: the facade may still be settling from the earlier
    // viewport/scale blocks (a pending scale move rebuilds once, with
    // changed content). Run frames first, THEN take the zeros, so the
    // probe measures the settled state, not the settle.
    for (int i = 0; i < 6; ++i) { gui.update(m, {1280, 720}, 0.016f); paintNow(); }
    const uint64_t mc0 = gui.root().mainPaintCalls(), mn0 = gui.root().mainPaintNoop();
    const uint64_t dc0 = gui.root().dynPaintCalls(), dn0 = gui.root().dynPaintNoop();
    for (int i = 0; i < 30; ++i) {
      gui.update(m, {1280, 720}, 0.016f);
      paintNow();
    }
    std::printf("    [probe] pacing settled 30 frames: main calls +%llu noop +%llu, dyn calls +%llu noop +%llu\n",
                (unsigned long long)(gui.root().mainPaintCalls() - mc0),
                (unsigned long long)(gui.root().mainPaintNoop() - mn0),
                (unsigned long long)(gui.root().dynPaintCalls() - dc0),
                (unsigned long long)(gui.root().dynPaintNoop() - dn0));
    check(gui.root().mainPaintCalls() == mc0 && gui.root().mainPaintNoop() == mn0,
          "pacing counters: a settled session never rebuilds the main layer (the dirty flag rests)");
    check(gui.root().dynPaintCalls() > dc0 && gui.root().dynPaintNoop() > dn0,
          "pacing counters: the readout layer repaints per frame by design; under frozen inputs those repaints reproduce identical bytes (counted no-ops on the readout layer only)");
    m.tickLine = "TPS 126 (1s)  127 (5s) / 128  dropped 4 in 5s (4 total)";
    gui.update(m, {1280, 720}, 0.016f);
    paintNow();
    check(gui.root().mainPaintCalls() == mc0,
          "pacing counters: a readout-only change rebuilds only the dynamic layer, never the main layer");
    check(gui.root().dynPaintCalls() > dc0 + 30,
          "pacing counters: the readout change lands in the dynamic layer's repaint count");
    m.tickLine.clear();
  }

  // Pacing probe, hover phase (JD 2026-11-11: "something's going on with
  // its dirty flag probably"). The creative screen is open; the pointer
  // rests on a palette slot and the tooltip is up. Thirty settled frames
  // in that state, model frozen: the main layer must not rebuild, not
  // even with identical content (a no-op rebuild there is exactly the
  // hover flicker shape: same bytes, re-expanded and re-uploaded).
  {
    HudLayer hud2;
    BlockMaterials mats2;
    const auto paintNow2 = [&] { gui.paint(hud2, mats2, {}, {}); };
    gui.openInventory(1);
    gui.update(m, {1280, 720}, 0.016f);
    paintNow2();
    const auto findSlot2 = [](const auto& self, lithogui::Widget& w) -> ui::cWidget* {
      if (auto* s = dynamic_cast<ui::cSlot*>(&w)) return s;
      for (const auto& c : w.children())
        if (ui::cWidget* s = self(self, *c)) return s;
      return nullptr;
    };
    ui::cWidget* cell = nullptr;
    if (ui::cWidget* modal = gui.root().topModal()) cell = findSlot2(findSlot2, *modal);
    if (cell) gui.root().handleEvent(ui::UiEvent::move(center(*cell)));
    for (int i = 0; i < 6; ++i) { gui.update(m, {1280, 720}, 0.1f); paintNow2(); }
    const uint64_t hc0 = gui.root().mainPaintCalls(), hn0 = gui.root().mainPaintNoop();
    for (int i = 0; i < 30; ++i) {
      gui.update(m, {1280, 720}, 0.016f);
      paintNow2();
    }
    check(gui.root().mainPaintCalls() == hc0,
          "pacing counters: resting hover with the tooltip up never rebuilds the main layer");
    check(gui.root().mainPaintNoop() == hn0,
          "pacing counters: resting hover never fires a no-op (identical-content) main rebuild");
    gui.root().handleEvent(ui::UiEvent::move({4, 4}));
    gui.update(m, {1280, 720}, 0.016f);
  }

  gui.openInventory(1);
  gui.update(m, {1600, 1000}, 0.016f);
  ui::cWidget* inv = gui.root().topModal();
  ui::cWidget* firstCell = nullptr;
  // The first slot of the inventory grid (block 1).
  const auto findSlot = [](const auto& self, lithogui::Widget& w) -> ui::cWidget* {
    if (auto* s = dynamic_cast<ui::cSlot*>(&w)) return s;
    for (const auto& c : w.children())
      if (ui::cWidget* s = self(self, *c)) return s;
    return nullptr;
  };
  if (inv) firstCell = findSlot(findSlot, *inv);
  {  // 2026-11-08 (WS3): palette hover tooltip carries the block's name.
    if (firstCell) gui.root().handleEvent(ui::UiEvent::move(center(*firstCell)));
    for (int i = 0; i < 6; ++i) gui.update(m, {1600, 1000}, 0.1f);
    std::string tipText;
    if (const auto* tip = gui.root().tooltipForTest()) tipText = tip->text();
    const std::string want = blockRegistry().def(paletteBlocksGrouped().front()).name.data();
    check(gui.root().tooltipVisible() && tipText == want, "tooltip: resting on a palette slot names the block");
    gui.root().handleEvent(ui::UiEvent::move({4, 4}));
    gui.update(m, {1600, 1000}, 0.016f);
  }
  if (firstCell) click(gui.root(), center(*firstCell), ui::kModShift);
  // 2026-10-31: the palette is family-grouped, so the first slot is paletteBlocksGrouped()[0], not block 1.
  const int firstBlock = int(paletteBlocksGrouped().front());
  check(gui.inventoryOpen() && picked == firstBlock && shift && paletteCursorCalls == 0,
        "inventory: modal dialog; shift-click picks the block (cursor-stack check 9: the palette pull is unchanged while not carrying, no discard fires)");
  // Item 4 (2026-11-09): the dialog IS the combined creative screen now
  // (JD's approved design superseded the palette-only dialog as the
  // creative entry point; the title says what it is).
  {
    const auto* dlg = dynamic_cast<const ui::cDialog*>(inv);
    check(dlg && dlg->title().find("Creative inventory") != std::string::npos,
          "creative screen: the dialog is the combined 'Creative inventory' screen");
    check(inv && inv->find("palette-note") != nullptr, "creative screen: the dialog explains the full-stack give and the cursor gestures");
    check(inv && inv->find("creative-picker") != nullptr && inv->find("creative-inventory-half") != nullptr,
          "creative screen: ONE dialog holding both halves (picker left, inventory right)");
    int tabs = 0;
    const auto countTabs = [](const auto& self, lithogui::Widget& w) -> int {
      int n = 0;
      if (auto* cw = dynamic_cast<ui::cWidget*>(&w); cw && cw->facadeId() == "creative-tab") n = 1;
      for (const auto& c : w.children()) n += self(self, *c);
      return n;
    };
    if (inv) tabs = countTabs(countTabs, *inv);
    check(tabs == 5, "creative screen: the picker has the five Bedrock tabs (Construction/Equipment/Items/Nature/All)");
    check(inv && inv->find("agent-health") != nullptr && inv->find("agent-kharge") != nullptr && inv->find("agent-static") != nullptr,
          "creative screen: the Agent panel carries the Health/Kharge/Static bars");
    check(inv && inv->find("creative-accessories") != nullptr && inv->find("creative-capacity") != nullptr,
          "creative screen: Accessories row and the capacity readout are present");
    int worn = 0, acc = 0, hands = 0;
    const auto countIds = [](const auto& self, lithogui::Widget& w, const char* id) -> int {
      int n = 0;
      if (auto* cw = dynamic_cast<ui::cWidget*>(&w); cw && cw->facadeId() == id) n = 1;
      for (const auto& c : w.children()) n += self(self, *c, id);
      return n;
    };
    if (inv) {
      worn = countIds(countIds, *inv, "creative-worn");
      acc = countIds(countIds, *inv, "creative-accessory");
      hands = countIds(countIds, *inv, "creative-hand-l") + countIds(countIds, *inv, "creative-hand-r");
    }
    check(worn == 8 && acc == 6 && hands == 2,
          "creative screen: 8 worn slots, 6 accessory slots, and the two hand slots outside the hotbar");
    // JD alignment correction (2026-11-09 08:01 EST): the left worn
    // column (Head/Face/Chest/Arms, gear flats kCarrySlots + 0/5/1/4: 82/87/83/86 since the
    // B9 four-row grid, 50/55/51/54 before) shares ONE x,
    // keyed to Chest... no staggered offset between the four slots.
    {
      ui::cSlot* col4[4] = {nullptr, nullptr, nullptr, nullptr};
      const int flats[4] = {kCarrySlots + 0, kCarrySlots + 5, kCarrySlots + 1, kCarrySlots + 4};  // Head, Face, Chest, Arms
      const auto findByData = [](const auto& self, lithogui::Widget& w, int data, ui::cSlot*& out) -> void {
        if (auto* s = dynamic_cast<ui::cSlot*>(&w); s && s->data() == data) out = s;
        for (const auto& c : w.children()) self(self, *c, data, out);
      };
      if (inv)
        for (int i = 0; i < 4; ++i) findByData(findByData, *inv, (1 << 20) + flats[i], col4[i]);
      bool aligned = col4[0] && col4[1] && col4[2] && col4[3];
      if (aligned) {
        const float x = col4[2]->rect().x;  // Chest is the alignment key
        for (int i = 0; i < 4; ++i) aligned = aligned && std::fabs(col4[i]->rect().x - x) < 0.5f;
      }
      check(aligned, "creative screen: Head/Face/Chest/Arms worn slots share one aligned column (keyed to Chest, JD 08:01)");
    }
    // Creative rework (JD ruling, 2026-11-09 12:06 EST: one global
    // frame, sub-frames anchored to it): the screen lives in the
    // global frame; at all four resolutions every slot outside the
    // palette scroll view sits inside the frame, the hotbar row fits
    // the inventory half, and the dialog itself stays in the viewport.
    // (Palette slots below the scroll fold are placed past the view's
    // bottom by the scroll mechanism itself: clipped at paint and at
    // hit test by cScrollView, so they are asserted on x-containment
    // and scroll-view containment instead.)
    {
      ui::cWidget* frameW = inv ? inv->find("creative-global-frame") : nullptr;
      check(frameW != nullptr, "creative rework: the screen sits in one global frame (creative-global-frame)");
      ui::cWidget* hotW = inv ? inv->find("creative-hotbar") : nullptr;
      ui::cWidget* rightW = inv ? inv->find("creative-inventory-half") : nullptr;
      ui::cWidget* scrollW = inv ? inv->find("creative-palette") : nullptr;
      const std::array<std::pair<int, int>, 4> vps{{{1600, 1000}, {1920, 1080}, {1280, 720}, {800, 600}}};
      const auto inside = [](const lithogui::Rect& r, const lithogui::Rect& box, float tol) {
        return r.x >= box.x - tol && r.y >= box.y - tol && r.x + r.w <= box.x + box.w + tol && r.y + r.h <= box.y + box.h + tol;
      };
      for (const auto& [vw, vh] : vps) {
        for (int i = 0; i < 4; ++i) gui.update(m, {float(vw), float(vh)}, 0.016f);
        ui::cWidget* invN = gui.root().topModal();
        ui::cWidget* frN = invN ? invN->find("creative-global-frame") : nullptr;
        ui::cWidget* hotN = invN ? invN->find("creative-hotbar") : nullptr;
        ui::cWidget* rightN = invN ? invN->find("creative-inventory-half") : nullptr;
        const std::string tag = "creative rework " + std::to_string(vw) + "x" + std::to_string(vh) + ": ";
        check(invN && inside(invN->rect(), lithogui::Rect{0, 0, float(vw), float(vh)}, 0.5f), tag + "the dialog stays inside the viewport");
        bool allIn = frN != nullptr;
        bool hotFits = hotN && rightN && inside(hotN->rect(), rightN->rect(), 0.5f);
        if (frN) {
          const lithogui::Rect fb = frN->rect();
          const auto walk = [&](const auto& self, lithogui::Widget& w, bool inScroll) -> void {
            if (auto* cw = dynamic_cast<ui::cWidget*>(&w)) {
              if (cw->facadeId() == "creative-palette") inScroll = true;
              if (dynamic_cast<ui::cSlot*>(&w) && w.rect().w > 0) {
                const lithogui::Rect r = w.rect();
                if (inScroll) {
                  if (r.x < fb.x - 0.5f || r.x + r.w > fb.x + fb.w + 0.5f) allIn = false;
                } else if (!inside(r, fb, 0.5f)) {
                  allIn = false;
                }
              }
            }
            for (const auto& c : w.children()) self(self, *c, inScroll);
          };
          walk(walk, *invN, false);
        }
        check(allIn, tag + "no slot rect exceeds the global frame");
        check(hotFits, tag + "the hotbar row (18 slots, both hands) fits its half");
      }
      for (int i = 0; i < 4; ++i) gui.update(m, {1600, 1000}, 0.016f);  // settle back at the bench size
      (void)frameW;
      (void)hotW;
      (void)rightW;
      (void)scrollW;
    }
  }
  const BlockMaterials mats;  // default materials suffice: the checks read layer outcomes, not swatches
  {  // JD paint ruling (2026-11-09): the GUI paints only when dirty.
    // In steady state (no interaction, no state change) the main layer
    // must REPLAY, not rebuild, across consecutive paints... at most
    // one main rebuild per dirty mark, with repeated marks inside one
    // frame coalescing. The tick/F3 readout text changing per frame
    // must never dirty the main layer (it is a dynamic surface: it
    // repaints its own tiny layer), and nothing but a readout change
    // may move there either.
    HudLayer layer;
    gui.paint(layer, mats);
    check(gui.root().paintRebuilt(), "paint ruling: the first paint builds the main layer");
    gui.paint(layer, mats);
    check(!gui.root().paintRebuilt(), "paint ruling: an unchanged frame replays the main layer (no rebuild)");
    // A state change (hotbar selection) marks the main layer dirty
    // exactly once: the next paint rebuilds, the one after replays.
    gui.root().handleEvent(ui::UiEvent::move(center(*gui.root().root().find("hotbar")->children()[2])));  // hover change: an interaction mark
    gui.update(m, {1600, 1000}, 0.016f);
    gui.paint(layer, mats);
    check(gui.root().paintRebuilt(), "paint ruling: an interaction/state change rebuilds the main layer once");
    gui.paint(layer, mats);
    check(!gui.root().paintRebuilt(), "paint ruling: dirty marks coalesce... the following clean frame replays");
    // The per-frame readouts: drive model changes through the tick
    // line and the F3 text for several frames. The main layer must
    // replay throughout; the dynamic layer carries the readout draws.
    HudModel m2 = m;
    bool mainStayedClean = true;
    bool dynAlive = false;
    for (int i = 0; i < 5; ++i) {
      m2.tickLine = "TPS 128 (1s) frame " + std::to_string(i);
      m2.debugLines = {"debug frame " + std::to_string(i), "look: Stone"};
      gui.update(m2, {1600, 1000}, 0.016f);
      gui.paint(layer, mats);
      if (gui.root().paintRebuilt() && i > 0) mainStayedClean = false;  // frame 0 is the readouts' first show (a real state change: the panels join the screen)
      if (gui.root().dynamicDrawCount() > 0) dynAlive = true;
    }
    check(mainStayedClean, "paint ruling: per-frame tick/F3 readout changes never rebuild the main layer");
    check(dynAlive, "paint ruling: the readouts paint on the dynamic layer (tick panel + F3 panel)");
    // Regression pin (2026-11-09, found by probe): the dynamic layer must
    // carry the readout TEXT itself, not only the panel surrounds... a
    // routing slip in paintTreeLayer once dropped every label inside a
    // dynamic surface while the draw COUNT still read healthy.
    {
      // The readout panels' visibility is re-derived per update from the
      // dialog state: the creative screen above is still OPEN at this
      // point in the test (it closes two checks later), and the F3 panel
      // hides under the inventory by design (2026-10-31)... so dismiss
      // it the way the player does (Escape), which runs the dialog's
      // close handler and clears the modal state the panels gate on.
      gui.root().handleEvent(ui::UiEvent::keyDown(ui::Key::Escape));
      gui.update(m2, {1600, 1000}, 0.016f);
      HudLayer composed;
      gui.paint(composed, mats);
      bool sawTickText = false, sawFpsText = false;
      for (const auto& d : composed.draws) {
        if (d.text.find("TPS 128 (1s) frame") != std::string::npos) sawTickText = true;
        if (d.text.find("debug frame") != std::string::npos) sawFpsText = true;
      }
      check(sawTickText && sawFpsText, "paint ruling: the composed layer carries the readout text (tick + F3 lines)");
    }
    // Build 189 correction: this assertion used to read "count >= the
    // count captured with the dialog open", which the paint-cache
    // stacking defect satisfied forever (the layer only ever grew) and
    // the honest layer fails (closing the dialog rightly SHRINKS it).
    // The true invariant: with the dialog closed (the Escape above),
    // the composed layer is the full HUD and replay-stable in size.
    {
      HudLayer hudA, hudB;
      gui.paint(hudA, mats);
      const size_t hudSize = gui.root().paintDrawCount();
      gui.paint(hudB, mats);
      check(hudSize > 0 && gui.root().paintDrawCount() == hudSize,
            "paint ruling: the composed layer still carries the full HUD (stable size across replays)");
    }
    gui.update(m, {1600, 1000}, 0.016f);
  }

  gui.root().handleEvent(ui::UiEvent::keyDown(ui::Key::Escape));
  check(!gui.inventoryOpen() && closed == 1, "inventory: Escape closes it and tells the game");
  gui.openInventory(0);
  gui.closeInventory();
  check(!gui.inventoryOpen() && closed == 1 && !gui.root().modalOpen(), "inventory: closing from the game sends no callback");

  {  // Build 189 defect hunt (2026-11-09, JD's live playtest): JD's
    // machine boots at 1920x1080, so the auto GUI scale is 2 BEFORE the
    // screen is ever opened and openInventory BUILDS the screen at
    // scale 2. The rework block above opens the screen at 1600x1000
    // (scale 1) and only resizes the viewport afterwards, so it never
    // exercises a scale-2 build... which is how 189 shipped with the
    // frame in unscaled design units (1152x568) while every control
    // inside was built at scale 2: the dialog solved at half the size
    // its contents assumed, the Agent column and hotbar section
    // arranged past the dialog's bottom edge, and the stack captions
    // painted at the global text scale (48 px for "512") inside slots
    // solved to ~33 px. JD's hierarchy (2026-11-09 20:34 EST): global
    // frame = the screen; the inventory window is a subframe anchored
    // (centered) to it; everything inside anchors to the window frame.
    // This block is his configuration as a named case: settle at
    // 1920x1080 FIRST, then open, with every carry slot holding a full
    // 512 stack like his screenshot.
    for (int i = 0; i < 3; ++i) gui.update(m, {1920, 1080}, 0.016f);
    HudLayer hudOnly;
    gui.paint(hudOnly, mats);
    const size_t hudDraws = gui.root().paintDrawCount();
    std::vector<ItemStack> full;
    full.resize(size_t(kTotalSlots));
    for (int i = 0; i < kCarrySlots; ++i) full[size_t(i)] = ItemStack{3, 512};  // B9 four-row: 82 carry (was 50)
    for (int i = kCarrySlots; i < kTotalSlots; ++i) full[size_t(i)] = ItemStack{5, 1};
    gui.openInventory(0, full);
    for (int i = 0; i < 4; ++i) gui.update(m, {1920, 1080}, 0.016f);
    ui::cWidget* dlg2 = gui.root().topModal();
    const std::string tag189 = "build 189 (JD config, scale 2 built at 1920x1080): ";
    check(dlg2 && dlg2->rect().x >= 0 && dlg2->rect().y >= 0 && dlg2->rect().x + dlg2->rect().w <= 1920 &&
              dlg2->rect().y + dlg2->rect().h <= 1080,
          tag189 + "the dialog stays inside the viewport");
    ui::cWidget* frame2 = dlg2 ? dlg2->find("creative-global-frame") : nullptr;
    ui::cWidget* pickWin = dlg2 ? dlg2->find("creative-picker-frame") : nullptr;
    ui::cWidget* invWin = dlg2 ? dlg2->find("creative-inventory-frame") : nullptr;
    check(pickWin && invWin, tag189 + "picker and inventory are window subframes of the global frame (JD hierarchy)");
    check(frame2 && frame2->rect().w > 1500,
          tag189 + "the global frame carries the build scale (design solved in build units, not the unscaled 1152)");
    const auto insideT = [](const lithogui::Rect& r, const lithogui::Rect& box, float tol) {
      return r.x >= box.x - tol && r.y >= box.y - tol && r.x + r.w <= box.x + box.w + tol && r.y + r.h <= box.y + box.h + tol;
    };
    bool slotsInWindows = frame2 != nullptr;
    bool agentInWindow = false, hotInWindow = false;
    std::vector<const ui::cSlot*> capSlots;
    if (dlg2 && frame2) {
      const lithogui::Rect fb = frame2->rect();
      const auto walk = [&](const auto& self, lithogui::Widget& w, bool inScroll) -> void {
        if (auto* cw = dynamic_cast<ui::cWidget*>(&w)) {
          if (cw->facadeId() == "creative-palette") inScroll = true;
          if (cw->facadeId() == "agent-health") agentInWindow = invWin && insideT(w.rect(), invWin->rect(), 0.5f);
          if (cw->facadeId() == "creative-hotbar") hotInWindow = invWin && insideT(w.rect(), invWin->rect(), 0.5f);
          if (auto* s = dynamic_cast<ui::cSlot*>(&w); s && w.rect().w > 0) {
            const lithogui::Rect r = w.rect();
            if (!inScroll && !insideT(r, fb, 0.5f)) slotsInWindows = false;
            if (inScroll && pickWin && (r.x < pickWin->rect().x - 0.5f || r.x + r.w > pickWin->rect().x + pickWin->rect().w + 0.5f))
              slotsInWindows = false;
            if (!inScroll && invWin && !insideT(r, invWin->rect(), 0.5f) && !(pickWin && insideT(r, pickWin->rect(), 0.5f)))
              slotsInWindows = false;
            if (!s->caption().empty()) capSlots.push_back(s);
          }
        }
        for (const auto& c : w.children()) self(self, *c, inScroll);
      };
      walk(walk, *dlg2, false);
    }
    check(slotsInWindows, tag189 + "every slot sits inside the global frame and inside its window subframe");
    check(agentInWindow && hotInWindow, tag189 + "the Agent panel and the hotbar section stay inside the inventory window");
    // The composed layer: every "512" caption draw must land inside the
    // slot it belongs to (the caption scale solves from the slot, not
    // from the global theme scale), and every captioned slot must show
    // its icon draw (the icons JD could not see).
    HudLayer open1;
    gui.paint(open1, mats);
    const size_t openDraws = gui.root().paintDrawCount();
    int capDraws = 0, capInside = 0, iconSlots = 0;
    for (const auto& d : open1.draws) {
      if (d.text != "512") continue;
      ++capDraws;
      // A text draw carries no w/h (renderer.h): its painted extent is
      // the glyph cell (8x16) times the draw's scale, from its origin.
      const lithogui::Rect dr{d.x, d.y, float(d.text.size()) * 8.f * d.scale, 16.f * d.scale};
      for (const ui::cSlot* s : capSlots)
        if (insideT(dr, s->rect(), 1.5f)) { ++capInside; break; }
    }
    for (const ui::cSlot* s : capSlots) {
      // The slot's icon rendering: an icon draw in the real game, a
      // fallback swatch rect in this headless harness (no textures
      // are registered here, so the swatch path runs)... either
      // way it is one draw fitted strictly INSIDE the slot (the inset
      // inner rect), covering the slot's centre. The slot's own
      // background matches the slot rect exactly, so the strict inset
      // is what identifies the icon's draw.
      const ui::Vec2 c{s->rect().x + s->rect().w * 0.5f, s->rect().y + s->rect().h * 0.5f};
      for (const auto& d : open1.draws)
        if (d.text.empty() && d.x > s->rect().x + 1.f && d.y > s->rect().y + 1.f &&
            d.x + d.w < s->rect().x + s->rect().w - 1.f && d.y + d.h < s->rect().y + s->rect().h - 1.f &&
            c.x >= d.x && c.x <= d.x + d.w && c.y >= d.y && c.y <= d.y + d.h) { ++iconSlots; break; }
    }
    check(!capSlots.empty() && capDraws == int(capSlots.size()) && capInside == capDraws,
          tag189 + "every stack caption paints inside its own slot");
    check(!capSlots.empty() && iconSlots == int(capSlots.size()), tag189 + "every filled slot shows its item icon");
    // The stacking pin (JD: "something is getting redrawn way too
    // much"; his 189 layer grew 49k -> 2.34M draws because a rebuild
    // APPENDED to the paint cache instead of clearing it): forced
    // rebuilds must reproduce the SAME composed size, never grow it.
    bool stable = true;
    ui::cWidget* anySlot = dlg2 ? findSlot(findSlot, *dlg2) : nullptr;
    for (int i = 0; i < 3 && anySlot; ++i) {
      gui.root().handleEvent(ui::UiEvent::move({anySlot->rect().x + 2.f + float(i) * 4.f, anySlot->rect().y + 2.f}));
      gui.update(m, {1920, 1080}, 0.016f);
      HudLayer again;
      gui.paint(again, mats);
      if (gui.root().paintDrawCount() != openDraws) stable = false;
    }
    check(stable && openDraws > hudDraws, tag189 + "rebuilds never grow the composed layer (no stacking)");
    // Close must clear the view: the game-asked close (JD's E key path)
    // and Escape both return the composed layer to the HUD alone.
    gui.closeInventory();
    for (int i = 0; i < 2; ++i) gui.update(m, {1920, 1080}, 0.016f);
    HudLayer afterClose;
    gui.paint(afterClose, mats);
    check(!gui.inventoryOpen() && !gui.root().modalOpen() && gui.root().paintDrawCount() == hudDraws,
          tag189 + "closing from the game empties the composed layer back to the HUD alone");
    gui.openInventory(0, full);
    for (int i = 0; i < 2; ++i) gui.update(m, {1920, 1080}, 0.016f);
    gui.root().handleEvent(ui::UiEvent::keyDown(ui::Key::Escape));
    for (int i = 0; i < 2; ++i) gui.update(m, {1920, 1080}, 0.016f);
    HudLayer afterEsc;
    gui.paint(afterEsc, mats);
    check(!gui.inventoryOpen() && gui.root().paintDrawCount() == hudDraws,
          tag189 + "Escape empties the composed layer back to the HUD alone");
    for (int i = 0; i < 3; ++i) gui.update(m, {1600, 1000}, 0.016f);  // settle back at the bench size
  }

  {  // Cursor-stack interactions (2026-11-11; plan section 10, the
    // module-suite half). These checks REWRITE the Item 4 put-back
    // checks at the same flats (plan 9.3): a press on a filled slot no
    // longer puts back... it dispatches cursorClick (the game lifts),
    // and putBack never fires. The gesture ALGEBRA (lift / place /
    // merge / swap, close chain, Q-from-the-hand) is asserted against
    // the real Game in the headless harness (testCursorStackGestures);
    // here the real tree proves dispatch, press timing, palette
    // routing, tab survival and the paint law.
    std::vector<ItemStack> snap;
    snap.resize(size_t(kTotalSlots));
    snap[18] = ItemStack{3, 64};
    snap[size_t(kCarrySlots)] = ItemStack{5, 1};  // first gear flat (82 since B9 four-row; was 50)
    gui.openInventory(0, snap);
    gui.update(m, {1600, 1000}, 0.016f);
    ui::cWidget* inv2 = gui.root().topModal();
    ui::cSlot* main0 = nullptr;
    ui::cSlot* worn0 = nullptr;
    const auto findByData = [](const auto& self, lithogui::Widget& w, int data, ui::cSlot*& out) -> void {
      if (auto* s = dynamic_cast<ui::cSlot*>(&w); s && s->data() == data) out = s;
      for (const auto& c : w.children()) self(self, *c, data, out);
    };
    if (inv2) {
      findByData(findByData, *inv2, (1 << 20) + 18, main0);
      findByData(findByData, *inv2, (1 << 20) + kCarrySlots, worn0);
    }
    // (a) The rewritten put-back checks (flats 18 and the first gear flat): the press
    // routes to the cursor gesture; putBack stays silent.
    putBackFlat = -1;
    cursorFlat = -1;
    cursorCalls = 0;
    if (main0) click(gui.root(), center(*main0));
    check(cursorFlat == 18 && cursorCalls == 1 && putBackFlat == -1,
          "cursor stack: pressing a filled main slot dispatches the cursor gesture (flat 18), never put-back");
    cursorFlat = -1;
    if (worn0) click(gui.root(), center(*worn0));
    check(cursorFlat == kCarrySlots && putBackFlat == -1,
          "cursor stack: pressing a filled worn slot dispatches the cursor gesture too (first gear flat, gear included)");
    // (b) Press timing (check 18): the DOWN alone dispatches, in that
    // frame; the UP adds nothing; a press on A released over B fires
    // only A. Exactly one dispatch per physical click.
    cursorFlat = -1;
    cursorCalls = 0;
    if (main0) {
      const ui::Vec2 c = center(*main0);
      gui.root().handleEvent(ui::UiEvent::move(c));
      gui.root().handleEvent(ui::UiEvent::down(c));
    }
    check(cursorFlat == 18 && cursorCalls == 1, "cursor stack (check 18): PointerDown alone dispatches the gesture (press edge)");
    if (main0) gui.root().handleEvent(ui::UiEvent::up(center(*main0)));
    check(cursorCalls == 1, "cursor stack (check 18): the release is inert... no second dispatch");
    cursorFlat = -1;
    cursorCalls = 0;
    if (main0 && worn0) {
      gui.root().handleEvent(ui::UiEvent::move(center(*main0)));
      gui.root().handleEvent(ui::UiEvent::down(center(*main0)));
      gui.root().handleEvent(ui::UiEvent::up(center(*worn0)));
    }
    check(cursorFlat == 18 && cursorCalls == 1,
          "cursor stack (check 18): press on A, release over B fires only A's gesture");
    // (c) Equipment tab: the give is unchanged while NOT carrying
    // (check 9's module half); while carrying, a palette press is the
    // discard and no give fires alongside it (check 8's module half).
    ui::cWidget* equipBtn = nullptr;
    const auto findBtn = [](const auto& self, lithogui::Widget& w, int want, ui::cWidget*& out, int& seen) -> void {
      if (auto* cw = dynamic_cast<ui::cWidget*>(&w); cw && cw->facadeId() == "creative-tab") {
        if (seen == want) out = cw;
        ++seen;
      }
      for (const auto& c : w.children()) self(self, *c, want, out, seen);
    };
    int seenBtn = 0;
    if (inv2) findBtn(findBtn, *inv2, 1, equipBtn, seenBtn);
    if (equipBtn) click(gui.root(), center(*equipBtn));
    gui.update(m, {1600, 1000}, 0.016f);
    gui.update(m, {1600, 1000}, 0.016f);  // second frame: the tab switch rebuilds the palette mid-frame; the new grid is laid out on the next update
    ui::cSlot* itemSlot = nullptr;
    if (inv2) {
      const auto findItemSlot = [](const auto& self, lithogui::Widget& w, ui::cSlot*& out) -> void {
        if (auto* s = dynamic_cast<ui::cSlot*>(&w);
            s && s->data() >= int(kFirstItemId) && s->data() < (1 << 20) && !out)
          out = s;
        for (const auto& c : w.children()) self(self, *c, out);
      };
      findItemSlot(findItemSlot, *inv2, itemSlot);
    }
    pickedItem = 0;
    paletteCursorCalls = 0;
    if (itemSlot) click(gui.root(), center(*itemSlot));
    check(pickedItem >= kFirstItemId && paletteCursorCalls == 0,
          "creative screen: the Equipment tab gives a registered item (pickItem fires; no discard while not carrying)");
    gui.refreshCursor(ItemStack{uint32_t(kWaferSilicon), 40});  // the game's display push: now carrying
    pickedItem = 0;
    if (itemSlot) click(gui.root(), center(*itemSlot));
    check(paletteCursorCalls == 1 && pickedItem == 0,
          "cursor stack (check 8, module half): a palette press while carrying discards... and does NOT also give");
    // (d) Background discard: a press on the palette panel's own
    // background (the note label's spot: inert content over the panel)
    // discards too; the whole left half is the target while carrying.
    ui::cWidget* note = inv2 ? inv2->find("palette-note") : nullptr;
    if (note) click(gui.root(), center(*note));
    check(paletteCursorCalls == 2, "cursor stack (check 8, module half): a palette-background press while carrying discards too");
    // (e) Tab survival (check 14, module half): carrying survives tab
    // switches (the palette subtree rebuilds; the cursor state lives
    // outside it). Switch Equipment -> All -> Equipment.
    ui::cWidget* allBtn = nullptr;
    seenBtn = 0;
    if (inv2) findBtn(findBtn, *inv2, 4, allBtn, seenBtn);
    if (equipBtn) click(gui.root(), center(*equipBtn));
    gui.update(m, {1600, 1000}, 0.016f);
    if (allBtn) click(gui.root(), center(*allBtn));
    gui.update(m, {1600, 1000}, 0.016f);
    check(gui.carrying() && gui.cursorDisplay().itemId == uint32_t(kWaferSilicon) && gui.cursorDisplay().count == 40 &&
              paletteCursorCalls == 2,
          "cursor stack (check 14, module half): carrying survives tab switches and palette rebuilds (tabs never discard)");
    // (f) Paint law (check 15): with a stack carried, pointer moves
    // WITHIN one slot (hover never changes) must not rebuild the main
    // layer or grow the icon census; the dynamic layer carries the
    // cursor cell, and the cell tracks the pointer.
    {
      {
        HudLayer warm;
        for (int i = 0; i < 4; ++i) { gui.update(m, {1600, 1000}, 0.016f); gui.paint(warm, mats); }
      }
      ui::cWidget* host = gui.cursorHostForTest();
      const bool hostOk = host && host->visible();
      bool tracks = hostOk;
      bool mainRested = true;
      bool censusRested = true;
      if (main0) {
        const ui::Vec2 base = center(*main0);
        gui.root().handleEvent(ui::UiEvent::move(base));
        gui.update(m, {1600, 1000}, 0.016f);
        HudLayer settle;
        gui.paint(settle, mats);
        // Catch-rule census (measure corrected twice, 2026-11-11; see
        // the implementation report's finish-run addendum): the catch
        // rule forbids re-CATCHING... a re-catch grows the atlas tile
        // set and bumps its version (publish() mints a new snapshot
        // only when a tile was added). The composed-layer measures an
        // earlier draft of this check used cannot see that quantity in
        // THIS scene: the module paints with empty icon providers
        // (default materials, layer outcomes not swatches), so the
        // textured lane never engages here... paintIconCount() is
        // identically 0 and no composed frame carries an atlas stamp,
        // in any process, and a "> 0 icons" guard on those numbers is
        // unsatisfiable by construction. Measure the rule where it
        // lives: the atlas builder's own tile census and version,
        // sampled around every move. The census is non-vacuous (the
        // module's textured-icon checks above have already caught
        // tiles, guarded by tiles0 > 0): any catch during the moves,
        // from any surface painting in this scene, moves either number.
        const size_t tiles0 = litholattice::hudIconAtlas().tileCount();
        const uint64_t atlasVer0 = litholattice::hudIconAtlas().version();
        const uint64_t mc = gui.root().mainPaintCalls();
        for (int i = 1; i <= 4; ++i) {
          const ui::Vec2 p{base.x + float(i), base.y + float(i)};
          gui.root().handleEvent(ui::UiEvent::move(p));
          gui.update(m, {1600, 1000}, 0.016f);
          HudLayer frame;
          gui.paint(frame, mats);
          if (gui.root().mainPaintCalls() != mc) mainRested = false;
          if (litholattice::hudIconAtlas().tileCount() != tiles0 ||
              litholattice::hudIconAtlas().version() != atlasVer0) censusRested = false;
          if (host) {
            const float cx = host->rect().x + host->rect().w * 0.5f, cy = host->rect().y + host->rect().h * 0.5f;
            if (std::fabs(cx - p.x) > 1.f || std::fabs(cy - p.y) > 1.f) tracks = false;
          }
        }
        check(mainRested, "cursor stack (check 15): pointer moves while carrying never rebuild the main layer");
        check(censusRested && tiles0 > 0,
              "cursor stack (check 15): the icon census does not grow across cursor moves and the atlas is never re-caught (tile count and version constant... the tile is re-sampled, never re-caught)");
        check(tracks && gui.root().dynamicDrawCount() > 0,
              "cursor stack (check 15): the carried cell rides the dynamic layer, centred on the pointer");
      } else {
        check(false, "cursor stack (check 15): main slot not found for the paint-law probe");
      }
    }
    gui.refreshCursor(ItemStack{});
    gui.closeInventory();
    check(!gui.carrying() && gui.cursorHostForTest() && !gui.cursorHostForTest()->visible(),
          "cursor stack: closing the screen clears the cursor display (the game owns the truth and runs the return chain)");
  }
}

void testSettingsFile(Checker& check) {
  namespace fs = std::filesystem;
  const fs::path dir = fs::temp_directory_path() / ("lithokraft_settings_selftest-" + std::to_string(::getpid()));
  const fs::path p = dir / "settings.json";
  std::error_code ec;
  fs::remove_all(dir, ec);

  GameSettings s;
  s.masterVolume = 0.25f; s.muted = true; s.hudScale = 2; s.displayMode = 3;
  s.vsync = true; s.tps = 256; s.tickRadius = 64; s.euv = false; s.pbrTier = 3;
  check(s.save(p.string()), "settings: save writes config/settings.json (creating directories)");
  const GameSettings r = GameSettings::load(p.string());
  const bool roundTrip = std::fabs(r.masterVolume - 0.25f) < 1e-6 && r.muted && r.hudScale == 2 &&
                         r.displayMode == 3 && r.vsync && r.tps == 256 && r.tickRadius == 64 && !r.euv &&
                         r.pbrTier == 3;
  check(roundTrip, "settings: load round-trips every field");

  writeFile(p, "{ not json");
  const GameSettings corrupt = GameSettings::load(p.string());
  check(corrupt.tps == 32 && corrupt.tickRadius == 6 /* JD 2026-10-24 */ && !corrupt.muted && corrupt.euv &&
        corrupt.pbrTier == 1,
        "settings: a corrupt file falls back to defaults");

  writeFile(p, R"({"masterVolume": 9, "tps": 100, "tickRadius": -5, "hudScale": 7, "displayMode": 42, "pbrTier": 99})");
  const GameSettings c = GameSettings::load(p.string());
  check(c.masterVolume == 1.f && c.tps == 128 && c.tickRadius == 0 && c.hudScale == 4 && c.displayMode == 3 &&
        c.pbrTier == 3,
        "settings: out-of-range values clamp to the nearest valid (volume 1, tps 128, radius 0, pbrTier 3)");

  const GameSettings missing = GameSettings::load((dir / "nope.json").string());
  check(missing.tps == 32 && missing.masterVolume == 0.6f, "settings: a missing file loads defaults");
  fs::remove_all(dir, ec);
}

void testLaunchMenu(Checker& check) {
  bool played = false, quit = false, settingsOpened = false;
  cGameUi gui({{}, {}, {}, {}, [&] { quit = true; }, [&] { played = true; }, {}, [&] { settingsOpened = true; }, {}});
  HudModel m;
  m.visible = false;  // the game hides the HUD under the menu overlay
  gui.openMenu("9.9.9-test");
  gui.update(m, {1280, 720}, 0.016f);
  check(gui.menuOpen(), "menu: openMenu builds the overlay");
  check(gui.root().theme().style("title").textScale == 4.5f, "menu: the 'title' style gives the big title text");
  ui::cLabel* title = gui.root().root().findAs<ui::cLabel>("menu-title");
  ui::cLabel* tag = gui.root().root().findAs<ui::cLabel>("menu-tagline");
  ui::cLabel* ver = gui.root().root().findAs<ui::cLabel>("menu-version");
  check(title && title->text() == "LITHOKRAFT", "menu: big LITHOKRAFT title");
  check(tag && tag->text() == "A LITHOLATTICE GAME", "menu: tagline reads 'A LITHOLATTICE GAME'");
  check(ver && ver->text().find("9.9.9-test") != std::string::npos, "menu: version footer shows the plumbed version");
  check(gui.root().root().find("menu-divider") != nullptr, "menu: divider under the tagline");
  ui::cButton* play = gui.root().root().findAs<ui::cButton>("menu-play");
  ui::cButton* set = gui.root().root().findAs<ui::cButton>("menu-settings");
  ui::cButton* exit = gui.root().root().findAs<ui::cButton>("menu-exit");
  check(play && set && exit, "menu: Play / Settings / Exit buttons");
  if (play) click(gui.root(), center(*play));
  check(played, "menu: Play dispatches the play callback");
  if (set) click(gui.root(), center(*set));
  check(settingsOpened, "menu: Settings dispatches openSettings");
  if (exit) click(gui.root(), center(*exit));
  check(quit, "menu: Exit dispatches quit");
  played = false;
  if (play) {
    gui.root().setFocus(play);  // keyboard-first: Enter on the focused button
    gui.root().handleEvent(ui::UiEvent::keyDown(ui::Key::Enter));
  }
  check(played, "menu: Enter on the focused Play button starts the game");
  gui.setHudScalePref(2);
  gui.update(m, {1280, 720}, 0.016f);
  check(gui.root().theme().scale() == 2.f, "menu: HUD scale preference overrides the auto scale");
  gui.setHudScalePref(0);
  gui.update(m, {1280, 720}, 0.016f);
  check(gui.root().theme().scale() == 1.f, "menu: HUD scale 0 falls back to auto");
  // Cortana 2026-10-16 (JD GUI overhaul): whole-number auto scale from the framebuffer, user 1-4
  // capped to what fits, and integer glyph scales (crisp text at any theme textScale).
  check(cGameUi::effectiveGuiScale(0, 1280, 720) == 1 && cGameUi::effectiveGuiScale(0, 1920, 1080) == 2 &&
            cGameUi::effectiveGuiScale(0, 2560, 1080) == 2 && cGameUi::effectiveGuiScale(0, 1024, 768) == 1 &&
            cGameUi::effectiveGuiScale(0, 3840, 2160) == 4 && cGameUi::effectiveGuiScale(0, 640, 360) == 1,
        "gui scale: auto = largest whole scale fitting 960x540 (720p 1, 1080p 2, 4K 4, min 1)");
  check(cGameUi::effectiveGuiScale(4, 3840, 2160) == 4 && cGameUi::effectiveGuiScale(4, 1280, 720) == 2 &&
            cGameUi::effectiveGuiScale(3, 1920, 1080) == 3 && cGameUi::effectiveGuiScale(1, 3840, 2160) == 1,
        "gui scale: a fixed 1-4 choice is kept, capped to the 640x360 safe canvas");
  {
    ui::Theme t = ui::Theme::defaults();
    bool crisp = true;
    for (float k : {1.f, 2.f, 3.f, 4.f})
      for (float ts : {0.75f, 1.f, 1.5f, 4.5f}) {
        t.setScale(k);
        const float px = t.pixelTextScale(ts);
        crisp = crisp && px >= 1.f && px == float(int(px)) && t.glyphSize(ts).x == t.glyph().x * px;
      }
    check(crisp, "gui text: every glyph scale is a whole number of pixels per font texel");
  }
  gui.closeMenu();
  check(!gui.menuOpen() && gui.root().root().find("menu") == nullptr, "menu: closeMenu removes the overlay");
}

void testPauseMenu(Checker& check) {
  bool resumed = false, quit = false, toMenu = false, settingsOpened = false;
  cGameUi gui({{}, {}, {}, [&] { resumed = true; }, [&] { quit = true; },
              {}, [&] { toMenu = true; }, [&] { settingsOpened = true; }, {}});
  HudModel m;
  const auto pauseButton = [&](const char* id) {
    ui::cWidget* modal = gui.root().topModal();
    return modal ? modal->findAs<ui::cButton>(id) : nullptr;
  };
  gui.openPause();
  gui.update(m, {1280, 720}, 0.016f);
  ui::cButton* resume = pauseButton("pause-resume");
  ui::cButton* set = pauseButton("pause-settings");
  ui::cButton* exitMenu = pauseButton("pause-exit-menu");
  ui::cButton* quitBtn = pauseButton("pause-quit");
  const bool four = resume && resume->text() == "Resume" && set && set->text() == "Settings" &&
                    exitMenu && exitMenu->text() == "Exit to menu" && quitBtn && quitBtn->text() == "Quit to desktop";
  check(four, "pause: Resume / Settings / Exit to menu / Quit to desktop");
  if (resume) click(gui.root(), center(*resume));
  check(resumed && !gui.pauseOpen(), "pause: Resume closes the dialog and reports it");
  gui.openPause();
  gui.update(m, {1280, 720}, 0.016f);
  if (ui::cButton* s2 = pauseButton("pause-settings")) click(gui.root(), center(*s2));
  check(settingsOpened && gui.pauseOpen(), "pause: Settings reports openSettings and keeps the pause dialog");
  if (ui::cButton* e2 = pauseButton("pause-exit-menu")) click(gui.root(), center(*e2));
  check(toMenu, "pause: Exit to menu dispatches exitToMenu");
  gui.openPause();
  gui.update(m, {1280, 720}, 0.016f);
  if (ui::cButton* q2 = pauseButton("pause-quit")) click(gui.root(), center(*q2));
  check(quit, "pause: Quit to desktop dispatches quit");
}

void testSettingsDialog(Checker& check) {
  GameSettings changed;
  bool fired = false;
  cGameUi gui({{}, {}, {}, {}, {}, {}, {}, {}, [&](const GameSettings& s) { changed = s; fired = true; }});
  HudModel m;
  GameSettings cur;
  cur.masterVolume = 0.75f; cur.muted = true; cur.hudScale = 2; cur.displayMode = 3;
  cur.vsync = true; cur.tps = 128; cur.tickRadius = 12; cur.euv = false;
  gui.openSettings(cur);
  gui.update(m, {1280, 720}, 0.016f);
  check(gui.settingsOpen(), "settings: openSettings shows the dialog");
  ui::cWidget* dlg = gui.root().topModal();
  check(dlg && dlg->facadeId() == "settings", "settings: the dialog is the top modal");
  ui::cTabView* tabs = dlg ? dlg->findAs<ui::cTabView>("settings-tabs") : nullptr;
  check(tabs && tabs->count() == 4, "settings: Audio / Video / Graphics / Sim tabs");  // Cortana 2026-10-16: + Graphics
  ui::cSlider* vol = dlg ? dlg->findAs<ui::cSlider>("set-volume") : nullptr;
  check(vol && std::fabs(vol->value() - 75.f) < 0.01f, "settings: volume slider initialised from the current settings");
  if (vol) vol->setValue(40.f, true);
  check(fired && std::fabs(changed.masterVolume - 0.4f) < 1e-6, "settings: the volume slider reports changes");
  fired = false;
  ui::cCheckbox* mute = dlg ? dlg->findAs<ui::cCheckbox>("set-mute") : nullptr;
  check(mute && mute->checked(), "settings: mute checkbox initialised");
  if (mute) mute->setChecked(false, true);
  check(fired && !changed.muted, "settings: the mute checkbox reports changes");
  fired = false;
  ui::cDropdown* disp = dlg ? dlg->findAs<ui::cDropdown>("set-display") : nullptr;
  check(disp && disp->selected() == 3, "settings: display mode dropdown initialised (Fullscreen)");
  if (disp) disp->setSelected(1, true);
  check(fired && changed.displayMode == 1, "settings: the display dropdown reports changes");
  fired = false;
  ui::cDropdown* hud = dlg ? dlg->findAs<ui::cDropdown>("set-hud") : nullptr;
  if (hud) hud->setSelected(0, true);
  check(fired && changed.hudScale == 0, "settings: the HUD scale dropdown reports changes");
  fired = false;
  ui::cDropdown* tps = dlg ? dlg->findAs<ui::cDropdown>("set-tps") : nullptr;
  check(tps && tps->selected() == 2, "settings: tick rate dropdown initialised (128)");
  if (tps) tps->setSelected(4, true);
  check(fired && changed.tps == 512, "settings: the tick rate dropdown reports changes");
  fired = false;
  ui::cNumberField* radius = dlg ? dlg->findAs<ui::cNumberField>("set-radius") : nullptr;
  check(radius && radius->value() == 12, "settings: tick radius field initialised");
  if (radius) radius->setValue(16, true);
  check(fired && changed.tickRadius == 16, "settings: the tick radius field reports changes");
  fired = false;
  ui::cCheckbox* euv = dlg ? dlg->findAs<ui::cCheckbox>("set-euv") : nullptr;
  if (euv) euv->setChecked(true, true);
  check(fired && changed.euv, "settings: the EUV checkbox reports changes");
  fired = false;
  ui::cDropdown* pbr = dlg ? dlg->findAs<ui::cDropdown>("set-pbrtier") : nullptr;
  check(pbr && pbr->selected() == cur.pbrTier, "settings: PBR transparency dropdown initialised from the current settings");
  if (pbr) pbr->setSelected(3, true);
  check(fired && changed.pbrTier == 3, "settings: the PBR transparency dropdown reports changes (live tier)");
  fired = false;
  ui::cLabel* pbrHint = dlg ? dlg->findAs<ui::cLabel>("pbr-tier-hint") : nullptr;
  check(pbrHint != nullptr, "settings: PBR tier pairing hint label present");
  gui.root().handleEvent(ui::UiEvent::keyDown(ui::Key::Escape));
  check(!gui.settingsOpen(), "settings: Escape closes the dialog");
}

// 2026-11-07 (JD bug report on Alpha build 16): clicking Play opened the
// world picker as a floating title with its content collapsed/clipped at the
// top-left of the screen. Probe: open the REAL picker through a cGameUi at
// JD's 1920x1080, lay out one frame, and pin the dialog geometry (centered,
// full content inside the viewport) plus the Escape behavior (picker closes,
// nothing else fires).
void testWorldPicker(Checker& check) {
  cGameUi gui({{}, {}, {}, {}, {}, {}, {}, {}, {}});
  HudModel m;
  m.visible = false;
  gui.update(m, {1920, 1080}, 0.016f);  // one frame so theme scale settles
  cWorldPicker picker;
  bool picked = false;
  picker.open(gui.root(), std::max(1.f, gui.root().theme().scale()), 0, {"spawn"},
              [&](int, const std::string&) { picked = true; }, {});
  gui.update(m, {1920, 1080}, 0.016f);  // the frame that lays the popup out
  check(picker.isOpen(), "world picker: dialog opens over the menu root");
  // The dialog lives in the popup tree, not under root(): the facade tracks
  // it in the modal stack (topModal = most recent).
  ui::cWidget* dlg = gui.root().topModal();
  if (auto* dd = dynamic_cast<ui::cDialog*>(dlg)) {
    std::printf("    [probe] dialog exempt=%d margin=(%.0f,%.0f,%.0f,%.0f) fixed=(%.0f,%.0f) ownersPending=%zu\n",
                dd->layoutExempt ? 1 : 0, dd->margin.l, dd->margin.t, dd->margin.r, dd->margin.b,
                dd->fixedW, dd->fixedH, gui.root().popupOwnersPending());
    {
      const lithogui::Rect tr = dd->treeRect();
      std::printf("    [probe] dialog treeRect: %.0f,%.0f %.0fx%.0f | raw rect: %.0f,%.0f %.0fx%.0f\n",
                  tr.x, tr.y, tr.w, tr.h, dd->rect().x, dd->rect().y, dd->rect().w, dd->rect().h);
    }
    // Bisect the layout chain inside ONE update: sample the dialog rect
    // after each stage of cGameUi::update's tail (update(dt), then layout).
    gui.root().update(0.016f);
    const lithogui::Rect s1 = dd->rect();
    std::printf("    [probe] dialog rect after root.update(dt): %.0f,%.0f %.0fx%.0f\n", s1.x, s1.y, s1.w, s1.h);
    gui.root().layout({1920, 1080});
    const lithogui::Rect s2 = dd->rect();
    std::printf("    [probe] dialog rect after root.layout(vp): %.0f,%.0f %.0fx%.0f\n", s2.x, s2.y, s2.w, s2.h);
    // Natural flow only (no manual place): open, one update, geometry.
    gui.update(m, {1920, 1080}, 0.016f);
    const lithogui::Rect r2 = dd->rect();
    std::printf("    [probe] dialog rect after 2nd update: %.0f,%.0f %.0fx%.0f\n", r2.x, r2.y, r2.w, r2.h);
  }
  if (dlg) {
    ui::cDialog* dd = dynamic_cast<ui::cDialog*>(dlg);
    if (dd) {
      const ui::Vec2 bm = dd->body().measure(gui.root().theme());
      std::printf("    [probe] dialog body measure: %.1f x %.1f\n", bm.x, bm.y);
      for (auto& c : dd->body().children()) {
        auto* cw = dynamic_cast<ui::cWidget*>(c.get());
        if (!cw) continue;
        const ui::Vec2 cm = cw->measure(gui.root().theme());
        std::printf("    [probe] body child '%s' measure: %.1f x %.1f\n", cw->facadeId().c_str(), cm.x, cm.y);
        for (auto& g : cw->children()) {
          auto* gw = dynamic_cast<ui::cWidget*>(g.get());
          if (!gw) continue;
          const ui::Vec2 gm = gw->measure(gui.root().theme());
          std::printf("    [probe]   grandchild '%s' measure: %.1f x %.1f (layout size %.1f x %.1f)\n",
                      gw->facadeId().c_str(), gm.x, gm.y, gw->layout().size.x, gw->layout().size.y);
        }
      }
    }
  }
  check(dlg != nullptr && dlg->facadeId() == "world-picker", "world picker: dialog is the tracked top modal");
  if (dlg) {
    // Geometry in TREE space (what the player sees / hit-testing uses):
    // a dialog hosted on a popup lives under the popup's own coordinate
    // frame, so raw rect() can be a design-space projection while
    // treeRect() carries the real tree placement (2026-11-07).
    const lithogui::Rect r = dlg->treeRect();
    std::printf("    [probe] world picker dialog rect: x=%.1f y=%.1f w=%.1f h=%.1f\n", r.x, r.y, r.w, r.h);
    check(r.w >= 500 && r.h >= 200, "world picker: dialog has real size (not collapsed)");
    check(r.x >= 0 && r.y >= 0 && r.x + r.w <= 1920 && r.y + r.h <= 1080,
          "world picker: dialog sits inside the viewport");
    ui::cWidget* list = dlg->find("world-list");
    ui::cWidget* first = nullptr;
    if (list && !list->children().empty()) first = dynamic_cast<ui::cWidget*>(list->children().front().get());
    if (first) {
      const lithogui::Rect fr = first->treeRect();
      std::printf("    [probe] first world button rect: x=%.1f y=%.1f w=%.1f h=%.1f\n", fr.x, fr.y, fr.w, fr.h);
      check(fr.w >= 200 && fr.h >= 20, "world picker: world buttons have real size");
      check(fr.x >= r.x && fr.y >= r.y && fr.x + fr.w <= r.x + r.w + 1 && fr.y + fr.h <= r.y + r.h + 1,
            "world picker: world buttons land inside the dialog");
    } else {
      check(false, "world picker: world list has buttons");
    }
  }
  const bool consumed = gui.root().handleEvent(ui::UiEvent::keyDown(ui::Key::Escape));
  check(consumed, "world picker: Escape is consumed by the UI (never reaches the game's quit path)");
  check(!picker.isOpen(), "world picker: Escape closes the picker");
  check(!picked, "world picker: Escape does not pick a world");
}

}  // namespace

// B9 hotbar-render fix (2026-11-11): the Vulkan backend keeps one HUD
// overlay range per frame in flight and decides per slot whether to
// refill it. A hotbar selection move changes content WITHOUT changing
// the vertex count, and the expansion's "fresh" flag is true for one
// frame only. This replays the backend's two-slot alternation on the
// CPU (no device): the old count-only rule must reproduce the stale
// slot (the presented frames alternate old/new selection), and the
// key-aware hudSlotRangeNeedsCopy rule must keep every presented frame
// equal to the current expansion, and copy nothing once settled.
void testHudSlotResidency(Checker& check) {
  using namespace litholattice;
  auto hotbar = [](int selected) {
    HudLayer hud;
    for (int i = 0; i < 9; ++i) hud.rects.push_back({10.f + 44.f * i, 600.f, 40.f, 40.f, 0.2f, 0.2f, 0.2f, 0.8f});
    hud.rects.push_back({10.f + 44.f * selected - 2.f, 598.f, 44.f, 44.f, 1.f, 1.f, 1.f, 0.95f});  // selection frame
    return hud;
  };
  const HudLayer a = hotbar(0), b = hotbar(3);
  constexpr size_t kSlots = 2;  // kFramesInFlight
  HudVertexCache cache;
  std::vector<HudVertex> verts;
  std::array<HudSlotRange, kSlots> ledger{};
  std::array<size_t, kSlots> oldCount{SIZE_MAX, SIZE_MAX};
  std::array<std::vector<HudVertex>, kSlots> resNew, resOld;
  bool newAllCurrent = true, oldSawStale = false, sameCount = true;
  int settledCopies = 0;
  auto same = [](const std::vector<HudVertex>& x, const std::vector<HudVertex>& y) {
    return x.size() == y.size() && (x.empty() || std::memcmp(x.data(), y.data(), x.size() * sizeof(HudVertex)) == 0);
  };
  size_t countA = 0;
  for (int frame = 0; frame < 16; ++frame) {
    const HudLayer& cur = frame < 6 ? a : (frame < 11 ? b : a);  // select 0 -> 3 -> back to 0
    const size_t s = size_t(frame) % kSlots;
    const bool fresh = buildHudVerticesCached(cur, cache, verts);
    if (frame == 0) countA = verts.size();
    if (verts.size() != countA) sameCount = false;
    const size_t count = verts.size();
    // Old rule (pre-B9): fresh || count mismatch.
    if (fresh || oldCount[s] != count) resOld[s] = verts;
    oldCount[s] = count;
    if (!same(resOld[s], verts)) oldSawStale = true;
    // New rule.
    const bool copy = hudSlotRangeNeedsCopy(ledger[s], cache.key, count, 0) || fresh;
    if (copy) resNew[s] = verts;
    if (!same(resNew[s], verts)) newAllCurrent = false;
    if (copy && (frame == 4 || frame == 5 || frame == 9 || frame == 10 || frame >= 13)) ++settledCopies;
  }
  check(sameCount, "B9 hud residency: selection move keeps the HUD vertex count (the case count-only residency misses)");
  check(oldSawStale, "B9 hud residency: old count-only rule leaves the other in-flight slot stale (reproduces the flicker)");
  check(newAllCurrent, "B9 hud residency: key-aware rule presents the current selection on every frame, both slots");
  check(settledCopies == 0, "B9 hud residency: settled frames copy nothing (vertex persistence kept)");
  // Offset move: same key and count, different address must refill.
  HudSlotRange r;
  check(hudSlotRangeNeedsCopy(r, 7, 12, 0), "B9 hud residency: empty slot range copies");
  check(!hudSlotRangeNeedsCopy(r, 7, 12, 0), "B9 hud residency: resident range skips");
  check(hudSlotRangeNeedsCopy(r, 7, 12, 6), "B9 hud residency: relocated range copies");
  check(hudSlotRangeNeedsCopy(r, 8, 12, 6), "B9 hud residency: same-count content change copies");
  r.reset();
  check(hudSlotRangeNeedsCopy(r, 8, 12, 6), "B9 hud residency: reset (grown buffer) range copies");
}

// C9 2026-11-12 (JD): the HUD hotbar's selected-slot outline has its own steady style (no blink /
// pulse: the colour is a constant style, not animated), and only the hotbar uses it.
// D9 2026-11-12 (JD): deeper, more saturated violet #8A2EFF, opaque, 3 px thick.
void testHotbarOutlinePurple(Checker& check) {
  const ui::Theme t = ui::Theme::defaults();
  const ui::Color hc = t.style("hotbar_slot").accent, sc = t.style("slot").accent;
  auto nearC = [](float a, float b) { return std::fabs(a - b) < 0.5f / 255.f; };
  check(t.hasStyle("hotbar_slot") && nearC(hc.r, 138.f / 255.f) && nearC(hc.g, 46.f / 255.f) && nearC(hc.b, 1.f) && nearC(hc.a, 1.f),
        "hotbar (D9): the built-in hotbar outline colour is #8A2EFF (138,46,255), opaque");
  check(t.style("hotbar_slot").selectedWidth == 3.f, "hotbar (D9): the built-in hotbar selected outline is 3 px thick");
  {
    auto sat = [](float r, float g, float b) { const float mx = std::max({r, g, b}), mn = std::min({r, g, b}); return mx > 0 ? (mx - mn) / mx : 0.f; };
    auto lum = [](float r, float g, float b) {
      auto lin = [](float c) { return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); };
      return 0.2126f * lin(r) + 0.7152f * lin(g) + 0.0722f * lin(b);
    };
    const float c9r = 169.f / 255.f, c9g = 112.f / 255.f, c9b = 1.f;
    const ui::Color bg = t.style("hotbar_slot").background;
    const float contrast = (lum(hc.r, hc.g, hc.b) + 0.05f) / (lum(bg.r, bg.g, bg.b) + 0.05f);
    check(sat(hc.r, hc.g, hc.b) > sat(c9r, c9g, c9b) + 0.2f && lum(hc.r, hc.g, hc.b) < lum(c9r, c9g, c9b) && contrast >= 3.f,
          "hotbar (D9): #8A2EFF is more saturated and deeper than C9's #A970FF and keeps >= 3:1 contrast on the slot background");
  }
  check(nearC(sc.r, 1.f) && nearC(sc.g, 1.f) && nearC(sc.b, 1.f), "hotbar (C9): the generic slot outline stays white (other chrome unchanged)");
  cGameUi gui(cGameUi::Callbacks{});
  HudModel m;
  m.hotbar = {ItemStack{1, 1}, ItemStack{2, 1}};
  m.selected = 1;
  bool all = true, steady = true;
  size_t n = 0;
  for (int f = 0; f < 6; ++f) {
    gui.update(m, {1920, 1080}, 0.25f);
    const ui::cWidget* bar = gui.root().root().find("hotbar");
    if (!bar) { all = false; break; }
    n = bar->children().size();
    for (const auto& c : bar->children()) {
      const auto* sl = dynamic_cast<const ui::cSlot*>(c.get());
      all = all && sl && sl->styleClass() == "hotbar_slot";
    }
    const ui::Color now = gui.root().theme().style("hotbar_slot").accent;
    steady = steady && nearC(now.r, hc.r) && nearC(now.g, hc.g) && nearC(now.b, hc.b) && nearC(now.a, hc.a);
  }
  check(all && n == size_t(kHotbarSlots), "hotbar (D9): every HUD hotbar slot draws its selected outline with the hotbar_slot (#8A2EFF) style");
  check(steady, "hotbar (C9): the hotbar outline colour is steady over time (no blink / pulse)");
}

int runGameUiSelfTest() {
  Checker check;
  testHotbarOutlinePurple(check);  // C9 2026-11-12 (JD): hotbar outline style; D9: #8A2EFF, 3 px
  testHudRenderer(check);
  testHudTexturedIcons(check);
  testHudSlotResidency(check);  // B9 hotbar-render fix
  testScriptBindings(check);
  testScriptEditor(check);
  testGameUi(check);
  testSettingsFile(check);
  testLaunchMenu(check);
  testWorldPicker(check);
  testPauseMenu(check);
  testSettingsDialog(check);
  return check.fails();
}

}  // namespace lithokraft
