// Bolt PCHIE Lab (2026-10-30, JD): --bolt-lab. Staged stations, one per scenario in
// bolt-pch-ie-public/docs/BOLT_PCHIE_TEST_SCENARIOS_2026-10-30.md, on the Flat Lab plain. The harness
// is not hooked in yet: these are physical set pieces only (no sim hooks, no HUD code).
#pragma once
#include <string>
#include <vector>
#include "lithokraft/engine.h"
#include "litholattice/world/chunk_map.h"

namespace lithokraft {

class cBoltLabWorld final {
 public:
  struct Station { std::string id, name; int x0 = 0, z0 = 0, x1 = 0, z1 = 0; float spawnX = 0, spawnZ = 0; };
  struct Layout { int floorY = 0; float spawnX = 0, spawnZ = 0; std::vector<Station> stations; int placed = 0; };

  /// Build every station on a grid of 40 x 40 pads north (+z) of (sx, sz); floorY = the plain's top block.
  Layout build(ChunkMap& world, int sx, int floorY, int sz);

 private:
  static constexpr int kSlot = 40, kPad = 34, kCols = 5;
  ChunkMap* w_ = nullptr;
  int fy_ = 0, placed_ = 0;

  void put(int x, int y, int z, BlockId b);
  void pad(int x0, int z0, int size, BlockId floor);
  void label(int x, int z, const std::string& text, BlockId b);
  void pedestal(int x, int z, BlockId top);
  void buildMaze(int x0, int z0);
  void buildCourt(int x0, int z0);
  void buildBuildOff(int x0, int z0);
  void buildGauntlet(int x0, int z0);
  void buildGeneric(int x0, int z0, int variant);
  void buildHudCorner(int x0, int z0);
  static const char* glyph(char c);
};

}  // namespace lithokraft