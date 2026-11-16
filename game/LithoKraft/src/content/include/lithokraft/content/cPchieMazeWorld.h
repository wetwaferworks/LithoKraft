// PCHIE P7 in-engine maze (2026-11-10, JD t5033u): --pchie-maze FILE [--spawn maze].
// Builds a Bolt PCHIE B1 grid, exported as text by pchie_lk_pilot, as voxels on the Flat Lab plain,
// so the PCHIE reasoning model can read the maze back over MCP (world_get_block) and walk it through
// the real input path (player_* tools). View-only example scene: generic blocks, no sim hooks, no HUD
// code, nothing hashed here, never a golden.
//
// Text format (one char per cell, row y = 0 is the north row, -z):
//   PCHIE-B1 <width> <height> <seed>
//   <height rows of <width> chars>   # wall  . floor  S start  K key  D door  E exit  P pit
// Cell (x, y) is the block column (originX + x, originZ + y). Layers (floorY = the plain's top):
//   floorY      floor (Aluminum Block; Start = Copper Block; Exit = Maze Exit; Pit = air down 2)
//   floorY+1,+2 wall = Stone x2; door = Frosted Glass x2; key = Maze Key at floorY+1 only
#pragma once
#include <string>
#include "lithokraft/engine.h"
#include "litholattice/world/chunk_map.h"

namespace lithokraft {

class cPchieMazeWorld final {
 public:
  struct Layout {
    bool ok = false;
    std::string err;
    int width = 0, height = 0;
    int originX = 0, originZ = 0, floorY = 0;
    int startX = 0, startY = 0;
    float spawnX = 0.f, spawnZ = 0.f;  ///< Start cell centre
    int placed = 0;
  };

  /// Parse `text` and build it so the Start cell is block column (sx, sz); floorY = the plain's top block.
  Layout build(ChunkMap& world, const std::string& text, int sx, int floorY, int sz);

 private:
  void put(int x, int y, int z, BlockId b);
  ChunkMap* w_ = nullptr;
  int placed_ = 0;
};

}  // namespace lithokraft
