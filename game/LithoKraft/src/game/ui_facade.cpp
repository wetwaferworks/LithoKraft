// ui_facade.cpp: the facade core - cWidget (tree, handlers, layout, paint)
// and cUiRoot (context ownership, event routing, modal popups) - over
// Litho GUI. See ui_facade.h for the architecture.
//
// Paint model: paint() implementations emit into a per-frame PaintSink (a
// lithogui::DrawList plus the active Theme). cUiRoot::paint runs the tree's
// paintTree into a scratch list, feeds it (and the open popups) through the
// context's draw, and hands the finished list to the engine's
// cHudUiRenderer, which is exactly how the new engine consumes Litho GUI.
#include "lithokraft/game/ui_facade.h"
#include <cmath>
#include "lithokraft/game/ui_widgets.h"  // DialogHost::finishDialogLayout (2026-11-07)
#include "litholattice/render/hud_ui_renderer.h"

namespace lithokraft {
namespace ui {
namespace {

Vec2 max2(Vec2 a, Vec2 b) { return {std::max(a.x, b.x), std::max(a.y, b.y)}; }

/// Renderer bridging Painter verbs to the frame's DrawList (paint sink).
class SinkRenderer final : public IUiRenderer {
 public:
  void fillRect(const Rect& r, Color c) override {
    if (paintSink().dl) paintSink().dl->fillRect(toGui(r), toGui(c));
  }
  void drawText(Vec2 pos, std::string_view text, Color c, float scale) override {
    PaintSink& sink = paintSink();
    if (!sink.dl || !sink.theme) return;
    lithogui::TextStyle ts;
    ts.size = sink.theme->glyph().y * scale;
    ts.lineHeightMul = 1.0f;
    sink.dl->textRun({pos.x, pos.y, float(text.size()) * sink.theme->glyph().x * scale, ts.size},
                     std::string(text), ts, toGui(c), ts.size * 0.8f);
  }
  void drawImage(const Rect& r, std::string_view image, Color tint) override {
    PaintSink& sink = paintSink();
    if (!sink.dl) return;
    const int id = iconTextureId(std::string(image));
    noteIconTint(id, tint);
    sink.dl->imageQuad(toGui(r), id, toGui(tint));  // full tint: RGB reaches the HUD icon path (Phase B)
  }
  void pushClip(const Rect& r) override {
    if (paintSink().dl) paintSink().dl->pushClip(toGui(r));
  }
  void popClip() override {
    if (paintSink().dl) paintSink().dl->popClip();
  }
};

}  // namespace

lithogui::KeyEvent toGuiKeyEvent(const UiEvent& e) {
  lithogui::KeyEvent k;
  k.down = e.type == UiEvent::Type::KeyDown;
  k.shift = e.shift();
  k.ctrl = e.ctrl();
  k.alt = e.alt();
  switch (e.key) {
    case Key::Tab: k.key = lithogui::KEY_TAB; break;
    case Key::Enter: k.key = lithogui::KEY_ENTER; break;
    case Key::Escape: k.key = lithogui::KEY_ESCAPE; break;
    case Key::Backspace: k.key = lithogui::KEY_BACKSPACE; break;
    case Key::Delete: k.key = lithogui::KEY_DELETE; break;
    case Key::Home: k.key = lithogui::KEY_HOME; break;
    case Key::End: k.key = lithogui::KEY_END; break;
    case Key::Left: k.key = lithogui::KEY_LEFT; break;
    case Key::Right: k.key = lithogui::KEY_RIGHT; break;
    case Key::Up: k.key = lithogui::KEY_UP; break;
    case Key::Down: k.key = lithogui::KEY_DOWN; break;
    case Key::Space: k.ch = U' '; break;
    case Key::Grave: k.ch = U'~'; break;
    case Key::A: k.ch = e.shift() ? U'A' : U'a'; break;
    case Key::C: k.ch = e.shift() ? U'C' : U'c'; break;
    case Key::S: k.ch = e.shift() ? U'S' : U's'; break;
    case Key::V: k.ch = e.shift() ? U'V' : U'v'; break;
    case Key::X: k.ch = e.shift() ? U'X' : U'x'; break;
    case Key::Y: k.ch = e.shift() ? U'Y' : U'y'; break;
    case Key::Z: k.ch = e.shift() ? U'Z' : U'z'; break;
    default: break;
  }
  if (e.type == UiEvent::Type::Text) {
    k.down = true;
    k.ch = e.codepoint;
  }
  return k;
}

// ------------------------------------------------------------- paint sink --
PaintSink& paintSink() {
  static PaintSink sink;
  return sink;
}

Painter sinkPainter(const Theme& theme) {
  static SinkRenderer renderer;
  return Painter(renderer, theme);
}

namespace {
std::vector<std::string>& iconNames() {
  static std::vector<std::string> names{""};  // id 0 = none
  return names;
}
std::map<int, Color>& iconTints() {
  static std::map<int, Color> tints;
  return tints;
}
}  // namespace

int iconTextureId(const std::string& name) {
  if (name.empty()) return -1;
  auto& names = iconNames();
  for (size_t i = 1; i < names.size(); ++i)
    if (names[i] == name) return int(i);
  names.push_back(name);
  return int(names.size() - 1);
}

const std::string& iconTextureName(int textureId) {
  auto& names = iconNames();
  static const std::string kEmpty;
  return textureId > 0 && textureId < int(names.size()) ? names[size_t(textureId)] : kEmpty;
}

void noteIconTint(int textureId, Color tint) { iconTints()[textureId] = tint; }
Color iconTint(int textureId) {
  const auto it = iconTints().find(textureId);
  return it != iconTints().end() ? it->second : Color{1, 1, 1, 1};
}

// ---------------------------------------------------------------- layout --
Rect anchoredRect(const Layout& l, const Rect& parent, Vec2 pref) {
  Vec2 size = l.size;
  if (size.x == 0) size.x = pref.x;
  if (size.y == 0) size.y = pref.y;
  size = max2(size, l.minSize);
  float x, y, w, h;
  if (l.anchorMin.x != l.anchorMax.x) {
    x = parent.x + parent.w * l.anchorMin.x + l.insets.left;
    w = parent.w * (l.anchorMax.x - l.anchorMin.x) - l.insets.left - l.insets.right;
  } else {
    w = size.x;
    x = parent.x + parent.w * l.anchorMin.x + l.offset.x - w * l.pivot.x;
  }
  if (l.anchorMin.y != l.anchorMax.y) {
    y = parent.y + parent.h * l.anchorMin.y + l.insets.top;
    h = parent.h * (l.anchorMax.y - l.anchorMin.y) - l.insets.top - l.insets.bottom;
  } else {
    h = size.y;
    y = parent.y + parent.h * l.anchorMin.y + l.offset.y - h * l.pivot.y;
  }
  return {x, y, std::max(0.f, w), std::max(0.f, h)};
}

// --------------------------------------------------------------- cWidget --
cWidget::cWidget(std::string id, std::string styleClass)
    : lithogui::Widget(id), styleClass_(std::move(styleClass)), idCopy_(id) {
  // The facade paints through paintTree(), not lithogui::draw(): suppress
  // the base class's own background fill so nothing draws twice.
  setBackground({0, 0, 0, 0});
  padding = toGui(Edges{});
  setTextPad(0.f);
}

cWidget::~cWidget() {
  if (ui_) ui_->forget(this);
}

void cWidget::attach(cUiRoot* ui) {
  // Structure changed (attach or detach): the paint output can change
  // with it, so the WS4 paint cache key moves. Bump on the OLD owner
  // too (a detach changes what the old root paints).
  if (ui_ && ui_ != ui) { ui_->touchPaint(); ui_->noteStructureBump(); }
  ui_ = ui;
  if (ui_) { ui_->touchPaint(); ui_->noteStructureBump(); }
  for (auto& c : children())
    if (auto* cw = dynamic_cast<cWidget*>(c.get())) cw->attach(ui);
}

cWidget& cWidget::addChild(std::unique_ptr<lithogui::Widget> child) {
  lithogui::Widget* raw = child.get();
  lithogui::Widget::addChild(std::move(child));
  if (auto* cw = dynamic_cast<cWidget*>(raw)) {
    cw->parent_ = this;
    cw->detached_ = false;  // re-homed: the base parent_ is true again
    if (ui_) cw->attach(ui_);
    return *cw;
  }
  return *this;
}

namespace {
// True when `needle` is `subtree` or one of its descendants. Pointer
// comparisons only: neither widget is dereferenced beyond reading the
// (live) subtree's child lists.
bool subtreeContains(const lithogui::Widget* subtree, const lithogui::Widget* needle) {
  if (!subtree || !needle) return false;
  if (subtree == needle) return true;
  for (const auto& c : subtree->children())
    if (subtreeContains(c.get(), needle)) return true;
  return false;
}
}  // namespace

std::unique_ptr<lithogui::Widget> cWidget::removeChild(const lithogui::Widget& child) {
  auto& kids = const_cast<std::vector<std::unique_ptr<lithogui::Widget>>&>(children());
  for (auto it = kids.begin(); it != kids.end(); ++it) {
    if (it->get() == &child) {
      std::unique_ptr<lithogui::Widget> out = std::move(*it);
      kids.erase(it);
      if (auto* cw = dynamic_cast<cWidget*>(out.get())) {
        // Focus/hover repair BEFORE detach (2026-11-08, round 2 of JD's
        // crash): this subtree is leaving the tree. If the root context's
        // focus lives inside it, clear the focus NOW, while this widget
        // still knows its owner... removeChild is about to attach(nullptr)
        // the whole subtree, after which ~cWidget can no longer reach the
        // owner (that is why the destructor-side repair in forget() never
        // fired for the destroyLater path, and the menu's focused Play
        // button dangled into wantsKeyboard()).
        if (ui_) {
          lithogui::UiContext& ctx = ui_->gui().context();
          lithogui::Widget* f = ctx.focused();
          if (f && subtreeContains(out.get(), f)) ctx.setFocus(nullptr);
          // Hover repair, same site, same reason (lifetime audit F-1):
          // UiContext::hover_ is focus_'s twin, but nothing ever
          // cleared it for a plain widget... the destroyLater drain
          // below destroys this subtree, and the engine's next pointer
          // Move would write WS_HOVERED through the freed widget in
          // applyHover. (applyHover now also tombstone-guards its old
          // holder engine-side; this clear keeps the state honest
          // instead of merely safe.)
          lithogui::Widget* hv = ctx.hovered();
          if (hv && subtreeContains(out.get(), hv)) ctx.setHover(nullptr);
          // Facade pointer capture, same shape again (audit F-7):
          // capture_ is set from a hit-test in handleEvent and its
          // destructor-side repair in forget() never fires on this
          // path (attach(nullptr) below severs the route), so clear
          // it here while the subtree is still intact.
          if (ui_->capture_ && subtreeContains(out.get(), ui_->capture_)) ui_->capture_ = nullptr;
        }
        cw->parent_ = nullptr;
        // FLAG (bug hunt #9): lithogui::Widget::parent_ still points at
        // this parent... the engine Widget has no detach/removeChild verb
        // and its parent_ is private, so the facade cannot clear it here.
        // A removed widget kept alive via destroyLater therefore still
        // shows the stale parent from lithogui's side (treeRect() /
        // snapshotChain() walk it). detached_ is the facade-side record:
        // ancestry walkers must stop at a detached widget instead of
        // trusting the base chain. Engine-side fix (out of scope): a
        // Widget::detach() helper clearing parent_, called from here.
        cw->detached_ = true;
        cw->attach(nullptr);
      }
      return out;
    }
  }
  return nullptr;
}

void cWidget::clearChildren() {
  while (!children().empty()) removeChild(*children().back());
}

cWidget* cWidget::find(std::string_view id) {
  if (facadeId() == id) return this;
  for (auto& c : children())
    if (auto* cw = dynamic_cast<cWidget*>(c.get()))
      if (cWidget* hit = cw->find(id)) return hit;
  return nullptr;
}

bool cWidget::interactive() const {
  if (!visible() || !enabled()) return false;
  // Stop at a detached widget (bug hunt #9): above it the parent chain
  // may be the stale lithogui one, through a since-destroyed parent.
  for (const cWidget* p = parent_; p && !p->detached_; p = p->parent_)
    if (!p->visible() || !p->enabled()) return false;
  return true;
}

cWidget& cWidget::setStyleClass(std::string c) {
  styleClass_ = std::move(c);
  return *this;
}

cWidget& cWidget::setLayout(const Layout& l) {
  layout_ = l;
  if (l.flex > 0) flex = l.flex;
  return *this;
}

const Style& cWidget::style() const { return theme().style(styleClass_); }
const Theme& cWidget::theme() const { return ui_ ? ui_->theme() : builtinTheme(); }

bool cWidget::hovered() const { return ui_ && ui_->hover() == this; }
bool cWidget::focused() const { return ui_ && ui_->focus() == this; }

void cWidget::focus() {
  if (ui_) ui_->setFocus(this);
}

cWidget& cWidget::on(std::string event, Handler h) {
  handlers_.emplace_back(std::move(event), std::move(h));
  return *this;
}

cWidget& cWidget::onScript(std::string event, std::string functionDecl) {
  scriptHandlers_.emplace_back(std::move(event), std::move(functionDecl));
  return *this;
}

bool cWidget::hasHandler(std::string_view event) const {
  for (const auto& [name, h] : handlers_)
    if (name == event) return true;
  for (const auto& [name, f] : scriptHandlers_)
    if (name == event) return true;
  return false;
}

void cWidget::emit(std::string_view event) {
  // Snapshot: a handler may rebuild parts of the tree; destroyLater keeps
  // removed widgets alive until the root's next safe point.
  const auto handlers = handlers_;
  for (const auto& [name, h] : handlers)
    if (name == event && h) h(*this);
  const auto scripts = scriptHandlers_;
  if (ui_ && ui_->scriptHost())
    for (const auto& [name, fn] : scripts)
      if (name == event) ui_->scriptHost()->callHandler(fn, facadeId());
}

Vec2 cWidget::measure(const Theme& theme) const {
  Vec2 extent{0, 0};
  for (auto& c : children()) {
    const auto* cw = dynamic_cast<const cWidget*>(c.get());
    if (!cw || !cw->visible()) continue;
    const Vec2 cs = childSize(*cw, theme);
    const Layout& l = cw->layout_;
    if (l.anchorMin == l.anchorMax) {
      extent.x = std::max(extent.x, l.offset.x + cs.x);
      extent.y = std::max(extent.y, l.offset.y + cs.y);
    } else {
      // Layout diagnosis 2026-11-08 (items 1+2, the root cause): a fill
      // or stretched child contributes its content size too. Before,
      // such children counted for NOTHING here, so a plain panel whose
      // only child is a bare fill label measured 0x0, and every
      // right/bottom anchor placed it as a zero-size box: the tick
      // panel sat at the screen's right edge with its text running off
      // (only "TI" visible), and the controls panel's bottom anchor
      // lifted by a measured height of 0 so its text ran off the
      // bottom of the frame.
      extent.x = std::max(extent.x, cs.x);
      extent.y = std::max(extent.y, cs.y);
    }
  }
  // arrange() insets the content by the style padding, so the measured
  // size includes it (cStackPanel::measure already did this; the plain
  // container now agrees).
  const Edges pad = style().padding;
  return {extent.x + pad.horizontal(), extent.y + pad.vertical()};
}

Vec2 cWidget::childSize(const cWidget& child, const Theme& theme) {
  Vec2 s = child.layout_.size;
  const Vec2 pref = child.measure(theme);
  if (s.x == 0) s.x = pref.x;
  if (s.y == 0) s.y = pref.y;
  return max2(s, child.layout_.minSize);
}

void cWidget::syncNative() {
  const Style& st = style();
  textStyle = lithogui::TextStyle{};
  textStyle->size = theme().glyph().y * st.textScale;
  textStyle->lineHeightMul = 1.0f;
  setBackground({0, 0, 0, 0});
}

void cWidget::place(const Rect& r, const Theme& theme) {
  setRect(toGui(r));
  arrange(theme);
  syncNative();
}

void cWidget::layout(lithogui::Rect r, const lithogui::Style& style) {
  noteStyle(style);
  place(fromGui(r), theme());
}

void cWidget::arrange(const Theme& theme) {
  const Rect content = fromGui(rect()).inset(style().padding);
  for (auto& c : children()) {
    auto* cw = dynamic_cast<cWidget*>(c.get());
    if (!cw) {
      // Foreign (native lithogui) child -- the debug console's panel is
      // added straight onto the root by cDebugConsole::attach. Place it in
      // the content box, honoring its fixed sizes and margins, and run its
      // own layout (the facade passes never touch it otherwise).
      if (!c->visible()) continue;
      const lithogui::Edges m = c->margin;
      const float w = c->fixedW >= 0 ? c->fixedW : std::max(0.f, content.w - m.l - m.r);
      const float h = c->fixedH >= 0 ? c->fixedH : std::max(0.f, content.h - m.t - m.b);
      c->layout({content.x + m.l, content.y + m.t, w, h}, lastStyle());
      continue;
    }
    if (!cw->visible()) {
      cw->place({content.x, content.y, 0, 0}, theme);
      continue;
    }
    cw->place(anchoredRect(cw->layout_, content, cw->measure(theme)), theme);
  }
}

void cWidget::relayout() { place(fromGui(rect()), theme()); }

Color cWidget::stateBackground() const {
  const Style& st = style();
  if (!enabled()) return st.disabled;
  if (pressed()) return st.pressed;
  if (hovered()) return st.hover;
  return st.background;
}

void cWidget::paint(Painter& p) const {
  const Style& st = style();
  if (st.background.a > 0) p.fill(fromGui(rect()), st.background);
  if (st.borderWidth > 0 && st.border.a > 0) p.frame(fromGui(rect()), st.border, st.borderWidth);
}

void cWidget::paintChildren(Painter& p) const { paintChildrenLayer(p, false, false); }

void cWidget::paintChildrenLayer(Painter& p, bool dynamicPass, bool inDynamic) const {
  for (auto& c : children())
    if (auto* cw = dynamic_cast<cWidget*>(c.get()))
      if (cw->visible()) cw->paintTreeLayer(p, dynamicPass, inDynamic);
}

void cWidget::paintTree(Painter& p) const { paintTreeLayer(p, false, false); }

void cWidget::paintTreeLayer(Painter& p, bool dynamicPass, bool inDynamic) const {
  if (!visible()) return;
  // Two-layer split (JD paint ruling, 2026-11-09): a dynamic-surface
  // subtree belongs wholly to the dynamic layer; everything else to
  // the main layer. The passes are complementary, so every widget
  // paints in exactly one pass per frame.
  if (dynamic_ && !dynamicPass) return;  // dynamic root on the main pass: the dynamic pass owns this subtree
  // Two-layer child routing (JD paint ruling, 2026-11-09, corrected):
  // on the DYNAMIC pass the pass flag must SURVIVE into dynamic
  // subtrees... a dynamic root's own children are unflagged, so a plain
  // `dynamicPass && !dynamic_` test at every node would mistake them
  // for ancestors and never paint them. The context therefore rides
  // down from the pass entry: once a dynamic root is entered, the rest
  // of that subtree paints on this pass. Ancestors above the root
  // still paint nothing of their own.
  const bool dynHere = dynamicPass && (dynamic_ || inDynamic);
  if (dynamicPass && !dynamic_ && !inDynamic) {
    paintChildrenLayer(p, true, false);
    return;
  }
  // WS4 off-viewport culling (perf diagnosis item (c)): the paint walk
  // used to emit every widget unconditionally; only the GPU scissor
  // rejected off-frame work, after the CPU paid the full emit. A widget
  // whose rect lies wholly outside the root viewport AND which holds
  // no facade children emits nothing. Not a subtree prune: children can
  // paint outside their parent's rect (a label's text overflows its
  // box), so any widget WITH children is still descended into, and each
  // descendant is culled on its own rect. (Painter here is the facade's
  // own background/border pass; text and images ride the same paint()
  // overrides, so the cull covers them for leaf widgets.)
  bool leaf = true;
  for (auto& c : children())
    if (dynamic_cast<const cWidget*>(c.get())) {
      leaf = false;
      break;
    }
  bool offViewport = false;
  if (leaf && ui_) {
    const lithogui::Rect vp = ui_->root().rect();
    const lithogui::Rect r = rect();
    offViewport = vp.w > 0 && vp.h > 0 &&
                  (r.x + r.w <= vp.x || r.y + r.h <= vp.y || r.x >= vp.x + vp.w || r.y >= vp.y + vp.h);
  }
  if (!offViewport) paint(p);
  // Children ride the same layer context; the main pass goes through
  // the VIRTUAL paintChildren (cScrollView clips its content there...
  // its override re-enters via paintTree, main layer by contract).
  if (dynamicPass) paintChildrenLayer(p, true, dynHere);
  else paintChildren(p);
}

// --------------------------------------------------------------- cUiRoot --
cUiRoot::cUiRoot(Theme theme)
    : theme_(std::move(theme)), guiOwner_(std::make_unique<lithogui::cLithoGui>()), gui_(*guiOwner_) {
  root_ = std::make_unique<cWidget>("", "root");
  root_->attach(this);
  gui_.setRoot(root_.get());
  gui_.setStyle(projectTheme(theme_, gui_.style()));
}

cUiRoot::~cUiRoot() {
  gui_.setRoot(nullptr);  // drains popups while their owners below still live
  popupOwners_.clear();
  root_.reset();
}

void cUiRoot::setTheme(Theme t) {
  theme_ = std::move(t);
  gui_.setStyle(projectTheme(theme_, gui_.style()));
  touchPaint();  // colours/metrics changed: the paint cache key moves
  noteThemeBump();
}

void cUiRoot::forget(const cWidget* w) {
  std::erase(modals_, const_cast<cWidget*>(w));
  // Capture repair coverage note (audit F-7): this line only ever
  // fires for a widget destroyed while still attached (ui_ intact).
  // The destroyLater path removes the subtree through removeChild,
  // which now clears capture_ itself before attach(nullptr); this
  // stays as the direct-destruction half of the same repair.
  if (capture_ == w) capture_ = nullptr;
  // Destructor repair (2026-11-08, JD's crash): a plain widget that dies
  // while holding the context's keyboard focus leaves UiContext::focus_
  // dangling... unlike ~Popup, ~Widget never repairs it (it only writes
  // its tombstone), so the next wantsKeyboard() / focus() query calls
  // through freed memory (the menu Play button held focus; closeMenu
  // destroyLater + update() destroyed it mid-transition). Clear the
  // context focus here, by pointer comparison only, while the dying
  // widget is still a valid object (~cWidget runs before ~Widget).
  // Mirrors the repair ~Popup performs for its own family.
  if (gui_.context().focused() == static_cast<const lithogui::Widget*>(w))
    gui_.context().setFocus(nullptr);
}

namespace {
/// True when any widget in the subtree is a visible dynamic-surface
/// root (the readout panels). Cheap walk: the flagged set is tiny and
/// sits near the root of the HUD subtree.
bool anyDynamicVisible(const lithogui::Widget& w) {
  const auto* cw = dynamic_cast<const cWidget*>(&w);
  if (cw && !cw->visible()) return false;
  if (cw && cw->dynamicSurface()) return true;
  for (const auto& c : w.children())
    if (anyDynamicVisible(*c)) return true;
  return false;
}

/// Pacing instrumentation (2026-11-11): exact content equality of two
/// expanded HUD layers... every rect, text (string included) and draw
/// (clip and icon UVs included), plus the atlas stamp. Used only to
/// count no-op rebuilds; cost is one walk of the smaller layer and it
/// runs only on frames that already paid for a rebuild.
bool hudLayersEqual(const litholattice::HudLayer& a, const litholattice::HudLayer& b) {
  if (a.scale != b.scale || a.iconAtlasVersion != b.iconAtlasVersion) return false;
  if (a.rects.size() != b.rects.size() || a.texts.size() != b.texts.size() || a.draws.size() != b.draws.size()) return false;
  for (size_t i = 0; i < a.rects.size(); ++i) {
    const auto& x = a.rects[i];
    const auto& y = b.rects[i];
    if (x.x != y.x || x.y != y.y || x.w != y.w || x.h != y.h || x.r != y.r || x.g != y.g || x.b != y.b || x.a != y.a) return false;
  }
  for (size_t i = 0; i < a.texts.size(); ++i) {
    const auto& x = a.texts[i];
    const auto& y = b.texts[i];
    if (x.x != y.x || x.y != y.y || x.text != y.text || x.r != y.r || x.g != y.g || x.b != y.b || x.a != y.a) return false;
  }
  for (size_t i = 0; i < a.draws.size(); ++i) {
    const auto& x = a.draws[i];
    const auto& y = b.draws[i];
    if (x.x != y.x || x.y != y.y || x.w != y.w || x.h != y.h || x.r != y.r || x.g != y.g || x.b != y.b || x.a != y.a ||
        x.scale != y.scale || x.text != y.text || x.iu0 != y.iu0 || x.iv0 != y.iv0 || x.iu1 != y.iu1 || x.iv1 != y.iv1 ||
        x.icon != y.icon || x.clip.x0 != y.clip.x0 || x.clip.y0 != y.clip.y0 || x.clip.x1 != y.clip.x1 || x.clip.y1 != y.clip.y1)
      return false;
  }
  return true;
}
}  // namespace

void cUiRoot::update(float dt) {
  time_ += dt;
  // JD paint ruling (2026-11-09): the dynamic readout layer is dirty
  // by the frame while its surfaces are on screen... its generation
  // advances here, once per update, so its paint coalesces to at most
  // one rebuild per frame and replays (empty) the rest of the time.
  // The main generation is untouched by this: readout changes mark
  // ONLY this dynamic generation (their setText bumps are routed by
  // touchPaint below), never the main layer's key.
  dynVisible_ = root_ && anyDynamicVisible(*root_);
  if (dynVisible_) ++dynGen_;
  prunePopupOwner(nullptr);  // flush owners closed since the last frame
  closing_.clear();  // deferred destruction lands at the frame boundary
  updateTooltip(dt);
}

// ------------------------------------------------------------- tooltips --
namespace {
/// Hover dwell before a tooltip opens (seconds of UI time).
constexpr float kTooltipDelay = 0.45f;
}  // namespace

void cUiRoot::hideTooltip() {
  if (tooltip_ && tooltip_->isOpen()) {
    tooltip_->hide();
    touchPaint();
    noteTooltipBump();
  }
  tipTarget_ = nullptr;
  tipText_.clear();
  tipTimer_ = 0;
}

void cUiRoot::updateTooltip(float dt) {
  // The widget carrying the tooltip text: the context's (checked)
  // hovered widget, else its nearest facade ancestor with text. Slots
  // carry their own text; children of a labelled widget inherit it.
  const cWidget* target = nullptr;
  std::string text;
  if (const lithogui::Widget* hov = gui_.context().hovered()) {
    for (const lithogui::Widget* w = hov; w && !target; w = w->parent()) {
      const auto* cw = dynamic_cast<const cWidget*>(w);
      if (cw && !cw->tooltip().empty()) {
        target = cw;
        text = cw->tooltip();
      }
    }
  }
  if (!target) {
    if (tooltip_ && tooltip_->isOpen()) hideTooltip();
    tipTarget_ = nullptr;
    tipText_.clear();
    tipTimer_ = 0;
    return;
  }
  if (target != tipTarget_ || text != tipText_) {
    if (tooltip_ && tooltip_->isOpen()) {
      tooltip_->hide();
      touchPaint();
      noteTooltipBump();
    }
    tipTarget_ = target;
    tipText_ = std::move(text);
    tipTimer_ = 0;
    return;
  }
  // Same target, same text: dwell. The target pointer is only ever
  // compared, never dereferenced, in this branch... except for the
  // anchor rect at show time, and there the context's hover check a
  // moment ago vouched for it (single thread, same call stack).
  tipTimer_ += dt;
  if (tipTimer_ >= kTooltipDelay && !(tooltip_ && tooltip_->isOpen())) {
    if (!tooltip_) tooltip_ = std::make_unique<lithogui::Tooltip>("facade-tooltip", tipText_);
    tooltip_->setText(tipText_);
    // 2026-11-09 (JD note, next-build-2): render at the POLLED position.
    // The anchor is the target the context's hover POLL resolved for the
    // pointer event as dispatched (hover_ is set from the dispatch's own
    // hit test of the event coordinates, which also lands in lastPointer_),
    // and showAt receives that same target's tree-space rect... the one
    // position object the poll itself produced. The FRM-2 popup contract
    // requires treeRect() here, not rect() (design coordinates inside a
    // Frame; the facade tree is unframed today, where the two agree, but
    // the contract is the poll's space either way).
    tooltip_->showAt(gui_.context(), target->treeRect());
    touchPaint();
    noteTooltipBump();
  }
}

void cUiRoot::layout(Vec2 viewport) {
  // WS4 layout gate: layout is a pure function of (generation,
  // viewport, popup count). When none of those moved since the last
  // layout, the rects it produced are still the rects it would produce;
  // skip the full tree walk (the perf diagnosis measured layout plus
  // recursive measure running twice per frame, unconditionally).
  const size_t popupsNow = gui_.context().popupCount();
  if (layoutGenBuilt_ == paintGen_ && layoutPopupsBuilt_ == popupsNow &&
      layoutViewportBuilt_.x == viewport.x && layoutViewportBuilt_.y == viewport.y)
    return;
  layoutGenBuilt_ = paintGen_;
  layoutPopupsBuilt_ = popupsNow;
  layoutViewportBuilt_ = viewport;
  viewport_ = viewport;
  root_->place({0, 0, viewport.x, viewport.y}, theme_);
  gui_.layout({0, 0, viewport.x, viewport.y});  // lays out open popups too
  // Facade dialogs are laid out AGAIN by the facade pass below: the native
  // pass cannot place them (their host popup is not part of the root
  // layout tree, and the dialog's measure lives on the facade side), so
  // the DialogHost centres its dialog itself (2026-11-07, JD's world
  // picker bug: the dialog kept the native pass's 8x36 anchor-collapse).
  const lithogui::Rect rootRect = root_->rect();
  // The native context never re-lays popups and Popup::layout honours a
  // set frame over any handed rect, so a dialog kept the centering from
  // its OPEN moment, when its body was still empty (measure = padding
  // only): the world picker rendered as an 8x36 sliver at the screen
  // centre with its content spilling elsewhere (JD, 2026-11-07). The
  // remedy: re-run the DialogHost's centering layout, then re-open the
  // popup through the context (openPopup on an already-open popup is a
  // re-layout in place) so the context's own bookkeeping... layout,
  // focus, capture... re-runs against the corrected geometry.
  // Owners stay in popupOwners_ only while their popup is live: a close
  // prunes the owner (bug hunt #34), so this loop never resurrects a
  // closed dialog; hosts still open (or pending their first layout after
  // being shown) are re-centered and re-opened as before.
  inLayout_ = true;
  for (auto& owner : popupOwners_)
    if (auto* host = dynamic_cast<DialogHost*>(owner.get())) {
      host->layout(rootRect, gui_.style());
      gui_.context().openPopup(host);
    }
  inLayout_ = false;
  prunePopupOwner(nullptr);  // owners closed during the loop itself
  deferred_.clear();
}

void cUiRoot::prunePopupOwner(lithogui::Widget* owner) {
  if (owner) {
    // A close can fire from inside layout()'s own re-open pass (a host
    // hook may close another popup) or from deep inside event routing:
    // never erase mid-iteration... queue it and let a safe point flush.
    if (std::find(ownersToPrune_.begin(), ownersToPrune_.end(), owner) == ownersToPrune_.end())
      ownersToPrune_.push_back(owner);
    if (inLayout_) return;
  }
  if (ownersToPrune_.empty()) return;
  // Move the doomed owners out first: destroying a host destroys its
  // dialog, whose callbacks reach back into this root, so keep them alive
  // until the erase completes, then let them die at scope exit.
  std::vector<std::unique_ptr<lithogui::Widget>> dead;
  for (lithogui::Widget* raw : ownersToPrune_) {
    for (auto it = popupOwners_.begin(); it != popupOwners_.end(); ++it)
      if (it->get() == raw) {
        dead.push_back(std::move(*it));
        popupOwners_.erase(it);
        break;
      }
  }
  ownersToPrune_.clear();
}

void cUiRoot::paint(litholattice::HudLayer& hud,
                    std::function<std::optional<Color>(int)> images,
                    std::function<const litholattice::Image*(int)> icons) const {
  // JD paint ruling (2026-11-09, supersedes the WS4 heartbeat): the
  // GUI paints ONLY when marked dirty, at most once per frame, and
  // dirty is marked by interaction and state change. The main layer
  // below is dirty-driven pure: it rebuilds when the paint generation
  // (structure, visibility, text, icons, theme, events, tooltip), the
  // popup count, or the root rect moved since its last build, and
  // replays verbatim otherwise... no time-based forced rebuild
  // remains. The per-frame readouts (tick/TPS line, F3 debug text)
  // live on the DYNAMIC layer: their mutations mark only dynGen_
  // (cWidget::touchPaint routes bumps inside a dynamic surface), so
  // they repaint a tiny layer of their own and can never dirty this
  // one. The two layers are composed into `hud` in paint order (main
  // first, dynamic readouts on top, matching their old tree order:
  // the readout panels sit at the screen corners over the HUD).
  const size_t popups = gui_.context().popupCount();
  const lithogui::Rect rr = root_->rect();
  const bool mainDirty = !paintCache_ || paintGenBuilt_ != paintGen_ || paintPopupsBuilt_ != popups ||
                         paintViewportBuilt_.x != rr.w || paintViewportBuilt_.y != rr.h;
  if (!mainDirty) {
    paintRebuilt_ = false;
  } else {
    // Pacing instrumentation (2026-11-11): snapshot the cache this
    // rebuild is about to replace; after the render, a byte-identical
    // result counts as a no-op rebuild (a dirty bump that changed no
    // pixel). A shallow copy is enough: the layer's vectors are the
    // content, and the atlas is compared by stamp below.
    const bool hadMainCache = paintCache_ != nullptr;
    litholattice::HudLayer prevMain;
    if (hadMainCache) prevMain = *paintCache_;
    ++mainPaintCalls_;
    if (!paintCache_) paintCache_ = std::make_unique<litholattice::HudLayer>();
    // Repaint into a clean layer: render() appends (the dynamic branch
    // below has always cleared first; the main cache did not, so every
    // rebuild stacked a full copy of the layer onto the last... menu
    // 24 -> 48 draws, HUD 4,892 -> 9,783, proven on the bench
    // 2026-11-09, Phase B step 1). The atlas stamp resets with it: a
    // fresh render re-stamps from the atlas it actually used.
    else {
      paintCache_->rects.clear();
      paintCache_->texts.clear();
      paintCache_->draws.clear();
      paintCache_->iconAtlas.reset();
      paintCache_->iconAtlasVersion = 0;
    }
    lithogui::DrawList dl;
    PaintSink& sink = paintSink();
    sink.dl = &dl;
    sink.theme = &theme_;
    SinkRenderer sr;
    Painter painter(sr, theme_);
    root_->paintTreeLayer(painter, false);
    // Append the context's own draw (open popups paint after the tree).
    // Popup hosts that paint facade subtrees read the sink, so point it at
    // the context list for the emitFrame pass.
    lithogui::DrawList ctxList;
    sink.dl = &ctxList;
    gui_.emitFrame(ctxList, time_);
    sink.dl = nullptr;
    sink.theme = nullptr;
    dl.absorb(ctxList);
    litholattice::cHudUiRenderer renderer(
        *paintCache_,
        [images](int id) -> std::optional<lithogui::Color> {
          if (!images) return std::nullopt;
          if (auto c = images(id)) return toGui(*c);
          return std::nullopt;
        },
        [&icons](int id) -> const litholattice::Image* { return icons ? icons(id) : nullptr; });
    renderer.render(dl);
    paintGenBuilt_ = paintGen_;
    paintPopupsBuilt_ = popups;
    paintViewportBuilt_ = {rr.w, rr.h};
    paintRebuilt_ = true;
    if (hadMainCache && hudLayersEqual(prevMain, *paintCache_)) ++mainPaintNoop_;
  }
  // ---- dynamic readout layer: its own generation, its own cache.
  // Rebuilt at most once per frame (update() advances dynGen_ at most
  // once per frame, and only while a readout surface is visible), and
  // composed over the main layer. When no readout is visible the layer
  // is empty and this costs one visibility check per paint.
  const bool dynDirty = dynVisible_ && (!dynCache_ || dynGenBuilt_ != dynGen_ || !dynVisibleBuilt_);
  if (dynDirty) {
    // Pacing instrumentation (2026-11-11): same no-op accounting as
    // the main layer. The readout generation advances once per frame
    // by design, so dynPaintCalls_ tracks frames; dynPaintNoop_
    // tracks how many of those repaints changed nothing at all.
    const bool hadDynCache = dynCache_ != nullptr;
    litholattice::HudLayer prevDyn;
    if (hadDynCache) prevDyn = *dynCache_;
    ++dynPaintCalls_;
    if (!dynCache_) dynCache_ = std::make_unique<litholattice::HudLayer>();
    else { dynCache_->rects.clear(); dynCache_->texts.clear(); dynCache_->draws.clear(); dynCache_->iconAtlas.reset(); dynCache_->iconAtlasVersion = 0; }  // repaint into a clean layer: render() appends
    lithogui::DrawList dl;
    PaintSink& sink = paintSink();
    sink.dl = &dl;
    sink.theme = &theme_;
    SinkRenderer sr;
    Painter painter(sr, theme_);
    root_->paintTreeLayer(painter, true);
    sink.dl = nullptr;
    sink.theme = nullptr;
    litholattice::cHudUiRenderer renderer(
        *dynCache_,
        [images](int id) -> std::optional<lithogui::Color> {
          if (!images) return std::nullopt;
          if (auto c = images(id)) return toGui(*c);
          return std::nullopt;
        },
        [&icons](int id) -> const litholattice::Image* { return icons ? icons(id) : nullptr; });
    renderer.render(dl);
    dynGenBuilt_ = dynGen_;
    dynVisibleBuilt_ = true;
    dynamicRebuilt_ = true;
    if (hadDynCache && hudLayersEqual(prevDyn, *dynCache_)) ++dynPaintNoop_;
  } else {
    if (!dynVisible_ && dynVisibleBuilt_) dynVisibleBuilt_ = false;  // readouts hidden: layer drops to empty
    dynamicRebuilt_ = false;
  }
  dynamicDrawCount_ = dynVisible_ && dynCache_ ? dynCache_->draws.size() + dynCache_->rects.size() + dynCache_->texts.size() : 0;
  // ---- compose: the main layer verbatim, dynamic readouts appended.
  // The target is ASSIGNED, never appended to: callers may hand paint()
  // a layer that already holds a previous frame (the selftest does),
  // and the old append-onto-nonempty behaviour doubled the frame.
  hud = *paintCache_;
  if (dynVisible_ && dynCache_) {
    hud.rects.insert(hud.rects.end(), dynCache_->rects.begin(), dynCache_->rects.end());
    hud.texts.insert(hud.texts.end(), dynCache_->texts.begin(), dynCache_->texts.end());
    hud.draws.insert(hud.draws.end(), dynCache_->draws.begin(), dynCache_->draws.end());
  }
  // Textured icons (Phase B): the composed layer carries the newest
  // atlas snapshot any sublayer stamped. The dynamic layer emits no
  // icons today, so this is belt-and-braces... snapshots are
  // append-only supersets at identical tile positions, so a newer
  // snapshot is always a safe upgrade for older draws.
  if (dynCache_ && dynCache_->iconAtlasVersion > hud.iconAtlasVersion) {
    hud.iconAtlas = dynCache_->iconAtlas;
    hud.iconAtlasVersion = dynCache_->iconAtlasVersion;
  }
  paintDrawCount_ = hud.draws.size() + hud.rects.size() + hud.texts.size();
  paintRectCount_ = hud.rects.size();
  paintTextCount_ = hud.texts.size();
  paintDrawOnlyCount_ = hud.draws.size();
  paintIconCount_ = 0;  // textured icons (Phase B step 1): one draw per icon slot
  for (const auto& d : hud.draws)
    if (d.icon) ++paintIconCount_;
}

void cUiRoot::paintSplit(litholattice::HudLayer& hud, litholattice::HudLayerSplit& split,
                         std::function<std::optional<Color>(int textureId)> images,
                         std::function<const litholattice::Image*(int textureId)> icons) const {
  paint(hud, std::move(images), std::move(icons));
  split = {};
  // The static stream is the main paint cache under its build key:
  // paintGenBuilt_ advances exactly when that cache was rebuilt, and
  // the viewport is part of the rebuild condition, so the triple
  // (generation, viewport w, viewport h) is the cache's identity.
  if (paintCache_) {
    split.staticLayer = paintCache_.get();
    split.staticGeneration = paintGenBuilt_ * 4294967296ull +
                             uint64_t(uint32_t(paintViewportBuilt_.x)) * 65536ull +
                             uint64_t(uint32_t(paintViewportBuilt_.y));
    if (split.staticGeneration == 0) split.staticGeneration = 1;
  }
  // The dynamic stream rides only while a readout surface is visible
  // (dynCache_ then holds the readout layer; dynGenBuilt_ is its key).
  // When hidden the stream is absent and backends draw static only...
  // matching the composed layer, which appends nothing then either.
  if (dynVisible_ && dynCache_) {
    split.dynamicLayer = dynCache_.get();
    split.dynamicGeneration = dynGenBuilt_ ? dynGenBuilt_ : 1;
  }
}

bool cUiRoot::handleEvent(const UiEvent& e) {
  mods_ = e.mods;
  const size_t popupsBefore = gui_.context().popupCount();
  // 2026-11-09 next-build-2 (GUI diagnosis, cheap cut c-i + attribution):
  // snapshot the paint-relevant interaction state BEFORE dispatch. A
  // pointer move that changes no hover target and no press state cannot
  // alter a pixel, so its blanket generation bump below is skipped...
  // the gate then survives mouse wiggle over menus and the inventory.
  // Anything else (hover moved, press state moved, popup count moved,
  // tooltip visible, any non-move event) keeps the bump: correctness of
  // the gate beats the saving wherever the two could disagree.
  const lithogui::Widget* hoverBefore = gui_.context().hovered();
  const bool tooltipWasOpen = tooltip_ && tooltip_->isOpen();
  bool consumed = false;
  switch (e.type) {
    case UiEvent::Type::PointerMove:
    case UiEvent::Type::PointerDown:
    case UiEvent::Type::PointerUp:
    case UiEvent::Type::Wheel: {
      lastPointer_ = e.pos;
      pointerSeen_ = true;
      lithogui::PointerEvent pe;
      pe.type = e.type == UiEvent::Type::PointerMove   ? lithogui::PointerEvent::Type::Move
                : e.type == UiEvent::Type::PointerDown ? lithogui::PointerEvent::Type::Down
                : e.type == UiEvent::Type::PointerUp   ? lithogui::PointerEvent::Type::Up
                                                       : lithogui::PointerEvent::Type::Wheel;
      pe.x = e.pos.x;
      pe.y = e.pos.y;
      pe.button = e.button == PointerButton::Left ? 0 : e.button == PointerButton::Right ? 1 : 2;
      pe.wheelDelta = e.wheel;
      pe.clickCount = e.clickCount;
      pe.shift = e.shift();
      pe.ctrl = e.ctrl();
      pe.alt = e.alt();
      if (e.type == UiEvent::Type::PointerDown && pe.button == 0) {
        if (auto* hit = dynamic_cast<cWidget*>(gui_.context().hitTest(pe.x, pe.y))) capture_ = hit;
      }
      consumed = gui_.dispatchPointer(pe);
      if (e.type == UiEvent::Type::PointerUp) capture_ = nullptr;
      break;
    }
    case UiEvent::Type::KeyDown:
    case UiEvent::Type::KeyUp:
    case UiEvent::Type::Text: {
      // Close-guard veto (old dialog behavior): the context closes the
      // top popup on Escape unconditionally, so a guarded top modal gets
      // first refusal here.
      if (e.type == UiEvent::Type::KeyDown && e.key == Key::Escape && !modals_.empty() &&
          modals_.back()->vetoPopupClose()) {
        consumed = true;
        break;
      }
      const lithogui::KeyEvent ke = toGuiKeyEvent(e);
      // Escape closes dialogs through the context itself; the facade hears
      // about it via the dialog's onClose hook in the popup-count check.
      consumed = gui_.dispatchKey(ke);
      break;
    }
  }
  if (gui_.context().popupCount() != popupsBefore) syncModalsFromPopups();
  // Any event can move hover/press/focus state, which is paint output:
  // the WS4 paint cache key moves with every event... EXCEPT the one
  // provably pixel-neutral case (next-build-2, cheap cut c-i): a pointer
  // MOVE whose dispatch left the hover target, the popup count and the
  // tooltip state exactly as they were. Hover is the only paint-visible
  // state a move can change (press/focus change on Down/Up/Key, which
  // always bump). The 2x/second age bound still caps any missed path.
  const bool moveNoop = e.type == UiEvent::Type::PointerMove &&
                        gui_.context().hovered() == hoverBefore &&
                        gui_.context().popupCount() == popupsBefore &&
                        ((tooltip_ && tooltip_->isOpen()) == tooltipWasOpen);
  if (!moveNoop) {
    touchPaint();
    noteEventBump();  // attribution: this dispatch cost the gate a rebuild
  }
  // A press dismisses a showing tooltip (it re-arms after the dwell).
  if (e.type == UiEvent::Type::PointerDown) hideTooltip();
  return consumed;
}

void cUiRoot::syncModalsFromPopups() {
  // Dialogs removed by the context (Escape / outside rules) have already
  // fired their onClose through ~Popup bookkeeping; prune facade pointers
  // whose popups are gone. The test is on the dialog's HOST, not the
  // dialog: popupOwners_ holds hosts (DialogHost / the pushModal popup),
  // while modals_ holds the dialogs hosted inside them, so comparing
  // owner.get() to the dialog could never match (bug hunt #35) and every
  // dialog was dropped on the first popup-count change (a dropdown
  // opening inside Settings silently cleared the modal stack, losing the
  // Escape veto). A dialog stays while its host popup isOpen(); plain
  // pushModal popups are their own host.
  std::vector<cWidget*> alive;
  for (cWidget* m : modals_) {
    lithogui::Popup* host = m->asPopup();
    if (!host) host = dynamic_cast<lithogui::Popup*>(static_cast<lithogui::Widget*>(m));
    if (host && host->isOpen()) alive.push_back(m);
  }
  modals_ = std::move(alive);
}

bool cUiRoot::focusOwnerAlive() const {
  // The one checked validity query over the raw focused() value: the
  // tombstone registry answers by pointer value alone, so this is safe
  // even when focus_ dangles (round 2 of JD's crash: focused() returns
  // the stale pointer check-free, and wantsKeyboard() was calling
  // focusable() through it).
  lithogui::Widget* f = gui_.focused();
  return f && !lithogui::widgetTombstoned(f);
}

cWidget* cUiRoot::focus() const {
  return focusOwnerAlive() ? dynamic_cast<cWidget*>(gui_.focused()) : nullptr;
}

void cUiRoot::setFocus(cWidget* w) { gui_.context().setFocus(w); }

cWidget* cUiRoot::hover() const {
  if (!pointerSeen_) return nullptr;
  return dynamic_cast<cWidget*>(gui_.context().hitTest(lastPointer_.x, lastPointer_.y));
}

void cUiRoot::destroyLater(std::unique_ptr<lithogui::Widget> w) { closing_.push_back(std::move(w)); }

bool cUiRoot::wantsKeyboard() const {
  if (modalOpen() || popupOpen()) return true;
  if (!focusOwnerAlive()) return false;
  const lithogui::Widget* f = gui_.focused();
  return f && f->focusable();
}

bool cUiRoot::wantsPointer(Vec2 p) const { return gui_.context().hitTest(p.x, p.y) != nullptr; }

cWidget& cUiRoot::pushModal(std::unique_ptr<lithogui::Widget> modal) {
  lithogui::Widget* raw = modal.get();
  if (auto* popup = dynamic_cast<lithogui::Popup*>(raw)) {
    popupOwners_.push_back(std::move(modal));
    // Prune the owner when its popup closes (bug hunt #34): compose with
    // any onClose the caller already set (cMenuBar's menu reset, #46).
    const auto prevClose = popup->onClose;
    popup->onClose = [this, raw, prevClose](lithogui::Popup& p) {
      if (prevClose) prevClose(p);
      prunePopupOwner(raw);
    };
    if (auto* cw = dynamic_cast<cWidget*>(popup)) cw->attach(this);
    gui_.context().openPopup(popup);
    if (auto* cw = dynamic_cast<cWidget*>(popup)) {
      if (std::find(modals_.begin(), modals_.end(), cw) == modals_.end()) modals_.push_back(cw);
      return *cw;
    }
    return root();
  }
  cWidget* cw = dynamic_cast<cWidget*>(raw);
  root_->addChild(std::move(modal));
  if (cw) modals_.push_back(cw);
  return cw ? *cw : root();
}

cWidget& cUiRoot::pushDialog(std::unique_ptr<lithogui::Widget> host, cWidget& dialog) {
  auto* popup = dynamic_cast<lithogui::Popup*>(host.get());
  if (popup) {
    // Prune the owner when the host closes (bug hunt #34): compose with
    // the onClose cDialog::show already set (notifyHostClosed); the host
    // is destroyed at the next safe point, after the dialog's own close
    // side effects have run.
    lithogui::Widget* raw = host.get();
    const auto prevClose = popup->onClose;
    popup->onClose = [this, raw, prevClose](lithogui::Popup& p) {
      if (prevClose) prevClose(p);
      prunePopupOwner(raw);
    };
  }
  popupOwners_.push_back(std::move(host));
  dialog.attach(this);
  if (popup) gui_.context().openPopup(popup);
  if (std::find(modals_.begin(), modals_.end(), &dialog) == modals_.end()) modals_.push_back(&dialog);
  return dialog;
}

void cUiRoot::closeModal(const lithogui::Widget& modal) {
  const auto* cw = dynamic_cast<const cWidget*>(&modal);
  if (cw) {
    const_cast<cWidget*>(cw)->noteProgrammaticClose();
    if (lithogui::Popup* host = const_cast<cWidget*>(cw)->asPopup()) {
      gui_.context().closePopup(host);
      return;
    }
  }
  if (const auto* popup = dynamic_cast<const lithogui::Popup*>(&modal))
    gui_.context().closePopup(const_cast<lithogui::Popup*>(popup));
}

}  // namespace ui
}  // namespace lithokraft
