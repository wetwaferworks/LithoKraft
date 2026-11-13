// Game-side UI facade over Litho GUI (lithogui), replacing the retired engine
// ui toolkit (litholattice/ui/*, removed when Litho GUI landed in the engine).
//
// The game was written against the old retained toolkit (cUiRoot, cWidget,
// per-class styles, anchor Layout, an event-name handler bus). Litho GUI is a
// different shape: a non-owning UiContext over a caller-owned widget tree,
// per-widget std::function callbacks, box layout (dir/fixedW/fixedH/flex/
// margins/anchors), one global lithogui::Style, and integer texture ids for
// images. This header provides the smallest faithful adapters so the game's
// screens keep their behavior:
//
//   - Theme / Style: the old per-class theme model (styles keyed by class
//     name, glyph metrics, scale) the packs' ui/theme.json describes. The game
//     keeps consuming it exactly as before (resource_manager parses it,
//     game_ui consults classes such as "hud_small" / "hud_debug" / "title");
//     cGameUi ALSO projects the pieces that exist in lithogui::Style onto the
//     live context style (palette, uiScale, row height) so the new widgets
//     pick the theme up.
//   - cUiRoot: owns a lithogui::cLithoGui plus the widget tree root, routes
//     the game's UiEvent stream (see ui_events.h) into dispatchPointer /
//     dispatchKey, owns dialogs as Modal popups, and preserves the old
//     root's queries (wantsKeyboard / wantsPointer / hover / focus /
//     modifiers / popup state).
//   - cWidget: a lithogui::Widget subclass carrying everything the old
//     cWidget had that lithogui::Widget lacks: anchor Layout, the on(name)
//     handler bus + script handlers, tooltip text, data(), text scale, and
//     the old measure() -> desired size bridge. Concrete old widgets
//     (cLabel, cButton, ...) are thin subclasses over their lithogui
//     counterparts with the old setter surface.
//   - Painter / IUiRenderer: the old paint contract, still implemented by
//     the game's cGameUi::paint through the engine's cHudUiRenderer draw
//     list path; cWidget::paintTree walks the tree emitting into it.
//
// Layout note (the one structural adaptation): the old tree placed HUD
// widgets with anchor Layout inside plain containers, which the old toolkit
// arranged itself. lithogui only anchor-places children of
// LayoutDir::None parents, and plain cWidget parents do that; stack/grid
// containers in the new API are dir-based boxes. Placement inputs the game
// sets (anchor/pivot/offset/size) are therefore carried on the widget and
// resolved by the parent facade container at layout time, exactly like the
// old anchoredRect() did.
#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "litholattice/core/json.h"
// Windows header hygiene: winuser.h defines WS_DISABLED / WS_VISIBLE as
// window-style macros, which collide with the GUI's state-bit constants of
// the same names when a translation unit includes Windows headers first.
#if defined(WS_DISABLED)
#undef WS_DISABLED
#endif
#if defined(WS_VISIBLE)
#undef WS_VISIBLE
#endif
#include "litholattice/ui/litho_gui.h"

namespace litholattice {
class Image;
struct HudLayer;
struct HudLayerSplit;
}  // namespace litholattice

namespace lithokraft {
namespace ui {

// The facade lives in namespace lithokraft::ui; the old code spelled it
// litholattice::ui. Geometry comes from lithogui but with the old value
// semantics (notably Color defaulting to opaque white, Edges field names),
// so it is re-declared here exactly as the old ui/geometry.h had it.
struct Vec2 {
  float x = 0, y = 0;
  friend Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
  friend Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
  friend bool operator==(Vec2, Vec2) = default;
};

struct Edges {
  float left = 0, top = 0, right = 0, bottom = 0;
  static constexpr Edges all(float v) { return {v, v, v, v}; }
  static constexpr Edges symmetric(float h, float v) { return {h, v, h, v}; }
  constexpr float horizontal() const { return left + right; }
  constexpr float vertical() const { return top + bottom; }
  friend bool operator==(const Edges&, const Edges&) = default;
};

struct Rect {
  float x = 0, y = 0, w = 0, h = 0;
  constexpr float right() const { return x + w; }
  constexpr float bottom() const { return y + h; }
  constexpr Vec2 pos() const { return {x, y}; }
  constexpr Vec2 size() const { return {w, h}; }
  constexpr bool empty() const { return w <= 0 || h <= 0; }
  constexpr bool contains(Vec2 p) const { return p.x >= x && p.y >= y && p.x < x + w && p.y < y + h; }
  constexpr Rect inset(const Edges& e) const {
    return {x + e.left, y + e.top, std::max(0.f, w - e.horizontal()), std::max(0.f, h - e.vertical())};
  }
  constexpr Rect intersect(const Rect& o) const {
    const float x0 = std::max(x, o.x), y0 = std::max(y, o.y);
    const float x1 = std::min(right(), o.right()), y1 = std::min(bottom(), o.bottom());
    return {x0, y0, std::max(0.f, x1 - x0), std::max(0.f, y1 - y0)};
  }
  friend bool operator==(const Rect&, const Rect&) = default;
};

struct Color {
  float r = 1, g = 1, b = 1, a = 1;
  constexpr Color withAlpha(float alpha) const { return {r, g, b, alpha}; }
  friend bool operator==(const Color&, const Color&) = default;
};

// Conversions at the lithogui boundary (its Rect/Color/Edges differ only in
// member names / defaults; values copy field by field).
inline lithogui::Rect toGui(Rect r) { return {r.x, r.y, r.w, r.h}; }
inline Rect fromGui(lithogui::Rect r) { return {r.x, r.y, r.w, r.h}; }
inline lithogui::Color toGui(Color c) { return {c.r, c.g, c.b, c.a}; }
inline lithogui::Edges toGui(Edges e) {
  lithogui::Edges g;
  g.l = e.left; g.t = e.top; g.r = e.right; g.b = e.bottom;
  return g;
}
inline lithogui::Vec2 toGui(Vec2 v) { return {v.x, v.y}; }

// ------------------------------------------------------------ old theme ---
// The old per-widget-class theme (engine ui/theme.h at 67c1590), kept as the
// game's theme model: resource packs still ship ui/theme.json in this format
// and Theme::parse keeps the old semantics (unknown keys are errors, classes
// inherit from "default", missing classes fall back to "default").

struct Style {
  Color background{0.12f, 0.12f, 0.14f, 0.92f};
  Color hover{0.22f, 0.22f, 0.26f, 0.95f};
  Color pressed{0.32f, 0.34f, 0.42f, 1.f};
  Color disabled{0.10f, 0.10f, 0.10f, 0.6f};
  Color text{0.92f, 0.92f, 0.92f, 1.f};
  Color textDisabled{0.5f, 0.5f, 0.5f, 1.f};
  Color accent{0.35f, 0.62f, 1.f, 1.f};
  Color border{0.f, 0.f, 0.f, 0.6f};
  float borderWidth = 1.f;
  Edges padding = Edges::all(4.f);
  float textScale = 1.f;
  /// D9 2026-11-12 (JD): a selected cSlot's outline thickness in pixels
  /// ("selected_width" in theme.json). 0 = the legacy width (the slot's
  /// scaled border width + 1).
  float selectedWidth = 0.f;
};

class Theme {
 public:
  static Theme defaults();
  static bool parse(const litholattice::Json& json, Theme& out, std::string& err);
  static bool parse(const std::string& text, Theme& out, std::string& err);

  const Style& style(std::string_view widgetClass) const;
  bool hasStyle(std::string_view widgetClass) const { return styles_.find(widgetClass) != styles_.end(); }
  void setStyle(std::string widgetClass, Style s) { styles_[std::move(widgetClass)] = std::move(s); }
  const std::map<std::string, Style, std::less<>>& styles() const { return styles_; }

  float scale() const { return scale_; }
  void setScale(float s) { scale_ = s; }
  Vec2 glyph() const { return glyph_; }
  static float snapTextScale(float s) { return s < 1.f ? 1.f : float(int(s + 0.5f)); }
  float pixelTextScale(float textScale = 1.f) const { return snapTextScale(textScale * scale_); }
  Vec2 glyphSize(float textScale = 1.f) const {
    const float k = pixelTextScale(textScale);
    return {glyph_.x * k, glyph_.y * k};
  }
  float textWidth(std::string_view text, float textScale = 1.f) const {
    return float(text.size()) * glyphSize(textScale).x;
  }

 private:
  std::map<std::string, Style, std::less<>> styles_;
  Vec2 glyph_{8.f, 16.f};
  float scale_ = 1.f;
};

/// Projects a game Theme onto a lithogui::Style for the live context: the
/// "default" class drives the palette (background -> panelBg/controlBg,
/// hover -> controlBgHot, accent -> selection, text, border, border width,
/// default padding), the glyph cell drives body/title text sizes, and the
/// theme scale becomes the context uiScale. Per-class values that have no
/// global counterpart (per-class textScale etc.) stay in the Theme and are
/// applied per widget by the facade classes below.
lithogui::Style projectTheme(const Theme& theme, const lithogui::Style& base);

// ---------------------------------------------------------- old events ----
// The game's input event type (engine ui/events.h at 67c1590), kept as the
// event the game produces (ui_input_bridge) and consumes (game loop, script
// editor, tests). cUiRoot translates it into lithogui dispatch calls.
enum class Key : uint8_t {
  Tab, Enter, Escape, Backspace, Delete, Left, Right, Up, Down, Home, End,
  PageUp, PageDown, Space,
  A, C, V, X, Z, Y, S,  // shortcuts (with Ctrl)
  F10, Grave,           // editor toggle key; tilde console key (character '~')
  Other,
};

enum Modifier : uint8_t { kModNone = 0, kModShift = 1, kModCtrl = 2, kModAlt = 4 };
enum class PointerButton : uint8_t { Left, Right, Middle };

struct UiEvent {
  enum class Type : uint8_t { PointerMove, PointerDown, PointerUp, Wheel, KeyDown, KeyUp, Text };
  Type type = Type::PointerMove;
  Vec2 pos{};
  PointerButton button = PointerButton::Left;
  float wheel = 0;  ///< + = up
  Key key = Key::Other;
  uint8_t mods = kModNone;
  char32_t codepoint = 0;
  int clickCount = 1;  ///< host double-click count for PointerDown

  static UiEvent move(Vec2 p) { UiEvent e; e.type = Type::PointerMove; e.pos = p; return e; }
  static UiEvent down(Vec2 p, PointerButton b = PointerButton::Left) {
    UiEvent e; e.type = Type::PointerDown; e.pos = p; e.button = b; return e;
  }
  static UiEvent up(Vec2 p, PointerButton b = PointerButton::Left) {
    UiEvent e; e.type = Type::PointerUp; e.pos = p; e.button = b; return e;
  }
  static UiEvent scroll(Vec2 p, float steps) { UiEvent e; e.type = Type::Wheel; e.pos = p; e.wheel = steps; return e; }
  static UiEvent keyDown(Key k, uint8_t m = kModNone) { UiEvent e; e.type = Type::KeyDown; e.key = k; e.mods = m; return e; }
  static UiEvent keyUp(Key k, uint8_t m = kModNone) { UiEvent e; e.type = Type::KeyUp; e.key = k; e.mods = m; return e; }
  static UiEvent text(char32_t c) { UiEvent e; e.type = Type::Text; e.codepoint = c; return e; }

  bool shift() const { return (mods & kModShift) != 0; }
  bool ctrl() const { return (mods & kModCtrl) != 0; }
  bool alt() const { return (mods & kModAlt) != 0; }
};

/// Clipboard seam (old ui_renderer.h): the game injects the OS clipboard;
/// the process-local fallback answers when none is wired.
class IClipboard {
 public:
  virtual ~IClipboard() = default;
  virtual std::string text() const = 0;
  virtual void setText(const std::string& text) = 0;
};

class cMemoryClipboard final : public IClipboard {
 public:
  std::string text() const override { return text_; }
  void setText(const std::string& t) override { text_ = t; }

 private:
  std::string text_;
};

/// Script host seam (old ui_root.h): script handlers by widget event.
class IScriptHost {
 public:
  virtual ~IScriptHost() = default;
  virtual bool callHandler(std::string_view functionDecl, std::string_view widgetId) = 0;
};

// ------------------------------------------------------------- layout -----
/// Anchor layout (old ui/widget.h Layout + anchoredRect), unchanged math.
struct Layout {
  Vec2 anchorMin{0, 0}, anchorMax{1, 1};
  Edges insets;
  Vec2 offset;
  Vec2 pivot{0, 0};
  Vec2 size;
  Vec2 minSize;
  Edges margin;
  float flex = 0;

  static Layout fill(Edges in = {}) { Layout l; l.insets = in; return l; }
  static Layout at(Vec2 anchor, Vec2 pivot, Vec2 offset = {}, Vec2 size = {}) {
    Layout l;
    l.anchorMin = l.anchorMax = anchor;
    l.pivot = pivot;
    l.offset = offset;
    l.size = size;
    return l;
  }
  static Layout sized(Vec2 size, float flex = 0) {
    Layout l = at({0, 0}, {0, 0}, {}, size);
    l.flex = flex;
    return l;
  }
};

Rect anchoredRect(const Layout& l, const Rect& parent, Vec2 pref);

// ------------------------------------------------------------- painter ----
class cWidget;

/// Immediate drawing in paint order (old IUiRenderer), with the theme the
/// widgets were laid out under so text uses the game's snapped glyph scale.
class IUiRenderer {
 public:
  virtual ~IUiRenderer() = default;
  virtual void fillRect(const Rect& r, Color c) = 0;
  virtual void drawText(Vec2 pos, std::string_view text, Color c, float scale) = 0;
  virtual void drawImage(const Rect& r, std::string_view image, Color tint) = 0;
  virtual void pushClip(const Rect& r) = 0;
  virtual void popClip() = 0;
};

class Painter {
 public:
  Painter(IUiRenderer& renderer, const Theme& theme) : r_(renderer), theme_(theme) {}
  const Theme& theme() const { return theme_; }
  void fill(const Rect& r, Color c) {
    if (c.a > 0 && r.w > 0 && r.h > 0) r_.fillRect(r, c);
  }
  void frame(const Rect& r, Color c, float w);
  void text(Vec2 pos, std::string_view s, Color c, float textScale = 1.f) {
    if (!s.empty()) r_.drawText(pos, s, c, theme_.pixelTextScale(textScale));
  }
  void textIn(const Rect& r, std::string_view s, Color c, bool center = false, float textScale = 1.f);
  void image(const Rect& r, std::string_view name, Color tint) { r_.drawImage(r, name, tint); }

  class ClipScope {
   public:
    ClipScope(Painter& p, const Rect& r) : p_(p) { p_.r_.pushClip(r); }
    ~ClipScope() { p_.r_.popClip(); }
    ClipScope(const ClipScope&) = delete;
    ClipScope& operator=(const ClipScope&) = delete;

   private:
    Painter& p_;
  };

 private:
  IUiRenderer& r_;
  const Theme& theme_;
};

// -------------------------------------------------------------- widget ----
class cUiRoot;

/// Facade widget: a lithogui::Widget that also carries the old cWidget
/// surface (id is lithogui's; style class, layout, handler bus, tooltip,
/// data, measure). Concrete game widgets derive from this via their
/// lithogui counterparts (see the cXxx classes below).
class cWidget : public lithogui::Widget {
 public:
  using Handler = std::function<void(cWidget&)>;

  explicit cWidget(std::string id = {}, std::string styleClass = "none");
  ~cWidget() override;

  // ---- tree (lithogui owns the children; these mirror the old verbs)
  template <typename T, typename... Args>
  T& add(Args&&... args) {
    auto w = std::make_unique<T>(std::forward<Args>(args)...);
    T& ref = *w;
    addChild(std::move(w));
    return ref;
  }
  std::unique_ptr<lithogui::Widget> removeChild(const lithogui::Widget& child);
  void clearChildren();
  /// lithogui::Widget::addChild is non-virtual; the facade version (used by
  /// add<> and the container builders) also records parentage. Returns the
  /// added widget (as a cWidget when it is one; *this for a foreign child).
  cWidget& addChild(std::unique_ptr<lithogui::Widget> child);
  cWidget* parentW() const { return parent_; }
  /// True once removeChild() has detached this widget from a parent: its
  /// lithogui base parent_ may still point at the old parent (the engine
  /// Widget has no detach verb), so ancestry walkers must not trust it.
  bool detached() const { return detached_; }
  /// Depth-first search of this subtree by id.
  cWidget* find(std::string_view id);
  const cWidget* find(std::string_view id) const { return const_cast<cWidget*>(this)->find(id); }
  template <typename T>
  T* findAs(std::string_view id) { return dynamic_cast<T*>(find(id)); }
  template <typename T>
  const T* findAs(std::string_view id) const { return dynamic_cast<const T*>(find(id)); }
  cUiRoot* uiRoot() const { return ui_; }

  // ---- properties
  cWidget& setId(std::string id) { idCopy_ = std::move(id); return *this; }
  const std::string& styleClass() const { return styleClass_; }
  cWidget& setStyleClass(std::string c);
  cWidget& setVisible(bool v);  // defined after cUiRoot (JD paint ruling: flips are dirty marks)
  cWidget& setEnabled(bool e) { Widget::setEnabled(e); return *this; }
  bool interactive() const;
  /// Dynamic readout surface membership (JD paint ruling, 2026-11-09):
  /// a widget flagged dynamic (the tick/TPS panel, the F3 debug panel)
  /// and its whole subtree paint into the root's DYNAMIC layer only,
  /// never the main layer... so per-frame readout text can dirty the
  /// tiny dynamic layer every frame without ever rebuilding the main
  /// layer. Unflagged subtrees are main-layer, as before.
  cWidget& setDynamicSurface(bool d) { dynamic_ = d; return *this; }
  bool dynamicSurface() const { return dynamic_; }
  const std::string& tooltip() const { return tooltip_; }
  cWidget& setTooltip(std::string t) { tooltip_ = std::move(t); return *this; }
  Layout& layout() { return layout_; }
  const Layout& layout() const { return layout_; }
  cWidget& setLayout(const Layout& l);
  int data() const { return data_; }
  cWidget& setData(int d) { data_ = d; return *this; }
  bool hovered() const;
  bool pressed() const { return hasState(lithogui::WS_PRESSED); }
  bool focused() const;
  void focus();
  const Style& style() const;   ///< own class style from the attached root's theme (built-in theme while detached)
  const Theme& theme() const;   ///< the attached root's theme (built-in theme while detached)

  // ---- events
  cWidget& on(std::string event, Handler h);
  cWidget& onScript(std::string event, std::string functionDecl);
  void emit(std::string_view event);
  bool hasHandler(std::string_view event) const;

  // ---- layout / paint (facade passes; parents call these, not Widget::layout)
  /// Preferred outer size (content + padding). Default: the anchored
  /// children's extent or 0. Containers override.
  virtual Vec2 measure(const Theme& theme) const;
  static Vec2 childSize(const cWidget& child, const Theme& theme);
  /// Places this widget at `r` and arranges its children.
  void place(const Rect& rect, const Theme& theme);
  void paintTree(Painter& p) const;
  /// Paints this subtree for ONE layer of the root's two-layer paint
  /// (JD paint ruling, 2026-11-09): dynamic pass paints only subtrees
  /// rooted at a dynamic-surface widget; the main pass skips them.
  /// Foreign (non-facade) children belong to the main layer.
  void paintTreeLayer(Painter& p, bool dynamicPass, bool inDynamic = false) const;

  // ---- lithogui overrides: keep rect_/lastStyle bookkeeping, then run the
  // facade arrangement (box containers arrange in arrange(), others anchor).
  void layout(lithogui::Rect r, const lithogui::Style& style = lithogui::defaultStyle()) override;

  /// For widgets hosted inside a lithogui::Popup they do not own (dialogs):
  /// the popup to close when this widget is closed through the root.
  virtual lithogui::Popup* asPopup() { return nullptr; }
  /// The root is about to close this widget's popup programmatically
  /// (closeModal): implementations suppress their user-close side effects.
  virtual void noteProgrammaticClose() {}
  /// Asked by the root before an Escape key reaches the context (which
  /// would close the top popup unconditionally): true vetoes the close
  /// (the old dialog close guard; the implementation runs its own
  /// confirmation flow instead).
  virtual bool vetoPopupClose() { return false; }

  /// lithogui has no widget rename; the facade keeps its own id copy so
  /// setId()/find() semantics match the old toolkit. find() uses this.
  const std::string& facadeId() const { return idCopy_; }

 protected:
  /// Positions children inside the widget's rect minus style padding.
  /// Default: anchored placement. Box containers override.
  virtual void arrange(const Theme& theme);
  /// Draws this widget (not its children). Default: background + border.
  virtual void paint(Painter& p) const;
  virtual void paintChildren(Painter& p) const;
  /// Layer-aware child paint (see paintTreeLayer). Overrides of
  /// paintChildren that emit their children themselves (cScrollView)
  /// ride the main layer: dynamic surfaces never live inside them.
  void paintChildrenLayer(Painter& p, bool dynamicPass, bool inDynamic = false) const;
  void relayout();
  Color stateBackground() const;
  /// Syncs the lithogui placement fields (dir / fixed sizes / flex /
  /// margins / text style scale) from facade state before a layout pass.
  virtual void syncNative();
  void attach(cUiRoot* ui);
  /// WS4 change gate: leaf mutators call this when a paint-visible
  /// property actually changes, so cUiRoot::paint can skip its rebuild
  /// on frames where nothing changed. Defined after cUiRoot below.
  void touchPaint();

  std::string styleClass_;
  std::string tooltip_;
  std::string idCopy_;
  Layout layout_;
  int data_ = 0;

 private:
  friend class cUiRoot;
  cWidget* parent_ = nullptr;
  bool dynamic_ = false;  // dynamic readout surface membership (see setDynamicSurface)
  bool detached_ = false;  // set by removeChild: the base parent_ is stale from here on
  cUiRoot* ui_ = nullptr;
  std::vector<std::pair<std::string, Handler>> handlers_;
  std::vector<std::pair<std::string, std::string>> scriptHandlers_;
};

// ---------------------------------------------------------------- root ----
/// Owns the Litho GUI context and the tree; routes the game's UiEvents.
class cUiRoot {
 public:
  explicit cUiRoot(Theme theme = Theme::defaults());
  ~cUiRoot();
  cUiRoot(const cUiRoot&) = delete;
  cUiRoot& operator=(const cUiRoot&) = delete;

  cWidget& root() { return *root_; }
  const cWidget& root() const { return *root_; }
  const Theme& theme() const { return theme_; }
  void setTheme(Theme t);
  void setScriptHost(IScriptHost* host) { scripts_ = host; }
  IScriptHost* scriptHost() const { return scripts_; }
  void setClipboard(IClipboard* c) { clipboard_ = c; }
  IClipboard& clipboard() { return clipboard_ ? *clipboard_ : fallbackClipboard_; }
  lithogui::ILithoGui& gui() { return gui_; }
  const lithogui::ILithoGui& gui() const { return gui_; }
  lithogui::UiContext& context() { return gui_.context(); }

  // ---- frame
  void layout(Vec2 viewport);
  /// Paints the tree into `hud` through the engine's cHudUiRenderer draw
  /// list path: facade Drawer widgets (ui_widgets.cpp) translate paint()
  /// into lithogui draw commands whose image quads carry texture ids.
  /// `images` resolves a texture id to a colour swatch, `icons` to a
  /// pixel-art icon image (the cGameUi::paint seams, unchanged).
  void paint(litholattice::HudLayer& hud,
             std::function<std::optional<Color>(int textureId)> images,
             std::function<const litholattice::Image*(int textureId)> icons) const;
  /// Split delivery (GUI draw-cost program, cure 1, 2026-11-10): fills
  /// `hud` exactly as paint() does AND reports the two stream refs
  /// (static main layer + dynamic readout layer, each with the
  /// generation it was built under) for backends that expand and
  /// upload the streams separately. The refs point at this root's own
  /// paint caches: stable while their generations hold (the caches are
  /// rebuilt only when a generation advances), freed with the root.
  void paintSplit(litholattice::HudLayer& hud, litholattice::HudLayerSplit& split,
                  std::function<std::optional<Color>(int textureId)> images,
                  std::function<const litholattice::Image*(int textureId)> icons) const;
  /// Advances UI time (frame animation phase; kept for the old call sites).
  void update(float dt);
  float time() const { return time_; }
  /// Routes one event; true if the UI consumed it.
  bool handleEvent(const UiEvent& e);
  bool wantsKeyboard() const;
  bool wantsPointer(Vec2 p) const;
  void destroyLater(std::unique_ptr<lithogui::Widget> w);
  uint8_t modifiers() const { return mods_; }

  // ---- focus / state
  cWidget* focus() const;
  /// Checked validity of the context's focus holder: true only when the
  /// focused widget is not tombstoned. Pointer-value check only... safe
  /// on a dangling focus (JD's crash, 2026-11-08 round 2).
  bool focusOwnerAlive() const;
  void setFocus(cWidget* w);
  cWidget* hover() const;
  cWidget* capture() const { return capture_; }

  // ---- hover tooltips (2026-11-08, WS3)
  /// Driven from update(): when the pointer rests on a widget carrying
  /// tooltip text (or on a child of one) for kTooltipDelay seconds, the
  /// engine Tooltip popup opens anchored to that widget, above every
  /// panel; it closes the moment the hover target or its text changes,
  /// on any pointer press, or when the target dies (tombstone-checked,
  /// never dereferenced). The engine Tooltip clamps itself to the
  /// viewport; the renderer gives its text the standard drop shadow.
  void updateTooltip(float dt);
  void hideTooltip();
  bool tooltipVisible() const { return tooltip_ && tooltip_->isOpen(); }
  /// next-build-2: whether the last paint() call rebuilt (vs replayed the
  /// cache), and how many draws the layer it produced carries. Set inside
  /// paint(); read by the GUI phase profile.
  bool paintRebuilt() const { return paintRebuilt_; }
  /// Textured icons Phase B step 0 (2026-11-09): the composed layer's
  /// element split behind paintDrawCount()... rects / texts / draws as
  /// separate counts, plus how many of the draws are textured icon
  /// quads, so the gui-profile line can price the icon migration
  /// surface by surface instead of arguing from one summed number.
  size_t paintRectCount() const { return paintRectCount_; }
  size_t paintTextCount() const { return paintTextCount_; }
  size_t paintDrawOnlyCount() const { return paintDrawOnlyCount_; }
  size_t paintIconCount() const { return paintIconCount_; }
  size_t paintDrawCount() const { return paintDrawCount_; }
  /// Dynamic readout layer outcome of the last paint(): whether it
  /// rebuilt, and how many draws it carries. The dynamic layer has its
  /// own generation discipline (update() advances it only while a
  /// dynamic surface is visible); the main layer's generation is never
  /// touched by readout text (JD paint ruling, 2026-11-09).
  bool dynamicRebuilt() const { return dynamicRebuilt_; }
  size_t dynamicDrawCount() const { return dynamicDrawCount_; }
  /// The live tooltip widget while one is showing (selftests read its
  /// text; the context exposes no popup iteration).
  const lithogui::Tooltip* tooltipForTest() const {
    return tooltipVisible() ? tooltip_.get() : nullptr;
  }
  /// Marks the paint output stale (WS4 change gate): every facade
  /// mutation that can alter a pixel funnels through here. cGameUi and
  /// the tooltip driver call it for changes the facade cannot see.
  uint64_t paintGeneration() const { return paintGen_; }
  /// 2026-11-09 next-build-2 (GUI diagnosis instrumentation): generation
  /// bumps attributed by source, for the gui-profile line (which source
  /// keeps the change gate from engaging is a number, not an argument).
  /// Counters only... no behavior change. touchPaint() above stays the
  /// generic funnel (widget-side mutators attribute through their root);
  /// handleEvent's blanket bump is the Event source.
  enum class PaintBump : uint8_t { Text, Event, Structure, Tooltip, Theme, Other };
  void touchPaintAs(PaintBump s) { ++paintGen_; ++paintBumps_[static_cast<size_t>(s)]; }
  void touchPaint() { ++paintGen_; }
  /// Marks ONLY the dynamic readout layer dirty (see cWidget::touchPaint).
  void touchDynamic() { ++dynGen_; }
  /// A dynamic surface's own membership changed (it was shown, hidden,
  /// or re-flagged): both layers' content changed (the readout leaves
  /// or joins the dynamic layer; any main-layer paint of its surround
  /// is keyed separately), so both generations move. Called by
  /// cWidget::setVisible through the facade's own visibility path...
  /// see cWidget::setVisible in ui_facade.h.
  void touchDynamicMembership() { ++dynGen_; ++paintGen_; }
  void noteEventBump() { ++paintBumps_[static_cast<size_t>(PaintBump::Event)]; }
  void noteTextBump() { ++paintBumps_[static_cast<size_t>(PaintBump::Text)]; }
  void noteStructureBump() { ++paintBumps_[static_cast<size_t>(PaintBump::Structure)]; }
  void noteTooltipBump() { ++paintBumps_[static_cast<size_t>(PaintBump::Tooltip)]; }
  void noteThemeBump() { ++paintBumps_[static_cast<size_t>(PaintBump::Theme)]; }
  uint64_t paintBumps(PaintBump s) const { return paintBumps_[static_cast<size_t>(s)]; }
  /// The last pointer position the facade dispatched (physical px),
  /// and whether any pointer event has been seen yet. Read by the
  /// cursor-stack display (2026-11-11): the carried stack's widget is
  /// placed from this polled position at layout time, the same
  /// discipline the hover protocol uses, so a pointer move never has
  /// to push layout through the widget tree.
  Vec2 lastPointer() const { return lastPointer_; }
  bool pointerSeen() const { return pointerSeen_; }
  // Pacing instrumentation getters (2026-11-11): see the counters
  // declared beside the paint caches below.
  uint64_t mainPaintCalls() const { return mainPaintCalls_; }
  uint64_t mainPaintNoop() const { return mainPaintNoop_; }
  uint64_t dynPaintCalls() const { return dynPaintCalls_; }
  uint64_t dynPaintNoop() const { return dynPaintNoop_; }


  // ---- layers: dialogs/modals are lithogui Modal popups
  cWidget& pushModal(std::unique_ptr<lithogui::Widget> modal);
  /// Pushes a popup host (owned here) whose facade face is `dialog`:
  /// the dialog is what topModal()/modals tracking report.
  cWidget& pushDialog(std::unique_ptr<lithogui::Widget> host, cWidget& dialog);
  void closeModal(const lithogui::Widget& modal);
  bool modalOpen() const { return !modals_.empty(); }
  cWidget* topModal() const { return modals_.empty() ? nullptr : modals_.back(); }
  bool popupOpen() const { return gui_.context().popupCount() > 0; }
  /// Hosts owned by the facade (dialogs live here while open); the probe
  /// reads this to prove the per-frame centering pass actually iterates.
  size_t popupOwnersPending() const { return popupOwners_.size(); }
  /// Number of popups the context currently reports (dialogs included).
  size_t popupCount() const { return gui_.context().popupCount(); }

 private:
  friend class cWidget;
  void forget(const cWidget* w);
  void syncModalsFromPopups();
  /// Erases one owner from popupOwners_ (called from the owner's onClose
  /// hook when its popup closes, so closed dialogs stop being re-laid-out
  /// and re-opened on every frame). Deferred while layout() iterates.
  void prunePopupOwner(lithogui::Widget* owner);
  Theme theme_;
  std::unique_ptr<lithogui::cLithoGui> guiOwner_;
  lithogui::cLithoGui& gui_;
  std::unique_ptr<cWidget> root_;
  std::vector<std::unique_ptr<lithogui::Widget>> popupOwners_;  // dialogs live here while open
  std::vector<lithogui::Widget*> ownersToPrune_;               // closed owners, erased at a safe point
  bool inLayout_ = false;  // layout() is iterating popupOwners_: prune requests defer to its tail
  std::vector<cWidget*> modals_;                       // open dialogs (facade views of popupOwners_)
  std::vector<std::unique_ptr<lithogui::Widget>> closing_;
  std::vector<std::unique_ptr<lithogui::Widget>> deferred_;  // destroyed at the next layout()
  cWidget* capture_ = nullptr;
  IScriptHost* scripts_ = nullptr;
  IClipboard* clipboard_ = nullptr;
  cMemoryClipboard fallbackClipboard_;
  Vec2 viewport_{};
  Vec2 lastPointer_{};
  bool pointerSeen_ = false;
  float time_ = 0;
  uint8_t mods_ = kModNone;
  // Hover tooltip state (WS3). Declared after guiOwner_ so the Tooltip
  // (which unregisters itself from the context) dies before the gui.
  std::unique_ptr<lithogui::Tooltip> tooltip_;
  const cWidget* tipTarget_ = nullptr;  // pointer-compared only, tombstone-checked
  std::string tipText_;
  float tipTimer_ = 0;
  // WS4 paint cache: the last expanded HudLayer and the key it was
  // built under. paint() replays it while the key holds (see paint()).
  uint64_t paintGen_ = 1;
  uint64_t paintBumps_[6] = {};  // next-build-2: PaintBump attribution counters
  mutable uint64_t paintGenBuilt_ = 0;
  mutable size_t paintPopupsBuilt_ = ~size_t(0);
  mutable Vec2 paintViewportBuilt_{-1, -1};
  mutable bool paintRebuilt_ = false;   // next-build-2 profile: last paint() rebuilt vs replayed
  mutable size_t paintDrawCount_ = 0;   // next-build-2 profile: draws in the last painted layer
  mutable size_t paintRectCount_ = 0;   // textured icons step 0: composed layer split
  mutable size_t paintTextCount_ = 0;
  mutable size_t paintDrawOnlyCount_ = 0;
  mutable size_t paintIconCount_ = 0;
  mutable std::unique_ptr<litholattice::HudLayer> paintCache_;
  // Dynamic readout layer cache (JD paint ruling, 2026-11-09): the
  // dynamic subtrees' own expanded layer and its own key. dynGen_ is
  // advanced by update() (non-const) once per frame while any dynamic
  // surface is visible, so the dynamic layer repaints at most once per
  // frame and only while readouts are on screen; when none is visible
  // the layer replays empty and costs nothing.
  uint64_t dynGen_ = 1;
  mutable uint64_t dynGenBuilt_ = 0;
  mutable bool dynVisibleBuilt_ = false;
  bool dynVisible_ = false;  // update()'s last anyDynamicVisible result
  mutable bool dynamicRebuilt_ = false;
  mutable size_t dynamicDrawCount_ = 0;
  mutable std::unique_ptr<litholattice::HudLayer> dynCache_;
  // Pacing instrumentation (2026-11-11, JD's dirty-flag question on
  // build 217): per-layer paint accounting, read by the game-ui
  // selftest (mutable because paint() is const). mainPaintCalls_
  // counts paint() invocations that found the main layer dirty;
  // mainPaintNoop_ counts the subset whose rebuilt layer was
  // byte-identical to the cache it replaced... bumps that cost a full
  // rebuild for zero pixel change. The dyn pair does the same for the
  // readout layer, whose generation advances once per frame BY DESIGN
  // (update() above): there, dynNoop_ counts the frames whose readout
  // repaint reproduced the previous layer exactly, which times the
  // readout's real change rate (the tick line moves at about 1 Hz).
  mutable uint64_t mainPaintCalls_ = 0, mainPaintNoop_ = 0;
  mutable uint64_t dynPaintCalls_ = 0, dynPaintNoop_ = 0;
  uint64_t layoutGenBuilt_ = 0;
  size_t layoutPopupsBuilt_ = ~size_t(0);
  Vec2 layoutViewportBuilt_{-1, -1};
};

inline cWidget& cWidget::setVisible(bool v) {
  if (v != visible()) {
    Widget::setVisible(v);
    // JD paint ruling (2026-11-09): visibility is a paint dirty mark.
    // A plain widget's flip dirties the main layer through the normal
    // funnel; a DYNAMIC surface's own flip changes which layer
    // carries its surround, so the root is told directly as well.
    touchPaint();
    if (dynamic_ && ui_) ui_->touchDynamicMembership();
  }
  return *this;
}

inline void cWidget::touchPaint() {
  if (!ui_) return;
  // JD paint ruling (2026-11-09): a mutation inside a dynamic readout
  // surface marks ONLY the dynamic layer dirty. The walk is the
  // widget's own ancestor chain (short; readout panels sit near the
  // HUD root), checked per bump... text churn at frame rate pays a
  // handful of pointer hops, never a main-layer rebuild.
  const cWidget* w = this;
  while (w) {
    if (w->dynamicSurface()) {
      ui_->touchDynamic();
      return;
    }
    w = w->parentW();
  }
  ui_->touchPaint();
}

/// The built-in theme used for detached widgets (old cUiRoot behavior).
const Theme& builtinTheme();

/// Icon/image name <-> texture id registry for the draw-list paint path:
/// Litho GUI image quads carry integer ids, the game names its images
/// ("block:12", "item:3"). Ids are assigned in first-use order and stay
/// stable for the process; cGameUi::paint resolves them back per id.
int iconTextureId(const std::string& name);
const std::string& iconTextureName(int textureId);

/// Icon tint a cSlot shows with (carried beside the quad because DrawList
/// image quads have no tint channel; the renderer's swatch path multiplies).
void noteIconTint(int textureId, Color tint);
Color iconTint(int textureId);

/// The draw context of the paint pass in flight (set by cUiRoot::paint):
/// paint() implementations emit into it through the Drawer shim.
struct PaintSink {
  lithogui::DrawList* dl = nullptr;
  const Theme* theme = nullptr;
};
PaintSink& paintSink();

/// UiEvent -> lithogui::KeyEvent (the debug console's new API consumes
/// lithogui events directly; game.cpp converts at that call site).
lithogui::KeyEvent toGuiKeyEvent(const UiEvent& e);

/// A Painter that emits into the active paint sink (used by popup hosts
/// whose draw() runs inside cUiRoot::paint's emitFrame pass).
Painter sinkPainter(const Theme& theme);

}  // namespace ui
}  // namespace lithokraft
