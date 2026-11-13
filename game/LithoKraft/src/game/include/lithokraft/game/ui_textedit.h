// Text input widgets for the facade: cTextBuffer (the editing model),
// cTextBox (one line) and cTextEditor (multi-line, line numbers, scrolling).
// This is a port of the old toolkit's text_buffer.h / text_edit.h surface
// (engine ui at 67c1590) onto facade widgets: same positions API, same
// editing operations and undo history, same key contract (arrows with
// Shift/Ctrl modifiers, Home/End, Ctrl+A/C/X/V/Z/Y, Ctrl+S -> "save" in
// the editor, Enter -> "submit" in the box / newline in the editor).
// Columns are byte indices (the UI font is ASCII), as before.
#pragma once
#include <compare>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "lithokraft/game/ui_facade.h"

namespace lithokraft {
namespace ui {

struct TextPos {
  int line = 0, col = 0;
  friend auto operator<=>(const TextPos&, const TextPos&) = default;
};

class cTextBuffer {
 public:
  explicit cTextBuffer(bool multiLine = true);

  // ---- content
  void setText(std::string_view text);  ///< resets caret, selection and history
  std::string text() const;
  int lineCount() const { return int(lines_.size()); }
  const std::string& line(int i) const { return lines_[size_t(i)]; }
  bool multiLine() const { return multiLine_; }
  /// Bumped by every edit (lets views and owners notice changes cheaply).
  uint64_t revision() const { return revision_; }

  // ---- positions
  TextPos clamp(TextPos p) const;
  /// Byte offset of `p` in text() ('\n' counts as one).
  size_t offsetOf(TextPos p) const;
  TextPos posAt(size_t offset) const;
  TextPos endPos() const { return {lineCount() - 1, int(lines_.back().size())}; }

  // ---- caret and selection
  TextPos caret() const { return caret_; }
  TextPos anchor() const { return anchor_; }
  bool hasSelection() const { return caret_ != anchor_; }
  /// Ordered (begin, end).
  std::pair<TextPos, TextPos> selection() const;
  std::string selectedText() const;
  /// Moves the caret; keeps the anchor when `extend` (shift-selection).
  void setCaret(TextPos p, bool extend = false);
  void select(TextPos from, TextPos to);
  void selectAll();

  // ---- editing (replace the selection; recorded for undo)
  void insert(std::string_view s);
  void backspace();
  void deleteForward();
  void newline();
  /// Removes the selection; returns false if there was none.
  bool eraseSelection();

  // ---- caret movement
  void moveLeft(bool extend, bool word = false);
  void moveRight(bool extend, bool word = false);
  void moveUp(bool extend, int lines = 1);
  void moveDown(bool extend, int lines = 1);
  void moveHome(bool extend);  ///< first non-blank, then column 0
  void moveEnd(bool extend);
  void moveDocStart(bool extend) { setCaret({0, 0}, extend); }
  void moveDocEnd(bool extend) { setCaret(endPos(), extend); }

  // ---- history
  bool undo();
  bool redo();
  bool canUndo() const { return !undo_.empty(); }

 private:
  struct Snapshot {
    std::vector<std::string> lines;
    TextPos caret, anchor;
  };
  void record();
  void replaceRange(TextPos from, TextPos to, std::string_view s);
  void changed();

  std::vector<std::string> lines_{std::string()};
  TextPos caret_, anchor_;
  int preferredCol_ = -1;  ///< column kept across vertical moves
  bool multiLine_;
  uint64_t revision_ = 0;
  std::vector<Snapshot> undo_, redo_;
  static constexpr size_t kMaxHistory = 256;
};

/// Behaviour shared by cTextBox and cTextEditor; not used directly.
class cTextEditBase : public cWidget {
 public:
  cTextBuffer& buffer() { return buffer_; }
  const cTextBuffer& buffer() const { return buffer_; }
  std::string text() const { return buffer_.text(); }
  void setText(std::string_view t);
  bool readOnly() const { return readOnly_; }
  void setReadOnly(bool r) { readOnly_ = r; }

  /// Text position under a screen point (clamped into the text).
  TextPos positionAt(Vec2 p) const;
  /// Screen rectangle of the caret (1 px wide).
  Rect caretRect() const;

  bool focusable() const override { return true; }
  bool handlePointer(const lithogui::PointerEvent& e) override;
  bool handleKey(const lithogui::KeyEvent& e) override;

 protected:
  cTextEditBase(std::string styleClass, bool multiLine);
  /// Top-left of the text (line 0, column 0) on screen, after scrolling.
  virtual Vec2 textOrigin() const = 0;
  /// Called after the caret moved or the text changed (scroll the caret into view).
  virtual void caretMoved() = 0;
  /// Keys only one variant handles (Enter, Tab, Ctrl+S).
  virtual bool onSpecialKey(const lithogui::KeyEvent& e) = 0;
  Vec2 glyph() const;
  void paintText(Painter& p, const Rect& clip, int firstLine, int lastLine) const;
  void edited();

 private:
  cTextBuffer buffer_;
  bool readOnly_ = false;
  bool dragging_ = false;
};

class cTextBox final : public cTextEditBase {
 public:
  explicit cTextBox(std::string text = {});
  cTextBox& setPlaceholder(std::string p) { placeholder_ = std::move(p); return *this; }
  Vec2 measure(const Theme& theme) const override;

 protected:
  Vec2 textOrigin() const override;
  void caretMoved() override;
  bool onSpecialKey(const lithogui::KeyEvent& e) override;
  void paint(Painter& p) const override;

 private:
  std::string placeholder_;
  float scrollX_ = 0;
};

class cTextEditor final : public cTextEditBase {
 public:
  cTextEditor();
  cTextEditor& setLineNumbers(bool on) { lineNumbers_ = on; return *this; }
  /// Lines that fit in the view.
  int visibleLines() const;
  int firstVisibleLine() const;
  /// Scrolls so `line` is visible.
  void scrollToLine(int line);
  Vec2 measure(const Theme& theme) const override;
  bool handlePointer(const lithogui::PointerEvent& e) override;

 protected:
  Vec2 textOrigin() const override;
  void caretMoved() override;
  bool onSpecialKey(const lithogui::KeyEvent& e) override;
  void paint(Painter& p) const override;

 private:
  float gutterWidth() const;
  Rect textArea() const;
  bool lineNumbers_ = true;
  float scrollX_ = 0, scrollY_ = 0;
};

}  // namespace ui
}  // namespace lithokraft
