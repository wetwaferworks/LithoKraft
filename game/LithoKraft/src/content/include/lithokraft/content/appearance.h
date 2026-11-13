// LithoKraft's block looks on top of the engine mesher: wire strips joined to their neighbours,
// lit lamps / latches, colour-lamp palettes, repeator pips, memory-cell values, torch sprites.
#pragma once
#include "lithokraft/engine.h"
#include "litholattice/render/block_appearance.h"
#include "litholattice/world/raycast.h"
#include <utility>
#include "lithokraft/content/foliage.h"     // B9 2026-11-12: Via B quads
#include "lithokraft/content/trace_link.h"  // B9 2026-11-12: viaSideOk (the sim's via link rule)

namespace lithokraft {

class cLithoKraftAppearance final : public IBlockAppearance {
 public:
  bool active(const BlockDef& def, CellState state) const override;
  FaceLook faceLook(const BlockDef& def, CellState state, FaceSlot slot, bool active) const override;
  FaceLook faceLookAt(const BlockDef& def, CellState state, FaceSlot slot, bool active,
                      int x, int y, int z) const override;
  void meshCustom(const CustomBlock& cell, MeshWriter& out) const override;
  std::optional<FaceMaterial> faceMaterial(const CustomBlock& cell, FaceSlot slot, bool active) const override;
  std::optional<FaceLook> faceLookCell(const CustomBlock& cell, FaceSlot slot, bool active) const override;  // Cortana 2026-10-15 1755
  void meshExtra(const CustomBlock& cell, MeshWriter& out) const override;
  bool meshShaped(const CustomBlock& cell, MeshWriter& out) const override;  // 2026-10-24 slab components
  bool shapedRender(const BlockDef& def) const override;
  const BoxModel* boxModel(const BlockDef& def) const override;
  const BoxModel* boxModel(const BlockDef& def, CellState st) const override;  // 2026-10-24: lever on / off
  int boxModelTurns(const BlockDef& def, CellState st) const override;
  void bindMaterials(const BlockDef& def, const BlockModel& model, MaterialBindings& bindings) const override;
};

/// Fix 3c: connected via side variant (kConnectedSide*) from same-colour vias directly above /
/// below (MODDING.md: lowest of a run = bottom, highest = top, between = mid, lone = single).
inline int viaSideVariant(bool sameAbove, bool sameBelow) {
  return sameAbove && sameBelow ? kConnectedSideMid : sameBelow ? kConnectedSideTop : sameAbove ? kConnectedSideBottom : kConnectedSideSingle;
}

/// 2026-10-25 (JD): the look / target pick shape of a partial block (litholattice::PickShapeFn):
/// the boxes it is drawn with (slab components, Repeator, Lithographor, LED / lever / button box
/// models); -1 = full cell. Wire keeps the engine's kFlatPick plate. Installed by Game.
int pickShapeBoxes(const litholattice::ChunkMap& w, const litholattice::cBlockReg& blocks, int x, int y, int z,
                   litholattice::BlockId b, litholattice::PickBox* out, int maxBoxes);

/// B9 (JD 2026-11-11 A9 playtest, "strength does not render"): the DIALOR top-face strength
/// visual for a cell state, pure so the selftest can pin it. `still` = concept C pointer still
/// (v01 / v32 / v64: the pack ships only those three); `value` = the exact dial 1..64 drawn by
/// the two-digit readout; readoutX/Y = the readout tens-digit origin in 128 texture space (the
/// ghost-88 cells of dialor_top_*_base: x76..99, y57..70).
/// C9 2026-11-12 (JD): the dial runs 0..64; value 0 draws "0" (the ones digit) on the v01 still.
struct DialorTopLook { const char* still; int value; int readoutX, readoutY; };
DialorTopLook dialorTopLook(CellState s);
/// C9 2026-11-12 (JD Bitor ranges): the armed-op still for a Bitor state ("and" / "or" / "xor" /
/// "nor" = the approved row B stills), from the op in force (aux bits 1-2).
const char* bitorStillOf(CellState s);
/// A v2 readout window's footprint on the model top, in 1/16 block, for a 128-space origin
/// (24 x 14 px digits pair, rounded out to whole 1/16 cells).
void v2ReadoutFootprint(int roX, int roY, int& x0, int& x1, int& z0, int& z1);

/// B9 2026-11-12 (JD Via B, lattice cage + 90-degree auto-elbows). The look of a via cell, derived
/// from its neighbours each mesh (nothing stored: same ids, same state, sim untouched). Arms are the
/// same-colour VIA neighbours the sim links to (signal.cpp wireLinks: vertical = two vertical vias,
/// horizontal = viaSideOk). Exactly two perpendicular arms -> the elbow still (only a vertical via
/// can be the corner: side vias link only along their own axis). Anything else -> the straight still
/// on the cell's own axis; an end is a joint when that arm is linked. No T / Y / cross looks.
/// Faces: 0 +X, 1 -X, 2 +Y, 3 -Y, 4 +Z, 5 -Z. up / south = the faces the canonical +Y / +Z end turn to.
struct ViaLook {
  bool elbow = false;
  const char* model = nullptr;  ///< "via_single|top|bottom|mid" or "via_elbow_single|upjoint|sidejoint|mid"
  const char* state = nullptr;  ///< the variant / end state (texture state tag)
  int up = 2, south = 4;
  int arms = 0;                 ///< bit f = linked same-colour via on face f
};
inline constexpr int kViaFace[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
template <class Get, class GetState>
ViaLook viaLook(BlockId self, CellState selfState, Get&& get, GetState&& getState) {
  ViaLook L;
  const int ax = viaAxis(self);
  if (ax < 0) return L;
  const int ch = blockDef(self).channel;
  for (int f = 0; f < 6; ++f) {
    const int dx = kViaFace[f][0], dy = kViaFace[f][1], dz = kViaFace[f][2];
    const BlockId nb = get(dx, dy, dz);
    if (!isVia(nb) || blockDef(nb).channel != ch) continue;
    const bool link = dy ? (ax == 0 && viaAxis(nb) == 0 && cTraceConn::sideOn(selfState, 0, dy, 0) &&
                            cTraceConn::sideOn(getState(dx, dy, dz), 0, -dy, 0))
                         : viaSideOk(self, nb, dx);
    if (link) L.arms |= 1 << f;
  }
  int a = -1, b = -1, n = 0;
  for (int f = 0; f < 6; ++f) if (L.arms & (1 << f)) { (n == 0 ? a : b) = f; ++n; }
  if (n == 2 && a / 2 != b / 2) {  // 90-degree L: the elbow (both arms are linked same-colour vias -> joints)
    L.elbow = true;
    if (b / 2 == 1) std::swap(a, b);  // a vertical arm takes the canonical +Y end
    L.up = a; L.south = b;
    const bool upJ = (L.arms >> L.up) & 1, sideJ = (L.arms >> L.south) & 1;
    static constexpr const char* kE[4] = {"single", "upjoint", "sidejoint", "mid"};
    static constexpr const char* kEM[4] = {"via_elbow_single", "via_elbow_upjoint", "via_elbow_sidejoint", "via_elbow_mid"};
    const int i = (upJ ? 1 : 0) | (sideJ ? 2 : 0);
    L.state = kE[i]; L.model = kEM[i];
    return L;
  }
  // Straight on the cell's own axis (vertical: +Y up; side X: +Y -> +X, +Z stays; side Z: +Y -> +Z, +Z -> -Y).
  L.up = ax == 0 ? 2 : ax == 1 ? 0 : 4;
  L.south = ax == 2 ? 3 : 4;
  const bool posJ = (L.arms >> L.up) & 1, negJ = (L.arms >> (L.up ^ 1)) & 1;
  static constexpr const char* kS[4] = {"single", "top", "bottom", "mid"};  // top = joint below, bottom = joint above
  static constexpr const char* kSM[4] = {"via_single", "via_top", "via_bottom", "via_mid"};
  const int i = (negJ ? 1 : 0) | (posJ ? 2 : 0);
  L.state = kS[i]; L.model = kSM[i];
  return L;
}
/// A canonical via model quad turned so +Y -> face `up`, +Z -> face `south` (a proper rotation about
/// the cell centre, 1/16 units). Render only.
foliage::Quad viaRotated(const foliage::Quad& q, int up, int south);

/// The shared instance (stateless, safe on mesh worker threads).
const IBlockAppearance& blockAppearance();

}  // namespace lithokraft
