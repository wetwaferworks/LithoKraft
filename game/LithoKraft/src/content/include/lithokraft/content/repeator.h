// Cortana 2026-10-15 2215 [GAME]: Repeator delay encoding (JD D4: 2 / 4 / 8 / 16 ticks; PlayM: 2..64, see kDelays).
//
// New encoding: state bit 13 set (the attach bits are unused on a Repeator: it is not
// attachable), aux bits 0-1 = delay index 0..3 -> 2 / 4 / 8 / 16 ticks.
// Legacy encoding (bit 13 clear, every pre-2215 save and scene): aux 0..7 = delay 1..8, migrated
// to the nearest new delay (ties go to the shorter delay): 1,2,3 -> 2; 4,5,6 -> 4; 7,8 -> 8.
// Migration is lazy and exact: every read decodes either form; the first tick or use rewrites
// the cell in the new form. No save-format version bump is needed.
// The delay line is 64 bits: history (bits 0-7), timer (8-15), historyHi (16-31) and, PlayM, the
// prevIn / prevSide / prevClk / channel bytes (32-63, unused by a Repeator, already saved as 0), so the
// saved memory layout is unchanged.
#pragma once
#include <cstdint>
#include "litholattice/world/block.h"

namespace lithokraft::repeator {

inline constexpr litholattice::CellState kNewEncoding = 0x2000;
// 2026-11-01 PlayM (JD 10:08 AM): six settings 2 / 4 / 8 / 16 / 32 / 64 ticks, cycled by use in that order.
// The "setting" (0..5, ascending ticks) is what delayIndex() returns. On disk the new encoding stores a raw
// 3-bit code = aux bits 0-1 + bit 15 (0x8000, an attach bit a Repeator never uses):
//   raw 0..3 = 4 / 8 / 16 / 32 ticks (EXACTLY what every PlayL-and-earlier new-encoding save meant, bit 15 was 0),
//   raw 4 = 64 ticks, raw 5 = 2 ticks (only PlayM writes bit 15).
// So old saves keep their exact delay with no migration; legacy (bit 13 clear) saves keep their mapping
// (aux 0..7 = old delay 1..8: 1-3 -> 4, 4-6 -> 8, 7-8 -> 16 ticks), so a fresh legacy cell is still 4 ticks.
inline constexpr int kSettings = 6;
inline constexpr int kDelays[kSettings] = {2, 4, 8, 16, 32, 64};
inline constexpr litholattice::CellState kRawHigh = 0x8000;
inline int legacyIndex(int legacyDelay) { return legacyDelay <= 3 ? 1 : legacyDelay <= 6 ? 2 : 3; }  // -> setting (4 / 8 / 16)
inline int rawToSetting(int raw) { return raw <= 3 ? raw + 1 : raw == 4 ? 5 : 0; }
inline int settingToRaw(int setting) { return setting == 0 ? 5 : setting == 5 ? 4 : setting - 1; }
/// Delay setting 0..5 (2 / 4 / 8 / 16 / 32 / 64 ticks) of a Repeator state in either encoding.
inline int delayIndex(litholattice::CellState s) {
  if (!(s & kNewEncoding)) return legacyIndex(int(litholattice::stAux(s)) + 1);
  return rawToSetting(int(litholattice::stAux(s) & 3) | ((s & kRawHigh) ? 4 : 0));
}
inline int delayTicks(litholattice::CellState s) { return kDelays[delayIndex(s)]; }
inline int nextSetting(int setting) { return (setting + 1) % kSettings; }
/// 2026-10-31 (JD t3680u): Trace colour carried down a Repeator chain. Bit 11 (aux bit 2, unused by
/// the 2-bit delay index; read only in the new encoding) = blue. The sim sets it from the Trace or
/// Repeator behind the back port; the mesher tints with it when the back neighbour has no colour.
inline constexpr litholattice::CellState kBlueChannel = 0x0800;
inline bool isBlue(litholattice::CellState s) { return (s & kNewEncoding) && (s & kBlueChannel); }
inline litholattice::CellState withBlue(litholattice::CellState s, bool blue) {
  return litholattice::CellState(blue ? (s | kBlueChannel) : (s & ~kBlueChannel));
}
/// 2026-10-31 (JD): purple = fed directly by a Switch / Button (neither red nor blue). Bit 14 (an
/// attach bit, unused on a Repeator), new encoding only; exclusive with kBlueChannel.
inline constexpr litholattice::CellState kPurpleChannel = 0x4000;
/// Colour code of a Repeator state: 1 red, 2 blue, 3 purple.
inline int colourOf(litholattice::CellState s) {
  if (!(s & kNewEncoding)) return 1;
  return (s & kPurpleChannel) ? 3 : (s & kBlueChannel) ? 2 : 1;
}
inline litholattice::CellState withColour(litholattice::CellState s, int ch) {
  s = litholattice::CellState(s & ~(kBlueChannel | kPurpleChannel));
  return litholattice::CellState(ch == 2 ? (s | kBlueChannel) : ch == 3 ? (s | kPurpleChannel) : s);
}
/// A Repeator state in the new encoding.
/// `setting` is 0..5 (delayIndex order); colour bits are added by withColour.
inline litholattice::CellState encode(int value, int facing, int setting, bool on) {
  const int raw = settingToRaw(((setting % kSettings) + kSettings) % kSettings);
  return litholattice::CellState(litholattice::makeState(value, facing, raw & 3, on) | kNewEncoding | ((raw & 4) ? kRawHigh : 0));
}

}  // namespace lithokraft::repeator