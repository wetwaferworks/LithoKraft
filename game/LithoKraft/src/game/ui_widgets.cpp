// ui_widgets.cpp: the concrete facade widgets (containers, controls, lists,
// menus, dialog) over Litho GUI primitives. Paint goes through Painter into
// the frame's DrawList; interaction arrives as lithogui events dispatched by
// cUiRoot and is re-expressed on the old "click"/"change"/"select" handler
// bus. See ui_facade.h for the architecture.
#include "lithokraft/game/ui_widgets.h"
#include <cmath>

namespace lithokraft {
namespace ui {
namespace {

constexpr float kGrip = 14.f;  // dialog title-bar drag handle

bool inside(const Rect& r, float x, float y) { return r.contains({x, y}); }

void splitLines(const std::string& text, std::vector<std::string>& out) {
  out.clear();
  std::string cur;
  for (char c : text) {
    if (c == '\n') {
      out.push_back(cur);
      cur.clear();
    } else cur += c;
  }
  out.push_back(cur);
}

}  // namespace

// ------------------------------------------------------------- containers --
bool cPanel::handlePointer(const lithogui::PointerEvent& e) {
  // Press tap (see ui_widgets.h): bubblePointer reaches a panel only
  // after every widget under the press declined the event, so an
  // armed panel hears exactly the background presses. Emit and stay
  // unconsumed: the tap observes, it does not capture.
  if (pressTap_ && enabled() && e.type == lithogui::PointerEvent::Type::Down && e.button == 0) emit("press");
  return false;
}

void cPanel::paint(Painter& p) const {
  const Style& st = style();
  const Color bg = hasBg_ ? bgOverride_ : st.background;
  if (bg.a > 0) p.fill(fromGui(rect()), bg);
  if (hasBorder_) {
    if (borderOverride_.a > 0) p.frame(fromGui(rect()), borderOverride_, borderW_);
  } else if (st.borderWidth > 0 && st.border.a > 0) {
    p.frame(fromGui(rect()), st.border, st.borderWidth);
  }
}

void cFrame::arrange(const Theme& theme) {
  const Rect content = fromGui(rect()).inset(Edges::all(pad_));
  bool first = true;
  for (auto& c : children()) {
    auto* cw = dynamic_cast<cWidget*>(c.get());
    if (!cw) continue;
    if (!cw->visible() || !first) {
      cw->place({content.x, content.y, 0, 0}, theme);
      continue;
    }
    first = false;
    cw->place(content, theme);
  }
}

void cStackPanel::syncNative() {
  cWidget::syncNative();
  dir = orientation_ == Orientation::Vertical ? lithogui::LayoutDir::Column : lithogui::LayoutDir::Row;
}

Vec2 cStackPanel::measure(const Theme& theme) const {
  Vec2 out{0, 0};
  bool first = true;
  for (auto& c : children()) {
    const auto* cw = dynamic_cast<const cWidget*>(c.get());
    if (!cw || !cw->visible()) continue;
    const Vec2 cs = childSize(*cw, theme);
    if (!first) {
      if (orientation_ == Orientation::Vertical) out.y += spacing_;
      else out.x += spacing_;
    }
    first = false;
    if (orientation_ == Orientation::Vertical) {
      out.x = std::max(out.x, cs.x);
      out.y += cs.y;
    } else {
      out.x += cs.x;
      out.y = std::max(out.y, cs.y);
    }
  }
  const Edges pad = style().padding;
  return {out.x + pad.horizontal(), out.y + pad.vertical()};
}

void cStackPanel::arrange(const Theme& theme) {
  const Rect content = fromGui(rect()).inset(style().padding);
  const bool vert = orientation_ == Orientation::Vertical;
  float fixedTotal = 0, flexTotal = 0;
  for (auto& c : children()) {
    const auto* cw = dynamic_cast<const cWidget*>(c.get());
    if (!cw || !cw->visible()) continue;
    const Vec2 cs = childSize(*cw, theme);
    if (cw->layout().flex > 0) flexTotal += cw->layout().flex;
    else fixedTotal += vert ? cs.y : cs.x;
  }
  int visibleCount = 0;
  for (auto& c : children()) {
    const auto* cw = dynamic_cast<const cWidget*>(c.get());
    if (cw && cw->visible()) ++visibleCount;
  }
  fixedTotal += spacing_ * float(std::max(0, visibleCount - 1));
  const float main = vert ? content.h : content.w;
  const float leftover = std::max(0.f, main - fixedTotal);
  float cursor = vert ? content.y : content.x;
  bool first = true;
  for (auto& c : children()) {
    auto* cw = dynamic_cast<cWidget*>(c.get());
    if (!cw) continue;
    if (!cw->visible()) {
      cw->place({content.x, content.y, 0, 0}, theme);
      continue;
    }
    if (!first) cursor += spacing_;
    first = false;
    const Vec2 cs = childSize(*cw, theme);
    float m = vert ? cs.y : cs.x;
    if (cw->layout().flex > 0 && flexTotal > 0) m = leftover * (cw->layout().flex / flexTotal);
    const float cross = stretch_ ? (vert ? content.w : content.h) : (vert ? cs.x : cs.y);
    const Rect r = vert ? Rect{content.x, cursor, cross, m} : Rect{cursor, content.y, m, cross};
    cw->place(r, theme);
    cursor += m;
  }
}

Vec2 cGridPanel::measure(const Theme& theme) const {
  (void)theme;
  int n = 0;
  for (auto& c : children())
    if (c->visible()) ++n;
  const int rows = (n + columns_ - 1) / columns_;
  const Edges pad = style().padding;
  const Vec2 cell = (fit_ || fitBox_) && solvedCell_ > 0 ? Vec2{solvedCell_, solvedCell_} : cell_;
  return {float(columns_) * cell.x + float(std::max(0, columns_ - 1)) * spacing_ + pad.horizontal(),
          float(rows) * cell.y + float(std::max(0, rows - 1)) * spacing_ + pad.vertical()};
}

void cGridPanel::arrange(const Theme& theme) {
  const Rect content = fromGui(rect()).inset(style().padding);
  Vec2 cell = cell_;
  float ox = 0, oy = 0;  // C9 fit-box: centre horizontally, bottom-align
  if (fitBox_) {
    int n = 0;
    for (auto& c : children())
      if (c->visible()) ++n;
    const int rows = std::max(1, (n + columns_ - 1) / columns_);
    const float cw = (content.w - float(std::max(0, columns_ - 1)) * spacing_) / float(columns_);
    const float ch = (content.h - float(rows - 1) * spacing_) / float(rows);
    const float c = std::max(1.f, std::floor(std::min(cw, ch)));
    cell = {c, c};
    solvedCell_ = c;
    const float usedW = float(columns_) * c + float(std::max(0, columns_ - 1)) * spacing_;
    const float usedH = float(rows) * c + float(rows - 1) * spacing_;
    ox = std::max(0.f, (content.w - usedW) * 0.5f);
    oy = std::max(0.f, content.h - usedH);
  } else if (fit_) {
    const float cw = (content.w - float(std::max(0, columns_ - 1)) * spacing_) / float(columns_);
    cell = {std::max(1.f, cw), std::max(1.f, cw)};
    solvedCell_ = cell.x;
  }
  int i = 0;
  for (auto& c : children()) {
    auto* cw = dynamic_cast<cWidget*>(c.get());
    if (!cw) continue;
    if (!cw->visible()) {
      cw->place({content.x, content.y, 0, 0}, theme);
      continue;
    }
    const int col = i % columns_, row = i / columns_;
    cw->place({content.x + ox + float(col) * (cell.x + spacing_), content.y + oy + float(row) * (cell.y + spacing_), cell.x, cell.y}, theme);
    ++i;
  }
}

void cScrollBar::paint(Painter& p) const {
  const Rect r = fromGui(rect());
  const Style& st = style();
  if (!needed()) return;
  p.fill(r, {0, 0, 0, 0.35f});
  const float frac = view_ / content_;
  const float span = orientation_ == Orientation::Vertical ? r.h : r.w;
  const float thumbLen = std::max(12.f, span * frac);
  const float travel = std::max(0.f, span - thumbLen);
  const float t = maxOffset() > 0 ? offset_ / maxOffset() : 0;
  const Rect thumb = orientation_ == Orientation::Vertical
                         ? Rect{r.x, r.y + travel * t, r.w, thumbLen}
                         : Rect{r.x + travel * t, r.y, thumbLen, r.h};
  p.fill(thumb, st.accent);
}

cWidget& cScrollView::setContent(std::unique_ptr<lithogui::Widget> content) {
  clearChildren();
  content_ = nullptr;
  bar_ = nullptr;
  // Capture the raw child BEFORE addChild (bug hunt #10): addChild
  // returns *this for a foreign (non-cWidget) child, so casting its
  // return aliased content_ to the scroll view itself and measure() /
  // arrange() recursed on it forever. A foreign child is rejected
  // (dropped, content_ stays null) instead of aliased.
  lithogui::Widget* raw = content.get();
  cWidget& ref = addChild(std::move(content));
  content_ = dynamic_cast<cWidget*>(raw);
  auto bar = std::make_unique<cScrollBar>(Orientation::Vertical);
  lithogui::Widget* braw = bar.get();
  addChild(std::move(bar));
  bar_ = static_cast<cScrollBar*>(braw);
  return ref;
}

Vec2 cScrollView::measure(const Theme& theme) const {
  const Edges pad = style().padding;
  Vec2 c = content_ ? content_->measure(theme) : Vec2{};
  if (layout().size.x > 0) c.x = layout().size.x;
  if (layout().size.y > 0) c.y = layout().size.y;
  return {c.x + pad.horizontal(), c.y + pad.vertical()};
}

void cScrollView::arrange(const Theme& theme) {
  const Rect contentBox = fromGui(rect()).inset(style().padding);
  if (!content_ || !bar_) return;
  const Vec2 cs = content_->measure(theme);
  const bool needBar = cs.y > contentBox.h + 0.5f;
  const float barW = needBar ? cScrollBar::kThickness : 0;
  bar_->setRange(cs.y, contentBox.h);
  float off = bar_->offset();
  content_->place({contentBox.x, contentBox.y - off, contentBox.w - barW, cs.y}, theme);
  bar_->place(needBar ? Rect{contentBox.right() - barW, contentBox.y, barW, contentBox.h}
                      : Rect{contentBox.x, contentBox.y, 0, 0},
              theme);
}

bool cScrollView::handlePointer(const lithogui::PointerEvent& e) {
  if (e.type == lithogui::PointerEvent::Type::Wheel && bar_) {
    const float before = bar_->offset();
    bar_->setOffset(before - e.wheelDelta * 24.f);
    if (bar_->offset() != before) relayout();
    return true;
  }
  return false;
}

void cScrollView::paintChildren(Painter& p) const {
  if (!content_) return;
  const Rect clip = fromGui(rect()).inset(style().padding);
  Painter::ClipScope cs(p, clip);
  if (content_->visible()) content_->paintTreeLayer(p, false);  // scroll content is main-layer by contract
  if (bar_ && bar_->visible()) bar_->paintTree(p);
}

cWidget& cSplitter::setPane(int index, std::unique_ptr<lithogui::Widget> pane) {
  if (index < 0 || index > 1) return *this;
  if (panes_[index]) removeChild(*panes_[index]);
  // Raw child captured before addChild (bug hunt #10): addChild returns
  // *this for a foreign child; aliasing it as the pane recursed layout.
  lithogui::Widget* raw = pane.get();
  cWidget& ref = addChild(std::move(pane));
  panes_[index] = dynamic_cast<cWidget*>(raw);
  return ref;
}

Vec2 cSplitter::measure(const Theme& theme) const {
  if (layout().size.x > 0 || layout().size.y > 0) return layout().size;
  Vec2 a = panes_[0] ? panes_[0]->measure(theme) : Vec2{};
  Vec2 b = panes_[1] ? panes_[1]->measure(theme) : Vec2{};
  if (orientation_ == Orientation::Horizontal) return {a.x + b.x + kHandle, std::max(a.y, b.y)};
  return {std::max(a.x, b.x), a.y + b.y + kHandle};
}

void cSplitter::arrange(const Theme& theme) {
  const Rect box = fromGui(rect()).inset(style().padding);
  const bool horiz = orientation_ == Orientation::Horizontal;
  const float main = horiz ? box.w : box.h;
  const float first = std::max(0.f, (main - kHandle) * ratio_);
  const Rect r0 = horiz ? Rect{box.x, box.y, first, box.h} : Rect{box.x, box.y, box.w, first};
  const Rect r1 = horiz ? Rect{box.x + first + kHandle, box.y, std::max(0.f, main - first - kHandle), box.h}
                        : Rect{box.x, box.y + first + kHandle, box.w, std::max(0.f, main - first - kHandle)};
  if (panes_[0]) panes_[0]->place(panes_[0]->visible() ? r0 : Rect{box.x, box.y, 0, 0}, theme);
  if (panes_[1]) panes_[1]->place(panes_[1]->visible() ? r1 : Rect{box.x, box.y, 0, 0}, theme);
}

bool cSplitter::handlePointer(const lithogui::PointerEvent& e) {
  const Rect box = fromGui(rect());
  const bool horiz = orientation_ == Orientation::Horizontal;
  const float main = horiz ? box.w : box.h;
  const float first = std::max(0.f, (main - kHandle) * ratio_);
  const float handlePos = (horiz ? box.x : box.y) + first;
  const float along = horiz ? e.x : e.y;
  if (e.type == lithogui::PointerEvent::Type::Down && e.button == 0 && along >= handlePos && along < handlePos + kHandle) {
    dragging_ = true;
    return true;
  }
  if (dragging_ && e.type == lithogui::PointerEvent::Type::Move) {
    setRatio(main > kHandle ? (along - (horiz ? box.x : box.y)) / (main - kHandle) : ratio_);
    relayout();
    return true;
  }
  if (dragging_ && e.type == lithogui::PointerEvent::Type::Up) {
    dragging_ = false;
    return true;
  }
  return false;
}

cWidget& cTabView::addTab(std::string title, std::unique_ptr<lithogui::Widget> page) {
  // Raw child captured before addChild (bug hunt #10): addChild returns
  // *this for a foreign child; aliasing it as the page recursed layout.
  lithogui::Widget* raw = page.get();
  cWidget& ref = addChild(std::move(page));
  titles_.push_back(std::move(title));
  pages_.push_back(dynamic_cast<cWidget*>(raw));
  if (current_ < 0) {
    current_ = 0;
  }
  if (pages_.back()) pages_.back()->setVisible(int(pages_.size()) - 1 == current_);
  return ref;
}

void cTabView::setCurrent(int index, bool notify) {
  if (index < 0 || index >= count() || index == current_) return;
  current_ = index;
  for (size_t i = 0; i < pages_.size(); ++i)
    if (pages_[i]) pages_[i]->setVisible(int(i) == current_);
  relayout();
  if (notify) emit("change");
}

float cTabView::headerHeight() const { return theme().glyph().y + 10.f; }

Rect cTabView::tabRect(int i) const {
  const Rect r = fromGui(rect());
  const Theme& t = theme();
  float x = r.x;
  for (int j = 0; j < i; ++j) x += t.textWidth(titles_[size_t(j)]) + 24.f;
  return {x, r.y, t.textWidth(titles_[size_t(i)]) + 24.f, headerHeight()};
}

Vec2 cTabView::measure(const Theme& theme) const {
  Vec2 out{0, headerHeight()};
  for (size_t i = 0; i < pages_.size(); ++i) {
    if (!pages_[i]) continue;
    const Vec2 ps = pages_[i]->measure(theme);
    out.x = std::max(out.x, ps.x);
    out.y = std::max(out.y, headerHeight() + ps.y);
  }
  float strip = 0;
  for (const auto& title : titles_) strip += theme.textWidth(title) + 24.f;
  out.x = std::max(out.x, strip);
  if (layout().size.x > 0) out.x = layout().size.x;
  if (layout().size.y > 0) out.y = layout().size.y;
  return out;
}

void cTabView::arrange(const Theme& theme) {
  const Rect r = fromGui(rect());
  const Rect pageBox{r.x, r.y + headerHeight(), r.w, std::max(0.f, r.h - headerHeight())};
  for (size_t i = 0; i < pages_.size(); ++i) {
    if (!pages_[i]) continue;
    pages_[i]->place(int(i) == current_ ? pageBox : Rect{r.x, r.y, 0, 0}, theme);
  }
}

bool cTabView::handlePointer(const lithogui::PointerEvent& e) {
  if (e.type == lithogui::PointerEvent::Type::Down && e.button == 0)
    for (int i = 0; i < count(); ++i)
      if (inside(tabRect(i), e.x, e.y)) {
        setCurrent(i, true);
        return true;
      }
  return false;
}

bool cTabView::handleKey(const lithogui::KeyEvent& e) {
  if (!e.down) return false;
  if (e.key == lithogui::KEY_LEFT) { setCurrent(std::max(0, current_ - 1), true); return true; }
  if (e.key == lithogui::KEY_RIGHT) { setCurrent(std::min(count() - 1, current_ + 1), true); return true; }
  return false;
}

void cTabView::paint(Painter& p) const {
  const Style& st = style();
  if (st.background.a > 0) p.fill(fromGui(rect()), st.background);
  const Theme& t = theme();
  for (int i = 0; i < count(); ++i) {
    const Rect tr = tabRect(i);
    p.fill(tr, i == current_ ? st.accent : st.background);
    if (st.borderWidth > 0 && st.border.a > 0) p.frame(tr, st.border, st.borderWidth);
    p.textIn(tr, titles_[size_t(i)], st.text, true);
    (void)t;
  }
}

// -------------------------------------------------------------- controls --
cLabel::cLabel(std::string text, std::string styleClass)
    : cWidget({}, std::move(styleClass)), text_(std::move(text)) {}

cLabel& cLabel::setText(std::string t) {
  // WS4 (perf diagnosis): cGameUi::update re-sets every HUD string every
  // frame; an unchanged string must cost nothing and must not move the
  // paint generation, or the change gate never engages.
  if (t == text_) return *this;
  text_ = std::move(t);
  touchPaint();
  if (uiRoot()) uiRoot()->noteTextBump();  // next-build-2 attribution: live text is the gate's main defeater
  return *this;
}

Vec2 cLabel::measure(const Theme& theme) const {
  std::vector<std::string> lines;
  splitLines(text_, lines);
  float w = 0;
  for (const auto& ln : lines) w = std::max(w, theme.textWidth(ln, style().textScale));
  const Edges pad = style().padding;
  return {w + pad.horizontal(), float(lines.size()) * theme.glyphSize(style().textScale).y + pad.vertical()};
}

void cLabel::paint(Painter& p) const {
  const Style& st = style();
  const Rect r = fromGui(rect());
  if (st.background.a > 0) p.fill(r, st.background);
  if (st.borderWidth > 0 && st.border.a > 0) p.frame(r, st.border, st.borderWidth);
  const Color c = hasColor_ ? color_ : st.text;
  const float ts = st.textScale;
  const Vec2 g = theme().glyphSize(ts);
  const Rect inner = r.inset(st.padding);
  std::vector<std::string> lines;
  splitLines(text_, lines);
  float y = inner.y;
  for (const auto& ln : lines) {
    const float w = theme().textWidth(ln, ts);
    const float x = centered_ ? inner.x + std::max(0.f, (inner.w - w) * 0.5f) : inner.x;
    // 2026-11-08: no facade-side shadow pass anymore. The renderer
    // (buildHudVertices) shadows EVERY GUI text draw now (JD's ruling:
    // any text gets a slight drop shadow), so the old per-label double
    // draw would double-shadow the two name labels that used it. The
    // shadow_ flag stays as API surface; it no longer emits geometry.
    p.text({x, y}, ln, c, ts);
    y += g.y;
  }
}

lithogui::Widget* cLabel::hitTest(float x, float y) {
  // Like the old cLabel: only hittable when it carries a tooltip.
  if (!tooltip().empty() && visible() && fromGui(rect()).contains({x, y})) return this;
  return nullptr;
}

cButton::cButton(std::string text, std::string styleClass)
    : cWidget({}, std::move(styleClass)), label_(std::move(text)) {}

Vec2 cButton::measure(const Theme& theme) const {
  const Edges pad = style().padding;
  return {theme.textWidth(label_, style().textScale) + pad.horizontal() + 16.f,
          theme.glyphSize(style().textScale).y + pad.vertical() + 8.f};
}

bool cButton::handlePointer(const lithogui::PointerEvent& e) {
  if (e.button != 0 || !enabled()) return false;
  // Arm WS_PRESSED on Down OURSELVES (2026-11-07, menu-click fix): the old
  // comment claimed the context capture arms it, but UiContext only tracks
  // the capture POINTER; the native lithogui::Button raises WS_PRESSED in
  // its own handler. Without this raise the Up saw hasState(WS_PRESSED)
  // false forever and no facade button ever fired its click.
  if (e.type == lithogui::PointerEvent::Type::Down) { raiseState(lithogui::WS_PRESSED); return true; }
  if (e.type == lithogui::PointerEvent::Type::Up) {
    const bool fire = hasState(lithogui::WS_PRESSED) && fromGui(rect()).contains({e.x, e.y});
    dropState(lithogui::WS_PRESSED);
    if (fire) emit("click");
    return true;
  }
  return false;
}

bool cButton::handleKey(const lithogui::KeyEvent& e) {
  if (e.down && enabled() && (e.key == lithogui::KEY_ENTER || e.ch == U' ')) {
    emit("click");
    return true;
  }
  return false;
}

void cButton::paint(Painter& p) const {
  const Style& st = style();
  const Rect r = fromGui(rect());
  const Color bg = stateBackground();
  if (bg.a > 0) p.fill(r, bg);
  const Color border = focused() ? st.accent : st.border;
  if (st.borderWidth > 0 && border.a > 0) p.frame(r, border, st.borderWidth);
  p.textIn(r, label_, enabled() ? st.text : st.textDisabled, true, st.textScale);
}

void Image::paint(Painter& p) const {
  if (!image_.empty()) p.image(fromGui(rect()), image_, tint_);
}

cCheckbox::cCheckbox(std::string text, bool checked, bool toggle)
    : cWidget({}, "checkbox"), label_(std::move(text)), checked_(checked), toggle_(toggle) {}

void cCheckbox::setChecked(bool c, bool notify) {
  if (c == checked_) {
    if (notify) emit("change");
    return;
  }
  checked_ = c;
  if (notify) emit("change");
}

Vec2 cCheckbox::measure(const Theme& theme) const {
  const Edges pad = style().padding;
  const float box = theme.glyph().y;
  return {box + 6.f + theme.textWidth(label_, style().textScale) + pad.horizontal(),
          theme.glyphSize(style().textScale).y + pad.vertical()};
}

bool cCheckbox::handlePointer(const lithogui::PointerEvent& e) {
  if (e.button != 0 || !enabled()) return false;
  if (e.type == lithogui::PointerEvent::Type::Down) { raiseState(lithogui::WS_PRESSED); return true; }  // arm own WS_PRESSED (2026-11-07, see cButton)
  if (e.type == lithogui::PointerEvent::Type::Up) {
    // Mirror cButton (bug hunt #36): toggle only when the release lands
    // inside the rect; the context routes the release to the captured
    // widget wherever the pointer is, so a press-drag-off-release must
    // not apply (and persist) the setting. Always clear WS_PRESSED.
    const bool fire = hasState(lithogui::WS_PRESSED) && fromGui(rect()).contains({e.x, e.y});
    dropState(lithogui::WS_PRESSED);
    if (fire) setChecked(!checked_, true);
    return true;
  }
  return false;
}

bool cCheckbox::handleKey(const lithogui::KeyEvent& e) {
  if (e.down && enabled() && (e.key == lithogui::KEY_ENTER || e.ch == U' ')) {
    setChecked(!checked_, true);
    return true;
  }
  return false;
}

void cCheckbox::paint(Painter& p) const {
  const Style& st = style();
  const Rect r = fromGui(rect());
  if (st.background.a > 0) p.fill(r, st.background);
  const float box = theme().glyph().y;
  const Rect br{r.x + st.padding.left, r.y + (r.h - box) * 0.5f, box, box};
  p.fill(br, st.background.a > 0 ? st.background : Color{0, 0, 0, 0.4f});
  if (st.border.a > 0) p.frame(br, focused() ? st.accent : st.border, std::max(1.f, st.borderWidth));
  if (checked_) {
    const Rect inner = br.inset(Edges::all(3.f));
    p.fill(inner, st.accent);
  }
  p.text({br.right() + 6.f, r.y + (r.h - theme().glyphSize(st.textScale).y) * 0.5f}, label_,
         enabled() ? st.text : st.textDisabled, st.textScale);
}

Vec2 cProgressBar::measure(const Theme& theme) const {
  const Edges pad = style().padding;
  return {200.f + pad.horizontal(), theme.glyphSize(style().textScale).y + pad.vertical()};
}

void cProgressBar::paint(Painter& p) const {
  const Style& st = style();
  const Rect r = fromGui(rect());
  if (st.background.a > 0) p.fill(r, st.background);
  if (st.borderWidth > 0 && st.border.a > 0) p.frame(r, st.border, st.borderWidth);
  const Rect inner = r.inset(st.padding);
  p.fill({inner.x, inner.y, inner.w * value_, inner.h}, hasFill_ ? fillOverride_ : st.accent);
  if (!text_.empty()) p.textIn(r, text_, st.text, true, st.textScale);
}

cSlider::cSlider(float min, float max, float value, float step)
    : cWidget({}, "slider"), min_(min), max_(max), step_(step) {
  value_ = std::clamp(value, min_, max_);
}

void cSlider::setValue(float v, bool notify) {
  float nv = std::clamp(v, min_, max_);
  if (step_ > 0) nv = min_ + std::round((nv - min_) / step_) * step_;
  nv = std::clamp(nv, min_, max_);
  const bool changed = nv != value_;
  value_ = nv;
  if (notify && changed) emit("change");
}

void cSlider::setFromX(float x) {
  const Rect r = fromGui(rect());
  const float t = r.w > 0 ? std::clamp((x - r.x) / r.w, 0.f, 1.f) : 0;
  setValue(min_ + t * (max_ - min_), true);
}

Vec2 cSlider::measure(const Theme& theme) const {
  const Edges pad = style().padding;
  return {160.f + pad.horizontal(), theme.glyphSize(style().textScale).y + pad.vertical() + 4.f};
}

bool cSlider::handlePointer(const lithogui::PointerEvent& e) {
  if (e.button != 0 || !enabled()) return false;
  if (e.type == lithogui::PointerEvent::Type::Down) {
    dragging_ = true;
    setFromX(e.x);
    return true;
  }
  if (dragging_ && e.type == lithogui::PointerEvent::Type::Move) {
    setFromX(e.x);
    return true;
  }
  if (dragging_ && e.type == lithogui::PointerEvent::Type::Up) {
    dragging_ = false;
    return true;
  }
  if (e.type == lithogui::PointerEvent::Type::Wheel) {
    setValue(value_ + (step_ > 0 ? step_ : (max_ - min_) * 0.05f) * e.wheelDelta, true);
    return true;
  }
  return false;
}

bool cSlider::handleKey(const lithogui::KeyEvent& e) {
  if (!e.down || !enabled()) return false;
  const float step = step_ > 0 ? step_ : (max_ - min_) * 0.05f;
  if (e.key == lithogui::KEY_LEFT || e.key == lithogui::KEY_DOWN) { setValue(value_ - step, true); return true; }
  if (e.key == lithogui::KEY_RIGHT || e.key == lithogui::KEY_UP) { setValue(value_ + step, true); return true; }
  return false;
}

void cSlider::paint(Painter& p) const {
  const Style& st = style();
  const Rect r = fromGui(rect());
  const Rect inner = r.inset(st.padding);
  const float midY = inner.y + inner.h * 0.5f;
  p.fill({inner.x, midY - 2.f, inner.w, 4.f}, st.border);
  const float t = max_ > min_ ? (value_ - min_) / (max_ - min_) : 0;
  p.fill({inner.x, midY - 2.f, inner.w * t, 4.f}, st.accent);
  const Rect thumb{inner.x + inner.w * t - 5.f, midY - 8.f, 10.f, 16.f};
  p.fill(thumb, enabled() ? st.accent : st.disabled);
  if (focused() && st.border.a > 0) p.frame(thumb, st.accent, 1.f);
}

cNumberField::cNumberField(int min, int max, int value) : cWidget({}, "number"), min_(min), max_(max) {
  value_ = std::clamp(value, min_, max_);
}

void cNumberField::setValue(int v, bool notify) {
  const int nv = std::clamp(v, min_, max_);
  const bool changed = nv != value_;
  value_ = nv;
  typing_.clear();
  if (notify && changed) emit("change");
}

Rect cNumberField::minusRect() const {
  const Rect r = fromGui(rect());
  const float h = theme().glyph().y + 4.f;
  return {r.x, r.y + (r.h - h) * 0.5f, 22.f, h};
}
Rect cNumberField::plusRect() const {
  const Rect r = fromGui(rect());
  const float h = theme().glyph().y + 4.f;
  return {r.right() - 22.f, r.y + (r.h - h) * 0.5f, 22.f, h};
}

void cNumberField::commitTyping() {
  if (typing_.empty()) return;
  int v = value_;
  try { v = std::stoi(typing_); } catch (...) { v = value_; }
  typing_.clear();
  setValue(v, true);
}

Vec2 cNumberField::measure(const Theme& theme) const {
  const Edges pad = style().padding;
  return {120.f + pad.horizontal(), theme.glyphSize(style().textScale).y + pad.vertical() + 4.f};
}

bool cNumberField::handlePointer(const lithogui::PointerEvent& e) {
  if (e.button != 0 || !enabled()) return false;
  if (e.type != lithogui::PointerEvent::Type::Down) return false;
  if (inside(minusRect(), e.x, e.y)) { commitTyping(); setValue(value_ - 1, true); return true; }
  if (inside(plusRect(), e.x, e.y)) { commitTyping(); setValue(value_ + 1, true); return true; }
  return true;  // take focus
}

bool cNumberField::handleKey(const lithogui::KeyEvent& e) {
  if (!e.down || !enabled()) return false;
  if (e.ch >= U'0' && e.ch <= U'9') {
    typing_ += char(e.ch);
    return true;
  }
  switch (e.key) {
    case lithogui::KEY_ENTER: commitTyping(); return true;
    case lithogui::KEY_BACKSPACE:
      if (!typing_.empty()) typing_.pop_back();
      else setValue(value_ - 1, true);
      return true;
    case lithogui::KEY_DELETE: typing_.clear(); return true;
    case lithogui::KEY_UP: commitTyping(); setValue(value_ + 1, true); return true;
    case lithogui::KEY_DOWN: commitTyping(); setValue(value_ - 1, true); return true;
    default: return false;
  }
}

void cNumberField::paint(Painter& p) const {
  const Style& st = style();
  const Rect r = fromGui(rect());
  if (st.background.a > 0) p.fill(r, st.background);
  if (st.borderWidth > 0 && (focused() ? st.accent : st.border).a > 0)
    p.frame(r, focused() ? st.accent : st.border, st.borderWidth);
  p.textIn(minusRect(), "-", st.text, true, st.textScale);
  p.textIn(plusRect(), "+", st.text, true, st.textScale);
  const std::string shown = typing_.empty() ? std::to_string(value_) : typing_;
  const Rect mid{minusRect().right() + 4.f, r.y, std::max(0.f, plusRect().x - minusRect().right() - 8.f), r.h};
  p.textIn(mid, shown, st.text, true, st.textScale);
}

bool cSlot::handlePointer(const lithogui::PointerEvent& e) {
  if (!enabled()) return false;
  // Press-activated slots (cursor-stack plan, 2026-11-11): the LEFT
  // button dispatches on the press edge. Down emits "press" at once;
  // the matching Up is consumed silently, so one physical click can
  // never dispatch twice (no press-fire plus release-fire). The press
  // target is the gesture's slot, full stop: a press on A released
  // over B fires A's press and nothing for B. Right-button events fall
  // through to the shared arming below, unchanged.
  if (pressActivated_ && e.button == 0) {
    if (e.type == lithogui::PointerEvent::Type::Down) {
      raiseState(lithogui::WS_PRESSED);
      armedButton_ = 0;
      emit("press");
      return true;
    }
    if (e.type == lithogui::PointerEvent::Type::Up) {
      dropState(lithogui::WS_PRESSED);
      armedButton_ = -1;
      return true;
    }
  }
  // Arm own WS_PRESSED on either button's Down (2026-11-07, see cButton)
  // and remember WHICH button armed it (bug hunt #37): one shared bit
  // let a left press followed by a right release anywhere fire
  // rightclick. Both branches now require the release inside the rect
  // and the matching arming button; any Up disarms.
  if (e.type == lithogui::PointerEvent::Type::Down && (e.button == 0 || e.button == 1)) {
    raiseState(lithogui::WS_PRESSED);
    armedButton_ = e.button;
    return true;
  }
  if (e.type == lithogui::PointerEvent::Type::Up && (e.button == 0 || e.button == 1)) {
    const bool fire = hasState(lithogui::WS_PRESSED) && armedButton_ == e.button &&
                      fromGui(rect()).contains({e.x, e.y});
    dropState(lithogui::WS_PRESSED);
    armedButton_ = -1;
    if (fire) emit(e.button == 0 ? "click" : "rightclick");
    return true;
  }
  return false;
}

bool cSlot::handleKey(const lithogui::KeyEvent& e) {
  if (e.down && enabled() && (e.key == lithogui::KEY_ENTER || e.ch == U' ')) {
    emit(pressActivated_ ? "press" : "click");  // keyboard activation follows the slot's dispatch edge
    return true;
  }
  return false;
}

void cSlot::paint(Painter& p) const {
  const Style& st = style();
  const Rect r = fromGui(rect());
  const Color bg = stateBackground();
  if (bg.a > 0) p.fill(r, bg);
  const float bw = std::max(1.f, st.borderWidth) * scale_;
  if (selected_) {
    // D9: a style may pin the selected outline's thickness (hotbar_slot: 3 px).
    if (st.accent.a > 0) p.frame(r, st.accent, st.selectedWidth > 0 ? st.selectedWidth : bw + 1.f);
  } else if (st.border.a > 0) {
    p.frame(r, st.border, bw);
  }
  if (!icon_.empty()) {
    const Rect inner = r.inset(Edges::all(3.f * scale_));
    p.image(inner, icon_, tint_);
  }
  if (!caption_.empty()) {
    // The caption's scale solves from THIS SLOT's rect, never from the
    // global theme scale alone (Build 189, JD's machine: slots solved
    // to ~33 px at GUI scale 2 while captions painted at the theme's
    // scale 2, so "512" spanned 48 px and buried the icons). The text
    // pipeline snaps scales to whole glyph multiples, so the solve
    // picks the largest snappable scale whose caption fits the slot's
    // width and a bottom band of its height; the right anchor goes
    // flush when even the smallest scale is tight, so the caption
    // stays inside the frame either way. This is also JD's stack-text
    // item from the 189 seal (17:33 EST: baseline stack text scaled to
    // inside the frame).
    const Theme& th = theme();
    const Vec2 cell = th.glyph();
    const float wantAbs = th.pixelTextScale(st.textScale * scale_);
    const float byW = (r.w - 2.f) / (cell.x * float(caption_.size()));
    const float byH = (r.h * 0.6f) / cell.y;
    float absFit = std::min(wantAbs, std::floor(byW));
    absFit = std::min(absFit, std::floor(byH));
    if (absFit < 1.f) absFit = 1.f;  // smallest scale the pipeline can paint
    const float w = cell.x * absFit * float(caption_.size());
    const float hgt = cell.y * absFit;
    const float padR = std::max(0.f, std::min(2.f, r.w - w));
    p.text({r.right() - w - padR, r.bottom() - hgt - 1.f}, caption_, st.text,
           absFit / std::max(0.001f, th.scale()));
  }
}

Vec2 cDivider::measure(const Theme& theme) const {
  (void)theme;
  return orientation_ == Orientation::Horizontal ? Vec2{40.f, thickness_} : Vec2{thickness_, 40.f};
}

void cDivider::paint(Painter& p) const {
  const Style& st = style();
  if (st.accent.a > 0) p.fill(fromGui(rect()), st.accent);
}

// ----------------------------------------------------------------- lists --
float cListView::rowHeight() const { return theme().glyphSize(style().textScale).y + 4.f; }

void cListView::setItems(std::vector<std::string> items) {
  items_ = std::move(items);
  if (selected_ >= int(items_.size())) selected_ = int(items_.size()) - 1;
  scrollY_ = std::clamp(scrollY_, 0.f, std::max(0.f, float(items_.size()) * rowHeight() - fromGui(rect()).h));
}

void cListView::setSelected(int index, bool notify) {
  const int nv = std::clamp(index, -1, int(items_.size()) - 1);
  const bool changed = nv != selected_;
  selected_ = nv;
  if (selected_ >= 0) scrollTo(selected_);
  relayout();
  if (notify && changed) emit("select");
}

const std::string* cListView::selectedItem() const {
  return selected_ >= 0 && selected_ < int(items_.size()) ? &items_[size_t(selected_)] : nullptr;
}

int cListView::rowAt(Vec2 p) const {
  const Rect inner = fromGui(rect()).inset(style().padding);
  if (!inner.contains(p)) return -1;
  const int row = int((p.y - inner.y + scrollY_) / rowHeight());
  return row >= 0 && row < int(items_.size()) ? row : -1;
}

void cListView::scrollTo(int row) {
  const Rect inner = fromGui(rect()).inset(style().padding);
  const float rh = rowHeight();
  const float top = float(row) * rh, bottom = top + rh;
  if (top < scrollY_) scrollY_ = top;
  else if (bottom > scrollY_ + inner.h) scrollY_ = bottom - inner.h;
  scrollY_ = std::max(0.f, scrollY_);
}

Vec2 cListView::measure(const Theme& theme) const {
  const Edges pad = style().padding;
  float w = 0;
  for (const auto& it : items_) w = std::max(w, theme.textWidth(it, style().textScale));
  return {w + pad.horizontal() + 8.f, float(items_.size()) * rowHeight() + pad.vertical()};
}

bool cListView::handlePointer(const lithogui::PointerEvent& e) {
  if (!enabled()) return false;
  if (e.type == lithogui::PointerEvent::Type::Down && e.button == 0) {
    const int row = rowAt({e.x, e.y});
    if (row >= 0) setSelected(row, true);
    return true;
  }
  if (e.type == lithogui::PointerEvent::Type::Wheel) {
    scrollY_ = std::clamp(scrollY_ - e.wheelDelta * 3.f * rowHeight(), 0.f,
                          std::max(0.f, float(items_.size()) * rowHeight() - fromGui(rect()).h));
    return true;
  }
  return false;
}

bool cListView::handleKey(const lithogui::KeyEvent& e) {
  if (!e.down || !enabled()) return false;
  if (e.key == lithogui::KEY_UP) { setSelected(std::max(0, selected_ - 1), true); return true; }
  if (e.key == lithogui::KEY_DOWN) { setSelected(std::min(int(items_.size()) - 1, selected_ + 1), true); return true; }
  return false;
}

void cListView::paint(Painter& p) const {
  const Style& st = style();
  const Rect r = fromGui(rect());
  if (st.background.a > 0) p.fill(r, st.background);
  if (st.borderWidth > 0 && st.border.a > 0) p.frame(r, st.border, st.borderWidth);
  const Rect inner = r.inset(st.padding);
  Painter::ClipScope clip(p, inner);
  const float rh = rowHeight();
  const Vec2 g = theme().glyphSize(st.textScale);
  const int first = std::max(0, int(scrollY_ / rh));
  for (int i = first; i < int(items_.size()); ++i) {
    const float y = inner.y + float(i) * rh - scrollY_;
    if (y > inner.bottom()) break;
    if (i == selected_) p.fill({inner.x, y, inner.w, rh}, st.accent);
    p.text({inner.x + 2.f, y + (rh - g.y) * 0.5f}, items_[size_t(i)], st.text, st.textScale);
  }
}

float cTreeView::rowHeight() const { return theme().glyphSize(style().textScale).y + 4.f; }

std::vector<cTreeView::Row> cTreeView::rows() const {
  std::vector<Row> out;
  const std::function<void(const std::vector<TreeNode>&, int, std::vector<int>&)> walk =
      [&](const std::vector<TreeNode>& nodes, int depth, std::vector<int>& path) {
        for (size_t i = 0; i < nodes.size(); ++i) {
          path.push_back(int(i));
          out.push_back({&nodes[i], depth, path});
          if (nodes[i].expanded) walk(nodes[i].children, depth + 1, path);
          path.pop_back();
        }
      };
  std::vector<int> path;
  walk(roots_, 0, path);
  return out;
}

TreeNode* cTreeView::nodeAt(const std::vector<int>& path) {
  std::vector<TreeNode>* level = &roots_;
  TreeNode* node = nullptr;
  for (int i : path) {
    if (i < 0 || i >= int(level->size())) return nullptr;
    node = &(*level)[size_t(i)];
    level = &node->children;
  }
  return node;
}

const TreeNode* cTreeView::selected() const {
  return const_cast<cTreeView*>(this)->nodeAt(path_);
}

int cTreeView::selectedRow(const std::vector<Row>& rows) const {
  for (size_t i = 0; i < rows.size(); ++i)
    if (rows[i].path == path_) return int(i);
  return -1;
}

void cTreeView::selectPath(std::vector<int> path, bool notify) {
  const bool changed = path != path_;
  path_ = std::move(path);
  if (notify && changed) emit("select");
}

Vec2 cTreeView::measure(const Theme& theme) const {
  const Edges pad = style().padding;
  return {240.f + pad.horizontal(), float(rows().size()) * rowHeight() + pad.vertical()};
}

bool cTreeView::handlePointer(const lithogui::PointerEvent& e) {
  if (!enabled()) return false;
  const Rect inner = fromGui(rect()).inset(style().padding);
  if (e.type == lithogui::PointerEvent::Type::Wheel) {
    scrollY_ = std::clamp(scrollY_ - e.wheelDelta * 3.f * rowHeight(), 0.f,
                          std::max(0.f, float(rows().size()) * rowHeight() - inner.h));
    return true;
  }
  if (e.type != lithogui::PointerEvent::Type::Down || e.button != 0) return false;
  if (!inner.contains({e.x, e.y})) return false;
  const auto rs = rows();
  const int row = int((e.y - inner.y + scrollY_) / rowHeight());
  if (row < 0 || row >= int(rs.size())) return true;
  TreeNode* node = nodeAt(rs[size_t(row)].path);
  const float indent = float(rs[size_t(row)].depth) * 14.f;
  if (node && !node->children.empty() && e.x < inner.x + indent + 12.f) {
    node->expanded = !node->expanded;
    return true;
  }
  selectPath(rs[size_t(row)].path, true);
  return true;
}

bool cTreeView::handleKey(const lithogui::KeyEvent& e) {
  if (!e.down || !enabled()) return false;
  const auto rs = rows();
  int cur = selectedRow(rs);
  if (e.key == lithogui::KEY_UP && cur > 0) { selectPath(rs[size_t(cur - 1)].path, true); return true; }
  if (e.key == lithogui::KEY_DOWN && cur + 1 < int(rs.size())) { selectPath(rs[size_t(cur + 1)].path, true); return true; }
  if (e.key == lithogui::KEY_RIGHT || e.key == lithogui::KEY_LEFT) {
    if (TreeNode* n = nodeAt(path_)) {
      n->expanded = e.key == lithogui::KEY_RIGHT;
      return true;
    }
  }
  return false;
}

void cTreeView::paint(Painter& p) const {
  const Style& st = style();
  const Rect r = fromGui(rect());
  if (st.background.a > 0) p.fill(r, st.background);
  if (st.borderWidth > 0 && st.border.a > 0) p.frame(r, st.border, st.borderWidth);
  const Rect inner = r.inset(st.padding);
  Painter::ClipScope clip(p, inner);
  const float rh = rowHeight();
  const Vec2 g = theme().glyphSize(st.textScale);
  const auto rs = rows();
  const int sel = selectedRow(rs);
  for (size_t i = 0; i < rs.size(); ++i) {
    const float y = inner.y + float(i) * rh - scrollY_;
    if (y > inner.bottom()) break;
    if (int(i) == sel) p.fill({inner.x, y, inner.w, rh}, st.accent);
    const float indent = float(rs[i].depth) * 14.f;
    if (!rs[i].node->children.empty())
      p.text({inner.x + indent, y + (rh - g.y) * 0.5f}, rs[i].node->expanded ? "v" : ">", st.text, st.textScale);
    p.text({inner.x + indent + 14.f, y + (rh - g.y) * 0.5f}, rs[i].node->label, st.text, st.textScale);
  }
}

cDropdown::cDropdown(std::vector<std::string> items, int selected)
    : cWidget({}, "dropdown"), items_(std::move(items)) {
  selected_ = items_.empty() ? -1 : std::clamp(selected, 0, int(items_.size()) - 1);
}

void cDropdown::setItems(std::vector<std::string> items) {
  items_ = std::move(items);
  selected_ = items_.empty() ? -1 : std::clamp(selected_, 0, int(items_.size()) - 1);
}

void cDropdown::setSelected(int index, bool notify) {
  const int nv = items_.empty() ? -1 : std::clamp(index, 0, int(items_.size()) - 1);
  const bool changed = nv != selected_;
  selected_ = nv;
  if (notify && changed) emit("change");
}

Vec2 cDropdown::measure(const Theme& theme) const {
  const Edges pad = style().padding;
  float w = 0;
  for (const auto& it : items_) w = std::max(w, theme.textWidth(it, style().textScale));
  return {w + pad.horizontal() + 28.f, theme.glyphSize(style().textScale).y + pad.vertical() + 6.f};
}

void cDropdown::open() {
  if (!ctx_ && uiRoot()) ctx_ = &uiRoot()->context();  // bind lazily at first open
  if (!ctx_ || items_.empty()) return;
  if (!menuOwner_) {
    menuOwner_ = std::make_unique<lithogui::Popup>(facadeId() + "-menu", lithogui::Popup::Flavor::Menu);
    auto panel = std::make_unique<lithogui::Panel>(facadeId() + "-menu-panel");
    panel->dir = lithogui::LayoutDir::Column;
    auto list = std::make_unique<lithogui::ListBox>(facadeId() + "-menu-list");
    menuList_ = list.get();
    list->flex = 1;
    panel->addChild(std::move(list));
    menuPanel_ = panel.get();
    menuOwner_->addChild(std::move(panel));
    menuList_->onSelect = [this](lithogui::ListBox&, int i) {
      setSelected(i, true);
      closeMenu();
    };
    menuOwner_->onClose = [this](lithogui::Popup&) {};
  }
  menuList_->items() = items_;
  menuList_->setSelectedIndex(selected_);
  const Rect tr = fromGui(treeRect());
  const float lh = menuList_->lastStyle().rowHeight > 0 ? menuList_->lastStyle().rowHeight : 22.f;
  const float h = std::min(8.f, float(items_.size())) * lh + 10.f;
  menuOwner_->setFrame({tr.x, tr.y + tr.h, std::max(tr.w, 120.f), h});
  ctx_->openPopup(menuOwner_.get());
  menu_ = menuOwner_.get();
}

void cDropdown::closeMenu() {
  if (menu_) menu_->close();
  menu_ = nullptr;
}

bool cDropdown::handlePointer(const lithogui::PointerEvent& e) {
  if (e.button != 0 || !enabled()) return false;
  if (e.type == lithogui::PointerEvent::Type::Down) { raiseState(lithogui::WS_PRESSED); return true; }  // arm own WS_PRESSED (2026-11-07, see cButton)
  if (e.type == lithogui::PointerEvent::Type::Up) {
    // Mirror cButton (bug hunt #38): toggle the menu only when the
    // release lands inside the rect; capture delivers the release at any
    // coordinates. Always clear WS_PRESSED.
    const bool fire = hasState(lithogui::WS_PRESSED) && fromGui(rect()).contains({e.x, e.y});
    dropState(lithogui::WS_PRESSED);
    if (fire) {
      if (isOpen()) closeMenu();
      else open();
    }
    return true;
  }
  return false;
}

bool cDropdown::handleKey(const lithogui::KeyEvent& e) {
  if (!e.down || !enabled()) return false;
  if (e.key == lithogui::KEY_ENTER || e.ch == U' ') {
    if (isOpen()) closeMenu();
    else open();
    return true;
  }
  if (e.key == lithogui::KEY_UP) { setSelected(selected_ - 1, true); return true; }
  if (e.key == lithogui::KEY_DOWN) { setSelected(selected_ + 1, true); return true; }
  return false;
}

void cDropdown::detachContext(lithogui::UiContext* ctx) {
  if (ctx_ == ctx) {
    ctx_ = nullptr;
    menu_ = nullptr;
  }
}

void cDropdown::paint(Painter& p) const {
  const Style& st = style();
  const Rect r = fromGui(rect());
  const Color bg = stateBackground();
  if (bg.a > 0) p.fill(r, bg);
  if (st.borderWidth > 0 && (focused() ? st.accent : st.border).a > 0)
    p.frame(r, focused() ? st.accent : st.border, st.borderWidth);
  const std::string shown = selected_ >= 0 && selected_ < int(items_.size()) ? items_[size_t(selected_)] : std::string();
  p.textIn({r.x, r.y, std::max(0.f, r.w - 20.f), r.h}, shown, enabled() ? st.text : st.textDisabled, false, st.textScale);
  p.textIn({r.right() - 20.f, r.y, 20.f, r.h}, "v", st.text, true, st.textScale);
}

// ----------------------------------------------------------------- menus --
float cMenu::rowHeight() const { return theme().glyphSize(style().textScale).y + 8.f; }

Rect cMenu::rowRect(int i) const {
  const Rect r = fromGui(rect());
  const Edges pad = style().padding;
  return {r.x + pad.left, r.y + pad.top + float(i) * rowHeight(), r.w - pad.horizontal(), rowHeight()};
}

int cMenu::rowAt(Vec2 p) const {
  const Rect inner = fromGui(rect()).inset(style().padding);
  if (!inner.contains(p)) return -1;
  const int row = int((p.y - inner.y) / rowHeight());
  return row >= 0 && row < int(items_.size()) ? row : -1;
}

cMenu::cMenu(std::vector<MenuItem> items) : cWidget({}, "menu"), items_(std::move(items)) {}

cMenu& cMenu::open(cUiRoot& ui, std::vector<MenuItem> items, Vec2 pos) {
  auto host = std::make_unique<lithogui::Popup>("menu-host", lithogui::Popup::Flavor::Menu);
  auto menu = std::make_unique<cMenu>(std::move(items));
  cMenu* raw = menu.get();
  host->addChild(std::move(menu));
  const Vec2 m = raw->measure(ui.theme());
  host->setFrame({pos.x, pos.y, m.x, m.y});
  raw->hostOwner_ = nullptr;  // the root owns the host while open
  ui.pushModal(std::move(host));
  return *raw;
}

bool cMenu::activate(int index) {
  if (index < 0 || index >= int(items_.size())) return false;
  MenuItem item = items_[size_t(index)];
  if (item.separator) return false;
  if (item.action) item.action();
  if (auto* popup = dynamic_cast<lithogui::Popup*>(parent())) popup->close();
  if (onDone) onDone();
  return true;
}

void cMenu::step(int dir) {
  const int n = int(items_.size());
  if (n == 0) return;
  int i = highlight_;
  for (int step = 0; step < n; ++step) {
    i = (i + dir + n) % n;
    if (!items_[size_t(i)].separator) {
      highlight_ = i;
      return;
    }
  }
}

Vec2 cMenu::measure(const Theme& theme) const {
  const Edges pad = style().padding;
  float w = 0;
  for (const auto& it : items_) {
    w = std::max(w, theme.textWidth(it.label, style().textScale) + theme.textWidth(it.shortcut, style().textScale) + 24.f);
  }
  return {w + pad.horizontal(), float(items_.size()) * rowHeight() + pad.vertical()};
}

bool cMenu::handlePointer(const lithogui::PointerEvent& e) {
  if (e.type == lithogui::PointerEvent::Type::Move) {
    highlight_ = rowAt({e.x, e.y});
    return highlight_ >= 0;
  }
  if (e.type == lithogui::PointerEvent::Type::Down && e.button == 0) return true;
  if (e.type == lithogui::PointerEvent::Type::Up && e.button == 0) {
    const int row = rowAt({e.x, e.y});
    if (row >= 0) activate(row);
    return true;
  }
  return false;
}

bool cMenu::handleKey(const lithogui::KeyEvent& e) {
  if (!e.down) return false;
  if (e.key == lithogui::KEY_UP) { step(-1); if (onNavigate) onNavigate(-1); return true; }
  if (e.key == lithogui::KEY_DOWN) { step(1); if (onNavigate) onNavigate(1); return true; }
  if (e.key == lithogui::KEY_ENTER && highlight_ >= 0) { activate(highlight_); return true; }
  return false;
}

void cMenu::paint(Painter& p) const {
  const Style& st = style();
  const Rect r = fromGui(rect());
  if (st.background.a > 0) p.fill(r, st.background);
  if (st.borderWidth > 0 && st.border.a > 0) p.frame(r, st.border, st.borderWidth);
  const Vec2 g = theme().glyphSize(st.textScale);
  for (int i = 0; i < int(items_.size()); ++i) {
    const Rect rr = rowRect(i);
    const MenuItem& it = items_[size_t(i)];
    if (it.separator) {
      p.fill({rr.x, rr.y + rr.h * 0.5f - 0.5f, rr.w, 1.f}, st.border);
      continue;
    }
    if (i == highlight_) p.fill(rr, st.accent);
    p.text({rr.x + 4.f, rr.y + (rr.h - g.y) * 0.5f}, it.label, st.text, st.textScale);
    if (!it.shortcut.empty()) {
      const float w = theme().textWidth(it.shortcut, st.textScale);
      p.text({rr.right() - w - 4.f, rr.y + (rr.h - g.y) * 0.5f}, it.shortcut, st.textDisabled, st.textScale);
    }
  }
}

void cMenuBar::addMenu(std::string title, std::vector<MenuItem> items) {
  menus_.push_back({std::move(title), std::move(items)});
}

int cMenuBar::openIndex() const { return open_; }

Rect cMenuBar::titleRect(int i) const {
  const Rect r = fromGui(rect());
  float x = r.x + style().padding.left;
  for (int j = 0; j < i; ++j) x += theme().textWidth(menus_[size_t(j)].title) + 20.f;
  return {x, r.y, theme().textWidth(menus_[size_t(i)].title) + 16.f, r.h};
}

int cMenuBar::titleAt(Vec2 p) const {
  for (int i = 0; i < count(); ++i)
    if (titleRect(i).contains(p)) return i;
  return -1;
}

void cMenuBar::openMenu(int index) {
  closeMenu();
  if (index < 0 || index >= count() || !uiRoot()) return;
  open_ = index;
  const Rect tr = titleRect(index);
  auto menu = std::make_unique<cMenu>(menus_[size_t(index)].items);
  cMenu* raw = menu.get();
  raw->onDone = [this] { open_ = -1; menuPopup_ = nullptr; };
  auto host = std::make_unique<lithogui::Popup>("menubar-menu", lithogui::Popup::Flavor::Menu);
  host->addChild(std::move(menu));
  const Vec2 m = raw->measure(theme());
  host->setFrame({tr.x, tr.bottom(), m.x, m.y});
  // Resync on EVERY close path (bug hunt #46): the context closes a Menu
  // popup on Escape / outside click without invoking the menu's onDone,
  // which left open_ set (stale highlight) and made the next title click
  // "close" an already-closed popup instead of reopening. pushModal
  // composes its own onClose (owner pruning) after this one.
  host->onClose = [this](lithogui::Popup& p) {
    if (menuPopup_ == &p) {
      open_ = -1;
      menuPopup_ = nullptr;
    }
  };
  menuPopup_ = host.get();
  uiRoot()->pushModal(std::move(host));
}

void cMenuBar::closeMenu() {
  if (menuPopup_) menuPopup_->close();
  menuPopup_ = nullptr;
  open_ = -1;
}

Vec2 cMenuBar::measure(const Theme& theme) const {
  const Edges pad = style().padding;
  float w = pad.horizontal();
  for (const auto& m : menus_) w += theme.textWidth(m.title) + 20.f;
  return {w, theme.glyphSize(style().textScale).y + pad.vertical() + 6.f};
}

bool cMenuBar::handlePointer(const lithogui::PointerEvent& e) {
  if (e.type != lithogui::PointerEvent::Type::Down || e.button != 0) return false;
  const int i = titleAt({e.x, e.y});
  if (i < 0) return false;
  if (i == open_) closeMenu();
  else openMenu(i);
  return true;
}

bool cMenuBar::handleKey(const lithogui::KeyEvent& e) {
  (void)e;
  return false;
}

void cMenuBar::paint(Painter& p) const {
  const Style& st = style();
  const Rect r = fromGui(rect());
  if (st.background.a > 0) p.fill(r, st.background);
  for (int i = 0; i < count(); ++i) {
    const Rect tr = titleRect(i);
    if (i == open_) p.fill(tr, st.accent);
    p.textIn(tr, menus_[size_t(i)].title, st.text, true, st.textScale);
  }
}

// ---------------------------------------------------------------- dialog --
float cDialog::titleHeight() const { return theme().glyph().y + 12.f; }

cDialog::cDialog(std::string title, Vec2 size) : cWidget({}, "dialog"), title_(std::move(title)), size_(size) {
  auto bodyStack = std::make_unique<cStackPanel>(Orientation::Vertical, 6.f, "stack");
  body_ = bodyStack.get();
  addChild(std::move(bodyStack));
}

cDialog& cDialog::show(cUiRoot& ui, std::string title, Vec2 size) {
  auto dlg = std::make_unique<cDialog>(std::move(title), size);
  cDialog& ref = *dlg;
  auto host = std::make_unique<DialogHost>(ref.facadeId().empty() ? std::string("dialog-host") : ref.facadeId() + "-host");
  host->dialog = &ref;
  // The dialog is owner-placed: DialogHost::layout centres it, and the
  // facade's own passes place/paint it. It must stay out of the host's
  // Widget::layout child pass, which would otherwise design-place it at
  // the host's origin (fixedW/H unset, so inner-minus-margins: at open
  // time the host rect is still 0x0) and stamp WS_ABS_PLACED on it,
  // which that pass then treats as placed forever after. layoutExempt is
  // that pass's documented owner-placed marker (title-bar pattern).
  ref.layoutExempt = true;
  ref.host_ = host.get();
  host->addChild(std::move(dlg));
  host->onClose = [&ref](lithogui::Popup&) { ref.notifyHostClosed(); };
  ui.pushDialog(std::move(host), ref);
  return ref;
}

void cDialog::notifyHostClosed() {
  host_ = nullptr;
  if (closeFired_ || suppressEmit_) return;
  closeFired_ = true;
  emit("close");
}

cButton& cDialog::addButton(std::string text, std::function<void()> onClick, bool closes) {
  if (!buttons_) {
    auto row = std::make_unique<cStackPanel>(Orientation::Horizontal, 8.f, "stack");
    buttons_ = row.get();
    // The button row is a direct child after the body (arrange seats it).
    addChild(std::move(row));
  }
  auto* row = static_cast<cStackPanel*>(buttons_);
  cButton& b = row->add<cButton>(std::move(text));
  if (onClick || closes) {
    b.on("click", [this, onClick, closes](cWidget&) {
      if (onClick) onClick();
      if (closes) close();
    });
  }
  return b;
}

void cDialog::close() {
  if (!closeFired_) {
    closeFired_ = true;
    emit("close");
  }
  if (host_) host_->close();
}

bool cDialog::handleKey(const lithogui::KeyEvent& e) {
  // The context normally intercepts Escape while a popup is open and
  // closes the host (notifyHostClosed then emits "close"); this path only
  // runs for a dialog shown without a host.
  if (e.down && e.key == lithogui::KEY_ESCAPE && !host_) {
    if (closeGuard_ && !closeGuard_()) return true;  // vetoed: stays open
    close();
    return true;
  }
  return false;
}

bool cDialog::handlePointer(const lithogui::PointerEvent& e) {
  const Rect r = fromGui(rect());
  const Rect titleBar{r.x, r.y, r.w, titleHeight()};
  if (e.type == lithogui::PointerEvent::Type::Down && e.button == 0 && titleBar.contains({e.x, e.y})) {
    dragGrab_ = {e.x - r.x, e.y - r.y};
    return true;
  }
  if (dragGrab_.x >= 0 && e.type == lithogui::PointerEvent::Type::Move) {
    // Dragging moves the dialog inside its host: re-place at the grabbed
    // offset (the host's layout only centres on open / relayout).
    place({e.x - dragGrab_.x, e.y - dragGrab_.y, r.w, r.h}, theme());
    return true;
  }
  if (e.type == lithogui::PointerEvent::Type::Up) {
    const bool was = dragGrab_.x >= 0;
    dragGrab_ = {-1, -1};
    return was;
  }
  return false;
}

Vec2 cDialog::measure(const Theme& theme) const {
  if (size_.x > 0 || size_.y > 0) {
    const Vec2 body = body_ ? body_->measure(theme) : Vec2{};
    return {size_.x > 0 ? size_.x : body.x, size_.y > 0 ? size_.y : body.y + titleHeight()};
  }
  const Edges pad = style().padding;
  Vec2 out = body_ ? body_->measure(theme) : Vec2{};
  out.x += pad.horizontal();
  out.y += titleHeight() + pad.vertical();
  if (buttons_) {
    const Vec2 b = static_cast<const cWidget*>(buttons_)->measure(theme);
    out.x = std::max(out.x, b.x + pad.horizontal());
    out.y += b.y + 6.f;
  }
  return out;
}

void cDialog::arrange(const Theme& theme) {
  const Rect r = fromGui(rect());
  const Edges pad = style().padding;
  const float th = titleHeight();
  const Rect bodyBox{r.x + pad.left, r.y + th + pad.top, std::max(0.f, r.w - pad.horizontal()),
                     std::max(0.f, r.h - th - pad.vertical())};
  Vec2 bs{0, 0};
  if (buttons_) bs = static_cast<cWidget*>(buttons_)->measure(theme);
  const float buttonsH = buttons_ ? bs.y + 6.f : 0;
  if (body_) body_->place({bodyBox.x, bodyBox.y, bodyBox.w, std::max(0.f, bodyBox.h - buttonsH)}, theme);
  if (buttons_) {
    auto* row = static_cast<cWidget*>(buttons_);
    row->place({bodyBox.x, bodyBox.bottom() - bs.y, bodyBox.w, bs.y}, theme);
  }
}

void cDialog::paint(Painter& p) const {
  const Style& st = style();
  const Rect r = fromGui(rect());
  if (st.background.a > 0) p.fill(r, st.background);
  if (st.borderWidth > 0 && st.border.a > 0) p.frame(r, st.border, st.borderWidth);
  const Rect bar{r.x, r.y, r.w, titleHeight()};
  p.fill(bar, st.hover);
  p.textIn(bar, title_, st.text, false, st.textScale);
}

void DialogHost::layout(lithogui::Rect r, const lithogui::Style& style) {
  noteStyle(style);
  setRect(r);  // the host spans the viewport; the dialog centres inside
  if (!dialog) return;
  // An explicitly inset dialog layout (the script editor's fill with
  // insets) stretches with the viewport, like the old root-placed dialog.
  const Layout& lay = dialog->layout();
  if (lay.insets.left > 0 || lay.insets.top > 0 || lay.insets.right > 0 || lay.insets.bottom > 0) {
    dialog->place(anchoredRect(lay, fromGui(r), dialog->measure(dialog->theme())), dialog->theme());
    return;
  }
  // A dialog the user dragged keeps its place AND SIZE across
  // relayouts... but only when its current size still matches its
  // content's measure. The open-time layout centred the dialog while
  // it was still EMPTY (cDialog::show opens the popup before the
  // picker adds its body), freezing it at a padding-only 8x36 while
  // its body measured 1232x552: the world picker rendered as a sliver
  // at the screen centre with its content spilling elsewhere
  // (JD, 2026-11-07). A size mismatch against a fresh measure means
  // the placement is stale, not dragged: fall through and re-centre.
  const Vec2 fresh = dialog->measure(dialog->theme());
  if (dialog->rect().w > 0 && dialog->rect().h > 0 &&
      std::abs(dialog->rect().w - fresh.x) < 1.f && std::abs(dialog->rect().h - fresh.y) < 1.f &&
      dialog->rect().x >= r.x - 0.5f && dialog->rect().y >= r.y - 0.5f &&
      dialog->rect().x + dialog->rect().w <= r.x + r.w + 0.5f &&
      dialog->rect().y + dialog->rect().h <= r.y + r.h + 0.5f) {
    dialog->place(fromGui(dialog->rect()), dialog->theme());
    return;
  }
  const Vec2 m = fresh;
  const float w = std::min(m.x, std::max(0.f, r.w - 16.f));
  const float h = std::min(m.y, std::max(0.f, r.h - 16.f));
  dialog->place({r.x + (r.w - w) * 0.5f, r.y + (r.h - h) * 0.5f, w, h}, dialog->theme());
  // Popup::layout honours a set frame over any rect it is handed, and
  // UiContext::layout never re-lays popups: without this, the centering
  // above would never run again after the open-time layout (which ran
  // while the dialog was still EMPTY and measured as a sliver), so the
  // picker rendered as an 8x36 stub at the screen centre on JD's machine
  // (2026-11-07). setRect pins the host's own rect to the handed
  // viewport; the frame follows on the next direct set in layout().
  setRect(r);
  setFrame(r);
}

void DialogHost::draw(lithogui::DrawList& dl, const lithogui::DrawParams& params) const {
  (void)dl;
  (void)params;
  if (!dialog || !dialog->visible()) return;
  PaintSink& sink = paintSink();
  if (!sink.dl || !sink.theme) return;  // outside cUiRoot::paint: nothing to emit into
  Painter p = sinkPainter(*sink.theme);
  dialog->paintTree(p);
}

}  // namespace ui
}  // namespace lithokraft
