#pragma once
// 2026-11-08 (JD wire job, Phase 0): the ONE flat-Trace link derivation.
// Before this header the link logic existed three times (cSignalSim::wireLinks,
// the full-pass forEachLink lambda, and the appearance arm scan) and had
// already diverged twice (a via target at (d,+1) set a render arm the sim
// never links; bug #17's state-write divergence in the same neighbourhood).
// Every consumer now derives links here: nets, both floods, and the mesher's
// arm scan. Shape and climb are DERIVED from geometry + the cTraceConn mask,
// never stored (a wire cell has no free state bits; derivation is also what
// keeps saves and goldens stable).
//
// The derivation, for the flat wire at the origin (vias keep their own
// vertical / side-elbow paths in signal.cpp; a flat wire reaches a via only
// through a side via's open end, at dy == 0):
//   per horizontal direction d, per dy in {-1, 0, +1}:
//     dy == +1 is cut when the cell above THIS wire cuts the climb;
//     dy == -1 is cut when the cell beside this wire at its own level
//       (the cell above the lower wire) cuts the climb;
//     the target must be a same-channel wire, and BOTH ends' cTraceConn
//       masks must allow the horizontal side.
// The cut test itself is climbCut() below: JD's derived property
// (blocks.h cutsWireClimb), swapped in at Phase 3 of the wire job in this
// one place after Phase 0 proved the single-sourcing hash-neutral.
#include "lithokraft/content/blocks.h"
#include "lithokraft/content/trace_conn.h"

namespace lithokraft {

/// Per-direction link outcome of traceLinkDirs (directions in kDirX/kDirZ order).
struct TraceLinkDirs {
  bool side = false;  ///< dy == 0 link to a wire
  bool up = false;    ///< dy == +1 link (already cut-gated)
  bool down = false;  ///< dy == -1 link (already cut-gated)
};

/// Whether the block in a climb's cut cell cuts the climb past it.
/// JD's rule (2026-11-08, wire plan decision 2), implemented in blocks.h as
/// cutsWireClimb: solid AND opaque cuts; insulating blocks cut; transparent
/// and non-solid blocks never cut (glass and frosted glass let Trace stair
/// through them; substrate does not cut either). This replaced the legacy
/// kCutsWire flag read in Phase 3 of the wire job -- the flag survives in
/// the registry for its other readers, but the climb no longer consults it.
inline bool climbCut(BlockId b) { return cutsWireClimb(b); }

/// Side-via elbow rule (moved from signal.cpp so every link consumer shares
/// it): a horizontal step dx/dz between self and nb is a via link only when
/// at least one end is a side via whose axis matches the step, and neither
/// end is a side via across the step.
inline bool viaSideOk(BlockId self, BlockId nb, int dx) {
  const int ax = dx ? 1 : 2, sa = viaAxis(self), na = viaAxis(nb);
  if (sa > 0 && sa != ax) return false;
  if (na > 0 && na != ax) return false;
  return sa > 0 || na > 0;
}

/// Fill out[4] with the flat-Trace links of the wire `self` (channel `channel`,
/// state `selfState`) at the origin. `get(dx, dy, dz)` returns the BlockId at
/// the relative cell, `getState(dx, dy, dz)` its CellState. Callers adapt
/// their own storage (ChunkMap for the sim, the mesher's padded MeshInput
/// for appearance) through these two reads, so the rule itself exists once.
template <class Get, class GetState>
void traceLinkDirs(BlockId self, int channel, CellState selfState, Get&& get, GetState&& getState,
                   TraceLinkDirs out[4]) {
  const bool upBlocked = climbCut(get(0, 1, 0));
  for (int d = 0; d < 4; ++d) {
    const int dx = kDirX[d], dz = kDirZ[d];
    const bool downBlocked = climbCut(get(dx, 0, dz));
    for (int dy = -1; dy <= 1; ++dy) {
      if (dy == 1 && upBlocked) continue;
      if (dy == -1 && downBlocked) continue;
      const BlockId nb = get(dx, dy, dz);
      const BlockDef nd = blockDef(nb);
      if (nd.role != SimRole::Wire || nd.channel != channel) continue;
      if (isVia(nb) && !(dy == 0 && viaSideOk(self, nb, dx))) continue;  // Trace feeds a via only through a side via's open end
      if (!cTraceConn::sideOn(selfState, dx, 0, dz) ||
          !cTraceConn::sideOn(getState(dx, dy, dz), -dx, 0, -dz)) continue;  // PlayL connection states, both ends
      if (dy == 0) out[d].side = true;
      else if (dy == 1) out[d].up = true;
      else out[d].down = true;
    }
  }
}

}  // namespace lithokraft
