// Cortana 2026-10-15 2215 [GAME]: Repeator delay encoding (JD D4: 2 / 4 / 8 / 16 ticks).
//
// New encoding: state bit 13 set (the attach bits are unused on a Repeator: it is not
// attachable), aux bits 0-1 = delay index 0..3 -> 2 / 4 / 8 / 16 ticks.
// Legacy encoding (bit 13 clear, every pre-2215 save and scene): aux 0..7 = delay 1..8, migrated
// to the nearest new delay (ties go to the shorter delay): 1,2,3 -> 2; 4,5,6 -> 4; 7,8 -> 8.
// Migration is lazy and exact: every read decodes either form; the first tick or use rewrites
// the cell in the new form. No save-format version bump is needed.
// The delay line is 16 bits: ComponentMemory::history (low byte) + ComponentMemory::timer
// (high byte; a Repeator never used timer), so the saved memory layout is unchanged.
#pragma once
#include <cstdint>
#include "litholattice/world/block.h"

namespace lithokraft::repeator {

inline constexpr litholattice::CellState kNewEncoding = 0x2000;
inline constexpr int kDelays[4] = {4, 8, 16, 32};  // 2026-10-24 9:33 PM JD: was {2, 4, 8, 16}; same button index

inline int legacyIndex(int legacyDelay) { return legacyDelay <= 3 ? 0 : legacyDelay <= 6 ? 1 : 2; }
/// Delay index 0..3 of a Repeator state in either encoding.
inline int delayIndex(litholattice::CellState s) {
  return (s & kNewEncoding) ? int(litholattice::stAux(s) & 3) : legacyIndex(int(litholattice::stAux(s)) + 1);
}
inline int delayTicks(litholattice::CellState s) { return kDelays[delayIndex(s)]; }
/// A Repeator state in the new encoding.
inline litholattice::CellState encode(int value, int facing, int index, bool on) {
  return litholattice::CellState(litholattice::makeState(value, facing, index & 3, on) | kNewEncoding);
}

}  // namespace lithokraft::repeator