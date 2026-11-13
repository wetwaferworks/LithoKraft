// Concrete facade widgets over their Litho GUI counterparts, carrying the
// old toolkit's class names and setter surface (see ui_facade.h for the
// architecture). Included by game code that builds screens; the classes are
// default-constructible like the old ones, with ids assigned via setId().
//
// Dialog hosting: a cDialog IS a lithogui::Popup (Modal flavor). The
// context's popup stack owns input routing and close semantics for it; the
// cUiRoot keeps the owning unique_ptr while it is open.
#pragma once
#include <string>
#include <vector>
#include "lithokraft/game/ui_facade.h"

namespace lithokraft {
namespace ui {

// ------------------------------------------------------------ containers --
enum class Orientation { Vertical, Horizontal };

class cPanel : public cWidget {
 public:
  explicit cPanel(std::string styleClass = "panel") : cWidget({}, std::move(styleClass)) {}
  // Per-instance paint overrides (the cLabel::setColor precedent): a
  // screen section can carry its own panel colours (the inventory
  // half's navy, the Agent panel's pink border) without a theme edit.
  cPanel& setBackgroundColor(Color c) { bgOverride_ = c; hasBg_ = true; return *this; }
  cPanel& setBorderColor(Color c, float width = 1.f) { borderOverride_ = c; borderW_ = width; hasBorder_ = true; return *this; }
  /// Press tap (cursor-stack plan, 2026-11-11): when armed, a left
  /// PointerDown that BUBBLES UP to this panel unconsumed (every
  /// widget under the press already declined it: background, gaps,
  /// inert labels) emits "press" here, and the press stays unconsumed
  /// so routing above the panel is unchanged. A press a child consumed
  /// (a slot, a button, a text field) never reaches the panel, so an
  /// armed panel hears exactly the presses that hit nothing live.
  /// Default off: every other panel in the game is untouched.
  cPanel& setPressTap(bool on) { pressTap_ = on; return *this; }
  bool handlePointer(const lithogui::PointerEvent& e) override;
 protected:
  void paint(Painter& p) const override;
 private:
  Color bgOverride_{};
  Color borderOverride_{};
  float borderW_ = 1.f;
  bool hasBg_ = false;
  bool hasBorder_ = false;
  bool pressTap_ = false;
};

/// The screen's global frame (JD ruling, 2026-11-09: "set the GUI in
/// one global frame that is the the screen size and then anchor all
/// the sub frames to that frame"). A cFrame owns a DESIGN size: the
/// dialog measures itself from it, and the frame's single content
/// child is placed at the frame's content box (its rect inset by the
/// content pad) at every layout, so the whole screen is solved inside
/// the frame's bounds at any viewport. Sections inside are sub-frame
/// panels anchored to this frame; the layout selftest asserts no
/// descendant rect exceeds the frame's rect.
class cFrame : public cWidget {
 public:
  cFrame(float designW, float designH) : cWidget({}, "frame"), design_{designW, designH} {}
  Vec2 designSize() const { return design_; }
  cFrame& setContentPad(float p) { pad_ = p; return *this; }
  float contentPad() const { return pad_; }
  Vec2 measure(const Theme&) const override { return design_; }
 protected:
  void arrange(const Theme& theme) override;
  void paint(Painter& p) const override { (void)p; }  // bare: the dialog paints the ground
 private:
  Vec2 design_;
  float pad_ = 0;
};

/// Vertical/horizontal box of children (lithogui dir box). The old stack
/// stretched children across its cross axis and packed along the main axis;
/// spacing is carried as per-child margins (before = top/left) so plain
/// facade parents and lithogui containers agree.
class cStackPanel : public cWidget {
 public:
  explicit cStackPanel(Orientation o = Orientation::Vertical, float spacing = 4, std::string styleClass = "stack")
      : cWidget({}, std::move(styleClass)), orientation_(o), spacing_(spacing) {
    dir = o == Orientation::Vertical ? lithogui::LayoutDir::Column : lithogui::LayoutDir::Row;
  }
  Orientation orientation() const { return orientation_; }
  cStackPanel& setSpacing(float s) { spacing_ = s; return *this; }
  float spacing() const { return spacing_; }
  cStackPanel& setStretch(bool s) { stretch_ = s; return *this; }
  bool stretch() const { return stretch_; }
  Vec2 measure(const Theme& theme) const override;

 protected:
  void arrange(const Theme& theme) override;
  void paint(Painter& p) const override { (void)p; }  // bare, like the old "stack" class
  void syncNative() override;

 private:
  Orientation orientation_;
  float spacing_;
  bool stretch_ = true;
};

/// Fixed-cell grid (palette / carry grids).
class cGridPanel : public cWidget {
 public:
  cGridPanel(int columns, Vec2 cell, float spacing = 4, std::string styleClass = "stack")
      : cWidget({}, std::move(styleClass)), columns_(columns < 1 ? 1 : columns), cell_(cell), spacing_(spacing) {}
  int columns() const { return columns_; }
  Vec2 cell() const { return cell_; }
  cGridPanel& setColumns(int c) { columns_ = c < 1 ? 1 : c; return *this; }
  /// Fit mode: the cell is SOLVED from the width the grid is actually
  /// placed at (square cells: (width - gaps) / columns) instead of
  /// taken from the constructor constant, so a grid can never sum
  /// wider than its sub-frame. measure() reports the last solved cell
  /// (the constructor cell until the first arrange), so a fit grid's
  /// height settles one layout after its width does.
  cGridPanel& setFitWidth(bool f) { fit_ = f; return *this; }
  bool fitWidth() const { return fit_; }
  /// C9 2026-11-12 (JD inventory): fit-box mode. The square cell is the LARGER of nothing and the
  /// smaller of the width solve and the height solve, so the grid fills its rect without spilling;
  /// the cells are centred horizontally and sit at the bottom of the rect.
  cGridPanel& setFitBox(bool f) { fitBox_ = f; return *this; }
  bool fitBox() const { return fitBox_; }
  float solvedCell() const { return solvedCell_; }
  Vec2 measure(const Theme& theme) const override;

 protected:
  void arrange(const Theme& theme) override;
  void paint(Painter& p) const override { (void)p; }

 private:
  int columns_;
  Vec2 cell_;
  float spacing_;
  bool fit_ = false;
  bool fitBox_ = false;
  mutable float solvedCell_ = 0;
};

/// Overlay scrollbar for cScrollView (also exposes the old thickness).
class cScrollBar : public cWidget {
 public:
  static constexpr float kThickness = 10;
  explicit cScrollBar(Orientation o = Orientation::Vertical) : cWidget({}, "scrollbar"), orientation_(o) {}
  void setRange(float contentSize, float viewSize) { content_ = contentSize; view_ = viewSize; }
  float offset() const { return offset_; }
  float maxOffset() const { return std::max(0.f, content_ - view_); }
  void setOffset(float o) { offset_ = std::clamp(o, 0.f, maxOffset()); }
  bool needed() const { return content_ > view_ + 0.5f; }
  Orientation orientation() const { return orientation_; }
  Vec2 measure(const Theme&) const override { return {kThickness, kThickness}; }

 protected:
  void paint(Painter& p) const override;

 private:
  Orientation orientation_;
  float content_ = 0, view_ = 0, offset_ = 0;
};

/// A clipped, wheel-scrolled viewport over one content child, with an
/// overlay scrollbar (cScrollBar) down the right edge, as the old toolkit.
class cScrollView : public cWidget {
 public:
  cScrollView() : cWidget({}, "scrollview") {}
  cWidget& setContent(std::unique_ptr<lithogui::Widget> content);
  template <typename T, typename... Args>
  T& emplaceContent(Args&&... args) {
    auto w = std::make_unique<T>(std::forward<Args>(args)...);
    T& ref = *w;
    setContent(std::move(w));
    return ref;
  }
  cWidget* content() const { return content_; }
  float scroll() const { return bar_ ? bar_->offset() : 0; }
  void setScroll(float y) { if (bar_) bar_->setOffset(y); }
  Vec2 measure(const Theme& theme) const override;
  bool handlePointer(const lithogui::PointerEvent& e) override;
  // The view paints its children clipped to its own rect (paintChildren's
  // ClipScope), so hit testing must clip the same way (litho_gui's
  // hitRecurse C9 rule): scrolled-away rows were hittable through the
  // dialog's empty area below the view before this override existed.
  bool clipsChildren() const override { return true; }

 protected:
  void arrange(const Theme& theme) override;
  void paint(Painter& p) const override { (void)p; }
  void paintChildren(Painter& p) const override;

 private:
  cWidget* content_ = nullptr;
  cScrollBar* bar_ = nullptr;
};

/// Two panes with a draggable divider (script editor file list / text).
class cSplitter : public cWidget {
 public:
  explicit cSplitter(Orientation o = Orientation::Horizontal, float ratio = 0.3f)
      : cWidget({}, "splitter"), orientation_(o), ratio_(ratio) {}
  cWidget& setPane(int index, std::unique_ptr<lithogui::Widget> pane);
  float ratio() const { return ratio_; }
  void setRatio(float r) { ratio_ = std::clamp(r, 0.05f, 0.95f); }
  static constexpr float kHandle = 6;
  Vec2 measure(const Theme& theme) const override;
  bool handlePointer(const lithogui::PointerEvent& e) override;

 protected:
  void arrange(const Theme& theme) override;
  void paint(Painter& p) const override { (void)p; }

 private:
  Orientation orientation_;
  float ratio_;
  cWidget* panes_[2] = {nullptr, nullptr};
  bool dragging_ = false;
};

/// Tabbed pages (settings screen). Pages are facade widgets; the header
/// strip is drawn by paint and hit-tested in handlePointer.
class cTabView : public cWidget {
 public:
  cTabView() : cWidget({}, "tabview") {}
  cWidget& addTab(std::string title, std::unique_ptr<lithogui::Widget> page);
  int current() const { return current_; }
  void setCurrent(int index, bool notify = false);
  int count() const { return int(titles_.size()); }
  Vec2 measure(const Theme& theme) const override;
  bool handlePointer(const lithogui::PointerEvent& e) override;
  bool handleKey(const lithogui::KeyEvent& e) override;

 protected:
  void arrange(const Theme& theme) override;
  void paint(Painter& p) const override;

 private:
  float headerHeight() const;
  Rect tabRect(int i) const;
  std::vector<std::string> titles_;
  std::vector<cWidget*> pages_;
  int current_ = -1;
};

// -------------------------------------------------------------- controls --
class cLabel : public cWidget {
 public:
  explicit cLabel(std::string text = {}, std::string styleClass = "label");
  const std::string& text() const { return text_; }
  cLabel& setText(std::string t);
  cLabel& setColor(Color c) {
    if (hasColor_ && color_.r == c.r && color_.g == c.g && color_.b == c.b && color_.a == c.a) return *this;
    color_ = c;
    hasColor_ = true;
    touchPaint();
    return *this;
  }
  cLabel& setCentered(bool c) { centered_ = c; return *this; }
  cLabel& setShadow(bool s) { shadow_ = s; return *this; }
  Vec2 measure(const Theme& theme) const override;
  lithogui::Widget* hitTest(float x, float y);

 protected:
  void paint(Painter& p) const override;

 private:
  std::string text_;
  Color color_{};
  bool hasColor_ = false;
  bool centered_ = false;
  bool shadow_ = false;
};

class cButton : public cWidget {
 public:
  explicit cButton(std::string text = {}, std::string styleClass = "button");
  const std::string& text() const { return label_; }
  cButton& setText(std::string t) { label_ = std::move(t); return *this; }
  bool focusable() const override { return true; }
  Vec2 measure(const Theme& theme) const override;
  bool handlePointer(const lithogui::PointerEvent& e) override;
  bool handleKey(const lithogui::KeyEvent& e) override;

 protected:
  void paint(Painter& p) const override;

 private:
  std::string label_;
};

/// A named image (block / item icon) resolved by the renderer's image seam.
class Image : public cWidget {
 public:
  explicit Image(std::string image = {}, Vec2 size = {32, 32}) : cWidget({}, "image"), size_(size), image_(std::move(image)) {}
  const std::string& image() const { return image_; }
  Image& setImage(std::string name) { image_ = std::move(name); return *this; }
  Image& setTint(Color c) { tint_ = c; return *this; }
  Vec2 measure(const Theme&) const override { return size_; }

 protected:
  void paint(Painter& p) const override;

 private:
  Vec2 size_;
  std::string image_;
  Color tint_{1, 1, 1, 1};
};

class cCheckbox : public cWidget {
 public:
  explicit cCheckbox(std::string text = {}, bool checked = false, bool toggle = false);
  const std::string& text() const { return label_; }
  bool checked() const { return checked_; }
  /// toggle-mode: the whole row is the switch (old cCheckbox(toggle=true));
  /// in facade terms the state lives here either way.
  void setChecked(bool c, bool notify = false);
  bool focusable() const override { return true; }
  Vec2 measure(const Theme& theme) const override;
  bool handlePointer(const lithogui::PointerEvent& e) override;
  bool handleKey(const lithogui::KeyEvent& e) override;

 protected:
  void paint(Painter& p) const override;

 private:
  std::string label_;
  bool checked_ = false;
  bool toggle_ = false;
};

class cProgressBar : public cWidget {
 public:
  cProgressBar() : cWidget({}, "progress") {}
  float value() const { return value_; }
  cProgressBar& setValue(float v) {
    const float nv = std::clamp(v, 0.f, 1.f);
    if (nv == value_) return *this;  // unchanged fill dirties nothing (JD paint ruling, 2026-11-09)
    value_ = nv;
    touchPaint();
    return *this;
  }
  cProgressBar& setText(std::string t) {
    if (t == text_) return *this;
    text_ = std::move(t);
    touchPaint();
    return *this;
  }
  /// Per-instance fill colour (the Agent bars each carry their own:
  /// Health red, Kharge blue, Static gold); without it the theme
  /// accent paints, as before.
  cProgressBar& setFillColor(Color c) { fillOverride_ = c; hasFill_ = true; return *this; }
  Vec2 measure(const Theme& theme) const override;

 protected:
  void paint(Painter& p) const override;

 private:
  float value_ = 0;
  std::string text_;
  Color fillOverride_{};
  bool hasFill_ = false;
};

class cSlider : public cWidget {
 public:
  cSlider(float min = 0, float max = 1, float value = 0, float step = 0);
  float value() const { return value_; }
  void setValue(float v, bool notify = false);
  bool focusable() const override { return true; }
  Vec2 measure(const Theme& theme) const override;
  bool handlePointer(const lithogui::PointerEvent& e) override;
  bool handleKey(const lithogui::KeyEvent& e) override;

 protected:
  void paint(Painter& p) const override;

 private:
  void setFromX(float x);
  float min_, max_, value_, step_;
  bool dragging_ = false;
};

/// Integer field with - / + buttons: typing digits, Up / Down, wheel.
class cNumberField : public cWidget {
 public:
  cNumberField(int min = 0, int max = 64, int value = 0);
  int value() const { return value_; }
  void setValue(int v, bool notify = false);
  int min() const { return min_; }
  int max() const { return max_; }
  bool focusable() const override { return true; }
  Vec2 measure(const Theme& theme) const override;
  bool handlePointer(const lithogui::PointerEvent& e) override;
  bool handleKey(const lithogui::KeyEvent& e) override;

 protected:
  void paint(Painter& p) const override;

 private:
  Rect minusRect() const;
  Rect plusRect() const;
  void commitTyping();
  int min_, max_, value_;
  std::string typing_;
};

/// Inventory / hotbar cell: an icon (named image), caption, selection frame.
class cSlot : public cWidget {
 public:
  explicit cSlot(Vec2 size = {40, 40}) : cWidget({}, "slot"), size_(size) {}
  cSlot& setIcon(std::string image, Color tint = Color{1, 1, 1, 1}) {
    if (image == icon_ && tint.r == tint_.r && tint.g == tint_.g && tint.b == tint_.b && tint.a == tint_.a) return *this;
    icon_ = std::move(image);
    tint_ = tint;
    touchPaint();
    return *this;
  }
  const std::string& icon() const { return icon_; }
  cSlot& setCaption(std::string c) {
    if (c == caption_) return *this;
    caption_ = std::move(c);
    touchPaint();
    return *this;
  }
  const std::string& caption() const { return caption_; }
  cSlot& setSelected(bool s) {
    if (s == selected_) return *this;
    selected_ = s;
    touchPaint();
    return *this;
  }
  bool selected() const { return selected_; }
  cSlot& setScale(float k) { scale_ = k; return *this; }
  /// Cursor-stack plan (2026-11-11, Cinnabar press timing): OPT-IN
  /// press-edge activation. A press-activated slot emits "press" on the
  /// left PointerDown itself and swallows the matching release (no
  /// "click" follows), so one physical click dispatches exactly once,
  /// on the press edge. The default stays the shared release-click
  /// contract (arm on Down, "click" on the matching Up inside the
  /// rect) that menus, the HUD hotbar and bug-hunt #37 rely on; only
  /// the creative screen's slots (carry grid + palette) opt in.
  /// Right-click handling is unchanged either way. Keyboard Enter /
  /// Space activation emits "press" instead of "click" when set.
  cSlot& setPressActivated(bool on) { pressActivated_ = on; return *this; }
  bool pressActivated() const { return pressActivated_; }
  Vec2 measure(const Theme&) const override { return size_; }
  bool handlePointer(const lithogui::PointerEvent& e) override;
  bool handleKey(const lithogui::KeyEvent& e) override;

 protected:
  void paint(Painter& p) const override;

 private:
  Vec2 size_;
  std::string icon_, caption_;
  Color tint_{1, 1, 1, 1};
  bool selected_ = false;
  float scale_ = 1.f;
  bool pressActivated_ = false;  // opt-in press-edge dispatch (see setPressActivated)
  int armedButton_ = -1;  // which button's Down armed WS_PRESSED (-1 = none; bug hunt #37)
};

class cDivider : public cWidget {
 public:
  explicit cDivider(Orientation o = Orientation::Horizontal, float thickness = 1.f)
      : cWidget({}, "divider"), orientation_(o), thickness_(thickness < 1.f ? 1.f : thickness) {}
  Orientation orientation() const { return orientation_; }
  cDivider& setThickness(float t) { thickness_ = t < 1.f ? 1.f : t; return *this; }
  float thickness() const { return thickness_; }
  Vec2 measure(const Theme& theme) const override;
  lithogui::Widget* hitTest(float, float) { return nullptr; }

 protected:
  void paint(Painter& p) const override;

 private:
  Orientation orientation_;
  float thickness_;
};

// ---------------------------------------------------------------- lists ---
class cListView : public cWidget {
 public:
  cListView() : cWidget({}, "listview") {}
  void setItems(std::vector<std::string> items);
  const std::vector<std::string>& items() const { return items_; }
  int selected() const { return selected_; }
  void setSelected(int index, bool notify = false);
  const std::string* selectedItem() const;
  float rowHeight() const;
  Vec2 measure(const Theme& theme) const override;
  bool handlePointer(const lithogui::PointerEvent& e) override;
  bool handleKey(const lithogui::KeyEvent& e) override;

 protected:
  void paint(Painter& p) const override;
  void paintChildren(Painter& p) const override { (void)p; }  // rows are painted, not children

 private:
  int rowAt(Vec2 p) const;
  void scrollTo(int row);
  std::vector<std::string> items_;
  int selected_ = -1;
  float scrollY_ = 0;
};

struct TreeNode {
  std::string label;
  bool expanded = true;
  std::vector<TreeNode> children;
};

class cTreeView : public cWidget {
 public:
  cTreeView() : cWidget({}, "treeview") {}
  std::vector<TreeNode>& roots() { return roots_; }
  void setRoots(std::vector<TreeNode> roots) { roots_ = std::move(roots); path_.clear(); }
  const TreeNode* selected() const;
  const std::vector<int>& selectedPath() const { return path_; }
  void selectPath(std::vector<int> path, bool notify = false);
  int visibleRowCount() const { return int(rows().size()); }
  Vec2 measure(const Theme& theme) const override;
  bool handlePointer(const lithogui::PointerEvent& e) override;
  bool handleKey(const lithogui::KeyEvent& e) override;
  struct Row {
    const TreeNode* node = nullptr;
    int depth = 0;
    std::vector<int> path;
  };
  std::vector<Row> rows() const;
  TreeNode* nodeAt(const std::vector<int>& path);
  int selectedRow(const std::vector<Row>& rows) const;
  float rowHeight() const;

 protected:
  void paint(Painter& p) const override;
  void paintChildren(Painter& p) const override { (void)p; }

 private:
  std::vector<TreeNode> roots_;
  std::vector<int> path_;
  float scrollY_ = 0;
};

class cDropdown : public cWidget {
 public:
  explicit cDropdown(std::vector<std::string> items = {}, int selected = 0);
  void setItems(std::vector<std::string> items);
  const std::vector<std::string>& items() const { return items_; }
  int selected() const { return selected_; }
  void setSelected(int index, bool notify = false);
  bool isOpen() const { return menu_ && menu_->isOpen(); }
  void open();
  void closeMenu();
  /// Through cWidget::attachContext, remember our context so open() can
  /// attach the menu popup; the UiConnection binds this per showing.
  void attachContext(lithogui::UiContext* ctx) { ctx_ = ctx; }
  Vec2 measure(const Theme& theme) const override;
  bool handlePointer(const lithogui::PointerEvent& e) override;
  bool handleKey(const lithogui::KeyEvent& e) override;

 protected:
  void paint(Painter& p) const override;
  void detachContext(lithogui::UiContext* ctx) override;

 private:
  std::vector<std::string> items_;
  int selected_ = 0;
  std::unique_ptr<lithogui::Popup> menuOwner_;
  lithogui::Panel* menuPanel_ = nullptr;
  lithogui::ListBox* menuList_ = nullptr;
  lithogui::Popup* menu_ = nullptr;
  lithogui::UiContext* ctx_ = nullptr;
};

// ---------------------------------------------------------------- menus ---
struct MenuItem {
  std::string label, shortcut;
  std::function<void()> action;
  bool separator = false;
  static MenuItem separatorItem() { MenuItem m; m.separator = true; return m; }
};

/// A simple menu popup (list of rows) hosted on the context popup stack.
class cMenu : public cWidget {
 public:
  explicit cMenu(std::vector<MenuItem> items);
  static cMenu& open(cUiRoot& ui, std::vector<MenuItem> items, Vec2 pos);
  const std::vector<MenuItem>& items() const { return items_; }
  int highlighted() const { return highlight_; }
  bool activate(int index);
  Vec2 measure(const Theme& theme) const override;
  bool handlePointer(const lithogui::PointerEvent& e) override;
  bool handleKey(const lithogui::KeyEvent& e) override;
  std::function<void()> onDone;
  std::function<void(int dir)> onNavigate;
  float rowHeight() const;
  Rect rowRect(int i) const;
  int rowAt(Vec2 p) const;
  void openSubmenu(int) {}
  void step(int dir);

 protected:
  void paint(Painter& p) const override;

 private:
  std::vector<MenuItem> items_;
  int highlight_ = -1;
  std::unique_ptr<lithogui::Popup> hostOwner_;  // set by open(): the popup hosting this menu
};

struct ContextMenu {
  static cMenu& open(cUiRoot& ui, Vec2 pointer, std::vector<MenuItem> items) {
    return cMenu::open(ui, std::move(items), pointer);
  }
};

class cMenuBar : public cWidget {
 public:
  cMenuBar() : cWidget({}, "menubar") {}
  void addMenu(std::string title, std::vector<MenuItem> items);
  int count() const { return int(menus_.size()); }
  int openIndex() const;
  void openMenu(int index);
  void closeMenu();
  Vec2 measure(const Theme& theme) const override;
  bool handlePointer(const lithogui::PointerEvent& e) override;
  bool handleKey(const lithogui::KeyEvent& e) override;

 protected:
  void paint(Painter& p) const override;

 private:
  Rect titleRect(int i) const;
  int titleAt(Vec2 p) const;
  struct Entry { std::string title; std::vector<MenuItem> items; };
  std::vector<Entry> menus_;
  int open_ = -1;
  lithogui::Popup* menuPopup_ = nullptr;
};

// --------------------------------------------------------------- dialog ---
/// Modal dialog: a facade widget hosted by a DialogHost (a lithogui Modal
/// popup). The context's popup stack owns input routing and close semantics
/// for the host; the cUiRoot keeps the owning unique_ptr while it is open.
/// Escape or close() emits "close" once (the context closes the host on
/// Escape; the host's onClose hook routes that back here). Dialogs size to
/// their measured body, capped to the viewport, and centre themselves.
class cDialog : public cWidget {
 public:
  explicit cDialog(std::string title, Vec2 size = {});
  static cDialog& show(cUiRoot& ui, std::string title, Vec2 size = {});
  const std::string& title() const { return title_; }
  cStackPanel& body() { return *body_; }
  const cStackPanel& body() const { return *body_; }
  cButton& addButton(std::string text, std::function<void()> onClick = {}, bool closes = true);
  void close();
  void setCloseGuard(std::function<bool()> guard) { closeGuard_ = std::move(guard); }
  Vec2 measure(const Theme& theme) const override;
  bool handleKey(const lithogui::KeyEvent& e) override;
  bool handlePointer(const lithogui::PointerEvent& e) override;
  lithogui::Popup* asPopup() override { return host_; }
  void noteProgrammaticClose() override { suppressEmit_ = true; }
  /// The close guard, asked before a context Escape close: a veto runs
  /// the guard's own flow (e.g. the editor's discard confirmation).
  bool vetoPopupClose() override { return closeGuard_ && !closeGuard_(); }
  /// The host popup closed (any path): fire the user-close side effect
  /// unless this close was programmatic or already fired.
  void notifyHostClosed();

 protected:
  void arrange(const Theme& theme) override;
  void paint(Painter& p) const override;

 private:
  float titleHeight() const;
  std::string title_;
  cStackPanel* body_ = nullptr;
  cWidget* buttons_ = nullptr;
  Vec2 size_{};
  std::function<bool()> closeGuard_;
  bool closeFired_ = false;
  bool suppressEmit_ = false;
  Vec2 dragGrab_{-1, -1};
  lithogui::Popup* host_ = nullptr;
  friend struct DialogHost;
};

/// The popup that hosts a cDialog on the context stack (internal). Its
/// layout centres the dialog in the viewport; its draw emits the dialog's
/// facade paint through the active paint sink (see cUiRoot::paint).
struct DialogHost : lithogui::Popup {
  explicit DialogHost(std::string id) : lithogui::Popup(std::move(id), Flavor::Modal) {}
  cDialog* dialog = nullptr;
  void layout(lithogui::Rect r, const lithogui::Style& style = lithogui::defaultStyle()) override;
  void draw(lithogui::DrawList& dl, const lithogui::DrawParams& params) const override;
};

}  // namespace ui
}  // namespace lithokraft
