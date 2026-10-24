// LithoKraft's block looks on top of the engine mesher: wire strips joined to their neighbours,
// lit lamps / latches, colour-lamp palettes, repeator pips, memory-cell values, torch sprites.
#pragma once
#include "lithokraft/engine.h"
#include "litholattice/render/block_appearance.h"

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

/// The shared instance (stateless, safe on mesh worker threads).
const IBlockAppearance& blockAppearance();

}  // namespace lithokraft
