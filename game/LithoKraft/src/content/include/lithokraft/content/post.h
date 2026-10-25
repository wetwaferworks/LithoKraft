// 2026-10-25 (JD, Posts; art by Cloud): Standing and Wall Posts -- pure rules shared by the sim
// (placement pair, text edits), the appearance (panel layout) and the selftests.
//
// Standing Post: two cells, same block id. Lower = the stand, upper = the holographic panel
// (kUpperBit). Facing: kFacing8 octant (engine block.h). Wall Post: one cell, 4 facings (front = the
// wall it hangs on). The text lives on the panel cell (ChunkMap::cellText), up to kMaxChars.
#pragma once
#include <string>
#include <vector>
#include "lithokraft/content/blocks.h"

namespace lithokraft::post {

constexpr size_t kMaxChars = 256;
constexpr CellState kUpperBit = CellState(1u << 10);  ///< aux bit 1: this cell is the panel half
constexpr int kRows = 16;                             ///< panel rows (1/16 block each)

inline bool isStanding(BlockId b) { return b == kVirtualPost || b == kOrganicPost; }
inline bool isWall(BlockId b) { return b == kVirtualWallPost || b == kOrganicWallPost; }
inline bool isPost(BlockId b) { return isStanding(b) || isWall(b); }
inline bool isUpper(CellState s) { return (s & kUpperBit) != 0; }
/// The cell that holds the text for a post cell at height y (a standing post's lower half -> y + 1).
inline int panelY(BlockId b, CellState s, int y) { return isStanding(b) && !isUpper(s) ? y + 1 : y; }
/// Text columns the panel can show: 16 square 1/16 glyph cells straight on; 10 on a diagonal
/// (each glyph cell is one (1, 1) lattice step there, so its corners stay on the 1/16 grid).
inline int columnsFor(BlockId b, CellState s) { return isStanding(b) && (stFacing8(s) & 1) ? 10 : 16; }

/// Edit controls (SimCommand b) for kCmdPostEdit.
enum Control : int { kTypeChar = 0, kBackspace = 1, kNewline = 2, kClear = 3 };

/// Applies one edit; printable ASCII (32..126) only, capped at kMaxChars. True when `t` changed.
inline bool applyEdit(std::string& t, int ch, int control) {
  switch (control) {
    case kBackspace: if (t.empty()) return false; t.pop_back(); return true;
    case kNewline: if (t.size() >= kMaxChars) return false; t.push_back('\n'); return true;
    case kClear: if (t.empty()) return false; t.clear(); return true;
    default:
      if (ch < 32 || ch > 126 || t.size() >= kMaxChars) return false;
      t.push_back(char(ch));
      return true;
  }
}

/// Wraps `t` into at most `rows` lines of `cols` characters ('\n' breaks a line). What does not fit
/// is cut off (the full text is still stored and saved).
inline std::vector<std::string> layout(const std::string& t, int cols, int rows = kRows) {
  std::vector<std::string> lines(1);
  for (char c : t) {
    if (c == '\n') { lines.emplace_back(); continue; }
    if (int(lines.back().size()) >= cols) lines.emplace_back();
    lines.back().push_back(c);
  }
  if (int(lines.size()) > rows) lines.resize(size_t(rows));
  return lines;
}

}  // namespace lithokraft::post
