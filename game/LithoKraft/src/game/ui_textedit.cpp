// ui_textedit.cpp: cTextBuffer + the text input widgets. Port of the old
// toolkit's text_buffer.cpp / text_edit.cpp behavior onto facade widgets
// (see ui_textedit.h). Clipboard goes through the root's IClipboard seam,
// exactly where the old widgets read it.
#include "lithokraft/game/ui_textedit.h"
#include <algorithm>

namespace lithokraft {
namespace ui {
namespace {

void appendUtf8(std::string& out, char32_t cp) {
  if (cp < 0x80) {
    out += char(cp);
  } else if (cp < 0x800) {
    out += char(0xC0 | (cp >> 6));
    out += char(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    out += char(0xE0 | (cp >> 12));
    out += char(0x80 | ((cp >> 6) & 0x3F));
    out += char(0x80 | (cp & 0x3F));
  } else {
    out += char(0xF0 | (cp >> 18));
    out += char(0x80 | ((cp >> 12) & 0x3F));
    out += char(0x80 | ((cp >> 6) & 0x3F));
    out += char(0x80 | (cp & 0x3F));
  }
}

bool wordChar(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; }

}  // namespace

// ------------------------------------------------------------- cTextBuffer --
cTextBuffer::cTextBuffer(bool multiLine) : multiLine_(multiLine) {}

void cTextBuffer::changed() { ++revision_; }

void cTextBuffer::record() {
  undo_.push_back({lines_, caret_, anchor_});
  if (undo_.size() > kMaxHistory) undo_.erase(undo_.begin());
  redo_.clear();
}

void cTextBuffer::setText(std::string_view text) {
  lines_.clear();
  std::string cur;
  for (char c : text) {
    if (c == '\n' && multiLine_) {
      lines_.push_back(cur);
      cur.clear();
    } else if (c != '\r' && c != '\n') {
      cur += c;
    }
  }
  lines_.push_back(cur);
  if (lines_.empty()) lines_.push_back("");
  caret_ = anchor_ = {0, 0};
  preferredCol_ = -1;
  undo_.clear();
  redo_.clear();
  changed();
}

std::string cTextBuffer::text() const {
  std::string out;
  for (size_t i = 0; i < lines_.size(); ++i) {
    if (i) out += '\n';
    out += lines_[i];
  }
  return out;
}

TextPos cTextBuffer::clamp(TextPos p) const {
  p.line = std::clamp(p.line, 0, lineCount() - 1);
  p.col = std::clamp(p.col, 0, int(lines_[size_t(p.line)].size()));
  return p;
}

size_t cTextBuffer::offsetOf(TextPos p) const {
  p = clamp(p);
  size_t off = 0;
  for (int i = 0; i < p.line; ++i) off += lines_[size_t(i)].size() + 1;
  return off + size_t(p.col);
}

TextPos cTextBuffer::posAt(size_t offset) const {
  for (int i = 0; i < lineCount(); ++i) {
    const size_t len = lines_[size_t(i)].size();
    if (offset <= len) return {i, int(offset)};
    offset -= len + 1;
  }
  return endPos();
}

std::pair<TextPos, TextPos> cTextBuffer::selection() const {
  return caret_ < anchor_ ? std::pair{caret_, anchor_} : std::pair{anchor_, caret_};
}

std::string cTextBuffer::selectedText() const {
  if (!hasSelection()) return {};
  const auto [from, to] = selection();
  const std::string all = text();
  const size_t a = offsetOf(from), b = offsetOf(to);
  return all.substr(a, b - a);
}

void cTextBuffer::setCaret(TextPos p, bool extend) {
  caret_ = clamp(p);
  if (!extend) anchor_ = caret_;
  preferredCol_ = -1;
}

void cTextBuffer::select(TextPos from, TextPos to) {
  anchor_ = clamp(from);
  caret_ = clamp(to);
  preferredCol_ = -1;
}

void cTextBuffer::selectAll() { select({0, 0}, endPos()); }

void cTextBuffer::replaceRange(TextPos from, TextPos to, std::string_view s) {
  from = clamp(from);
  to = clamp(to);
  if (to < from) std::swap(from, to);
  std::string ins;
  ins.reserve(s.size());
  for (char c : s) {
    if (c == '\r') continue;
    if (c == '\n' && !multiLine_) continue;
    ins += c;
  }
  const std::string head = lines_[size_t(from.line)].substr(0, size_t(from.col));
  const std::string tail = lines_[size_t(to.line)].substr(size_t(to.col));
  std::vector<std::string> mid;
  std::string cur;
  for (char c : ins) {
    if (c == '\n') {
      mid.push_back(cur);
      cur.clear();
    } else cur += c;
  }
  mid.push_back(cur);
  std::vector<std::string> next;
  for (int i = 0; i < from.line; ++i) next.push_back(lines_[size_t(i)]);
  TextPos caretPos;
  if (mid.size() == 1) {
    next.push_back(head + mid[0] + tail);
    caretPos = {from.line, from.col + int(mid[0].size())};
  } else {
    next.push_back(head + mid.front());
    for (size_t i = 1; i + 1 < mid.size(); ++i) next.push_back(mid[i]);
    next.push_back(mid.back() + tail);
    caretPos = {from.line + int(mid.size()) - 1, int(mid.back().size())};
  }
  for (int i = to.line + 1; i < lineCount(); ++i) next.push_back(lines_[size_t(i)]);
  lines_ = std::move(next);
  caret_ = anchor_ = caretPos;
  preferredCol_ = -1;
  changed();
}

void cTextBuffer::insert(std::string_view s) {
  if (s.empty() && !hasSelection()) return;
  record();
  const auto [from, to] = selection();
  replaceRange(from, to, s);
}

bool cTextBuffer::eraseSelection() {
  if (!hasSelection()) return false;
  record();
  const auto [from, to] = selection();
  replaceRange(from, to, "");
  return true;
}

void cTextBuffer::backspace() {
  if (eraseSelection()) return;
  if (caret_.col > 0) {
    record();
    replaceRange({caret_.line, caret_.col - 1}, caret_, "");
  } else if (caret_.line > 0) {
    record();
    replaceRange({caret_.line - 1, int(lines_[size_t(caret_.line - 1)].size())}, caret_, "");
  }
}

void cTextBuffer::deleteForward() {
  if (eraseSelection()) return;
  if (caret_.col < int(lines_[size_t(caret_.line)].size())) {
    record();
    replaceRange(caret_, {caret_.line, caret_.col + 1}, "");
  } else if (caret_.line + 1 < lineCount()) {
    record();
    replaceRange(caret_, {caret_.line + 1, 0}, "");
  }
}

void cTextBuffer::newline() {
  if (!multiLine_) return;
  insert("\n");
}

void cTextBuffer::moveLeft(bool extend, bool word) {
  TextPos p = caret_;
  if (word) {
    const std::string& ln = lines_[size_t(p.line)];
    int c = p.col;
    while (c > 0 && !wordChar(ln[size_t(c - 1)])) --c;
    while (c > 0 && wordChar(ln[size_t(c - 1)])) --c;
    if (c == p.col && p.line > 0) { --p.line; p.col = int(lines_[size_t(p.line)].size()); }
    else p.col = c;
  } else if (p.col > 0) {
    --p.col;
  } else if (p.line > 0) {
    --p.line;
    p.col = int(lines_[size_t(p.line)].size());
  }
  setCaret(p, extend);
  preferredCol_ = -1;
}

void cTextBuffer::moveRight(bool extend, bool word) {
  TextPos p = caret_;
  const std::string& ln = lines_[size_t(p.line)];
  if (word) {
    int c = p.col;
    const int n = int(ln.size());
    while (c < n && wordChar(ln[size_t(c)])) ++c;
    while (c < n && !wordChar(ln[size_t(c)])) ++c;
    if (c == p.col && p.line + 1 < lineCount()) { ++p.line; p.col = 0; }
    else p.col = c;
  } else if (p.col < int(ln.size())) {
    ++p.col;
  } else if (p.line + 1 < lineCount()) {
    ++p.line;
    p.col = 0;
  }
  setCaret(p, extend);
  preferredCol_ = -1;
}

void cTextBuffer::moveUp(bool extend, int lines) {
  if (preferredCol_ < 0) preferredCol_ = caret_.col;
  TextPos p = caret_;
  p.line = std::max(0, p.line - lines);
  p.col = std::min(preferredCol_, int(lines_[size_t(p.line)].size()));
  caret_ = p;
  if (!extend) anchor_ = caret_;
}

void cTextBuffer::moveDown(bool extend, int lines) {
  if (preferredCol_ < 0) preferredCol_ = caret_.col;
  TextPos p = caret_;
  p.line = std::min(lineCount() - 1, p.line + lines);
  p.col = std::min(preferredCol_, int(lines_[size_t(p.line)].size()));
  caret_ = p;
  if (!extend) anchor_ = caret_;
}

void cTextBuffer::moveHome(bool extend) {
  const std::string& ln = lines_[size_t(caret_.line)];
  int first = 0;
  while (first < int(ln.size()) && (ln[size_t(first)] == ' ' || ln[size_t(first)] == '\t')) ++first;
  setCaret({caret_.line, caret_.col == first ? 0 : first}, extend);
}

void cTextBuffer::moveEnd(bool extend) { setCaret({caret_.line, int(lines_[size_t(caret_.line)].size())}, extend); }

bool cTextBuffer::undo() {
  if (undo_.empty()) return false;
  redo_.push_back({lines_, caret_, anchor_});
  Snapshot s = std::move(undo_.back());
  undo_.pop_back();
  lines_ = std::move(s.lines);
  caret_ = anchor_ = s.caret;
  anchor_ = s.anchor;
  changed();
  return true;
}

bool cTextBuffer::redo() {
  if (redo_.empty()) return false;
  undo_.push_back({lines_, caret_, anchor_});
  Snapshot s = std::move(redo_.back());
  redo_.pop_back();
  lines_ = std::move(s.lines);
  caret_ = s.caret;
  anchor_ = s.anchor;
  changed();
  return true;
}

// ------------------------------------------------------------ cTextEditBase --
cTextEditBase::cTextEditBase(std::string styleClass, bool multiLine)
    : cWidget({}, std::move(styleClass)), buffer_(multiLine) {}

Vec2 cTextEditBase::glyph() const { return theme().glyphSize(style().textScale); }

void cTextEditBase::setText(std::string_view t) {
  buffer_.setText(t);
  caretMoved();
}

void cTextEditBase::edited() {
  caretMoved();
  emit("change");
}

TextPos cTextEditBase::positionAt(Vec2 p) const {
  const Vec2 o = textOrigin();
  const Vec2 g = glyph();
  int line = g.y > 0 ? int((p.y - o.y) / g.y) : 0;
  int col = g.x > 0 ? int((p.x - o.x) / g.x + 0.5f) : 0;
  return buffer_.clamp({line, col});
}

Rect cTextEditBase::caretRect() const {
  const Vec2 o = textOrigin();
  const Vec2 g = glyph();
  const TextPos c = buffer_.caret();
  return {o.x + float(c.col) * g.x, o.y + float(c.line) * g.y, 1.f, g.y};
}

bool cTextEditBase::handlePointer(const lithogui::PointerEvent& e) {
  if (!enabled()) return false;
  if (e.type == lithogui::PointerEvent::Type::Down && e.button == 0) {
    focus();
    dragging_ = true;
    buffer_.setCaret(positionAt({e.x, e.y}), e.shift);
    caretMoved();
    return true;
  }
  if (dragging_ && e.type == lithogui::PointerEvent::Type::Move) {
    buffer_.setCaret(positionAt({e.x, e.y}), true);
    caretMoved();
    return true;
  }
  if (dragging_ && e.type == lithogui::PointerEvent::Type::Up) {
    dragging_ = false;
    return true;
  }
  return false;
}

bool cTextEditBase::handleKey(const lithogui::KeyEvent& e) {
  if (!e.down || !enabled()) return false;
  cTextBuffer& b = buffer_;
  const bool ext = e.shift;
  if (e.ctrl) {
    const char32_t ch = e.ch >= U'A' && e.ch <= U'Z' ? e.ch + (U'a' - U'A') : e.ch;
    switch (ch) {
      case U'a': b.selectAll(); caretMoved(); return true;
      case U'c':
        if (uiRoot()) uiRoot()->clipboard().setText(b.selectedText());
        return true;
      case U'x':
        if (readOnly_) return true;
        if (uiRoot()) uiRoot()->clipboard().setText(b.selectedText());
        if (b.eraseSelection()) edited();
        return true;
      case U'v':
        if (readOnly_) return true;
        if (uiRoot()) {
          b.insert(uiRoot()->clipboard().text());
          edited();
        }
        return true;
      case U'z':
        if (!readOnly_ && b.undo()) edited();
        return true;
      case U'y':
        if (!readOnly_ && b.redo()) edited();
        return true;
      case U's': return onSpecialKey(e);  // the editor's save chord
      default: break;
    }
  }
  switch (e.key) {
    case lithogui::KEY_LEFT: b.moveLeft(ext, e.ctrl); caretMoved(); return true;
    case lithogui::KEY_RIGHT: b.moveRight(ext, e.ctrl); caretMoved(); return true;
    case lithogui::KEY_UP: b.moveUp(ext); caretMoved(); return true;
    case lithogui::KEY_DOWN: b.moveDown(ext); caretMoved(); return true;
    case lithogui::KEY_HOME: b.moveHome(ext); caretMoved(); return true;
    case lithogui::KEY_END: b.moveEnd(ext); caretMoved(); return true;
    case lithogui::KEY_BACKSPACE:
      if (!readOnly_) { b.backspace(); edited(); }
      return true;
    case lithogui::KEY_DELETE:
      if (!readOnly_) { b.deleteForward(); edited(); }
      return true;
    default: break;
  }
  if (onSpecialKey(e)) return true;
  if (e.ch >= 0x20 && e.ch != 0x7f && !e.ctrl && !e.alt) {
    if (!readOnly_) {
      std::string s;
      appendUtf8(s, e.ch);
      b.insert(s);
      edited();
    }
    return true;
  }
  return false;
}

void cTextEditBase::paintText(Painter& p, const Rect& clip, int firstLine, int lastLine) const {
  const Style& st = style();
  const Vec2 o = textOrigin();
  const Vec2 g = glyph();
  const auto [selA, selB] = buffer_.selection();
  const bool hasSel = buffer_.hasSelection();
  for (int line = firstLine; line <= lastLine && line < buffer_.lineCount(); ++line) {
    const std::string& text = buffer_.line(line);
    const float y = o.y + float(line) * g.y;
    if (y + g.y < clip.y || y > clip.bottom()) continue;
    if (hasSel && line >= selA.line && line <= selB.line) {
      const int from = line == selA.line ? selA.col : 0;
      const int to = line == selB.line ? selB.col : int(text.size()) + 1;
      p.fill({o.x + float(from) * g.x, y, float(std::max(0, to - from)) * g.x, g.y}, st.accent);
    }
    if (!text.empty()) p.text({o.x, y}, text, st.text, st.textScale);
  }
  if (focused()) {
    const Rect caret = caretRect();
    if (caret.y + caret.h >= clip.y && caret.y <= clip.bottom()) p.fill({caret.x, caret.y, 2.f, caret.h}, st.text);
  }
}

// ---------------------------------------------------------------- cTextBox --
cTextBox::cTextBox(std::string text) : cTextEditBase("textbox", false) {
  if (!text.empty()) setText(text);
}

Vec2 cTextBox::textOrigin() const {
  const Rect r = fromGui(rect());
  const Edges pad = style().padding;
  return {r.x + pad.left - scrollX_, r.y + (r.h - glyph().y) * 0.5f};
}

void cTextBox::caretMoved() {
  const Rect r = fromGui(rect());
  const Edges pad = style().padding;
  const float innerW = std::max(0.f, r.w - pad.horizontal());
  const float caretX = float(buffer().caret().col) * glyph().x;
  if (caretX - scrollX_ < 0) scrollX_ = caretX;
  else if (caretX - scrollX_ > innerW) scrollX_ = caretX - innerW;
  scrollX_ = std::max(0.f, scrollX_);
}

bool cTextBox::onSpecialKey(const lithogui::KeyEvent& e) {
  if (e.key == lithogui::KEY_ENTER) {
    emit("submit");
    return true;
  }
  return false;
}

Vec2 cTextBox::measure(const Theme& theme) const {
  const Edges pad = style().padding;
  return {200.f + pad.horizontal(), theme.glyphSize(style().textScale).y + pad.vertical() + 4.f};
}

void cTextBox::paint(Painter& p) const {
  const Style& st = style();
  const Rect r = fromGui(rect());
  if (st.background.a > 0) p.fill(r, st.background);
  if (st.borderWidth > 0 && (focused() ? st.accent : st.border).a > 0)
    p.frame(r, focused() ? st.accent : st.border, st.borderWidth);
  const Rect inner = r.inset(st.padding);
  Painter::ClipScope clip(p, inner);
  if (buffer().lineCount() > 0 && buffer().line(0).empty() && !placeholder_.empty() && !focused())
    p.text(textOrigin(), placeholder_, st.textDisabled, st.textScale);
  paintText(p, inner, 0, 0);
}

// -------------------------------------------------------------- cTextEditor --
cTextEditor::cTextEditor() : cTextEditBase("editor", true) {}

float cTextEditor::gutterWidth() const {
  if (!lineNumbers_) return 0;
  const int digits = std::max(2, int(std::to_string(std::max(1, buffer().lineCount())).size()));
  return float(digits) * glyph().x + 10.f;
}

Rect cTextEditor::textArea() const {
  const Rect r = fromGui(rect());
  const Edges pad = style().padding;
  return {r.x + pad.left + gutterWidth(), r.y + pad.top, std::max(0.f, r.w - pad.horizontal() - gutterWidth()),
          std::max(0.f, r.h - pad.vertical())};
}

Vec2 cTextEditor::textOrigin() const {
  const Rect a = textArea();
  return {a.x - scrollX_, a.y - scrollY_};
}

int cTextEditor::visibleLines() const {
  const float gh = glyph().y;
  return gh > 0 ? std::max(1, int(textArea().h / gh)) : 1;
}

int cTextEditor::firstVisibleLine() const {
  const float gh = glyph().y;
  return gh > 0 ? int(scrollY_ / gh) : 0;
}

void cTextEditor::scrollToLine(int line) {
  const float gh = glyph().y;
  if (gh <= 0) return;
  const float top = float(line) * gh, bottom = top + gh;
  if (top < scrollY_) scrollY_ = top;
  else if (bottom > scrollY_ + textArea().h) scrollY_ = bottom - textArea().h;
  scrollY_ = std::max(0.f, scrollY_);
}

void cTextEditor::caretMoved() {
  const TextPos c = buffer().caret();
  scrollToLine(c.line);
  const Rect a = textArea();
  const float caretX = float(c.col) * glyph().x;
  if (caretX - scrollX_ < 0) scrollX_ = caretX;
  else if (caretX - scrollX_ > a.w) scrollX_ = caretX - a.w;
  scrollX_ = std::max(0.f, scrollX_);
}

bool cTextEditor::onSpecialKey(const lithogui::KeyEvent& e) {
  if (e.ctrl && (e.ch == U's' || e.ch == U'S')) {
    emit("save");
    return true;
  }
  if (e.key == lithogui::KEY_ENTER) {
    if (!readOnly()) {
      buffer().newline();
      edited();
    }
    return true;
  }
  if (e.key == lithogui::KEY_TAB) {
    if (!readOnly()) {
      buffer().insert("  ");  // the old editor: Tab inserts spaces
      edited();
    }
    return true;
  }
  return false;
}

bool cTextEditor::handlePointer(const lithogui::PointerEvent& e) {
  if (e.type == lithogui::PointerEvent::Type::Wheel) {
    scrollY_ = std::max(0.f, scrollY_ - e.wheelDelta * 3.f * glyph().y);
    return true;
  }
  return cTextEditBase::handlePointer(e);
}

Vec2 cTextEditor::measure(const Theme& theme) const {
  const Edges pad = style().padding;
  return {480.f + pad.horizontal(), 10.f * theme.glyphSize(style().textScale).y + pad.vertical()};
}

void cTextEditor::paint(Painter& p) const {
  const Style& st = style();
  const Rect r = fromGui(rect());
  if (st.background.a > 0) p.fill(r, st.background);
  if (st.borderWidth > 0 && (focused() ? st.accent : st.border).a > 0)
    p.frame(r, focused() ? st.accent : st.border, st.borderWidth);
  const Rect a = textArea();
  const Vec2 g = glyph();
  if (lineNumbers_) {
    const int first = firstVisibleLine();
    const int last = std::min(buffer().lineCount() - 1, first + visibleLines());
    for (int line = first; line <= last; ++line) {
      const std::string num = std::to_string(line + 1);
      const float w = theme().textWidth(num, st.textScale);
      p.text({a.x - 6.f - w, a.y + float(line) * g.y - scrollY_}, num, st.textDisabled, st.textScale);
    }
  }
  Painter::ClipScope clip(p, a);
  paintText(p, a, firstVisibleLine(), std::min(buffer().lineCount() - 1, firstVisibleLine() + visibleLines()));
}

}  // namespace ui
}  // namespace lithokraft
