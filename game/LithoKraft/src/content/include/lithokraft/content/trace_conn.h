#pragma once
// 2026-11-01 PlayL (JD 7:48): per-Trace connection states.
// A flat Trace cell keeps its strength in state bits 0..6 (and bits 7..8 untouched). Bits 9..14 hold a
// 6-side connection mask, bit 15 marks "custom". Bit 15 clear = auto/default (connect everywhere, the old
// behaviour; every existing world reads as auto, so goldens are unchanged). The state lives in the cell,
// so it saves / loads with the world, meshes lock-free, and is deterministic.
// Sides (mask bit): +X 0, -X 1, +Y 2, -Y 3, +Z 4, -Z 5. A stair link (horizontal + up/down) uses its
// horizontal side. States: index 0 = auto, 1..63 = mask (64 states, so Alt+wheel's 63 steps = one back).
// LL_TRACE_CONN=0: every Trace reads as auto (feature off).
// 2026-11-08 (wire job Phase 2): a wire's SHAPE is a pure derived function of
// geometry + this mask -- per direction, conn[d] in {none, side, up, down}
// comes from the one derivation in trace_link.h (traceLinkDirs), read alike
// by the nets, both floods and the mesher. Nothing stores shape; there are
// no free state bits to store it in, and derivation is what keeps saves and
// goldens stable. JD's policies on record: an isolated wire stays a DOT
// (2026-11-08 ruling 4 -- no cross default, no toggle), and redirection
// NOTIFIES: any topology change re-floods and components re-read on the
// settle (ruling 5 -- Java's no-notify quirk is not adopted; the Signal
// Propagation Law argues the same way).
#include <cstdlib>
#include "lithokraft/content/blocks.h"

namespace lithokraft {

class cTraceConn {
 public:
  static constexpr int kStates = 64;
  static constexpr CellState kCustom = CellState(0x8000);
  static constexpr CellState kMaskBits = CellState(0x7E00);
  static bool enabled() {
    static const bool on = [] { const char* e = std::getenv("LL_TRACE_CONN"); return !(e && e[0] == '0'); }();
    return on;
  }
  static int sideBit(int dx, int dy, int dz) {
    if (dx > 0) return 0;
    if (dx < 0) return 1;
    if (dz > 0) return 4;
    if (dz < 0) return 5;
    return dy > 0 ? 2 : 3;
  }
  static int index(CellState s) { return (s & kCustom) ? int((s & kMaskBits) >> 9) : 0; }
  static bool isAuto(CellState s) { return !enabled() || !(s & kCustom); }
  /// Does the Trace in state `s` connect toward (dx, dy, dz)?
  static bool sideOn(CellState s, int dx, int dy, int dz) {
    if (isAuto(s)) return true;
    return (index(s) >> sideBit(dx, dy, dz)) & 1;
  }
  static CellState withIndex(CellState s, int i) {
    i = ((i % kStates) + kStates) % kStates;
    const CellState base = CellState(s & CellState(0x01FF));
    return i == 0 ? base : CellState(base | kCustom | CellState(i << 9));
  }
  static CellState cycled(CellState s, int step) { return withIndex(s, index(s) + step); }
  /// Keep the connection bits when the sim writes a new strength.
  static CellState withValue(CellState old, int v) { return CellState((old & CellState(~0x7F)) | CellState(v & 0x7F)); }
};

}  // namespace lithokraft
