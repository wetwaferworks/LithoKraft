#pragma once
// Fix (b): IColumnPresence over the active dimension's ChunkMap (game thread only:
// the game thread alone inserts/erases columns, so the lookup is safe while the sim runs).
#include "litholattice/world/chunk_map.h"
#include "lithokraft/game/IColumnPresence.h"

namespace lithokraft {

class cChunkMapPresence final : public IColumnPresence {
 public:
  explicit cChunkMapPresence(const litholattice::ChunkMap& map) : map_(map) {}
  bool hasColumn(int cx, int cz) const override { return map_.column(cx, cz) != nullptr; }

 private:
  const litholattice::ChunkMap& map_;
};

}  // namespace lithokraft
